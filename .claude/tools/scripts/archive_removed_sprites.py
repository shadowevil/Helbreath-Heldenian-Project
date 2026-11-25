"""Copy sprite files a commit removed from staged/client/sprites into working/archive/, byte-identical, from git.

  python .claude/tools/scripts/archive_removed_sprites.py <commit>            dry run: lists what would be written
  python .claude/tools/scripts/archive_removed_sprites.py <commit> --apply    writes the files

Deleted files are taken from <commit>^. A manifest.json the commit changed (not deleted) is archived as
manifest_before.json beside the archived images. Read-only git (git diff-tree, git show). ASCII output only.
"""
import os
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
PREFIX = "staged/client/sprites/"
ARCHIVE = os.path.join(ROOT, "working", "archive")


def git(*args):
	return subprocess.run(["git", "-C", ROOT, *args], check=True, capture_output=True).stdout


def main():
	if len(sys.argv) < 2:
		print(__doc__)
		return 2
	commit, apply = sys.argv[1], "--apply" in sys.argv
	out = git("diff-tree", "--no-commit-id", "-r", "--name-status", commit).decode("utf-8").splitlines()
	plan = []
	for line in out:
		status, path = line.split("\t", 1)
		if not path.startswith(PREFIX):
			continue
		rel = path[len(PREFIX):]
		if status == "D":
			plan.append((path, rel))
		elif status == "M" and rel.endswith("manifest.json"):
			plan.append((path, rel[:-len("manifest.json")] + "manifest_before.json"))
	for src, rel in plan:
		dest = os.path.join(ARCHIVE, *rel.split("/"))
		print(("write " if apply else "would write ") + "working/archive/" + rel)
		if apply:
			data = git("show", "%s^:%s" % (commit, src))
			os.makedirs(os.path.dirname(dest), exist_ok=True)
			with open(dest, "wb") as f:
				f.write(data)
	print("%d file(s)%s" % (len(plan), "" if apply else " (dry run)"))
	return 0


if __name__ == "__main__":
	sys.exit(main())
