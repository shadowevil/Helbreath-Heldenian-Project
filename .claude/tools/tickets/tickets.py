"""Ticket store CLI: the one way the PM and the agents create and edit tickets.

Tickets are JSON files in .claude/tickets/ (one per ticket, named after its id: the configured
id_prefix plus four digits, e.g. XY-0001.json), validated against .claude/tickets/ticket.schema.json.
The tags live in .claude/tickets/tags.json, the tag store (format 2: numbered tags, each with a name, a colour
and a description; see parse_tag_store), and a ticket's tags are the store's ids. The viewer (serve.py) only
reads them. Rules and field meanings: .claude/docs/tickets.md. author.py, beside this file, is the authoring
tool for agents (tag store, tag groups, drafts, migration); it writes through this module's writers.

    python <CLI_DIR>/tickets.py new --summary "..." --severity medium --owner <area> --author pm \
        --tags bug --body-file <path to a markdown file, or - for stdin>
    python <CLI_DIR>/tickets.py set XY-0004 --status in_progress --add-tag docs
    python <CLI_DIR>/tickets.py set XY-0004 --blocked-by XY-0002 --relates 7
    python <CLI_DIR>/tickets.py comment XY-0004 --author pm --body-file <path, or - for stdin>
    python <CLI_DIR>/tickets.py list --active --owner <area>
    python <CLI_DIR>/tickets.py show XY-0004
    python <CLI_DIR>/tickets.py validate
    python <CLI_DIR>/tickets.py import <file.json> [--dry-run]
    python <CLI_DIR>/tickets.py import --abort-interrupted
    python <CLI_DIR>/tickets.py readme

readme writes .claude/tickets/README.md, the "what changed for agents" file the viewer's one-click update rewrites
(updater.py), when it is missing. After an update, the first run prints once to stderr where it is (readme_pointer).

--tags, --add-tag and --remove-tag take tag names or ids; list and show print names. Tag groups (config.json
tag_groups, load_tag_groups) say how many tags of a set a ticket must carry; new, set and import refuse a ticket
that breaks one, and validate reports it. While a store moves from 1.2 (tags.json a name map, tickets holding
names), readers take both forms; writers refuse until `author.py tags --probe` has numbered the store, and
validate warns until `author.py --convert-all` has turned the tickets' names into ids.

Ticket links come typed (1.4.0): links.blocks, links.blocked_by and links.relates_to, optional lists that writers add
as needed; the untyped links.tickets is still read, as relates-to. A block is written on both tickets by new and set
(--block, --blocked-by, and the --remove-* flags), and validate fails a block that has one side only. comment appends
{author, at, body} to a ticket's optional comments list. Mentions of ticket ids in a body or a comment are derived,
never stored: show prints them, and validate warns about one naming a ticket the store lacks.

import creates one ticket per record of a JSON array, in input order, with the ids `new` would give and
the records' own created / updated times: for moving in from another tracker. It validates the store and
every record first and writes nothing unless all pass (the record format: import_ticket). A failure or
Ctrl+C part-way removes what the run wrote. A killed import leaves a journal and files behind: the next
import refuses to start, and --abort-interrupted removes exactly what the journal lists, once that import's
process is gone (write_import, abort_interrupted). It also removes the temporary file a killed new or set leaves,
and the one (tags.store-<pid>.tmp) a tag store write killed part-way leaves.

<CLI_DIR> is this file's folder (src/tools/tickets or tools/tickets). The repository root is the
nearest folder at or above it that holds .claude/tickets/config.json, whose keys set the project's
values (id_prefix and store_name are required; see load_config).

Stdlib only. Console output is ASCII (show re-encodes a body with errors replaced); files are read
and written as UTF-8 with LF line endings.
"""

import argparse
import contextlib
import hashlib
import json
import os
import re
import sys
import threading
import time
from datetime import datetime, timezone

script_dir = os.path.dirname(os.path.abspath(__file__))
config_relative_path = os.path.join(".claude", "tickets", "config.json")
# Every pattern below is checked with fullmatch, never match: Python's "$" also matches before a final newline,
# so a value such as "XY-" plus a newline would pass a $-anchored match.
# id_prefix: a capital letter, then capitals or digits, then a hyphen (e.g. XY- or AB2-).
id_prefix_pattern = re.compile(r"^[A-Z][A-Z0-9]*-$")
# A colour written #rrggbb, either case: tag colours and icon_color.
colour_pattern = re.compile(r"^#[0-9a-fA-F]{6}$")
default_first_ticket_number = 1
# The background make_icons.py paints: Visual Studio blue, the colour 1.1.0 always baked.
default_icon_color = "#007acc"


class ticket_error(Exception):
	pass


class tag_store_error(ticket_error, ValueError):
	"""tags.json reads but cannot be used. A ValueError too, so a reader that catches (OSError, ValueError) around
	load_tag_store, as the viewer does, catches it with the unreadable cases."""
	pass


def reject_constant(name):
	"""json's parse_constant: NaN, Infinity and -Infinity are not JSON, though Python's json reads them by default. Refused,
	a file holding one does not read, and the viewer never sends one on (a browser cannot parse it). Defined here, above
	configure(), which reads config.json at import."""
	raise ValueError("{} is not a JSON value".format(name))


def find_repo_root(start):
	"""The nearest folder at or above start that holds .claude/tickets/config.json, or None."""
	folder = start
	while True:
		if os.path.isfile(os.path.join(folder, config_relative_path)):
			return folder
		parent = os.path.dirname(folder)
		if parent == folder:
			return None
		folder = parent


def default_viewer_service(store_name):
	"""Lowercase store_name, runs of anything but a-z and 0-9 -> '-', trimmed, plus '-tickets'."""
	slug = re.sub(r"[^a-z0-9]+", "-", store_name.lower()).strip("-")
	return slug + "-tickets" if slug else "tickets"


def load_config(path):
	"""The project's settings from config.json, with every optional key filled in.

	Required: id_prefix, store_name. Optional: first_ticket_number (1), short_name (store_name), page_title (the
	viewer's title and heading, any string, used as written; store_name + ' Tickets'),
	viewer_service (from store_name), theme_key (viewer_service + '-theme'), server_version
	(viewer_service), icon_text (id_prefix without the hyphen, at most 3 characters), icon_color (the
	icons' background as #rrggbb, either case, kept as written; #007acc). viewer_port is read by
	serve.py only. Unknown keys are kept and ignored.
	"""
	name = ".claude/tickets/config.json"
	try:
		with open(path, "r", encoding="utf-8-sig") as handle:
			config = json.load(handle, parse_constant=reject_constant)
	except OSError as error:
		raise ticket_error("{}: unreadable ({})".format(name, error))
	except (ValueError, RecursionError) as error:
		raise ticket_error("{}: not valid JSON ({})".format(name, error))
	if not isinstance(config, dict):
		raise ticket_error(name + ": expected a JSON object of settings")
	for key in ("id_prefix", "store_name"):
		if key not in config:
			raise ticket_error("{}: missing required key '{}'".format(name, key))
	if not isinstance(config["id_prefix"], str) or not id_prefix_pattern.fullmatch(config["id_prefix"]):
		raise ticket_error("{}: id_prefix must be a capital letter, then capitals or digits, then '-' "
			"(e.g. XY-), not {}".format(name, json.dumps(config["id_prefix"])))
	if not isinstance(config["store_name"], str) or not config["store_name"].strip():
		raise ticket_error(name + ": store_name must be a non-empty string")
	config.setdefault("first_ticket_number", default_first_ticket_number)
	number = config["first_ticket_number"]
	if not isinstance(number, int) or isinstance(number, bool) or number < 0:
		raise ticket_error(name + ": first_ticket_number must be a whole number, 0 or more")
	config.setdefault("short_name", config["store_name"])
	# The viewer's page title and heading, used as written (1.4.0); without it, store_name plus " Tickets".
	config.setdefault("page_title", config["store_name"] + " Tickets")
	if not isinstance(config["page_title"], str):
		raise ticket_error("{}: page_title must be a string, not {}".format(name, json.dumps(config["page_title"])))
	config.setdefault("viewer_service", default_viewer_service(config["store_name"]))
	if isinstance(config["viewer_service"], str):
		config.setdefault("theme_key", config["viewer_service"] + "-theme")
		config.setdefault("server_version", config["viewer_service"])
	config.setdefault("icon_text", config["id_prefix"].rstrip("-")[:3])
	config.setdefault("icon_color", default_icon_color)
	if not isinstance(config["icon_color"], str) or not colour_pattern.fullmatch(config["icon_color"]):
		raise ticket_error("{}: icon_color must be a colour written #rrggbb (e.g. {}), not {}".format(
			name, default_icon_color, json.dumps(config["icon_color"])))
	return config


def configure():
	"""Find the repository and read its config; a clean ERROR and exit 1 when either is missing."""
	root = find_repo_root(script_dir)
	if root is None:
		print("ERROR no .claude/tickets/config.json in {} or any folder above it; the tool must sit "
			"inside the repository whose store it serves".format(script_dir).encode("ascii", "replace").decode("ascii"))
		sys.exit(1)
	try:
		return root, load_config(os.path.join(root, config_relative_path))
	except ticket_error as error:
		print("ERROR " + str(error).encode("ascii", "replace").decode("ascii"))
		sys.exit(1)


repo_root, config = configure()
tickets_dir = os.path.join(repo_root, ".claude", "tickets")
schema_path = os.path.join(tickets_dir, "ticket.schema.json")
tags_path = os.path.join(tickets_dir, "tags.json")
config_path = os.path.join(tickets_dir, "config.json")

id_prefix = config["id_prefix"]
# Four digits keep the file names sorting; the schema accepts more once past 9999.
id_digits = 4
first_ticket_number = config["first_ticket_number"]
id_pattern = re.compile("^" + re.escape(id_prefix) + r"[0-9]{4,}$")
# A ticket file's number, and a journal's ids, are of sane length: at most this many digits, more than any store reaches
# (and than first_ticket_number has). A longer one is no ticket name (validate reports the file, too_long_names), so a
# hand-made file can neither move `new` to an absurd id nor end a command in int()'s digit limit.
id_max_digits = max(9, len(str(first_ticket_number)) + 1)
ticket_file_pattern = re.compile("^" + re.escape(id_prefix) + r"([0-9]{4," + str(id_max_digits) + r"})\.json$")
# The highest ticket number that fits (id_for_number).
id_number_max = 10 ** id_max_digits - 1
# What ticket_file_pattern would match but for the digit count.
long_ticket_file_pattern = re.compile("^" + re.escape(id_prefix) + r"([0-9]{" + str(id_max_digits + 1) + r",})\.json$")
# A ticket id mentioned in text (mentioned_ids): the prefix after a non-word character, digits not followed by one.
mention_id_pattern = re.compile(r"(?<![\w-])(" + re.escape(id_prefix) + r"[0-9]{4," + str(id_max_digits) + r"})(?![\w])")
# Canonical field order on disk, so every file diffs the same way.
field_order = ["id", "summary", "status", "severity", "owner_area", "target_patch", "tags",
	"author", "created", "updated", "links", "body", "comments"]
# The links every ticket carries (the schema requires them; `new` writes them empty).
link_kinds = ["commits", "files", "tickets"]
# The typed ticket links (1.4.0): optional, so a ticket from before them validates unchanged; a writer adds one when a
# ticket gets its first such link and drops it again when the list empties. links.tickets, the untyped list from
# before, is read as relates-to.
typed_link_kinds = ["blocks", "blocked_by", "relates_to"]
ticket_link_kinds = ["tickets"] + typed_link_kinds
# A block is written on both tickets: A blocks B is B blocked_by A. Writers write the other side too (other_sides);
# validate reports a pair that has only one side (block_pair_errors).
reverse_link = {"blocks": "blocked_by", "blocked_by": "blocks"}
# A mention of a ticket in a body or a comment (mention_id_pattern): inline code and fenced code blocks are left out,
# as the viewer leaves code alone when it links ids.
code_pattern = re.compile(r"```.*?(?:```|\Z)|~~~.*?(?:~~~|\Z)|`[^`\n]*`", re.S)
# Every status but done and wont_do. user_review (the user is to look at it) and verified (reviewed and green, only
# landing it left) are 1.4.0; a store whose schema lacks them lists the same tickets.
active_statuses = ["open", "in_progress", "in_review", "user_review", "verified", "blocked"]
default_target_patch = "next"
# Column widths for `list`; the summary takes what is left of a 120-column console.
list_summary_width = 64
# What utc_now() writes, and the only form `import` takes for created and updated.
timestamp_format = "%Y-%m-%dT%H:%M:%SZ"
timestamp_pattern = re.compile(r"^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z$")
# `import`: a record's keys. owner and target are the names `new` uses; the ticket file's own names work too.
import_required = ["summary", "severity", "owner_area", "author"]
import_aliases = {"owner_area": ["owner", "owner_area"], "target_patch": ["target", "target_patch"]}
import_keys = ["summary", "severity", "owner", "owner_area", "author", "status", "target", "target_patch",
	"tags", "body", "links", "comments", "created", "updated", "id"]
# At most this many FAIL lines are printed, each cut to this many characters.
import_shown_failures = 100
import_failure_width = 300
# A file another process holds for a moment (the viewer reading it, a scanner) is tried again this often.
busy_file_attempts = 5
busy_file_wait_seconds = 0.2
# The store's reads (read_store_json) wait at most this long in all, per command (the viewer: per request); after
# that a busy read fails at once. busy_read_state.spent is what this thread has waited so far: per thread, so the
# viewer's concurrent requests each have their own (reset_busy_read_budget).
busy_read_budget_seconds = 3.0
busy_read_state = threading.local()
# `new` takes the next id again this often when another writer took the one it chose.
new_id_attempts = 20
# The fields that are one line each: `list` and the viewer print them in a row.
one_line_fields = ["summary", "target_patch"]
# What ends a line there: everything str.splitlines() splits on (LF, CR, VT, FF, FS, GS, RS, NEL, U+2028, U+2029).
line_break_characters = "\n\r\x0b\x0c\x1c\x1d\x1e\x85\u2028\u2029"
# What an import leaves in the tickets folder while it runs, and when it is killed: its journal
# (journal.import-<pid>.tmp) and its temporary tickets (<ticket id>.import-<pid>.tmp). `new` and `set` write
# <ticket id>.new-<pid>.tmp and <ticket id>.set-<pid>.tmp (temp_path); a tag store write (write_tag_store, from
# author.py) writes tags.store-<pid>.tmp. Exactly these names, and no others, are what --abort-interrupted removes. No
# name here is a ticket file's. A pid is a positive number of at most 10 digits.
pid_text = r"([1-9][0-9]{0,9})"
import_journal_pattern = re.compile(r"^journal\.import-" + pid_text + r"\.tmp$")
import_temp_pattern = re.compile("^(" + re.escape(id_prefix) + r"[0-9]{4,})\.import-" + pid_text + r"\.tmp$")
writer_temp_pattern = re.compile("^(" + re.escape(id_prefix) + r"[0-9]{4,})\.(?:new|set)-" + pid_text + r"\.tmp$")
tag_store_temp_pattern = re.compile(r"^tags\.store-" + pid_text + r"\.tmp$")
sha256_pattern = re.compile(r"^[0-9a-f]{64}$")
# A process that started more than this many seconds after a file was last written did not write it: the file's pid
# now belongs to a later process (pid_state, on Windows). Wide, because the file's time may come from another clock (a
# network share's server): 5 minutes is the clock skew Kerberos tolerates in a Windows domain. Wider only makes a
# cleanup wait longer after a pid is reused, never remove a running writer's files.
pid_reuse_slack_seconds = 300.0
# --abort-interrupted removes a ticket of the batch only when it was last written no more than this many seconds before
# the journal (the import writes its journal first): file times are that coarse on some filesystems (FAT).
journal_mtime_slack_seconds = 2.0
# At most this many names are listed in one message.
shown_names = 5
# The tag store (tags.json). Format 2 numbers the tags; format 1 (1.0.0 to 1.2.2) was a name -> {color,
# description} map, still read (numbered in name order, in memory) and refused by every writer.
tag_store_format_version = 2
# A tag name: lowercase letters, digits and underscores, with at least one letter or underscore, because an int
# or a digit string always means a tag id. Names match exactly; nothing folds case.
tag_name_pattern = re.compile(r"^(?=.*[a-z_])[a-z0-9_]+$")
tag_id_text_pattern = re.compile(r"^[0-9]+$")
# Where an all-digit 1.2 tag name is renamed, and an int tag value on a 1.2 ticket replaced: by hand, since 1.2 has no
# command for either (validate and the probe say so).
all_digit_rename_steps = "the ticket system's docs/UPGRADING.md sect. 7 step 7"
# A tag id as a tags.json key: a positive whole number without leading zeros, of sane length.
tag_id_key_pattern = re.compile(r"^[1-9][0-9]{0,8}$")
# What a store tag holds beyond its name.
default_tag_color = "#808080"
# Every read-modify-write of tags.json holds tags.json.lock (tag_store_lock): a writer waits this long for it,
# trying again this often, then refuses naming the file.
tag_lock_wait_seconds = 30.0
tag_lock_poll_seconds = 0.05
# config.json tag_groups: the keys a group may carry.
tag_group_keys = ["name", "tags", "min", "max"]
# What a ticket's tags node is checked as, whatever the project's schema says (read_schema): the items are
# checked by store_errors, so a schema from before 1.3.0 (names only) keeps working once tickets hold ids.
tags_schema_node = {"type": "array", "uniqueItems": True}


def utc_now():
	return datetime.now(timezone.utc).replace(microsecond=0).strftime(timestamp_format)


def read_json(path):
	"""A JSON file's content. Raises OSError, or ValueError when it is not JSON (NaN and Infinity included,
	reject_constant) or nests too deeply to read (json's RecursionError, which every caller then handles as a file that
	does not read)."""
	with open(path, "r", encoding="utf-8") as handle:
		try:
			return json.load(handle, parse_constant=reject_constant)
		except RecursionError:
			raise ValueError("nested too deeply to read")


def ordered(ticket):
	result = {name: ticket[name] for name in field_order if name in ticket}
	for name in ticket:
		if name not in result:
			result[name] = ticket[name]
	return result


def ticket_text(ticket):
	return json.dumps(ordered(ticket), indent=2, ensure_ascii=False) + "\n"


def write_failed(ticket, error, outcome):
	return ticket_error("could not write {}.json ({}); {}".format(ticket["id"], one_line(error), outcome))


def write_temp(ticket, kind, outcome):
	"""The ticket written to a temporary name beside its file (temp_path); the temporary file's path. outcome is
	what a failure leaves, for the message: the ticket is unchanged, or no ticket was created."""
	temp = temp_path(ticket["id"], kind)
	try:
		with open(temp, "w", encoding="utf-8", newline="\n") as handle:
			handle.write(ticket_text(ticket))
	except BaseException as error:
		remove_created([temp])
		if isinstance(error, OSError):
			raise write_failed(ticket, error, outcome)
		raise
	return temp


def write_ticket(ticket, command="set"):
	"""Rewrite an existing ticket (`set`): written whole to a temporary name, then moved over the file (os.replace,
	tried again for a moment while Windows reports the file in use). It never writes into the file in place. Any failure,
	and Ctrl+C, leaves the ticket as it was and removes the temporary file; a move that stays refused (another process
	keeps the file open, e.g. an editor) ends in an ERROR naming the ticket. Only a killed process leaves its temporary
	file, which --abort-interrupted removes once that process is gone."""
	path = ticket_path(ticket["id"])
	unchanged = "the ticket is unchanged"
	temp = write_temp(ticket, "set", unchanged)
	try:
		when_not_busy(os.replace, temp, path)
	except BaseException as error:
		left = remove_created([temp])
		if not isinstance(error, OSError):
			raise
		outcome = unchanged
		detail = error
		if isinstance(error, PermissionError):
			# Windows refuses the move while another process keeps the file open. Writing into the file instead could
			# leave it cut off, so the command fails.
			detail = "moving the new version over it was refused: {}".format(error.strerror or error)
			# command: what to run again, as the user typed it (set, or new for the other side of a new ticket's block).
			outcome += "; another process may hold the file open: close it, then run {} again".format(command)
		if left:
			outcome += "; its temporary file {} is left: run `tickets.py import --abort-interrupted`".format(left[0])
		raise write_failed(ticket, detail, outcome)
	return path


def new_block_changes(ticket):
	"""The block changes a new ticket makes on the tickets it names (other_sides): each of its blocks and blocked_by."""
	links = ticket.get("links")
	if not isinstance(links, dict):
		return []
	return [(kind, True, other) for kind in sorted(reverse_link) if isinstance(links.get(kind), list)
		for other in links[kind]]


def create_ticket(ticket):
	"""Write a new ticket (`new`) under the next id, with the other side of each of its blocks (other_sides); the ids of
	the other tickets written. The file is created exclusively: when another writer took the id meanwhile, the next one
	is taken instead, and no file is ever replaced. The ticket and every other ticket it changes are checked before
	anything is written, and they are written together or not at all, as `set` writes: when an other side fails, the
	new ticket is removed again (write_together puts back the other sides already written)."""
	no_ticket = "no ticket was created"
	for attempt in range(new_id_attempts):
		ticket["id"] = next_id()
		if attempt == 0:
			# Once is enough: a later id changes no answer. A link to it was refused here as dangling.
			check_before_write(ticket)
		# The other sides name this id, so they are read and checked again for each id tried, the new id counted as known.
		pairs = other_sides(ticket["id"], new_block_changes(ticket))
		for other, _ in pairs:
			check_before_write(other, [ticket["id"]])
		temp = write_temp(ticket, "new", no_ticket)
		placed = []
		try:
			place_new_file(temp, ticket_path(ticket["id"]), placed)
		except FileExistsError:
			continue
		except OSError as error:
			raise write_failed(ticket, error, no_ticket)
		finally:
			remove_created([temp])
		try:
			write_together(pairs, "new")
		except BaseException as error:
			left = remove_created(placed)
			if not isinstance(error, ticket_error):
				raise
			outcome = no_ticket if not left else "{} could not be removed again: remove it, then run validate".format(left[0])
			raise ticket_error("the other side of its blocks was not written ({}); {}".format(error, outcome))
		return [other["id"] for other, _ in pairs]
	raise ticket_error("no free id after {} tries: other writers kept taking the next one; run it again".format(
		new_id_attempts))


def ticket_path(ticket_id):
	return os.path.join(tickets_dir, ticket_id + ".json")


def normalize_id(text):
	value = text.strip().upper()
	if value.isdigit():
		value = id_prefix + value.zfill(id_digits)
	if not id_pattern.fullmatch(value) or len(value) - len(id_prefix) > id_max_digits:
		raise ticket_error("not a ticket id: " + text)
	return value


def ticket_files():
	if not os.path.isdir(tickets_dir):
		return []
	names = [name for name in os.listdir(tickets_dir) if ticket_file_pattern.fullmatch(name)]
	return sorted(names, key=lambda name: int(ticket_file_pattern.fullmatch(name).group(1)))


def too_long_names():
	"""One validate problem per file named like a ticket but with a number of more than id_max_digits digits: no
	reader takes it as a ticket, so it would otherwise go unseen. The name itself is not echoed (it may be huge)."""
	try:
		names = sorted(name for name in os.listdir(tickets_dir) if long_ticket_file_pattern.fullmatch(name))
	except OSError:
		return []
	return ["{}<{} digits>.json: not a ticket file name, the number has more than {} digits; no command reads it as a "
		"ticket: rename or remove it".format(id_prefix, len(long_ticket_file_pattern.fullmatch(name).group(1)), id_max_digits)
		for name in names]


# --- the tag store: tags.json --------------------------------------------------------------------

def empty_tag_store():
	return {"format": tag_store_format_version, "next_id": 1, "tags": {}}


def raw_tag_format(raw):
	"""2 when tags.json's content carries a whole-number "format" key, else 1 (a name map; a tag named "format"
	there maps to an object, never a number)."""
	if isinstance(raw, dict) and isinstance(raw.get("format"), int) and not isinstance(raw.get("format"), bool):
		return 2
	return 1


def store_entry(name, entry):
	"""A store tag as written: name, color, description first, then any other key the file gave it."""
	result = {"name": name, "color": entry.get("color"), "description": entry.get("description", "")}
	for key, value in entry.items():
		if key not in result:
			result[key] = value
	return result


def parse_tag_store(raw, invalid=None):
	"""tags.json's content as (store, format, problems). invalid, when a set is given, receives the id (int, format 2)
	and the name (str) of every tag that tags.json holds but that does not read, so that a ticket carrying one is told
	apart from one carrying a tag tags.json lacks (ticket_tag_ids).

	store is the format-2 dict, {"format": 2, "next_id": n, "tags": {"<id>": {"name", "color", "description"}}},
	with every tag that reads; None when the content cannot be used at all. format is the one the file is
	written in: 2, or 1 for the name -> {color, description} map of 1.0.0 to 1.2.2, which is numbered here in
	name order (1, 2, ...), in memory only. problems are one-line messages naming tags.json.

	A format-1 file's store keeps "format": 1. Its ids are provisional (for listing only): resolve_tag never resolves
	an id through them, so a ticket's integer or all-digit value finds no tag until `author.py tags --probe` has
	numbered the file, and ticket_tag_ids reports it as the probe does. The probe sets "format" to 2 before writing.

	Format 2: ids are the keys, positive whole numbers below next_id, never reused or renumbered; names are
	unique and match ^[a-z0-9_]+$; color is #rrggbb; description is a string (absent reads as ""). Other keys are
	kept as they are.
	"""
	name = "tags.json"
	if not isinstance(raw, dict):
		return None, 1, [name + ": expected a JSON object"]
	problems = []
	bad = invalid if invalid is not None else set()
	if raw_tag_format(raw) == 1:
		store = empty_tag_store()
		store["format"] = 1
		for tag_name in sorted(raw):
			entry = raw[tag_name]
			if tag_id_text_pattern.fullmatch(tag_name):
				problems.append("{}: the tag name '{}' is all digits, which reads as a tag id; rename it by hand first, in "
					"tags.json and on every ticket carrying it ({})".format(name, tag_name, all_digit_rename_steps))
				continue
			if not tag_name_pattern.fullmatch(tag_name):
				problems.append("{}: bad tag name '{}'".format(name, one_line(tag_name)))
				bad.add(tag_name)
				continue
			if not isinstance(entry, dict) or not colour_pattern.fullmatch(str(entry.get("color", ""))):
				problems.append("{}: '{}' needs a #rrggbb color".format(name, tag_name))
				bad.add(tag_name)
				continue
			if not isinstance(entry.get("description", ""), str):
				problems.append("{}: '{}': description must be a string".format(name, tag_name))
				bad.add(tag_name)
				continue
			store["tags"][str(store["next_id"])] = store_entry(tag_name, entry)
			store["next_id"] += 1
		return store, 1, problems
	if raw["format"] != tag_store_format_version:
		return None, 2, ["{}: format {} is not one this tool reads ({}); upgrade the ticket system".format(
			name, raw["format"], tag_store_format_version)]
	next_tag = raw.get("next_id")
	if not isinstance(next_tag, int) or isinstance(next_tag, bool) or next_tag < 1:
		return None, 2, [name + ": next_id must be a whole number, 1 or more"]
	if not isinstance(raw.get("tags"), dict):
		return None, 2, [name + ": tags must be an object of tag id -> {name, color, description}"]
	store = {key: value for key, value in raw.items() if key != "tags"}
	store["tags"] = {}
	names = set()
	for key in sorted(raw["tags"], key=lambda key: (len(key), key)):
		entry = raw["tags"][key]
		where = "{}: tag {}".format(name, one_line(key))
		if not tag_id_key_pattern.fullmatch(key):
			problems.append(where + ": an id is a positive whole number written without leading zeros")
			continue
		# From here on the entry is tags.json's tag of this id, read or not.
		bad.add(int(key))
		tag_name = entry.get("name") if isinstance(entry, dict) else None
		if int(key) >= next_tag:
			problems.append("{}: is not below next_id {} (ids are given from next_id and never reused)".format(where, next_tag))
			if isinstance(tag_name, str) and tag_name_pattern.fullmatch(tag_name):
				bad.add(tag_name)
			continue
		if not isinstance(entry, dict):
			problems.append(where + ": expected an object {name, color, description}")
			continue
		if not isinstance(tag_name, str) or not tag_name_pattern.fullmatch(tag_name):
			problems.append("{}: bad tag name {} (lowercase letters, digits and _, with at least one letter or _)".format(
				where, shown(tag_name)))
			continue
		if tag_name in names:
			problems.append("{}: the name '{}' is taken by another id".format(where, tag_name))
			continue
		if not colour_pattern.fullmatch(str(entry.get("color", ""))):
			problems.append("{} ('{}'): needs a #rrggbb color".format(where, tag_name))
			bad.add(tag_name)
			continue
		if not isinstance(entry.get("description", ""), str):
			problems.append("{} ('{}'): description must be a string".format(where, tag_name))
			bad.add(tag_name)
			continue
		bad.discard(int(key))
		names.add(tag_name)
		store["tags"][key] = store_entry(tag_name, entry)
	return store, 2, problems


def tag_store_state(invalid=None):
	"""(store, format, problems) of tags.json as parse_tag_store gives them (invalid as there); (None, 0, [why]) when it
	does not read."""
	try:
		raw = read_tag_file()
	except (OSError, ValueError) as error:
		return None, 0, ["tags.json: unreadable ({})".format(one_line(error))]
	return parse_tag_store(raw, invalid)


def read_tag_file():
	"""tags.json's content, read from tags_path as it is now (serve.py --data-dir moves it). Raises OSError, or
	ValueError when it is not JSON or nests too deeply to read (read_json). Windows refuses the open for a moment while a
	writer moves the new tags.json into place; it is tried again then (when_not_busy)."""
	return read_store_json(tags_path)


def load_tag_store_with_format():
	"""(store, format): load_tag_store's store and the format the file is written in (1 or 2), from one read."""
	store, version, problems = parse_tag_store(read_tag_file())
	if store is None:
		raise tag_store_error(ascii_text(one_line("; ".join(problems))))
	return store, version


def load_tag_store():
	"""The tag store as the format-2 dict (parse_tag_store), for readers: a format-1 file comes back converted, in
	memory, and nothing is written. A tag that does not read is left out (validate names it). Raises OSError from
	reading, or ValueError when tags.json is not JSON or cannot be used at all (tag_store_error, which is also a
	ticket_error, with an ASCII one-line message)."""
	return load_tag_store_with_format()[0]


def tag_store_format():
	"""The format tags.json is written in: 2, or 1 for the name map of 1.0.0 to 1.2.2. Raises OSError or ValueError
	as load_tag_store does."""
	return raw_tag_format(read_tag_file())


def writable_tag_store():
	"""The tag store for a writer: refused while tags.json is in format 1 or has any problem."""
	store, version, problems = tag_store_state()
	if store is not None and version == 1:
		raise ticket_error("tags.json is in the old format (a name -> colour map); every writer refuses until it is "
			"numbered: run author.py tags --probe, then try again")
	if problems:
		raise ticket_error("tags.json has {} problem(s); fix them first: {}".format(len(problems),
			"; ".join(problems[:shown_names]) + ("; ..." if len(problems) > shown_names else "")))
	return store


# The lock this process holds: how deep tag_store_lock is nested, and the line written into tags.json.lock.
tag_lock_depth = 0
tag_lock_token = None


def tag_lock_path():
	return tags_path + ".lock"


@contextlib.contextmanager
def tag_store_lock():
	"""Hold tags.json.lock, beside tags.json, for one read-modify-write of the tag store: read the store inside it, change
	it, write it (write_tag_store refuses without the lock), then leave. Nested use in one process holds it once.

	The lock is a file created exclusively (os.open with O_CREAT | O_EXCL, on Windows and POSIX alike) holding the
	holder's pid, the time and a random token. A writer that finds it waits up to tag_lock_wait_seconds, then refuses
	with an ERROR naming the file, its holder and its age. Nothing breaks a lock automatically: a pid cannot be checked
	safely on every system, and a lock taken from a live writer would let two write at once. A process killed while it
	held the lock (rare: it is held for one command, and Ctrl+C still releases it) leaves the file behind; the ERROR says
	to delete it once no tickets.py or author.py is running. Readers never take the lock."""
	global tag_lock_depth
	if tag_lock_depth:
		tag_lock_depth += 1
		try:
			yield
		finally:
			tag_lock_depth -= 1
		return
	lock = tag_lock_path()
	acquire_tag_lock(lock)
	tag_lock_depth = 1
	try:
		yield
	finally:
		tag_lock_depth = 0
		release_tag_lock(lock)


def acquire_tag_lock(lock):
	"""Create the lock file, waiting while another writer holds it; a ticket_error when the wait runs out."""
	global tag_lock_token
	deadline = time.monotonic() + tag_lock_wait_seconds
	denied = 0
	while True:
		try:
			handle = os.open(lock, os.O_CREAT | os.O_EXCL | os.O_WRONLY)
		except FileExistsError:
			pass
		except PermissionError as error:
			# Windows refuses the create while the holder's delete of the file is under way (or a scanner holds it). With
			# no lock file there, a refusal that keeps coming means the folder cannot be written.
			if not os.path.lexists(lock):
				denied += 1
				if denied >= busy_file_attempts:
					raise ticket_error("cannot create the tag store lock {} ({}); tags.json is unchanged".format(
						one_line(lock), one_line(error.strerror or error)))
		except OSError as error:
			raise ticket_error("cannot create the tag store lock {} ({}); tags.json is unchanged".format(
				one_line(lock), one_line(error.strerror or error)))
		else:
			token = "pid {} at {} token {}".format(os.getpid(), utc_now(), os.urandom(8).hex())
			try:
				os.write(handle, (token + "\n").encode("ascii"))
			except OSError:
				# The content only describes the holder; the file is the lock, released as the empty file it is.
				token = ""
			except BaseException:
				os.close(handle)
				remove_created([lock])
				raise
			os.close(handle)
			tag_lock_token = token
			return
		if time.monotonic() >= deadline:
			raise ticket_error(tag_lock_held(lock))
		time.sleep(tag_lock_poll_seconds)


def tag_lock_held(lock):
	"""The refusal when the lock stays taken: the file, who holds it and since when, and what to do."""
	holder = "holder unknown"
	try:
		with open(lock, "r", encoding="ascii", errors="replace") as handle:
			line = handle.readline().strip()
		if line:
			holder = line.split(" token ")[0]
		holder += ", {:.0f} s old".format(max(0.0, time.time() - os.path.getmtime(lock)))
	except OSError:
		pass
	return ("tags.json is locked: {} ({}) was not released within {:.0f} s. Another tickets.py or author.py is changing "
		"the tag store: try again. If none is running (one was killed while it held the lock), delete that file and try "
		"again; tags.json is unchanged".format(one_line(lock), holder, tag_lock_wait_seconds))


def release_tag_lock(lock):
	"""Delete the lock file, only while it still holds this process's token: a lock deleted by hand and taken by another
	writer meanwhile is that writer's. A lock that cannot be deleted is reported on stderr: the command itself worked."""
	global tag_lock_token
	token, tag_lock_token = tag_lock_token, None
	try:
		with open(lock, "r", encoding="ascii", errors="replace") as handle:
			if handle.readline().strip() != token:
				return
		when_not_busy(os.unlink, lock)
	except FileNotFoundError:
		pass
	except OSError as error:
		sys.stderr.write(ascii_text("WARNING could not delete the tag store lock {} ({}); delete it once no tickets.py or "
			"author.py is running\n".format(one_line(lock), one_line(error))))


def write_tag_store(store):
	"""Write the format-2 store to tags.json whole: to a temporary name, then moved over the file, so a failure leaves
	tags.json as it was. The store is checked first and refused when it would not read back the same. The caller holds
	tag_store_lock and read the store inside it, so that no other writer's change is lost. Only a killed process leaves
	its temporary file (tag_store_temp_pattern), which --abort-interrupted removes once that process is gone."""
	if not tag_lock_depth:
		raise ticket_error("tags.json is written only under the tag store lock (tag_store_lock); nothing written")
	again, version, problems = parse_tag_store(store)
	if again is None or version != tag_store_format_version or problems:
		raise ticket_error("refusing to write an invalid tag store: " + "; ".join(problems or ["not format 2"]))
	ordered_store = {"format": tag_store_format_version, "next_id": store["next_id"]}
	ordered_store.update({key: value for key, value in store.items() if key not in ("format", "next_id", "tags")})
	ordered_store["tags"] = {key: store_entry(store["tags"][key]["name"], store["tags"][key])
		for key in sorted(store["tags"], key=int)}
	text = json.dumps(ordered_store, indent=2, ensure_ascii=False) + "\n"
	temp = os.path.join(tickets_dir, "tags.store-{}.tmp".format(os.getpid()))
	try:
		with open(temp, "w", encoding="utf-8", newline="\n") as handle:
			handle.write(text)
		when_not_busy(os.replace, temp, tags_path)
	except BaseException as error:
		remove_created([temp])
		if isinstance(error, OSError):
			raise ticket_error("could not write tags.json ({}); it is unchanged".format(one_line(error)))
		raise
	return tags_path


def load_tags():
	"""The 1.2 view of the store, kept for the transition: {name: {"color", "description"}}, from either format.
	Raises OSError or ValueError as load_tag_store does."""
	store = load_tag_store()
	return {entry["name"]: {"color": entry["color"], "description": entry["description"]}
		for _, entry in sorted(store["tags"].items(), key=lambda item: int(item[0]))}


def tag_names_by_id(store):
	"""{int id: name} for every tag of the store."""
	return {int(key): entry["name"] for key, entry in store["tags"].items()}


def tag_ids_by_name(store):
	return {entry["name"]: int(key) for key, entry in store["tags"].items()}


def is_tag_id(value):
	return isinstance(value, int) and not isinstance(value, bool) and value >= 1


def is_numbered(store):
	"""False for a format-1 store (parse_tag_store): its ids are provisional and resolve nothing."""
	return store.get("format") == tag_store_format_version


def unnumbered_id_problem(value):
	"""An int id on a ticket while tags.json is in format 1: validate and the probe say it alike."""
	return "tag id {} on a ticket while tags.json is not numbered yet; replace it with the tag's name ({})".format(
		value, all_digit_rename_steps)


def all_digit_name_problem(value):
	"""A digit string on a ticket while tags.json is in format 1, where it is a 1.2 tag name: validate and the probe
	say it alike."""
	return "the tag name '{}' is all digits, which reads as a tag id; rename it by hand first ({})".format(
		ascii_text(one_line(value)), all_digit_rename_steps)


def resolve_tag(store, value, by_name=None):
	"""The int id of a tag given as an int id, a digit string (always an id) or a name (matched exactly: names are
	lowercase and nothing folds case). None when the store has no such tag, or for any other value, and for every id
	while the store is in format 1 (is_numbered)."""
	if is_tag_id(value):
		return value if is_numbered(store) and str(value) in store["tags"] else None
	if not isinstance(value, str):
		return None
	if tag_id_text_pattern.fullmatch(value):
		# int() of a huge digit string is slow and limited; no id is that long (tag_id_key_pattern).
		return int(value) if is_numbered(store) and len(value) <= 9 and str(int(value)) in store["tags"] else None
	return (by_name if by_name is not None else tag_ids_by_name(store)).get(value)


def stored_tag_id(store, value, by_name=None):
	"""The int id of one value of a ticket file's tags: an int (or a digit string) is an id, any other string is a name
	(1.2 tickets). None when the store has no such tag."""
	return resolve_tag(store, value, by_name)


def tag_label(store, value):
	"""How a ticket's tag value prints: the tag's name, or the value itself when the store does not know it."""
	if store is not None:
		tag_id = stored_tag_id(store, value)
		if tag_id is not None:
			return store["tags"][str(tag_id)]["name"]
	return str(value)


def ticket_tag_ids(store, tags, invalid=()):
	"""A ticket file's tags resolved through the store: (ids in order, once each; problems, one string each). While the
	store is in format 1, an int or all-digit value is a problem, worded as the probe words it. invalid: the ids and
	names of the tags tags.json holds but cannot read (parse_tag_store), reported as such rather than as unknown."""
	by_name = tag_ids_by_name(store)
	ids = []
	problems = []
	for value in tags:
		if is_tag_id(value) or (isinstance(value, str) and tag_id_text_pattern.fullmatch(value)):
			if not is_numbered(store):
				problems.append(unnumbered_id_problem(value) if is_tag_id(value) else all_digit_name_problem(value))
				continue
			tag_id = stored_tag_id(store, value, by_name)
			number = value if is_tag_id(value) else (int(value) if len(value) <= 9 else None)
			if tag_id is None and number in invalid:
				problems.append("tag id {}: its entry in tags.json is invalid (the tags.json problem for tag {} says "
					"why)".format(number, number))
			elif tag_id is None:
				problems.append("unknown tag id {} (not in tags.json)".format(shown(value)))
		elif isinstance(value, str) and tag_name_pattern.fullmatch(value):
			tag_id = by_name.get(value)
			if tag_id is None and value in invalid:
				problems.append("tag '{}': its entry in tags.json is invalid (the tags.json problem for it says "
					"why)".format(value))
			elif tag_id is None:
				problems.append("unknown tag '{}' (not in tags.json: run author.py tags --probe, or add it with "
					"author.py tags add)".format(value))
		else:
			problems.append("tag {} is neither a tag id (a whole number, 1 or more) nor a tag name".format(shown(value)))
			continue
		if tag_id is not None:
			if tag_id in ids:
				problems.append("tag '{}' (id {}) is given twice".format(store["tags"][str(tag_id)]["name"], tag_id))
			else:
				ids.append(tag_id)
	return ids, problems


def input_tag_ids(store, values):
	"""Tags as typed (--tags, --add-tag, --remove-tag) or sent (an import record, author.py): names or ids, resolved
	with resolve_tag. (ids in order, once each; problems, one string each)."""
	by_name = tag_ids_by_name(store)
	ids = []
	problems = []
	for value in values:
		if not isinstance(value, (int, str)) or isinstance(value, bool):
			problems.append("tag {} is neither a tag id nor a tag name".format(shown(value)))
			continue
		tag_id = resolve_tag(store, value, by_name)
		if tag_id is None:
			problems.append("unknown tag {} (not in tags.json; author.py tags lists the store)".format(shown(value)))
		elif tag_id not in ids:
			ids.append(tag_id)
	return ids, problems


def resolve_input_tags(store, values):
	"""input_tag_ids for a command: the ids, or a ticket_error naming every tag it does not know."""
	ids, problems = input_tag_ids(store, values)
	if problems:
		raise ticket_error("; ".join(problems))
	return ids


# --- tag groups: config.json tag_groups ----------------------------------------------------------

def load_tag_groups(store, invalid=()):
	"""config.json's tag_groups resolved through the store: [{"name", "tags": [names], "ids": [ids], "min", "max"}];
	[] when the key is absent. Raises ticket_error naming every problem.

	A group is {"name": "type", "tags": ["bug", "cleanup", "docs"], "min": 1, "max": 1}: a ticket carries at least
	min (default 0) and at most max (default null: any number) of the group's tags. Names are tag names, as people
	read them; a name the store does not hold is a config error. A tag may sit in more than one group. invalid: as
	ticket_tag_ids takes it; such a name is reported as a tag whose entry is invalid, not as unknown.
	"""
	groups = config.get("tag_groups")
	if groups is None:
		return []
	where = "config.json tag_groups"
	if not isinstance(groups, list):
		raise ticket_error(where + ": expected a list of {name, tags, min, max}")
	by_name = tag_ids_by_name(store)
	problems = []
	result = []
	seen = set()
	for index, group in enumerate(groups):
		at = "{}[{}]".format(where, index)
		if not isinstance(group, dict):
			problems.append(at + ": expected an object {name, tags, min, max}")
			continue
		unknown = [key for key in group if key not in tag_group_keys]
		if unknown:
			problems.append("{}: unknown key(s) {} (a group takes {})".format(at, ", ".join(shown(key) for key in unknown),
				", ".join(tag_group_keys)))
		name = group.get("name")
		if not isinstance(name, str) or not tag_name_pattern.fullmatch(name):
			problems.append("{}: name must match ^[a-z0-9_]+$, not {}".format(at, shown(name)))
			continue
		at = "{} ('{}')".format(at, name)
		if name in seen:
			problems.append(at + ": another group has this name")
		seen.add(name)
		names = group.get("tags")
		if not isinstance(names, list) or not names or not all(isinstance(item, str) for item in names):
			problems.append(at + ": tags must be a non-empty list of tag names")
			continue
		if len(set(names)) != len(names):
			problems.append(at + ": a tag is listed twice")
		missing = [item for item in names if item not in by_name and item not in invalid]
		if missing:
			problems.append("{}: unknown tag(s) {} (not in tags.json)".format(at, ", ".join(shown(item) for item in missing)))
		broken = [item for item in names if item not in by_name and item in invalid]
		if broken:
			problems.append("{}: tag(s) {} have an invalid entry in tags.json (the tags.json problem for each says "
				"why)".format(at, ", ".join(shown(item) for item in broken)))
		low = group.get("min", 0)
		high = group.get("max")
		if not isinstance(low, int) or isinstance(low, bool) or low < 0:
			problems.append(at + ": min must be a whole number, 0 or more")
			continue
		if high is not None and (not isinstance(high, int) or isinstance(high, bool) or high < 1 or high < low):
			problems.append(at + ": max must be null or a whole number, 1 or more and not below min")
			continue
		if low > len(set(names)):
			problems.append("{}: min {} is more than the group's {} tag(s)".format(at, low, len(set(names))))
			continue
		unique = []
		add_unique(unique, names)
		result.append({"name": name, "tags": unique, "ids": [by_name.get(item) for item in unique], "min": low, "max": high})
	if problems:
		raise ticket_error("; ".join(problems))
	return result


def group_rule(group):
	"""What a group asks, as a sentence: "pick exactly 1 of bug, cleanup, docs"."""
	names = ", ".join(group["tags"])
	low, high = group["min"], group["max"]
	if high is None:
		return "pick at least {} of {}".format(low, names) if low else "pick any of {}".format(names)
	if low == high:
		return "pick exactly {} of {}".format(low, names)
	if low == 0:
		return "pick at most {} of {}".format(high, names)
	return "pick {} to {} of {}".format(low, high, names)


def group_errors(ids, groups, store):
	"""The groups a ticket's tag ids break, one string each: "tag group type: pick exactly 1 of ... (has none)"."""
	errors = []
	for group in groups:
		carried = [tag_id for tag_id in ids if tag_id in group["ids"]]
		if len(carried) < group["min"] or (group["max"] is not None and len(carried) > group["max"]):
			has = "has none" if not carried else "has {}: {}".format(len(carried),
				", ".join(store["tags"][str(tag_id)]["name"] for tag_id in carried))
			errors.append("tag group {}: {} ({})".format(group["name"], group_rule(group), has))
	return errors


def load_all():
	"""Every ticket that parses, plus one problem string per file that does not."""
	tickets = []
	problems = []
	for name in ticket_files():
		path = os.path.join(tickets_dir, name)
		try:
			# Windows refuses the open for a moment while a `set` moves the new file into place.
			tickets.append(read_store_json(path))
		except (OSError, ValueError) as error:
			problems.append("{}: unreadable JSON ({})".format(name, error))
	return tickets, problems


def load_ticket(ticket_id):
	"""One ticket file's content, a JSON object (show, set); a ticket_error naming the ticket when it is anything else."""
	path = ticket_path(ticket_id)
	if not os.path.isfile(path):
		raise ticket_error("no such ticket: " + ticket_id)
	try:
		ticket = read_store_json(path)
	except (OSError, ValueError) as error:
		raise ticket_error("{}: unreadable JSON ({})".format(ticket_id, one_line(error)))
	if not isinstance(ticket, dict):
		raise ticket_error("{}: not a JSON object but {}; run validate, then fix the file".format(ticket_id, shown(ticket)))
	return ticket


def refuse_unusable_ticket(ticket, ticket_id, args):
	"""`set` refuses up front only a ticket it cannot work on: filed under another id, or holding a field set reads or
	adds to in the wrong form (tags not a list, links not an object, a links list not a list, a body to append to that
	is not a string). A ticket_error naming the ticket and the field. Anything else wrong may be what the call repairs
	(a summary too long, a missing field set gives): the changed ticket is checked whole before it is written."""
	problem = None
	links = ticket.get("links")
	if ticket.get("id") != ticket_id:
		problem = "the file holds id {}".format(shown(ticket.get("id")))
	elif "tags" in ticket and not isinstance(ticket["tags"], list):
		problem = "tags is {}, not a list".format(shown(ticket["tags"]))
	elif "links" in ticket and not isinstance(links, dict):
		problem = "links is {}, not an object".format(shown(links))
	elif getattr(args, "append_body_file", None) and "body" in ticket and not isinstance(ticket["body"], str):
		problem = "body is {}, not text to append to".format(shown(ticket["body"]))
	elif "comments" in ticket and not isinstance(ticket["comments"], list):
		problem = "comments is {}, not a list".format(shown(ticket["comments"]))
	else:
		for kind in link_kinds + typed_link_kinds:
			if isinstance(links, dict) and kind in links and not isinstance(links[kind], list):
				problem = "links.{} is {}, not a list".format(kind, shown(links[kind]))
				break
	if problem:
		# other_side: the ticket is the other side of a block another ticket is given (other_sides).
		how = "to take the other side of a block" if getattr(args, "other_side", False) else "with " + (
			getattr(args, "command", None) or "set")
		raise ticket_error("{} cannot be changed {}: {}; fix the file, then run validate".format(ticket_id, how, problem))


def next_id():
	top = max([int(ticket_file_pattern.fullmatch(name).group(1)) for name in ticket_files()] + [first_ticket_number - 1])
	# The ids an import has claimed in its journal are taken too, placed or not: `new` goes after its batch.
	claimed_by = None
	for name in import_leftovers():
		journal = read_journal(name) if import_journal_pattern.fullmatch(name) else None
		if journal:
			last = max(int(ticket_id[len(id_prefix):]) for ticket_id in journal["files"])
			if last > top:
				top, claimed_by = last, (name, journal)
	if claimed_by and top >= id_number_max:
		raise ticket_error("{}; the highest id is claimed by an interrupted import's journal ({}, batch {}), not by a "
			"ticket file: once no import is running, `tickets.py import --abort-interrupted` frees its ids; nothing was "
			"written".format(store_full_text(top + 1), claimed_by[0], batch_text(claimed_by[1])))
	return id_for_number(top + 1)


def store_full_text(number):
	"""Why no ticket can take this number: its id would be longer than any command reads."""
	return ("the store is full: the next id, {}{}, would have more than {} digits, and no command reads such a file as a "
		"ticket (the limit is 9 digits, or one more than config.json's first_ticket_number has)".format(id_prefix,
		str(number).zfill(id_digits), id_max_digits))


def id_for_number(number):
	"""The ticket id of a number, as `new` and `import` write it. A ticket_error, before anything is written, when it
	would have more than id_max_digits digits: no command would read that file as a ticket (ticket_file_pattern)."""
	if number > id_number_max:
		raise ticket_error(store_full_text(number) + "; nothing was written")
	return id_prefix + str(number).zfill(id_digits)


# --- validation: the subset of JSON Schema that ticket.schema.json uses -------------------------

def resolve_ref(schema_root, node):
	while "$ref" in node:
		reference = node["$ref"]
		if not reference.startswith("#/"):
			raise ticket_error("unsupported $ref: " + reference)
		target = schema_root
		for part in reference[2:].split("/"):
			target = target[part]
		node = target
	return node


def type_matches(value, expected):
	checks = {
		"object": lambda v: isinstance(v, dict),
		"array": lambda v: isinstance(v, list),
		"string": lambda v: isinstance(v, str),
		"integer": lambda v: isinstance(v, int) and not isinstance(v, bool),
		"boolean": lambda v: isinstance(v, bool),
	}
	if expected not in checks:
		raise ticket_error("unsupported schema type: " + expected)
	return checks[expected](value)


def pattern_found(pattern, value):
	"""JSON Schema's pattern test: a search, where a closing "$" ends the value, never before a final newline."""
	if pattern.endswith("$"):
		body = pattern[:-1]
		# An odd run of backslashes before the "$" escapes it: a literal dollar sign, not an anchor.
		if (len(body) - len(body.rstrip("\\"))) % 2 == 0:
			pattern = body + r"\Z"
	return re.search(pattern, value) is not None


def schema_errors(schema_root, node, value, where):
	# Only the keywords tested below count: any other, "x-project-owned" (read by the updater) included, is ignored.
	node = resolve_ref(schema_root, node)
	errors = []
	expected = node.get("type")
	if expected and not type_matches(value, expected):
		return ["{}: expected {}".format(where, expected)]
	if "enum" in node and value not in node["enum"]:
		errors.append("{}: '{}' is not one of {}".format(where, one_line(value), ", ".join(node["enum"])))
	if "anyOf" in node and all(schema_errors(schema_root, choice, value, where) for choice in node["anyOf"]):
		errors.append("{}: {} matches none of the forms allowed here".format(where, shown(value)))
	if "minimum" in node and type_matches(value, "integer") and value < node["minimum"]:
		errors.append("{}: {} is less than {}".format(where, value, node["minimum"]))
	if isinstance(value, str):
		if "minLength" in node and len(value) < node["minLength"]:
			errors.append("{}: shorter than {}".format(where, node["minLength"]))
		if "maxLength" in node and len(value) > node["maxLength"]:
			errors.append("{}: longer than {}".format(where, node["maxLength"]))
		if "pattern" in node and not pattern_found(node["pattern"], value):
			errors.append("{}: '{}' does not match {}".format(where, one_line(value), node["pattern"]))
	if isinstance(value, list):
		if node.get("uniqueItems") and len(set(json.dumps(item) for item in value)) != len(value):
			errors.append("{}: duplicate entries".format(where))
		if "items" in node:
			for index, item in enumerate(value):
				errors += schema_errors(schema_root, node["items"], item, "{}[{}]".format(where, index))
	if isinstance(value, dict):
		for name in node.get("required", []):
			if name not in value:
				errors.append("{}: missing '{}'".format(where, name))
		properties = node.get("properties", {})
		for name, item in value.items():
			if name in properties:
				errors += schema_errors(schema_root, properties[name], item, where + "." + name)
			elif node.get("additionalProperties") is False:
				errors.append("{}: unknown field '{}'".format(where, one_line(name)))
	return errors


def read_schema():
	"""ticket.schema.json, with the tags node replaced by tags_schema_node: store_errors checks the items (ids, or
	names until converted, that the store holds), so the project's schema decides nothing about them."""
	try:
		schema = read_json(schema_path)
	except (OSError, ValueError, RecursionError) as error:
		raise ticket_error("cannot read the schema {} ({})".format(one_line(schema_path),
			one_line(getattr(error, "strerror", None) or error)))
	if isinstance(schema, dict) and isinstance(schema.get("properties"), dict):
		schema = dict(schema)
		schema["properties"] = dict(schema["properties"])
		schema["properties"]["tags"] = tags_schema_node
	return schema


def store_errors(ticket, store, groups, known_ids, file_name=None, invalid_tags=()):
	"""The checks a schema cannot express, for a ticket that passed it: (field, what is wrong) pairs. store is the
	tag store (None: tags.json does not read, and the tags are not checked); groups are load_tag_groups's;
	invalid_tags as ticket_tag_ids takes it."""
	errors = []
	# The schema accepts any prefix (it is shared by every project); the store takes only its own.
	if not id_pattern.fullmatch(ticket["id"]):
		errors.append(("id", "id {} does not start with this store's prefix {} (config.json id_prefix)".format(
			ticket["id"], id_prefix)))
	if file_name and file_name != ticket["id"] + ".json":
		errors.append(("id", "file name does not match id {}".format(ticket["id"])))
	if store is not None:
		ids, problems = ticket_tag_ids(store, ticket["tags"], invalid_tags)
		errors += [("tags", problem) for problem in problems]
		if not problems:
			errors += [("tags", problem) for problem in group_errors(ids, groups, store)]
	for kind in ticket_link_kinds:
		field = "links." + kind
		linked_ids = ticket["links"].get(kind, [])
		# An older project schema may not know the typed lists; one that is not a list of text is not checked further.
		if not isinstance(linked_ids, list):
			continue
		for linked in linked_ids:
			if not isinstance(linked, str):
				continue
			if not id_pattern.fullmatch(linked):
				errors.append((field, "ticket link {} does not start with this store's prefix {} (config.json id_prefix)".format(
					linked, id_prefix)))
			elif linked == ticket["id"]:
				errors.append((field, "links to itself"))
			elif linked not in known_ids:
				errors.append((field, "dangling ticket link {}".format(linked)))
	links = ticket["links"]
	both = [linked for linked in links.get("blocks", []) if isinstance(links.get("blocked_by"), list)
		and linked in links["blocked_by"]] if isinstance(links.get("blocks"), list) else []
	for linked in both:
		errors.append(("links.blocks", "both blocks and is blocked by {}".format(linked)))
	comments = ticket.get("comments", [])
	for index, comment in enumerate(comments if isinstance(comments, list) else []):
		if isinstance(comment, dict) and isinstance(comment.get("at"), str) and comment["at"] < ticket["created"]:
			errors.append(("comments[{}]".format(index), "the comment is dated before the ticket was created"))
	if ticket["updated"] < ticket["created"]:
		errors.append(("updated", "updated is earlier than created"))
	return errors


def future_comment_errors(checked):
	"""validate: one problem per comment dated after now, the check (and the tolerance: none) `import` makes. Only
	`comment` writes a comment, stamped now, so a later date was written by hand or on a clock that runs ahead."""
	now = utc_now()
	errors = []
	for ticket in checked:
		comments = ticket.get("comments")
		for index, comment in enumerate(comments if isinstance(comments, list) else []):
			if isinstance(comment, dict) and is_timestamp(comment.get("at")) and comment["at"] > now:
				errors.append("{}: comments[{}].at: {} is in the future (now is {})".format(ticket["id"], index,
					comment["at"], now))
	return errors


def block_pair_errors(checked, by_id, set_hint=True):
	"""The blocks that have only one side, as (ticket id, what is wrong) pairs: for each ticket of checked, every id in its
	links.blocks must list it in that ticket's links.blocked_by, and the other way round. by_id: every ticket by id. A link
	to a ticket by_id lacks is a dangling link, reported by store_errors, not here. set_hint: end each with the set call
	that finishes the pair (not for `import`, whose tickets do not exist yet)."""
	errors = []
	for ticket in checked:
		links = ticket.get("links") if isinstance(ticket, dict) else None
		if not isinstance(links, dict):
			continue
		for kind, reverse in sorted(reverse_link.items()):
			linked_ids = links.get(kind, [])
			for linked in linked_ids if isinstance(linked_ids, list) else []:
				other = by_id.get(linked) if isinstance(linked, str) else None
				if not isinstance(other, dict) or other is ticket:
					continue
				other_links = other.get("links")
				listed_back = other_links.get(reverse, []) if isinstance(other_links, dict) else []
				if not isinstance(listed_back, list) or ticket["id"] not in listed_back:
					problem = "links.{} names {}, but {}'s links.{} lacks {}".format(kind, linked, linked, reverse, ticket["id"])
					if set_hint:
						problem += ": run tickets.py set {} --{} {}".format(ticket["id"],
							kind.replace("_", "-").replace("blocks", "block"), linked)
					errors.append((ticket["id"], problem))
	return errors


def mentioned_ids(ticket):
	"""The ticket ids a ticket's body and comments mention (mention_id_pattern), outside code, in the order first met,
	once each, without its own id. Derived, never stored."""
	texts = [ticket.get("body")]
	comments = ticket.get("comments")
	for comment in comments if isinstance(comments, list) else []:
		texts.append(comment.get("body") if isinstance(comment, dict) else None)
	found = []
	for text in texts:
		if isinstance(text, str):
			add_unique(found, mention_id_pattern.findall(code_pattern.sub(" ", text)))
	return [ticket_id for ticket_id in found if ticket_id != ticket.get("id")]


def has_line_break(text):
	return any(character in text for character in line_break_characters)


def line_break_errors(ticket):
	"""The one-line fields that hold a line break, as (field, what is wrong) pairs. The schema has no rule for it."""
	return [(field, "has a line break; it must be one line") for field in one_line_fields
		if isinstance(ticket.get(field), str) and has_line_break(ticket[field])]


def refuse_line_breaks(args):
	"""`new` and `set`: --summary and --target are one line each."""
	for option, value in (("--summary", args.summary), ("--target", args.target)):
		if value is not None and has_line_break(value):
			raise ticket_error(option + " has a line break; it must be one line")


def ticket_errors(ticket, schema, store, groups, known_ids, file_name=None, invalid_tags=()):
	where = file_name or ticket.get("id", "?")
	errors = schema_errors(schema, schema, ticket, where)
	errors += ["{}.{}: {}".format(where, field, problem) for field, problem in line_break_errors(ticket)]
	if errors:
		return errors
	return ["{}: {}".format(where, problem) for _, problem in store_errors(ticket, store, groups, known_ids, file_name,
		invalid_tags)]


def carries_tag_names(ticket):
	"""True when a ticket's tags still hold a string: a name (a 1.2 ticket, not converted yet), or an id written as
	digits. author.py --convert-all writes either as the int id."""
	tags = ticket.get("tags") if isinstance(ticket, dict) else None
	return isinstance(tags, list) and any(isinstance(value, str) for value in tags)


def validate_all():
	"""(ticket count, problems), as `validate` reports them; validate_store gives the warnings too."""
	count, errors, _ = validate_store()
	return count, errors


def validate_store():
	"""(ticket count, problems, warnings). The warnings are the transition's, which never fail validate: tags.json
	still in format 1, and tickets that still carry tag names."""
	schema = read_schema()
	invalid_tags = set()
	store, version, errors = tag_store_state(invalid_tags)
	warnings = []
	if store is not None and version == 1:
		warnings.append("tags.json is in the old format (a name -> colour map); readers accept it, every writer "
			"refuses it: run author.py tags --probe")
	groups = []
	if store is not None:
		try:
			groups = load_tag_groups(store, invalid_tags)
		except ticket_error as error:
			errors.append(str(error))
	tickets, problems = load_all()
	errors += problems + too_long_names()
	known_ids = set()
	for ticket in tickets:
		ticket_id = ticket.get("id") if isinstance(ticket, dict) else None
		if not isinstance(ticket_id, str):
			# A file that is not an object, or an id that is missing or not text: the checks below name it, and two such
			# files are no duplicate.
			continue
		if ticket_id in known_ids:
			errors.append("duplicate id " + str(ticket_id))
		known_ids.add(ticket_id)
	for name in ticket_files():
		path = os.path.join(tickets_dir, name)
		try:
			ticket = read_store_json(path)
		except (OSError, ValueError):
			continue
		if not isinstance(ticket, dict):
			errors.append(name + ": not a JSON object")
			continue
		errors += ticket_errors(ticket, schema, store, groups, known_ids, name, invalid_tags)
	by_id = {ticket["id"]: ticket for ticket in tickets if isinstance(ticket, dict) and isinstance(ticket.get("id"), str)}
	errors += ["{}: {}".format(ticket_id, problem) for ticket_id, problem in block_pair_errors(list(by_id.values()), by_id)]
	errors += future_comment_errors(by_id.values())
	for ticket in by_id.values():
		missing = [linked for linked in mentioned_ids(ticket) if linked not in by_id]
		if missing:
			warnings.append("{}: mentions {} in its body or comments, which is not in the store".format(ticket["id"],
				", ".join(missing)))
	done_warning = done_without_commit_warning(by_id.values())
	if done_warning:
		warnings.append(done_warning)
	named = sum(1 for ticket in tickets if carries_tag_names(ticket))
	if named:
		warnings.append("{} tickets still carry tag names: run author.py --convert-all".format(named))
	return len(tickets), errors, warnings


def write_problems(ticket, also_known=()):
	"""What stops a writer from writing this ticket (its tags already ids), one string each: the store's state, the
	tag groups, the schema, the tags and the links. also_known: ids written in the same step (a new ticket, for the
	other side of its blocks), so a link to one is not dangling. Raises ticket_error when tags.json or config.json's
	tag_groups refuses every write."""
	store = writable_tag_store()
	groups = load_tag_groups(store)
	schema = read_schema()
	tickets, _ = load_all()
	known_ids = {item.get("id") for item in tickets if isinstance(item, dict)} | {ticket["id"]} | set(also_known)
	return ticket_errors(ticket, schema, store, groups, known_ids)


def check_before_write(ticket, also_known=()):
	errors = write_problems(ticket, also_known)
	if errors:
		raise ticket_error("refusing to write an invalid ticket:\n  " + "\n  ".join(errors))


def has_commit_link(ticket):
	links = ticket.get("links")
	commits = links.get("commits") if isinstance(links, dict) else None
	return isinstance(commits, list) and len(commits) > 0


def done_commit_problem(ticket, how="add --link-commit <hash> to the same command"):
	"""The rules' "done always needs the commit link" (B62), for a writer setting status done: the problem as text, or
	None. Only new, set and author.py new/draft refuse it; import and validate do not, so an old store still reads."""
	if ticket.get("status") == "done" and not has_commit_link(ticket):
		return "status done needs a commit link (links.commits is empty): {}".format(how)
	return None


def done_without_commit_warning(checked):
	"""validate's warning for done tickets with no commit link (closed before the writers refused that), or None."""
	ids = sorted(ticket["id"] for ticket in checked if ticket.get("status") == "done" and not has_commit_link(ticket))
	if not ids:
		return None
	shown_ids = ", ".join(ids[:10]) + (", ..." if len(ids) > 10 else "")
	return ("{} done ticket(s) have no commit link ({}): add one with set <id> --link-commit <hash> where a commit "
		"exists".format(len(ids), shown_ids))


# --- commands -----------------------------------------------------------------------------------

def split_list(text):
	if not text:
		return []
	return [part.strip() for part in text.split(",") if part.strip()]


def read_body(path):
	"""A body file (or - for stdin) as text; a ticket_error, not a traceback, when it cannot be read."""
	try:
		if path == "-":
			data = sys.stdin.buffer.read()
			return data.decode("utf-8-sig").replace("\r\n", "\n")
		with open(path, "r", encoding="utf-8-sig") as handle:
			return handle.read().replace("\r\n", "\n")
	except OSError as error:
		raise ticket_error("cannot read body file {} ({})".format(path, error.strerror or error))
	except ValueError:
		raise ticket_error("body file {} is not valid UTF-8".format(path))


def add_unique(items, additions):
	for item in additions:
		if item not in items:
			items.append(item)


def apply_link_arguments(ticket, args):
	"""Add the --link-* values. A links list (or links itself) that a value goes into is created when the ticket lacks
	it, so `set` can repair that; one no value goes into is left as it is."""
	for kind, items in (("tickets", [normalize_id(item) for item in args.link_ticket or []]),
			("files", args.link_file or []), ("commits", args.link_commit or [])):
		if items:
			add_unique(ticket.setdefault("links", {}).setdefault(kind, []), items)


# The typed link flags: (argparse dest, link kind, add or remove).
typed_link_flags = [("block", "blocks", True), ("blocked_by", "blocked_by", True), ("relates", "relates_to", True),
	("remove_block", "blocks", False), ("remove_blocked_by", "blocked_by", False), ("remove_relates", "relates_to", False)]


def typed_link_changes(args):
	"""The typed link flags given, as [(kind, add, ticket id)] in flag order; each flag is repeatable and takes
	comma-separated ids (bare numbers too, as --link-ticket)."""
	changes = []
	for dest, kind, add in typed_link_flags:
		for value in getattr(args, dest, None) or []:
			changes += [(kind, add, normalize_id(item)) for item in split_list(value)]
	return changes


def apply_typed_links(ticket, changes):
	"""Apply typed link changes to one ticket. An add creates the list when the ticket lacks it; a remove that empties a
	typed list drops the key, so a ticket without typed links stays as an older schema reads it. Removing a relates-to
	also takes the id out of the untyped links.tickets, which is read as relates-to. A ticket_error for a link to itself."""
	for kind, add, other in changes:
		if other == ticket.get("id"):
			raise ticket_error("{} cannot link to itself".format(other))
		if add:
			add_unique(ticket.setdefault("links", {}).setdefault(kind, []), [other])
			continue
		links = ticket.get("links")
		if not isinstance(links, dict):
			continue
		for name in [kind] + (["tickets"] if kind == "relates_to" else []):
			if isinstance(links.get(name), list):
				links[name] = [item for item in links[name] if item != other]
		if kind in typed_link_kinds and links.get(kind) == []:
			del links[kind]


def copied(value):
	return json.loads(json.dumps(value))


def other_sides(ticket_id, changes):
	"""The other tickets a set of block changes writes (reverse_link), as [(changed ticket, ticket as read)]: each read,
	changed and stamped `updated`, not written. A ticket that is not in the store is skipped for a remove (only this side
	holds the link) and refused for an add, as a dangling link."""
	others = {}
	order = []
	for kind, add, other in changes:
		reverse = reverse_link.get(kind)
		if reverse is None:
			continue
		if other not in others:
			if not os.path.isfile(ticket_path(other)):
				if add:
					raise ticket_error("no such ticket: {} (a block is written on both tickets)".format(other))
				continue
			loaded = load_ticket(other)
			refuse_unusable_ticket(loaded, other, argparse.Namespace(other_side=True))
			others[other] = (loaded, copied(loaded))
			order.append(other)
		apply_typed_links(others[other][0], [(reverse, add, ticket_id)])
	now = utc_now()
	result = []
	for other in order:
		changed, original = others[other]
		if changed != original:
			changed["updated"] = now
			result.append((changed, original))
	return result


def write_together(pairs, command="set"):
	"""Write each (ticket, ticket as read) with write_ticket, in order. When one fails, the ones already written are put
	back as they were read, so a block is never left on one side by this command; a ticket_error says what happened. Two
	files cannot be replaced in one step: only a killed process leaves a pair half written, which validate reports
	(block_pair_errors)."""
	written = []
	for ticket, original in pairs:
		try:
			write_ticket(ticket, command)
		except BaseException as error:
			left = []
			for done, before in reversed(written):
				try:
					write_ticket(before)
				except ticket_error:
					left.append(done["id"])
			if not isinstance(error, ticket_error):
				raise
			outcome = "the others were put back" if not left else "{} could not be put back: run validate".format(
				", ".join(left))
			raise ticket_error("{}; {}".format(error, outcome) if written else str(error))
		written.append((ticket, original))


def command_new(args):
	refuse_line_breaks(args)
	store = writable_tag_store()
	now = utc_now()
	ticket = {
		"id": next_id(),
		"summary": args.summary,
		"status": args.status,
		"severity": args.severity,
		"owner_area": args.owner,
		"target_patch": args.target,
		"tags": resolve_input_tags(store, split_list(args.tags)),
		"author": args.author,
		"created": now,
		"updated": now,
		"links": {kind: [] for kind in link_kinds},
		"body": read_body(args.body_file) if args.body_file else "",
	}
	apply_link_arguments(ticket, args)
	apply_typed_links(ticket, typed_link_changes(args))
	problem = done_commit_problem(ticket)
	if problem:
		raise ticket_error(problem)
	# The ticket and the other side of each of its blocks are checked first, then written together (create_ticket).
	written = create_ticket(ticket)
	print("created {} - {}".format(ticket["id"], ascii_text(ticket["summary"])))
	if written:
		print("updated {} (the other side of its blocks)".format(", ".join(written)))


def command_set(args):
	refuse_line_breaks(args)
	store = writable_tag_store()
	ticket_id = normalize_id(args.id)
	ticket = load_ticket(ticket_id)
	refuse_unusable_ticket(ticket, ticket_id, args)
	original = copied(ticket)
	for field, value in [("summary", args.summary), ("status", args.status), ("severity", args.severity),
			("owner_area", args.owner), ("target_patch", args.target), ("author", args.author)]:
		if value is not None:
			ticket[field] = value
	# Writers write ids: a name the ticket still carries becomes its id; one the store lacks is left for the check.
	if "tags" in ticket:
		ticket["tags"] = [stored_tag_id(store, tag) or tag for tag in ticket["tags"]]
	if args.tags is not None:
		ticket["tags"] = resolve_input_tags(store, split_list(args.tags))
	if args.add_tag:
		add_unique(ticket.setdefault("tags", []), resolve_input_tags(store, split_list(args.add_tag)))
	if args.remove_tag:
		removed = resolve_input_tags(store, split_list(args.remove_tag))
		if "tags" in ticket:
			ticket["tags"] = [tag for tag in ticket["tags"] if tag not in removed]
	if args.body_file:
		ticket["body"] = read_body(args.body_file)
	if args.append_body_file:
		# Appended as its own paragraph block; the existing body is never rewritten.
		ticket["body"] = ticket.get("body", "").rstrip("\n") + "\n\n" + read_body(args.append_body_file).strip("\n") + "\n"
	apply_link_arguments(ticket, args)
	changes = typed_link_changes(args)
	apply_typed_links(ticket, changes)
	# Only a command that sets done is held to the commit link: editing an older done ticket without one still works.
	if args.status == "done":
		problem = done_commit_problem(ticket)
		if problem:
			raise ticket_error(problem)
	ticket["updated"] = utc_now()
	# The changed ticket is checked whole, as `new` checks one: nothing is written unless it is valid. So is each other
	# ticket a block writes (other_sides); then all are written, or none stays changed (write_together).
	check_before_write(ticket)
	others = other_sides(ticket_id, changes)
	for other, _ in others:
		check_before_write(other)
	write_together([(ticket, original)] + others)
	print(ascii_text("updated {} ({} {} {})".format(ticket["id"], ticket["status"], ticket["severity"], ticket["owner_area"])))
	if others:
		print("updated {} (the other side of the blocks)".format(", ".join(other["id"] for other, _ in others)))


def add_comment(ticket_id, author, body):
	"""Append a comment {author, at, body} to a ticket and stamp `updated` with the same time; the ticket as written.
	Comments are only appended, never edited, in the order written (the viewer shows the newest first)."""
	body = body.strip("\n")
	if not body.strip():
		raise ticket_error("the comment is empty; nothing written")
	ticket = load_ticket(ticket_id)
	refuse_unusable_ticket(ticket, ticket_id, argparse.Namespace(command="comment"))
	store = writable_tag_store()
	if isinstance(ticket.get("tags"), list):
		ticket["tags"] = [stored_tag_id(store, tag) or tag for tag in ticket["tags"]]
	now = utc_now()
	ticket.setdefault("comments", []).append({"author": author, "at": now, "body": body})
	ticket["updated"] = now
	check_before_write(ticket)
	write_ticket(ticket)
	return ticket


def command_comment(args):
	ticket = add_comment(normalize_id(args.id), args.author, read_body(args.body_file))
	print("commented on {} ({} comment(s))".format(ticket["id"], len(ticket["comments"])))


def ascii_text(text):
	return text.encode("ascii", "replace").decode("ascii")


def one_line(value):
	"""A value echoed in a problem message, with line breaks written out so the message stays one line."""
	return str(value).replace("\r", "\\r").replace("\n", "\\n")


def reader_tag_store():
	"""The tag store for a reader (list, show), or None when tags.json does not read: the raw values print then."""
	try:
		return load_tag_store()
	except (OSError, ValueError):
		return None


def carried_tags(ticket, store):
	"""The tags a ticket carries, as comparable keys: ids through the store (names and ids alike), raw values without one."""
	tags = ticket.get("tags", [])
	if not isinstance(tags, list):
		return set()
	if store is None:
		return {str(value) for value in tags}
	by_name = tag_ids_by_name(store)
	return {stored_tag_id(store, value, by_name) or str(value) for value in tags if isinstance(value, (int, str))}


def wanted_tags(text, store):
	"""list --tag: the filter's tags as carried_tags keys; a tag the store lacks matches no ticket."""
	if store is None:
		return [value for value in split_list(text)]
	return [resolve_tag(store, value) or ("unknown", value) for value in split_list(text)]


def matches(ticket, args, store=None):
	statuses = split_list(args.status) or (active_statuses if args.active else [])
	carried = carried_tags(ticket, store) if args.tag else set()
	checks = [
		not statuses or ticket.get("status") in statuses,
		not args.severity or ticket.get("severity") in split_list(args.severity),
		not args.owner or ticket.get("owner_area") in split_list(args.owner),
		not args.target or ticket.get("target_patch") == args.target,
		not args.tag or all(tag in carried for tag in wanted_tags(args.tag, store)),
	]
	if args.text:
		needle = args.text.lower()
		checks.append(needle in (text_field(ticket, "summary", "") + "\n" + text_field(ticket, "body", "")).lower())
	return all(checks)


def text_field(ticket, field, default):
	"""A field as `list` prints and searches it: the string, or default when the ticket holds anything else there."""
	value = ticket.get(field)
	return value if isinstance(value, str) else default


def command_list(args):
	tickets, problems = load_all()
	store = reader_tag_store() if args.tag else None
	shown = [ticket for ticket in tickets if isinstance(ticket, dict) and matches(ticket, args, store)]
	# The id column fits the longest shown id (8 at least, so ids of 8 characters or fewer print as always).
	id_width = max([8] + [len(text_field(ticket, "id", "?")) for ticket in shown])
	for ticket in shown:
		summary = ascii_text(text_field(ticket, "summary", ""))
		if len(summary) > list_summary_width:
			summary = summary[:list_summary_width - 3] + "..."
		# After the summary: who blocks the ticket, and how many comments it has.
		links = ticket.get("links")
		blockers = links.get("blocked_by") if isinstance(links, dict) else None
		if isinstance(blockers, list) and blockers:
			summary += " [blocked by {}]".format(", ".join(str(item) for item in blockers))
		comments = ticket.get("comments")
		if isinstance(comments, list) and comments:
			summary += " [{} comment(s)]".format(len(comments))
		print(ascii_text("{:<{}} {:<11} {:<8} {:<7} {}".format(text_field(ticket, "id", "?"), id_width,
			text_field(ticket, "status", "?"), text_field(ticket, "severity", "?"), text_field(ticket, "owner_area", "?"),
			summary)))
	print("{} of {} tickets".format(len(shown), len(tickets)))
	for problem in problems:
		print("WARNING " + ascii_text(problem))


def command_show(args):
	"""Print a ticket. One that fails the schema is printed as far as it reads, with a WARNING naming its first problem."""
	ticket_id = normalize_id(args.id)
	ticket = load_ticket(ticket_id)
	store = reader_tag_store()
	for field in field_order:
		if field in ("body", "links", "comments"):
			continue
		value = ticket.get(field)
		if field == "tags" and isinstance(value, list):
			value = ", ".join(tag_label(store, tag) for tag in value)
		elif isinstance(value, list):
			value = ", ".join(str(item) for item in value)
		print("{:<15} {}".format(field + ":", ascii_text(str(value))))
	links = ticket.get("links")
	for kind in link_kinds + typed_link_kinds:
		items = links.get(kind) if isinstance(links, dict) else None
		if items:
			text = ", ".join(str(item) for item in items) if isinstance(items, list) else str(items)
			print("{:<15} {}".format("links." + kind + ":", ascii_text(text)))
	# Mentions are derived from the text (mentioned_ids), never stored.
	mentions = mentioned_ids(ticket)
	if mentions:
		print("{:<15} {}".format("mentions:", ", ".join(mentions)))
	others, _ = load_all()
	mentioned_by = [other["id"] for other in others if isinstance(other, dict) and isinstance(other.get("id"), str)
		and other["id"] != ticket_id and ticket_id in mentioned_ids(other)]
	if mentioned_by:
		print("{:<15} {}".format("mentioned by:", ", ".join(mentioned_by)))
	print("")
	body = ticket.get("body", "")
	print(ascii_text(body if isinstance(body, str) else json.dumps(body)))
	comments = ticket.get("comments")
	if isinstance(comments, list) and comments:
		print("")
		print("comments ({}, newest first):".format(len(comments)))
		for comment in reversed(comments):
			if not isinstance(comment, dict):
				print("  " + ascii_text(one_line(shown(comment))))
				continue
			print("")
			print(ascii_text("-- {} at {}".format(comment.get("author"), comment.get("at"))))
			text = comment.get("body")
			print(ascii_text(text if isinstance(text, str) else json.dumps(text)))
	schema = read_schema()
	errors = schema_errors(schema, schema, ticket, ticket_id)
	if not errors and ticket.get("id") != ticket_id:
		errors = ["{}: the file holds id {}".format(ticket_id, shown(ticket.get("id")))]
	if errors:
		print("WARNING {} is invalid ({} problem(s); the first: {}); run validate".format(ticket_id, len(errors),
			ascii_text(one_line(errors[0]))))


def command_validate(args):
	count, errors, warnings = validate_store()
	for warning in warnings:
		print("WARNING " + ascii_text(warning))
	for error in errors:
		print("FAIL " + ascii_text(error))
	if errors:
		print("{} problem(s) in {} ticket(s)".format(len(errors), count))
		return 1
	print("OK: {} tickets valid against the schema, tags and links".format(count))
	return 0


# --- import: many tickets from one JSON file, all or none ----------------------------------------

def unique_keys(pairs):
	"""json's object hook: a key written twice is an error, where json would silently keep the last."""
	result = {}
	for key, value in pairs:
		if key in result:
			raise ValueError("the key {} is written twice in one object".format(shown(key)))
		result[key] = value
	return result


def shown(value):
	"""A value from the import file echoed in a message: JSON text (ASCII, one line), cut when long."""
	try:
		text = json.dumps(value)
	except (TypeError, ValueError):
		text = one_line(repr(value))
	return text if len(text) <= 80 else text[:77] + "..."


def shown_key(key):
	text = one_line(key)
	return text if len(text) <= 60 else text[:57] + "..."


def read_import_file(path):
	"""The file's records: UTF-8 (a BOM is accepted) holding one JSON array."""
	try:
		with open(path, "r", encoding="utf-8-sig") as handle:
			records = json.load(handle, object_pairs_hook=unique_keys, parse_constant=reject_constant)
	except OSError as error:
		raise ticket_error("cannot read the import file {} ({})".format(one_line(path), one_line(error.strerror or error)))
	except ValueError as error:
		raise ticket_error("the import file {} is not valid UTF-8 JSON ({})".format(one_line(path), one_line(error)))
	except (RecursionError, MemoryError) as error:
		raise ticket_error("the import file {} is nested too deeply or is too large to read ({})".format(
			one_line(path), type(error).__name__))
	if not isinstance(records, list):
		raise ticket_error("the import file {} must hold a JSON array of records, [{{...}}, ...]".format(one_line(path)))
	return records


def is_timestamp(value):
	if not isinstance(value, str) or not timestamp_pattern.fullmatch(value):
		return False
	try:
		datetime.strptime(value, timestamp_format)
	except ValueError:
		return False
	return True


def normalized_link(item):
	"""A ticket link as --link-ticket would store it; anything that is not an id is left for the checks to name."""
	if not isinstance(item, str):
		return item
	try:
		return normalize_id(item)
	except ticket_error:
		return item


def import_ticket(record, ticket_id, now, schema, store, groups, known_ids):
	"""The ticket one record makes, and its problems as (record key, what is wrong) pairs. schema is read_schema's,
	store the tag store and groups load_tag_groups's. author.py's draft and new build their ticket here too.

	A record is an object. Required: summary, severity, owner (or owner_area), author. Optional: status
	(open), target (or target_patch; 'next'), tags ([]; names or ids, as --tags takes them, written as ids and
	checked against the tag groups), body (""; CRLF becomes LF), links ({}; any of tickets,
	files, commits, each an array; a ticket link may be a bare number or lower case, as --link-ticket takes it,
	and may name a ticket of this batch), created (now), updated (created), id (must be the id assigned).
	created and updated are written as utc_now() writes them, and none may be after now, the time this run
	started. Any other key is a problem, and so is giving both names of a pair. summary and target are one line.
	"""
	if not isinstance(record, dict):
		return None, [("record", "expected an object, not {}".format(shown(record)))]
	problems = [(key, "unknown key") for key in record if key not in import_keys]
	# The key each ticket field was given under, so a problem names what the file says.
	keys = {}
	for field, names in import_aliases.items():
		given = [name for name in names if name in record]
		if len(given) > 1:
			problems.append((given[0], "given twice, as '{}' and as '{}'".format(given[0], given[1])))
		keys[field] = given[0] if given else names[0]
	for field in import_required:
		if keys.get(field, field) not in record:
			problems.append((keys.get(field, field), "missing"))
	if "id" in record and record["id"] != ticket_id:
		problems.append(("id", "expected {}, given {}: ids are assigned in input order, as `new` would".format(
			ticket_id, shown(record["id"]))))
	if problems:
		return None, problems
	created = record.get("created", now)
	updated = record.get("updated", created)
	# A date reported here is not reported again by the schema's pattern, and an updated that only repeats
	# created is not reported beside it.
	dated = []
	for key, value in (("created", created), ("updated", updated)):
		if key == "updated" and "updated" not in record:
			dated += ["updated"] if dated else []
		elif not is_timestamp(value):
			problems.append((key, "{} is not a UTC time written YYYY-MM-DDTHH:MM:SSZ".format(shown(value))))
			dated.append(key)
		elif value > now:
			problems.append((key, "{} is in the future (now is {})".format(value, now)))
	links = record.get("links", {})
	if isinstance(links, dict):
		# In link_kinds order, as `new` writes them; a kind the schema does not know is left for it to name.
		given = links
		links = {kind: given.get(kind, []) for kind in link_kinds}
		links.update({kind: value for kind, value in given.items() if kind not in links})
		for kind in ticket_link_kinds:
			if isinstance(links.get(kind), list):
				links[kind] = [normalized_link(item) for item in links[kind]]
	body = record.get("body", "")
	ticket = {
		"id": ticket_id,
		"summary": record["summary"],
		"status": record.get("status", "open"),
		"severity": record["severity"],
		"owner_area": record[keys["owner_area"]],
		"target_patch": record.get(keys["target_patch"], default_target_patch),
		"tags": record.get("tags", []),
		"author": record["author"],
		"created": created,
		"updated": updated,
		"links": links,
		"body": body.replace("\r\n", "\n") if isinstance(body, str) else body,
	}
	if "comments" in record:
		ticket["comments"] = record["comments"]
		for index, comment in enumerate(record["comments"] if isinstance(record["comments"], list) else []):
			if isinstance(comment, dict) and isinstance(comment.get("at"), str) and comment["at"] > now:
				problems.append(("comments[{}].at".format(index), "{} is in the future (now is {})".format(comment["at"], now)))
	schema_passed = True
	for field, problem in line_break_errors(ticket):
		schema_passed = False
		problems.append((keys.get(field, field), problem))
	for error in schema_errors(schema, schema, ticket, ""):
		schema_passed = False
		# With where "", an error reads ".field: why" or ".links.files[0]: why".
		path, _, problem = error.lstrip(".").partition(": ")
		field = re.split(r"[.\[]", path, maxsplit=1)[0]
		if field not in dated:
			problems.append((keys.get(field, field) + path[len(field):], problem))
	if schema_passed:
		ids, tag_problems = input_tag_ids(store, ticket["tags"])
		problems += [("tags", problem) for problem in tag_problems]
		ticket["tags"] = ids
		# A group counted over a part of the record's tags would only repeat the problem above.
		problems += store_errors(ticket, store, [] if tag_problems else groups, known_ids)
	return ticket, problems


def ticket_bytes(ticket):
	"""The file's bytes, or the field that cannot be written as UTF-8 (half of a surrogate pair)."""
	try:
		return (json.dumps(ordered(ticket), indent=2, ensure_ascii=False) + "\n").encode("utf-8"), None
	except UnicodeEncodeError:
		for field, value in ticket.items():
			try:
				json.dumps(value, ensure_ascii=False).encode("utf-8")
			except UnicodeEncodeError:
				return None, field
		return None, "record"


def temp_path(ticket_id, kind):
	"""Where a ticket is written before it is moved into place: a name ticket_file_pattern does not match, so
	validate, list, the viewer and the hooks never read it. kind is the command: import, new or set."""
	return os.path.join(tickets_dir, "{}.{}-{}.tmp".format(ticket_id, kind, os.getpid()))


def when_not_busy(action, *paths, budgeted=False):
	"""Run a file operation, again after a short wait while Windows reports the file in use by another process.

	budgeted (the store's reads, read_store_json): the waits count against busy_read_budget_seconds for the whole
	command (the thread's busy_read_state), so a permission that never comes (a file the user may not read) costs a few
	seconds once, not every file its full retries. Once the budget is spent, such a read fails at once, and is reported
	as before."""
	for attempt in range(busy_file_attempts):
		try:
			return action(*paths)
		except FileExistsError:
			raise
		except PermissionError:
			spent = getattr(busy_read_state, "spent", 0.0)
			if attempt == busy_file_attempts - 1 or (budgeted and spent >= busy_read_budget_seconds):
				raise
			time.sleep(busy_file_wait_seconds)
			if budgeted:
				busy_read_state.spent = spent + busy_file_wait_seconds


def reset_busy_read_budget():
	"""Start this thread's read retry budget again: the viewer calls it for each request it answers."""
	busy_read_state.spent = 0.0


def read_store_json(path):
	"""A ticket file or tags.json, read with read_json, again while Windows reports it busy (a writer moving the new file
	into place), within the command's retry budget (when_not_busy)."""
	return when_not_busy(read_json, path, budgeted=True)


def place_new_file(temp, target, placed):
	"""Move temp to target, failing when target exists; target joins placed the moment it is ours."""
	if os.name == "nt":
		# On Windows a rename never replaces: it raises FileExistsError.
		when_not_busy(os.rename, temp, target)
		placed.append(target)
		return
	try:
		# Elsewhere a rename replaces silently, and a hard link is what fails on an existing name.
		os.link(temp, target)
	except FileExistsError:
		raise
	except OSError:
		# No hard links on this filesystem: check, then rename.
		if os.path.lexists(target):
			raise FileExistsError("{} already exists".format(os.path.basename(target)))
		os.rename(temp, target)
		placed.append(target)
		return
	placed.append(target)
	os.unlink(temp)


def remove_created(paths):
	"""Delete the files this run created; the names of any that could not be deleted."""
	left = []
	for path in paths:
		try:
			when_not_busy(os.unlink, path)
		except FileNotFoundError:
			pass
		except OSError:
			left.append(os.path.basename(path))
	return left


def folder_names(pattern):
	"""The names in the tickets folder that pattern matches in full, sorted."""
	try:
		return sorted(name for name in os.listdir(tickets_dir) if pattern.fullmatch(name))
	except OSError:
		return []


def import_leftovers():
	"""The names an import left in the tickets folder: its journal and its temporary tickets, by the names it writes."""
	return sorted(folder_names(import_journal_pattern) + folder_names(import_temp_pattern))


def writer_leftovers():
	"""The temporary tickets `new` and `set` write (temp_path): only a killed one leaves its own behind."""
	return folder_names(writer_temp_pattern)


def tag_store_leftovers():
	"""The temporary tags.json a tag store write makes (write_tag_store): only a killed one leaves its own behind."""
	return folder_names(tag_store_temp_pattern)


def leftover_pid(name):
	"""The pid in the name of a file the tool leaves (import_journal_pattern, import_temp_pattern, writer_temp_pattern,
	tag_store_temp_pattern)."""
	for pattern in (import_journal_pattern, import_temp_pattern, writer_temp_pattern, tag_store_temp_pattern):
		match = pattern.fullmatch(name)
		if match:
			return int(match.group(match.lastindex))
	return None


def process_started(pid):
	"""When the process with this pid started, as a Unix time, on Windows (GetProcessTimes); None when it cannot be told
	or elsewhere."""
	if os.name != "nt":
		return None
	try:
		import ctypes
		from ctypes import wintypes
		kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
		kernel32.OpenProcess.restype = wintypes.HANDLE
		kernel32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
		kernel32.GetProcessTimes.argtypes = [wintypes.HANDLE] + [ctypes.POINTER(wintypes.FILETIME)] * 4
		kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
		# PROCESS_QUERY_LIMITED_INFORMATION
		handle = kernel32.OpenProcess(0x1000, False, pid)
		if not handle:
			return None
		try:
			times = [wintypes.FILETIME() for _ in range(4)]
			if not kernel32.GetProcessTimes(handle, *[ctypes.byref(value) for value in times]):
				return None
			ticks = (times[0].dwHighDateTime << 32) | times[0].dwLowDateTime
		finally:
			kernel32.CloseHandle(handle)
	except Exception:
		return None
	# FILETIME counts 100 ns ticks from 1601-01-01; Unix time counts seconds from 1970-01-01.
	return (ticks - 116444736000000000) / 10000000.0


def pid_state(pid, written=None):
	"""Whether a process with this pid runs now, without signalling it: on Windows os.kill(pid, 0) would end the
	process, so there the question goes to OpenProcess and GetExitCodeProcess; elsewhere to os.kill(pid, 0), which
	only checks. "running", "gone", or "unknown" when that cannot be told (access refused, no answer), which every
	caller treats as running, so a cleanup waits rather than removing a running writer's files. written: when the file
	naming the pid was last written (Unix time); a process that started clearly after it (on Windows, process_started)
	reuses the pid and is not its writer."""
	if not isinstance(pid, int) or isinstance(pid, bool) or pid <= 0:
		return "gone"
	if pid == os.getpid():
		return "running"
	if os.name == "nt":
		if pid > 0xFFFFFFFF:
			return "gone"
		try:
			import ctypes
			from ctypes import wintypes
			kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
			kernel32.OpenProcess.restype = wintypes.HANDLE
			kernel32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
			kernel32.GetExitCodeProcess.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)]
			kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
			# PROCESS_QUERY_LIMITED_INFORMATION
			handle = kernel32.OpenProcess(0x1000, False, pid)
			if not handle:
				# ERROR_INVALID_PARAMETER: no process has this pid. Anything else (access denied): one may.
				return "gone" if ctypes.get_last_error() == 87 else "unknown"
			try:
				code = wintypes.DWORD()
				if not kernel32.GetExitCodeProcess(handle, ctypes.byref(code)):
					return "unknown"
				# STILL_ACTIVE; a process that ended with exit code 259 reads as running, which only makes a cleanup wait.
				running = code.value == 259
			finally:
				kernel32.CloseHandle(handle)
		except Exception:
			return "unknown"
		if running and written is not None:
			started = process_started(pid)
			if started is not None and started > written + pid_reuse_slack_seconds:
				return "gone"
		return "running" if running else "gone"
	try:
		os.kill(pid, 0)
	except ProcessLookupError:
		return "gone"
	except (OverflowError, ValueError):
		return "gone"
	except OSError:
		# EPERM: a process of another user has this pid, which is most likely not the writer; it cannot be told.
		return "unknown"
	return "running"


def running_writers(names):
	"""[(name, pid, state)] for each of the names (files in the tickets folder the tool leaves) whose writer runs now,
	or may (pid_state "running" or "unknown"). A name carrying this process's own pid is an earlier process's, since
	the command asking writes none of them."""
	found = []
	for name in names:
		pid = leftover_pid(name)
		if pid is None or pid == os.getpid():
			continue
		try:
			written = os.path.getmtime(os.path.join(tickets_dir, name))
		except OSError:
			written = None
		state = pid_state(pid, written)
		if state != "gone":
			found.append((name, pid, state))
	return found


def pid_list(found):
	"""The distinct pids of running_writers' entries, in numeric order, as text."""
	return ", ".join(str(pid) for pid in sorted({pid for _, pid, _ in found}))


def process_image(pid):
	"""The file name of the program the process with this pid runs (python.exe, chrome.exe), on Windows
	(QueryFullProcessImageNameW, with the same access right pid_state uses); None when it cannot be told or elsewhere."""
	if os.name != "nt" or not 0 < pid <= 0xFFFFFFFF:
		return None
	try:
		import ctypes
		from ctypes import wintypes
		kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
		kernel32.OpenProcess.restype = wintypes.HANDLE
		kernel32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
		kernel32.QueryFullProcessImageNameW.argtypes = [wintypes.HANDLE, wintypes.DWORD, wintypes.LPWSTR,
			ctypes.POINTER(wintypes.DWORD)]
		kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
		# PROCESS_QUERY_LIMITED_INFORMATION
		handle = kernel32.OpenProcess(0x1000, False, pid)
		if not handle:
			return None
		try:
			size = wintypes.DWORD(1024)
			buffer = ctypes.create_unicode_buffer(size.value)
			if not kernel32.QueryFullProcessImageNameW(handle, 0, buffer, ctypes.byref(size)):
				return None
			path = buffer.value
		finally:
			kernel32.CloseHandle(handle)
	except Exception:
		return None
	return ascii_text(os.path.basename(path)) or None


def way_out(found, what, patterns, after="", program="tickets.py"):
	"""For running_writers' entries (running or unknown): what is known of each pid, and how to get out by hand when a
	pid is another program reusing the number. patterns: the names the files take, with {pid} (and [0-9]* for the ticket
	number, so a user's XY-notes.import-4.tmp is not swept in; a glob for bash and PowerShell, not for cmd's del), so the
	instruction is complete however many files there are; each pid's files are counted and a few named
	(listed). program: the script such a writer runs. Nothing is ever removed on this basis: the person decides."""
	notes = []
	files = []
	for pid in sorted({pid for _, pid, _ in found}):
		names = [name for name, other, _ in found if other == pid]
		state = next(state for _, other, state in found if other == pid)
		image = None if state == "unknown" else process_image(pid)
		if state == "unknown":
			notes.append("pid {} could not be checked (access refused)".format(pid))
		elif image is None:
			notes.append("pid {} runs (its program could not be read)".format(pid))
		elif image.lower().startswith("python"):
			notes.append("pid {} is {}".format(pid, image))
		else:
			notes.append("pid {} is {}, not python, so most likely not the {}".format(pid, image, what))
		files.append("for pid {}, {} ({} file(s): {})".format(pid, " and ".join(pattern.format(pid=pid) for pattern in patterns),
			len(names), listed(names)))
	return ("; {}. If a pid is not a python process running {} (another program can reuse the number), or you are "
		"sure no {} is running, delete its files by hand: {}{}".format("; ".join(notes), program, what, "; ".join(files),
		after))


def journal_number(ticket_id):
	"""A journal's ticket id as its number, or None unless it is an id this store's import writes: the store's prefix,
	then the number zero-padded to id_digits (as next_id writes it), of sane length (id_max_digits)."""
	if not isinstance(ticket_id, str) or not id_pattern.fullmatch(ticket_id):
		return None
	digits = ticket_id[len(id_prefix):]
	if len(digits) > id_max_digits:
		return None
	number = int(digits)
	return number if digits == str(number).zfill(id_digits) else None


def read_journal(name):
	"""An import's journal, or None when it does not read as one the import wrote (cut off while it was being written,
	which is before the import wrote anything else; or written by hand). {pid, source, started, first, last, count,
	files: {id: sha256}}: pid is the one in the file's name; the ids carry the store's prefix, are of sane length
	(journal_number) and contiguous; first, last and count are the lowest id, the highest and how many."""
	match = import_journal_pattern.fullmatch(name)
	if not match:
		return None
	try:
		journal = read_json(os.path.join(tickets_dir, name))
	except (OSError, ValueError):
		return None
	if not isinstance(journal, dict) or not isinstance(journal.get("source"), str):
		return None
	pid = journal.get("pid")
	if not isinstance(pid, int) or isinstance(pid, bool) or pid != int(match.group(1)):
		return None
	files = journal.get("files")
	if not isinstance(files, dict) or not files:
		return None
	numbers = []
	for ticket_id, digest in files.items():
		number = journal_number(ticket_id)
		if number is None or not isinstance(digest, str) or not sha256_pattern.fullmatch(digest):
			return None
		numbers.append(number)
	numbers.sort()
	count = journal.get("count")
	if numbers != list(range(numbers[0], numbers[0] + len(numbers))):
		return None
	if not isinstance(count, int) or isinstance(count, bool) or count != len(numbers):
		return None
	if journal_number(journal.get("first")) != numbers[0] or journal_number(journal.get("last")) != numbers[-1]:
		return None
	return journal


def journal_ids(journal):
	return sorted(journal["files"], key=lambda ticket_id: int(ticket_id[len(id_prefix):]))


def batch_text(journal):
	ids = journal_ids(journal)
	# ASCII: the source file's name may be anything, and the abort prints it after it has deleted files.
	return "{}..{} ({} records from {})".format(ids[0], ids[-1], len(ids),
		ascii_text(one_line(os.path.basename(journal["source"]))))


def listed(names):
	return ", ".join(names[:shown_names]) + (", ..." if len(names) > shown_names else "")


def refuse_after_interrupted_import():
	"""`import` does not start while an earlier import's files are in the folder."""
	names = import_leftovers()
	if not names:
		return
	batches = []
	for name in names:
		journal = read_journal(name) if import_journal_pattern.fullmatch(name) else None
		if journal:
			placed = sum(1 for ticket_id in journal["files"] if os.path.lexists(ticket_path(ticket_id)))
			batches.append(" Its batch is {}, of which {} ticket file(s) are in the store.".format(batch_text(journal), placed))
	raise ticket_error("an earlier import was interrupted, or another import is running now: .claude/tickets holds {} "
		"of its file(s) ({}).{} Once no import is running, run `tickets.py import --abort-interrupted` to remove what "
		"it left, then import again".format(len(names), listed(names), "".join(batches)))


def file_digest(path):
	with open(path, "rb") as handle:
		return hashlib.sha256(handle.read()).hexdigest()


def delete_listed(paths):
	for path in paths:
		try:
			when_not_busy(os.unlink, path)
		except FileNotFoundError:
			pass
		except OSError as error:
			raise ticket_error("could not delete {} ({}); when nothing holds it, run --abort-interrupted "
				"again".format(os.path.basename(path), one_line(error)))


def abort_interrupted():
	"""Remove what a killed import left, by its journal: the batch's ticket files that still hold exactly the bytes
	the import wrote (sha256, recorded in the journal) and were written after the journal, its temporary files, then the
	journal. Nothing else.

	A ticket file of the batch's ids with other content is kept and named: it was changed since (`set`), or
	another writer filed it under an id the import had not reached. So is one last written before the journal (beyond
	journal_mtime_slack_seconds), which the import cannot have written. When every ticket of the batch is in the
	store and no temporary file is left, the import had finished but for removing its journal: the tickets are
	kept. A journal that does not read as one the import wrote (read_journal) was cut off before the import wrote any
	ticket, or written by hand: no ticket is removed, only temporary files and the journal go.

	It removes nothing while the process named in any of the import's files runs, or cannot be checked (pid_state):
	that import is at work, not killed. The ERROR says what each pid is (its program on Windows, or that access was
	refused) and names the files to delete by hand when that pid is another program reusing the number (way_out); it
	never removes them on that basis itself. It also removes the temporary tickets a killed `new` or `set` left
	(writer_leftovers), and the temporary tags.json a killed tag store write left (tag_store_leftovers), each once the
	process named in it is gone; one whose process runs, or cannot be checked, is kept and named. Only the names the
	tool writes are ever touched (import_journal_pattern, import_temp_pattern, writer_temp_pattern,
	tag_store_temp_pattern).
	"""
	names = import_leftovers()
	writers = writer_leftovers()
	tag_temps = tag_store_leftovers()
	if not names and not writers and not tag_temps:
		raise ticket_error("no interrupted import, no killed new or set and no killed tag store write: .claude/tickets "
			"holds no journal.import-<pid>.tmp, <id>.import-<pid>.tmp, <id>.new-<pid>.tmp, <id>.set-<pid>.tmp or "
			"tags.store-<pid>.tmp file")
	running = running_writers(names)
	if running:
		raise ticket_error("an import is running now (pid {}; {}); nothing was removed. Once it has finished, run "
			"--abort-interrupted again only if its files are still there{}".format(pid_list(running),
			listed([name for name, _, _ in running]), way_out(running, "import",
			["journal.import-{pid}.tmp", id_prefix + "[0-9]*.import-{pid}.tmp"],
			"; tickets the import already placed stay: run validate")))
	covered = set()
	for name in names:
		match = import_journal_pattern.fullmatch(name)
		if not match:
			continue
		covered.add(name)
		suffix = ".import-{}.tmp".format(match.group(1))
		journal = read_journal(name)
		if journal is None:
			temps = [other for other in names if other.endswith(suffix) and id_pattern.fullmatch(other[:-len(suffix)])]
			covered.update(temps)
			delete_listed([os.path.join(tickets_dir, other) for other in temps + [name]])
			print("removed a journal that does not read as one the import wrote ({}) and {} temporary file(s); no ticket "
				"was removed".format(name, len(temps)))
			continue
		ids = journal_ids(journal)
		temps = [ticket_id + suffix for ticket_id in ids if ticket_id + suffix in names]
		covered.update(temps)
		present = [ticket_id for ticket_id in ids if os.path.lexists(ticket_path(ticket_id))]
		if len(present) == len(ids) and not temps:
			delete_listed([os.path.join(tickets_dir, name)])
			print("the interrupted import had placed all of {}; kept them and removed its journal".format(batch_text(journal)))
			continue
		try:
			# The import writes its journal before any ticket, so a ticket file last written clearly before the journal
			# is not one it placed, whatever its digest (a journal written by hand afterwards).
			since = os.path.getmtime(os.path.join(tickets_dir, name)) - journal_mtime_slack_seconds
			ours = [ticket_id for ticket_id in present if file_digest(ticket_path(ticket_id)) == journal["files"][ticket_id]
				and os.path.getmtime(ticket_path(ticket_id)) >= since]
		except OSError as error:
			raise ticket_error("could not read a ticket of the batch ({}); nothing was removed".format(one_line(error)))
		kept = [ticket_id for ticket_id in present if ticket_id not in ours]
		# The journal goes last: a run that stops part-way can be run again.
		delete_listed([ticket_path(ticket_id) for ticket_id in reversed(ours)])
		delete_listed([os.path.join(tickets_dir, other) for other in temps + [name]])
		print("removed {} ticket(s){} and {} temporary file(s) of the interrupted import {}".format(len(ours),
			" {}..{}".format(ours[0], ours[-1]) if ours else "", len(temps), batch_text(journal)))
		if kept:
			print("kept {} ticket(s) that are not as the import wrote them (changed or filed since, or older than its "
				"journal): {}; run validate".format(len(kept), listed(kept)))
	orphans = [name for name in names if name not in covered]
	if orphans:
		delete_listed([os.path.join(tickets_dir, name) for name in orphans])
		print("removed {} temporary file(s) no journal covers: {}".format(len(orphans), listed(orphans)))
	busy = running_writers(writers)
	stale = [name for name in writers if name not in {busy_name for busy_name, _, _ in busy}]
	if stale:
		delete_listed([os.path.join(tickets_dir, name) for name in stale])
		print("removed {} temporary file(s) a killed new or set left: {}".format(len(stale), listed(stale)))
	if busy:
		print("kept {} temporary file(s) of a new or set running now (pid {}): {}{}".format(len(busy), pid_list(busy),
			listed([name for name, _, _ in busy]), way_out(busy, "new or set",
			[id_prefix + "[0-9]*.new-{pid}.tmp", id_prefix + "[0-9]*.set-{pid}.tmp"])))
	busy = running_writers(tag_temps)
	stale = [name for name in tag_temps if name not in {busy_name for busy_name, _, _ in busy}]
	if stale:
		delete_listed([os.path.join(tickets_dir, name) for name in stale])
		print("removed {} temporary tags.json file(s) a killed tag store write left: {}; tags.json is as that write "
			"found it".format(len(stale), listed(stale)))
	if busy:
		print("kept {} temporary tags.json file(s) of a tag store write running now (pid {}): {}{}".format(len(busy),
			pid_list(busy), listed([name for name, _, _ in busy]), way_out(busy, "author.py tag store write",
			["tags.store-{pid}.tmp"], program="author.py")))
	return 0


def write_import(batch, source):
	"""Write every (ticket id, bytes) of the batch, or none: a journal first, then each ticket to a temporary name,
	then each moved into place in order, then the journal removed. Never replaces a file. On any failure, and on
	Ctrl+C, what this run created is deleted again. A killed process cannot do that: its journal stays, naming
	the batch (ids, and the sha256 of each file it writes), for refuse_after_interrupted_import and
	abort_interrupted."""
	journal = os.path.join(tickets_dir, "journal.import-{}.tmp".format(os.getpid()))
	record = {"pid": os.getpid(), "source": os.path.basename(source), "started": utc_now(), "first": batch[0][0],
		"last": batch[-1][0], "count": len(batch), "files": {ticket_id: hashlib.sha256(data).hexdigest() for ticket_id, data in batch}}
	journal_ours = False
	temps = []
	placed = []
	try:
		handle = open(journal, "x", encoding="utf-8", newline="\n")
		journal_ours = True
		with handle:
			handle.write(json.dumps(record, indent=1) + "\n")
			handle.flush()
			os.fsync(handle.fileno())
		for ticket_id, data in batch:
			if os.path.lexists(ticket_path(ticket_id)):
				raise ticket_error("{}.json already exists (another writer took the id)".format(ticket_id))
			temp = temp_path(ticket_id, "import")
			handle = open(temp, "xb")
			temps.append(temp)
			with handle:
				handle.write(data)
		for (ticket_id, data), temp in zip(batch, temps):
			try:
				place_new_file(temp, ticket_path(ticket_id), placed)
			except FileExistsError:
				raise ticket_error("{}.json already exists (another writer took the id)".format(ticket_id))
		when_not_busy(os.unlink, journal)
	except BaseException as error:
		left = remove_created(temps + placed)
		if not left and journal_ours:
			# The journal stays while any file of the batch does, so --abort-interrupted can finish the job.
			left = remove_created([journal])
		if not isinstance(error, (OSError, ticket_error)):
			raise
		if left:
			raise ticket_error("import failed: {}; rolling back could not delete {} of the batch's files ({}); when "
				"nothing holds them, run `tickets.py import --abort-interrupted`".format(one_line(error), len(left), listed(left)))
		raise ticket_error("import failed: {}; rolled back, no ticket of the batch was kept".format(one_line(error)))


def command_import(args):
	if args.abort_interrupted:
		if args.file is not None or args.dry_run:
			raise ticket_error("--abort-interrupted takes no file and no --dry-run")
		return abort_interrupted()
	if args.file is None:
		raise ticket_error("import needs a file: import <file.json> [--dry-run], or import --abort-interrupted")
	refuse_after_interrupted_import()
	records = read_import_file(args.file)
	if not records:
		print("OK: 0 records")
		return 0
	# Dates are compared with the time this run started.
	now = utc_now()
	schema = read_schema()
	store = writable_tag_store()
	groups = load_tag_groups(store)
	# The store itself first, as `validate` checks it: a link into it must lead to a ticket that reads. Its warnings
	# (tickets still carrying tag names) do not stop an import.
	_, store_problems = validate_all()
	if store_problems:
		raise ticket_error("the store has {} problem(s); run validate and fix them first".format(len(store_problems)))
	# The ids `new` would give, one after another: next_id()'s number, then +1 each.
	names = ticket_files()
	number = max([int(ticket_file_pattern.fullmatch(name).group(1)) for name in names] + [first_ticket_number - 1])
	fit = id_number_max - number
	if 0 < fit < len(records):
		raise ticket_error("the batch does not fit: its {} records would take {}..{}{}, but ids stop at {}{} ({} digits; "
			"the limit is 9, or one more than config.json's first_ticket_number has). Only the first {} record(s) fit "
			"({}..{}): import at most {}; nothing was written".format(len(records), id_for_number(number + 1), id_prefix,
			number + len(records), id_prefix, id_number_max, id_max_digits, fit, id_for_number(number + 1),
			id_for_number(id_number_max), fit))
	ids = [id_for_number(number + offset) for offset in range(1, len(records) + 1)]
	known_ids = {name[:-len(".json")] for name in names} | set(ids)
	batch = []
	failures = []
	failed_records = 0
	# The records that built, for the block pairs: an import writes no existing ticket, so both sides of a block must be
	# in the batch's records (or the other side already in the store).
	built = {}
	for index, (record, ticket_id) in enumerate(zip(records, ids)):
		ticket, problems = import_ticket(record, ticket_id, now, schema, store, groups, known_ids)
		if not problems:
			data, field = ticket_bytes(ticket)
			if data is None:
				problems = [(field, "holds text that cannot be written as UTF-8 (half of a surrogate pair)")]
			else:
				batch.append((ticket_id, data))
		if not problems:
			built[ticket_id] = (index, ticket)
		if problems:
			failed_records += 1
		for field, problem in problems:
			failures.append("FAIL record {} ({}): {}: {}".format(index, ticket_id, shown_key(field), problem))
	if built:
		existing, _ = load_all()
		by_id = {item["id"]: item for item in existing if isinstance(item, dict) and isinstance(item.get("id"), str)}
		by_id.update({ticket_id: ticket for ticket_id, (_, ticket) in built.items()})
		pair_failed = set()
		for ticket_id, problem in block_pair_errors([ticket for _, ticket in built.values()], by_id, set_hint=False):
			pair_failed.add(ticket_id)
			failures.append("FAIL record {} ({}): links: {}. A block is stored on both tickets and an import never changes "
				"a ticket already in the store: give the block to both records, or drop it here and add it after the import "
				"with tickets.py set {} --block / --blocked-by".format(built[ticket_id][0], ticket_id, problem, ticket_id))
		failed_records += len(pair_failed)
	for failure in failures[:import_shown_failures]:
		line = ascii_text(one_line(failure))
		print(line if len(line) <= import_failure_width else line[:import_failure_width - 3] + "...")
	if len(failures) > import_shown_failures:
		print("... {} more not shown".format(len(failures) - import_shown_failures))
	if failures:
		print("{} problem(s) in {} of {} record(s); nothing written".format(len(failures), failed_records, len(records)))
		return 1
	if args.dry_run:
		print("OK: {} records valid; would create {}..{}".format(len(records), ids[0], ids[-1]))
		return 0
	write_import(batch, args.file)
	print("imported {} tickets {}..{}".format(len(records), ids[0], ids[-1]))
	return 0


def schema_enum(schema, field):
	"""The allowed values of a ticket field, from read_schema's schema; a ticket_error when the schema lacks them."""
	try:
		return schema["properties"][field]["enum"]
	except (KeyError, TypeError):
		raise ticket_error("the schema {} has no properties.{}.enum".format(one_line(schema_path), field))


class strict_parser(argparse.ArgumentParser):
	"""argparse without prefix abbreviation: --body must not quietly reach --body-file."""
	def __init__(self, *args, **kwargs):
		kwargs["allow_abbrev"] = False
		super().__init__(*args, **kwargs)


def build_parser():
	schema = read_schema()
	enum_of = lambda field: schema_enum(schema, field)
	parser = strict_parser(description="{} ticket store (.claude/tickets).".format(ascii_text(config["store_name"])))
	commands = parser.add_subparsers(dest="command", required=True, parser_class=strict_parser)

	def add_links(sub):
		sub.add_argument("--link-ticket", action="append", help="another ticket id (repeatable)")
		sub.add_argument("--link-file", action="append", help="repo path or path:line (repeatable)")
		sub.add_argument("--link-commit", action="append", help="commit hash (repeatable)")
		sub.add_argument("--block", action="append", help="this ticket blocks that one (ids, comma-separated, "
			"repeatable); the other ticket gets blocked_by")
		sub.add_argument("--blocked-by", action="append", help="this ticket is blocked by that one (ids, repeatable); the "
			"other ticket gets blocks")
		sub.add_argument("--relates", action="append", help="this ticket relates to that one (ids, repeatable)")

	def add_link_removes(sub):
		sub.add_argument("--remove-block", action="append", help="drop a blocks link, on both tickets (ids, repeatable)")
		sub.add_argument("--remove-blocked-by", action="append", help="drop a blocked_by link, on both tickets")
		sub.add_argument("--remove-relates", action="append", help="drop a relates-to link (also from links.tickets)")

	new = commands.add_parser("new", help="create a ticket with the next id")
	new.add_argument("--summary", required=True)
	new.add_argument("--severity", required=True, choices=enum_of("severity"))
	new.add_argument("--owner", required=True, choices=enum_of("owner_area"))
	new.add_argument("--author", required=True, choices=enum_of("author"))
	new.add_argument("--status", default="open", choices=enum_of("status"))
	new.add_argument("--target", default=default_target_patch)
	new.add_argument("--tags", help="comma-separated tag names or ids from tags.json")
	new.add_argument("--body-file", help="markdown file for the body, or - for stdin")
	add_links(new)
	new.set_defaults(handler=command_new)

	change = commands.add_parser("set", help="change fields of a ticket (bumps updated)")
	change.add_argument("id")
	change.add_argument("--summary")
	change.add_argument("--status", choices=enum_of("status"))
	change.add_argument("--severity", choices=enum_of("severity"))
	change.add_argument("--owner", choices=enum_of("owner_area"))
	change.add_argument("--author", choices=enum_of("author"))
	change.add_argument("--target")
	change.add_argument("--tags", help="replace all tags (comma-separated names or ids)")
	change.add_argument("--add-tag", help="comma-separated tag names or ids to add")
	change.add_argument("--remove-tag", help="comma-separated tag names or ids to remove")
	body_source = change.add_mutually_exclusive_group()
	body_source.add_argument("--body-file", help="replace the body from a markdown file, or - for stdin")
	body_source.add_argument("--append-body-file", help="append a markdown file (or - for stdin) to the body")
	add_links(change)
	add_link_removes(change)
	change.set_defaults(handler=command_set)

	comment = commands.add_parser("comment", help="append a comment to a ticket (bumps updated)")
	comment.add_argument("id")
	comment.add_argument("--author", required=True, choices=enum_of("author"))
	comment.add_argument("--body-file", required=True, help="markdown file for the comment, or - for stdin")
	comment.set_defaults(handler=command_comment)

	listing = commands.add_parser("list", help="list tickets, filtered")
	listing.add_argument("--status", help="comma-separated statuses")
	listing.add_argument("--active", action="store_true", help="every status but done and wont_do ({})".format(", ".join(active_statuses)))
	listing.add_argument("--severity", help="comma-separated severities")
	listing.add_argument("--owner", help="comma-separated owner areas")
	listing.add_argument("--target", help="target patch")
	listing.add_argument("--tag", help="comma-separated tag names or ids; a ticket must carry all of them")
	listing.add_argument("--text", help="substring of summary or body")
	listing.set_defaults(handler=command_list)

	show = commands.add_parser("show", help="print one ticket")
	show.add_argument("id")
	show.set_defaults(handler=command_show)

	validate = commands.add_parser("validate", help="check every ticket; non-zero exit on a problem")
	validate.set_defaults(handler=command_validate)

	importing = commands.add_parser("import", help="create many tickets from a JSON file, all or none",
		description="Create one ticket per record of a JSON array, in input order, with the next ids. The store "
			"and every record are validated first, and nothing is written unless all pass. A record: summary, "
			"severity, owner, author (required); status, target, tags (names or ids), body, links {tickets, files, commits, blocks, blocked_by, relates_to}, comments, "
			"created, updated, id (optional). created and updated are UTC, written YYYY-MM-DDTHH:MM:SSZ, and not "
			"in the future; a ticket link may name a ticket of the same file. A failure or Ctrl+C part-way removes "
			"what the run wrote. After a killed import, the next one refuses to start until --abort-interrupted "
			"has removed what it left.")
	importing.add_argument("file", nargs="?", help="a UTF-8 JSON file holding an array of records")
	importing.add_argument("--dry-run", action="store_true", help="validate and print the id range; write nothing")
	importing.add_argument("--abort-interrupted", action="store_true", help="remove what a killed import left, by "
		"its journal: its temporary files and the tickets it placed that are unchanged since; also the temporary "
		"files of a killed new or set, and of a killed tag store write (tags.store-<pid>.tmp). Refuses while that "
		"import's process runs; takes no file")
	importing.set_defaults(handler=command_import)

	readme = commands.add_parser("readme", help="write .claude/tickets/README.md, the agents README, when it is missing")
	readme.set_defaults(handler=command_readme)
	return parser


def readme_pointer():
	"""After the viewer's one-click update (B65), say once, on stderr, where the agents README is: when
	.claude/tickets/README.md exists and build/tickets/readme-seen does not hold <CLI_DIR>/VERSION. Then it does. The
	updater's own runs (its validate) set TICKETS_UPDATER, so they never use the notice up."""
	if os.environ.get("TICKETS_UPDATER"):
		return
	# Where updater.state_paths writes it: always the repository's store folder, whatever folder a viewer serves
	# (serve.py --data-dir moves tickets_dir in its own process only).
	readme = os.path.join(repo_root, ".claude", "tickets", "README.md")
	seen_path = os.path.join(repo_root, "build", "tickets", "readme-seen")
	try:
		with open(os.path.join(script_dir, "VERSION"), "r", encoding="utf-8") as handle:
			version = handle.read(64).strip()
	except OSError:
		return
	if not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", version) or not os.path.isfile(readme):
		return
	try:
		with open(seen_path, "r", encoding="utf-8") as handle:
			if handle.read(64).strip() == version:
				return
	except OSError:
		pass
	sys.stderr.write("tickets: store updated to {} - see .claude/tickets/README.md\n".format(version))
	try:
		os.makedirs(os.path.dirname(seen_path), exist_ok=True)
		with open(seen_path, "w", encoding="utf-8", newline="\n") as handle:
			handle.write(version + "\n")
	except OSError:
		pass


def command_readme(args):
	"""Write .claude/tickets/README.md, the agents README the updater keeps, for the installed release when it is
	missing (the first one, after a manual install of 1.5.0)."""
	import updater
	try:
		path, written = updater.write_first_readme(repo_root, script_dir, config)
	except (OSError, ValueError) as error:
		raise ticket_error("cannot write the README: {}".format(error))
	shown = os.path.relpath(path, repo_root).replace(os.sep, "/")
	print("wrote {}".format(shown) if written else "{} exists; unchanged (the updater keeps it)".format(shown))
	return 0


def main():
	readme_pointer()
	try:
		# build_parser reads the schema, so it sits inside the try.
		args = build_parser().parse_args()
		result = args.handler(args)
	except ticket_error as error:
		print("ERROR " + ascii_text(str(error)))
		return 1
	return result or 0


if __name__ == "__main__":
	sys.exit(main())
