"""Repo paths, the flow error type and small helpers shared by every hb_driver module.

Stdlib only. Nothing here touches the client or the servers.
"""

import json
import os
import time

package_dir = os.path.dirname(os.path.abspath(__file__))
agent_mcp_dir = os.path.dirname(package_dir)
# hb_driver -> agent_mcp -> tools -> .claude -> <repo root>
repo_root = os.path.dirname(os.path.dirname(os.path.dirname(agent_mcp_dir)))

# Deployed output (build.md): the client runs from game/client, the server from game/server.
client_dir = os.path.join(repo_root, "game", "client")
server_dir = os.path.join(repo_root, "game", "server")
# The deployed Debug client (Game_x64_win.exe on Windows, Game_x64_linux on Linux), the only one with the bridge.
deployed_client_exe = os.path.join(client_dir, "Game_x64_win.exe" if os.name == "nt" else "Game_x64_linux")
debug_client_exe = deployed_client_exe
# Server-side paths below belong to the server flows (phase 4); they are best guesses here, not verified.
world_log_path = os.path.join(server_dir, "gamelogs", "server.log")
login_log_path = os.path.join(server_dir, "gamelogs", "server.log")
client_log_path = os.path.join(client_dir, "logs", "client.log")
world_config_path = os.path.join(server_dir, "server_config.json")
db_snapshot_script = os.path.join(repo_root, "sources", "database", "extract_databases.py")
backups_dir = os.path.join(repo_root, ".claude", "backups")
# Gitignored wholesale, so a throwaway password written under it never reaches a commit.
scratch_root = os.path.join(repo_root, ".claude", "scratch")

# Files the client writes into its working directory while it runs (settings.json on any settings
# change; the remembered-account file name is unverified here).
client_json_name = "settings.json"
remembered_account_name = "remembered_account.txt"

# The logical UI height every screen shares (800x600 design space; the width may stretch).
ui_height = 600


class flow_error(RuntimeError):
	"""A flow could not reach its goal. The message says where it stopped and what it saw."""


def poll_until(probe, timeout_s, interval_s):
	"""Call probe() until it returns something truthy or the time runs out; return its last value."""
	deadline = time.monotonic() + timeout_s
	while True:
		value = probe()
		if value or time.monotonic() >= deadline:
			return value
		time.sleep(interval_s)


def is_inside(path, root):
	"""True when `path` resolves inside `root` (both made absolute and case-folded on Windows)."""
	path = os.path.normcase(os.path.abspath(path))
	root = os.path.normcase(os.path.abspath(root))
	return path == root or path.startswith(root + os.sep)


def require_scratch_path(path, what):
	"""Refuse a path that is not absolute or not under the gitignored .claude/scratch/ tree."""
	if not os.path.isabs(path):
		raise flow_error("{} must be an absolute path: {}".format(what, path))
	if not is_inside(path, scratch_root):
		raise flow_error("{} must live under {} (gitignored), not {}".format(what, scratch_root, path))


def strip_json_comments(text):
	"""The text with // and /* */ comments removed; a marker inside a "string" is left alone (HB-0262)."""
	out = []
	i = 0
	in_string = False
	while i < len(text):
		char = text[i]
		pair = text[i:i + 2]
		if in_string:
			out.append(char)
			if char == "\\" and i + 1 < len(text):
				out.append(text[i + 1])
				i += 2
				continue
			if char == '"':
				in_string = False
			i += 1
		elif char == '"':
			in_string = True
			out.append(char)
			i += 1
		elif pair == "//":
			while i < len(text) and text[i] != "\n":
				i += 1
		elif pair == "/*":
			end = text.find("*/", i + 2)
			i = len(text) if end < 0 else end + 2
		else:
			out.append(char)
			i += 1
	return "".join(out)


def read_json_file(path):
	"""A JSON file that may carry a UTF-8 BOM and // or /* */ comments (the server configs do)."""
	with open(path, "r", encoding="utf-8-sig") as handle:
		return json.loads(strip_json_comments(handle.read()))


def merge_json(base, patch):
	"""Lays patch over base in place: objects merge key by key, anything else replaces."""
	for key, value in patch.items():
		if isinstance(value, dict) and isinstance(base.get(key), dict):
			merge_json(base[key], value)
		else:
			base[key] = value
	return base


def read_server_config(path):
	"""A server json as the servers read it: <name>.json, then the untracked <name>.local.json over it."""
	config = read_json_file(path)
	stem, extension = os.path.splitext(path)
	local = stem + ".local" + extension
	if os.path.isfile(local):
		override = read_json_file(local)
		if isinstance(override, dict):
			merge_json(config, override)
	return config


def write_json_file(path, value):
	os.makedirs(os.path.dirname(path), exist_ok=True)
	with open(path, "w", encoding="utf-8", newline="\n") as handle:
		json.dump(value, handle, indent="\t", sort_keys=True)
		handle.write("\n")


def ascii_json(value):
	"""JSON for the console: ASCII only (the console here is cp1252)."""
	return json.dumps(value, indent=2, sort_keys=True, ensure_ascii=True, default=str)
