"""Merging a release's ticket schema with a project's copy of it (B65).

The shipped ticket.schema.json lists, in its top-level "x-project-owned", the JSON pointers whose values belong to
the project: the ticket id patterns and the status, severity, owner_area and author enums. A project narrows or
extends those, so an update keeps them. Everything else in the schema is the release's.

    merge_schema(release, project, base) -> (merged, problems)

  release  the new release's schema (a parsed dict);
  project  the project's schema now;
  base     the schema of the release the project runs now (its export).

merged is the release schema with the project's value copied in at each owned pointer, or None when problems is not
empty. A problem is a line of text. The project differing from the base anywhere outside the owned pointers (a
local edit of something the release owns) is one: nothing is guessed, so the caller stops before it changes a file. The one exception is wording:
a title, description, $comment or examples text the project changed is kept as the project's.

From the command line (the manual upgrade to 1.5.0; the viewer's one-click update calls merge_schema itself):

    python <CLI_DIR>/schema_merge.py <release schema> <project schema> --base <base schema> --out <file>

writes the merged schema to --out (a new file, never an input) and exits 0, or prints each problem and exits 2 with
nothing written; 1 on an error (a file that does not read).

Stdlib only; ASCII console output.
"""

import argparse
import copy
import json
import os
import sys

owned_key = "x-project-owned"
# Keywords whose text is wording, not structure: a project may reword them, and that is kept.
text_keys = ("title", "description", "$comment", "examples")
# A key under one of these names a schema, a property or a requirement, never wording.
container_keys = ("properties", "$defs", "definitions", "patternProperties", "dependentSchemas", "dependentRequired")


def split_pointer(pointer):
	"""The reference tokens of a JSON pointer ("/a/b~1c" -> ["a", "b/c"]), or None when it is not one."""
	if not isinstance(pointer, str) or not pointer.startswith("/"):
		return None
	return [token.replace("~1", "/").replace("~0", "~") for token in pointer[1:].split("/")]


def child(node, token):
	"""(found, value) for one token under a dict or list node."""
	if isinstance(node, dict):
		if token in node:
			return True, node[token]
	elif isinstance(node, list) and token.isdigit() and int(token) < len(node):
		return True, node[int(token)]
	return False, None


def resolve(document, pointer):
	"""(found, value): what the JSON pointer names in the document."""
	tokens = split_pointer(pointer)
	if tokens is None:
		return False, None
	node = document
	for token in tokens:
		found, node = child(node, token)
		if not found:
			return False, None
	return True, node


def parent_of(document, tokens):
	"""(found, node) for the node holding the last of the tokens."""
	node = document
	for token in tokens[:-1]:
		found, node = child(node, token)
		if not found:
			return False, None
	return True, node


def assign(document, pointer, value):
	"""Set the value at the pointer; False when its parent is not there."""
	tokens = split_pointer(pointer)
	if not tokens:
		return False
	found, parent = parent_of(document, tokens)
	if not found:
		return False
	last = tokens[-1]
	if isinstance(parent, dict):
		parent[last] = copy.deepcopy(value)
		return True
	if isinstance(parent, list) and last.isdigit() and int(last) < len(parent):
		parent[int(last)] = copy.deepcopy(value)
		return True
	return False


def remove(document, pointer):
	"""Delete what the pointer names, when it is there."""
	tokens = split_pointer(pointer)
	if not tokens:
		return
	found, parent = parent_of(document, tokens)
	if found and isinstance(parent, dict):
		parent.pop(tokens[-1], None)


def escape(token):
	return token.replace("~", "~0").replace("/", "~1")


def differences(first, second, path=""):
	"""The pointers where two parsed JSON values differ."""
	if isinstance(first, dict) and isinstance(second, dict):
		found = []
		for key in sorted(set(first) | set(second)):
			where = path + "/" + escape(str(key))
			if key not in first or key not in second:
				found.append(where)
			else:
				found += differences(first[key], second[key], where)
		return found
	if isinstance(first, list) and isinstance(second, list) and len(first) == len(second):
		found = []
		for index in range(len(first)):
			found += differences(first[index], second[index], "{}/{}".format(path, index))
		return found
	return [] if first == second and type(first) is type(second) else [path or "/"]


def wording_pointer(pointer):
	"""The pointer of the wording keyword a changed pointer lies in, or None when it is structure. A wording keyword is
	title, description or $comment (the last token), or examples (the last token or inside its array), whose parent
	token is not a container of schemas or names (properties, $defs, ...): "/$defs/title" is a definition named title."""
	tokens = split_pointer(pointer) or []
	for index, token in enumerate(tokens):
		if token not in text_keys or (index > 0 and tokens[index - 1] in container_keys):
			continue
		if index == len(tokens) - 1 or token == "examples":
			return "/" + "/".join(escape(item) for item in tokens[:index + 1])
		return None
	return None


def wording_types_ok(project, release, pointer):
	"""A wording value is text (a list for examples) in the project, and in the release when it has one there."""
	want = list if pointer.rsplit("/", 1)[-1] == "examples" else str
	for document in (project, release):
		found, value = resolve(document, pointer)
		if found and not isinstance(value, want):
			return False
	return True


def owned_pointers(schema):
	"""The schema's x-project-owned list, or None when it has none (or it is not a list of text)."""
	value = schema.get(owned_key) if isinstance(schema, dict) else None
	if isinstance(value, list) and all(isinstance(item, str) for item in value):
		return value
	return None


def masked(schema, pointers):
	"""A copy of the schema without the owned values and without the x-project-owned list itself."""
	result = copy.deepcopy(schema)
	result.pop(owned_key, None)
	for pointer in pointers:
		remove(result, pointer)
	return result


def merge_schema(release, project, base):
	"""(merged, problems); see the module text."""
	problems = []
	for label, schema in (("release", release), ("project", project), ("base", base)):
		if not isinstance(schema, dict):
			problems.append("the {} schema is not a JSON object".format(label))
	if problems:
		return None, problems
	pointers = owned_pointers(release)
	if pointers is None:
		return None, ["the release schema has no \"{}\" list".format(owned_key)]
	for pointer in pointers:
		if not resolve(release, pointer)[0]:
			problems.append("the release schema has nothing at its owned pointer {}".format(pointer))
	if problems:
		return None, problems
	# The base may predate x-project-owned. Its own pointers are masked too, so a value the release no longer owns is
	# reported by name below, not as a stray difference.
	base_owned = owned_pointers(base) or []
	dropped = [pointer for pointer in base_owned if pointer not in pointers]
	for pointer in dropped:
		if resolve(project, pointer) != resolve(base, pointer):
			problems.append("the project changed {}, which the release no longer treats as project-owned (the base "
				"release did); resolve it by hand".format(pointer))
	masking = pointers + dropped
	changed = differences(masked(base, masking), masked(project, masking))
	texts = []
	for where in changed:
		target = wording_pointer(where)
		if target is not None and wording_types_ok(project, release, target):
			if target not in texts:
				texts.append(target)
		else:
			problems.append("the project's schema differs from the base release's outside the owned values, at {}".format(
				where))
	if problems:
		return None, problems
	merged = copy.deepcopy(release)
	for pointer in pointers:
		found, value = resolve(project, pointer)
		if found:
			assign(merged, pointer, value)
		else:
			# The project has no value here (it deleted it): the release's is not put back.
			remove(merged, pointer)
	# Wording a project edited (a title, a description) is kept; the release's wording stays where the project's is the base's.
	for where in texts:
		found, value = resolve(project, where)
		if found:
			assign(merged, where, value)
	return merged, []


def ascii_text(text):
	return str(text).encode("ascii", "replace").decode("ascii")


def main():
	parser = argparse.ArgumentParser(description="Merge a release's ticket schema with the project's (x-project-owned).",
		allow_abbrev=False)
	parser.add_argument("release", help="the new release's ticket.schema.json (from its export)")
	parser.add_argument("project", help="the project's .claude/tickets/ticket.schema.json")
	parser.add_argument("--base", required=True, help="the schema of the release the project runs now (its export)")
	parser.add_argument("--out", required=True, help="where to write the merged schema: a new file, never an input")
	args = parser.parse_args()
	out = os.path.normcase(os.path.abspath(args.out))
	if out in [os.path.normcase(os.path.abspath(path)) for path in (args.release, args.project, args.base)]:
		print("ERROR --out names an input; write the merged schema elsewhere, review it, then copy it over")
		return 1
	documents = []
	for path in (args.release, args.project, args.base):
		try:
			with open(path, "r", encoding="utf-8-sig") as handle:
				documents.append(json.load(handle))
		except (OSError, ValueError) as error:
			print("ERROR cannot read {}: {}".format(ascii_text(path), ascii_text(error)))
			return 1
	merged, problems = merge_schema(*documents)
	if problems:
		for problem in problems:
			print("PROBLEM " + ascii_text(problem))
		print("exit 2: nothing written. Bring those values back to the base release's (or tell the maintainer why they "
			"differ), then run it again.")
		return 2
	with open(args.out, "w", encoding="utf-8", newline="\n") as handle:
		json.dump(merged, handle, indent=2, ensure_ascii=False)
		handle.write("\n")
	print("wrote {}: the release's schema with your values at {} owned pointer(s)".format(ascii_text(args.out),
		len(owned_pointers(merged) or [])))
	return 0


if __name__ == "__main__":
	sys.exit(main())
