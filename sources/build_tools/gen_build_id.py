"""gen_build_id.py - derives the Build ID for this build and writes it into a generated header.

Ported from Helbreath-SDL (its .claude/docs/versioning/build_id.md). The Build ID identifies the SOURCE
STATE a build was produced from; it is never a hand-incremented counter and is never written into tracked
source. Each project runs this before compiling and writes build_id_generated.h into its own untracked
build folder (Windows: sources/<Project>/build/<Configuration>/<channel>/generated/, git-ignored by
**/build/; Linux: <CMake binary dir>/generated/<channel>/). Only shared/version.cpp includes it.

    python gen_build_id.py --environment DEV  --configuration Debug   --output <path>
    python gen_build_id.py --environment TEST --configuration Release --output <path> --fresh-source
    python gen_build_id.py --environment TEST --configuration Release --fresh-source --check-only

Derivation:
  clean source state -> first 8 hex characters of HEAD, read as a uint32, split High/Low
  dirty DEV tree     -> SHA-256 over HEAD plus the status, path and git blob id of every relevant
                        changed or untracked file, first 8 hex characters, split the same way

"Relevant" is narrower than the whole tree: only sources/ produces the binaries, and inside it the build
products (object and library folders, exe folders, logs, backups, the old version system's outputs) are
skipped, so building never changes the id of the tree it built. Paths are sorted ordinally and each file
is hashed as git would store it (`git hash-object`, which applies the .gitattributes line-ending
normalisation), so a Windows and a Linux checkout of the same state produce the same id.

Environment rules (they must never leak into one another):
  DEV  - may build from a dirty working tree; any configuration.
  TEST - Release configuration; source freshly obtained from Git (--fresh-source); whole tree clean.
  PROD - same as TEST.

The whole-tree check: build.ps1 runs it ONCE with --check-only before anything writes (MSBuild
modifies tracked files; build_release.ps1 runs a check too, but only to
refuse before it rolls a seed). It prints `verified head: <sha>`, and build.ps1 passes --clean-verified
<sha> to every project's run so the whole-tree check is not repeated against the build's own outputs. Run
without --clean-verified (a bare MSBuild or CMake build), the whole-tree check happens here, minus
KNOWN_TRACKED_ARTIFACTS.

--clean-verified never makes a dirty TEST/PROD build possible: the sha must equal HEAD at compile time,
and the source check still runs and refuses any change. For TEST/PROD that check is stricter than DEV's
relevance filter: EVERY tracked change under sources/ counts (tracked leftover
objects are linked in), except exactly KNOWN_TRACKED_ARTIFACTS; IGNORED_PATTERNS
apply only to untracked entries. No legitimate TEST/PROD build trips it.

Runs only read-only git (rev-parse, status, hash-object without -w), with --no-optional-locks so a build
never takes the index lock. Prints ASCII only.
Exit code 1 (message on stderr) when a gate fails or git cannot be read.
"""

import argparse
import hashlib
import re
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]

# The tree whose state produces the binaries.
RELEVANT_PREFIX = "sources/"

# Build products and bookkeeping inside sources/ that a build writes; never part of the source state.
IGNORED_PATTERNS = (
	re.compile(r"^sources/(Debug|Release|ReleaseDbg)/"),           # exe output folders
	re.compile(r"^sources/[^/]+/[^/]*_x64/"),                      # per-project IntDir (objs, libs)
	re.compile(r"/build(_linux)?/"),                                # generated headers, CMake build dirs
	re.compile(r"\.bak_[^/]*$"),                                    # bak.py checkpoints
	re.compile(r"\.log$"),                                          # build logs
)

# Build outputs that are still TRACKED in git, so a build modifies them and the TEST/PROD whole-tree check
# would refuse its own output. Exactly these paths are excused. Empty since 2026-11-23: the old *_x64 libs
# were untracked and all build output now goes to the repo-root build/ and bin/.
KNOWN_TRACKED_ARTIFACTS: frozenset[str] = frozenset()

CHANNEL_IDS = {"DEV": 0, "TEST": 1, "PROD": 2}
RELEASE_CONFIGURATION = "Release"

# git status -z field layout: two status letters, a space, then the path
STATUS_FIELD_MIN_LENGTH = 4
STATUS_LENGTH = 2
PATH_OFFSET = 3
HEX_PREFIX_LENGTH = 8
HALF_RANGE = 65536

HEADER_TEMPLATE = """// GENERATED FILE - do not edit and do not commit.
//
// Written by sources/build_tools/gen_build_id.py before every compile and consumed only by
// shared/version.cpp. It lives in an untracked build folder because a generated Build ID must never be
// written into tracked source.

#pragma once

namespace hb::shared::version::generated
{{

// 0 = DEV, 1 = TEST, 2 = PROD.
inline constexpr int channel_id = {channel_id};
inline constexpr char channel_label[] = "{environment}";

// Build ID halves, each 0-65535.
inline constexpr unsigned int build_id_high = {high};
inline constexpr unsigned int build_id_low = {low};

// True when the id was derived from HEAD alone, false when it fingerprints a dirty DEV tree.
inline constexpr bool source_clean = {clean};

// The commit the build started from - authoritative, unlike the 8-hex Build ID prefix.
inline constexpr char commit_sha[] = "{head_sha}";

}} // namespace hb::shared::version::generated
"""


class BuildIdError(Exception):
	pass


def configure_output() -> None:
	"""A stray non-ASCII byte in a git message must not take the build down on a cp1252 console."""
	for stream in (sys.stdout, sys.stderr):
		try:
			stream.reconfigure(errors="replace")
		except (AttributeError, ValueError):
			pass


def run_git(*args, stdin: bytes = None) -> bytes:
	command = ["git", "--no-optional-locks", "-C", str(REPO_ROOT), *args]
	try:
		result = subprocess.run(command, input=stdin, capture_output=True)
	except OSError as error:
		raise BuildIdError(f"git could not be started: {error}") from error
	if result.returncode != 0:
		raise BuildIdError(f"git {args[0]} failed: {result.stderr.decode('utf-8', 'replace').strip()}")
	return result.stdout


def get_head_sha() -> str:
	sha = run_git("rev-parse", "HEAD").decode("ascii", "replace").strip().lower()
	if not re.fullmatch(r"[0-9a-f]{40,64}", sha):
		raise BuildIdError("git rev-parse HEAD gave no commit - a Build ID cannot be derived without one.")
	return sha


def get_working_tree_entries() -> list:
	"""Parses `git status --porcelain=v1 -z`. NUL separation leaves paths unquoted and unescaped, so a path
	with a space or a non-ASCII byte still round-trips. Returns (status, path, origin) tuples."""
	raw = run_git("-c", "core.quotepath=false", "status", "--porcelain=v1", "-z", "--untracked-files=all")
	fields = [f for f in raw.decode("utf-8", "replace").split("\0") if f]
	entries = []
	i = 0
	while i < len(fields):
		field = fields[i]
		i += 1
		if len(field) < STATUS_FIELD_MIN_LENGTH:
			continue
		status = field[:STATUS_LENGTH]
		path = field[PATH_OFFSET:]
		origin = ""
		# a rename or copy carries its source path in the following field; both halves are part of the state
		# either column: a work-tree rename (" R", after git add -N) carries an origin field too
		if ("R" in status or "C" in status) and i < len(fields):
			origin = fields[i]
			i += 1
		entries.append((status, path, origin))
	return entries


def is_relevant(path: str) -> bool:
	"""DEV's view: a change under sources/ that is not a build product."""
	if not path.startswith(RELEVANT_PREFIX):
		return False
	return not any(pattern.search(path) for pattern in IGNORED_PATTERNS)


def is_release_source_change(status: str, path: str) -> bool:
	"""TEST/PROD's view: any TRACKED change under sources/ except the exact known artifacts; an untracked
	entry counts only when it is not a build product."""
	if not path.startswith(RELEVANT_PREFIX):
		return False
	if status == "??":
		return is_relevant(path)
	return path not in KNOWN_TRACKED_ARTIFACTS


def get_content_hashes(paths: list) -> dict:
	"""path -> git blob id, as `git hash-object` computes it after the clean filter and line-ending
	normalisation, so CRLF and LF checkouts of the same content agree. A deleted path maps to 'absent'
	(the status letter already distinguishes it)."""
	present = [p for p in paths if (REPO_ROOT / p).is_file()]
	hashes = {p: "absent" for p in paths}
	if present:
		output = run_git("hash-object", "--stdin-paths", stdin="\n".join(present).encode("utf-8") + b"\n")
		ids = output.decode("ascii", "replace").split()
		if len(ids) != len(present):
			raise BuildIdError(f"git hash-object returned {len(ids)} id(s) for {len(present)} path(s)")
		hashes.update(zip(present, ids))
	return hashes


def split_build_id(hex_prefix: str) -> tuple:
	"""8 hex characters -> uint32 -> two 16-bit halves. Reversible for a clean build:
	(High * 65536) + Low back to hex is the commit prefix `git show` accepts."""
	value = int(hex_prefix, 16)
	return value // HALF_RANGE, value % HALF_RANGE


def check_gates(environment: str, configuration: str, fresh_source: bool, clean_verified: str, head_sha: str,
		entries: list) -> None:
	"""clean_verified: the HEAD sha the pre-build whole-tree check passed at, or '' when it did not run."""
	if environment == "DEV":
		return
	doc = f"environment_{environment.lower()}.md"
	if not fresh_source:
		raise BuildIdError(f"{environment} builds must come from source freshly obtained from Git, never from a "
			f"developer working directory. Re-run with --fresh-source once that is true ({doc}).")
	if configuration != RELEASE_CONFIGURATION:
		raise BuildIdError(f"{environment} builds must use the {RELEASE_CONFIGURATION} configuration, not "
			f"'{configuration}'. Build stopped.")
	if clean_verified:
		if clean_verified.lower() != head_sha:
			raise BuildIdError(f"--clean-verified {clean_verified} is not the full HEAD sha {head_sha}; the tree "
				f"was verified at another commit (or the sha is abbreviated). Build stopped.")
		return
	changes = [e for e in entries if e[1] not in KNOWN_TRACKED_ARTIFACTS]
	if changes:
		shown = ", ".join(e[1] for e in changes[:5])
		raise BuildIdError(f"{environment} builds require a clean working tree; {len(changes)} change(s) present "
			f"(e.g. {shown}). Build stopped.")


def derive_build_id(environment: str, configuration: str, fresh_source: bool, clean_verified: str) -> tuple:
	"""(head_sha, high, low, clean). Raises BuildIdError when a gate fails."""
	head_sha = get_head_sha()
	entries = get_working_tree_entries()
	check_gates(environment, configuration, fresh_source, clean_verified, head_sha, entries)

	relevant = sorted((e for e in entries if is_relevant(e[1]) or (e[2] and is_relevant(e[2]))),
		key=lambda entry: (entry[1], entry[2]))
	clean = not relevant
	# The check nothing skips: a TEST/PROD binary always comes from HEAD's exact source state. Its set is a
	# superset of `relevant`, so passing it also means the Build ID below is HEAD's.
	if environment != "DEV":
		changes = [e for e in entries if is_release_source_change(e[0], e[1])
			or (e[2] and is_release_source_change(e[0], e[2]))]
		if changes:
			shown = ", ".join(e[1] for e in changes[:5])
			raise BuildIdError(f"{environment} builds require unmodified source; {len(changes)} source change(s) "
				f"present (e.g. {shown}). Build stopped.")
	if clean:
		prefix = head_sha[:HEX_PREFIX_LENGTH]
	else:
		# Deterministic fingerprint of HEAD plus the working state: the same state always produces the same
		# lines in the same (ordinal) order, so reverting a change restores the previous Build ID.
		hashes = get_content_hashes([path for _, path, _ in relevant])
		lines = [f"head {head_sha}"]
		lines += [f"{status} {path} {origin} {hashes[path]}" for status, path, origin in relevant]
		prefix = hashlib.sha256("\n".join(lines).encode("utf-8")).hexdigest()[:HEX_PREFIX_LENGTH]

	high, low = split_build_id(prefix)
	return head_sha, high, low, clean


def write_if_changed(output: Path, text: str) -> None:
	"""Rewrites only on a real change, so an unchanged source state does not force a recompile and relink."""
	data = text.encode("utf-8")
	if output.is_file() and output.read_bytes() == data:
		return
	output.parent.mkdir(parents=True, exist_ok=True)
	output.write_bytes(data)


def main() -> int:
	configure_output()
	parser = argparse.ArgumentParser(description="Derive the Build ID and write the generated header.")
	parser.add_argument("--environment", choices=sorted(CHANNEL_IDS), default="DEV")
	parser.add_argument("--configuration", default="Debug")
	parser.add_argument("--output", help="the header to write (not needed with --check-only)")
	parser.add_argument("--fresh-source", action="store_true",
		help="assert the source was freshly obtained from Git (required for TEST/PROD)")
	parser.add_argument("--check-only", action="store_true",
		help="run the environment gates against the tree as it is now, write nothing")
	parser.add_argument("--clean-verified", metavar="HEAD_SHA", default="",
		help="the whole-tree check already passed (--check-only) at this HEAD before the build started")
	args = parser.parse_args()
	if args.check_only and args.output:
		# a stray --check-only must never leave a build compiling against an old header
		parser.error("--check-only writes nothing; it cannot be combined with --output")
	if not args.check_only and not args.output:
		parser.error("--output is required unless --check-only is given")

	try:
		head_sha, high, low, clean = derive_build_id(args.environment, args.configuration, args.fresh_source,
			"" if args.check_only else args.clean_verified)
		state = "clean" if clean else "dirty"
		if args.check_only:
			print(f"source check: {args.environment} passed ({state} source state, head {head_sha[:HEX_PREFIX_LENGTH]})")
			# machine-readable: build.ps1 / build_release.ps1 pass this sha on as --clean-verified
			print(f"verified head: {head_sha}")
			return 0
		write_if_changed(Path(args.output), HEADER_TEMPLATE.format(channel_id=CHANNEL_IDS[args.environment],
			environment=args.environment, high=high, low=low, clean="true" if clean else "false", head_sha=head_sha))
	except (BuildIdError, OSError) as error:
		print(f"gen_build_id: {error}", file=sys.stderr)
		return 1

	print(f"build id: {args.environment}.{high}+{low} ({state} source state, head {head_sha[:HEX_PREFIX_LENGTH]})")
	return 0


if __name__ == "__main__":
	sys.exit(main())
