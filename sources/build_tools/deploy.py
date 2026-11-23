"""Assemble game/client and game/server from bin/ and staged/ (Phase H deploy step).

Usage:
	python deploy.py --target client|server|all --config Debug|Release|ReleaseWithDebugInfo
	                 [--platform windows|linux] [--dry-run] [--no-prune] [--full-check]
	                 [--game-root <dir>] [--build-databases]

Run by sources/build.ps1 and the build_*_linux.sh scripts after a successful build.

Sources, per target (relative to the repo root):
	client  bin/client/<config>/<exe>     -> game/client/
	        staged/client/**              -> game/client/**
	        staged/shared/mapdata/**      -> game/client/mapdata/**   (game.cpp loads "mapdata/<map>.amd")
	server  bin/server/<config>/<exe>     -> game/server/
	        staged/server/**              -> game/server/**
	        staged/shared/mapdata/**      -> game/server/mapdata/**   (map.cpp loads "mapdata/<map>.amd")
	        generated DBs                 -> game/server/              (hook, disabled until Phase E lands)

Safety rules - game/ also holds runtime state that a deploy must never lose:
	* Only files planned from bin/ or staged/ are ever written. Anything else in game/ is never touched.
	* Protected paths (account DBs, every *.db, logs, settings.json, server_config.json, gameconfigs/,
	  GameData/, cache/, save/) are never written, never deleted, whatever bin/ or staged/ contain.
	* Every relative path (planned or read from the manifest) must be a plain forward-slash path inside
	  game/<target>/: no absolute path, drive, backslash, empty/./.. segment, trailing dot or space, 8.3
	  short-name alias (GAMECO~1) or device name. A manifest holding any other key is refused whole.
	* Before every write and every delete the real (resolved) location must lie inside game/<target>/ and
	  must not be protected, so a junction or symlink inside game/ can never redirect a write or a delete.
	* A file is copied only when its content differs. A file whose content deploy did not put there
	  (unmanaged, or edited since the last deploy) is backed up under game/<target>/.deploy/backup/ first;
	  the last BACKUP_KEEP backup runs are kept.
	* Seed-only files (written by the program at runtime, e.g. the client's news cache contents1000.txt)
	  are copied only when missing.
	* Deploy removes a file only when its manifest (game/<target>/.deploy/manifest.json) says deploy put
	  it there, its content is still exactly what deploy wrote, and its source group (staged, bin-windows,
	  bin-linux) was deployed in this run - so a Windows deploy never removes the Linux binary.
	* Writes go to a temp file and are renamed into place; a locked exe (the game is running) is reported
	  as an error and left alone.
	* One deploy per target at a time: game/<target>/.deploy/lock holds the owner's PID. A stale lock is
	  reported with that PID and never removed automatically.
--dry-run prints the full plan and writes nothing (it takes no lock; it warns when a lock is present).

ASCII output only. Exit code 0 on success, 1 when any file failed, 2 when the plan could not be made.
"""

import argparse
import datetime
import hashlib
import json
import os
import re
import shutil
import stat as stat_mod
import subprocess
import sys
import tempfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_GAME_ROOT = REPO_ROOT / "game"
DEFAULT_BIN_ROOT = REPO_ROOT / "bin"
STAGED_ROOT = REPO_ROOT / "staged"

MANIFEST_VERSION = 1
STATE_DIR = ".deploy"
MANIFEST_NAME = "manifest.json"
LOCK_NAME = "lock"
BACKUP_DIR = "backup"
BACKUP_KEEP = 5
TEMP_SUFFIX = ".deploy_tmp"
HASH_CHUNK = 1 << 20
MANIFEST_GROUPS = ("staged", "bin-windows", "bin-linux")

CONFIGS = ("Debug", "Release", "ReleaseWithDebugInfo")
PLATFORMS = ("windows", "linux")

# ---------------------------------------------------------------------------------------------------------
# DATABASE HOOK (Phase E). Disabled until sources/database/build_databases.py is finished and reviewed.
# To enable: set DATABASE_HOOK_ENABLED = True; then `--build-databases` runs the generator into game/server.
# The generator has its own guard against overwriting a DB that changed since it generated it; deploy
# never passes --force, so that guard always applies. Deploy itself never copies or touches a *.db file.
DATABASE_HOOK_ENABLED = False
DATABASE_SCRIPT = REPO_ROOT / "sources" / "database" / "build_databases.py"
# ---------------------------------------------------------------------------------------------------------

# Paths are matched case-insensitively, relative to game/<target>/, with forward slashes.
# Whole top-level folders that hold runtime state.
PROTECTED_DIRS = {
	"client": {"cache", "save", "logs", STATE_DIR},
	"server": {"accounts", "gamelogs", "gameconfigs", "gamedata", "logs", STATE_DIR},
}
# Individual files (relative paths) that the programs generate and keep between runs.
PROTECTED_FILES = {
	"client": {"settings.json"},
	"server": {"server_config.json"},
}
# Any file with these endings, anywhere: databases (account DBs hold password hashes) and logs.
PROTECTED_SUFFIXES = (".db", ".db-journal", ".db-wal", ".db-shm", ".log")
# Shipped in staged/ but rewritten by the program at runtime: copied only when missing, never updated or
# removed. contents1000.txt is the client's cache of the server's news text (game.cpp noticement_handler).
SEED_ONLY = {
	"client": {"contents/contents1000.txt"},
	"server": set(),
}

# Executables taken from bin/<target>/<config>/. Only these names are ever deployed from bin/, so no
# .lib/.exp/.ilk reaches game/. Each entry is (alternative names, required); exactly one alternative may
# exist. The Linux client name follows sources/client/CMakeLists.txt: Game_<arch>_linux, where arch is x64
# for a 64-bit build and x86 otherwise.
BIN_FILES = {
	("client", "windows"): ((("Game_x64_win.exe",), True), (("Game_x64_win.pdb",), False)),
	("client", "linux"): ((("Game_x64_linux", "Game_x86_linux"), True),),
	("server", "windows"): ((("Server.exe",), True), (("Server.pdb",), False)),
	("server", "linux"): ((("HelbreathServer",), True),),
}

# (source folder under staged/, destination prefix under game/<target>/)
STAGED_SOURCES = {
	"client": (("client", ""), ("shared/mapdata", "mapdata")),
	"server": (("server", ""), ("shared/mapdata", "mapdata")),
}

# Windows resolves these names to devices, whatever the folder or extension.
DEVICE_NAMES = {"con", "prn", "aux", "nul"} | {"com%d" % i for i in range(1, 10)} | {"lpt%d" % i for i in range(1, 10)}
# An 8.3 short name (GAMECO~1) is a second spelling of a long name; it would slip past the protected list.
SHORT_NAME_RE = re.compile(r"~[0-9]")
BACKUP_STAMP_RE = re.compile(r"^[0-9]{8}_[0-9]{6}(_[0-9]{1,3})?$")


class PlanError(Exception):
	pass


def say(text=""):
	"""Print ASCII only: the console is cp1252 and a stray character must not abort a deploy."""
	print(str(text).encode("ascii", "replace").decode("ascii"), flush=True)


def sha256_of(path):
	digest = hashlib.sha256()
	with open(path, "rb") as handle:
		for chunk in iter(lambda: handle.read(HASH_CHUNK), b""):
			digest.update(chunk)
	return digest.hexdigest()


def repo_relative(path):
	"""Path relative to the repo when it is inside it (--bin-root may point elsewhere)."""
	try:
		return path.relative_to(REPO_ROOT).as_posix()
	except ValueError:
		return path.as_posix()


# ---------------------------------------------------------------------------------------------------------
# Path safety
# ---------------------------------------------------------------------------------------------------------

def rel_problem(rel):
	"""Why `rel` is not a plain relative path inside a target folder, or None when it is one."""
	if not isinstance(rel, str) or not rel:
		return "empty or not text"
	if "\\" in rel:
		return "contains a backslash"
	if ":" in rel:
		return "contains a drive or stream separator ':'"
	if rel.startswith("/"):
		return "is absolute"
	for segment in rel.split("/"):
		if segment in ("", ".", ".."):
			return "has an empty, '.' or '..' segment"
		if segment != segment.strip() or segment.endswith("."):
			return "has a segment with a trailing dot or surrounding space"
		if SHORT_NAME_RE.search(segment):
			return "has a segment that looks like an 8.3 short name (~digit)"
		if segment.split(".", 1)[0].lower() in DEVICE_NAMES:
			return "has a device-name segment"
		if any(ord(c) < 32 for c in segment):
			return "has a control character"
	return None


def is_protected(target, rel):
	"""Runtime state? `rel` is normalized first, so './x' or 'a/../x' cannot dodge the list."""
	low = os.path.normpath(rel).replace("\\", "/").lower()
	while low.startswith("./"):
		low = low[2:]
	if low in PROTECTED_FILES[target]:
		return True
	if "/" in low and low.split("/", 1)[0] in PROTECTED_DIRS[target]:
		return True
	return low.endswith(PROTECTED_SUFFIXES)


def is_seed_only(target, rel):
	return rel.lower() in SEED_ONLY[target]


def _norm(path):
	return os.path.normcase(os.path.abspath(str(path)))


# FILE_ATTRIBUTE_REPARSE_POINT: junctions, symlinks and every other Windows redirection carry it. Checked
# directly (not os.path.isjunction, which only exists from Python 3.12), so no Python version follows one.
_REPARSE_POINT = getattr(stat_mod, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400)


def is_link_like(path):
	"""True for a symlink, a junction or any other reparse point; never follows it."""
	try:
		st = os.lstat(str(path))
	except OSError:
		return False
	if stat_mod.S_ISLNK(st.st_mode):
		return True
	return bool(getattr(st, "st_file_attributes", 0) & _REPARSE_POINT)


def remove_link(path):
	"""Remove a link itself (file symlink, directory symlink or junction), never what it points at."""
	try:
		os.unlink(str(path))
	except (IsADirectoryError, PermissionError):
		os.rmdir(str(path))


def write_exclusive(src, dest):
	"""Copy src to a NEW file dest: O_CREAT|O_EXCL refuses an existing file or a planted link."""
	flags = os.O_CREAT | os.O_EXCL | os.O_WRONLY | getattr(os, "O_BINARY", 0) | getattr(os, "O_NOFOLLOW", 0)
	fd = os.open(str(dest), flags, 0o600)
	with os.fdopen(fd, "wb") as out, open(src, "rb") as inp:
		shutil.copyfileobj(inp, out, HASH_CHUNK)
	shutil.copystat(src, dest, follow_symlinks=False)


def replace_from_temp(dest, fill, stat_from=None):
	"""Write dest atomically: fill(handle) writes a fresh mkstemp file next to dest, which then replaces it.

	mkstemp creates its file with O_CREAT|O_EXCL under a random name, so nothing planted in advance can be
	written through. Before the rename the temp must still be the very file we created (same file id, not a
	link), so a link swapped in meanwhile can never be installed as dest. stat_from copies mtime and mode.
	"""
	fd, tmp = tempfile.mkstemp(dir=str(dest.parent), prefix=dest.name + ".", suffix=TEMP_SUFFIX)
	try:
		with os.fdopen(fd, "wb") as out:
			fill(out)
			out.flush()
			os.fsync(out.fileno())
			created = os.fstat(out.fileno())
		if stat_from is not None:
			shutil.copystat(str(stat_from), tmp, follow_symlinks=False)
		now = os.lstat(tmp)
		if is_link_like(tmp) or (now.st_dev, now.st_ino) != (created.st_dev, created.st_ino):
			raise OSError("temp file %s was replaced during the copy; refusing to install it" % tmp)
		os.replace(tmp, str(dest))
	finally:
		if os.path.lexists(tmp):
			if is_link_like(tmp):
				remove_link(tmp)
			else:
				os.unlink(tmp)


def is_inside(path, root_real):
	"""True when `path`, fully resolved (junctions and symlinks followed), lies inside `root_real`."""
	real = _norm(Path(path).resolve())
	return real == root_real or real.startswith(root_real.rstrip("\\/") + os.sep)


def real_rel(path, root_real):
	"""Relative path of the resolved `path` under `root_real` (forward slashes); None if outside."""
	real = os.path.abspath(str(Path(path).resolve()))
	if not is_inside(real, root_real):
		return None
	return os.path.relpath(real, root_real).replace("\\", "/")


def check_target(target, root, root_real, rel, for_write):
	"""Problem text if writing/deleting root/rel is unsafe right now, else None. Run just before acting."""
	problem = rel_problem(rel)
	if problem:
		return "path %s" % problem
	dest = root / rel
	# The deepest folder that exists must resolve inside the target: a junction anywhere on the way
	# (game/client/mapdata -> C:\elsewhere) would otherwise carry the write or the delete out of game/.
	parent = dest.parent
	while not parent.exists() and parent != root:
		parent = parent.parent
	if not is_inside(parent, root_real):
		return "its folder resolves outside %s (junction or symlink)" % root
	if os.path.lexists(dest):
		if is_link_like(dest):
			return "it is a symlink, junction or other reparse point"
		if not dest.is_file():
			return "it exists and is not a regular file"
		real = real_rel(dest, root_real)
		if real is None:
			return "it resolves outside %s" % root
		if is_protected(target, real):
			return "it resolves to protected runtime state (%s)" % real
	elif for_write:
		real_parent = real_rel(parent, root_real)
		if real_parent is not None and real_parent != ".":
			tail = dest.relative_to(parent).as_posix()
			if is_protected(target, real_parent + "/" + tail):
				return "it would land in protected runtime state (%s/%s)" % (real_parent, tail)
	return None


def safe_mkdirs(folder, root_real):
	"""Create `folder` one level at a time, checking each level still resolves inside the target."""
	missing = []
	probe = folder
	while not probe.exists():
		missing.append(probe)
		probe = probe.parent
	if not is_inside(probe, root_real):
		raise OSError("refusing to create %s: %s resolves outside the target" % (folder, probe))
	for level in reversed(missing):
		level.mkdir()
		if not is_inside(level, root_real):
			raise OSError("refusing to use %s: it resolves outside the target" % level)


def remove_tree(path):
	"""Delete a folder deploy created. Links inside are removed as links, never followed."""
	if is_link_like(path):
		raise OSError("refusing to delete through %s: it is a link or reparse point" % path)
	for entry in os.scandir(path):
		full = entry.path
		if is_link_like(full):
			remove_link(full)
		elif entry.is_dir(follow_symlinks=False):
			remove_tree(full)
		else:
			os.unlink(full)
	os.rmdir(path)


# ---------------------------------------------------------------------------------------------------------
# Planning
# ---------------------------------------------------------------------------------------------------------

class FileStat:
	"""Size + mtime + lazily computed sha256 of one file."""

	def __init__(self, path):
		self.path = path
		st = path.stat()
		self.size = st.st_size
		self.mtime_ns = st.st_mtime_ns
		self._sha = None

	@property
	def sha(self):
		if self._sha is None:
			self._sha = sha256_of(self.path)
		return self._sha


def collect_sources(target, config, platform, bin_root):
	"""dest rel path -> (source path, group). Raises PlanError on a missing build or a name clash."""
	sources = {}
	seen_lower = {}

	def add(rel, src, group):
		problem = rel_problem(rel)
		if problem:
			raise PlanError("source %s maps to %s/%s, which %s" % (src, target, rel, problem))
		low = rel.lower()
		if low in seen_lower:
			raise PlanError("two sources map to %s/%s: %s and %s" % (target, rel, seen_lower[low], src))
		seen_lower[low] = src
		sources[rel] = (src, group)

	bin_dir = bin_root / target / config
	for names, required in BIN_FILES[(target, platform)]:
		found = [name for name in names if (bin_dir / name).is_file()]
		if len(found) > 1:
			raise PlanError("%s has more than one of %s; remove the stale one" % (bin_dir, ", ".join(names)))
		if found:
			add(found[0], bin_dir / found[0], "bin-" + platform)
		elif required:
			raise PlanError("%s is not built: %s is missing (build %s %s first)" % (
				target, bin_dir / names[0], target, config))

	for folder, prefix in STAGED_SOURCES[target]:
		base = STAGED_ROOT / folder
		if not base.is_dir():
			continue
		for dirpath, dirnames, filenames in os.walk(base):
			dirnames[:] = sorted(d for d in dirnames if not d.startswith(".git"))
			for name in sorted(filenames):
				if name.startswith(".git"):
					continue
				src = Path(dirpath) / name
				rel = src.relative_to(base).as_posix()
				if prefix:
					rel = prefix + "/" + rel
				add(rel, src, "staged")
	return sources


def load_manifest(state_dir):
	"""The manifest, validated whole: one bad key or entry and deploy refuses to trust any of it."""
	path = state_dir / MANIFEST_NAME
	if is_link_like(state_dir) or is_link_like(path):
		raise PlanError("%s or its folder is a link or reparse point; deploy will not read through it" % path)
	if not path.exists():
		return {"version": MANIFEST_VERSION, "groups": {}, "backups": []}
	try:
		with open(path, encoding="utf-8") as handle:
			data = json.load(handle)
	except (OSError, ValueError) as exc:
		raise PlanError("cannot read %s (%s); fix or move it, deploy will not guess what it owns" % (path, exc))
	if not isinstance(data, dict) or data.get("version") != MANIFEST_VERSION:
		raise PlanError("%s has an unknown format; fix or move it" % path)
	groups = data.get("groups")
	if not isinstance(groups, dict):
		raise PlanError("%s has no valid 'groups'; fix or move it" % path)
	for group, entries in groups.items():
		if group not in MANIFEST_GROUPS or not isinstance(entries, dict):
			raise PlanError("%s: unknown or malformed group %r; fix or move it" % (path, group))
		for rel, entry in entries.items():
			problem = rel_problem(rel)
			if problem:
				raise PlanError("%s: key %r %s; refusing the whole manifest" % (path, rel, problem))
			if not isinstance(entry, dict) or not isinstance(entry.get("sha256"), str):
				raise PlanError("%s: entry %r is not a valid record; refusing the whole manifest" % (path, rel))
	backups = data.get("backups", [])
	if not isinstance(backups, list) or not all(isinstance(b, str) and BACKUP_STAMP_RE.match(b) for b in backups):
		raise PlanError("%s: malformed 'backups' list; fix or move it" % path)
	data["backups"] = backups
	return data


class Plan:
	def __init__(self, target, root):
		self.target = target
		self.root = root
		self.root_real = _norm(root.resolve())
		self.actions = []          # (kind, rel, detail) for reporting
		self.copies = []           # (rel, src, group, src_stat, backup_needed)
		self.removals = []         # (rel, group)
		self.manifest_drops = []   # (group, rel): entries no longer owned
		self.adopt = []            # (rel, src, group, src_stat): identical files recorded without copying

	def note(self, kind, rel, detail=""):
		self.actions.append((kind, rel, detail))


def make_plan(target, sources, manifest, root, groups_run, prune, full_check):
	plan = Plan(target, root)
	owned = manifest["groups"]

	def manifest_entry(group, rel):
		return owned.get(group, {}).get(rel)

	for rel in sorted(sources):
		src, group = sources[rel]
		dest = root / rel
		if is_protected(target, rel):
			plan.note("protected", rel, "source %s is never deployed over runtime state" % repo_relative(src))
			continue
		unsafe = check_target(target, root, plan.root_real, rel, True)
		if unsafe:
			plan.note("unsafe", rel, "skipped: %s" % unsafe)
			continue
		src_stat = FileStat(src)
		if not dest.exists():
			plan.copies.append((rel, src, group, src_stat, False))
			plan.note("new", rel)
			continue
		if is_seed_only(target, rel):
			plan.note("seed-kept", rel, "rewritten by the program at runtime; copied only when missing")
			continue
		dest_stat = FileStat(dest)
		entry = None
		for g in owned:
			if rel in owned[g]:
				entry = owned[g][rel]
				break
		# Fast path: both sides exactly as recorded at the last deploy, so their contents still match.
		if (not full_check and entry and entry.get("src_size") == src_stat.size
				and entry.get("src_mtime_ns") == src_stat.mtime_ns and entry.get("size") == dest_stat.size
				and entry.get("mtime_ns") == dest_stat.mtime_ns and manifest_entry(group, rel) is not None):
			plan.note("unchanged", rel)
			continue
		if src_stat.size == dest_stat.size and src_stat.sha == dest_stat.sha:
			plan.adopt.append((rel, src, group, src_stat))
			if manifest_entry(group, rel) is None:
				plan.note("adopt", rel, "already identical; now recorded as deployed")
			else:
				plan.note("unchanged", rel)
			continue
		# Content differs. Back it up unless it is exactly what deploy wrote last time.
		ours = entry is not None and entry.get("sha256") == dest_stat.sha
		plan.copies.append((rel, src, group, src_stat, not ours))
		plan.note("update", rel, "" if ours else "current file is not what deploy last wrote; backed up first")

	# Manifest entries of the groups deployed in this run that are no longer planned. Compared
	# case-insensitively: on Windows a case-only rename in staged/ is the SAME file on disk, so the old
	# spelling must only be forgotten, never unlinked.
	planned_lower = {rel.lower(): rel for rel in sources}
	for group in groups_run:
		for rel, entry in sorted(owned.get(group, {}).items()):
			current = planned_lower.get(rel.lower())
			if current == rel and sources[rel][1] == group:
				continue
			if current is not None:
				# renamed by case only, or moved to another group: the planned entry owns the file now
				plan.manifest_drops.append((group, rel))
				continue
			dest = root / rel
			if is_protected(target, rel) or is_seed_only(target, rel):
				plan.manifest_drops.append((group, rel))
				plan.note("release", rel, "protected or seed-only; kept, no longer tracked")
				continue
			if not os.path.lexists(dest):
				plan.manifest_drops.append((group, rel))
				plan.note("forget", rel, "already gone")
				continue
			unsafe = check_target(target, root, plan.root_real, rel, False)
			if unsafe:
				plan.manifest_drops.append((group, rel))
				plan.note("unsafe", rel, "not removed: %s; no longer tracked" % unsafe)
				continue
			if not prune:
				plan.note("stale", rel, "no longer in %s; kept (--no-prune)" % group)
				continue
			if sha256_of(dest) == entry.get("sha256"):
				plan.removals.append((rel, group))
				plan.note("remove", rel, "deployed earlier from %s, no longer in it, unmodified" % group)
			else:
				plan.manifest_drops.append((group, rel))
				plan.note("kept-modified", rel, "no longer in %s but edited since deploy; kept, no longer tracked" % group)
	return plan


# ---------------------------------------------------------------------------------------------------------
# Execution
# ---------------------------------------------------------------------------------------------------------

def atomic_copy(src, dest, root_real):
	safe_mkdirs(dest.parent, root_real)

	def fill(out):
		with open(src, "rb") as inp:
			shutil.copyfileobj(inp, out, HASH_CHUNK)

	replace_from_temp(dest, fill, stat_from=src)


def temp_names(name):
	"""Temp file names deploy may have left for `name`: the fixed legacy one and mkstemp's random ones."""
	return re.compile("^" + re.escape(name) + r"(\.[A-Za-z0-9_]{6,16})?" + re.escape(TEMP_SUFFIX) + "$")


def clear_stale_temps(plan):
	"""Clear leftovers of an interrupted deploy next to each planned file (and the manifest).

	Only names deploy itself uses (<file>.deploy_tmp and <file>.<random>.deploy_tmp) are considered. A link
	or reparse point is removed as a link (never followed); a plain file is removed; anything else (a
	folder) is refused: the planned file is not written and the run fails. Returns the refused rels.
	"""
	by_folder = {}
	for rel in [rel for rel, _, _, _, _ in plan.copies] + [STATE_DIR + "/" + MANIFEST_NAME]:
		folder, _, name = rel.rpartition("/")
		by_folder.setdefault(folder, []).append((rel, name))
	refused = set()
	for folder, entries in sorted(by_folder.items()):
		path = plan.root / folder if folder else plan.root
		if not path.is_dir() or is_link_like(path) or not is_inside(path, plan.root_real):
			continue
		listing = os.listdir(str(path))
		for rel, name in entries:
			pattern = temp_names(name)
			for leftover in sorted(n for n in listing if pattern.match(n)):
				full = path / leftover
				shown = (folder + "/" if folder else "") + leftover
				if is_link_like(full):
					remove_link(full)
					say("  cleared       %s  - a planted link; removed the link only" % shown)
				elif full.is_file():
					full.unlink()
					say("  cleared       %s  - left by an interrupted deploy" % shown)
				else:
					refused.add(rel)
					say("  ERROR         %s  - not a file or link; %s not written" % (shown, rel))
	return refused


def new_backup_stamp(backup_root, stamp):
	"""A backup folder name that does not exist yet: the timestamp, then _1, _2 ... on a clash."""
	name = stamp
	n = 0
	while os.path.lexists(backup_root / name):
		n += 1
		name = "%s_%d" % (stamp, n)
	return name


def prune_backups(manifest, backup_root, root_real):
	"""Keep the newest BACKUP_KEEP backup runs that deploy recorded; delete older ones by exact name."""
	names = manifest["backups"]
	keep = names[-BACKUP_KEEP:]
	for name in names[:-BACKUP_KEEP]:
		folder = backup_root / name
		if not os.path.lexists(folder):
			continue
		if is_link_like(folder) or not folder.is_dir() or not is_inside(folder, root_real):
			say("  WARNING       backup %s is not a plain folder inside the target; left alone" % name)
			continue
		try:
			remove_tree(str(folder))
			say("  pruned        backup %s (keeping the last %d)" % (name, BACKUP_KEEP))
		except OSError as exc:
			say("  WARNING       could not prune backup %s: %s" % (name, exc))
			keep.insert(0, name)
	manifest["backups"] = keep


def execute(plan, manifest, stamp, config, platform):
	errors = []
	root = plan.root
	root_real = plan.root_real
	state_dir = root / STATE_DIR
	backup_root = state_dir / BACKUP_DIR
	backup_name = None
	owned = manifest["groups"]

	refused = clear_stale_temps(plan)
	for rel in sorted(refused):
		errors.append((rel, "a non-file temp leftover blocks it; remove it by hand"))

	def record(group, rel, src, src_stat):
		dest = root / rel
		st = dest.stat()
		for other in list(owned):
			if other != group:
				owned[other].pop(rel, None)
		owned.setdefault(group, {})[rel] = {
			"sha256": src_stat.sha,
			"size": st.st_size,
			"mtime_ns": st.st_mtime_ns,
			"src": repo_relative(src),
			"src_size": src_stat.size,
			"src_mtime_ns": src_stat.mtime_ns,
			"config": config if group.startswith("bin-") else "",
		}

	for rel, src, group, src_stat in plan.adopt:
		record(group, rel, src, src_stat)

	for rel, src, group, src_stat, backup in plan.copies:
		dest = root / rel
		if rel in refused:
			continue
		# Re-checked right before acting: game/ may have changed since the plan was made.
		unsafe = check_target(plan.target, root, root_real, rel, True)
		if unsafe:
			errors.append((rel, "skipped: %s" % unsafe))
			continue
		try:
			if backup:
				if backup_name is None:
					backup_name = new_backup_stamp(backup_root, stamp)
				saved = backup_root / backup_name / rel
				safe_mkdirs(saved.parent, root_real)
				write_exclusive(dest, saved)
			atomic_copy(src, dest, root_real)
			record(group, rel, src, src_stat)
		except OSError as exc:
			errors.append((rel, "copy failed: %s" % exc))

	for rel, group in plan.removals:
		unsafe = check_target(plan.target, root, root_real, rel, False)
		if unsafe:
			errors.append((rel, "not removed: %s" % unsafe))
			owned.get(group, {}).pop(rel, None)
			continue
		try:
			(root / rel).unlink()
			owned.get(group, {}).pop(rel, None)
		except OSError as exc:
			errors.append((rel, "remove failed: %s" % exc))

	for group, rel in plan.manifest_drops:
		owned.get(group, {}).pop(rel, None)

	if backup_name is not None:
		manifest["backups"].append(backup_name)
	prune_backups(manifest, backup_root, root_real)

	manifest["groups"] = {g: entries for g, entries in owned.items() if entries}
	manifest["last_deploy"] = {"time": stamp, "config": config, "platform": platform}
	safe_mkdirs(state_dir, root_real)
	payload = json.dumps(manifest, indent=1, sort_keys=True).encode("utf-8") + b"\n"
	replace_from_temp(state_dir / MANIFEST_NAME, lambda out: out.write(payload))
	return errors


def report(plan, dry_run, verbose):
	counts = {}
	for kind, _, _ in plan.actions:
		counts[kind] = counts.get(kind, 0) + 1
	order = ("new", "update", "remove", "adopt", "unchanged", "seed-kept", "protected", "unsafe",
		"kept-modified", "stale", "release", "forget")
	summary = ", ".join("%s %d" % (k, counts[k]) for k in order if k in counts) or "nothing to do"
	say("deploy %s -> %s%s" % (plan.target, plan.root, " (dry run, nothing written)" if dry_run else ""))
	say("  " + summary)
	quiet = () if verbose else ("unchanged",)
	for kind, rel, detail in plan.actions:
		if kind in quiet:
			continue
		say("  %-13s %s%s" % (kind, rel, ("  - " + detail) if detail else ""))


def run_database_hook(game_root, channel, dry_run):
	"""Phase E hook point: generate gamedata.db / mapinfo.db into game/server. Disabled by default."""
	if not DATABASE_HOOK_ENABLED:
		say("database hook: disabled (DATABASE_HOOK_ENABLED = False in deploy.py); DBs left as they are")
		return 0
	command = [sys.executable, str(DATABASE_SCRIPT), "--db", "all", "--channel", channel,
		"--output-dir", str(game_root / "server")]
	if dry_run:
		say("database hook (dry run): would run " + " ".join(command))
		return 0
	say("database hook: " + " ".join(command))
	return subprocess.call(command)


# ---------------------------------------------------------------------------------------------------------
# Locking
# ---------------------------------------------------------------------------------------------------------

def lock_owner(lock):
	try:
		with open(lock, encoding="ascii", errors="replace") as handle:
			return handle.read().strip().replace("\n", "; ") or "unknown"
	except OSError:
		return "unreadable"


def acquire_lock(root):
	"""Create game/<target>/.deploy/lock holding our PID. Never removes an existing lock."""
	root_real = _norm(root.resolve()) if root.exists() else None
	if root_real is None:
		root.mkdir(parents=True, exist_ok=True)
		root_real = _norm(root.resolve())
	state_dir = root / STATE_DIR
	safe_mkdirs(state_dir, root_real)
	lock = state_dir / LOCK_NAME
	try:
		fd = os.open(str(lock), os.O_CREAT | os.O_EXCL | os.O_WRONLY)
	except FileExistsError:
		raise PlanError("%s is locked by another deploy (%s). If that process is gone the lock is stale: "
			"check the PID, then delete the lock file by hand" % (lock, lock_owner(lock)))
	stamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
	os.write(fd, ("pid %d\nstarted %s\n" % (os.getpid(), stamp)).encode("ascii"))
	os.close(fd)
	return lock


def main():
	try:
		sys.stdout.reconfigure(errors="replace")
	except (AttributeError, ValueError):
		pass
	ap = argparse.ArgumentParser(description="Assemble game/client and game/server from bin/ and staged/.")
	ap.add_argument("--target", required=True, choices=("client", "server", "all"))
	ap.add_argument("--config", required=True, choices=CONFIGS)
	ap.add_argument("--platform", choices=PLATFORMS, default="windows" if os.name == "nt" else "linux")
	ap.add_argument("--dry-run", action="store_true", help="print the plan, write nothing")
	ap.add_argument("--no-prune", action="store_true", help="never remove files, even deploy's own stale ones")
	ap.add_argument("--full-check", action="store_true", help="hash every file instead of trusting the manifest")
	ap.add_argument("--verbose", action="store_true", help="also list unchanged files")
	ap.add_argument("--game-root", type=Path, default=DEFAULT_GAME_ROOT, help="default: <repo>/game")
	ap.add_argument("--bin-root", type=Path, default=DEFAULT_BIN_ROOT, help="default: <repo>/bin")
	ap.add_argument("--build-databases", action="store_true",
		help="server: run the DB generator hook (currently disabled in code)")
	ap.add_argument("--channel", default="DEV", choices=("DEV", "TEST", "PROD"), help="for the DB hook")
	args = ap.parse_args()

	game_root = args.game_root.resolve()
	targets = ("client", "server") if args.target == "all" else (args.target,)
	groups_run = ("staged", "bin-" + args.platform)
	stamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")

	# Lock every target first (a real run only), then read its manifest and plan, so no other deploy can
	# change the manifest between the read and the write. A missing build stops before anything is written.
	locks = []
	plans = []
	try:
		try:
			for target in targets:
				root = game_root / target
				if args.dry_run:
					lock = root / STATE_DIR / LOCK_NAME
					if os.path.lexists(lock):
						say("deploy: WARNING: %s is locked (%s); a real run would stop here" % (lock, lock_owner(lock)))
				else:
					locks.append(acquire_lock(root))
			for target in targets:
				root = game_root / target
				sources = collect_sources(target, args.config, args.platform, args.bin_root.resolve())
				manifest = load_manifest(root / STATE_DIR)
				plan = make_plan(target, sources, manifest, root, groups_run, not args.no_prune, args.full_check)
				plans.append((plan, manifest))
		except (PlanError, OSError) as exc:
			say("deploy: ERROR: %s" % exc)
			say("deploy: nothing was written")
			return 2

		failed = False
		for plan, manifest in plans:
			report(plan, args.dry_run, args.verbose)
			unsafe = sum(1 for kind, _, _ in plan.actions if kind == "unsafe")
			if unsafe:
				# skipped for safety, but the deploy is incomplete: fail it so the build script reports it
				failed = True
				say("  %d file(s) skipped as unsafe (see 'unsafe' above); the deploy is incomplete" % unsafe)
			if args.dry_run:
				continue
			try:
				errors = execute(plan, manifest, stamp, args.config, args.platform)
			except OSError as exc:
				errors = [(STATE_DIR, "deploy stopped: %s" % exc)]
			for rel, message in errors:
				say("  ERROR         %s  - %s" % (rel, message))
			if errors:
				failed = True
				say("  %d file(s) failed; a running game or server locks its exe - stop it and deploy again" % len(errors))
	finally:
		for lock in locks:
			try:
				lock.unlink()
			except OSError:
				say("deploy: WARNING: could not remove %s" % lock)

	if "server" in targets and args.build_databases:
		if run_database_hook(game_root, args.channel, args.dry_run) != 0:
			say("deploy: ERROR: the database hook failed")
			failed = True

	say("deploy: %s" % ("FAILED" if failed else ("dry run complete" if args.dry_run else "done")))
	return 1 if failed else 0


if __name__ == "__main__":
	sys.exit(main())
