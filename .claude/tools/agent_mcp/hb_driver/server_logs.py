"""Read what the servers and the client logged AFTER a marked point.

The bridge journals the world socket only (HB-0100), so login-side results (why an account or
character create was refused) and the world's save of a logging-out character are read from the
logs. A log_watch remembers a byte offset; everything it returns was written after mark().

Line shape (hb::log): `[YYYY-MM-DD HH:MM:SS.mmm] [LEVEL] text`, local wall time, 1 ms resolution.
"""

import datetime
import os
import re
import time

from . import common

line_pattern = re.compile(r"^\[(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3})\] \[(\w+)\] (.*)$")
timestamp_format = "%Y-%m-%d %H:%M:%S.%f"
poll_interval_s = 0.1

# world_runtime.cpp save_player / the session loop / the persistence worker.
save_queued_pattern = r"save queued: character {id} -> '(?P<map>[^']+)' \((?P<x>-?\d+),(?P<y>-?\d+)\) dir (?P<dir>\d+)"
session_closed_text = "client session closed"
batch_committed_text = "persistence: batch"
player_entered_pattern = r"player entered: '{name}' "
# The session-closed line follows its save within this many lines (announce_leave may log between).
save_to_close_max_lines = 8
logout_save_wait_s = 15.0


def parse_time_ms(stamp):
	"""A log stamp as epoch milliseconds (local time, as written)."""
	moment = datetime.datetime.strptime(stamp, timestamp_format)
	return int(time.mktime(moment.timetuple()) * 1000) + moment.microsecond // 1000


def parse_line(line):
	"""{'stamp', 'wall_ms', 'level', 'text', 'line'} or None for a line that is not a log record."""
	match = line_pattern.match(line.rstrip("\r\n"))
	if not match:
		return None
	return {"stamp": match.group(1), "wall_ms": parse_time_ms(match.group(1)), "level": match.group(2),
	        "text": match.group(3), "line": match.group(0)}


class log_watch:
	"""Everything a log file gained since mark(). A file that shrank (truncated by a restart) is read
	from its start, and `truncated` says so."""

	def __init__(self, path):
		self.path = path
		self.offset = 0
		self.truncated = False
		self.mark()

	def mark(self):
		self.offset = os.path.getsize(self.path) if os.path.isfile(self.path) else 0
		self.truncated = False
		return self.offset

	def text(self):
		if not os.path.isfile(self.path):
			return ""
		size = os.path.getsize(self.path)
		start = self.offset
		if size < start:
			self.truncated = True
			start = 0
		with open(self.path, "rb") as handle:
			handle.seek(start)
			return handle.read().decode("utf-8", "replace")

	def records(self):
		return [record for record in (parse_line(line) for line in self.text().splitlines()) if record]

	def find(self, needle):
		"""Records whose text contains `needle` (a plain substring)."""
		return [record for record in self.records() if needle in record["text"]]

	def search(self, pattern):
		"""(record, match) for every record whose text matches the regex."""
		compiled = re.compile(pattern)
		found = []
		for record in self.records():
			match = compiled.search(record["text"])
			if match:
				found.append((record, match))
		return found


def logout_save(records, character_id):
	"""The proof a logout save landed, from records in order, or None.

	The LAST `save queued: character <id>` that is followed within save_to_close_max_lines by
	`client session closed`, with a `persistence: batch` commit after that close. A periodic save
	has no session close beside it, so it never qualifies.
	"""
	save_regex = re.compile(save_queued_pattern.format(id=int(character_id)))
	proof = None
	for index, record in enumerate(records):
		match = save_regex.search(record["text"])
		if not match:
			continue
		last = min(len(records), index + 1 + save_to_close_max_lines)
		close_index = next((at for at in range(index + 1, last) if session_closed_text in records[at]["text"]), None)
		if close_index is None:
			continue
		close = records[close_index]
		commit = next((row for row in records[close_index + 1:] if batch_committed_text in row["text"]), None)
		if commit is None:
			continue
		proof = {"character_id": int(character_id), "map": match.group("map"), "x": int(match.group("x")),
		         "y": int(match.group("y")), "direction": int(match.group("dir")),
		         "lines": [record["line"], close["line"], commit["line"]], "commit_wall_ms": commit["wall_ms"]}
	return proof


def wait_logout_saved(world_watch, character_id, timeout_s=logout_save_wait_s):
	"""Wait for a logout save of `character_id` in the world log since world_watch.mark().

	Mark the watch BEFORE the logout (switch character / log out / quit). Returns the proof dict that
	player_db.write_position() requires; raises on timeout.
	"""
	proof = common.poll_until(lambda: logout_save(world_watch.records(), character_id), timeout_s, poll_interval_s)
	if not proof:
		raise common.flow_error("no logout save for character {} in {} within {:g}s (save queued -> session "
		                        "closed -> persistence batch)".format(character_id, world_watch.path, timeout_s))
	proof["watch_path"] = world_watch.path
	proof["watch_offset"] = world_watch.offset
	return proof


def entered_since(world_watch, character_name):
	"""World-log `player entered:` records for the character since the watch's mark."""
	return world_watch.search(player_entered_pattern.format(name=re.escape(character_name)))


def login_outcome(login_watch, needles):
	"""The first login-log record containing any needle (e.g. 'account create rejected'), or None."""
	for record in login_watch.records():
		if any(needle in record["text"] for needle in needles):
			return record
	return None
