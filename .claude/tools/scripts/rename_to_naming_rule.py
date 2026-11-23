#!/usr/bin/env python
"""Phase R: rename files and folders to the global naming rule and rewrite every reference to them.

Rule (user, 2026-11-23; ~/.claude/CLAUDE.md "File and folder names"):
  * code files and their folders follow the language convention - C++ and Python snake_case;
  * Markdown docs and their sidecar folders are lowercase-hyphenated;
  * names a tool requires keep their spelling (CMakeLists.txt, README.md, CLAUDE.md, Makefile, __init__.py ...);
  * third-party trees keep their names (they still move when a parent folder is renamed).
Scope (user): project sources and folders (Sources -> sources), tools/, .claude/, and staged/ runtime data.

Modes (Mode 2 bulk script, .claude/docs/workflow.md and .claude/docs/bulk-scripts-dry-run.md):
  --dry-run   Build the rename map and every reference rewrite. Writes the logs, changes nothing.
  --verify    Collisions, case-only renames, duplicate targets, keyword and header conflicts, a virtual-tree
              closure check, unresolved references and unmatched path literals. Writes the log, changes nothing.
  --check     Read-only preflight: tracked entries deleted on disk under moved folders (prints the exact
              `git rm --cached` list), case-variant duplicate index entries, bak.py checkpoints, Visual Studio
              or the game/server running from the repo, .git/index.lock, and a move simulation.
  --apply --i-have-reviewed <run-id>
              Re-plans; refuses unless the plan still hashes to <run-id> (script sha256 included), verify passed
              with 0 open decisions, and the preflight passes. Stages backups and new contents in the run
              folder, then performs every step in a journal: the rewrites (temp file + os.replace) and then the
              moves (git mv for tracked paths, os.rename otherwise; case-only renames in two steps). Any
              failure rolls the whole run back automatically.
  --resume <run-id>    Continue an interrupted apply from journal.jsonl (each step is idempotent).
  --rollback <run-id>  Reverse the moves (newest first) and restore the rewritten files from the backups.

git: `git ls-files` (read-only) everywhere; `git mv` only inside --apply/--resume/--rollback (user ruling,
2026-11-23: working-tree git allowed, no commits). It never commits, pulls, resets or forces.
Logs: .claude/scratch/tools/bulk/rename_to_naming_rule_dry_run.log, ..._verify.log, and the run folder
      .claude/scratch/tools/bulk/rename_to_naming_rule/<run-id>/ (map.tsv, rewrites.tsv, findings.tsv,
      plan.json, decisions_template.json, moves.preview, moves_reverse.preview, verify.json, preflight.log,
      git_rm_cached.txt; after apply: apply_plan.json, backup/, new/, journal.jsonl, applied.json).
Decisions for ambiguous names: .claude/tools/scripts/rename_to_naming_rule.decisions.json (optional):
  {"names": {"SFMLEngine": "sfml_engine"}, "paths": {"Sources/Foo/Bar.h": "bar.h"}, "accept_mixed_style": true}
Python stdlib only. Console output is ASCII; files are read and written as bytes (encoding and line
endings preserved exactly).
"""

import argparse
import bisect
import hashlib
import json
import keyword
import os
import re
import shutil
import subprocess
import sys
import time
from collections import Counter, defaultdict
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
SCRIPT_REL = Path(__file__).resolve().relative_to(REPO).as_posix()
BULK_DIR = REPO / ".claude" / "scratch" / "tools" / "bulk"
RUN_ROOT = BULK_DIR / "rename_to_naming_rule"
DEFAULT_DECISIONS = REPO / ".claude" / "tools" / "scripts" / "rename_to_naming_rule.decisions.json"
DRY_LOG = BULK_DIR / "rename_to_naming_rule_dry_run.log"
VERIFY_LOG = BULK_DIR / "rename_to_naming_rule_verify.log"
REPO_DIR_NAME = REPO.name
ALL_CLASSES = ("code", "docs", "data")

# --------------------------------------------------------------------------------------------------
# Scope tables
# --------------------------------------------------------------------------------------------------

# Never part of the universe (outputs, runtime state, IDE caches, scratch).
SKIP_TOP = {"game", "bin", "build", "working", ".vs", ".git"}
SKIP_PREFIXES = (".claude/scratch/", ".claude/hooks/log/")

# Third-party trees: every segment at or below these keeps its spelling; contents are never rewritten.
THIRD_PARTY = (
	"Sources/Dependencies/SFML",
	"Sources/Dependencies/SQLite",
	"Sources/Dependencies/sqlite3.h",
	"Sources/Dependencies/sqlite3ext.h",
	"Sources/Dependencies/Server/Includes/SQLite",
	"Sources/Dependencies/Server/Includes/sqlite3.h",
	"Sources/Dependencies/Server/Includes/sqlite3ext.h",
	"Sources/Dependencies/Shared/ASIO",
	"Sources/Dependencies/Shared/json.hpp",
	"Sources/Dependencies/Shared/vendor",
	"Sources/AutoUpdater/vendor",
	"Sources/Dependencies/Client/Includes/miniaudio.h",
)
# Prebuilt libraries and their headers: names are linker/compiler inputs; kept, flagged for a decision.
PREBUILT = (
	"Sources/Dependencies/Client/Includes/Cint.h",
	"Sources/Dependencies/Client/Includes/PAK.h",
)
# Tracked build artifacts (Phase H says the user will `git rm --cached` them): names kept.
ARTIFACT_DIR_NAMES = {"debug_x64", "release_x64", "releasedbg"}
BINARY_KEEP_EXTS = {".lib", ".dll", ".a", ".so", ".exe", ".pdb", ".obj", ".ilk", ".exp", ".enc"}
# Stale outputs of old one-off scripts (copies of C++ sources): names kept, never rewritten, never
# used to resolve references (they would pollute suffix matching).
STALE = (".claude/tools/scripts/output",)

# Names a tool requires, kept exactly.
TOOL_REQUIRED = {
	"CMakeLists.txt": "CMake looks for exactly this name",
	"README.md": "GitHub/convention, and the global rule names it",
	"CLAUDE.md": "Claude Code loads exactly this name",
	"Makefile": "make looks for exactly this name",
	"__init__.py": "Python package marker",
	"__main__.py": "Python package entry point",
	"__pycache__": "Python cache folder",
}
# Project/solution files: no tool requires a spelling, but MSBuild derives $(MSBuildProjectName),
# $(TargetName) and this repo's HbProjectKey from the basename. Proposal: keep. Decision item.
PROJECT_FILE_SUFFIXES = (".sln", ".vcxproj", ".vcxproj.filters", ".vcxproj.user", ".vcproj")

# Text handling
BINARY_EXTS = {
	".png", ".jpg", ".jpeg", ".bmp", ".gif", ".ico", ".psd", ".pak", ".wav", ".ogg", ".mp3", ".ttf", ".otf",
	".lib", ".obj", ".exe", ".pdb", ".dll", ".db", ".sqlite", ".zip", ".7z", ".gz", ".enc", ".pyc", ".amd",
	".bin", ".a", ".so", ".ilk", ".exp", ".res", ".dat", ".map", ".spr",
}
CPP_EXTS = {".h", ".hpp", ".hh", ".hxx", ".c", ".cc", ".cpp", ".cxx", ".inl", ".ipp"}
MSBUILD_EXTS = {".vcxproj", ".filters", ".sln", ".props", ".targets"}
SCRIPT_EXTS = {".ps1", ".psm1", ".sh", ".bash", ".bat", ".cmd"}
GENERAL_EXTS = {".rc", ".iss", ".cfg", ".toml", ".ini", ".yml", ".yaml", ".xml", ".html", ".htm", ".js", ".css"}
DATA_TEXT_EXTS = {".txt", ".sql", ".csv"}
ASSET_EXTS = {".pak", ".wav", ".ogg", ".mp3", ".txt", ".json", ".amd", ".ttf", ".png", ".bmp", ".jpg"}
MAX_TEXT_BYTES = 20 * 1024 * 1024

# Report-only: scanned and reported, never written (history, one-off scripts, metrics, local settings).
REPORT_ONLY_EXACT = {"CHANGELOG.md"}
REPORT_ONLY_PREFIXES = (".claude/design/", ".claude/tools/scripts/", ".claude/metrics/")
REPORT_ONLY_NAMES = {"settings.json", "settings.local.json"}

CPP_KEYWORDS = set(
	"alignas alignof and and_eq asm auto bitand bitor bool break case catch char char8_t char16_t char32_t "
	"class compl concept const consteval constexpr constinit const_cast continue co_await co_return co_yield "
	"decltype default delete do double dynamic_cast else enum explicit export extern false float for friend "
	"goto if inline int long mutable namespace new noexcept not not_eq nullptr operator or or_eq private "
	"protected public register reinterpret_cast requires return short signed sizeof static static_assert "
	"static_cast struct switch template this thread_local throw true try typedef typeid typename union "
	"unsigned using virtual void volatile wchar_t while xor xor_eq final override import module".split())

# Header names that a project header must not take: C, POSIX/glibc, Windows SDK.
STD_HEADERS = set(
	"assert.h complex.h ctype.h errno.h fenv.h float.h inttypes.h iso646.h limits.h locale.h math.h setjmp.h "
	"signal.h stdalign.h stdarg.h stdatomic.h stdbool.h stddef.h stdint.h stdio.h stdlib.h stdnoreturn.h "
	"string.h tgmath.h threads.h time.h uchar.h wchar.h wctype.h aio.h cpio.h dirent.h dlfcn.h fcntl.h "
	"fmtmsg.h fnmatch.h ftw.h glob.h grp.h iconv.h langinfo.h libgen.h monetary.h mqueue.h ndbm.h netdb.h "
	"nl_types.h poll.h pthread.h pwd.h regex.h sched.h search.h semaphore.h spawn.h strings.h stropts.h "
	"syslog.h tar.h termios.h trace.h ulimit.h unistd.h utime.h utmpx.h wordexp.h malloc.h memory.h "
	"alloca.h endian.h byteswap.h error.h features.h link.h execinfo.h windows.h winsock.h winsock2.h "
	"ws2tcpip.h process.h io.h direct.h conio.h crtdbg.h tchar.h mmsystem.h shellapi.h shlobj.h winuser.h "
	"winbase.h windef.h wingdi.h objbase.h commctrl.h dbghelp.h psapi.h intrin.h mswsock.h iphlpapi.h "
	"timeapi.h synchapi.h fileapi.h handleapi.h".split())

# --------------------------------------------------------------------------------------------------
# Small helpers
# --------------------------------------------------------------------------------------------------


def relp(p):
	"""Repo-relative posix path for messages; the absolute path when p is outside the repo."""
	try:
		return Path(p).relative_to(REPO).as_posix()
	except ValueError:
		return str(p)


def say(msg=""):
	sys.stdout.write(str(msg).encode("ascii", "replace").decode("ascii") + "\n")
	sys.stdout.flush()


def asc(s):
	return str(s).encode("ascii", "backslashreplace").decode("ascii")


def parent_of(rel):
	return rel.rpartition("/")[0]


def base_of(rel):
	return rel.rpartition("/")[2]


def under(rel, root):
	r, x = rel.casefold(), root.casefold()
	return r == x or r.startswith(x + "/")


def ext_of(name):
	low = name.lower()
	if low.endswith(".vcxproj.filters"):
		return ".filters"
	i = low.rfind(".")
	return low[i:] if i > 0 else ""


def write_text(path, text):
	path.parent.mkdir(parents=True, exist_ok=True)
	with open(path, "w", encoding="utf-8", newline="\n") as f:
		f.write(text)


def skip_path(rel):
	parts = rel.split("/")
	if parts[0].casefold() in SKIP_TOP:
		return True
	low = rel.casefold()
	if any(low.startswith(p) for p in SKIP_PREFIXES):
		return True
	if "__pycache__" in parts:
		return True
	last = parts[-1]
	return ".bak_" in last or last.endswith(".pyc")


# --------------------------------------------------------------------------------------------------
# Universe: what exists, what is tracked, real on-disk case
# --------------------------------------------------------------------------------------------------


class DiskCase:
	def __init__(self):
		self.cache = {}

	def names(self, rel_dir):
		if rel_dir not in self.cache:
			try:
				self.cache[rel_dir] = {e.name.casefold(): e.name for e in os.scandir(REPO / rel_dir if rel_dir else REPO)}
			except OSError:
				self.cache[rel_dir] = {}
		return self.cache[rel_dir]

	def real(self, rel):
		out = []
		for part in rel.split("/"):
			name = self.names("/".join(out)).get(part.casefold())
			if name is None:
				return None
			out.append(name)
		return "/".join(out)


def git_ls(*args):
	r = subprocess.run(["git", "ls-files", "-z", *args], cwd=REPO, capture_output=True)
	if r.returncode != 0:
		raise SystemExit("git ls-files failed: " + asc(r.stderr.decode("utf-8", "replace")))
	return [p for p in r.stdout.decode("utf-8").split("\0") if p]


class FileInfo:
	__slots__ = ("rel", "tracked", "index_rel", "origin")

	def __init__(self, rel, tracked, index_rel, origin):
		self.rel, self.tracked, self.index_rel, self.origin = rel, tracked, index_rel, origin


def index_case_dups(index_paths, disk):
	"""Case-variant duplicates in the index: files (LAN_ENG.H + lan_eng.h) and folders (Sources/x + sources/y).
	Each entry: kind, entries (spellings), disk (on-disk spelling or None), stale (spellings to drop),
	commands (exact git commands that clear it)."""
	out = []
	files = defaultdict(set)
	for p in index_paths:
		files[p.casefold()].add(p)
	for k, v in sorted(files.items()):
		if len(v) > 1:
			real = disk.real(next(iter(v)))
			stale = sorted(x for x in v if x != real) if real in v else sorted(v)[1:]
			out.append({"kind": "file", "entries": sorted(v), "disk": real, "stale": stale,
				"commands": ["git rm --cached --quiet -- %s" % shq(s) for s in stale]})
	dirs = defaultdict(set)
	for p in index_paths:
		parts = p.split("/")
		for n in range(1, len(parts)):
			dirs["/".join(parts[:n]).casefold()].add("/".join(parts[:n]))
	for k, v in sorted(dirs.items()):
		if len(v) > 1:
			real = disk.real(next(iter(v)))
			stale = sorted(x for x in v if x != real)
			cmds = []
			for s in stale:
				for p in sorted(index_paths):
					if p.startswith(s + "/"):
						fixed = (real + p[len(s):]) if real else None
						cmds.append("git rm --cached --quiet -- %s" % shq(p))
						if fixed and (REPO / fixed).is_file():
							cmds.append("git add -- %s" % shq(fixed))
			out.append({"kind": "folder", "entries": sorted(v), "disk": real, "stale": stale, "commands": cmds})
	return out


def build_universe():
	disk = DiskCase()
	files = {}
	notes = {"index_case_mismatch": [], "index_case_duplicates": [], "tracked_missing_on_disk": 0, "untracked": 0,
		"ignored_claude": 0}
	index_paths = git_ls("-c")
	groups = defaultdict(list)
	for p in index_paths:
		groups[p.casefold()].append(p)
	for p in index_paths:
		if skip_path(p):
			continue
		real = disk.real(p)
		if real is None or not (REPO / real).is_file():
			notes["tracked_missing_on_disk"] += 1
			continue
		if real in files:
			# Case-variant duplicate index entry (e.g. LAN_ENG.H and lan_eng.h): keep the disk spelling.
			if p == real:
				files[real].index_rel = p
			continue
		files[real] = FileInfo(real, True, p, "tracked")
	for real, info in files.items():
		if info.tracked and info.index_rel != real:
			notes["index_case_mismatch"].append((info.index_rel, real))
	notes["index_case_duplicates"] = index_case_dups(index_paths, disk)
	for p in git_ls("-o", "--exclude-standard"):
		if skip_path(p):
			continue
		real = disk.real(p)
		if real is None or real in files or not (REPO / real).is_file():
			continue
		files[real] = FileInfo(real, False, None, "untracked")
		notes["untracked"] += 1
	# Ignored but present under .claude/ (plans, untracked docs): in scope, moved with plain mv.
	for root, dnames, fnames in os.walk(REPO / ".claude"):
		rel_root = Path(root).relative_to(REPO).as_posix()
		dnames[:] = [d for d in dnames if not skip_path(rel_root + "/" + d + "/x")]
		for fn in fnames:
			rel = rel_root + "/" + fn
			if skip_path(rel):
				continue
			real = disk.real(rel) or rel
			if real not in files:
				files[real] = FileInfo(real, False, None, "ignored")
				notes["ignored_claude"] += 1
	return files, notes, disk


# --------------------------------------------------------------------------------------------------
# Tree + resolver
# --------------------------------------------------------------------------------------------------


class Tree:
	def __init__(self, file_rels, case_sensitive, hidden=None):
		self.cs = case_sensitive
		self.files = set(file_rels)
		self.dirs = set()
		self.children = defaultdict(dict)
		self.by_last = defaultdict(list)
		self.hidden = hidden or (lambda rel: False)
		for f in sorted(self.files):
			parts = f.split("/")
			for i in range(1, len(parts)):
				d = "/".join(parts[:i])
				if d not in self.dirs:
					self.dirs.add(d)
					self.children["/".join(parts[:i - 1])][self.k(parts[i - 1])] = d
					self.by_last[self.k(parts[i - 1])].append(d)
			self.children[parent_of(f)][self.k(parts[-1])] = f
			self.by_last[self.k(parts[-1])].append(f)

	def k(self, s):
		return s if self.cs else s.casefold()

	def is_dir(self, rel):
		return rel == "" or rel in self.dirs

	def walk(self, base, comps):
		"""comps: list of (index, name). Returns (matched [(index, rel)], complete, final rel)."""
		if not self.is_dir(base):
			return [], False, None
		cur, matched = base, []
		for idx, c in comps:
			if c == ".":
				continue
			if c == "..":
				if cur == "":
					return matched, False, None
				cur = parent_of(cur)
				continue
			nxt = self.children.get(cur, {}).get(self.k(c))
			if nxt is None:
				return matched, False, cur
			matched.append((idx, nxt))
			cur = nxt
		return matched, True, cur

	def suffix(self, names, files_only=False):
		n = len(names)
		out = []
		for cand in self.by_last.get(self.k(names[-1]), ()):
			if self.hidden(cand) or (files_only and cand not in self.files):
				continue
			parts = cand.split("/")
			if len(parts) < n:
				continue
			if all(self.k(parts[len(parts) - n + i]) == self.k(names[i]) for i in range(n)):
				out.append(cand)
		return out


SEP_SPLIT = re.compile(r"([\\/]+)")
CPP_STR_SEP_SPLIT = re.compile(r"(/+|(?:\\\\)+)")


class Resolution:
	__slots__ = ("status", "new", "touched", "detail")

	def __init__(self, status, new=None, touched=(), detail=""):
		self.status, self.new, self.touched, self.detail = status, new, list(touched), detail


class Resolver:
	def __init__(self, plan):
		self.p = plan

	def newbase(self, rel):
		return base_of(self.p.newpath[rel])

	def _rebuild(self, parts, repl):
		out = list(parts)
		for idx, name in repl.items():
			out[idx] = name
		return "".join(out)

	def resolve(self, token, ctx_dir, policy, split_re=SEP_SPLIT, force_path=False):
		"""policy: general | include_q | include_a.

		General text is matched exact-case: a word that only matches case-insensitively (a bin/ key such as
		`sfmlengine`, a runtime folder such as `GameData`) is not a reference to the source entry. When only a
		case-insensitive match exists the result is `casemismatch` (reported, never rewritten). Quoted
		includes match case-insensitively, as MSVC does, so wrong-case includes get fixed. Angle includes
		match exact-case only, so a system header never binds to a project file."""
		if policy == "general":
			res = self._resolve(token, ctx_dir, policy, split_re, self.p.tree_cs, force_path)
			if res.status in ("unresolved",):
				ci = self._resolve(token, ctx_dir, policy, split_re, self.p.tree, force_path)
				if ci.status == "resolved" and ci.new != token and ci.touched:
					return Resolution("casemismatch", ci.new, ci.touched, "matches only case-insensitively")
			return res
		tree = self.p.tree_cs if policy == "include_a" else self.p.tree
		return self._resolve(token, ctx_dir, policy, split_re, tree, force_path)

	def _resolve(self, token, ctx_dir, policy, split_re, tree, force_path=False):
		parts = split_re.split(token)
		comp_idx = [i for i in range(0, len(parts), 2) if parts[i] != ""]
		if not comp_idx:
			return Resolution("skip")
		comps = [(i, parts[i]) for i in comp_idx]
		names = [c for _, c in comps]
		if all(c in (".", "..") for c in names):
			return Resolution("skip")
		has_sep = len(parts) > 1 or force_path
		bases = []
		if policy in ("general", "include_q"):
			bases.append(ctx_dir)
		if policy == "general":
			low = [c.casefold() for c in names]
			if REPO_DIR_NAME.casefold() in low:
				cut = low.index(REPO_DIR_NAME.casefold())
				comps_r = comps[cut + 1:]
				if comps_r:
					m, ok, _ = tree.walk("", comps_r)
					if ok and m:
						return self._finish(parts, m, "resolved")
					if m:
						return self._finish(parts, m, "partial")
				return Resolution("unresolved")
			bases.append("")
		best_partial = None
		for b in bases:
			m, ok, _ = tree.walk(b, comps)
			if ok and m:
				return self._finish(parts, m, "resolved")
			if m and policy == "general" and m[0][0] == comps[0][0] and (best_partial is None or len(m) > len(best_partial)):
				best_partial = m
		# suffix match
		lead_dots = 0
		while lead_dots < len(names) and names[lead_dots] in (".", ".."):
			lead_dots += 1
		tail = comps[lead_dots:]
		if tail and all(c not in (".", "..") for _, c in tail):
			files_only = not has_sep
			cands = tree.suffix([c for _, c in tail], files_only=files_only)
			if cands:
				spellings = {}
				for cand in cands:
					anc, cur = [], cand
					for _ in tail:
						anc.append(cur)
						cur = parent_of(cur)
					anc.reverse()
					spell = tuple(self.newbase(a) for a in anc)
					spellings.setdefault(spell, []).append(anc)
				if len(spellings) == 1:
					anc = next(iter(spellings.values()))[0]
					m = [(tail[i][0], anc[i]) for i in range(len(tail))]
					return self._finish(parts, m, "resolved")
				return Resolution("ambiguous", detail="; ".join(sorted("/".join(s) for s in spellings)))
		# stem fallback: "tiles/tile223-225" names staged/.../tiles/tile223-225.pak without its extension
		if policy == "general" and has_sep and tail and len(tail) >= 2 and not ext_of(tail[-1][1]):
			res = self._stem_fallback(parts, tail, tree)
			if res is not None:
				return res
		if best_partial:
			return self._finish(parts, best_partial, "partial")
		return Resolution("unresolved")

	def _stem_fallback(self, parts, tail, tree):
		dir_names = [c for _, c in tail[:-1]]
		stem = tail[-1][1]
		spellings = {}
		for d in tree.suffix(dir_names):
			if not tree.is_dir(d):
				continue
			hits = [c for c in tree.children.get(d, {}).values()
				if c in tree.files and tree.k(base_of(c).rpartition(".")[0]) == tree.k(stem)]
			if len(hits) != 1:
				continue
			anc, cur = [], d
			for _ in dir_names:
				anc.append(cur)
				cur = parent_of(cur)
			anc.reverse()
			new_stem = self.newbase(hits[0]).rpartition(".")[0]
			spell = tuple(self.newbase(a) for a in anc) + (new_stem,)
			spellings.setdefault(spell, (anc, hits[0]))
		if len(spellings) != 1:
			return None
		spell, (anc, hit) = next(iter(spellings.items()))
		out = list(parts)
		touched = []
		for (idx, _), rel, nb in zip(tail, anc + [hit], spell):
			out[idx] = nb
			if self.newbase(rel) != base_of(rel):
				touched.append(rel)
		return Resolution("resolved", "".join(out), touched, "stem")

	def _finish(self, parts, matched, status):
		if status == "partial":
			# A partial rewrite must not leave a renamed name behind in the unmatched tail
			# (e.g. "GlobalDef.h/RenderConstants.h" is two names, not a path).
			last = matched[-1][0]
			tail = [parts[i] for i in range(last + 2, len(parts), 2) if parts[i]]
			if any(t in self.p.renamed_old_names for t in tail):
				return Resolution("unresolved", detail="partial match leaves renamed names in the tail")
		repl, touched = {}, []
		for idx, rel in matched:
			nb = self.newbase(rel)
			if nb != parts[idx]:
				repl[idx] = nb
			if nb != base_of(rel):
				touched.append(rel)
		new = self._rebuild(parts, repl)
		return Resolution(status, new, touched)


# --------------------------------------------------------------------------------------------------
# Naming
# --------------------------------------------------------------------------------------------------


def snake(seg):
	s = re.sub(r"[- ]+", "_", seg)
	# plural acronym: IDs -> Ids (so DialogBoxIDs -> dialog_box_ids, not dialog_box_i_ds)
	s = re.sub(r"([A-Z])([A-Z]+)s(?![a-z])", lambda m: m.group(1) + m.group(2).lower() + "s", s)
	s = re.sub(r"([A-Z]+)([A-Z][a-z])", r"\1_\2", s)
	s = re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", s)
	return s.lower()


def kebab(seg):
	return snake(seg).replace("_", "-")


def reasons_for(seg, style):
	r = []
	if re.search(r"[^A-Za-z0-9_\- ]", seg):
		r.append("non-identifier character")
	if " " in seg:
		r.append("space")
	if re.search(r"[0-9]-[0-9]", seg):
		r.append("numeric range")
	if re.search(r"[A-Z]{2,}", seg):
		r.append("acronym or all-caps run")
	if re.search(r"[0-9][A-Z]|[A-Z][0-9]", seg):
		r.append("digit next to uppercase")
	if re.search(r"[A-Z][a-z][A-Z](?![a-z])", seg):
		r.append("mixed-case acronym (e.g. AoE, PvP)")
	if re.search(r"[A-Z]", seg) and re.search(r"[_\-]", seg) and style == "snake":
		r.append("mixed style")
	if re.search(r"[A-Z]", seg) and "_" in seg and style == "kebab":
		r.append("mixed style")
	return r


class SegResult:
	__slots__ = ("new", "kind", "reasons", "cls", "decided", "proposal")

	def __init__(self, new, kind, reasons, cls, decided, proposal):
		self.new, self.kind, self.reasons, self.cls, self.decided, self.proposal = new, kind, reasons, cls, decided, proposal


class Naming:
	def __init__(self, all_rels, file_set, decisions, roots):
		"""roots: dict name -> tuple of root rels (third_party, prebuilt, stale) in this tree's spelling."""
		self.files = file_set
		self.dec = decisions
		self.roots = roots
		desc_exts = defaultdict(set)
		for f in file_set:
			e = ext_of(base_of(f))
			p = parent_of(f)
			while p:
				desc_exts[p].add(e)
				p = parent_of(p)
		self.desc_exts = desc_exts

	def exclusion(self, rel):
		for kind in ("third_party", "prebuilt", "stale"):
			for root in self.roots[kind]:
				if under(rel, root):
					return kind.replace("_", "-"), root
		parts = rel.split("/")
		for i, p in enumerate(parts):
			if p.casefold() in ARTIFACT_DIR_NAMES:
				return "build-artifact", "/".join(parts[:i + 1])
		if rel in self.files and ext_of(parts[-1]) in BINARY_KEEP_EXTS and rel.casefold().startswith("sources/"):
			return "prebuilt-binary", rel
		return None

	def classify(self, rel):
		if rel.casefold().startswith("staged/") or rel.casefold() == "staged":
			return "data"
		name = base_of(rel)
		if rel in self.files and ext_of(name) == ".md":
			return "docs"
		if rel not in self.files and self.is_doc_dir(rel):
			return "docs"
		return "code"

	def is_doc_dir(self, rel):
		exts = self.desc_exts.get(rel)
		return bool(exts) and exts == {".md"} and rel.casefold().startswith(".claude/")

	def convert(self, name, is_dir, style):
		fn = snake if style == "snake" else kebab
		if is_dir:
			return ".".join(fn(s) for s in name.split("."))
		stem, dot, ext = name.rpartition(".")
		if not dot or not stem:
			return fn(name)
		return ".".join(fn(s) for s in stem.split(".")) + "." + ext.lower()

	def seg(self, rel):
		name = base_of(rel)
		is_dir = rel not in self.files
		cls = self.classify(rel)
		ex = self.exclusion(rel)
		if ex:
			return SegResult(name, "excluded:" + ex[0], [], cls, True, name)
		if name.startswith("."):
			return SegResult(name, "dot-name", [], cls, True, name)
		if name in TOOL_REQUIRED:
			return SegResult(name, "tool-required", [], cls, True, name)
		forced = self.dec.get("paths", {}).get(rel)
		if not is_dir and name.lower().endswith(PROJECT_FILE_SUFFIXES):
			decided_name = forced or self.dec.get("names", {}).get(name)
			proposal = name
			reasons = ["project file basename drives $(MSBuildProjectName), $(TargetName) and HbProjectKey"]
			return SegResult(decided_name or proposal, "project-file", reasons, cls, decided_name is not None, proposal)
		style = "kebab" if cls == "docs" else "snake"
		proposal = self.convert(name, is_dir, style)
		reasons = []
		if proposal != name or re.search(r"[^A-Za-z0-9_.\- ]", name):
			stem_part = name if is_dir else name.rpartition(".")[0] or name
			reasons = reasons_for(stem_part, style)
		if cls == "docs" and name.upper() == name and re.search(r"[A-Z]", name):
			reasons.append("conventional all-caps doc name")
		kind = "doc" if cls == "docs" else ("data" if cls == "data" else "code")
		decided_name = forced or self.dec.get("names", {}).get(name)
		if decided_name:
			return SegResult(decided_name, kind, reasons, cls, True, proposal)
		low_risk = reasons and all(r == "mixed style" for r in reasons)
		decided = not reasons or (low_risk and self.dec.get("accept_mixed_style", False))
		return SegResult(proposal, kind, reasons, cls, decided, proposal)


# --------------------------------------------------------------------------------------------------
# Encoding (byte-preserving)
# --------------------------------------------------------------------------------------------------


def decode_bytes(b):
	"""Returns (text, codec, bom) or (None, reason, None)."""
	if b.startswith(b"\xef\xbb\xbf"):
		codec, bom = "utf-8", b"\xef\xbb\xbf"
	elif b.startswith(b"\xff\xfe"):
		codec, bom = "utf-16-le", b"\xff\xfe"
	elif b.startswith(b"\xfe\xff"):
		codec, bom = "utf-16-be", b"\xfe\xff"
	elif len(b) >= 4 and b.count(b"\x00") > len(b) // 4:
		codec, bom = ("utf-16-le" if b[1:2] == b"\x00" else "utf-16-be"), b""
	else:
		if b"\x00" in b:
			return None, "binary", None
		try:
			b.decode("utf-8")
			codec, bom = "utf-8", b""
		except UnicodeDecodeError:
			codec, bom = "latin-1", b""
	try:
		text = b[len(bom):].decode(codec)
	except UnicodeDecodeError:
		return None, "decode-failed:" + codec, None
	if bom + text.encode(codec) != b:
		return None, "roundtrip-failed:" + codec, None
	return text, codec, bom


def encode_text(text, codec, bom):
	return bom + text.encode(codec)


def apply_edits(text, edits):
	for e in sorted(edits, key=lambda x: x["start"], reverse=True):
		text = text[:e["start"]] + e["new"] + text[e["end"]:]
	return text


# --------------------------------------------------------------------------------------------------
# Scanners
# --------------------------------------------------------------------------------------------------

CPP_LEX = re.compile(
	r'(?P<inc>^[ \t]*#[ \t]*include[ \t]*(?P<incq>"[^"\n]*"|<[^>\n]*>))'
	r'|(?P<lc>//[^\n]*)'
	r'|(?P<bc>/\*.*?\*/)'
	r'|(?P<raw>(?<![A-Za-z0-9_])(?:u8|[uUL])?R"(?P<delim>[^()\\\s"]{0,16})\(.*?\)(?P=delim)")'
	r'|(?P<str>(?:(?<![A-Za-z0-9_])(?:u8|[uUL]))?"(?P<body>(?:\\.|[^"\\\n])*)")'
	r"|(?P<chr>(?<![0-9A-Za-z_])'(?:\\.|[^'\\\n])*')",
	re.M | re.S)
INCLUDE_RE = re.compile(r'^[ \t]*#[ \t]*include[ \t]*("[^"\n]*"|<[^>\n]*>)', re.M)
TOKEN_RE = re.compile(r"[A-Za-z0-9_.+\-\\/]+")
MASK_RES = [
	re.compile(r"[A-Za-z][A-Za-z0-9+.\-]*://[^\s\"'<>)\]`]*"),
	re.compile(r"\$\([^)\n]*\)"),
	re.compile(r"\$\{[^}\n]*\}"),
	re.compile(r"\$[A-Za-z_][A-Za-z0-9_]*"),
	re.compile(r"%[A-Za-z_][A-Za-z0-9_]*%"),
	re.compile(r"@[A-Za-z_]+@"),
]
FILTER_MASK_RES = [re.compile(r'<Filter\s+Include="[^"]*"'), re.compile(r"<Filter>[^<]*</Filter>")]
SPACED_QUOTED_RE = re.compile(r"([\"'`])([^\"'`\n]*[\\/][^\"'`\n]*)\1")
LIT_RE = re.compile(r"([\"'])((?:(?!\1)[^\\\n]|\\.)*)\1")
DOTTED_RE = re.compile(r"[A-Za-z_][A-Za-z0-9_]*(?:\.[A-Za-z_][A-Za-z0-9_]*)+")
JSON_STR_RE = re.compile(r'"((?:[^"\\\n]|\\.)*)"')
PLACEHOLDER_RE = re.compile(r"\{[^}]*\}|%[-+ #0]*[0-9]*(?:\.[0-9]+)?(?:l|ll|h|z)?[a-zA-Z]")


def mask_text(text, extra=()):
	chars = None
	for rx in list(MASK_RES) + list(extra):
		for m in rx.finditer(text):
			if chars is None:
				chars = list(text)
			for i in range(m.start(), m.end()):
				chars[i] = "\x01"
	return text if chars is None else "".join(chars)


class FileScan:
	def __init__(self, rel, text, handler, report_only):
		self.rel, self.text, self.handler, self.report_only = rel, text, handler, report_only
		self.nl = [i for i, ch in enumerate(text) if ch == "\n"]
		self.edits = []
		self.taken = []

	def line(self, off):
		return bisect.bisect_right(self.nl, off - 1) + 1 if self.nl else 1

	def line_text(self, off):
		ln = self.line(off)
		s = self.nl[ln - 2] + 1 if ln >= 2 else 0
		e = self.nl[ln - 1] if ln - 1 < len(self.nl) else len(self.text)
		return self.text[s:e]

	def overlaps(self, s, e):
		for a, b in self.taken:
			if s < b and a < e:
				return True
		return False


class Scanner:
	def __init__(self, plan):
		self.p = plan
		self.r = Resolver(plan)

	# ---- recording
	def edit(self, fs, start, end, new, rule, touched):
		old = fs.text[start:end]
		if old == new:
			return
		if fs.overlaps(start, end):
			if any(e["start"] == start and e["end"] == end and e["new"] == new for e in fs.edits):
				return  # two rules agree on the same edit
			self.p.finding("WARN", "overlapping-edit", fs.rel, fs.line(start), old, "second rule %s skipped (-> %s)" % (rule, new))
			return
		fs.taken.append((start, end))
		e = {"file": fs.rel, "start": start, "end": end, "old": old, "new": new, "rule": rule,
			"line": fs.line(start), "touched": sorted(set(touched))}
		fs.edits.append(e)

	def unresolved(self, fs, off, token, res, context):
		names = [c for c in SEP_SPLIT.split(token)[0::2] if c]
		hit = [c for c in names if c in self.p.renamed_old_names]
		if res.status == "ambiguous":
			self.p.finding("BLOCKING" if fs.handler not in ("doc",) and not fs.report_only else "WARN",
				"ambiguous-reference", fs.rel, fs.line(off), token, res.detail)
		elif hit:
			self.p.finding("WARN", "unresolved-reference", fs.rel, fs.line(off), token,
				"%s; contains renamed name(s): %s" % (context, ", ".join(sorted(set(hit)))))

	# ---- general path tokens
	def scan_general(self, fs, start=0, end=None, masked=None, rule="path"):
		text = fs.text
		end = len(text) if end is None else end
		masked = masked if masked is not None else mask_text(text)
		ctx = parent_of(fs.rel)
		consumed = []
		for m in SPACED_QUOTED_RE.finditer(masked, start, end):
			content = m.group(2)
			if " " not in content or "\x01" in content:
				continue
			res = self.r.resolve(content, ctx, "general")
			if res.status == "resolved":
				consumed.append((m.start(2), m.end(2)))
				self.edit(fs, m.start(2), m.end(2), res.new, rule, res.touched)
		for m in TOKEN_RE.finditer(masked, start, end):
			s, e = m.start(), m.end()
			if any(a <= s < b for a, b in consumed):
				continue
			tok = m.group()
			while len(tok) > 1 and tok[-1] in ".-+" and not tok.endswith(("/..", "\\..")) and tok != "..":
				tok = tok[:-1]
				e -= 1
			while tok and tok[0] in "-+":
				tok = tok[1:]
				s += 1
			if not tok:
				continue
			# A name glued to a masked macro/variable ($(HbSourcesDir)CControls;) is a path even with no separator.
			after_macro = s > 0 and masked[s - 1] == "\x01"
			has_sep = "/" in tok or "\\" in tok or after_macro
			if not has_sep:
				ext = ext_of(tok)
				if not ext or ext not in self.p.known_exts or not re.match(r"^[A-Za-z0-9_+\-]", tok):
					continue
			if re.fullmatch(r"[0-9.\\/\-+]*", tok):
				continue
			res = self.r.resolve(tok, ctx, "general", force_path=after_macro)
			if res.status in ("resolved", "partial"):
				if res.new != tok:
					self.edit(fs, s, e, res.new, rule if res.status == "resolved" else rule + "-partial", res.touched)
					if res.status == "partial":
						self.p.finding("INFO", "partial-path-rewrite", fs.rel, fs.line(s), tok, "-> " + res.new)
			elif res.status == "casemismatch":
				self.p.finding("WARN", "case-mismatched-reference", fs.rel, fs.line(s), tok,
					"matches %s only case-insensitively; not rewritten" % ", ".join(res.touched[:2]))
			elif has_sep:
				self.unresolved(fs, s, tok, res, "path token")

	# ---- path components passed as separate literals (REPO / "Sources" / "Database", os.path.join, Join-Path)
	def scan_bare_literals(self, fs):
		"""Group string literals chained by `/` or `,`. A chain in a path-join context is joined and resolved
		as one path; only a chain that resolves is rewritten. Any other literal equal to a renamed name is
		reported, never rewritten."""
		text = fs.text
		groups, cur = [], []
		for m in LIT_RE.finditer(text):
			if cur and re.fullmatch(r"[ \t]*[/,][ \t]*", text[cur[-1].end():m.start()]):
				cur.append(m)
			else:
				if cur:
					groups.append(cur)
				cur = [m]
		if cur:
			groups.append(cur)
		ctx = parent_of(fs.rel)
		for g in groups:
			contents = [m.group(2) for m in g]
			comp_lists = [[c for c in SEP_SPLIT.split(x)[0::2] if c] for x in contents]
			flat = [c for cl in comp_lists for c in cl]
			if not flat or not any(c in self.p.renamed_old_names for c in flat):
				continue
			joined = any(in_path_join(text, m.start(), m.end()) for m in g) and \
				not any(re.search(r"[${}%*?<>|]", x) for x in contents)
			res = self.r.resolve("/".join(flat), ctx, "general") if joined else None
			if res is not None and res.status in ("resolved", "partial"):
				new_flat = [c for c in SEP_SPLIT.split(res.new)[0::2] if c]
				if len(new_flat) != len(flat):
					continue
				i = 0
				for m, cl in zip(g, comp_lists):
					n = len(cl)
					new_cl = new_flat[i:i + n]
					i += n
					if new_cl == cl:
						continue
					parts = SEP_SPLIT.split(m.group(2))
					j = 0
					for k in range(0, len(parts), 2):
						if parts[k]:
							parts[k] = new_cl[j]
							j += 1
					self.edit(fs, m.start(2), m.end(2), "".join(parts), "join-chain-literal", res.touched)
				self.p.finding("INFO", "join-chain-rewritten", fs.rel, fs.line(g[0].start()), " / ".join(contents),
					"-> " + res.new)
				continue
			for m in g:
				names = [c for c in SEP_SPLIT.split(m.group(2))[0::2] if c in self.p.renamed_old_names]
				if names and not fs.overlaps(m.start(2), m.end(2)):
					why = ("path-join chain %r does not resolve" % "/".join(flat)) if joined else "no path-join context"
					self.p.finding("WARN", "bare-name-literal", fs.rel, fs.line(m.start(2)), m.group(2),
						"contains renamed name(s) %s; %s; not rewritten" % (", ".join(names), why))

	# ---- python imports of renamed modules
	def scan_python_imports(self, fs):
		for old, new in self.p.py_module_map.items():
			for m in re.finditer(r"(?m)^[ \t]*(?:from|import)[ \t]+(" + re.escape(old) + r")\b", fs.text):
				self.edit(fs, m.start(1), m.end(1), new, "python-import", self.p.py_module_entries[old])

	# ---- python dotted module paths ("ItemRenameTool.app" for __import__, `from Pkg.mod import x`)
	def scan_python_dotted(self, fs):
		spans = []
		for m in LIT_RE.finditer(fs.text):
			if DOTTED_RE.fullmatch(m.group(2)):
				spans.append((m.start(2), m.group(2)))
		for m in re.finditer(r"(?m)^[ \t]*(?:from|import)[ \t]+([A-Za-z_][A-Za-z0-9_.]*)", fs.text):
			if "." in m.group(1):
				spans.append((m.start(1), m.group(1)))
		for m in LIT_RE.finditer(fs.text):
			v = m.group(2)
			if v in self.p.staged_old_stems and not fs.overlaps(m.start(2), m.end(2)):
				self.p.finding("WARN", "indirect-asset-stem", fs.rel, fs.line(m.start(2)), v,
					"Python literal equals a renamed staged asset stem (-> %s); not rewritten" % self.p.staged_old_stems[v])
		for s, c in spans:
			parts = c.split(".")
			if ("." + parts[-1].lower()) in self.p.known_exts | BINARY_EXTS:
				continue  # a file name such as Server.exe, not a module path
			if not any(p in self.p.renamed_old_names or (p + ".py") in self.p.renamed_old_names for p in parts):
				continue
			done = False
			for cand, is_file in (("/".join(parts[:-1] + [parts[-1] + ".py"]), True), ("/".join(parts), False)):
				res = self.r.resolve(cand, parent_of(fs.rel), "general")
				if res.status == "resolved":
					new = res.new[:-3] if is_file else res.new
					self.edit(fs, s, s + len(c), new.replace("/", "."), "python-dotted-module", res.touched)
					done = True
					break
			if not done:
				self.p.finding("WARN", "dotted-name-unresolved", fs.rel, fs.line(s), c,
					"dotted name contains a renamed name but does not resolve to a module; not rewritten")

	# ---- C++
	def scan_cpp(self, fs):
		text = fs.text
		side = cpp_side(fs.rel)
		for m in CPP_LEX.finditer(text):
			if m.group("inc"):
				q = m.group("incq")
				s, e = m.start("incq") + 1, m.end("incq") - 1
				path = q[1:-1]
				angle = q[0] == "<"
				res = self.r.resolve(path, parent_of(fs.rel), "include_a" if angle else "include_q")
				if res.status == "resolved":
					self.edit(fs, s, e, res.new, "include-angle" if angle else "include", res.touched)
				elif angle:
					ci = self.r.resolve(path, parent_of(fs.rel), "include_q")
					if ci.status == "resolved" and ci.touched and not any(self.p.naming.exclusion(t) for t in ci.touched):
						self.p.finding("WARN", "angle-include-case-mismatch", fs.rel, fs.line(s), path,
							"matches renamed project file(s) %s only case-insensitively; not rewritten" % ci.touched[:2])
				else:
					self.p.cpp_unresolved_includes.append((fs.rel, fs.line(s), path, res.status, res.detail))
					if res.status == "ambiguous":
						self.p.finding("BLOCKING", "ambiguous-include", fs.rel, fs.line(s), path, res.detail)
			elif m.group("lc") or m.group("bc"):
				self.scan_general(fs, m.start(), m.end(), rule="cpp-comment")
			elif m.group("str") is not None and m.group("body") is not None:
				self.scan_cpp_string(fs, m.start("body"), m.group("body"), side)

	def scan_cpp_string(self, fs, start, body, side):
		if not body or "://" in body or len(body) > 200:
			return
		parts = CPP_STR_SEP_SPLIT.split(body)
		has_sep = len(parts) > 1
		names = [c for c in parts[0::2] if c]
		if not names:
			return
		last_ext = ext_of(names[-1])
		path_like = (has_sep and " " not in body and all(re.fullmatch(r"[A-Za-z0-9_.+\-{}%]+", c) for c in names)) \
			or (not has_sep and last_ext in ASSET_EXTS and " " not in body and not PLACEHOLDER_RE.search(body))
		if not path_like:
			if body in self.p.staged_old_stems:
				self.p.finding("WARN", "indirect-asset-stem", fs.rel, fs.line(start), body,
					"C++ literal equals a renamed staged asset stem (-> %s); not rewritten" % self.p.staged_old_stems[body])
			return
		if not re.search(r"[A-Za-z]", PLACEHOLDER_RE.sub("", body)):
			return
		comps = [(i, parts[i]) for i in range(0, len(parts), 2) if parts[i] != ""]
		tree = self.p.tree
		for root in asset_roots(side):
			if not tree.is_dir(root):
				continue
			m, ok, cur = tree.walk(root, comps)
			if ok and m:
				res = self.r._finish(parts, m, "resolved")
				self.edit(fs, start, start + len(body), res.new, "cpp-asset-literal", res.touched)
				return
			if len(m) == len(comps) - 1 and cur is not None and tree.is_dir(cur):
				last_idx, last = comps[-1]
				if PLACEHOLDER_RE.search(last):
					rx = re.compile("^" + "".join(".+" if PLACEHOLDER_RE.fullmatch(t) else re.escape(t)
						for t in re.split("(" + PLACEHOLDER_RE.pattern + ")", last) if t) + "$", re.I)
					hits = [c for c in tree.children.get(cur, {}).values() if rx.match(base_of(c))]
					if hits:
						renamed = [h for h in hits if self.r.newbase(h) != base_of(h)]
						res = self.r._finish(parts, m, "resolved")
						if res.new != body:
							self.edit(fs, start, start + len(body), res.new, "cpp-asset-pattern-dir", res.touched)
						if renamed:
							self.p.finding("BLOCKING", "pattern-literal-hits-renamed", fs.rel, fs.line(start), body,
								"%d of %d matching assets are renamed, e.g. %s" % (len(renamed), len(hits), renamed[0]))
						return
				elif not ext_of(last):
					stems = [c for c in tree.children.get(cur, {}).values()
						if c in tree.files and base_of(c).rpartition(".")[0].casefold() == last.casefold()]
					if len(stems) == 1:
						hit = stems[0]
						res = self.r._finish(parts, m, "resolved")
						new_parts = CPP_STR_SEP_SPLIT.split(res.new)
						new_stem = self.r.newbase(hit).rpartition(".")[0]
						new_parts[last_idx] = new_stem
						self.edit(fs, start, start + len(body), "".join(new_parts), "cpp-asset-stem", res.touched + [hit])
						return
		if self.p.literal_could_be_asset(body):
			self.p.finding("WARN", "unmatched-path-literal", fs.rel, fs.line(start), body,
				"looks like a path; no staged/ or repo match (side=%s)" % side)

	# ---- data files
	def scan_data_json(self, fs, rewrite):
		for m in JSON_STR_RE.finditer(fs.text):
			v = m.group(1)
			if v in self.p.staged_old_basenames:
				new, rel = self.p.staged_old_basenames[v]
				if rewrite:
					self.edit(fs, m.start(1), m.end(1), new, "data-json-asset-name", [rel])
				else:
					self.p.finding("WARN", "indirect-asset-name", fs.rel, fs.line(m.start(1)), v, "-> %s (not rewritten)" % new)
			elif v in self.p.staged_old_stems:
				self.p.finding("WARN", "indirect-asset-stem", fs.rel, fs.line(m.start(1)), v,
					"JSON value equals a renamed staged asset stem (-> %s); not rewritten" % self.p.staged_old_stems[v])

	def scan_data_text(self, fs):
		for m in re.finditer(r"[A-Za-z0-9_.+\-]+", fs.text):
			v = m.group()
			if v in self.p.staged_old_basenames:
				self.p.finding("WARN", "indirect-asset-name", fs.rel, fs.line(m.start()), v,
					"-> %s (data text, not rewritten)" % self.p.staged_old_basenames[v][0])
			elif v in self.p.staged_old_stems:
				self.p.finding("WARN", "indirect-asset-stem", fs.rel, fs.line(m.start()), v,
					"-> %s (data text, not rewritten)" % self.p.staged_old_stems[v])

	def scan(self, fs):
		h = fs.handler
		if h == "cpp":
			self.scan_cpp(fs)
		elif h == "msbuild":
			extra = FILTER_MASK_RES if fs.rel.lower().endswith(".filters") else ()
			self.scan_general(fs, masked=mask_text(fs.text, extra), rule="msbuild-path")
		elif h in ("cmake", "script"):
			self.scan_general(fs, rule=h + "-path")
			self.scan_bare_literals(fs)
		elif h == "python":
			self.scan_general(fs, rule="python-path")
			self.scan_bare_literals(fs)
			self.scan_python_imports(fs)
			self.scan_python_dotted(fs)
		elif h in ("doc", "general", "gitignore"):
			self.scan_general(fs, rule=h + "-path")
		elif h == "json":
			self.scan_general(fs, rule="json-path")
			self.scan_data_json(fs, rewrite=True)
		elif h == "data_json":
			self.scan_data_json(fs, rewrite=True)
		elif h == "data_text":
			self.scan_data_text(fs)


def in_path_join(text, qs, qe):
	"""True when the quoted literal text[qs:qe] is itself an operand of a path join: pathlib `/`,
	an argument of os.path.join()/joinpath()/Path() still open on this line, or the child of Join-Path."""
	ls = text.rfind("\n", 0, qs) + 1
	le = text.find("\n", qe)
	le = len(text) if le < 0 else le
	left, right = text[ls:qs].rstrip(), text[qe:le].lstrip()
	if left.endswith("/") and not left.endswith("//"):
		return True
	if right.startswith("/") and not right.startswith("//"):
		return True
	open_call = re.search(r"(?:os\.path\.join|\.joinpath|\bPath|\bPurePath)\s*\(([^()]*)$", left)
	if open_call and (left.endswith(",") or left.endswith("(")):
		return True
	if re.search(r"Join-Path(?:\s+-Path)?(?:\s+\S+)?(?:\s+-ChildPath)?\s*$", left):
		return True
	return False


def cpp_side(rel):
	low = rel.casefold()
	if low.startswith("sources/server/"):
		return "server"
	if low.startswith(("sources/client/", "sources/sfmlengine/", "sources/ccontrols/", "sources/autoupdater/")):
		return "client"
	return "both"


def asset_roots(side):
	client = ("staged/client", "staged/client/sprites", "staged/shared")
	server = ("staged/server", "staged/shared")
	if side == "client":
		return client
	if side == "server":
		return server
	return client + ("staged/server",)


def handler_for(rel):
	name = base_of(rel)
	ext = ext_of(name)
	if name in (".gitignore", ".gitattributes"):
		return "gitignore"
	if ext in BINARY_EXTS:
		return None
	if ext in CPP_EXTS:
		return "cpp"
	if ext in MSBUILD_EXTS:
		return "msbuild"
	if name == "CMakeLists.txt" or ext == ".cmake":
		return "cmake"
	if ext in SCRIPT_EXTS:
		return "script"
	if ext == ".py":
		return "python"
	if ext == ".md":
		return "doc"
	if ext == ".json":
		return "data_json" if rel.casefold().startswith("staged/") else "json"
	if ext in GENERAL_EXTS:
		return "general"
	if ext in DATA_TEXT_EXTS:
		return "data_text"
	return None


def is_report_only(rel):
	if rel in REPORT_ONLY_EXACT or base_of(rel) in REPORT_ONLY_NAMES:
		return True
	return any(rel.startswith(p) for p in REPORT_ONLY_PREFIXES)


# --------------------------------------------------------------------------------------------------
# Plan
# --------------------------------------------------------------------------------------------------


class Plan:
	def __init__(self, classes, decisions, decisions_path):
		self.classes = classes
		self.decisions = decisions
		self.decisions_path = decisions_path
		self.findings = []
		self.cpp_unresolved_includes = []

	def finding(self, level, kind, rel, line, text, detail):
		self.findings.append({"level": level, "kind": kind, "file": rel, "line": line, "text": text, "detail": detail})

	def literal_could_be_asset(self, body):
		names = [c for c in CPP_STR_SEP_SPLIT.split(body)[0::2] if c]
		return bool(names) and bool(re.search(r"[A-Za-z]", names[0]))


def load_decisions(path):
	p = Path(path)
	if not p.is_file():
		return {}
	with open(p, "r", encoding="utf-8") as f:
		d = json.load(f)
	out = {"names": d.get("names", {}), "paths": d.get("paths", {}), "accept_mixed_style": bool(d.get("accept_mixed_style", False))}
	if d.get("classes"):
		out["classes"] = sorted(d["classes"])
	return out


def build_plan(classes, decisions_path):
	t0 = time.time()
	decisions = load_decisions(decisions_path)
	plan = Plan(classes, decisions, decisions_path)
	files, notes, disk = build_universe()
	plan.files, plan.notes, plan.disk = files, notes, disk
	file_set = set(files)
	dirs = set()
	for f in files:
		p = parent_of(f)
		while p and p not in dirs:
			dirs.add(p)
			p = parent_of(p)
	plan.dirs = dirs
	all_rels = sorted(file_set | dirs, key=lambda r: (r.count("/"), r))
	roots = {
		"third_party": tuple(disk.real(r) or r for r in THIRD_PARTY),
		"prebuilt": tuple(disk.real(r) or r for r in PREBUILT),
		"stale": tuple(disk.real(r) or r for r in STALE),
	}
	plan.roots = roots
	naming = Naming(all_rels, file_set, decisions, roots)
	plan.naming = naming
	seg = {}
	newpath = {"": ""}
	for rel in all_rels:
		r = naming.seg(rel)
		seg[rel] = r
		nb = r.new if r.cls in classes else base_of(rel)
		par = parent_of(rel)
		newpath[rel] = (newpath[par] + "/" + nb) if par else nb
	plan.seg, plan.newpath = seg, newpath

	def hidden(rel):
		ex = naming.exclusion(rel)
		return bool(ex) and ex[0] in ("stale", "build-artifact")

	plan.hidden = hidden
	plan.tree = Tree(file_set, case_sensitive=False, hidden=hidden)
	plan.tree_cs = Tree(file_set, case_sensitive=True, hidden=hidden)
	plan.known_exts = {ext_of(base_of(f)) for f in file_set if ext_of(base_of(f))} - {""}

	# rename bookkeeping
	plan.renamed = [rel for rel in all_rels if base_of(newpath[rel]) != base_of(rel)]
	plan.renamed_old_names = {base_of(r) for r in plan.renamed}
	plan.py_module_map, plan.py_module_entries = {}, defaultdict(list)
	for rel in plan.renamed:
		if rel in file_set and rel.endswith(".py") and not naming.exclusion(rel):
			o, n = base_of(rel)[:-3], base_of(newpath[rel])[:-3]
			plan.py_module_map[o] = n
			plan.py_module_entries[o].append(rel)
	plan.staged_old_basenames, plan.staged_old_stems = {}, {}
	stem_targets = defaultdict(set)
	for rel in plan.renamed:
		if rel in file_set and rel.casefold().startswith("staged/"):
			plan.staged_old_basenames[base_of(rel)] = (base_of(newpath[rel]), rel)
			stem_targets[base_of(rel).rpartition(".")[0]].add(base_of(newpath[rel]).rpartition(".")[0])
	plan.staged_old_stems = {k: next(iter(v)) for k, v in stem_targets.items() if len(v) == 1 and k}

	# scan text files
	plan.texts, plan.codecs, plan.shas, plan.scans = {}, {}, {}, {}
	plan.encoding_counts = Counter()
	scanner = Scanner(plan)
	for rel in sorted(file_set):
		if rel == SCRIPT_REL or base_of(rel) == "rename_to_naming_rule.decisions.json":
			continue
		ex = naming.exclusion(rel)
		if ex:
			continue
		h = handler_for(rel)
		if h is None:
			continue
		path = REPO / rel
		try:
			if path.stat().st_size > MAX_TEXT_BYTES:
				plan.finding("INFO", "skipped-large-file", rel, 0, "", "over %d bytes" % MAX_TEXT_BYTES)
				continue
			b = path.read_bytes()
		except OSError as exc:
			plan.finding("WARN", "file-vanished", rel, 0, "", asc(exc))
			continue
		text, codec, bom = decode_bytes(b)
		if text is None:
			if codec != "binary":
				plan.finding("WARN", "encoding-skip", rel, 0, "", codec)
			continue
		plan.encoding_counts[codec + ("+bom" if bom else "")] += 1
		plan.texts[rel] = text
		plan.codecs[rel] = (codec, bom)
		plan.shas[rel] = hashlib.sha256(b).hexdigest()
		fs = FileScan(rel, text, h, is_report_only(rel))
		scanner.scan(fs)
		plan.scans[rel] = fs
	plan.edits = []
	plan.report_only_hits = []
	for rel, fs in plan.scans.items():
		if fs.report_only:
			plan.report_only_hits.extend(fs.edits)
		else:
			plan.edits.extend(fs.edits)
	plan.edits_by_file = defaultdict(list)
	for e in plan.edits:
		plan.edits_by_file[e["file"]].append(e)

	# id
	payload = {
		"classes": sorted(classes),
		"decisions": decisions,
		"renames": [(r, newpath[r]) for r in sorted(newpath) if r and newpath[r] != r],
		"edits": [(e["file"], e["start"], e["end"], e["new"]) for e in plan.edits],
		"files": sorted(plan.shas.items()),
		"index_mismatch": notes["index_case_mismatch"],
		"index_duplicates": notes["index_case_duplicates"],
		# the script itself is part of the plan: a changed script means a new run id
		"script_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
	}
	plan.run_id = hashlib.sha256(json.dumps(payload, sort_keys=True).encode("utf-8")).hexdigest()[:12]
	plan.elapsed = time.time() - t0
	return plan


# --------------------------------------------------------------------------------------------------
# Moves (generated script for the user)
# --------------------------------------------------------------------------------------------------


def move_steps(plan):
	steps = []
	tracked_dirs = set()
	for f, i in plan.files.items():
		if i.tracked:
			p = parent_of(f)
			while p and p not in tracked_dirs:
				tracked_dirs.add(p)
				p = parent_of(p)
	rels = sorted([r for r in plan.newpath if r], key=lambda r: (-r.count("/"), r))
	for rel in rels:
		nb = base_of(plan.newpath[rel])
		info = plan.files.get(rel)
		src = rel
		if info is not None and info.tracked and info.index_rel != rel and info.index_rel.casefold() == rel.casefold():
			src = info.index_rel
		if nb == base_of(src):
			continue
		dst = (parent_of(rel) + "/" + nb) if parent_of(rel) else nb
		tracked = info.tracked if info is not None else rel in tracked_dirs
		steps.append({
			"src": src, "dst": dst, "git": tracked, "dir": info is None,
			"case_only": src.casefold() == dst.casefold(), "cls": plan.seg[rel].cls,
			# index-only case fix: the disk already has dst's spelling, only the index says src
			"index_fix": src != rel and base_of(rel) == nb,
		})
	return steps


def shq(s):
	return "'" + s.replace("'", "'\\''") + "'"


def move_preview(plan, steps, reverse=False):
	"""A readable listing of the moves. Not runnable: --apply performs the moves itself (journaled)."""
	out = [
		"# PREVIEW ONLY - run id %s - %s order. Not a script: rename_to_naming_rule.py --apply performs these"
		% (plan.run_id, "reverse (rollback)" if reverse else "apply"),
		"# moves itself, after the rewrites, journaled (resume with --resume <id>, undo with --rollback <id>).",
		"# kind: git = git mv (tracked), plain = os.rename (untracked only); two-step = via <src>.__rn_tmp__",
		"# because the rename is case-only and core.ignorecase=true.",
		"",
	]
	seq = list(reversed(steps)) if reverse else steps
	for st in seq:
		src, dst = (st["dst"], st["src"]) if reverse else (st["src"], st["dst"])
		out.append("%-5s %-8s %s -> %s   # %s %s" % ("git" if st["git"] else "plain", "two-step" if st["case_only"] else "",
			src, dst, st["cls"], "dir" if st["dir"] else "file"))
	return "\n".join(out) + "\n"


# --------------------------------------------------------------------------------------------------
# Preflight (read-only; refuses, never fixes or kills anything)
# --------------------------------------------------------------------------------------------------


def windows_processes():
	"""[(image, pid)] from tasklist, or None when not on Windows / unavailable."""
	if os.name != "nt":
		return None
	try:
		r = subprocess.run(["tasklist", "/FO", "CSV", "/NH"], capture_output=True)
	except OSError:
		return None
	out = []
	for line in r.stdout.decode("utf-8", "replace").splitlines():
		cols = [c.strip('"') for c in line.split('","')]
		if len(cols) >= 2:
			out.append((cols[0].strip('"'), cols[1]))
	return out


def process_paths(names):
	"""{pid: path or ''} for processes with these image names (PowerShell Get-Process; Windows only)."""
	if os.name != "nt":
		return {}
	cmd = ("Get-Process -Name %s -ErrorAction SilentlyContinue | ForEach-Object { '' + $_.Id + '|' + $_.Path }"
		% ",".join(n.rsplit(".", 1)[0] for n in names))
	try:
		r = subprocess.run(["powershell", "-NoProfile", "-NonInteractive", "-Command", cmd], capture_output=True)
	except OSError:
		return {}
	res = {}
	for line in r.stdout.decode("utf-8", "replace").splitlines():
		if "|" in line:
			pid, path = line.split("|", 1)
			res[pid.strip()] = path.strip()
	return res


def preflight(steps, simulate=True):
	"""Returns (refusals, warnings, info, rm_cached). Read-only: git ls-files, bak.py status, tasklist.
	Reads the index fresh, so it is valid for a resume too (simulate=False skips the initial-state move
	simulation, which a half-applied tree cannot pass)."""
	refuse, warn, info = [], [], []
	warn.append("close every shell, editor, terminal or tool whose working directory is inside the repo:"
		" Windows cannot rename a folder that a process has as its current directory")
	# 1. tracked entries deleted on disk under any moved source: `git mv <dir>` fails with "bad source"
	srcs = {s["src"].casefold() for s in steps}
	deleted = git_ls("-d")
	blocked = []
	for p in deleted:
		cur = p.casefold()
		while cur:
			if cur in srcs:
				blocked.append(p)
				break
			cur = parent_of(cur)
	rm_cached = ["git rm --cached --quiet -- %s" % shq(p) for p in sorted(blocked)]
	if blocked:
		refuse.append("%d tracked index entries are deleted on disk under moved folders (git mv fails with 'bad"
			" source'); clear them first with the git rm --cached list (git_rm_cached.txt)" % len(blocked))
	info.append("%d tracked entries deleted on disk in total; %d of them under moved folders" % (len(deleted), len(blocked)))
	# 2. case-variant duplicate index entries (files and folders), read fresh from the index
	# Only a duplicate that overlaps a moved path can break a git mv; others (e.g. the deleted Binaries/ tree)
	# are reported as info.
	def overlaps_move(entry):
		e = entry.casefold()
		return any(e == s or e.startswith(s + "/") or s.startswith(e + "/") for s in srcs)

	other = 0
	for dup in index_case_dups(git_ls("-c"), DiskCase()):
		if not any(overlaps_move(x) for x in dup["entries"]):
			other += 1
			continue
		refuse.append("index holds case-variant duplicate %ss %s (disk: %s); clear the stale spelling first"
			% (dup["kind"], dup["entries"], dup["disk"]))
		rm_cached += dup["commands"]
	if other:
		info.append("%d case-variant duplicate groups in the index outside every moved path (not blocking;"
			" e.g. the deleted Binaries/ tree)" % other)
	# 3. bak.py checkpoints (exit 1 when any exist)
	bak = REPO / ".claude" / "tools" / "bak.py"
	if bak.is_file():
		r = subprocess.run([sys.executable, str(bak), "status"], cwd=REPO, capture_output=True)
		text = asc(r.stdout.decode("utf-8", "replace")).strip()
		if r.returncode != 0:
			refuse.append("bak.py status shows checkpoints; commit or revert them first: " + text.replace("\n", " | ")[:400])
		else:
			info.append("bak.py status: " + text.splitlines()[0] if text else "bak.py status: clean")
	# 4. stray .bak_ files outside Sources/ (bak.py does not see them): report only
	for top in ("tools", ".claude/tools"):
		for root, dnames, fnames in os.walk(REPO / top):
			dnames[:] = [x for x in dnames if x != "__pycache__"]
			for fn in fnames:
				if ".bak_" in fn:
					warn.append("stray backup (not deleted): " + (Path(root) / fn).relative_to(REPO).as_posix())
	# 5. processes: Visual Studio, and the game/server running from inside the repo
	procs = windows_processes()
	if procs is None:
		info.append("process check skipped (not Windows or tasklist unavailable)")
	else:
		vs = [pid for img, pid in procs if img.lower() == "devenv.exe"]
		if vs:
			refuse.append("Visual Studio is running (devenv.exe pid %s); close it first (it locks .vs and project files)" % ", ".join(vs))
		game = [(img, pid) for img, pid in procs if img.lower() in ("server.exe", "game_x64_win.exe")]
		if game:
			paths = process_paths(["Server.exe", "Game_x64_win.exe"])
			repo_low = str(REPO).casefold()
			for img, pid in game:
				path = paths.get(pid, "")
				if not path:
					refuse.append("%s (pid %s) is running and its path cannot be read; close it or confirm it is outside the repo" % (img, pid))
				elif path.casefold().startswith(repo_low):
					refuse.append("%s (pid %s) is running from inside the repo: %s" % (img, pid, asc(path)))
				else:
					info.append("%s (pid %s) runs outside the repo: %s" % (img, pid, asc(path)))
		info.append("process check: %d processes scanned, devenv.exe %s" % (len(procs), "RUNNING" if vs else "not running"))
	# 6. git index lock
	if (REPO / ".git" / "index.lock").exists():
		refuse.append(".git/index.lock exists; another git process is running or crashed")
	# 7. initial-state move simulation: every source exists, no non-case-only target exists
	if not simulate:
		info.append("move simulation skipped (resume: the tree is part-way through the run)")
		return refuse, warn, info, rm_cached
	missing = [s["src"] for s in steps if not os.path.lexists(REPO / s["src"])]
	taken = [s["dst"] for s in steps if not s["case_only"] and os.path.lexists(REPO / s["dst"])]
	for m in missing[:20]:
		refuse.append("move source missing: " + m)
	for t in taken[:20]:
		refuse.append("move target already exists: " + t)
	info.append("move simulation: %d steps, %d sources missing, %d targets taken" % (len(steps), len(missing), len(taken)))
	return refuse, warn, info, rm_cached


def print_preflight(run_id, refuse, warn, info, rm_cached, out_dir):
	lines = ["=== Preflight: rename_to_naming_rule.py run %s ===" % run_id]
	for x in info:
		lines.append("INFO    " + x)
	for x in warn:
		lines.append("WARN    " + x)
	for x in refuse:
		lines.append("REFUSE  " + x)
	if rm_cached:
		lines.append("--- git rm --cached list (%d) - run by the main session, not by this script ---" % len(rm_cached))
		lines += rm_cached
	lines.append("Preflight: %s (%d refusals, %d warnings)" % ("PASS" if not refuse else "REFUSED", len(refuse), len(warn)))
	text = "\n".join(asc(x) for x in lines) + "\n"
	write_text(out_dir / "preflight.log", text)
	write_text(out_dir / "git_rm_cached.txt", "\n".join(rm_cached) + ("\n" if rm_cached else ""))
	for x in lines:
		say(x)
	return not refuse


# --------------------------------------------------------------------------------------------------
# Executor: journaled, idempotent, resumable, reversible
# --------------------------------------------------------------------------------------------------


class IndexView:
	"""Exact-case view of the git index, refreshed from `git ls-files` and updated after each git mv."""

	def __init__(self):
		self.paths = set(git_ls("-c"))

	def has(self, rel):
		if rel in self.paths:
			return True
		pre = rel + "/"
		return any(p.startswith(pre) for p in self.paths)

	def moved(self, src, dst):
		pre = src + "/"
		new = set()
		for p in self.paths:
			if p == src:
				new.add(dst)
			elif p.startswith(pre):
				new.add(dst + "/" + p[len(pre):])
			else:
				new.add(p)
		self.paths = new


def disk_has_exact(rel):
	"""True when rel exists with exactly this spelling of its last segment (case-insensitive file systems)."""
	p = REPO / rel
	if not os.path.lexists(p):
		return False
	try:
		return base_of(rel) in os.listdir(p.parent)
	except OSError:
		return False


TMP_SUFFIX = ".__rn_tmp__"


class StateMismatch(RuntimeError):
	"""Disk and git index disagree about a path. Never auto-rolled back: a human decides which one is right."""

	def __init__(self, msg, recovery):
		RuntimeError.__init__(self, msg)
		self.recovery = recovery


def _present(kind, rel, index):
	"""Presence of rel. For git steps disk and index must agree; a disagreement raises StateMismatch."""
	on_disk = disk_has_exact(rel)
	if kind != "git":
		return on_disk
	in_index = index.has(rel)
	if on_disk != in_index:
		if on_disk:
			fix = ["the path is on disk but not in the index (moved without git?):",
				"  either undo the disk move so disk matches the index, or record it in the index:",
				"  git add -A -- %s   (and git rm -r --cached --quiet -- <the old spelling> if the index still has it)" % shq(rel)]
		else:
			fix = ["the index has the path but the disk does not (moved or deleted outside git?):",
				"  either restore it on disk, or drop it from the index:",
				"  git rm -r --cached --quiet -- %s" % shq(rel)]
		raise StateMismatch("disk and index disagree about %s (disk: %s, index: %s)" % (rel, on_disk, in_index), fix)
	return on_disk


def _mv(kind, a, b, index):
	if kind == "git":
		r = subprocess.run(["git", "mv", "--", a, b], cwd=REPO, capture_output=True)
		if r.returncode != 0:
			raise RuntimeError("git mv %s %s failed: %s" % (a, b, asc(r.stderr.decode("utf-8", "replace")).strip()))
		index.moved(a, b)
	else:
		os.rename(REPO / a, REPO / b)


def do_index_fix(st, index, reverse):
	"""Index-only case fix (index `claude.md`, disk `CLAUDE.md`). Start state: index src, disk dst.
	End state: index dst, disk dst. Forward runs git mv through a temp; reverse puts the index back to src
	and then restores the disk spelling dst with plain renames, so a rollback is exact."""
	s, t = st["src"], st["dst"]
	tmp_s, tmp_t = s + TMP_SUFFIX, t + TMP_SUFFIX
	i_s, i_t = index.has(s), index.has(t)
	d_s, d_t = disk_has_exact(s), disk_has_exact(t)
	start = i_s and not i_t and d_t and not d_s
	end = i_t and not i_s and d_t and not d_s
	if not reverse:
		if end:
			return "skip"
		if index.has(tmp_s):
			_mv("git", tmp_s, t, index)
			return "resumed"
		if not start:
			raise StateMismatch("index-case fix %s -> %s: unexpected state (index %s/%s, disk %s/%s)"
				% (s, t, i_s, i_t, d_s, d_t), ["git ls-files -- %s %s ; compare with the disk, then --resume" % (shq(s), shq(t))])
		_mv("git", s, tmp_s, index)
		_mv("git", tmp_s, t, index)
		return "done"
	if start:
		return "skip"
	if index.has(tmp_s):  # forward stopped after its first half: put the index back, restore disk spelling
		_mv("git", tmp_s, s, index)
	elif index.has(tmp_t) or end:
		if end:
			_mv("git", t, tmp_t, index)
		_mv("git", tmp_t, s, index)
	else:
		raise StateMismatch("index-case fix %s -> %s: unexpected state on rollback (index %s/%s, disk %s/%s)"
			% (s, t, i_s, i_t, d_s, d_t), ["git ls-files -- %s %s ; compare with the disk, then --rollback again" % (shq(s), shq(t))])
	if disk_has_exact(s) and not disk_has_exact(t):  # disk now says s; the original disk spelling was t
		os.rename(REPO / s, REPO / tmp_t)
		os.rename(REPO / tmp_t, REPO / t)
	return "done"


def do_move(st, index, reverse=False):
	"""Idempotent: returns 'done' (performed), 'skip' (already in place) or 'resumed' (finished a half-done
	case-only rename). Raises RuntimeError on an unexpected state, StateMismatch when disk and index disagree.

	Both temp names are checked in both directions: <src>.__rn_tmp__ is left by a forward case-only rename
	that stopped after its first half, <dst>.__rn_tmp__ by a reverse one. Either way the temp goes to the
	target of the current direction."""
	frm, to = (st["dst"], st["src"]) if reverse else (st["src"], st["dst"])
	kind = "git" if st["git"] else "plain"
	if st.get("index_fix"):
		return do_index_fix(st, index, reverse)
	for tmp in (st["src"] + TMP_SUFFIX, st["dst"] + TMP_SUFFIX):
		if _present(kind, tmp, index):
			if _present(kind, to, index):
				raise RuntimeError("both %s and %s exist; resolve by hand" % (tmp, to))
			_mv(kind, tmp, to, index)
			return "resumed"
	at_from, at_to = _present(kind, frm, index), _present(kind, to, index)
	if not at_from and at_to:
		return "skip"
	if not at_from:
		raise RuntimeError("move source missing: %s (target %s also missing)" % (frm, to))
	if at_to and not st["case_only"]:
		raise RuntimeError("move target already exists: %s" % to)
	if st["case_only"]:
		own_tmp = frm + TMP_SUFFIX
		_mv(kind, frm, own_tmp, index)
		_mv(kind, own_tmp, to, index)
	else:
		_mv(kind, frm, to, index)
	return "done"


def sha_file(path):
	try:
		return hashlib.sha256(path.read_bytes()).hexdigest()
	except OSError:
		return None


def atomic_write(path, data):
	tmp = path.with_name(path.name + ".__rn_new__")
	try:
		with open(tmp, "wb") as f:
			f.write(data)
			f.flush()
			os.fsync(f.fileno())
		try:
			shutil.copymode(path, tmp)
		except OSError:
			pass
		os.replace(tmp, path)
	finally:
		if os.path.lexists(tmp):
			os.remove(tmp)


def do_rewrite(op, d, reverse=False):
	path = REPO / op["rel"]
	cur = sha_file(path)
	want_from, want_to = (op["sha_after"], op["sha_before"]) if reverse else (op["sha_before"], op["sha_after"])
	if cur == want_to:
		return "skip"
	if cur != want_from:
		raise RuntimeError("%s changed outside the run (sha %s); not touched" % (op["rel"], (cur or "missing")[:12]))
	src = d / ("backup" if reverse else "new") / op["rel"]
	atomic_write(path, src.read_bytes())
	return "done"


class Journal:
	def __init__(self, path):
		self.path = path

	def records(self):
		out = []
		if self.path.is_file():
			with open(self.path, "r", encoding="utf-8", errors="replace") as f:
				for n, line in enumerate(f, 1):
					line = line.strip()
					if not line:
						continue
					try:
						out.append(json.loads(line))
					except ValueError:
						say("journal: skipped unparseable line %d (torn write)" % n)
		return out

	def done(self):
		return {j["i"] for j in self.records() if j.get("phase") == "apply"}

	def add(self, **rec):
		rec["t"] = time.strftime("%Y-%m-%d %H:%M:%S")
		with open(self.path, "a", encoding="utf-8") as f:
			f.write(json.dumps(rec) + "\n")
			f.flush()
			os.fsync(f.fileno())


def print_mismatch(d, exc, i):
	say("STOPPED at step %d: %s" % (i, asc(exc)))
	say("Disk and git index disagree, so this run does NOT roll back automatically. Recovery:")
	for line in exc.recovery:
		say("  " + line)
	say("Then continue with:  python %s --resume %s" % (SCRIPT_REL, d.name))
	say("or undo the whole run:  python %s --rollback %s" % (SCRIPT_REL, d.name))


def execute(d, ops):
	journal = Journal(d / "journal.jsonl")
	done = journal.done()
	index = IndexView()
	n_done = n_skip = 0
	i = -1
	try:
		for i, op in enumerate(ops):
			if i in done:
				continue
			r = do_rewrite(op, d) if op["op"] == "rewrite" else do_move(op, index)
			journal.add(phase="apply", i=i, op=op["op"], result=r)
			n_done += r != "skip"
			n_skip += r == "skip"
	except StateMismatch as exc:
		journal.add(phase="mismatch", i=i, error=asc(exc))
		print_mismatch(d, exc, i)
		return 3
	except Exception as exc:
		say("FAILED at step %d: %s" % (i, asc(exc)))
		journal.add(phase="error", i=i, error=asc(exc))
		say("Rolling back everything this run changed ...")
		rc = rollback(d)
		say("Rollback %s." % ("complete" if rc == 0 else "INCOMPLETE - see above and journal.jsonl"))
		return 1
	write_text(d / "applied.json", json.dumps({"run_id": d.name, "ops": len(ops), "done": n_done, "skipped": n_skip,
		"generated": time.strftime("%Y-%m-%d %H:%M:%S")}, indent=1) + "\n")
	say("Applied run %s: %d steps performed, %d already in place." % (d.name, n_done, n_skip))
	say("Undo with: python %s --rollback %s" % (SCRIPT_REL, d.name))
	return 0


def load_ops(d):
	with open(d / "apply_plan.json", "r", encoding="utf-8") as f:
		return json.load(f)["ops"]


def rollback(d):
	"""Reverse every move (newest first), then restore rewritten files from the backups.

	Runs over ALL ops, not only journaled ones: a step can finish without being journaled (killed between
	the step and the journal write). Every op is idempotent and sha-guarded, so a step that never ran is
	a no-op. The step that failed may be absent entirely; that is tolerated."""
	ops = load_ops(d)
	journal = Journal(d / "journal.jsonl")
	failed = {j["i"] for j in journal.records() if j.get("phase") == "error"}
	index = IndexView()
	problems = 0
	for i in range(len(ops) - 1, -1, -1):
		op = ops[i]
		try:
			if i in failed and op["op"] == "rewrite":
				# a rewrite is atomic (temp file + os.replace): a failed one never changed the file
				journal.add(phase="rollback", i=i, op="rewrite", result="never-started")
				continue
			if i in failed and op["op"] == "move":
				kind = "git" if op["git"] else "plain"
				names = (op["src"], op["dst"], op["src"] + TMP_SUFFIX, op["dst"] + TMP_SUFFIX)
				if not any(_present(kind, n, index) for n in names):
					journal.add(phase="rollback", i=i, op="move", result="never-started")
					continue
			r = do_rewrite(op, d, reverse=True) if op["op"] == "rewrite" else do_move(op, index, reverse=True)
			journal.add(phase="rollback", i=i, op=op["op"], result=r)
		except StateMismatch as exc:
			problems += 1
			say("ROLLBACK stopped at step %d: %s" % (i, asc(exc)))
			for line in exc.recovery:
				say("  " + line)
			say("Fix that, then run --rollback %s again (it is idempotent)." % d.name)
			journal.add(phase="rollback-mismatch", i=i, error=asc(exc))
			break
		except Exception as exc:
			problems += 1
			say("ROLLBACK problem at step %d: %s" % (i, asc(exc)))
			journal.add(phase="rollback-error", i=i, error=asc(exc))
	if problems == 0:
		write_text(d / "rolled_back.json", json.dumps({"run_id": d.name, "problems": 0,
			"generated": time.strftime("%Y-%m-%d %H:%M:%S")}, indent=1) + "\n")
	return 0 if problems == 0 else 1


def archive_run_state(d):
	"""After a completed rollback, move the journal and staged files aside so a fresh apply can start."""
	arch = d / ("archive_" + time.strftime("%Y%m%d_%H%M%S"))
	arch.mkdir(parents=True, exist_ok=True)
	for name in ("journal.jsonl", "apply_plan.json", "rolled_back.json", "applied.json", "backup", "new"):
		p = d / name
		if os.path.lexists(p):
			os.rename(p, arch / name)
	return arch


# --------------------------------------------------------------------------------------------------
# Verify
# --------------------------------------------------------------------------------------------------


def expand_msbuild(path, file_dir, src_dir):
	m = re.match(r"^\$\(([A-Za-z]+)\)", path)
	if not m:
		return file_dir, path
	macro = m.group(1)
	rest = path[m.end():]
	if macro in ("HbRepoRoot", "SolutionDir"):
		return "", rest
	if macro == "HbSourcesDir":
		return src_dir, rest
	if macro in ("ProjectDir", "MSBuildThisFileDirectory", "MSBuildProjectDirectory"):
		return file_dir, rest.lstrip("\\/")
	return None, None


def tree_resolve(tree, base, path):
	comps = [(i, c) for i, c in enumerate(re.split(r"[\\/]+", path)) if c]
	if not comps:
		return None
	m, ok, cur = tree.walk(base, comps)
	return cur if ok else None


def include_resolve(tree, includer_dir, path, angle):
	if not angle:
		r = tree_resolve(tree, includer_dir, path)
		if r:
			return {r}
	names = [c for c in re.split(r"[\\/]+", path) if c and c not in (".", "..")]
	if not names:
		return set()
	return set(tree.suffix(names))


def closure_check(plan, out):
	"""Apply the plan in memory and re-resolve references case-sensitively, as Linux would."""
	np = plan.newpath
	new_tree = Tree([np[f] for f in plan.files], case_sensitive=True, hidden=lambda r: False)
	old_tree = plan.tree
	src_old = plan.disk.real("Sources") or "Sources"
	src_new = np.get(src_old, src_old)
	checked = Counter()
	for rel, fs in plan.scans.items():
		if fs.report_only or plan.naming.exclusion(rel):
			continue
		old_text = fs.text
		new_text = apply_edits(old_text, plan.edits_by_file.get(rel, []))
		new_rel = np[rel]
		if fs.handler == "cpp":
			olds = INCLUDE_RE.findall(old_text)
			news = INCLUDE_RE.findall(new_text)
			if len(olds) != len(news):
				out.append(("BLOCKING", "closure-alignment", rel, "include count changed %d -> %d" % (len(olds), len(news))))
				continue
			for qo, qn in zip(olds, news):
				angle = qo[0] == "<"
				res_o = include_resolve(old_tree if not angle else plan.tree_cs, parent_of(rel), qo[1:-1], angle)
				res_o = {r for r in res_o if not plan.hidden(r)}
				if not res_o:
					continue
				checked["include"] += 1
				res_n = include_resolve(new_tree, parent_of(new_rel), qn[1:-1], angle)
				want = {np[r] for r in res_o}
				if not res_n:
					out.append(("BLOCKING", "closure-include-broken", rel, "%s -> %s resolves to nothing on a case-sensitive tree" % (qo, qn)))
				elif not res_n <= want and not (res_n & want):
					out.append(("BLOCKING", "closure-include-rebinds", rel, "%s -> %s now binds %s" % (qo, qn, sorted(res_n)[:2])))
		elif fs.handler == "msbuild":
			if rel.endswith(".sln"):
				pat = re.compile(r'"([^"]+\.vcxproj)"')
			else:
				pat = re.compile(r'(?<!<Filter )(?:Include|Project)="([^"]+)"'
					r'|<Additional(?:Include|Library)Directories>([^<]*)<')
			olds = [m.group(1) or m.group(2) or "" for m in pat.finditer(old_text)]
			news = [m.group(1) or m.group(2) or "" for m in pat.finditer(new_text)]
			if len(olds) != len(news):
				out.append(("BLOCKING", "closure-alignment", rel, "Include count changed"))
				continue
			for po, pn in zip(olds, news):
				for po1, pn1 in zip(po.split(";"), pn.split(";")):
					po1, pn1 = po1.strip(), pn1.strip()
					if not po1 or "*" in po1 or "%(" in po1:
						continue
					bo, ro = expand_msbuild(po1, parent_of(rel), src_old)
					bn, rn = expand_msbuild(pn1, parent_of(new_rel), src_new)
					if bo is None or not ro:
						continue
					old_hit = tree_resolve(old_tree, bo, ro)
					if old_hit is None:
						continue
					checked["msbuild"] += 1
					new_hit = tree_resolve(new_tree, bn, rn)
					if new_hit != np[old_hit]:
						out.append(("BLOCKING", "closure-msbuild-broken", rel, "%s -> %s (case-sensitive: %s)" % (po1, pn1, new_hit)))
		elif fs.handler == "cmake":
			def cm_tokens(text):
				# source files, plus any ${VAR}/... or ../... path (include(), add_subdirectory(), include dirs)
				return re.findall(r"[^\s()\"]+\.(?:cpp|h|hpp|c|rc|cmake)\b|(?:\$\{[A-Za-z_]+\}|\.\.)/[^\s()\"]*", text)

			def cm_vars(text, fdir):
				v = {}
				for m in re.finditer(r"set\(\s*([A-Za-z_][A-Za-z0-9_]*)\s+([^\s)]+)\s*\)", text):
					v[m.group(1)] = m.group(2)
				return v

			def cm_expand(tok, fdir, vars_):
				for _ in range(4):
					tok = tok.replace("${CMAKE_CURRENT_SOURCE_DIR}", "@F@").replace("${CMAKE_CURRENT_LIST_DIR}", "@F@")
					tok = re.sub(r"\$\{([A-Za-z_][A-Za-z0-9_]*)\}", lambda m: vars_.get(m.group(1), "@X@"), tok)
				if "@X@" in tok or "$" in tok:
					return None, None
				if tok.startswith("@F@"):
					return fdir, tok[3:]
				return fdir, tok

			ov, nv = cm_vars(old_text, parent_of(rel)), cm_vars(new_text, parent_of(new_rel))
			olds, news = cm_tokens(old_text), cm_tokens(new_text)
			if len(olds) != len(news):
				out.append(("BLOCKING", "closure-alignment", rel, "CMake token count changed"))
				continue
			for to, tn in zip(olds, news):
				bo, ro = cm_expand(to, parent_of(rel), ov)
				bn, rn = cm_expand(tn, parent_of(new_rel), nv)
				if bo is None or bn is None:
					continue
				old_hit = tree_resolve(old_tree, bo, ro)
				if old_hit is None:
					continue
				checked["cmake"] += 1
				new_hit = tree_resolve(new_tree, bn, rn)
				if new_hit != np[old_hit]:
					out.append(("BLOCKING", "closure-cmake-broken", rel, "%s -> %s (case-sensitive: %s)" % (to, tn, new_hit)))
	# idempotency: planning the new tree must rename nothing
	new_files = {np[f] for f in plan.files}
	new_all = sorted(new_files | {np[d] for d in plan.dirs}, key=lambda r: (r.count("/"), r))
	new_roots = {k: tuple(np.get(r, r) for r in v) for k, v in plan.roots.items()}
	nd = dict(plan.decisions)
	# a decided name is final: the re-plan must keep it (CHANGELOG.md kept, renderer.h chosen, ...)
	nd["names"] = {base_of(np[r]): base_of(np[r]) for r, s in plan.seg.items() if r and s.reasons and s.decided}
	nd["paths"] = {}
	nd["accept_mixed_style"] = True
	n2 = Naming(new_all, new_files, nd, new_roots)
	for rel in new_all:
		r = n2.seg(rel)
		if r.cls not in plan.classes:
			continue
		if r.new != base_of(rel) and r.kind != "project-file":
			out.append(("BLOCKING", "non-idempotent-name", rel, "planner would rename again -> %s" % r.new))
	return checked


def run_verify_checks(plan):
	res = []  # (level, kind, where, detail)
	np = plan.newpath
	# duplicate targets and case-insensitive collisions among entries
	groups = defaultdict(list)
	for rel in np:
		if rel:
			groups[np[rel].casefold()].append(rel)
	for k, rels in groups.items():
		if len(rels) > 1:
			exact = len({np[r] for r in rels}) == 1
			res.append(("BLOCKING", "duplicate-target" if exact else "case-insensitive-collision", np[rels[0]],
				"sources: " + ", ".join(rels)))
	# collisions with on-disk entries outside the universe (ignored files) in the same folder
	for rel in plan.renamed:
		par = parent_of(rel)
		nb = base_of(np[rel])
		names = plan.disk.names(par)
		other = names.get(nb.casefold())
		if other and other != base_of(rel) and (par + "/" + other if par else other) not in np:
			res.append(("BLOCKING", "collision-with-unlisted-disk-entry", rel, "target %s collides with existing %s" % (nb, other)))
	# case-only renames
	case_only = [r for r in plan.renamed if base_of(r).casefold() == base_of(np[r]).casefold()]
	for r in case_only:
		res.append(("INFO", "case-only-rename", r, "-> %s (two-step move in git_mv_rename.sh)" % base_of(np[r])))
	for idx, real in plan.notes["index_case_mismatch"]:
		res.append(("WARN", "index-disk-case-mismatch", real, "index has only %s; the apply moves fix the index spelling" % idx))
	for dup in plan.notes["index_case_duplicates"]:
		res.append(("WARN", "index-case-duplicate-" + dup["kind"], dup["disk"] or dup["entries"][0],
			"index holds %s; disk has %s; clear the stale spelling first: %s" % (", ".join(dup["entries"]), dup["disk"],
			"; ".join(dup["commands"]))))
	# undecided ambiguous names
	for rel in sorted(plan.seg):
		s = plan.seg[rel]
		if s.reasons and not s.decided and s.cls in plan.classes:
			res.append(("DECISION", "ambiguous-name", rel, "proposal %s; %s" % (s.proposal, "; ".join(s.reasons))))
	# keyword / header / stdlib conflicts
	std_py = set(getattr(sys, "stdlib_module_names", ()))
	basenames_new = defaultdict(set)
	for rel in plan.files:
		if plan.naming.exclusion(rel):
			continue
		nb = base_of(np[rel])
		ext = ext_of(nb)
		stem = nb.rpartition(".")[0] if "." in nb else nb
		if ext in CPP_EXTS:
			if stem in CPP_KEYWORDS:
				res.append(("BLOCKING", "cpp-keyword-stem", rel, nb))
			if nb.casefold() in STD_HEADERS:
				already = base_of(rel).casefold() == nb.casefold()
				res.append(("BLOCKING" if not already else "WARN", "shadows-system-header", rel,
					"%s shadows <%s>%s" % (nb, nb, " (already true on Windows)" if already else "")))
			if ext in (".h", ".hpp"):
				basenames_new[nb].add(rel)
		if ext == ".py":
			if keyword.iskeyword(stem):
				res.append(("BLOCKING", "python-keyword-stem", rel, nb))
			if stem in std_py and base_of(rel)[:-3] != stem:
				res.append(("BLOCKING", "shadows-python-stdlib", rel, nb))
	for nb, rels in basenames_new.items():
		olds = {base_of(r) for r in rels}
		if len(rels) > 1 and len(olds) > 1:
			res.append(("WARN", "new-header-basename-clash", nb, "different old names now share it: " + ", ".join(sorted(rels))))
	# .filters pairing
	for rel in plan.files:
		if rel.lower().endswith(".vcxproj.filters"):
			proj = rel[: -len(".filters")]
			if proj in plan.files and np[proj] + ".filters" != np[rel]:
				res.append(("BLOCKING", "filters-pairing", rel, "%s vs %s" % (np[rel], np[proj])))
	# MSBuild project keys and target names
	layout = [r for r in plan.texts if base_of(r) in ("layout.props", "layout.cmake", "deploy.py", "build.ps1")
		or base_of(r) == "CMakeLists.txt" or r.endswith(".vcxproj")]
	for rel in sorted(plan.files):
		if rel.endswith(".vcxproj"):
			ok_key, nk = base_of(rel)[:-8].lower(), base_of(np[rel])[:-8].lower()
			txt = plan.texts.get(rel, "")
			for m in re.finditer(r"<(TargetName|ProjectName|RootNamespace|AdditionalDependencies)[^>]*>([^<]*)<", txt):
				res.append(("INFO", "msbuild-name-site", rel, "<%s> %s" % (m.group(1), m.group(2)[:120])))
			if ok_key != nk:
				sites = []
				for lr in layout:
					for i, line in enumerate(plan.texts[lr].split("\n"), 1):
						if re.search(r"(?<![A-Za-z0-9_])" + re.escape(ok_key) + r"(?![A-Za-z0-9_])", line, re.I):
							sites.append("%s:%d" % (lr, i))
				res.append(("BLOCKING", "project-key-change", rel, "HbProjectKey %s -> %s; also $(TargetName)/lib name; sites: %s"
					% (ok_key, nk, ", ".join(sites[:30]))))
	# residual scan: a renamed folder name that survives, exact-case and as a whole word, in the
	# post-edit text of a build/script/config file is either a miss or a label. Bounded list, review it.
	dir_names = sorted({base_of(r) for r in plan.renamed if r not in plan.files and not plan.naming.exclusion(r)}, key=len, reverse=True)
	if dir_names:
		rx = re.compile(r"(?<![A-Za-z0-9_])(" + "|".join(re.escape(n) for n in dir_names) + r")(?![A-Za-z0-9_])")
		for rel, fs in sorted(plan.scans.items()):
			if fs.report_only or fs.handler not in ("msbuild", "cmake", "script", "python", "gitignore", "json", "general"):
				continue
			new_text = apply_edits(fs.text, plan.edits_by_file.get(rel, []))
			for ln, line in enumerate(new_text.split("\n"), 1):
				for m in rx.finditer(line):
					res.append(("WARN", "residual-renamed-folder-name", "%s:%d" % (rel, ln),
						"%s survives after rewrite: %s" % (m.group(1), line.strip()[:140])))
	# byte preservation: every edited file re-encodes in its own codec/BOM, and keeps its line endings
	enc_checked = 0
	for rel, es in plan.edits_by_file.items():
		codec, bom = plan.codecs[rel]
		old_b = encode_text(plan.texts[rel], codec, bom)
		try:
			new_b = encode_text(apply_edits(plan.texts[rel], es), codec, bom)
		except UnicodeEncodeError as exc:
			res.append(("BLOCKING", "re-encode-failed", rel, "%s: %s" % (codec, asc(exc))))
			continue
		nl = "\n".encode(codec)
		crlf = "\r\n".encode(codec)
		if old_b.count(crlf) != new_b.count(crlf) or old_b.count(nl) != new_b.count(nl) or not new_b.startswith(bom):
			res.append(("BLOCKING", "line-endings-changed", rel, codec))
		enc_checked += 1
	res.append(("INFO", "byte-preservation-checked", "%d files" % enc_checked,
		"decode/re-encode round trip proven at scan; edited files keep codec, BOM and CRLF/LF counts"))
	# newly ignored paths (approximate gitignore matcher, Linux case-sensitive semantics)
	gi_old = plan.texts.get(".gitignore")
	ign = load_gitignore(gi_old) if gi_old is not None else None
	ign_new = load_gitignore(apply_edits(gi_old, plan.edits_by_file.get(".gitignore", []))) if gi_old is not None else None
	if ign:
		for rel, info in plan.files.items():
			if np[rel] == rel or info.origin == "ignored":
				continue
			if ign_new(np[rel]) and not ign(rel):
				res.append(("WARN" if not info.tracked else "INFO", "newly-ignored (approximate)", rel,
					"%s would match .gitignore%s" % (np[rel], "" if not info.tracked else " (tracked, stays tracked)")))
	# leftover on-disk entries inside renamed folders (they travel with the folder rename)
	leftovers = Counter()
	for rel in plan.renamed:
		if rel in plan.files:
			continue
		for low, name in plan.disk.names(rel).items():
			child = rel + "/" + name
			if child not in np:
				leftovers[rel] += 1
	for rel, n in sorted(leftovers.items()):
		res.append(("INFO", "unlisted-entries-travel-with-folder", rel, "%d on-disk entries not in the universe (ignored/build output)" % n))
	return res


def load_gitignore(gi):
	rules = []
	for raw in gi.split("\n"):
		line = raw.rstrip("\r").rstrip()
		if not line or line.startswith("#"):
			continue
		neg = line.startswith("!")
		if neg:
			line = line[1:]
		line = line.replace("\\ ", " ")
		dir_only = line.endswith("/")
		pat = line.rstrip("/")
		anchored = "/" in pat
		pat = pat.lstrip("/")
		rx = ""
		i = 0
		while i < len(pat):
			if pat.startswith("**/", i):
				rx += "(?:.*/)?"
				i += 3
			elif pat.startswith("/**", i) and i + 3 == len(pat):
				rx += "/.*"
				i += 3
			elif pat[i] == "*":
				rx += "[^/]*"
				i += 1
			elif pat[i] == "?":
				rx += "[^/]"
				i += 1
			elif pat[i] == "[":
				j = pat.find("]", i)
				if j < 0:
					rx += re.escape(pat[i])
					i += 1
				else:
					rx += pat[i:j + 1]
					i = j + 1
			else:
				rx += re.escape(pat[i])
				i += 1
		try:
			c = re.compile(("^" if anchored else "^(?:.*/)?") + rx + "$")
		except re.error:
			continue
		rules.append((c, neg, dir_only))

	def ignored(rel):
		parts = rel.split("/")
		state = False
		for n in range(1, len(parts) + 1):
			sub = "/".join(parts[:n])
			is_dir = n < len(parts)
			for c, neg, dir_only in rules:
				if dir_only and not is_dir:
					continue
				if c.match(sub):
					state = not neg
			if state and is_dir:
				return True
		return state

	return ignored


# --------------------------------------------------------------------------------------------------
# Reports
# --------------------------------------------------------------------------------------------------


def classify_rename(plan, rel):
	return ("dir" if rel not in plan.files else "file"), plan.seg[rel].cls


def dry_run_report(plan, steps):
	np = plan.newpath
	L = []
	L.append("=== Dry Run: rename_to_naming_rule.py ===")
	L.append("Run id: %s   (apply needs --i-have-reviewed %s)" % (plan.run_id, plan.run_id))
	L.append("Generated: %s   elapsed %.1fs" % (time.strftime("%Y-%m-%d %H:%M:%S"), plan.elapsed))
	L.append("Classes: %s   decisions file: %s (%s)" % (",".join(sorted(plan.classes)), plan.decisions_path,
		"loaded" if plan.decisions else "absent - proposals used"))
	n = plan.notes
	L.append("Universe: %d files, %d folders (tracked %d, untracked %d, ignored-under-.claude %d); %d tracked index entries missing on disk ignored"
		% (len(plan.files), len(plan.dirs), sum(1 for i in plan.files.values() if i.tracked), n["untracked"], n["ignored_claude"],
		n["tracked_missing_on_disk"]))
	L.append("Text files scanned: %d  encodings: %s" % (len(plan.texts), dict(plan.encoding_counts)))
	L.append("")
	# counts
	c = Counter()
	for rel in plan.renamed:
		c[classify_rename(plan, rel)] += 1
	case_only = sum(1 for r in plan.renamed if base_of(r).casefold() == base_of(np[r]).casefold())
	L.append("--- Rename counts (entries whose own name changes) ---")
	for (kind, cls), v in sorted(c.items()):
		L.append("  %-5s %-5s %5d" % (cls, kind, v))
	L.append("  total %d   case-only %d   full paths changed %d   move steps %d (two-step %d)"
		% (len(plan.renamed), case_only, sum(1 for r in np if r and np[r] != r), len(steps), sum(1 for s in steps if s["case_only"])))
	L.append("")
	# edits
	by_ext, by_rule, files_by_ext = Counter(), Counter(), defaultdict(set)
	for e in plan.edits:
		ext = ext_of(base_of(e["file"])) or base_of(e["file"])
		by_ext[ext] += 1
		files_by_ext[ext].add(e["file"])
		by_rule[e["rule"]] += 1
	L.append("--- Rewrite counts by file type ---")
	for ext, v in sorted(by_ext.items(), key=lambda x: -x[1]):
		L.append("  %-18s %6d rewrites in %4d files" % (ext, v, len(files_by_ext[ext])))
	L.append("  total %d rewrites in %d files" % (len(plan.edits), len(plan.edits_by_file)))
	L.append("--- Rewrite counts by rule ---")
	for r, v in sorted(by_rule.items(), key=lambda x: -x[1]):
		L.append("  %-28s %6d" % (r, v))
	L.append("  report-only hits (history/one-off scripts/metrics/settings, NOT written): %d in %d files"
		% (len(plan.report_only_hits), len({e["file"] for e in plan.report_only_hits})))
	L.append("")
	# exclusions and kept names
	excl = Counter()
	for rel in plan.files:
		ex = plan.naming.exclusion(rel)
		if ex:
			excl[(ex[0], ex[1])] += 1
	L.append("--- Excluded (names kept; still move with renamed parents; contents never rewritten) ---")
	for (k, root), v in sorted(excl.items()):
		L.append("  %-16s %-60s %5d files" % (k, root, v))
	L.append("--- Tool-required / kept names ---")
	kept = Counter()
	for rel, s in plan.seg.items():
		if s.kind in ("tool-required", "project-file", "dot-name"):
			kept[(s.kind, base_of(rel))] += 1
	for (k, name), v in sorted(kept.items()):
		why = TOOL_REQUIRED.get(name, "")
		if k == "project-file":
			why = "proposal keep: basename drives $(MSBuildProjectName)/$(TargetName)/HbProjectKey (decision)"
		if k == "dot-name":
			why = "leading-dot name (git / Claude Code)"
		L.append("  %-13s %-28s x%-3d %s" % (k, name, v, why))
	L.append("")
	# ambiguous
	L.append("--- Ambiguous names (proposal shown; need a decision) ---")
	amb = defaultdict(list)
	for rel, s in sorted(plan.seg.items()):
		if s.reasons and s.cls in plan.classes:
			amb[(base_of(rel), s.proposal, "; ".join(s.reasons), s.decided)].append(rel)
	high = [(k, v) for k, v in amb.items() if any(r != "mixed style" for r in k[2].split("; "))]
	low = [(k, v) for k, v in amb.items() if all(r == "mixed style" for r in k[2].split("; "))]
	L.append("  needs decision: %d names; mixed-style (low risk): %d names" % (len(high), len(low)))
	for (name, prop, why, dec), rels in sorted(high):
		L.append("  [%s] %-40s -> %-40s %s   (%d: %s)" % ("decided" if dec else "OPEN", name, prop, why, len(rels), rels[0]))
	L.append("  -- mixed style --")
	for (name, prop, why, dec), rels in sorted(low):
		L.append("  [%s] %-40s -> %-40s (%d: %s)" % ("decided" if dec else "OPEN", name, prop, len(rels), rels[0]))
	L.append("")
	# keyword check (dry-run spec)
	kw = [r for r in plan.renamed if base_of(np[r]).rpartition(".")[0] in CPP_KEYWORDS or keyword.iskeyword(base_of(np[r]).rpartition(".")[0])]
	L.append("--- Keyword check: %d rename targets are C++/Python keywords %s" % (len(kw), kw[:10]))
	L.append("")
	# per-entry summary / unused entries
	ref_count = Counter()
	for e in plan.edits:
		for t in e["touched"]:
			ref_count[t] += 1
	L.append("--- Per-entry reference summary (renamed entries, rewrites that touch them) ---")
	for rel in sorted(plan.renamed):
		L.append("  %5d  %s -> %s" % (ref_count[rel], rel, base_of(np[rel])))
	unused = [r for r in plan.renamed if ref_count[r] == 0]
	L.append("--- Unused entries (renamed, 0 rewritten references): %d ---" % len(unused))
	for r in unused:
		L.append("  %s -> %s" % (r, base_of(np[r])))
	L.append("")
	# full map
	L.append("--- Full rename map (every path that changes) ---")
	for rel in sorted(r for r in np if r and np[r] != r):
		s = plan.seg[rel]
		tag = "own" if base_of(np[rel]) != base_of(rel) else "parent"
		info = plan.files.get(rel)
		L.append("  %s -> %s   [%s %s %s %s]" % (rel, np[rel], s.cls, "dir" if info is None else "file", tag,
			("tracked" if info.tracked else info.origin) if info else ""))
	L.append("")
	L.append("--- Rewrites (file:line rule: old -> new) ---")
	for rel in sorted(plan.edits_by_file):
		es = sorted(plan.edits_by_file[rel], key=lambda e: e["start"])
		L.append("%s: %d rewrites" % (rel, len(es)))
		for e in es:
			L.append("  L%d %s: %s -> %s" % (e["line"], e["rule"], e["old"], e["new"]))
	L.append("")
	L.append("--- Report-only hits (would change, NOT written) ---")
	for e in sorted(plan.report_only_hits, key=lambda e: (e["file"], e["start"])):
		L.append("  %s:%d %s: %s -> %s" % (e["file"], e["line"], e["rule"], e["old"], e["new"]))
	L.append("")
	L.append("--- Summary ---")
	L.append("Would rename %d entries (%d paths change) and make %d rewrites across %d files. Nothing was changed."
		% (len(plan.renamed), sum(1 for r in np if r and np[r] != r), len(plan.edits), len(plan.edits_by_file)))
	return "\n".join(asc(x) for x in L) + "\n"


def verify_report(plan, checks, closure, checked):
	L = ["=== Verify: rename_to_naming_rule.py ===",
		"Run id: %s   generated %s" % (plan.run_id, time.strftime("%Y-%m-%d %H:%M:%S"))]
	allf = [(f["level"], f["kind"], "%s:%d" % (f["file"], f["line"]), "%s | %s" % (f["text"], f["detail"])) for f in plan.findings]
	allf += [(lv, k, w, d) for lv, k, w, d in checks]
	allf += [(lv, k, w, d) for lv, k, w, d in closure]
	for rel, line, path, status, detail in plan.cpp_unresolved_includes:
		if status != "ambiguous":
			allf.append(("INFO", "unresolved-include (system/generated/missing)", "%s:%d" % (rel, line), path))
	order = {"BLOCKING": 0, "DECISION": 1, "WARN": 2, "INFO": 3}
	counts = Counter((lv, k) for lv, k, _, _ in allf)
	L.append("Closure check: re-resolved %s on the renamed tree, case-sensitive" % dict(checked))
	L.append("")
	L.append("--- Counts ---")
	for (lv, k), v in sorted(counts.items(), key=lambda x: (order.get(x[0][0], 9), x[0][1])):
		L.append("  %-9s %-46s %6d" % (lv, k, v))
	L.append("")
	for lv in ("BLOCKING", "DECISION", "WARN", "INFO"):
		items = sorted([x for x in allf if x[0] == lv], key=lambda x: (x[1], x[2]))
		L.append("--- %s (%d) ---" % (lv, len(items)))
		for _, k, w, d in items:
			L.append("[%s] %s  %s" % (k, w, d))
		L.append("")
	blocking = sum(1 for x in allf if x[0] == "BLOCKING")
	decisions = sum(1 for x in allf if x[0] == "DECISION")
	L.append("--- Summary ---")
	L.append("%d blocking, %d decisions open, %d warnings, %d info. Apply requires 0 blocking and 0 open decisions"
		" (or --accept-proposed-ambiguous)." % (blocking, decisions, counts_level(allf, "WARN"), counts_level(allf, "INFO")))
	return "\n".join(asc(x) for x in L) + "\n", blocking, decisions, allf


def counts_level(allf, lv):
	return sum(1 for x in allf if x[0] == lv)


def write_run_files(plan, steps):
	d = RUN_ROOT / plan.run_id
	d.mkdir(parents=True, exist_ok=True)
	np = plan.newpath
	rows = ["old\tnew\tclass\tkind\tstatus\treasons"]
	for rel in sorted(r for r in np if r and np[r] != r):
		s = plan.seg[rel]
		info = plan.files.get(rel)
		rows.append("\t".join([rel, np[rel], s.cls, "dir" if info is None else "file",
			("tracked" if info.tracked else info.origin) if info else "-", "; ".join(s.reasons)]))
	write_text(d / "map.tsv", "\n".join(asc(r) for r in rows) + "\n")
	rows = ["file\tline\trule\told\tnew"]
	for e in sorted(plan.edits, key=lambda e: (e["file"], e["start"])):
		rows.append("\t".join([e["file"], str(e["line"]), e["rule"], e["old"], e["new"]]))
	write_text(d / "rewrites.tsv", "\n".join(asc(r) for r in rows) + "\n")
	rows = ["level\tkind\tfile\tline\ttext\tdetail"]
	for f in plan.findings:
		rows.append("\t".join([f["level"], f["kind"], f["file"], str(f["line"]), f["text"], f["detail"]]))
	write_text(d / "findings.tsv", "\n".join(asc(r) for r in rows) + "\n")
	tmpl = {"names": {}, "paths": {}, "accept_mixed_style": False, "_reasons": {}}
	for rel, s in sorted(plan.seg.items()):
		if s.reasons:
			tmpl["names"].setdefault(base_of(rel), s.proposal)
			tmpl["_reasons"].setdefault(base_of(rel), "; ".join(s.reasons) + " (e.g. " + rel + ")")
	write_text(d / "decisions_template.json", json.dumps(tmpl, indent=1, sort_keys=True) + "\n")
	write_text(d / "plan.json", json.dumps({
		"run_id": plan.run_id, "classes": sorted(plan.classes), "decisions": plan.decisions,
		"renames": {r: np[r] for r in sorted(np) if r and np[r] != r},
		"edit_files": {f: plan.shas[f] for f in sorted(plan.edits_by_file)},
		"edit_count": len(plan.edits),
	}, indent=1) + "\n")
	write_text(d / "moves.preview", move_preview(plan, steps, reverse=False))
	write_text(d / "moves_reverse.preview", move_preview(plan, steps, reverse=True))
	return d


# --------------------------------------------------------------------------------------------------
# Modes
# --------------------------------------------------------------------------------------------------


def do_dry_run(plan):
	steps = move_steps(plan)
	d = write_run_files(plan, steps)
	text = dry_run_report(plan, steps)
	write_text(DRY_LOG, text)
	write_text(d / "dry_run.log", text)
	say("Dry run %s: %d renames, %d rewrites in %d files, %d move steps." % (plan.run_id, len(plan.renamed),
		len(plan.edits), len(plan.edits_by_file), len(steps)))
	say("Log: %s" % relp(DRY_LOG))
	say("Run folder: %s" % relp(d))
	return 0


def do_verify(plan):
	steps = move_steps(plan)
	d = write_run_files(plan, steps)
	checks = run_verify_checks(plan)
	closure = []
	checked = closure_check(plan, closure)
	closure = [(lv, k, w, det) for lv, k, w, det in closure]
	text, blocking, decisions, allf = verify_report(plan, checks, closure, checked)
	write_text(VERIFY_LOG, text)
	write_text(d / "verify.log", text)
	write_text(d / "verify.json", json.dumps({"run_id": plan.run_id, "blocking": blocking, "decisions_open": decisions,
		"generated": time.strftime("%Y-%m-%d %H:%M:%S")}, indent=1) + "\n")
	say("Verify %s: %d blocking, %d open decisions, %d warnings, %d info." % (plan.run_id, blocking, decisions,
		counts_level(allf, "WARN"), counts_level(allf, "INFO")))
	say("Log: %s" % relp(VERIFY_LOG))
	return 0


def dir_size(path):
	total = 0
	for root, _, fns in os.walk(path):
		for fn in fns:
			try:
				total += os.path.getsize(os.path.join(root, fn))
			except OSError:
				pass
	return total


def do_apply(plan, reviewed, accept_proposed):
	if not reviewed:
		say("REFUSED: apply needs --i-have-reviewed <run-id> from a reviewed dry run.")
		return 2
	if reviewed != plan.run_id:
		say("REFUSED: the tree or decisions changed since run %s; the plan now hashes to %s. Re-run --dry-run and --verify,"
			" review, then pass the new id." % (reviewed, plan.run_id))
		return 2
	d = RUN_ROOT / plan.run_id
	vj = d / "verify.json"
	if not (d / "dry_run.log").is_file() or not vj.is_file():
		say("REFUSED: run %s has no dry_run.log and verify.json; run --dry-run and --verify first." % plan.run_id)
		return 2
	with open(vj, "r", encoding="utf-8") as f:
		v = json.load(f)
	if v.get("blocking", 1) != 0:
		say("REFUSED: verify for run %s reported %s blocking findings." % (plan.run_id, v.get("blocking")))
		return 2
	if v.get("decisions_open", 1) != 0 and not accept_proposed:
		say("REFUSED: %s ambiguous names are undecided; fill the decisions file or pass --accept-proposed-ambiguous."
			% v.get("decisions_open"))
		return 2
	# Every file must still be byte-identical to what was planned (two other agents may be editing).
	for rel in plan.edits_by_file:
		if hashlib.sha256((REPO / rel).read_bytes()).hexdigest() != plan.shas[rel]:
			say("REFUSED: %s changed during the run; nothing written." % rel)
			return 2
	if dir_size(REPO / ".claude") > 900 * 1024 * 1024:
		say("REFUSED: .claude/ is near its 1 GB limit; prune scratch before backing up.")
		return 2
	if (d / "journal.jsonl").exists():
		if (d / "rolled_back.json").exists():
			arch = archive_run_state(d)
			say("Previous attempt of run %s was rolled back completely; its journal and staging moved to %s."
				% (plan.run_id, relp(arch)))
		else:
			say("REFUSED: run %s already has a journal; use --resume %s or --rollback %s."
				% (plan.run_id, plan.run_id, plan.run_id))
			return 2
	steps = move_steps(plan)
	if not print_preflight(plan.run_id, *preflight(steps), d):
		say("REFUSED: preflight failed; nothing written.")
		return 2
	# Stage everything before touching the tree. Backups go to the run folder, not bak.py: bak.py puts
	# .bak_<guid> next to each file under Sources/, and the folder renames would carry or strand them.
	ops = []
	for rel, es in sorted(plan.edits_by_file.items()):
		codec, bom = plan.codecs[rel]
		new_b = encode_text(apply_edits(plan.texts[rel], es), codec, bom)
		for sub, data in (("backup", (REPO / rel).read_bytes()), ("new", new_b)):
			p = d / sub / rel
			p.parent.mkdir(parents=True, exist_ok=True)
			with open(p, "wb") as f:
				f.write(data)
		ops.append({"op": "rewrite", "rel": rel, "sha_before": plan.shas[rel], "sha_after": hashlib.sha256(new_b).hexdigest()})
	for st in steps:  # rewrites first (at the old paths), then the moves, deepest first
		ops.append(dict(st, op="move"))
	write_text(d / "apply_plan.json", json.dumps({"run_id": plan.run_id, "ops": ops}, indent=1) + "\n")
	return execute(d, ops)


def run_dir_for(run_id):
	d = RUN_ROOT / run_id
	if not re.fullmatch(r"[0-9a-f]{12}", run_id or "") or not (d / "apply_plan.json").is_file():
		say("REFUSED: run %s has no apply_plan.json (it was never applied)." % run_id)
		return None
	return d


def do_resume(run_id):
	d = run_dir_for(run_id)
	if d is None:
		return 2
	if (d / "applied.json").exists() or (d / "rolled_back.json").exists():
		say("REFUSED: run %s is already finished (applied.json or rolled_back.json exists)." % run_id)
		return 2
	ops = load_ops(d)
	done = Journal(d / "journal.jsonl").done()
	remaining = [op for i, op in enumerate(ops) if op["op"] == "move" and i not in done]
	if not print_preflight(run_id, *preflight(remaining, simulate=False), d):
		say("REFUSED: preflight failed; nothing changed.")
		return 2
	return execute(d, ops)


def do_rollback(run_id):
	d = run_dir_for(run_id)
	if d is None:
		return 2
	if (REPO / ".git" / "index.lock").exists():
		say("REFUSED: .git/index.lock exists.")
		return 2
	rc = rollback(d)
	say("Rollback of run %s %s." % (run_id, "complete" if rc == 0 else "finished WITH PROBLEMS - see above and journal.jsonl"))
	return rc


def do_check(plan):
	steps = move_steps(plan)
	d = RUN_ROOT / plan.run_id
	d.mkdir(parents=True, exist_ok=True)
	ok = print_preflight(plan.run_id, *preflight(steps), d)
	say("Preflight log: %s" % relp((d / "preflight.log")))
	return 0 if ok else 1


def main():
	try:
		sys.stdout.reconfigure(errors="replace")
	except Exception:
		pass
	ap = argparse.ArgumentParser(description="Phase R rename to the global naming rule (dry-run / verify / apply).")
	g = ap.add_mutually_exclusive_group(required=True)
	g.add_argument("--dry-run", action="store_true", help="Preview the rename map and rewrites; write logs only")
	g.add_argument("--verify", action="store_true", help="Collision, case, keyword, closure and reference checks")
	g.add_argument("--check", action="store_true", help="Read-only preflight: deleted index entries, duplicates, bak.py,"
		" Visual Studio / game processes, index.lock, move simulation")
	g.add_argument("--apply", action="store_true", help="Apply rewrites then moves (git mv), journaled; needs --i-have-reviewed")
	g.add_argument("--resume", metavar="RUN_ID", help="Continue an interrupted apply from its journal")
	g.add_argument("--rollback", metavar="RUN_ID", help="Undo an apply: reverse the moves, restore the rewritten files")
	ap.add_argument("--i-have-reviewed", metavar="RUN_ID", help="Run id of the reviewed dry run (required by --apply)")
	ap.add_argument("--classes", default=None,
		help="Rename classes to include: code,docs,data (default: the decisions file's \"classes\", else all)")
	ap.add_argument("--decisions", default=str(DEFAULT_DECISIONS), help="Decisions JSON for ambiguous names")
	ap.add_argument("--accept-proposed-ambiguous", action="store_true", help="Apply: accept proposals for open decisions")
	args = ap.parse_args()
	if args.resume:
		return do_resume(args.resume)
	if args.rollback:
		return do_rollback(args.rollback)
	if args.classes is None:
		classes = set(load_decisions(args.decisions).get("classes") or ALL_CLASSES)
	else:
		classes = {c.strip() for c in args.classes.split(",") if c.strip()}
	bad = classes - set(ALL_CLASSES)
	if bad:
		say("Unknown classes: %s" % ", ".join(sorted(bad)))
		return 2
	plan = build_plan(classes, args.decisions)
	if args.dry_run:
		return do_dry_run(plan)
	if args.verify:
		return do_verify(plan)
	if args.check:
		return do_check(plan)
	return do_apply(plan, args.i_have_reviewed, args.accept_proposed_ambiguous)


if __name__ == "__main__":
	sys.exit(main())
