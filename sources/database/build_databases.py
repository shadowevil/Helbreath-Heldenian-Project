#!/usr/bin/env python
"""Generate gamedata.db and mapinfo.db from the SQL source directory (Phase E).

Reads sources/database/<db>/<table>.sql (structure) and <table>.data.sql (rows) and writes
    <output-dir>/gamedata.db, <output-dir>/mapinfo.db   (default output dir: game/server/)
    <header-out>  schema_sql_generated.h  (untracked; per-DB schema text + fingerprint,
                  constexpr std::string_view in hb::server::schema)

Account databases are runtime state (password hashes) and are never built or touched.

STOP THE SERVER FIRST, on every OS. On Windows a DB the server holds open cannot be replaced; on Linux
the rename succeeds and every later server write goes to the old, unlinked file and is lost. The build
probes each existing target with BEGIN EXCLUSIVE and refuses one that is busy, but an idle server holds
no lock, so the probe cannot prove the server is stopped.

The `meta` table of each DB keeps the rows from its sidecar (schema_version stays until the server
stops reading it, Phase F) and gains the generated keys: sql_release, schema_fingerprint, channel,
version. All of them are deterministic, so an unchanged source rebuilds to an identical DB.

Operator state: the rows of admins, banned_list and admin_command_permissions (gamedata) are carried
over from the existing target DB on every rebuild (columns matched by name); the sidecars only seed a
DB that does not exist yet. With --verify they are carried from the --compare-dir DB. A new schema that
drops an operator column holding data is refused unless --force.

Overwrite guard: after each build a sidecar <db>.generated.json records the content digest of the
generated DB (sqlite_master text + per-table row hashes, same logic as verify_roundtrip.py, leaving out
the operator-state rows). The build replaces a target only when
  - the target is missing (create), or
  - it already equals the fresh build (adopted, no rewrite), or
  - it equals the fresh build except for the generated meta keys (adopted: e.g. a DB just extracted
    with extract_databases.py, or a channel switch), or
  - its digest still matches its generation record.
Anything else may hold edits that exist nowhere else: the build refuses and says to run
extract_databases.py first, unless --force. A -wal, -shm or -journal file beside a target (create or
replace) is always refused, since SQLite would replay it into the new DB; --force deletes exactly those
files and logs them. Stale temp files from an interrupted run (.<db>.tmp<pid>) are removed at start.

Install: every DB is built to a temp file beside its target and all guards are checked first, so a
refusal leaves every target untouched. Then each existing target is moved to a backup name
(.<db>.bak<pid>), the new file is renamed in, and the generation records are written. If any step
fails or is interrupted (Ctrl-C), every backup is restored and every record put back; if a restore
itself fails, the error names the backup file that still holds the old DB. A missing target with such a
backup beside it is refused (even with --force) until it is renamed back or --discard-backup is given,
so a lost restore never silently resets the operator rows to the seed. The restore decides from the
disk (an existing backup goes back, a target that was not there before is removed), so it also undoes
an interruption right after a rename. Each in-use probe runs before any stale sidecar is deleted, and
targets without stale sidecars are probed first. One build per output folder at a time:
<output-dir>/.build_databases.lock is created exclusively and removed at exit only while it still
holds this run's pid; a killed run leaves it, and the next run says so.

--verify builds into a scratch folder (never game/server, never --compare-dir, and only over DBs it
generated itself; it always reinstalls them so the carried operator rows are current) and compares
per-table content hashes with the existing DBs (read-only, immutable), reusing verify_roundtrip.py's logic.

Usage:
    python build_databases.py [--db gamedata|mapinfo|all] [--output-dir DIR] [--header-out FILE]
        [--channel DEV|TEST|PROD] [--force] [--discard-backup] [--verify [--compare-dir DIR]]
Exit status: 0 ok, 1 build/verify failure or refusal.
"""

import argparse
import hashlib
import json
import os
import re
import sqlite3
import sys
import urllib.parse

import extract_databases as ex
import verify_roundtrip as vr

REPO_ROOT = os.path.normpath(os.path.join(ex.HERE, "..", ".."))
VERSION_HEADER = os.path.join(ex.HERE, "..", "dependencies", "shared", "version.h")
DEFAULT_OUTPUT_DIR = ex.SERVER_DIR
DEFAULT_HEADER_OUT = os.path.join(REPO_ROOT, "build", "generated", "schema_sql_generated.h")
DEFAULT_VERIFY_DIR = os.path.join(REPO_ROOT, ".claude", "scratch", "build_databases", "verify")

BUILD_KEYS = ["gamedata", "mapinfo"]
CHANNELS = ["DEV", "TEST", "PROD"]
GENERATED_META_KEYS = ["sql_release", "schema_fingerprint", "channel", "version"]
# Runtime-written tables whose live rows are operator state: carried across rebuilds, not guarded.
OPERATOR_TABLES = {"gamedata": ["admin_command_permissions", "admins", "banned_list"]}
STATE_SUFFIX = ".generated.json"
DIGEST_FORMAT = 2
SQLITE_SIDECARS = ["-journal", "-wal", "-shm"]
LOCK_TIMEOUT = 0.5
LITERAL_PIECE = 4000       # max characters of one C++ string literal piece (MSVC limit is 16380)
SCHEMA_LIMIT = 65000       # max bytes of one DB's schema text (MSVC concatenated-literal limit is 65535)
IN_USE = "a target is in use - is the server running? Stop it and build again."
LOCK_NAME = ".build_databases.lock"   # one build per output folder at a time

RELEASE_PATTERN = (r"inline\s+constexpr\s+release\s+sql_release\s*\{\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*"
	r"maturity::(alpha|beta|stable)\s*\}")


class BuildError(Exception):
	pass


# ---------------------------------------------------------------- release and fingerprint

def read_sql_release():
	"""'0.0.1-alpha' parsed from the sql_release line of shared/version.h (a stable release has no suffix)."""
	with open(VERSION_HEADER, "r", encoding="utf-8") as f:
		text = f.read()
	match = re.search(RELEASE_PATTERN, text)
	if not match:
		raise BuildError("could not find 'inline constexpr release sql_release{...}' in " + VERSION_HEADER)
	major, minor, patch, maturity = match.groups()
	base = "%s.%s.%s" % (int(major), int(minor), int(patch))
	return base if maturity == "stable" else "%s-%s" % (base, maturity)


def version_string(release, channel):
	"""Canonical form M.m.p[-alpha|-beta]:CHANNEL; PROD drops the channel label."""
	return release if channel == "PROD" else "%s:%s" % (release, channel)


def structure_files(key):
	"""Sorted (ordinal) names of the <table>.sql structure files of one DB."""
	d = os.path.join(ex.HERE, key)
	if not os.path.isdir(d):
		raise BuildError("source directory missing: " + d)
	names = sorted(f for f in os.listdir(d) if f.endswith(".sql") and not f.endswith(".data.sql"))
	if not names:
		raise BuildError("no structure files in " + d)
	return names


def read_text(path):
	with open(path, "r", encoding="utf-8", newline="") as f:
		return f.read()


def normalize(text):
	"""Line endings only (git or an editor may rewrite them); everything else is kept."""
	return text.replace("\r\n", "\n").replace("\r", "\n")


def schema_parts(key):
	"""[(file name, raw text)] in ordinal order."""
	d = os.path.join(ex.HERE, key)
	return [(n, read_text(os.path.join(d, n))) for n in structure_files(key)]


def fingerprint(parts):
	h = hashlib.sha256()
	for name, text in parts:
		h.update(name.encode("utf-8"))
		h.update(b"\0")
		h.update(normalize(text).encode("utf-8"))
		h.update(b"\0")
	return h.hexdigest()


# ---------------------------------------------------------------- header

def escape_byte(byte):
	ch = chr(byte)
	if ch == "\n":
		return "\\n"
	if ch == "\r":
		return "\\r"
	if ch == "\t":
		return "\\t"
	if ch == "\\":
		return "\\\\"
	if ch == '"':
		return '\\"'
	if 32 <= byte < 127:
		return ch
	return "\\%03o" % byte


def cpp_literal_lines(text):
	"""Adjacent string literals, one or more per source line (each at most LITERAL_PIECE characters
	between the quotes, never splitting an escape), escaped to plain ASCII."""
	lines = []
	for chunk in text.splitlines(keepends=True):
		piece = []
		size = 0
		for byte in chunk.encode("utf-8"):
			esc = escape_byte(byte)
			if size + len(esc) > LITERAL_PIECE:
				lines.append('"' + "".join(piece) + '"')
				piece = []
				size = 0
			piece.append(esc)
			size += len(esc)
		lines.append('"' + "".join(piece) + '"')
	return lines or ['""']


HEADER_TEMPLATE = """// GENERATED FILE - do not edit and do not commit.
//
// Written by sources/database/build_databases.py from the SQL source directory (sources/database/<db>/*.sql,
// structure files only). Each *_schema is the concatenation of that database's structure files in ordinal
// order; each *_fingerprint is the sha256 over those files (names and line-ending-normalized text).
#pragma once

#include <string_view>

namespace hb::server::schema
{{
	inline constexpr std::string_view sql_release_text = "{release}";
	inline constexpr std::string_view channel_label = "{channel}";
	inline constexpr std::string_view version_text = "{version}";

{blocks}}} // namespace hb::server::schema
"""


def schema_text_of(info):
	return "".join(text for _n, text in info["parts"])


def check_schema_size(key, info):
	size = len(schema_text_of(info).encode("utf-8"))
	if size > SCHEMA_LIMIT:
		raise BuildError("[%s] schema text is %d bytes; the header literal limit is %d (MSVC caps a "
			"concatenated string literal at 65535 bytes)" % (key, size, SCHEMA_LIMIT))
	return size


def header_text(infos, release, channel):
	blocks = []
	for key in BUILD_KEYS:
		if key not in infos:
			continue
		info = infos[key]
		lines = cpp_literal_lines(schema_text_of(info))
		body = "\n".join("\t\t" + l for l in lines)
		blocks.append("\tinline constexpr std::string_view %s_schema =\n%s;\n\n" % (key, body))
		blocks.append("\tinline constexpr std::string_view %s_fingerprint = \"%s\";\n\n" % (key, info["fingerprint"]))
	return HEADER_TEMPLATE.format(release=release, channel=channel, version=version_string(release, channel),
		blocks="".join(blocks))


def write_if_changed(path, text):
	data = text.encode("ascii")
	if os.path.isfile(path):
		with open(path, "rb") as f:
			if f.read() == data:
				return "same"
	os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
	tmp = path + ".tmp%d" % os.getpid()
	with open(tmp, "wb") as f:
		f.write(data)
	os.replace(tmp, path)
	return "written"


# ---------------------------------------------------------------- files beside a target

def sqlite_sidecars(path):
	"""Existing -journal / -wal / -shm files of a DB path (exact names, nothing pattern-matched)."""
	return [path + s for s in SQLITE_SIDECARS if os.path.lexists(path + s)]


def db_uri(path, query):
	return "file:" + urllib.parse.quote(path.replace("\\", "/"), safe="/:") + "?" + query


def open_ro(path):
	"""Read-only immutable open; refuses (BuildError) when a sidecar would make the main file stale."""
	side = sqlite_sidecars(path)
	if side:
		raise BuildError("%s has %s beside it (the database is open or was not checkpointed); stop the "
			"server / checkpoint it first" % (path, ", ".join(os.path.basename(s) for s in side)))
	return ex.open_ro(path)


def remove_stale_temps(out_dir, keys):
	"""Delete temp files an interrupted run left beside the targets. Only names listed from out_dir that
	fully match this script's own temp naming are removed; backups (.<db>.bak<pid>) are only reported."""
	if not os.path.isdir(out_dir):
		return
	temp_patterns = []
	backup_patterns = []
	for key in keys:
		name = re.escape(ex.DB_DEFS[key][0])
		temp_patterns.append(re.compile(r"\.%s\.tmp[0-9]+(-journal|-wal|-shm)?" % name))
		temp_patterns.append(re.compile(r"%s\.tmp[0-9]+" % re.escape(ex.DB_DEFS[key][0] + STATE_SUFFIX)))
		backup_patterns.append(re.compile(r"\.%s\.bak[0-9]+" % name))
	for entry in sorted(os.listdir(out_dir)):
		path = os.path.join(out_dir, entry)
		if any(p.fullmatch(entry) for p in temp_patterns) and os.path.isfile(path):
			os.remove(path)
			print("removed stale temp file from an interrupted run: %s" % path)
		elif any(p.fullmatch(entry) for p in backup_patterns):
			print("WARNING: %s is a backup left by a failed install; it may hold the only copy of an old "
				"DB. Inspect it and delete it by hand." % path)


def leftover_backups(target):
	"""Backups (.<db>.bak<pid>) an interrupted install left beside target: exact listed names only."""
	folder, name = os.path.split(target)
	if not os.path.isdir(folder):
		return []
	pattern = re.compile(r"\.%s\.bak[0-9]+" % re.escape(name))
	return sorted(os.path.join(folder, f) for f in os.listdir(folder) if pattern.fullmatch(f))


def acquire_lock(out_dir):
	"""Create <out_dir>/.build_databases.lock exclusively; a second concurrent build refuses."""
	path = os.path.join(out_dir, LOCK_NAME)
	try:
		fd = os.open(path, os.O_CREAT | os.O_EXCL | os.O_WRONLY)
	except FileExistsError:
		raise BuildError("another build_databases.py run holds %s (one build per folder at a time). If none "
			"is running, a killed run left it behind: delete that file and build again." % path)
	try:
		with os.fdopen(fd, "w", encoding="ascii") as f:
			f.write(lock_text())
	except BaseException:
		# a failed or interrupted write must not leave a lock behind that blocks every later build
		try:
			os.close(fd)   # still open if fdopen itself failed; already closed otherwise
		except OSError:
			pass
		if os.path.lexists(path):
			os.remove(path)
		raise
	return path


def lock_text():
	return "pid %d\n" % os.getpid()


def release_lock(path):
	"""Remove the lock only while it still holds our pid (never another run's lock)."""
	try:
		with open(path, "r", encoding="ascii", errors="replace") as f:
			ours = f.read() == lock_text()
	except OSError:
		return
	if ours:
		os.remove(path)
	else:
		print("WARNING: %s no longer holds this run's pid; left in place" % path)


# ---------------------------------------------------------------- database content

def filtered_hash(conn, table, exclude_keys):
	"""verify_roundtrip.content_hash over the rows whose "key" is not in exclude_keys."""
	cols, pk = ex.table_columns(conn, table)
	collist = ", ".join(ex.qident(c) for c in cols)
	marks = ", ".join("?" for _k in exclude_keys)
	cur = conn.execute('SELECT %s FROM %s WHERE "key" NOT IN (%s)%s' % (
		collist, ex.qident(table), marks, ex.order_clause(pk)), tuple(exclude_keys))
	h = hashlib.sha256()
	n = 0
	for row in cur:
		h.update(repr(row).encode("utf-8", "surrogatepass"))
		h.update(b"\n")
		n += 1
	return n, h.hexdigest()


def table_report(path, skip_rows=(), skip_meta_keys=()):
	"""({table: (master sql dict, rows, sha256)}, sequence rows) of a DB, read-only and immutable.
	Tables in skip_rows keep their structure but no row count/hash; meta leaves out skip_meta_keys."""
	conn = open_ro(path)
	try:
		master = vr.master_by_table(conn)
		out = {}
		for name in sorted(master):
			if ("table", name) not in master[name]:
				continue
			if name in skip_rows:
				out[name] = (master[name], None, None)
				continue
			if name == "meta" and skip_meta_keys:
				n, h = filtered_hash(conn, name, skip_meta_keys)
			else:
				n, h = vr.content_hash(conn, name)
			out[name] = (master[name], n, h)
		seq = None
		if ex.has_sequence(conn):
			seq = [r for r in conn.execute("SELECT name, seq FROM sqlite_sequence ORDER BY name").fetchall()
				if r[0] not in skip_rows]
		return out, seq
	finally:
		conn.close()


def digest(path, key, loose=False):
	"""One sha256 over everything verify_roundtrip compares (structure text, row hashes, counters),
	without the operator-state rows. loose=True also leaves out the generated meta keys."""
	report, seq = table_report(path, OPERATOR_TABLES.get(key, ()), GENERATED_META_KEYS if loose else ())
	h = hashlib.sha256()
	for name in sorted(report):
		master, n, rows_hash = report[name]
		h.update(repr((name, sorted(master.items()), n, rows_hash)).encode("utf-8", "surrogatepass"))
		h.update(b"\n")
	h.update(repr(seq).encode("utf-8"))
	return h.hexdigest()


# ---------------------------------------------------------------- build

def deny_attach(action, _arg1, _arg2, _db, _trigger):
	"""Authorizer for the source scripts: no ATTACH (which also blocks VACUUM INTO)."""
	if action == sqlite3.SQLITE_ATTACH:
		return sqlite3.SQLITE_DENY
	return sqlite3.SQLITE_OK


def carry_operator_rows(conn, key, source, force):
	"""Replace the seed rows of the operator tables with the rows of the existing DB at source.
	Returns printable notes."""
	tables = OPERATOR_TABLES.get(key, [])
	if not tables or not os.path.isfile(source):
		return ["no existing DB at %s: operator tables keep the sidecar seed rows" % source] if tables else []
	notes = []
	if sqlite_sidecars(source):
		if not force:
			return []   # the guard refuses this target anyway; nothing is installed
		notes.append("WARNING: operator rows read from the main file only; anything in the stale "
			"-wal/-journal is lost")
		src = sqlite3.connect(db_uri(source, "mode=ro&immutable=1"), uri=True)
	else:
		src = open_ro(source)
	try:
		counts = []
		for table in tables:
			if src.execute("SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = ?", (table,)).fetchone() is None:
				counts.append("%s=seed (not in existing DB)" % table)
				continue
			new_cols, _pk = ex.table_columns(conn, table)
			old_cols, _pk = ex.table_columns(src, table)
			common = [c for c in new_cols if c in old_cols]
			dropped = [c for c in old_cols if c not in new_cols]
			if dropped:
				filled = [c for c in dropped if src.execute(
					"SELECT 1 FROM %s WHERE %s IS NOT NULL AND %s <> '' LIMIT 1" % (
						ex.qident(table), ex.qident(c), ex.qident(c))).fetchone() is not None]
				if filled and not force:
					raise BuildError("[%s] REFUSED: the new schema of operator table %s no longer has column(s) %s, "
						"which hold data in %s; carrying the rows would lose it (for example admins.approved_ip "
						"falling back to its default weakens admin IP locking). Keep or rename the column in the "
						"SQL source with the data migrated, or pass --force to drop it." % (
						key, table, ", ".join(filled), source))
				notes.append("WARNING: %s columns not in the new schema are dropped: %s%s" % (table, ", ".join(dropped),
					(" (holding data: %s; --force)" % ", ".join(filled)) if filled else " (all empty)"))
			collist = ", ".join(ex.qident(c) for c in common)
			rows = src.execute("SELECT %s FROM %s" % (collist, ex.qident(table))).fetchall()
			conn.execute("DELETE FROM %s" % ex.qident(table))
			try:
				conn.executemany("INSERT INTO %s (%s) VALUES (%s)" % (ex.qident(table), collist,
					", ".join("?" for _c in common)), rows)
			except sqlite3.Error as error:
				raise BuildError("[%s] cannot carry the operator rows of %s into the new schema: %s"
					% (key, table, error))
			if ex.has_sequence(conn) and ex.has_sequence(src):
				seq = src.execute("SELECT seq FROM sqlite_sequence WHERE name = ?", (table,)).fetchone()
				if seq is not None:
					conn.execute("DELETE FROM sqlite_sequence WHERE name = ?", (table,))
					conn.execute("INSERT INTO sqlite_sequence (name, seq) VALUES (?, ?)", (table, seq[0]))
			counts.append("%s=%d" % (table, len(rows)))
		notes.insert(0, "operator rows carried from %s: %s" % (source, ", ".join(counts)))
	finally:
		src.close()
	return notes


def build_to_temp(key, info, release, channel, tmp_path, carry_source, force):
	"""Load the SQL directory into a new database file at tmp_path, carry the operator rows and fill the
	generated meta keys. Returns printable notes."""
	d = os.path.join(ex.HERE, key)
	files = sorted(os.listdir(d))
	conn = sqlite3.connect(tmp_path)
	try:
		conn.set_authorizer(deny_attach)
		try:
			for name in structure_files(key):
				conn.executescript(read_text(os.path.join(d, name)))
			for name in files:
				if name.endswith(".data.sql") and name != ex.SEQUENCE_FILE:
					conn.executescript(read_text(os.path.join(d, name)))
			seq = os.path.join(d, ex.SEQUENCE_FILE)
			if os.path.isfile(seq):
				conn.executescript(read_text(seq))
		except sqlite3.DatabaseError as error:
			raise BuildError("[%s] source SQL failed: %s" % (key, error))
		finally:
			conn.set_authorizer(None)
		if conn.execute("SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = 'meta'").fetchone() is None:
			raise BuildError("[%s] source has no meta table" % key)
		notes = carry_operator_rows(conn, key, carry_source, force)
		values = {
			"sql_release": release,
			"schema_fingerprint": info["fingerprint"],
			"channel": channel,
			"version": version_string(release, channel),
		}
		for k, v in values.items():
			conn.execute('INSERT OR REPLACE INTO "meta" ("key", "value") VALUES (?, ?)', (k, v))
		conn.commit()
		check = conn.execute("PRAGMA integrity_check").fetchone()[0]
		if check != "ok":
			raise BuildError("[%s] integrity_check: %s" % (key, check))
	finally:
		conn.close()
	return notes


def state_path(target):
	return target + STATE_SUFFIX


def read_state(target):
	"""The generation record as a dict, or None (missing, unreadable, or not a JSON object)."""
	p = state_path(target)
	if not os.path.isfile(p):
		return None
	try:
		with open(p, "r", encoding="utf-8") as f:
			state = json.load(f)
	except (OSError, ValueError):
		return None
	return state if isinstance(state, dict) else None


def record_matches(state, current):
	return state is not None and state.get("digest_format") == DIGEST_FORMAT and state.get("digest") == current


def state_bytes(target, info, release, channel, new_digest):
	state = {
		"database": os.path.basename(target),
		"digest": new_digest,
		"digest_format": DIGEST_FORMAT,
		"operator_tables_not_guarded": OPERATOR_TABLES.get(info["key"], []),
		"schema_fingerprint": info["fingerprint"],
		"sql_release": release,
		"channel": channel,
	}
	return (json.dumps(state, indent="\t", sort_keys=True) + "\n").encode("utf-8")


def judge(key, target, new, force, discard_backup=False):
	"""Return (action, note, stale sidecars to delete) with action 'create'|'replace'|'same', or raise
	BuildError when the guard refuses. new = {'strict': digest, 'loose': digest} of the fresh build."""
	exists = os.path.exists(target)
	if not exists and not discard_backup:
		backups = leftover_backups(target)
		if backups:
			raise BuildError("[%s] REFUSED to create %s: an interrupted install left %s, which may hold the "
				"previous DB and its operator rows (admins, bans, command levels). Rename it back to %s and "
				"build again, or pass --discard-backup to build from the sidecar seed rows (--force does not "
				"skip this check)." % (key, target, ", ".join(backups), os.path.basename(target)))
	side = sqlite_sidecars(target)
	if side:
		names = ", ".join(os.path.basename(s) for s in side)
		if not force:
			raise BuildError("[%s] REFUSED: %s beside %s (the server has the database open, or it stopped "
				"without checkpointing); SQLite would replay it into the new DB. Stop the server / "
				"checkpoint first; or pass --force to delete it and discard what it holds." % (key, names, target))
		return ("replace" if exists else "create"), "--force (deleting stale %s)" % names, side
	if not exists:
		return "create", "target missing", []
	current = digest(target, key)
	if current == new["strict"]:
		return "same", "target already equals the fresh build", []
	if digest(target, key, loose=True) == new["loose"]:
		return "replace", "adopted: target matches the SQL source, only generated meta keys differ", []
	state = read_state(target)
	if record_matches(state, current):
		return "replace", "target unchanged since last generation", []
	if state is None:
		why = "no generation record for the target"
	elif state.get("digest_format") != DIGEST_FORMAT:
		why = "the generation record is from an older build_databases.py"
	else:
		why = "the target changed since its last generation"
	if force:
		return "replace", "--force (%s)" % why, []
	raise BuildError("[%s] REFUSED to overwrite %s: %s. It may hold edits that exist nowhere else. "
		"Run 'python sources/database/extract_databases.py --db %s' first (it reads game/server/ and writes "
		"those edits back into the SQL source), then build again: a target that matches the source is "
		"adopted. Or pass --force to discard them." % (key, target, why, key))


def assert_not_in_use(key, target, known=()):
	"""BEGIN EXCLUSIVE with a short timeout: refuse a target another connection is using. known lists the
	stale sidecars --force is about to delete; with a hot journal present SQLite may roll it back into
	the target while probing, which is harmless because the target is replaced right after."""
	try:
		conn = sqlite3.connect(db_uri(target, "mode=rw"), uri=True, timeout=LOCK_TIMEOUT, isolation_level=None)
		try:
			conn.execute("BEGIN EXCLUSIVE")
			conn.execute("ROLLBACK")
		finally:
			conn.close()
	except sqlite3.OperationalError as error:
		raise BuildError("[%s] %s is busy (%s): %s" % (key, target, error, IN_USE))
	except sqlite3.DatabaseError as error:
		raise BuildError("[%s] cannot probe %s (%s)" % (key, target, error))
	if [s for s in sqlite_sidecars(target) if s not in known]:
		raise BuildError("[%s] %s gained a -journal/-wal/-shm file while probing: %s" % (key, target, IN_USE))


def install(plans):
	"""Move every existing target to a backup, rename the new files in, write the records; on any failure
	restore every backup and record. plans: dicts with key, target, tmp, backup, action, state."""
	moves = [p for p in plans if p["action"] != "same"]
	records = [p for p in plans if p["state"] is not None]
	for p in records:
		p["state_tmp"] = state_path(p["target"]) + ".tmp%d" % os.getpid()
	for p in moves:
		# rollback decides from the disk: record what existed, and never start over a same-named backup
		p["existed"] = os.path.lexists(p["target"])
		if os.path.lexists(p["backup"]):
			raise BuildError("[%s] %s already exists (a leftover backup with this run's pid); inspect it, "
				"rename or delete it by hand, and build again" % (p["key"], p["backup"]))
	try:
		for p in records:
			sp = state_path(p["target"])
			old = None
			if os.path.isfile(sp):
				with open(sp, "rb") as f:
					old = f.read()
			p["old_state"] = old   # set only once read: rollback restores records that have this key
			with open(p["state_tmp"], "wb") as f:
				f.write(p["state"])
		for p in moves:
			if p["existed"]:
				os.replace(p["target"], p["backup"])
			os.replace(p["tmp"], p["target"])
		for p in records:
			os.replace(p["state_tmp"], state_path(p["target"]))
	except BaseException as error:
		# BaseException: a Ctrl-C (KeyboardInterrupt) mid-install must restore too, then propagate.
		failed = rollback(moves, records)
		if failed:
			message = "install failed (%r) and the restore failed too: %s" % (error, "; ".join(failed))
			if not isinstance(error, Exception):
				print("build_databases: " + message, file=sys.stderr)
				raise
			raise BuildError(message)
		if not isinstance(error, OSError):
			print("build_databases: install interrupted (%s); every DB was restored to its previous state"
				% type(error).__name__, file=sys.stderr)
			raise
		if isinstance(error, PermissionError):
			raise BuildError("%s (%s) Every DB was restored to its previous state." % (IN_USE, error))
		raise BuildError("install failed (%s); every DB was restored to its previous state" % error)
	finally:
		for p in records:
			if os.path.lexists(p["state_tmp"]):
				os.remove(p["state_tmp"])
	for p in moves:
		if p["existed"]:
			try:
				os.remove(p["backup"])
			except OSError as error:
				print("WARNING: could not remove backup %s (%s); delete it by hand" % (p["backup"], error))


def rollback(moves, records):
	"""Restore from what is on disk, not from progress flags, so an interruption between a rename and
	the next statement is still undone: a backup that exists goes back over its target; a target that
	did not exist before is removed; every record read before the install gets its old bytes back."""
	failed = []
	for p in reversed(moves):
		try:
			if p["existed"]:
				if os.path.lexists(p["backup"]):
					os.replace(p["backup"], p["target"])
			elif os.path.lexists(p["target"]):
				os.remove(p["target"])
		except OSError as error:
			where = (" (old DB kept at %s)" % p["backup"]) if os.path.lexists(p["backup"]) else ""
			failed.append("%s: %s%s" % (p["target"], error, where))
	for p in records:
		if "old_state" not in p:
			continue   # interrupted before this record was read: it was never replaced either
		sp = state_path(p["target"])
		try:
			if p["old_state"] is None:
				if os.path.lexists(sp):
					os.remove(sp)
			else:
				current = None
				if os.path.isfile(sp):
					with open(sp, "rb") as f:
						current = f.read()
				if current != p["old_state"]:
					with open(sp, "wb") as f:
						f.write(p["old_state"])
		except OSError as error:
			failed.append("%s: %s" % (sp, error))
	return failed


def build(args, keys, out_dir, header_out, force, carry_dir, reinstall=False):
	"""reinstall: --verify rewrites even an up-to-date output DB, so its operator rows are always the
	ones just carried from --compare-dir (the guard digest leaves them out)."""
	os.makedirs(out_dir, exist_ok=True)
	lock = acquire_lock(out_dir)
	try:
		return build_locked(args, keys, out_dir, header_out, force, carry_dir, reinstall)
	finally:
		release_lock(lock)


def build_locked(args, keys, out_dir, header_out, force, carry_dir, reinstall):
	release = read_sql_release()
	channel = args.channel
	print("sql release %s, channel %s, version %s" % (release, channel, version_string(release, channel)))
	print("NOTE: stop the server before building into a folder it uses (Windows cannot replace an open DB; "
		"Linux replaces it under the server and later server writes are lost).")
	infos = {}
	for key in keys:
		parts = schema_parts(key)
		infos[key] = {"key": key, "parts": parts, "fingerprint": fingerprint(parts)}
		size = check_schema_size(key, infos[key])
		print("[%s] %d structure files, %d schema bytes, fingerprint %s" % (key, len(parts), size,
			infos[key]["fingerprint"]))

	remove_stale_temps(out_dir, keys)
	plans = []
	try:
		for key in keys:
			name = ex.DB_DEFS[key][0]
			target = os.path.join(out_dir, name)
			plan = {"key": key, "target": target, "state": None,
				"tmp": os.path.join(out_dir, "." + name + ".tmp%d" % os.getpid()),
				"backup": os.path.join(out_dir, "." + name + ".bak%d" % os.getpid())}
			plans.append(plan)
			for notes_line in build_to_temp(key, infos[key], release, channel, plan["tmp"],
					os.path.join(carry_dir, name), force):
				print("[%s] %s" % (key, notes_line))
			plan["new"] = {"strict": digest(plan["tmp"], key), "loose": digest(plan["tmp"], key, loose=True)}
		# all guards first: a refusal must leave every target untouched
		for plan in plans:
			plan["action"], plan["note"], plan["stale"] = judge(plan["key"], plan["target"], plan["new"], force,
				getattr(args, "discard_backup", False))
			if reinstall and plan["action"] == "same":
				plan["action"], plan["note"] = "replace", "--verify reinstalls so the operator rows match --compare-dir"
		# probe every target before deleting anything, then delete the stale sidecars last. Targets without
		# stale sidecars go first: opening one that has them lets SQLite run its own recovery (roll back a
		# hot journal, merge or drop a -wal), so it is only probed once every clean target is known free.
		for plan in sorted(plans, key=lambda p: bool(p["stale"])):
			if plan["action"] == "replace":
				assert_not_in_use(plan["key"], plan["target"], plan["stale"])
		for plan in plans:
			for path in plan["stale"]:
				if not os.path.lexists(path):
					print("[%s] --force: stale %s was already cleared by SQLite while probing" % (plan["key"], path))
					continue
				try:
					os.remove(path)
				except PermissionError as error:
					raise BuildError("[%s] cannot delete %s (%s): %s" % (plan["key"], path, error, IN_USE))
				print("[%s] --force: removed stale %s" % (plan["key"], path))
		for plan in plans:
			if plan["action"] != "same" or not record_matches(read_state(plan["target"]), plan["new"]["strict"]):
				plan["state"] = state_bytes(plan["target"], infos[plan["key"]], release, channel, plan["new"]["strict"])
		install(plans)
		for plan in plans:
			label = {"same": "up to date", "create": "created", "replace": "replaced"}[plan["action"]]
			print("[%s] %s: %s (%s)" % (plan["key"], label, plan["target"], plan["note"]))
	finally:
		for plan in plans:
			for path in [plan["tmp"]] + sqlite_sidecars(plan["tmp"]):
				if os.path.exists(path):
					os.remove(path)
	if header_out:
		status = write_if_changed(header_out, header_text(infos, release, channel))
		print("header %s: %s" % (status, header_out))
	return infos


# ---------------------------------------------------------------- verify

def same_dir(a, b):
	"""True when a and b are the same folder, also through 8.3 short names, symlinks and junctions."""
	try:
		if os.path.exists(a) and os.path.exists(b):
			return os.path.samefile(a, b)
	except OSError:
		pass
	return os.path.normcase(os.path.realpath(a)) == os.path.normcase(os.path.realpath(b))


def check_verify_folder(keys, out_dir):
	"""--verify only overwrites DBs it generated itself; --force does not override this."""
	for key in keys:
		target = os.path.join(out_dir, ex.DB_DEFS[key][0])
		if not os.path.lexists(target) and not sqlite_sidecars(target):
			continue
		if sqlite_sidecars(target) or not os.path.isfile(target) \
				or not record_matches(read_state(target), digest(target, key)):
			raise BuildError("--verify REFUSED: %s holds %s without a matching generation record (or with a "
				"-wal/-shm/-journal file). --verify only overwrites DBs it generated itself, even with --force; "
				"use an empty --output-dir." % (out_dir, os.path.basename(target)))


def verify(keys, built_dir, compare_dir):
	total = 0
	for key in keys:
		name = ex.DB_DEFS[key][0]
		built_path = os.path.join(built_dir, name)
		orig_path = os.path.join(compare_dir, name)
		print("[%s] built %s vs existing %s" % (key, built_path, orig_path))
		if not os.path.isfile(orig_path):
			print("  FAIL existing database missing: " + orig_path)
			total += 1
			continue
		o_tables, o_seq = table_report(orig_path)
		b_tables, b_seq = table_report(built_path)
		fails = 0
		for table in sorted(set(o_tables) | set(b_tables)):
			problems = []
			if table not in b_tables:
				problems.append("missing from built")
			elif table not in o_tables:
				problems.append("not in existing")
			elif table == "meta":
				problems += compare_meta(o_tables[table], b_tables[table], built_path, orig_path)
			else:
				if o_tables[table][0] != b_tables[table][0]:
					problems.append("sqlite_master sql differs")
				if o_tables[table][1] != b_tables[table][1]:
					problems.append("row count %d != %d" % (o_tables[table][1], b_tables[table][1]))
				if o_tables[table][2] != b_tables[table][2]:
					problems.append("content hash differs")
			if problems:
				fails += 1
				print("  FAIL %s: %s" % (table, "; ".join(problems)))
			else:
				t = b_tables[table]
				extra = ""
				if table == "meta":
					extra = " (original rows kept; generated keys added)"
				elif table in OPERATOR_TABLES.get(key, ()):
					extra = " (operator state, carried from the existing DB)"
				print("  PASS %s rows=%d sha256=%s%s" % (table, t[1], t[2][:16], extra))
		if o_seq != b_seq:
			fails += 1
			print("  FAIL sqlite_sequence differs")
		elif o_seq is not None:
			print("  PASS sqlite_sequence rows=%d" % len(o_seq))
		print("[%s] tables=%d failures=%d" % (key, len(set(o_tables) | set(b_tables)), fails))
		total += fails
	return total


def compare_meta(orig, built, built_path, orig_path):
	"""meta is the one table the generator extends: every original row must survive unchanged and every
	generated key must be present; the hash is expected to differ."""
	problems = []
	if orig[0] != built[0]:
		problems.append("sqlite_master sql differs")
	o_rows = meta_rows(orig_path)
	b_rows = meta_rows(built_path)
	for k, v in o_rows.items():
		if k in GENERATED_META_KEYS:
			continue
		if b_rows.get(k) != v:
			problems.append("original row %r changed" % k)
	for k in GENERATED_META_KEYS:
		if k not in b_rows:
			problems.append("generated key %r missing" % k)
	return problems


def meta_rows(path):
	conn = open_ro(path)
	try:
		return dict(conn.execute('SELECT "key", "value" FROM "meta"').fetchall())
	finally:
		conn.close()


# ---------------------------------------------------------------- main

def main():
	for stream in (sys.stdout, sys.stderr):
		try:
			stream.reconfigure(errors="replace")
		except (AttributeError, ValueError):
			pass
	ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
	ap.add_argument("--db", default="all", choices=BUILD_KEYS + ["all"], help="account DBs are never built")
	ap.add_argument("--output-dir", default=None, help="default: game/server/ (with --verify: a scratch folder)")
	ap.add_argument("--header-out", default=None,
		help="schema_sql_generated.h path (default: build/generated/; with --verify: only when given)")
	ap.add_argument("--channel", default="DEV", choices=CHANNELS)
	ap.add_argument("--force", action="store_true",
		help="overwrite a target changed since its last generation; delete stale -wal/-shm/-journal files")
	ap.add_argument("--discard-backup", action="store_true",
		help="create a missing target although an interrupted install left a .<db>.bak<pid> beside it "
			"(the backup file is kept; the new DB gets the sidecar seed rows)")
	ap.add_argument("--verify", action="store_true",
		help="build into a scratch folder and compare per-table hashes with the existing DBs")
	ap.add_argument("--compare-dir", default=DEFAULT_OUTPUT_DIR, help="--verify: folder holding the existing DBs")
	args = ap.parse_args()
	keys = BUILD_KEYS if args.db == "all" else [args.db]

	try:
		if args.verify:
			compare_dir = os.path.abspath(args.compare_dir)
			out_dir = os.path.abspath(args.output_dir or DEFAULT_VERIFY_DIR)
			if same_dir(out_dir, compare_dir):
				raise BuildError("--verify must build into a folder other than the one it compares against")
			if same_dir(out_dir, DEFAULT_OUTPUT_DIR):
				raise BuildError("--verify never builds into game/server/")
			check_verify_folder(keys, out_dir)
			header_out = os.path.abspath(args.header_out) if args.header_out else None
			build(args, keys, out_dir, header_out, args.force, compare_dir, reinstall=True)
			failures = verify(keys, out_dir, compare_dir)
			print("RESULT: %s (%d failing)" % ("PASS" if failures == 0 else "FAIL", failures))
			return 0 if failures == 0 else 1
		out_dir = os.path.abspath(args.output_dir or DEFAULT_OUTPUT_DIR)
		header_out = os.path.abspath(args.header_out or DEFAULT_HEADER_OUT)
		build(args, keys, out_dir, header_out, args.force, out_dir)
	except (BuildError, OSError, sqlite3.Error) as error:
		print("build_databases: %s" % error, file=sys.stderr)
		return 1
	except KeyboardInterrupt:
		print("build_databases: interrupted", file=sys.stderr)
		return 130
	return 0


if __name__ == "__main__":
	sys.exit(main())
