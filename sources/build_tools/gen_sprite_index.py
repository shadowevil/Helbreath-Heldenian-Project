"""Regenerate a sprite tree's index.json from its extracted sprite folders.

Usage:
	python gen_sprite_index.py [--check] [<sprite root> ...]

The sprite root defaults to staged/client/sprites (relative to the repo root). Every folder below it
holding a sprite manifest.json (its "tool" field is TOOL_ID) is one asset, keyed by its path below the
root ("npcs/ogre"); the client looks names up in index.json and never scans folders. Folders inside an
asset are not searched.

The output keeps the existing index format byte for byte (sorted keys, indent 1, LF, trailing
newline), so regenerating an unchanged tree makes no diff. --check writes nothing and exits 1 when an
index is missing or stale. Exit 2 on a malformed manifest.

Not wired into deploy; it would run before deploy.py copies staged/client/sprites (or as a --check step
in the build scripts) so an edited or added sprite folder is indexed.
"""

import argparse
import json
import os
import sys

FORMAT_VERSION = 1
TOOL_ID = "pak_extract"  # the "tool" value every sprite manifest and index.json carries (data, kept as is)
DEFAULT_ROOT = "staged/client/sprites"


def repo_root():
	return os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def read_manifest(path):
	"""The manifest as a dict when it is a sprite manifest (tool TOOL_ID), else None."""
	with open(path, "r", encoding="utf-8") as f:
		manifest = json.load(f)
	if not isinstance(manifest, dict) or manifest.get("tool") != TOOL_ID:
		return None
	return manifest


def build_index(root):
	assets = {}
	for dirpath, dirnames, filenames in os.walk(root):
		dirnames[:] = sorted(d for d in dirnames if not os.path.islink(os.path.join(dirpath, d)))
		if "manifest.json" not in filenames or dirpath == root:
			continue
		manifest = read_manifest(os.path.join(dirpath, "manifest.json"))
		if manifest is None:
			continue
		dirnames[:] = []  # an asset's own subfolders (unknown/) are not assets
		key = os.path.relpath(dirpath, root).replace(os.sep, "/")
		count = manifest.get("sprite_count")
		sprites = manifest.get("sprites")
		if not isinstance(count, int) or not isinstance(sprites, list) or count != len(sprites):
			raise ValueError("%s: sprite_count does not match the sprite list" % key)
		assets[key] = {"dir": key, "sprite_count": count, "manifest": key + "/manifest.json"}
	return assets


def render(assets):
	text = json.dumps({"format_version": FORMAT_VERSION, "tool": TOOL_ID, "assets": assets},
		indent=1, sort_keys=True)
	return (text + "\n").encode("utf-8")


def main():
	parser = argparse.ArgumentParser(description="Regenerate sprite index.json from sprite folder manifests.")
	parser.add_argument("roots", nargs="*", help="sprite roots (default: %s)" % DEFAULT_ROOT)
	parser.add_argument("--check", action="store_true", help="write nothing; exit 1 when an index is stale")
	args = parser.parse_args()

	stale = False
	for raw in args.roots or [DEFAULT_ROOT]:
		root = raw if os.path.isabs(raw) else os.path.join(repo_root(), raw)
		if not os.path.isdir(root):
			print("error: %s is not a folder" % raw)
			return 2
		try:
			assets = build_index(root)
		except (OSError, ValueError) as ex:
			print("error: %s: %s" % (raw, ex))
			return 2
		wanted = render(assets)
		index_path = os.path.join(root, "index.json")
		try:
			with open(index_path, "rb") as f:
				current = f.read()
		except FileNotFoundError:
			current = None

		if current == wanted:
			print("ok: %s/index.json is current (%d assets)" % (raw, len(assets)))
		elif args.check:
			print("stale: %s/index.json differs from its folders (%d assets)" % (raw, len(assets)))
			stale = True
		else:
			tmp = index_path + ".tmp"
			with open(tmp, "wb") as f:
				f.write(wanted)
			os.replace(tmp, index_path)
			print("wrote: %s/index.json (%d assets)" % (raw, len(assets)))
	return 1 if stale else 0


if __name__ == "__main__":
	sys.exit(main())
