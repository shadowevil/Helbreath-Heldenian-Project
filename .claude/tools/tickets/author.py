"""Ticket authoring tool for agents: the tag store, the tag groups, drafts and new tickets, and the move to numbered
tags. Flags and JSON only, never a prompt. It writes only through tickets.py beside it (write_tag_store,
create_ticket, write_ticket), so that module stays the store's only writer and every check it makes applies here.

    python <CLI_DIR>/author.py tags [--unused] [--group <g>] [--json]
    python <CLI_DIR>/author.py tags --probe [--dry-run] [--json]
    python <CLI_DIR>/author.py tags add <name> [--color #rrggbb] [--description "..."] [--json]
    python <CLI_DIR>/author.py tags rename <old name or id> <new name> [--json]
    python <CLI_DIR>/author.py rules [--json]
    python <CLI_DIR>/author.py draft --summary "..." --severity medium --owner <area> --author pm --tags bug [--json]
    python <CLI_DIR>/author.py draft --from-json <file, or - for stdin> [--json]
    python <CLI_DIR>/author.py new <the same input as draft> [--json]
    python <CLI_DIR>/author.py comment <id> --author pm --body-file <file, or - for stdin> [--json]
    python <CLI_DIR>/author.py --convert <ticket file or id> [--dry-run] [--json]
    python <CLI_DIR>/author.py --convert-all [--dry-run] [--json]

tags lists the store: id, name, groups, how many tickets carry the tag, description. rules prints the tag groups
(config.json tag_groups). draft prints the ticket `new` would write, or names every problem, each missing group
among them ("tag group type: pick exactly 1 of bug, cleanup, docs (has none)"); new writes it. Their input is the
flags of `tickets.py new`, or one JSON object with the keys of an import record (tickets.py import_ticket) except
id, created and updated, which new sets. Tags are names or ids, and are written as ids.

Moving a 1.2 store (tags.json a name map, tickets holding names): `tags --probe` numbers tags.json in name order,
keeping every name, colour and description, then adds each name found on the tickets that it lacks, with the next
id, the colour default_tag_color and the description "found by probe". `--convert-all` then rewrites every
ticket's names as ids, each file atomically, leaving ids alone and `updated` unchanged (a format change, not an
edit; the tag groups are not checked here, validate reports them). Both refuse, writing nothing, while a value
cannot be placed, naming the file and the tag. Run again, each changes nothing. `tags rename` waits for the move: it
refuses, writing nothing, while any ticket still carries the old or the new name as text (the tag would leave those
tickets, or join them), and while any ticket file does not read (it cannot be checked).

tags add, tags rename, tags --probe and a --convert / --convert-all run (not their --dry-run) hold tags.json.lock while
they read and write (tickets.tag_store_lock): concurrent writers take turns, and one that waits too long refuses.

Exit 0 when it did the job (or, with --dry-run and draft, would do it); 1 when it refused or the input is invalid.
With --json, stdout is one JSON object, {"ok": true, ...} or {"ok": false, "errors": [...]}; without it, lines for
people. Everything printed is ASCII.
"""

import argparse
import json
import os
import sys

# Importing tickets.py would leave a __pycache__ in the tool folder.
sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tickets  # noqa: E402 - the sibling module, found through the path above

probe_description = "found by probe"
# Columns of `tags`: the description takes what is left of a 120-column console, cut when longer.
tags_description_width = 60


class refused(Exception):
	"""A refusal: a summary line, the problems behind it (one string each) and extra JSON fields."""
	def __init__(self, summary, problems=(), data=None):
		Exception.__init__(self, summary)
		self.summary = summary
		self.problems = list(problems)
		self.data = data or {}


def json_wanted():
	return "--json" in sys.argv[1:]


def text(value):
	return tickets.ascii_text(tickets.one_line(value))


def emit(args, data, lines):
	"""The answer: data as one JSON object with --json ({"ok": true} first), else the lines."""
	if args.json:
		answer = {"ok": True}
		answer.update(data)
		print(json.dumps(answer, indent=2))
		return
	for line in lines:
		print(tickets.ascii_text(line))


def fail(as_json, summary, problems=(), data=None):
	if as_json:
		answer = {"ok": False, "errors": list(problems) or [summary]}
		if problems:
			answer["summary"] = summary
		answer.update(data or {})
		print(json.dumps(answer, indent=2))
	else:
		for problem in problems:
			print("FAIL " + tickets.ascii_text(problem))
		print("ERROR " + tickets.ascii_text(summary))
	return 1


class argument_parser(tickets.strict_parser):
	"""argparse without prefix abbreviation, with a usage error as a refusal: exit 1, and JSON when --json was given."""
	def error(self, message):
		if not json_wanted():
			self.print_usage(sys.stderr)
		sys.exit(fail(json_wanted(), "usage: " + message))


def listed(names):
	return tickets.listed([text(name) for name in names])


def read_ticket_file(name):
	"""One ticket file of the store, read again while Windows reports it busy (a `set` moving it into place), within the
	command's retry budget (tickets.read_store_json)."""
	return tickets.read_store_json(os.path.join(tickets.tickets_dir, name))


def each_ticket():
	"""(file name, ticket or None, problem or None) for every ticket file of the store, in id order."""
	for name in tickets.ticket_files():
		try:
			yield name, read_ticket_file(name), None
		except (OSError, ValueError, RecursionError) as error:
			yield name, None, "{}: unreadable JSON ({})".format(name, text(error))


def is_id_text(value):
	return isinstance(value, str) and tickets.tag_id_text_pattern.fullmatch(value) is not None


# --- tags: the store, its usage, --probe, add, rename ---------------------------------------------

def tag_usage(store):
	"""How the tickets use the store: {id: tickets carrying it}, {name not in the store: tickets}, {id the store lacks:
	tickets}, unreadable files (one problem each), and the number of tickets read. A ticket counts once per tag,
	whether it carries the tag's name, its id or both."""
	by_name = tickets.tag_ids_by_name(store)
	counts = {int(key): 0 for key in store["tags"]}
	undefined = {}
	unknown = {}
	problems = []
	total = 0
	for name, ticket, problem in each_ticket():
		if problem:
			problems.append(problem)
			continue
		total += 1
		values = ticket.get("tags") if isinstance(ticket, dict) else None
		if not isinstance(values, list):
			continue
		carried, missing, lacking = set(), set(), set()
		for value in values:
			tag_id = tickets.stored_tag_id(store, value, by_name)
			if tag_id is not None:
				carried.add(tag_id)
			elif tickets.is_tag_id(value) or is_id_text(value):
				lacking.add(str(value))
			elif isinstance(value, str):
				missing.add(value)
		for tag_id in carried:
			counts[tag_id] += 1
		for value in missing:
			undefined[value] = undefined.get(value, 0) + 1
		for value in lacking:
			unknown[value] = unknown.get(value, 0) + 1
	return counts, undefined, unknown, problems, total


def command_tags_list(args):
	store, version, problems = tickets.tag_store_state()
	if store is None:
		raise refused("tags.json cannot be read", problems)
	warnings = list(problems)
	if version == 1:
		warnings.append("tags.json is in the old format: these ids are provisional (name order) until tags --probe "
			"numbers it")
	try:
		groups = tickets.load_tag_groups(store)
	except tickets.ticket_error as error:
		groups = []
		warnings.append(str(error))
	if args.group is not None and args.group not in [group["name"] for group in groups]:
		raise refused("no tag group '{}' (rules lists them)".format(text(args.group)))
	counts, undefined, unknown, unreadable, total = tag_usage(store)
	warnings += unreadable
	rows = []
	for key in sorted(store["tags"], key=int):
		tag_id = int(key)
		entry = store["tags"][key]
		member = [group["name"] for group in groups if tag_id in group["ids"]]
		if args.group is not None and args.group not in member:
			continue
		if args.unused and counts[tag_id]:
			continue
		rows.append({"id": tag_id, "name": entry["name"], "color": entry["color"], "description": entry["description"],
			"groups": member, "tickets": counts[tag_id]})
	if undefined:
		warnings.append("{} tag name(s) on tickets are not in the store: {}; run author.py tags --probe".format(
			len(undefined), listed(sorted(undefined))))
	if unknown:
		warnings.append("{} tag id(s) on tickets are not in the store: {}".format(len(unknown), listed(sorted(unknown))))
	name_width = max([4] + [len(row["name"]) for row in rows])
	group_width = max([6] + [len(", ".join(row["groups"])) for row in rows])
	lines = ["{:>4} {:<{}} {:>7}  {:<{}}  {}".format("id", "name", name_width, "tickets", "groups", group_width,
		"description")]
	for row in rows:
		description = text(row["description"])
		if len(description) > tags_description_width:
			description = description[:tags_description_width - 3] + "..."
		lines.append("{:>4} {:<{}} {:>7}  {:<{}}  {}".format(row["id"], row["name"], name_width, row["tickets"],
			", ".join(row["groups"]) or "-", group_width, description))
	lines.append("{} of {} tag(s) shown; {} ticket(s) read".format(len(rows), len(store["tags"]), total))
	lines += ["WARNING " + warning for warning in warnings]
	emit(args, {"format": version, "next_id": store["next_id"], "tags": rows, "tickets": total,
		"undefined_names": undefined, "unknown_ids": unknown, "warnings": warnings}, lines)
	return 0


def command_tags_probe(args):
	if args.unused or args.group is not None:
		raise refused("tags --probe takes no --unused or --group")
	try:
		raw = tickets.read_tag_file()
	except FileNotFoundError:
		raw = None
	except (OSError, ValueError) as error:
		raise refused("tags.json: unreadable ({}); fix it first, nothing written".format(text(error)))
	if raw is None:
		store, version, problems = tickets.empty_tag_store(), 0, []
	else:
		store, version, problems = tickets.parse_tag_store(raw)
	if store is None or problems:
		raise refused("tags.json has {} problem(s); fix them first, nothing written".format(len(problems)), problems)
	by_name = tickets.tag_ids_by_name(store)
	found = {}
	refusals = []
	warnings = []
	for name, ticket, problem in each_ticket():
		if problem:
			warnings.append(problem + "; its tags were not probed")
			continue
		values = ticket.get("tags") if isinstance(ticket, dict) else None
		if not isinstance(values, list):
			warnings.append("{}: has no tags list; run validate".format(name))
			continue
		for value in values:
			# The refusals for a store not numbered yet are worded as validate words them (tickets.ticket_tag_ids).
			if tickets.is_tag_id(value):
				if version != 2:
					refusals.append("{}: {}".format(name, tickets.unnumbered_id_problem(value)))
				elif str(value) not in store["tags"]:
					warnings.append("{}: tag id {} is not in the store; probe cannot name it".format(name, value))
			elif is_id_text(value):
				# A 1.2 ticket's strings are names, and an all-digit name reads as an id from 1.3.0 on.
				if version != 2 or tickets.stored_tag_id(store, value, by_name) is None:
					refusals.append("{}: {}".format(name, tickets.all_digit_name_problem(value)))
			elif isinstance(value, str) and tickets.tag_name_pattern.fullmatch(value):
				if value not in by_name:
					found.setdefault(value, []).append(name)
			else:
				refusals.append("{}: tag {} is not a valid tag name (lowercase letters, digits and _); fix it first".format(
					name, tickets.shown(value)))
	if refusals:
		raise refused("probe refused, nothing written: {} value(s) cannot be loaded".format(len(refusals)), refusals,
			{"warnings": warnings})
	added = []
	for tag_name in sorted(found):
		tag_id = store["next_id"]
		store["tags"][str(tag_id)] = {"name": tag_name, "color": tickets.default_tag_color, "description": probe_description}
		store["next_id"] += 1
		added.append({"id": tag_id, "name": tag_name, "tickets": len(set(found[tag_name]))})
	numbered = [{"id": int(key), "name": store["tags"][key]["name"]} for key in sorted(store["tags"], key=int)
		if version != 2 and not any(item["id"] == int(key) for item in added)]
	changed = version != 2 or bool(added)
	would = "would " if args.dry_run else ""
	lines = []
	if version == 1:
		lines.append("tags.json: {}number the {} tag(s) of the old format in name order, ids 1..{}".format(
			would or "", len(numbered), len(numbered)) if numbered else "tags.json: {}move the empty old-format map to "
			"format 2".format(would))
	elif version == 0:
		lines.append("tags.json: absent; {}create a numbered store".format(would or ""))
	for item in added:
		lines.append("{}add tag {} {} (on {} ticket(s))".format(would, item["id"], item["name"], item["tickets"]))
	if not changed:
		lines.append("nothing to add: tags.json is numbered and holds every tag name on the tickets")
	elif args.dry_run:
		lines.append("dry run: nothing written")
	else:
		# A format-1 file's store carries "format": 1 (its ids were provisional); from here they are the ids.
		store["format"] = tickets.tag_store_format_version
		tickets.write_tag_store(store)
		lines.append("wrote tags.json: {} tag(s), next_id {}".format(len(store["tags"]), store["next_id"]))
	lines += ["WARNING " + warning for warning in warnings]
	emit(args, {"dry_run": args.dry_run, "from_format": version, "numbered": numbered, "added": added,
		"written": changed and not args.dry_run, "next_id": store["next_id"], "warnings": warnings}, lines)
	return 0


def check_new_name(store, name):
	if is_id_text(name):
		raise refused("the tag name '{}' is all digits, which reads as a tag id; give it a letter or _".format(text(name)))
	if not tickets.tag_name_pattern.fullmatch(name):
		raise refused("bad tag name '{}': lowercase letters, digits and _, with at least one letter or _".format(text(name)))
	taken = tickets.tag_ids_by_name(store).get(name)
	if taken is not None:
		raise refused("the tag name '{}' is taken by id {}".format(name, taken))


def command_tags_add(args):
	if args.unused or args.group is not None or args.probe:
		raise refused("tags add takes no --unused, --group or --probe")
	store = tickets.writable_tag_store()
	check_new_name(store, args.name)
	color = args.color if args.color is not None else tickets.default_tag_color
	if not tickets.colour_pattern.fullmatch(color):
		raise refused("--color must be a colour written #rrggbb, not {}".format(tickets.shown(color)))
	description = args.description if args.description is not None else ""
	if tickets.has_line_break(description):
		raise refused("--description has a line break; it must be one line")
	tag_id = store["next_id"]
	store["tags"][str(tag_id)] = {"name": args.name, "color": color, "description": description}
	store["next_id"] += 1
	tickets.write_tag_store(store)
	emit(args, {"id": tag_id, "name": args.name, "color": color, "description": description},
		["added tag {} {} ({})".format(tag_id, args.name, color)])
	return 0


def tickets_carrying_names(*names):
	"""({name: [ticket files carrying it as a string]}, [one problem per ticket file that does not read]): the tickets a
	rename would change the meaning of. A file that does not read cannot be checked, so rename refuses while there is
	one."""
	carrying = {name: [] for name in names}
	unreadable = []
	for file_name, ticket, problem in each_ticket():
		if problem:
			unreadable.append(problem)
			continue
		values = ticket.get("tags") if isinstance(ticket, dict) else None
		for name in names:
			if isinstance(values, list) and name in values:
				carrying[name].append(file_name)
	return carrying, unreadable


def command_tags_rename(args):
	if args.unused or args.group is not None or args.probe:
		raise refused("tags rename takes no --unused, --group or --probe")
	store = tickets.writable_tag_store()
	tag_id = tickets.resolve_tag(store, args.old)
	if tag_id is None:
		raise refused("no tag {} in tags.json (author.py tags lists the store)".format(tickets.shown(args.old)))
	old_name = store["tags"][str(tag_id)]["name"]
	check_new_name(store, args.new)
	# A ticket still carrying the old name as text (not converted yet) would lose the tag, and one carrying the new
	# name as text would gain it: the rename waits for --convert-all. Checked under the lock (command_tags), so no
	# conversion or other rename runs meanwhile.
	carrying, unreadable = tickets_carrying_names(old_name, args.new)
	if unreadable:
		# Such a file may carry either name as text; it is checked once it reads.
		raise refused("rename refused, nothing written: {} ticket file(s) do not read, so they cannot be checked for '{}' "
			"or '{}'; fix them first (tickets.py validate names them)".format(len(unreadable), old_name, args.new), unreadable)
	warnings = []
	if carrying[old_name] or carrying[args.new]:
		problems = []
		if carrying[old_name]:
			problems.append("{} ticket(s) still carry the tag name '{}' as text and would lose the tag: {}; run author.py "
				"--convert-all first, then rename".format(len(carrying[old_name]), old_name, listed(carrying[old_name])))
		if carrying[args.new]:
			# tags.json lacks that name (check_new_name), so --convert-all would refuse these tickets too.
			problems.append("{} ticket(s) carry the tag name '{}' as text, which tags.json lacks, and would gain tag {}: {}; "
				"take it off them (tickets.py set <id> --tags ...) or choose another name".format(len(carrying[args.new]),
				args.new, tag_id, listed(carrying[args.new])))
		raise refused("rename refused, nothing written", problems, {"warnings": warnings})
	store["tags"][str(tag_id)]["name"] = args.new
	tickets.write_tag_store(store)
	# Renaming changes only the store; config.json is the project's to edit.
	groups = tickets.config.get("tag_groups")
	for group in groups if isinstance(groups, list) else []:
		if isinstance(group, dict) and isinstance(group.get("tags"), list) and old_name in group["tags"]:
			warnings.append("config.json tag_groups: group {} still names '{}'; change it to '{}' (until then every "
				"writer refuses: unknown tag)".format(tickets.shown(group.get("name")), old_name, args.new))
	emit(args, {"id": tag_id, "old": old_name, "name": args.new, "warnings": warnings},
		["renamed tag {} from {} to {}".format(tag_id, old_name, args.new)] + ["WARNING " + warning for warning in warnings])
	return 0


def command_tags(args):
	if args.dry_run and not args.probe:
		raise refused("--dry-run goes with tags --probe, --convert or --convert-all")
	if args.tags_command in ("add", "rename") or (args.probe and not args.dry_run):
		# Each reads tags.json, changes it and writes it back: under the lock, so that no other writer's change is lost.
		with tickets.tag_store_lock():
			return tags_subcommand(args)
	return tags_subcommand(args)


def tags_subcommand(args):
	if args.tags_command == "add":
		return command_tags_add(args)
	if args.tags_command == "rename":
		return command_tags_rename(args)
	if args.probe:
		return command_tags_probe(args)
	return command_tags_list(args)


# --- rules: the tag groups -----------------------------------------------------------------------

def command_rules(args):
	refuse_dry_run(args)
	try:
		store = tickets.load_tag_store()
	except (OSError, ValueError) as error:
		raise refused("tags.json cannot be read ({})".format(text(error)))
	groups = tickets.load_tag_groups(store)
	rows = [{"name": group["name"], "tags": group["tags"], "ids": group["ids"], "min": group["min"], "max": group["max"],
		"rule": tickets.group_rule(group)} for group in groups]
	lines = ["{}: {}".format(row["name"], row["rule"]) for row in rows] or ["no tag groups (config.json has no tag_groups)"]
	emit(args, {"groups": rows}, lines)
	return 0


# --- draft and new -------------------------------------------------------------------------------

def read_record(path):
	"""--from-json: one JSON object, UTF-8 (a BOM is accepted), from a file or - for stdin."""
	try:
		if path == "-":
			data = sys.stdin.buffer.read().decode("utf-8-sig")
		else:
			with open(path, "r", encoding="utf-8-sig") as handle:
				data = handle.read()
		record = json.loads(data, object_pairs_hook=tickets.unique_keys, parse_constant=tickets.reject_constant)
	except OSError as error:
		raise refused("cannot read --from-json {} ({})".format(text(path), text(error.strerror or error)))
	except (ValueError, RecursionError) as error:
		raise refused("--from-json {} is not valid UTF-8 JSON ({})".format(text(path), text(error)))
	if not isinstance(record, dict):
		raise refused("--from-json {} must hold one JSON object, {{...}}".format(text(path)))
	return record


def ticket_record(args):
	"""The input as an import record (tickets.import_ticket): from --from-json, or from the flags."""
	flags = {"summary": args.summary, "severity": args.severity, "owner": args.owner, "author": args.author,
		"status": args.status, "target": args.target}
	given = [name for name, value in flags.items() if value is not None]
	given += [name for name in ("tags", "body_file", "link_ticket", "link_file", "link_commit", "block", "blocked_by",
		"relates") if getattr(args, name)]
	if args.from_json is not None:
		if given:
			raise refused("--from-json takes no ticket flags; put them in the JSON ({})".format(", ".join(given)))
		record = read_record(args.from_json)
		set_by_new = [key for key in ("id", "created", "updated") if key in record]
		if set_by_new:
			raise refused("{} set by new, not by the input; tickets.py import takes dated records".format(
				", ".join(set_by_new)))
		if "comments" in record:
			raise refused("comments are not taken by draft or new: create the ticket, then add each comment with "
				"author.py comment <id> --author <who> --body-file <file> (or tickets.py comment)")
		return record
	record = {name: value for name, value in flags.items() if value is not None}
	if args.tags is not None:
		record["tags"] = tickets.split_list(args.tags)
	if args.body_file:
		record["body"] = tickets.read_body(args.body_file)
	record["links"] = {"tickets": args.link_ticket or [], "files": args.link_file or [], "commits": args.link_commit or []}
	# The typed links, only when given (a ticket without them reads as before); ids as tickets.py takes them.
	for kind, add, other in tickets.typed_link_changes(args):
		tickets.add_unique(record["links"].setdefault(kind, []), [other])
	return record


def built_ticket(args):
	"""(ticket, store): the ticket `new` would write now, or a refusal naming every problem."""
	refuse_dry_run(args)
	record = ticket_record(args)
	store = tickets.writable_tag_store()
	groups = tickets.load_tag_groups(store)
	schema = tickets.read_schema()
	existing, _ = tickets.load_all()
	ticket_id = tickets.next_id()
	known_ids = {item.get("id") for item in existing if isinstance(item, dict)} | {ticket_id}
	ticket, problems = tickets.import_ticket(record, ticket_id, tickets.utc_now(), schema, store, groups, known_ids)
	# import takes a done ticket without a commit (old records); filing one does not (B62).
	done_problem = tickets.done_commit_problem(ticket, "add --link-commit <hash>, or a hash in links.commits with "
		"--from-json") if isinstance(ticket, dict) else None
	if done_problem:
		problems = list(problems) + [("status", done_problem)]
	if not problems:
		# The tickets its blocks name get the other side when new writes it: they must exist, be usable and stay valid
		# with it (the new id counted as known), or new writes nothing.
		try:
			for other, _ in tickets.other_sides(ticket_id, tickets.new_block_changes(ticket)):
				problems += [("links", "the other side of its blocks: " + error)
					for error in tickets.write_problems(other, [ticket_id])]
		except tickets.ticket_error as error:
			problems = [("links", str(error))]
	if problems:
		shown = ["{}: {}".format(tickets.shown_key(field), problem) for field, problem in problems]
		groups_missing = [problem for field, problem in problems if field == "tags" and problem.startswith("tag group ")]
		raise refused("the ticket is invalid: {} problem(s); nothing written".format(len(problems)), shown,
			{"groups": groups_missing})
	return ticket, store


def tag_names(store, ticket):
	return [tickets.tag_label(store, tag) for tag in ticket["tags"]]


def command_draft(args):
	ticket, store = built_ticket(args)
	ordered = tickets.ordered(ticket)
	lines = ["OK: valid; new would write it as {} (or the next free id, which new prints)".format(ticket["id"]),
		"tags: " + (", ".join(tag_names(store, ticket)) or "-")] + json.dumps(ordered, indent=2).splitlines()
	emit(args, {"id": ticket["id"], "tag_names": tag_names(store, ticket), "ticket": ordered}, lines)
	return 0


def command_new(args):
	ticket, store = built_ticket(args)
	# The ticket and the other side of each of its blocks are checked, then written together or not at all.
	written = tickets.create_ticket(ticket)
	emit(args, {"id": ticket["id"], "tag_names": tag_names(store, ticket), "ticket": tickets.ordered(ticket),
		"other_sides": written}, ["created {} - {}".format(ticket["id"], text(ticket["summary"]))]
		+ (["updated {} (the other side of its blocks)".format(", ".join(written))] if written else []))
	return 0


def command_comment(args):
	refuse_dry_run(args)
	ticket = tickets.add_comment(tickets.normalize_id(args.id), args.author, tickets.read_body(args.body_file))
	emit(args, {"id": ticket["id"], "comment": ticket["comments"][-1], "comments": len(ticket["comments"])},
		["commented on {} ({} comment(s))".format(ticket["id"], len(ticket["comments"]))])
	return 0


def refuse_dry_run(args):
	if args.dry_run:
		raise refused("--dry-run goes with tags --probe, --convert or --convert-all")


# --- --convert and --convert-all: tag names to ids ------------------------------------------------

def converted_tags(store, values, by_name):
	"""A ticket's tags with every name as its id: (new list, problems). Ids are left alone, a digit string becomes the
	id it names, and a tag given twice (name and id) is kept once, where it first stood."""
	result = []
	problems = []
	for value in values:
		if tickets.is_tag_id(value):
			tag_id = value
		elif is_id_text(value):
			tag_id = tickets.stored_tag_id(store, value, by_name)
			if tag_id is None:
				problems.append("unknown tag id '{}' (not in tags.json)".format(text(value)))
		elif isinstance(value, str):
			tag_id = by_name.get(value)
			if tag_id is None:
				problems.append("unknown tag '{}' (run author.py tags --probe first)".format(text(value)))
		else:
			tag_id = None
			problems.append("tag {} is neither a tag id nor a tag name".format(tickets.shown(value)))
		if tag_id is not None and tag_id not in result:
			result.append(tag_id)
	return result, problems


def conversion(store, by_name, name, ticket):
	"""(new tags or None when the file needs no change, problems) for one ticket file."""
	if not isinstance(ticket, dict) or not isinstance(ticket.get("tags"), list):
		return None, ["{}: has no tags list; run validate".format(name)]
	if not isinstance(ticket.get("id"), str) or ticket["id"] + ".json" != name:
		return None, ["{}: the file name does not match its id; run validate".format(name)]
	new, problems = converted_tags(store, ticket["tags"], by_name)
	if problems:
		return None, ["{}: {}".format(name, problem) for problem in problems]
	return (new if new != ticket["tags"] else None), []


def ticket_name(path):
	"""The store's ticket file that --convert names: a path to it, or its id."""
	if os.path.isfile(path):
		full = os.path.abspath(path)
		name = os.path.basename(full)
		if (os.path.normcase(os.path.dirname(full)) != os.path.normcase(os.path.abspath(tickets.tickets_dir))
				or not tickets.ticket_file_pattern.fullmatch(name)):
			raise refused("{} is not a ticket file of this store ({})".format(text(path), text(tickets.tickets_dir)))
		return name
	try:
		ticket_id = tickets.normalize_id(path)
	except tickets.ticket_error:
		raise refused("{}: no such file, and not a ticket id".format(text(path)))
	if not os.path.isfile(tickets.ticket_path(ticket_id)):
		raise refused("no such ticket: " + ticket_id)
	return ticket_id + ".json"


def command_convert(args, names):
	"""Convert the named ticket files. A real run holds the tag store lock throughout, so tags.json cannot change (a
	rename, a probe) between reading the store and writing the last ticket."""
	if args.dry_run:
		return convert_files(args, names)
	with tickets.tag_store_lock():
		return convert_files(args, names)


def convert_files(args, names):
	"""Convert the named ticket files: every one is checked first, and nothing is written while any refuses."""
	store = tickets.writable_tag_store()
	by_name = tickets.tag_ids_by_name(store)
	plan = []
	problems = []
	for name in names:
		try:
			ticket = read_ticket_file(name)
		except (OSError, ValueError, RecursionError) as error:
			problems.append("{}: unreadable JSON ({})".format(name, text(error)))
			continue
		new, found = conversion(store, by_name, name, ticket)
		problems += found
		if new is not None:
			plan.append((name, ticket["tags"], new))
	if problems:
		raise refused("nothing converted: {} problem(s)".format(len(problems)), problems)
	changes = [{"file": name, "from": old, "to": new} for name, old, new in plan]
	describe = lambda change: "{}: {} -> {}".format(change["file"], ", ".join(text(value) for value in change["from"]),
		", ".join(str(value) for value in change["to"]))
	values = sum(1 for change in changes for value in change["from"] if not tickets.is_tag_id(value))
	if args.dry_run:
		lines = [describe(change) for change in changes]
		lines.append("dry run: would convert {} of {} ticket(s) ({} tag value(s)); nothing written".format(len(changes),
			len(names), values))
		emit(args, {"dry_run": True, "converted": changes, "tickets": len(names), "values": values}, lines)
		return 0
	done = []
	for name, _, _ in plan:
		# Read again just before writing: a `set` meanwhile may have changed the ticket, or converted it.
		try:
			ticket = read_ticket_file(name)
			new, found = conversion(store, by_name, name, ticket)
			if found:
				raise tickets.ticket_error("; ".join(found))
			if new is not None:
				ticket["tags"] = new
				tickets.write_ticket(ticket)
				done.append(name)
		except (OSError, ValueError, tickets.ticket_error) as error:
			raise refused("stopped after converting {} of {} ticket(s); the rest are unchanged: run it again".format(
				len(done), len(plan)), [text(error)], {"converted_files": done})
	lines = [describe(change) for change in changes] if len(names) == 1 else []
	lines.append("converted {} of {} ticket(s) ({} tag value(s)); {} needed no change".format(len(done), len(names),
		values, len(names) - len(plan)))
	emit(args, {"dry_run": False, "converted": changes, "tickets": len(names), "values": values}, lines)
	return 0


# --- the command line ----------------------------------------------------------------------------

def build_parser():
	schema = tickets.read_schema()
	enum_of = lambda field: ", ".join(tickets.schema_enum(schema, field))
	parser = argument_parser(prog="author.py", description="Authoring tool for agents, {} ticket store: tags, tag "
		"groups, drafts, and the move to numbered tags. Writes only through tickets.py.".format(
		text(tickets.config["store_name"])))
	parser.add_argument("--json", action="store_true", help="print one JSON object on stdout")
	parser.add_argument("--convert", metavar="PATH", help="rewrite one ticket file's tag names as ids (a path or an id)")
	parser.add_argument("--convert-all", action="store_true", help="rewrite every ticket's tag names as ids")
	parser.add_argument("--dry-run", action="store_true", help="with --convert, --convert-all or tags --probe: show, "
		"write nothing")
	commands = parser.add_subparsers(dest="command", parser_class=argument_parser)

	def add_json(sub):
		# SUPPRESS: given before the command, the top-level value stays.
		sub.add_argument("--json", action="store_true", default=argparse.SUPPRESS, help="print one JSON object on stdout")

	tags = commands.add_parser("tags", help="list the tag store, with usage counts")
	tags.add_argument("--unused", action="store_true", help="only tags no ticket carries")
	tags.add_argument("--group", help="only the tags of this tag group")
	tags.add_argument("--probe", action="store_true", help="number an old-format tags.json and add the tag names "
		"found on tickets that it lacks")
	tags.add_argument("--dry-run", action="store_true", default=argparse.SUPPRESS, help="with --probe: show, write nothing")
	add_json(tags)
	tags_commands = tags.add_subparsers(dest="tags_command", parser_class=argument_parser)
	adding = tags_commands.add_parser("add", help="add a tag with the next id")
	adding.add_argument("name")
	adding.add_argument("--color", help="#rrggbb (default {})".format(tickets.default_tag_color))
	adding.add_argument("--description", help="one line")
	add_json(adding)
	renaming = tags_commands.add_parser("rename", help="rename a tag, keeping its id")
	renaming.add_argument("old", help="the tag's name or id")
	renaming.add_argument("new", help="the new name")
	add_json(renaming)
	tags.set_defaults(handler=command_tags)

	rules = commands.add_parser("rules", help="print the tag groups (config.json tag_groups)")
	add_json(rules)
	rules.set_defaults(handler=command_rules)

	def add_ticket_input(sub):
		sub.add_argument("--summary")
		sub.add_argument("--severity", help=enum_of("severity"))
		sub.add_argument("--owner", help=enum_of("owner_area"))
		sub.add_argument("--author", help=enum_of("author"))
		sub.add_argument("--status", help="{} (default open)".format(enum_of("status")))
		sub.add_argument("--target", help="default {}".format(tickets.default_target_patch))
		sub.add_argument("--tags", help="comma-separated tag names or ids")
		sub.add_argument("--body-file", help="markdown file for the body, or - for stdin")
		sub.add_argument("--link-ticket", action="append", help="another ticket id (repeatable)")
		sub.add_argument("--link-file", action="append", help="repo path or path:line (repeatable)")
		sub.add_argument("--link-commit", action="append", help="commit hash (repeatable)")
		sub.add_argument("--block", action="append", help="a ticket this one blocks (repeatable); new writes the other side")
		sub.add_argument("--blocked-by", action="append", help="a ticket that blocks this one (repeatable)")
		sub.add_argument("--relates", action="append", help="a related ticket (repeatable)")
		sub.add_argument("--from-json", metavar="FILE", help="one JSON object with an import record's keys (summary, "
			"severity, owner, author, status, target, tags, body, links {tickets, files, commits, blocks, blocked_by, "
			"relates_to}; no comments: add them with comment afterwards), or - for stdin; instead of the flags")
		add_json(sub)

	draft = commands.add_parser("draft", help="print the ticket new would write, or every problem")
	add_ticket_input(draft)
	draft.set_defaults(handler=command_draft)
	new = commands.add_parser("new", help="write a ticket with the next id, as tickets.py new does")
	add_ticket_input(new)
	new.set_defaults(handler=command_new)
	comment = commands.add_parser("comment", help="append a comment to a ticket, as tickets.py comment does")
	comment.add_argument("id")
	comment.add_argument("--author", required=True, help=enum_of("author"))
	comment.add_argument("--body-file", required=True, help="markdown file for the comment, or - for stdin")
	add_json(comment)
	comment.set_defaults(handler=command_comment)
	return parser


def main():
	try:
		args = build_parser().parse_args()
	except tickets.ticket_error as error:
		# build_parser reads the schema; --json is read from argv, as the usage errors do.
		return fail(json_wanted(), str(error))
	try:
		if args.convert is not None and args.convert_all:
			raise refused("give --convert <path> or --convert-all, not both")
		if args.convert is not None or args.convert_all:
			if args.command:
				raise refused("--convert and --convert-all take no command")
			return command_convert(args, [ticket_name(args.convert)] if args.convert is not None else tickets.ticket_files())
		if not args.command:
			raise refused("give a command (tags, rules, draft, new, comment) or --convert <path> / --convert-all; -h for help")
		return args.handler(args)
	except refused as error:
		return fail(args.json, error.summary, error.problems, error.data)
	except tickets.ticket_error as error:
		lines = [line.strip() for line in str(error).splitlines() if line.strip()]
		if len(lines) > 1:
			return fail(args.json, lines[0].rstrip(":"), lines[1:])
		return fail(args.json, str(error))


if __name__ == "__main__":
	sys.exit(main())
