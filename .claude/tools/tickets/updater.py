"""The viewer's one-click update (B65): finding a newer release, preparing it, staging and the agents README.

    serve.py           GET /api/update (discover), POST /api/update/apply (prepare, in a thread), its start (recovery)
    updater_helper.py  the detached process that swaps the files, validates, restarts the viewer and stages
    tickets.py         `readme` (write_first_readme) and the one-time pointer after an update

The source is this machine's ticket-system/ folder, named by config.json's update_source (absent or empty: updates
are off). A release is read only from its tag, tickets-v<x.y.z>, so unreleased work on whatever branch the source has
checked out is never installed or run: the release tool itself is the TAGGED one (1.5.3, B66), taken from the tag
with `git -C <update_source> show tickets-v<x.y.z>:./tools/release.py` into the work area and run by run_release_tool
as if it sat at <update_source>/tools/release.py (so it finds the source's git repository). It exports the release
and, with the same tagged tool, the installed release (the merge base); it also merges the rules doc. The source's
working-tree release.py never runs. The latest release is the highest x.y.z with a releases/<x.y.z>.json manifest
and that tag; the installed one is <CLI_DIR>/VERSION.

Work area: <repo>/build/tickets/ (git-ignored): update.lock (one apply at a time), update/status.json, and
update/<x.y.z>/ holding the tagged release tool (release-tool/release.py), the export, the export of the installed
release, the merged schema, the backup, plan.json and helper.log. Staged items go to staged/<x.y.z>/. Nothing here
runs git in the adopter's repository (only read-only git in the source), and nothing is committed.

Line endings: an adopter whose .gitattributes checks text out as CRLF has CRLF tool files. A comparison that means
"same content" (is this tool file already the release's? is the merged schema already the project's?) ignores CRLF
against LF (same_content); one that means "the same bytes" (has the restore already put the backup back?) does not.

Stdlib only; ASCII console output; files are UTF-8 with LF.
"""

import hashlib
import importlib.util
import json
import os
import re
import shutil
import stat
import subprocess
import sys
import time
from datetime import datetime, timezone

version_pattern = re.compile(r"^[0-9]+\.[0-9]+\.[0-9]+$")
tag_prefix = "tickets-v"
# A manifest path the apply may read: under one of these, with no empty, "." or ".." part.
allowed_prefixes = ("src/tools/tickets/", "dot-claude/")
tool_prefix = "src/tools/tickets/"
icons_prefix = "src/tools/tickets/icons/"
schema_rel = "dot-claude/tickets/ticket.schema.json"
skill_rel = "dot-claude/skills/tickets/SKILL.md"
doc_rel = "dot-claude/docs/tickets.md"
hooks_prefix = "dot-claude/hooks/tickets/"
version_file_rel = "src/tools/tickets/VERSION"
helper_name = "updater_helper.py"
# plan.json's format: the installed viewer writes it, the new release's helper reads it.
plan_format = 1
# Final states of status.json; anything else means an apply is under way.
final_states = ("done", "rolled_back", "failed")
# Set in the environment of the helper's own tickets.py runs, so validate does not use up the one-time pointer.
quiet_env = "TICKETS_UPDATER"
readme_marker = "<!-- updates -->"
manifest_read_max = 16 * 1024 * 1024
export_timeout_seconds = 300
git_timeout_seconds = 30
# Windows process creation flags (winbase.h).
detached_process = 0x00000008
create_new_process_group = 0x00000200
create_no_window = 0x08000000
create_breakaway_from_job = 0x01000000


def utc_now():
	return datetime.now(timezone.utc).replace(microsecond=0).strftime("%Y-%m-%dT%H:%M:%SZ")


def ascii_text(text):
	return str(text).encode("ascii", "replace").decode("ascii")


def version_key(value):
	return tuple(int(part) for part in value.split("."))


def state_paths(repo):
	"""Where the updater keeps its state in the repository (all under build/tickets/, git-ignored)."""
	build = os.path.join(repo, "build", "tickets")
	update = os.path.join(build, "update")
	return {"build": build, "update": update, "lock": os.path.join(build, "update.lock"),
		"status": os.path.join(update, "status.json"), "staged": os.path.join(build, "staged"),
		"readme": os.path.join(repo, ".claude", "tickets", "README.md"), "seen": os.path.join(build, "readme-seen")}


def read_text(path, limit=None):
	with open(path, "rb") as handle:
		data = handle.read(limit + 1 if limit else -1)
	if limit and len(data) > limit:
		raise ValueError("larger than {} bytes".format(limit))
	return data.decode("utf-8-sig")


def read_json_file(path, limit=manifest_read_max):
	"""A parsed JSON file, or None when it is missing, unreadable or not JSON."""
	try:
		return json.loads(read_text(path, limit))
	except (OSError, ValueError, RecursionError):
		return None


def write_json_atomically(path, value):
	os.makedirs(os.path.dirname(path), exist_ok=True)
	temp = "{}.{}.tmp".format(path, os.getpid())
	with open(temp, "w", encoding="utf-8", newline="\n") as handle:
		json.dump(value, handle, indent=2, ensure_ascii=False)
		handle.write("\n")
	replace_with_retry(temp, path)


def write_text_atomically(path, text):
	os.makedirs(os.path.dirname(path), exist_ok=True)
	temp = "{}.{}.tmp".format(path, os.getpid())
	with open(temp, "w", encoding="utf-8", newline="\n") as handle:
		handle.write(text)
	replace_with_retry(temp, path)


def replace_with_retry(source, target, seconds=10.0):
	"""os.replace, retried while Windows reports the target held (an editor, a virus scanner) for up to `seconds`."""
	deadline = time.monotonic() + seconds
	while True:
		try:
			os.replace(source, target)
			return
		except PermissionError:
			if time.monotonic() >= deadline:
				raise
			time.sleep(0.25)


def remove_with_retry(path, seconds=10.0):
	deadline = time.monotonic() + seconds
	while True:
		try:
			os.remove(path)
			return
		except FileNotFoundError:
			return
		except PermissionError:
			if time.monotonic() >= deadline:
				raise
			time.sleep(0.25)


def remove_quietly(path):
	try:
		os.remove(path)
	except OSError:
		pass


def install_file(source, target):
	"""Copy source to <target>.new, then os.replace it over target (retried while target is held); the .new copy is
	removed when that fails."""
	os.makedirs(os.path.dirname(target), exist_ok=True)
	temp = target + ".new"
	try:
		shutil.copyfile(source, temp)
		replace_with_retry(temp, target)
	except OSError:
		remove_quietly(temp)
		raise


def remove_tree(path):
	"""Delete a folder the updater made (an old work area), clearing read-only bits on the way."""
	def clear_and_retry(function, target, _info):
		try:
			os.chmod(target, stat.S_IWRITE)
			function(target)
		except OSError:
			pass
	if os.path.isdir(path):
		shutil.rmtree(path, onerror=clear_and_retry)


def clear_pycache(folder):
	"""Delete <folder>/__pycache__, so no compiled copy of a replaced module is ever loaded."""
	remove_tree(os.path.join(folder, "__pycache__"))


def run(arguments, cwd=None, timeout=None, env=None):
	"""(exit code, combined output text) of a command, without a console window; (None, reason) when it cannot run."""
	options = {"cwd": cwd, "stdin": subprocess.DEVNULL, "stdout": subprocess.PIPE, "stderr": subprocess.STDOUT,
		"timeout": timeout, "env": env}
	if os.name == "nt":
		options["creationflags"] = create_no_window
	try:
		result = subprocess.run(arguments, **options)
	except subprocess.TimeoutExpired:
		return None, "timed out after {} s".format(timeout)
	except OSError as error:
		return None, "cannot run {}: {}".format(os.path.basename(arguments[0]), error)
	return result.returncode, result.stdout.decode("utf-8", "replace")


def last_lines(text, count=12):
	lines = [line for line in (text or "").replace("\r\n", "\n").split("\n") if line.strip()]
	return "\n".join(lines[-count:])


# --- discovery ---------------------------------------------------------------------------------------------------

def source_folder(config):
	"""(the update source folder or None, a problem line or None). Absent or empty update_source: (None, None), off."""
	value = config.get("update_source") if isinstance(config, dict) else None
	if value is None and "update_source" not in (config or {}):
		return None, None
	if not isinstance(value, str):
		return None, "update_source must be a string (the absolute path of the ticket-system folder)"
	if not value.strip():
		return None, None
	if not os.path.isabs(value):
		return None, "update_source is not an absolute path"
	folder = os.path.normpath(value)
	if not os.path.isfile(os.path.join(folder, "tools", "release.py")) or not os.path.isdir(os.path.join(folder, "releases")):
		return None, "update_source {} holds no tools/release.py and releases/".format(ascii_text(folder))
	return folder, None


def installed_version(cli_dir):
	"""The version in <CLI_DIR>/VERSION, or None when the file is missing or not x.y.z (a copy before 1.5.0)."""
	try:
		value = read_text(os.path.join(cli_dir, "VERSION"), 64).strip()
	except (OSError, ValueError):
		return None
	return value if version_pattern.match(value) else None


def tagged_versions(source):
	"""(versions with a manifest and a tag, ascending; a problem line or None)."""
	try:
		names = os.listdir(os.path.join(source, "releases"))
	except OSError as error:
		return [], "cannot list {}/releases ({})".format(ascii_text(source), error.strerror or error)
	manifests = {name[:-5] for name in names if name.endswith(".json") and version_pattern.match(name[:-5])}
	code, output = run(["git", "-C", source, "tag", "-l", tag_prefix + "*"], timeout=git_timeout_seconds)
	if code != 0:
		return [], "git cannot list the tags in {} ({})".format(ascii_text(source), ascii_text(last_lines(output, 1)))
	tags = {line.strip()[len(tag_prefix):] for line in output.splitlines() if line.strip().startswith(tag_prefix)}
	return sorted(manifests & tags, key=version_key), None


def manifest_of(source, version):
	value = read_json_file(os.path.join(source, "releases", version + ".json"))
	return value if isinstance(value, dict) else None


def release_notes(source, versions, installed, latest):
	"""Per release after the installed one up to the latest, newest first: its version, changelog entry and agents lines."""
	notes = []
	for version in reversed(versions):
		if version_key(installed) < version_key(version) <= version_key(latest):
			manifest = manifest_of(source, version) or {}
			changelog = manifest.get("changelog")
			agents = manifest.get("agents")
			notes.append({"version": version, "changelog": changelog if isinstance(changelog, str) else "",
				"agents": [line for line in agents if isinstance(line, str)] if isinstance(agents, list) else []})
	return notes


def discover(config, cli_dir):
	"""What GET /api/update reports: {"state", "installed", "latest", "notes", "reason"}. state is off (no update_source),
	unavailable (the source cannot be read; reason says why), unknown (no <CLI_DIR>/VERSION), current or available."""
	result = {"state": "off", "installed": installed_version(cli_dir), "latest": None, "notes": [], "reason": None}
	source, problem = source_folder(config)
	if source is None:
		if problem:
			result.update(state="unavailable", reason=problem)
		return result
	versions, problem = tagged_versions(source)
	if problem:
		result.update(state="unavailable", reason=problem)
		return result
	result["latest"] = versions[-1] if versions else None
	if result["installed"] is None:
		result.update(state="unknown", reason="no VERSION file in the tool folder (a copy before 1.5.0 updates by hand)")
	elif result["latest"] is None or version_key(result["latest"]) <= version_key(result["installed"]):
		result["state"] = "current"
	else:
		result["state"] = "available"
		result["notes"] = release_notes(source, versions, result["installed"], result["latest"])
	return result


# --- the lock and the status ---------------------------------------------------------------------------------------

def process_created(pid):
	"""The process's creation time (Windows FILETIME ticks), or None when it is gone, unreadable or not on Windows."""
	if os.name != "nt" or not isinstance(pid, int) or isinstance(pid, bool) or not 0 < pid <= 0xFFFFFFFF:
		return None
	import ctypes
	from ctypes import wintypes
	kernel32 = ctypes.windll.kernel32
	kernel32.OpenProcess.restype = wintypes.HANDLE
	kernel32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
	kernel32.GetProcessTimes.argtypes = [wintypes.HANDLE] + [ctypes.POINTER(wintypes.FILETIME)] * 4
	kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
	handle = kernel32.OpenProcess(0x1000, False, pid)
	if not handle:
		return None
	try:
		times = [wintypes.FILETIME() for _ in range(4)]
		if not kernel32.GetProcessTimes(handle, *[ctypes.byref(value) for value in times]):
			return None
		return (times[0].dwHighDateTime << 32) | times[0].dwLowDateTime
	finally:
		kernel32.CloseHandle(handle)


def process_alive(pid, created):
	"""True while pid runs and (on Windows, when created is known) is still the process created then."""
	if not isinstance(pid, int) or isinstance(pid, bool) or pid <= 0:
		return False
	if os.name == "nt":
		current = process_created(pid)
		if current is None:
			# OpenProcess fails for a pid that is gone; an exited process whose handle someone holds still answers.
			return False
		if not isinstance(created, int) or isinstance(created, bool):
			return True
		if current != created:
			return False
		# GetExitCodeProcess: 259 (STILL_ACTIVE) while it runs.
		import ctypes
		from ctypes import wintypes
		kernel32 = ctypes.windll.kernel32
		handle = kernel32.OpenProcess(0x1000, False, pid)
		if not handle:
			return False
		try:
			code = wintypes.DWORD()
			if not kernel32.GetExitCodeProcess(handle, ctypes.byref(code)):
				return True
			return code.value == 259
		finally:
			kernel32.CloseHandle(handle)
	try:
		os.kill(pid, 0)
	except OSError:
		return False
	return True


def lock_record(pid, version, work):
	return {"pid": pid, "created": process_created(pid), "version": version, "work": work, "since": utc_now()}


def take_lock(paths, version, work):
	"""Create build/tickets/update.lock exclusively for this process; False when an apply already holds it."""
	os.makedirs(paths["build"], exist_ok=True)
	try:
		descriptor = os.open(paths["lock"], os.O_CREAT | os.O_EXCL | os.O_WRONLY)
	except FileExistsError:
		return False
	with os.fdopen(descriptor, "w", encoding="utf-8", newline="\n") as handle:
		json.dump(lock_record(os.getpid(), version, work), handle, indent=2)
		handle.write("\n")
	return True


def hand_lock(paths, pid, version, work):
	"""Name another process (the helper) as the lock's holder."""
	write_json_atomically(paths["lock"], lock_record(pid, version, work))


def read_lock(paths):
	value = read_json_file(paths["lock"], 64 * 1024)
	return value if isinstance(value, dict) else None


def release_lock(paths):
	try:
		os.remove(paths["lock"])
	except OSError:
		pass


def read_status(paths):
	value = read_json_file(paths["status"], 1024 * 1024)
	return value if isinstance(value, dict) else None


def write_status(paths, state, **fields):
	"""Record the apply's state in update/status.json, keeping what earlier steps recorded unless replaced."""
	status = read_status(paths) or {}
	if fields.pop("fresh", False):
		status = {}
	status.update(fields)
	status["state"] = state
	status["updated"] = utc_now()
	write_json_atomically(paths["status"], status)
	return status


# --- reading a release -------------------------------------------------------------------------------------------

def path_problem(relative):
	"""Why a manifest path may not be used by the apply, or None: it must sit under an allowed prefix, as plain parts."""
	if not isinstance(relative, str) or not relative:
		return "not a path"
	if "\\" in relative or ":" in relative or relative.startswith("/") or os.path.isabs(relative):
		return "absolute, or holds a backslash or colon"
	parts = relative.split("/")
	if any(part in ("", ".", "..") for part in parts):
		return "an empty, '.' or '..' part"
	if not relative.startswith(allowed_prefixes):
		return "outside {}".format(" and ".join(allowed_prefixes))
	return None


def manifest_problems(manifest, version):
	"""Problems with an exported release's own manifest: its version, and every path it lists (path_problem)."""
	if not isinstance(manifest, dict) or not isinstance(manifest.get("files"), dict):
		return ["releases/{}.json in the export is not a manifest".format(version)]
	problems = []
	if manifest.get("version") != version:
		problems.append("releases/{}.json in the export records version {!r}".format(version, manifest.get("version")))
	for relative, entry in sorted(manifest["files"].items()):
		problem = path_problem(relative)
		if problem:
			problems.append("manifest path {!r}: {}".format(ascii_text(relative), problem))
		elif not isinstance(entry, dict) or not isinstance(entry.get("raw"), str):
			problems.append("manifest entry {!r} has no raw hash".format(ascii_text(relative)))
	return problems


def sha256_file(path):
	digest = hashlib.sha256()
	with open(path, "rb") as handle:
		for block in iter(lambda: handle.read(1024 * 1024), b""):
			digest.update(block)
	return digest.hexdigest()


def payload_path(export_dir, relative):
	return os.path.join(export_dir, "payload", *relative.split("/"))


def verify_export(export_dir, manifest):
	"""Problems where an exported file is missing or its bytes differ from the manifest's raw hash. This catches a
	half-written or interrupted export (the source is our own repository, so it is no defence against an attacker)."""
	problems = []
	root = os.path.realpath(os.path.join(export_dir, "payload"))
	for relative, entry in sorted(manifest["files"].items()):
		path = payload_path(export_dir, relative)
		if not os.path.realpath(path).startswith(root + os.sep):
			problems.append("{}: resolves outside the export".format(relative))
		elif not os.path.isfile(path):
			problems.append("{}: missing from the export".format(relative))
		elif sha256_file(path) != entry["raw"]:
			problems.append("{}: its bytes differ from the manifest (a cut-off or altered export)".format(relative))
	return problems


# Runs a tagged release.py (argv[1]) as though it were the file argv[2] (<update_source>/tools/release.py): release.py
# finds its ticket-system folder, and so the git repository, from its own __file__. Started with -I, so neither the
# environment nor the current folder adds anything to the import path.
release_launcher = ("import sys\n"
	"tool, home = sys.argv[1], sys.argv[2]\n"
	"sys.argv = [home] + sys.argv[3:]\n"
	"with open(tool, 'rb') as handle:\n"
	"    code = compile(handle.read(), home, 'exec')\n"
	"exec(code, {'__name__': '__main__', '__file__': home, '__builtins__': __builtins__})\n")
release_tool_name = os.path.join("release-tool", "release.py")


def tagged_release_tool(source, version, destination):
	"""Write tools/release.py as tagged tickets-v<version> in the source to destination; a problem line or None. The
	blob is read as committed (git show applies no filter), never from the source's working tree."""
	options = {"stdin": subprocess.DEVNULL, "stdout": subprocess.PIPE, "stderr": subprocess.PIPE,
		"timeout": git_timeout_seconds}
	if os.name == "nt":
		options["creationflags"] = create_no_window
	try:
		result = subprocess.run(["git", "-C", source, "show", "{}{}:./tools/release.py".format(tag_prefix, version)],
			**options)
	except subprocess.TimeoutExpired:
		return "git show of the tagged release.py timed out"
	except OSError as error:
		return "cannot run git: {}".format(error)
	if result.returncode != 0 or not result.stdout:
		return "cannot read tools/release.py at {}{}: {}".format(tag_prefix, version, ascii_text(last_lines(
			result.stderr.decode("utf-8", "replace"), 2)) or "empty")
	try:
		os.makedirs(os.path.dirname(destination), exist_ok=True)
		with open(destination, "wb") as handle:
			handle.write(result.stdout)
	except OSError as error:
		return "cannot write the tagged release.py: {}".format(error)
	return None


def run_release_tool(tool, source, arguments, cwd, timeout, python=None):
	"""(exit code, output) of the tagged release tool `tool` run with arguments, as <source>/tools/release.py."""
	return run([python or sys.executable, "-I", "-B", "-c", release_launcher, tool,
		os.path.join(source, "tools", "release.py")] + list(arguments), cwd=cwd, timeout=timeout)


def export_release(source, version, destination, tool):
	"""Run the tagged release tool's export of version into destination (made fresh); a problem line or None."""
	remove_tree(destination)
	os.makedirs(os.path.dirname(destination), exist_ok=True)
	code, output = run_release_tool(tool, source, ["export", version, destination], source, export_timeout_seconds)
	if code != 0:
		return "release.py export {} failed: {}".format(version, ascii_text(last_lines(output, 3)))
	return None


def load_schema_merge(export_dir, fallback_dir):
	"""The release's schema_merge module (its merge rules are the release's), else the installed one."""
	for folder in (os.path.join(export_dir, "payload", "src", "tools", "tickets"), fallback_dir):
		path = os.path.join(folder, "schema_merge.py")
		if os.path.isfile(path):
			spec = importlib.util.spec_from_file_location("schema_merge_for_update", path)
			module = importlib.util.module_from_spec(spec)
			before = sys.dont_write_bytecode
			sys.dont_write_bytecode = True
			try:
				spec.loader.exec_module(module)
			finally:
				sys.dont_write_bytecode = before
			return module
	return None


def tool_target(cli_dir, relative):
	return os.path.join(cli_dir, *relative[len(tool_prefix):].split("/"))


def replaced_tool_files(manifest):
	"""The release's tool-folder files the apply installs: product files under src/tools/tickets/, icons excluded."""
	return sorted(relative for relative, entry in manifest["files"].items()
		if relative.startswith(tool_prefix) and not relative.startswith(icons_prefix) and entry.get("kind") == "product")


def same_bytes(left, right):
	"""Byte identity: the restore's "already put back" check, where the backup is a byte copy of the target."""
	try:
		return os.path.getsize(left) == os.path.getsize(right) and sha256_file(left) == sha256_file(right)
	except OSError:
		return False


def same_content(left, right):
	"""Byte identity once CRLF is read as LF: a tool file or schema in a CRLF checkout (an adopter's .gitattributes) is
	the release's file when only its line endings differ, and is neither replaced nor listed as changed."""
	if same_bytes(left, right):
		return True
	try:
		if max(os.path.getsize(left), os.path.getsize(right)) > manifest_read_max:
			return False
		with open(left, "rb") as handle:
			left_bytes = handle.read()
		with open(right, "rb") as handle:
			right_bytes = handle.read()
	except OSError:
		return False
	return left_bytes.replace(b"\r\n", b"\n") == right_bytes.replace(b"\r\n", b"\n")


inline_array_width = 100


def layout_of(text):
	"""(indent unit, CRLF?) of a JSON file's text: its first indented line's leading whitespace; 2 spaces by default."""
	if not text:
		return "  ", False
	for line in text.replace("\r\n", "\n").split("\n")[1:]:
		stripped = line.lstrip(" \t")
		if stripped and len(stripped) < len(line):
			return line[:len(line) - len(stripped)], "\r\n" in text
	return "  ", "\r\n" in text


def schema_text(schema, project_text=None):
	"""The schema as JSON in the project file's layout: its indent unit and line endings, an array of scalars on one
	line when it fits (as the shipped schema writes its enums and required lists), objects one key per line."""
	unit, crlf = layout_of(project_text)

	def dump(value, depth):
		pad = unit * (depth + 1)
		if isinstance(value, dict):
			if not value:
				return "{}"
			items = ["{}{}: {}".format(pad, json.dumps(key, ensure_ascii=False), dump(item, depth + 1))
				for key, item in value.items()]
			return "{\n" + ",\n".join(items) + "\n" + unit * depth + "}"
		if isinstance(value, list):
			if not value:
				return "[]"
			if all(not isinstance(item, (dict, list)) for item in value):
				inline = "[" + ", ".join(json.dumps(item, ensure_ascii=False) for item in value) + "]"
				if len(pad) + len(inline) <= inline_array_width:
					return inline
			return "[\n" + ",\n".join(pad + dump(item, depth + 1) for item in value) + "\n" + unit * depth + "]"
		return json.dumps(value, ensure_ascii=False)
	text = dump(schema, 0) + "\n"
	return text.replace("\n", "\r\n") if crlf else text


# --- the apply's preparation (in the viewer) ---------------------------------------------------------------------

class stop_update(Exception):
	"""A reason to stop before anything outside build/ changes."""
	pass


def prepare(repo, cli_dir, config, version, viewer):
	"""Export, verify, merge the schema, back up, write plan.json and start the helper; run in a thread of the viewer
	once the lock is taken. viewer: {"pid", "port", "bind", "data_dir", "service"}. On a problem the status says
	failed with the reason, the lock is released and no file outside build/tickets/ has changed."""
	paths = state_paths(repo)
	work = os.path.join(paths["update"], version)
	try:
		helper = prepare_steps(repo, cli_dir, config, version, viewer, paths, work)
	except stop_update as reason:
		write_status(paths, "failed", detail=str(reason), finished=utc_now())
		release_lock(paths)
		return False
	except Exception as error:  # noqa: BLE001 - a thread must never die silently holding the lock
		write_status(paths, "failed", detail="unexpected {}: {}".format(type(error).__name__, ascii_text(error)),
			finished=utc_now())
		release_lock(paths)
		return False
	return helper


readme_shown = ".claude/tickets/README.md"


def shown_path(repo, target):
	"""A path as the user sees it in the changed list: relative to the repository, forward slashes."""
	try:
		return os.path.relpath(target, repo).replace("\\", "/")
	except ValueError:  # another drive on Windows
		return str(target).replace("\\", "/")


def prepare_steps(repo, cli_dir, config, version, viewer, paths, work):
	installed = installed_version(cli_dir)
	write_status(paths, "running", fresh=True, version=version, installed=installed, step="exporting",
		started=utc_now(), changed=[], actions=[])
	source, problem = source_folder(config)
	if source is None:
		raise stop_update(problem or "updates are off (no update_source)")
	if installed is None:
		raise stop_update("no VERSION file in the tool folder")
	# A store that is already invalid would fail the helper's validate after the swap; refuse it before anything moves.
	write_status(paths, "running", step="validating the store")
	env = dict(os.environ)
	env[quiet_env] = "1"
	code, output = run([sys.executable, "-B", os.path.join(cli_dir, "tickets.py"), "validate"], cwd=repo, timeout=300,
		env=env)
	if code != 0:
		raise stop_update("the store does not validate now, so nothing was changed; fix it (tickets.py validate), then "
			"update:\n" + ascii_text(last_lines(output, 15)))
	write_status(paths, "running", step="exporting")
	# The last update's work area (and its backup) is kept until this one starts.
	if os.path.isdir(paths["update"]):
		for name in os.listdir(paths["update"]):
			if version_pattern.match(name) and name != version:
				remove_tree(os.path.join(paths["update"], name))
	remove_tree(work)
	export_dir = os.path.join(work, "export")
	base_dir = os.path.join(work, "base")
	# The new release's own release.py, from its tag: the source's working tree may hold unreleased tool code.
	release_tool = os.path.join(work, release_tool_name)
	problem = tagged_release_tool(source, version, release_tool)
	if problem:
		raise stop_update(problem)
	for wanted, folder in ((version, export_dir), (installed, base_dir)):
		problem = export_release(source, wanted, folder, release_tool)
		if problem:
			raise stop_update(problem)
	write_status(paths, "running", step="verifying")
	manifests = {}
	for wanted, folder in ((version, export_dir), (installed, base_dir)):
		manifest = read_json_file(os.path.join(folder, "releases", wanted + ".json"))
		problems = manifest_problems(manifest, wanted)
		if not problems:
			problems = verify_export(folder, manifest)
		if problems:
			raise stop_update("the export of {} does not match its manifest: {}".format(wanted, "; ".join(problems[:5])))
		manifests[wanted] = manifest
	release, base = manifests[version], manifests[installed]
	helper_source = payload_path(export_dir, tool_prefix + helper_name)
	if tool_prefix + helper_name not in release["files"]:
		raise stop_update("release {} has no {}".format(version, helper_name))

	write_status(paths, "running", step="merging the schema")
	merger = load_schema_merge(export_dir, cli_dir)
	project_schema_path = os.path.join(repo, ".claude", "tickets", "ticket.schema.json")
	schemas = {}
	for label, path in (("release", payload_path(export_dir, schema_rel)), ("base", payload_path(base_dir, schema_rel)),
			("project", project_schema_path)):
		schemas[label] = read_json_file(path)
		if schemas[label] is None:
			raise stop_update("cannot read the {} schema ({})".format(label, ascii_text(path)))
	merged, problems = merger.merge_schema(schemas["release"], schemas["project"], schemas["base"])
	if problems:
		raise stop_update("the schema cannot be merged, so nothing was changed: " + "; ".join(problems))
	# Only a real change is installed, in the least disruptive bytes: none when the merge is the project's schema as it
	# stands, the release's own file when it is that, else the project's layout (schema_text).
	merged_path = None
	if merged == schemas["project"]:
		# Nothing to change, however the project's file is laid out (even when it equals the release's schema).
		pass
	elif merged == schemas["release"]:
		merged_path = payload_path(export_dir, schema_rel)
	else:
		merged_path = os.path.join(work, "ticket.schema.json")
		try:
			project_text = read_text(project_schema_path)
		except (OSError, ValueError):
			project_text = None
		write_text_atomically(merged_path, schema_text(merged, project_text))

	write_status(paths, "running", step="backing up")
	copies, backed_up, added, deletions = [], [], [], []
	backup_dir = os.path.join(work, "backup")
	wanted_files = replaced_tool_files(release)
	# VERSION goes last: a copy stopped part-way still says the release it was.
	wanted_files.sort(key=lambda relative: relative == version_file_rel)
	for relative in wanted_files:
		target = tool_target(cli_dir, relative)
		source_file = payload_path(export_dir, relative)
		# Already the release's (a CRLF checkout of the same file included): left alone, and not listed as changed.
		if os.path.isfile(target) and same_content(source_file, target):
			continue
		copies.append({"source": source_file, "target": target, "shown": shown_path(repo, target)})
	if merged_path and not same_content(merged_path, project_schema_path):
		copies.append({"source": merged_path, "target": project_schema_path, "shown": shown_path(repo, project_schema_path)})
	copies.sort(key=lambda item: os.path.basename(item["target"]) == "VERSION")
	for relative in replaced_tool_files(base):
		target = tool_target(cli_dir, relative)
		if relative not in release["files"] and os.path.isfile(target):
			deletions.append({"target": target, "shown": shown_path(repo, target)})
	for index, item in enumerate(copies + deletions):
		if os.path.isfile(item["target"]):
			backup = os.path.join(backup_dir, "{:03d}-{}".format(index, os.path.basename(item["target"])))
			os.makedirs(backup_dir, exist_ok=True)
			shutil.copy2(item["target"], backup)
			backed_up.append({"target": item["target"], "backup": backup})
		else:
			added.append(item["target"])

	plan = {"format": plan_format, "version": version, "installed": installed, "repo": repo, "cli_dir": cli_dir,
		"work": work, "export": export_dir, "base": base_dir, "source": source, "release_tool": release_tool,
		"python": sys.executable,
		"copies": copies, "deletions": deletions, "backed_up": backed_up, "added": added,
		"schema": project_schema_path, "viewer": viewer, "status": paths["status"], "lock": paths["lock"],
		"created": utc_now()}
	write_json_atomically(os.path.join(work, "plan.json"), plan)
	write_status(paths, "running", step="handing off", changed=[item["shown"] for item in copies] +
		["{} (deleted)".format(item["shown"]) for item in deletions] + [readme_shown])
	pid = start_helper(helper_source, os.path.join(work, "plan.json"), work)
	hand_lock(paths, pid, version, work)
	return pid


def start_helper(helper, plan_path, work):
	"""Start the release's updater_helper.py detached (it outlives this viewer, which it stops); its pid."""
	log = open(os.path.join(work, "helper.log"), "ab")
	try:
		arguments = [sys.executable, "-B", helper, plan_path]
		options = {"stdin": subprocess.DEVNULL, "stdout": log, "stderr": subprocess.STDOUT, "cwd": work, "close_fds": True}
		if os.name == "nt":
			flags = detached_process | create_new_process_group | create_no_window
			try:
				# Out of any job the viewer runs in (a session's background task), so stopping the viewer cannot end it.
				child = subprocess.Popen(arguments, creationflags=flags | create_breakaway_from_job, **options)
			except OSError:
				child = subprocess.Popen(arguments, creationflags=flags, **options)
		else:
			child = subprocess.Popen(arguments, start_new_session=True, **options)
	finally:
		log.close()
	return child.pid


# --- restore (the helper's rollback) -----------------------------------------------------------------------------

def restore(plan):
	"""Put every backed-up file back and remove the files the apply added; the problem lines (empty when all went)."""
	problems = []
	for item in plan.get("backed_up", []):
		temp = None
		try:
			if same_bytes(item["backup"], item["target"]):
				# Never replaced (the swap stopped before it), or already put back. Bytes, not same_content: the backup
				# is a byte copy, and only the target's own bytes (CRLF included) count as restored.
				continue
			temp = item["target"] + ".restore"
			shutil.copy2(item["backup"], temp)
			replace_with_retry(temp, item["target"])
		except (OSError, KeyError, TypeError) as error:
			problems.append("cannot restore {}: {}".format(ascii_text(item.get("target")), ascii_text(error)))
			if temp:
				remove_quietly(temp)
	for target in plan.get("added", []):
		try:
			remove_with_retry(target)
		except OSError as error:
			problems.append("cannot remove {}: {}".format(ascii_text(target), ascii_text(error)))
	for item in plan.get("copies", []):
		# A .new copy a stopped swap left behind.
		if isinstance(item, dict) and isinstance(item.get("target"), str):
			remove_quietly(item["target"] + ".new")
	return problems


# --- staging -----------------------------------------------------------------------------------------------------

def raw_hash(manifest, relative):
	entry = manifest.get("files", {}).get(relative) if isinstance(manifest, dict) else None
	return entry.get("raw") if isinstance(entry, dict) else None


settings_heading = re.compile(r"^## Settings(?![0-9A-Za-z_])")
table_row = re.compile(r"^ {0,3}\|")


def settings_rows(lines):
	"""(start, end) line indexes of the Settings section's table rows, or None."""
	start = None
	for index, line in enumerate(lines):
		if settings_heading.match(line):
			start = index
			break
	if start is None:
		return None
	rows = [index for index in range(start + 1, len(lines)) if table_row.match(lines[index])]
	end = len(lines)
	for index in range(start + 1, len(lines)):
		if lines[index].startswith("# ") or lines[index].startswith("## "):
			end = index
			break
	rows = [index for index in rows if index < end]
	return (rows[0], rows[-1] + 1) if rows else None


def row_key(line):
	cells = line.strip().strip("|").split("|")
	return cells[0].strip().lower() if cells else ""


def staged_skill(release_text, project_text, version):
	"""The release's skill with the project's Settings rows in place of its own, the version row set; (text, notes)."""
	notes = []
	release_lines = release_text.replace("\r\n", "\n").split("\n")
	span = settings_rows(release_lines)
	if span is None:
		return release_text, ["the release skill has no Settings table; review it whole"]
	project_rows = {}
	if project_text is not None:
		project_lines = project_text.replace("\r\n", "\n").split("\n")
		project_span = settings_rows(project_lines)
		if project_span:
			for line in project_lines[project_span[0]:project_span[1]]:
				project_rows.setdefault(row_key(line), line)
	rows = []
	for line in release_lines[span[0]:span[1]]:
		key = row_key(line)
		if key == "ticket system version":
			rows.append("| Ticket system version | {} |".format(version))
		elif key in project_rows and not set(key) <= set("-: "):
			rows.append(project_rows[key])
		else:
			rows.append(line)
			if key and not set(key) <= set("-: ") and key != "setting" and project_rows:
				notes.append("new Settings row '{}': fill it in".format(ascii_text(key)))
	return "\n".join(release_lines[:span[0]] + rows + release_lines[span[1]:]), notes


def stage(plan, log=None):
	"""Stage the skill, the hooks (where installed) and the merged rules doc in build/tickets/staged/<x.y.z>/ when the
	release changed them; the ACTION lines for the README."""
	repo, version = plan["repo"], plan["version"]
	release = read_json_file(os.path.join(plan["export"], "releases", version + ".json")) or {}
	base = read_json_file(os.path.join(plan["base"], "releases", plan["installed"] + ".json")) or {}
	staged = os.path.join(state_paths(repo)["staged"], version)
	shown_staged = "build/tickets/staged/{}".format(version)
	actions = []
	remove_tree(staged)

	if raw_hash(release, skill_rel) != raw_hash(base, skill_rel):
		project_skill = os.path.join(repo, ".claude", "skills", "tickets", "SKILL.md")
		try:
			project_text = read_text(project_skill) if os.path.isfile(project_skill) else None
			text, notes = staged_skill(read_text(payload_path(plan["export"], skill_rel)), project_text, version)
			write_text_atomically(os.path.join(staged, "SKILL.md"), text)
			actions.append("staged skill at {}/SKILL.md (your Settings rows carried over, version row set{}): have it "
				"reviewed by a separate read-only agent, then copy it to .claude/skills/tickets/SKILL.md".format(
				shown_staged, "; " + "; ".join(notes) if notes else ""))
		except (OSError, ValueError) as error:
			actions.append("staging the skill failed ({}); take it from the release by hand".format(ascii_text(error)))

	hooks_dir = os.path.join(repo, ".claude", "hooks", "tickets")
	hook_files = sorted(relative for relative in release.get("files", {}) if relative.startswith(hooks_prefix))
	base_hooks = sorted(relative for relative in base.get("files", {}) if relative.startswith(hooks_prefix))
	if os.path.isdir(hooks_dir) and (hook_files != base_hooks or any(raw_hash(release, relative) != raw_hash(base,
			relative) for relative in hook_files)):
		try:
			for relative in hook_files:
				target = os.path.join(staged, "hooks", "tickets", *relative[len(hooks_prefix):].split("/"))
				os.makedirs(os.path.dirname(target), exist_ok=True)
				shutil.copyfile(payload_path(plan["export"], relative), target)
			actions.append("staged hooks at {}/hooks/tickets/: run their staged self-test, have them reviewed, then "
				"install them as the source's docs/upgrading/hooks.md says".format(shown_staged))
		except OSError as error:
			actions.append("staging the hooks failed ({}); take them from the release by hand".format(ascii_text(error)))

	project_doc = os.path.join(repo, ".claude", "docs", "tickets.md")
	if raw_hash(release, doc_rel) != raw_hash(base, doc_rel) and os.path.isfile(project_doc):
		out = os.path.join(staged, "tickets.md")
		os.makedirs(staged, exist_ok=True)
		# The tagged release tool prepare extracted; a plan from a viewer before 1.5.3 has none, so take it from the tag
		# now. The source's working-tree release.py is never run.
		tool = plan.get("release_tool")
		problem = None
		if not isinstance(tool, str) or not os.path.isfile(tool):
			tool = os.path.join(plan["work"], release_tool_name)
			problem = tagged_release_tool(plan["source"], version, tool)
		if problem:
			code, output = 1, problem
		else:
			code, output = run_release_tool(tool, plan["source"], ["merge-doc", payload_path(plan["export"], doc_rel),
				project_doc, "--base", payload_path(plan["base"], doc_rel), "--out", out], repo, 120, plan["python"])
		if log:
			log("merge-doc exit {}:\n{}".format(code, output))
		if code == 0:
			actions.append("merged rules doc at {}/tickets.md: review its diff against .claude/docs/tickets.md, then "
				"copy it over".format(shown_staged))
		elif code == 2:
			looks = [line for line in output.splitlines() if line.startswith("LOOK ")]
			actions.append("merged rules doc at {}/tickets.md has {} thing(s) to look at (conflict blocks or dropped "
				"text): resolve them, review the diff, then copy it over".format(shown_staged, len(looks) or "some"))
		else:
			actions.append("merging the rules doc failed ({}); merge it by hand with the source's release.py "
				"merge-doc".format(ascii_text(last_lines(output, 1))))
	return actions


# --- the agents README -------------------------------------------------------------------------------------------

readme_head = """# Ticket system: what changed for agents

Written by the ticket system's updater (and by `tickets.py readme`); do not edit it, it is rewritten. Newest first.
Per release: what agents need to know (a new CLI flag, a new status, a renamed field). Lines starting `ACTION` are
for this copy: act on one only if your role allows it, otherwise tell the user. Agents only ever deal with the
ticket JSON, through the CLI; nothing here asks for a hand edit of a ticket.
"""


def readme_section(version, heading, agents, actions):
	lines = ["## {} ({})".format(version, heading), ""]
	if agents:
		lines += ["For agents:"] + ["- " + line for line in agents]
	else:
		lines.append("For agents: nothing new.")
	if actions:
		lines.append("")
		lines += ["ACTION " + line for line in actions]
	return "\n".join(lines) + "\n"


def write_readme(repo, sections):
	"""Rewrite .claude/tickets/README.md: the head, the new sections (newest first), then the earlier ones it held."""
	path = state_paths(repo)["readme"]
	earlier = ""
	try:
		previous = read_text(path)
		if readme_marker in previous:
			earlier = previous.split(readme_marker, 1)[1].strip("\n")
	except (OSError, ValueError):
		pass
	body = "\n".join(sections).rstrip("\n")
	text = readme_head + "\n" + readme_marker + "\n\n" + body + "\n" + ("\n" + earlier + "\n" if earlier else "")
	write_text_atomically(path, text)
	return path


def mark_readme_seen(repo, version):
	path = state_paths(repo)["seen"]
	try:
		os.makedirs(os.path.dirname(path), exist_ok=True)
		with open(path, "w", encoding="utf-8", newline="\n") as handle:
			handle.write(version + "\n")
	except OSError:
		pass


def update_readme(plan, actions, changed):
	"""The README after an update: one section per release from the installed one (exclusive) to the new one, with this
	copy's ACTION lines under the newest."""
	export_manifest = read_json_file(os.path.join(plan["export"], "releases", plan["version"] + ".json")) or {}
	source_versions, _ = tagged_versions(plan["source"])
	versions = [version for version in reversed(source_versions) if version_key(plan["installed"]) < version_key(version)
		< version_key(plan["version"])]
	sections = []
	agents = export_manifest.get("agents") if isinstance(export_manifest.get("agents"), list) else []
	all_actions = list(actions)
	if changed:
		all_actions.append("nothing was committed: review and commit the changed files ({})".format(", ".join(changed)))
	sections.append(readme_section(plan["version"], "updated from {}, {}".format(plan["installed"], utc_now()[:10]),
		[line for line in agents if isinstance(line, str)], all_actions))
	for version in versions:
		manifest = manifest_of(plan["source"], version) or {}
		lines = manifest.get("agents") if isinstance(manifest.get("agents"), list) else []
		sections.append(readme_section(version, "skipped over by this update", [line for line in lines
			if isinstance(line, str)], []))
	return write_readme(plan["repo"], sections)


def write_first_readme(repo, cli_dir, config):
	"""`tickets.py readme`: write .claude/tickets/README.md for the installed release when it is missing (the
	bootstrap of a manual 1.5.0 install); (path, written)."""
	path = state_paths(repo)["readme"]
	installed = installed_version(cli_dir)
	if installed is None:
		raise ValueError("no VERSION file in the tool folder")
	if os.path.isfile(path):
		mark_readme_seen(repo, installed)
		return path, False
	agents = []
	source, _ = source_folder(config)
	if source:
		manifest = manifest_of(source, installed) or {}
		if isinstance(manifest.get("agents"), list):
			agents = [line for line in manifest["agents"] if isinstance(line, str)]
	write_readme(repo, [readme_section(installed, "installed by hand, {}".format(utc_now()[:10]), agents, [])])
	mark_readme_seen(repo, installed)
	return path, True
