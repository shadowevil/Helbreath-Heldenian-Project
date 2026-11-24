"""Self-test for hb_driver and hb_flow.py against an in-process FAKE client - no game, no servers.

The fake speaks the bridge wire (newline JSON, one connection at a time, wait_for answered by a
later frame) and plays the parts of the client the flows lean on: screens with fades, modal
overlays that disable the page's widgets (busy, then a message dismissed by a click), the widget
ids and values the real screens report, a journal with seq / frame / t_ms, probes with a
non-square tile pitch, click-to-move with move_request / move_confirm, and client.json persisted
on a mute change. It checks the LIBRARY's logic, not the client's side of the contract - only a
live run does that (.claude/docs/agent_bridge_playbook.md).

Run from anywhere:  python .claude/tools/agent_mcp/hb_driver_selftest.py
Scratch goes under .claude/scratch/hb_driver_selftest/ and is deleted at the end.
Exits non-zero on any failure.
"""

import json
import math
import os
import re
import shutil
import socket
import subprocess
import sys
import threading
import time

here = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, here)

from hb_driver import bridge  # noqa: E402
from hb_driver import client_files  # noqa: E402
from hb_driver import client_process  # noqa: E402
from hb_driver import common  # noqa: E402
from hb_driver import flows  # noqa: E402
from hb_driver import identity  # noqa: E402
from hb_driver import mcp_flows  # noqa: E402
from hb_driver import player_db  # noqa: E402
from hb_driver import server_logs  # noqa: E402
from hb_driver import world  # noqa: E402

scratch = os.path.join(common.repo_root, ".claude", "scratch", "hb_driver_selftest")
failures = []

frame_s = 0.01
busy_frames = 6
fade_frames = 3
move_answer_frames = 3
out_w, out_h = 1920, 1080
world_ui_width = 853
presets = ["Archer", "Battlemage", "Mage", "Support", "Warrior"]


def check(label, condition, detail=""):
	if condition:
		print("  PASS  {}".format(label))
	else:
		print("  FAIL  {}  {}".format(label, str(detail)[:400]))
		failures.append(label)


def expect_error(label, action, needle):
	try:
		action()
	except common.flow_error as error:
		check(label, needle in str(error), error)
		return
	check(label, False, "no error raised")


def widget(wid, kind, x, y, w, h, label="", value=None, enabled=True):
	out = {"id": wid, "kind": kind, "x": x, "y": y, "w": w, "h": h, "label": label, "enabled": enabled}
	if value is not None:
		out["value"] = value
	return out


class fake_client:
	"""This client's screens as far as the flows need them: main menu, login, character select, the world. It answers the
	commands strictly (an unknown argument key is refused, as the C++ does), plays the widget names the real screens
	report, and never echoes a typed value: a masked field holds its text in `typed` for the test to inspect."""

	accepted = {
		"ping": "", "get_state": "", "widgets": "", "widget_click": "name button count shift ctrl timeout_ms",
		"click": "x y space button count shift ctrl timeout_ms", "set_text": "value clear timeout_ms",
		"wait_for": "conditions timeout_ms since", "tile_to_screen": "x y", "walk_to": "x y timeout_ms wait",
		"entities": "kinds radius max on_screen", "close_window": "name dialog_id", "set_audio": "master music effects",
		"key": "name shift ctrl alt repeat timeout_ms", "probe_point": "x y space",
	}

	def __init__(self):
		self.quit = False
		self.lock = threading.RLock()
		self.screen = "Screen_MainMenu"
		self.overlay = None
		self.focus = None
		self.typed = {}                 # widget -> text typed into it
		self.accounts = {"goodacct": "goodpass1"}
		self.slots = ["Alpha", "Beta", None, None]
		self.selected = None
		self.drop_first_char_once = False
		self.player = (134, 37)
		self.in_world = False
		self.character = None
		self.seen = []
		self.walk_status = "arrived"
		self.audio = {"master": 100, "music": 80, "effects": 70}
		self.windows = {"inventory": 2}

	def tick(self):
		return []

	def state(self):
		out = {"screen": self.screen, "overlay": self.overlay, "transitioning": False, "in_world": self.in_world,
		       "focus_id": None, "mouse_x": 0, "mouse_y": 0}
		if self.focus in ("login.name", "login.password"):
			out["focus_masked"] = self.focus == "login.password"
			if self.focus == "login.name":
				out["focus_text_length"] = len(self.typed.get("login.name", ""))
		if self.in_world:
			out.update({"character": self.character, "player_x": self.player[0], "player_y": self.player[1],
			            "map": "default"})
		return out

	def rows(self):
		def row(name, kind, x, y, w=100, h=20, **extra):
			base = {"name": name, "kind": kind, "owner": "screen:" + self.screen, "enabled": True, "visible": True,
			        "focused": self.focus == name, "rect": {"x": x, "y": y, "w": w, "h": h}}
			base.update(extra)
			return base
		if self.overlay == "Overlay_LogResMsg":
			return [row("log_res_msg.ok", "button", 300, 300)]
		if self.screen == "Screen_MainMenu":
			return [row("main_menu.login", "button", 465, 238), row("main_menu.new_account", "button", 465, 276),
			        row("main_menu.quit", "button", 465, 315)]
		if self.screen == "Screen_Login":
			return [row("login.name", "text_input", 300, 200, masked=False, length=len(self.typed.get("login.name", ""))),
			        row("login.password", "text_input", 300, 230, masked=True), row("login.ok", "button", 300, 280),
			        row("login.cancel", "button", 400, 280)]
		if self.screen == "Screen_SelectCharacter":
			return [row("select_character.slot{}".format(n), "slot", 10 + n * 50, 100) for n in range(1, 5)] + \
			       [row("select_character.enter", "button", 300, 400), row("select_character.exit", "button", 400, 400)]
		return []

	def command(self, conn, request):
		with self.lock:
			cmd, rid, args = request.get("cmd"), request.get("id"), request.get("args") or {}
			self.seen.append({"cmd": cmd, "args": dict(args)})
			unknown = sorted(set(args) - set(self.accepted.get(cmd, "").split()))
			if cmd not in self.accepted or unknown:
				return {"id": rid, "ok": False, "error": "{}: unknown argument '{}'".format(cmd, unknown[0] if unknown else cmd)}
			try:
				return {"id": rid, "ok": True, "result": self.run(cmd, args)}
			except ValueError as error:
				return {"id": rid, "ok": False, "error": str(error)}

	def run(self, cmd, args):
		if cmd == "ping":
			return {"build": "0.0.0-selftest", "bridge_protocol": 5, "commands": sorted(self.accepted)}
		if cmd == "get_state":
			return self.state()
		if cmd == "widgets":
			return {"surface": self.screen, "overlay": self.overlay is not None, "transitioning": False, "widgets": self.rows()}
		if cmd == "widget_click":
			return self.click_widget(args["name"])
		if cmd == "set_text":
			if self.focus not in ("login.name", "login.password"):
				raise ValueError("set_text needs a focused text field")
			text = args["value"]
			if self.drop_first_char_once:
				self.drop_first_char_once = False
				text = text[1:]
			self.typed[self.focus] = text
			return {"chars": len(text)}
		if cmd == "wait_for":
			state = self.state()
			for index, condition in enumerate(args["conditions"]):
				if state.get(condition.get("state")) == condition.get("equals"):
					return {"satisfied": True, "matched": {"index": index, "observed": condition["equals"]}, "state": state}
			time.sleep(min(args.get("timeout_ms", 0), 200) / 1000.0)
			return {"satisfied": False, "state": self.state()}
		if cmd == "tile_to_screen":
			self.need_world()
			x, y = args["x"], args["y"]
			dx, dy = x - self.player[0], y - self.player[1]
			on_screen = abs(dx) <= 9 and abs(dy) <= 7
			window = [960 + dx * 32, 540 + dy * 32]
			return {"tile": [x, y], "in_bounds": 0 <= x < 300 and 0 <= y < 300, "on_screen": on_screen,
			        "logical": [400 + dx * 32, 300 + dy * 32], "window": window, "player_tile": list(self.player),
			        "resolves_to": [x, y], "round_trip": True}
		if cmd == "click":
			self.need_world()
			dx, dy = round((args["x"] - 960) / 32), round((args["y"] - 540) / 32)
			self.player = (self.player[0] + dx, self.player[1] + dy)
			return {"x": args["x"], "y": args["y"], "button": args["button"], "consumed_by": {"kind": "world", "owner": "screen:Screen_OnGame"}}
		if cmd == "walk_to":
			self.need_world()
			x, y = args["x"], args["y"]
			if x >= 300:
				raise ValueError("walk_to refused: ({},{}) is outside the map".format(x, y))
			if not args.get("wait", True):
				return {"status": "walking", "goal": [x, y], "from": list(self.player)}
			start = self.player
			if self.walk_status == "arrived":
				self.player = (x, y)
			return {"status": self.walk_status, "goal": [x, y], "from": list(start), "at": list(self.player), "map": "default",
			        "elapsed_ms": 900}
		if cmd == "entities":
			self.need_world()
			rows = [{"object_id": 19, "kind": "npc", "name": "gandalf", "tile": [136, 36], "distance": 2, "on_screen": True,
			         "dead": False, "point": {"logical": [464, 268], "window": [1024, 508]}}]
			return {"player_tile": list(self.player), "map": "default", "total": 1, "count": 1, "truncated": False, "entities": rows}
		if cmd == "close_window":
			self.need_world()
			name = args.get("name")
			if name == "all":
				closed = [{"name": key, "dialog_id": value} for key, value in sorted(self.windows.items())]
				self.windows = {}
				return {"closed": closed}
			if name not in ("inventory", "magic") and "dialog_id" not in args:
				raise ValueError("close_window: unknown window name '{}'".format(name))
			if name in self.windows:
				return {"closed": [{"name": name, "dialog_id": self.windows.pop(name)}]}
			return {"closed": []}
		if cmd == "set_audio":
			self.audio.update(args)
			return dict(self.audio, persisted=False)
		raise ValueError("{} is not played by this fake".format(cmd))

	def need_world(self):
		if not self.in_world:
			raise ValueError("needs the world (not logged in)")

	def click_widget(self, name):
		row = next((item for item in self.rows() if item["name"] == name), None)
		if row is None:
			raise ValueError("no widget named '{}' on the screen that takes input (see widgets)".format(name))
		self.focus = name if row["kind"] == "text_input" else self.focus
		if name == "main_menu.login":
			self.screen, self.focus = "Screen_Login", None
		elif name == "login.cancel":
			self.screen = "Screen_MainMenu"
		elif name == "log_res_msg.ok":
			self.overlay, self.screen = None, "Screen_MainMenu"
		elif name == "login.ok":
			if self.accounts.get(self.typed.get("login.name")) == self.typed.get("login.password"):
				self.screen = "Screen_SelectCharacter"
			else:
				self.overlay = "Overlay_LogResMsg"
		elif name == "select_character.exit":
			self.screen = "Screen_MainMenu"
		elif name.startswith("select_character.slot"):
			self.selected = int(name[-1]) - 1
		elif name == "select_character.enter":
			if self.selected is None or self.slots[self.selected] is None:
				raise ValueError("no character in that slot")
			self.screen, self.in_world, self.character = "Screen_OnGame", True, self.slots[self.selected]
		return {"widget": name, "enabled": True, "x": row["rect"]["x"], "y": row["rect"]["y"], "button": "left"}


class fake_server:
	"""One connection at a time, like the client; a frame thread settles waits."""

	def __init__(self, game):
		self.game = game
		self.listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		self.listener.bind(("127.0.0.1", 0))
		self.listener.listen(4)
		self.port = self.listener.getsockname()[1]
		self.conns = []
		self.stopped = False
		threading.Thread(target=self.accept_loop, daemon=True).start()
		threading.Thread(target=self.frame_loop, daemon=True).start()

	def send(self, conn, reply):
		try:
			conn.sendall((json.dumps(reply) + "\n").encode("utf-8"))
		except OSError:
			pass

	def accept_loop(self):
		while not self.stopped:
			try:
				conn, _ = self.listener.accept()
			except OSError:
				return
			self.serve(conn)  # sequential: a second connection waits, as with the real client

	def serve(self, conn):
		buffer = b""
		while not self.stopped:
			try:
				chunk = conn.recv(65536)
			except OSError:
				break
			if not chunk:
				break
			buffer += chunk
			while b"\n" in buffer:
				line, _, buffer = buffer.partition(b"\n")
				if line.strip():
					reply = self.game.command(conn, json.loads(line))
					if reply is not None:
						self.send(conn, reply)
			if self.game.quit:
				break
		conn.close()

	def frame_loop(self):
		while not self.stopped:
			for conn, reply in self.game.tick():
				self.send(conn, reply)
			time.sleep(frame_s)

	def stop(self):
		self.stopped = True
		self.listener.close()


class fake_db:
	"""The four statement shapes player_db issues, over dicts."""

	def __init__(self):
		self.accounts = {1: "hbdaccount", 2: "ShadowEvil"}
		self.characters = {"Hbdchar": (100, 1), "Owned": (200, 2)}
		self.positions = {100: [1, 134, 37, 4], 200: [1, 10, 10, 4]}
		self.maps = {1: "travelerzone", 77: "efarmwarehouse"}
		self.updates = 0

	def query(self, sql, schema):
		match = re.search(r"WHERE c\.name = '(\w+)'", sql)
		if match:
			row = self.characters.get(match.group(1))
			return [[str(row[0]), str(row[1]), self.accounts[row[1]]]] if row else []
		match = re.search(r"FROM maps WHERE id = (\d+)", sql)
		if match:
			number = int(match.group(1))
			return [[str(number), self.maps[number]]] if number in self.maps else []
		match = re.search(r"FROM maps WHERE internal_name = '(\w+)'", sql)
		if match:
			return [[str(k), v] for k, v in self.maps.items() if v == match.group(1)]
		match = re.search(r"FROM character_position WHERE character_id = (\d+)", sql)
		if match:
			return [[str(v) for v in self.positions[int(match.group(1))]]]
		match = re.search(r"SET map_id = (\d+), x = (-?\d+), y = (-?\d+), direction = (\d+) WHERE character_id = (\d+)", sql)
		if match:
			values = [int(v) for v in match.groups()]
			self.positions[values[4]] = values[:4]
			self.updates += 1
			return []
		raise AssertionError("unexpected sql: " + sql)


# ------------------------------------------------------------------------------------------ tests

def test_units():
	print("units")
	check("valid_name accepts 3..10 alnum", identity.valid_name("abc") and identity.valid_name("a1b2c3d4e5"))
	check("valid_name refuses 11 chars / symbols / the user's account",
	      not identity.valid_name("a" * 11) and not identity.valid_name("ab-c") and not identity.valid_name("ShadowEvil"))
	names = [identity.random_name("hbd") for _ in range(50)]
	check("random names are valid and 10 long", all(identity.valid_name(n) and len(n) == 10 for n in names), names[:3])
	check("random names differ", len(set(names)) == len(names))
	password = identity.random_password()
	check("password is 10 printable alnum", len(password) == 10 and password.isalnum(), password)
	expect_error("identity file outside .claude/scratch/ is refused",
	             lambda: identity.new_identity(os.path.join(common.repo_root, "ident.json")), "scratch")
	expect_error("identity file must be absolute", lambda: identity.new_identity("ident.json"), "absolute")

	path = os.path.join(scratch, "ident_unit.json")
	made = identity.new_identity(path, characters=2)
	shown = made.public()
	check("public() masks the password", shown["password"] == "*" * 10 and made.password not in json.dumps(shown))
	check("two character names reserved, not created",
	      len(made.characters) == 2 and not any(c["created"] for c in made.characters))
	expect_error("new_identity refuses to overwrite", lambda: identity.new_identity(path), "exists")
	check("load_identity round-trips", identity.load_identity(path).account == made.account)
	check("load_identity takes a repo-relative path (HB-0625)",
	      identity.load_identity(os.path.relpath(path, common.repo_root)).account == made.account)
	check("created_character false before creation", not made.created_character(made.characters[0]["name"]))
	check("delete_identity removes the file", identity.delete_identity(path) and not os.path.exists(path))

	fake_exe = os.path.join(scratch, "fake_debug.exe")
	with open(fake_exe, "wb") as handle:
		handle.write(b"MZ....agent bridge: listening on %s:%d....")
	fake_release = os.path.join(scratch, "fake_release.exe")
	with open(fake_release, "wb") as handle:
		handle.write(b"MZ....nothing here....")
	check("exe_has_bridge sees the debug marker", client_process.exe_has_bridge(fake_exe))
	check("exe_has_bridge refuses a release exe", not client_process.exe_has_bridge(fake_release))
	expect_error("pick_client_exe refuses a bridgeless exe", lambda: client_process.pick_client_exe(fake_release), "bridge")
	process = client_process.client_process(8899, exe=fake_exe)
	line = process.command_line()
	check("this client takes no --asset-root or --start-screen", "--asset-root" not in line and "--start-screen" not in line, line)
	check("the command line carries the port as --agent-bridge-port=N", line[1:] == ["--agent-bridge-port=8899"], line)

	pid_file = os.path.join(scratch, "pids.txt")
	client_process.record_pid(pid_file, 999999, 8899)
	check("pid file round-trips", client_process.recorded_pids(pid_file) == [(999999, 8899)])
	report = client_process.stop_recorded(pid_file)
	check("a recorded pid that no longer runs is reported gone, nothing killed",
	      report == [{"pid": 999999, "port": 8899, "action": "already gone"}], report)
	expect_error("stop refuses a pid it did not launch", lambda: client_process.stop_recorded(pid_file, pid=4), "only PIDs")


def test_client_files():
	print("client_files_guard")
	json_path = os.path.join(scratch, "files", "client.json")
	absent_path = os.path.join(scratch, "files", "remembered_account.txt")
	os.makedirs(os.path.dirname(json_path), exist_ok=True)
	original = b'{\r\n  "audio": {"master_muted": false}\r\n}\r\n'
	with open(json_path, "wb") as handle:
		handle.write(original)
	backup = os.path.join(scratch, "files_backup")
	with client_files.client_files_guard([json_path, absent_path], backup) as guard:
		with open(json_path, "w") as handle:
			handle.write('{"audio": {"master_muted": true}}')
		with open(absent_path, "w") as handle:
			handle.write("hbdsomeone")
	check("restored byte-identical (CRLF kept)", client_files.read_bytes(json_path) == original)
	check("a file that did not exist is removed again", not os.path.exists(absent_path))
	check("the report proves both by hash", all(row["ok"] for row in guard.report), guard.report)
	with open(json_path, "w") as handle:
		handle.write("changed after")
	restored = client_files.restore_from_backup(backup)
	check("restore_from_backup (a crashed run) also restores", client_files.read_bytes(json_path) == original, restored)
	expect_error("a backup dir outside .claude/scratch/ is refused",
	             lambda: client_files.client_files_guard([json_path], os.path.join(common.repo_root, "x")).save(), "scratch")


def test_logs():
	print("server_logs")
	log_path = os.path.join(scratch, "world.log")
	with open(log_path, "w") as handle:
		handle.write("[2026-09-30 09:00:00.000] [INFO] old line before the mark\n")
	watch = server_logs.log_watch(log_path)
	lines = [
		"[2026-09-30 09:00:01.000] [DEBUG] save queued: character 9 -> 'travelerzone' (134,37) dir 4 - 3 statement(s)",
		"[2026-09-30 09:00:01.500] [INFO] world save (periodic): 0.7 ms",
		"[2026-09-30 09:00:02.000] [DEBUG] persistence: batch 3 statement(s)",
		"[2026-09-30 09:00:05.000] [DEBUG] save queued: character 9 -> 'efarmwarehouse' (56,36) dir 2 (dead -> revived state) - 3 statement(s)",
		"[2026-09-30 09:00:05.001] [INFO] client session closed (0 session(s))",
		"[2026-09-30 09:00:05.030] [DEBUG] persistence: batch 3 statement(s) in 1 unit(s)",
	]
	with open(log_path, "a") as handle:
		handle.write("\n".join(lines) + "\n")
	proof = server_logs.logout_save(watch.records(), 9)
	check("a periodic save (no session close) is not a logout proof; the logout save is",
	      proof and (proof["map"], proof["x"], proof["y"], proof["direction"]) == ("efarmwarehouse", 56, 36, 2), proof)
	check("another character has no proof", server_logs.logout_save(watch.records(), 10) is None)
	check("records before the mark are not read", all("old line" not in r["text"] for r in watch.records()))
	record = server_logs.parse_line(lines[0])
	check("log time parses to ms", record and record["wall_ms"] % 1000 == 0 and record["level"] == "DEBUG")
	expect_error("wait_logout_saved times out cleanly", lambda: server_logs.wait_logout_saved(watch, 10, 0.2), "no logout save")


def test_position_guard():
	print("player_db write_position guard")
	db = fake_db()
	path = os.path.join(scratch, "ident_db.json")
	ident = identity.identity(path, {"account": "hbdaccount", "password": "x" * 10, "account_created": True,
	                                 "characters": [{"name": "Hbdchar", "created": True}, {"name": "Owned", "created": True}]})
	ident.save()
	backups = os.path.join(scratch, "backups")
	saved_backups = common.backups_dir
	common.backups_dir = backups
	try:
		os.makedirs(os.path.join(backups, "20260930-000000-unit", "mysql"), exist_ok=True)
		with open(os.path.join(backups, "20260930-000000-unit", "mysql", "hb_player.sql"), "w") as handle:
			handle.write("-- dump")
		log_path = os.path.join(scratch, "world_db.log")
		with open(log_path, "w") as handle:
			handle.write("")
		watch = server_logs.log_watch(log_path)
		with open(log_path, "a") as handle:
			handle.write("[2026-09-30 09:00:05.000] [DEBUG] save queued: character 100 -> 'travelerzone' (134,37) dir 4 - 3 statement(s)\n"
			             "[2026-09-30 09:00:05.001] [INFO] client session closed (0 session(s))\n"
			             "[2026-09-30 09:00:05.030] [DEBUG] persistence: batch 3 statement(s)\n")
		proof = server_logs.wait_logout_saved(watch, 100, 1.0)
		base = dict(logout_proof=proof, snapshot_set="20260930-000000-unit")
		expect_error("refused: a character another account owns",
		             lambda: player_db.write_position(db, ident, "Owned", "efarmwarehouse", 56, 36, **base), "belongs to account")
		expect_error("refused: a character not created by this tooling",
		             lambda: player_db.write_position(db, ident, "Nobody", "efarmwarehouse", 56, 36, **base), "not created")
		expect_error("refused: no logout proof",
		             lambda: player_db.write_position(db, ident, "Hbdchar", "efarmwarehouse", 56, 36, snapshot_set="20260930-000000-unit"),
		             "logout_proof")
		expect_error("refused: no snapshot",
		             lambda: player_db.write_position(db, ident, "Hbdchar", "efarmwarehouse", 56, 36, logout_proof=proof), "snapshot")
		expect_error("refused: a snapshot set with no hb_player dump",
		             lambda: player_db.write_position(db, ident, "Hbdchar", "efarmwarehouse", 56, 36, logout_proof=proof,
		                                              snapshot_set="20260930-999999-none"), "no hb_player dump")
		db.positions[100] = [1, 135, 37, 4]
		expect_error("refused: the row differs from the logged save",
		             lambda: player_db.write_position(db, ident, "Hbdchar", "efarmwarehouse", 56, 36, **base), "does not equal")
		db.positions[100] = [1, 134, 37, 4]
		check("no UPDATE was issued by any refusal", db.updates == 0)
		result = player_db.write_position(db, ident, "Hbdchar", "efarmwarehouse", 56, 36, **base)
		check("the guarded write lands and is re-read", result["after"]["map"] == "efarmwarehouse"
		      and (result["after"]["x"], result["after"]["y"]) == (56, 36) and db.updates == 1, result)
		with open(log_path, "a") as handle:
			handle.write("[2026-09-30 09:00:09.000] [INFO] player entered: 'Hbdchar' (object 5) at (56,36) on 'efarmwarehouse' [hp 1/1]\n")
		db.positions[100] = [1, 134, 37, 4]
		expect_error("refused: the character entered the world after its logout save",
		             lambda: player_db.write_position(db, ident, "Hbdchar", "travelerzone", 1, 1, **base), "entered the world")
	finally:
		common.backups_dir = saved_backups


def test_identity():
	print("test accounts and credentials (never echoed)")
	accounts = os.path.join(scratch, "test_accounts.json")
	common.write_json_file(accounts, {"_about": "x", "accounts": [
		{"name": "first8489", "password": "pw-first-1", "characters": ["Alpha", "Beta"]},
		{"name": "second999", "password": "pw-second-2", "characters": []}]})
	first = identity.test_account(path=accounts)
	named = identity.test_account("second999", path=accounts)
	check("test_account reads the first entry, or the one named", first.account == "first8489" and named.account == "second999")
	check("its characters are the slot order", [entry["name"] for entry in first.characters] == ["Alpha", "Beta"])
	check("the password is held but never printed: public() masks it, repr hides it",
	      first.password == "pw-first-1" and "pw-first-1" not in json.dumps(first.public()) and "pw-first-1" not in repr(first)
	      and first.public()["password"] == "*" * len(first.password), first.public())
	check("saving a credentials object writes nothing", first.save() is None and not os.path.exists(os.path.join(scratch, "None")))
	expect_error("an unknown test account is refused", lambda: identity.test_account("nobody", path=accounts), "no usable test account")
	expect_error("a missing accounts file is a named error", lambda: identity.test_account(path=os.path.join(scratch, "none.json")), "no test accounts file")
	saved = {key: os.environ.get(key) for key in ("HB_TEST_ACCOUNT", "HB_TEST_PASSWORD", "HB_TEST_CHARACTERS")}
	try:
		for key in saved:
			os.environ.pop(key, None)
		expect_error("from_env needs both variables", identity.from_env, "HB_TEST_ACCOUNT")
		os.environ.update({"HB_TEST_ACCOUNT": "envacct", "HB_TEST_PASSWORD": "envpass99", "HB_TEST_CHARACTERS": "One, Two"})
		from_env = identity.from_env()
		check("from_env reads the account, password and slot-ordered characters",
		      from_env.account == "envacct" and from_env.password == "envpass99"
		      and [entry["name"] for entry in from_env.characters] == ["One", "Two"])
	finally:
		for key, value in saved.items():
			if value is None:
				os.environ.pop(key, None)
			else:
				os.environ[key] = value


def test_mcp_flow_arguments():
	print("hb_flow argument checks")
	check("a hyphenated name is canonicalized", mcp_flows.check_arguments({"flow": "click-tile", "x": 1, "y": 2}) == "click_tile")
	check("so is a mixed-case one", mcp_flows.check_arguments({"flow": "Wait-Screen", "screen": "x"}) == "wait_screen")
	expect_error("walk_to is a tool, not a flow, and the error names it",
	             lambda: mcp_flows.check_arguments({"flow": "walk-to", "x": 1, "y": 2}), "hb_walk_to")
	expect_error("mute is a tool, and the error names it", lambda: mcp_flows.check_arguments({"flow": "mute"}), "hb_set_audio")
	for flow in ("create_account", "create_character", "delete_character", "switch", "logout", "time_entry"):
		expect_error("{} is named as not ported".format(flow), lambda flow=flow: mcp_flows.check_arguments({"flow": flow}), "not ported")
	check("step takes walk_fallback", mcp_flows.check_arguments({"flow": "step", "direction": "n", "walk_fallback": True}) == "step")
	check("an absent walk_fallback is None, a given one is forced either way",
	      mcp_flows._fallback({}) is None and mcp_flows._fallback({"walk_fallback": True}) is True
	      and mcp_flows._fallback({"walk_fallback": False}) is False)
	check("client and timeout_s are accepted by every flow",
	      mcp_flows.check_arguments({"flow": "main_menu", "client": "c1", "timeout_s": 5}) == "main_menu")
	check("login takes test_account, env, identity_file and force",
	      mcp_flows.check_arguments({"flow": "login", "test_account": True, "force": True}) == "login"
	      and mcp_flows.check_arguments({"flow": "login", "env": True}) == "login"
	      and mcp_flows.check_arguments({"flow": "login", "identity_file": "x"}) == "login")
	check("enter takes a slot or a name", mcp_flows.check_arguments({"flow": "enter", "slot": 2}) == "enter"
	      and mcp_flows.check_arguments({"flow": "enter", "name": "Alpha", "test_account": "a"}) == "enter")
	expect_error("a password is never an argument, and the error says so",
	             lambda: mcp_flows.check_arguments({"flow": "login", "password": "x"}), "never an argument")
	expect_error("an args wrapper is refused with the fix", lambda: mcp_flows.check_arguments(
		{"flow": "click_tile", "args": {"x": 1, "y": 2}}), "top-level")
	expect_error("a key another flow owns is refused", lambda: mcp_flows.check_arguments(
		{"flow": "step", "direction": "n", "x": 1}), "does not take: x")
	expect_error("a missing required key is named", lambda: mcp_flows.check_arguments({"flow": "click_tile", "x": 1}), "needs y")
	expect_error("an unknown flow lists the flows", lambda: mcp_flows.check_arguments({"flow": "nope"}), "main_menu")
	expect_error("proof_file is gone with the logout flows", lambda: mcp_flows.check_arguments(
		{"flow": "enter", "proof_file": "x"}), "does not take")


def test_flows():
	print("flows and world moves against the fake client")
	game = fake_client()
	server = fake_server(game)
	ident = identity.credentials("goodacct", "goodpass1", ["Alpha", "Beta"])
	try:
		sess = bridge.session.connect(server.port, timeout_s=5)
		check("connect answers ping", sess.ping().get("bridge_protocol") == 5)
		listing = sess.widgets()
		check("widgets come back with id (the name) and x, y, w, h read off the rect",
		      listing["widgets"][0]["id"] == "main_menu.login" and listing["widgets"][0]["x"] == 465 and listing["widgets"][0]["w"] == 100,
		      listing["widgets"][0])
		expect_error("wait_screen names where it is stuck", lambda: sess.wait_screen("nowhere", 0.3), "Screen_MainMenu")

		# --- login ------------------------------------------------------------------------------------------------
		bad = identity.credentials("goodacct", "wrongpass1")
		try:
			flows.log_in(sess, bad, timeout_s=5)
			refused = ""
		except common.flow_error as error:
			refused = str(error)
		check("a refused login raises, names the overlay, and never carries the password",
		      "login was refused (Overlay_LogResMsg)" in refused and "wrongpass1" not in refused, refused)
		check("the refusal message was dismissed", game.overlay is None and game.screen == "Screen_MainMenu", game.screen)
		game.drop_first_char_once = True
		landed = flows.log_in(sess, ident, timeout_s=5)
		check("log_in reaches character select (after a retried name whose first character was dropped)",
		      landed["screen"] == "Screen_SelectCharacter" and landed["account"] == "goodacct"
		      and [slot["slot"] for slot in landed["slots"]] == [1, 2, 3, 4], landed)
		check("the password went into the masked field only, and nothing returned holds it",
		      game.typed["login.password"] == "goodpass1" and "goodpass1" not in json.dumps(landed), landed)
		sent = [row for row in game.seen if row["cmd"] == "widget_click"]
		check("every widget click is by name, with only the client's keys", sent and all(set(row["args"]) <= {"name", "button", "shift", "ctrl"} for row in sent))
		expect_error("log_in from the wrong screen is refused", lambda: flows.log_in(sess, ident, timeout_s=2), "starts on the main menu")

		# --- select / enter -------------------------------------------------------------------------------------------------
		check("slot_for maps a name to its position on the account, a slot to itself, nothing to 1",
		      flows.slot_for(ident, "Beta") == 2 and flows.slot_for(ident, None, 3) == 3 and flows.slot_for(None) == 1)
		expect_error("an unknown character is named", lambda: flows.slot_for(ident, "Nobody"), "not listed")
		expect_error("slot 9 is refused", lambda: flows.slot_for(None, None, 9), "slot must be")
		expect_error("an empty slot is refused by the client and the flow says so",
		             lambda: flows.enter_world(sess, slot=3, timeout_s=3), "no character in that slot")
		game.screen = "Screen_SelectCharacter"
		expect_error("entering as the wrong character is caught", lambda: flows.enter_world(sess, "Beta", timeout_s=3, ident=identity.credentials("goodacct", "x", ["Beta", "Alpha"])), "entered as")
		game.screen, game.in_world, game.character = "Screen_SelectCharacter", False, None
		entered = flows.enter_world(sess, "Beta", timeout_s=5, ident=ident)
		check("enter_world picks the slot by name and proves the character", entered["character"] == "Beta"
		      and entered["slot"] == 2 and entered["player_tile"] == [134, 37], entered)

		# --- the world ------------------------------------------------------------------------------------------------------------
		check("player_tile reads get_state", world.player_tile(sess) == (134, 37))
		aim = world.aim_tile(sess, (136, 37))
		check("aim_tile returns the pixel tile_to_screen gives (32 per tile)", aim["window"] == [1024, 540], aim)
		expect_error("a tile off screen is refused", lambda: world.aim_tile(sess, (200, 37)), "off screen")
		expect_error("a tile outside the map is refused", lambda: world.aim_tile(sess, (-5, 37)), "outside the map")
		clicked = world.click_tile(sess, (135, 37))
		check("click_tile clicks the aimed window pixel and arrives", clicked["arrived"] and clicked["player_tile"] == [135, 37]
		      and clicked["consumed_by"]["kind"] == "world", clicked)
		stepped = world.step(sess, "n")
		check("step is a click on the neighbouring tile", stepped["tile"] == [135, 36] and stepped["arrived"], stepped)
		expect_error("a bad direction is refused", lambda: world.step(sess, "up"), "direction must be")
		check("tile_pixels is 32", world.tile_pixels == 32)
		walked = world.walk_to(sess, (140, 40))
		check("walk_to returns the client's own reply", walked["status"] == "arrived" and walked["at"] == [140, 40], walked)
		game.walk_status = "blocked"
		check("a blocked walk is a reply, not an exception", world.walk_to(sess, (150, 40))["status"] == "blocked")
		game.walk_status = "arrived"
		check("walk_to without waiting says walking", world.walk_to(sess, (141, 40), arrive=False)["status"] == "walking")
		walk_wire = [row["args"] for row in game.seen if row["cmd"] == "walk_to"]
		check("walk_to sends x, y, wait and (when waiting) timeout_ms only",
		      all(set(args) <= {"x", "y", "wait", "timeout_ms"} for args in walk_wire) and "timeout_ms" not in walk_wire[-1], walk_wire)
		expect_error("a walk outside the map is the client's refusal", lambda: world.walk_to(sess, (999, 4)), "outside the map")
		check("entities, close_window and set_audio are session calls on the client's keys",
		      sess.entities(max_rows=5)["entities"][0]["name"] == "gandalf"
		      and sess.close_window("inventory")["closed"][0]["dialog_id"] == 2
		      and sess.close_window("inventory")["closed"] == []
		      and sess.set_audio(master=10)["master"] == 10 and sess.set_audio()["music"] == 80)
		expect_error("time_entry is named as not ported", lambda: world.time_entry(sess), "not ported")
		for name in ("create_account", "log_out", "switch_character", "quit_client"):
			expect_error("flows.{} says it is not ported".format(name), lambda name=name: getattr(flows, name)(sess, ident) if name == "create_account" else getattr(flows, name)(sess), "not ported")

		# --- back to the menus ----------------------------------------------------------------------------------------------------
		game.screen, game.in_world = "Screen_SelectCharacter", False
		check("go_to_main_menu leaves character select", flows.go_to_main_menu(sess, 5)["screen"] == "Screen_MainMenu")
		game.screen = "Screen_Login"
		check("and the login screen", flows.go_to_main_menu(sess, 5)["screen"] == "Screen_MainMenu")
		sess.close()

		# --- the CLI -------------------------------------------------------------------------------------------------------------------
		print("CLI against the fake client")
		cli = [sys.executable, os.path.join(here, "hb_flow.py")]
		run = subprocess.run(cli + ["state", "--port", str(server.port)], capture_output=True, text=True, timeout=60)
		check("hb_flow.py state prints JSON", run.returncode == 0 and json.loads(run.stdout)["screen"] == "Screen_MainMenu",
		      run.stdout + run.stderr)
		env = dict(os.environ, HB_TEST_ACCOUNT="goodacct", HB_TEST_PASSWORD="goodpass1", HB_TEST_CHARACTERS="Alpha,Beta")
		run = subprocess.run(cli + ["login", "--port", str(server.port), "--env"], capture_output=True, text=True, timeout=60, env=env)
		check("hb_flow.py login --env logs in and prints no password",
		      run.returncode == 0 and json.loads(run.stdout)["screen"] == "Screen_SelectCharacter" and "goodpass1" not in run.stdout + run.stderr,
		      run.stdout + run.stderr)
		run = subprocess.run(cli + ["enter", "--port", str(server.port), "--env", "--name", "Alpha"], capture_output=True, text=True, timeout=60, env=env)
		check("hb_flow.py enter --name proves the character", run.returncode == 0 and json.loads(run.stdout)["character"] == "Alpha",
		      run.stdout + run.stderr)
		run = subprocess.run(cli + ["login", "--port", str(server.port)], capture_output=True, text=True, timeout=60)
		check("a login with no credentials is ERROR on stderr, exit 1", run.returncode == 1 and run.stderr.startswith("ERROR:")
		      and "--test-account" in run.stderr, run.stdout + run.stderr)
		run = subprocess.run(cli + ["--help"], capture_output=True, text=True, timeout=60)
		check("--help lists the flows", run.returncode == 0 and "step" in run.stdout and "login" in run.stdout)
		files = os.path.join(scratch, "fake_bin")
		relative_backup = os.path.join(".claude", "scratch", "hb_driver_selftest", "files_backup")
		os.makedirs(files, exist_ok=True)
		run = subprocess.run(cli + ["launch", "--port", "1", "--pid-file", os.path.join(common.repo_root, "pids.txt"),
		                            "--backup-dir", relative_backup], capture_output=True, text=True, timeout=60)
		check("a --pid-file outside the scratch tree is refused before anything launches",
		      run.returncode == 1 and "--pid-file" in run.stderr, run.stdout + run.stderr)
	finally:
		server.stop()


def main():
	if os.path.isdir(scratch):
		shutil.rmtree(scratch)
	os.makedirs(scratch)
	try:
		test_units()
		test_client_files()
		test_logs()
		test_position_guard()
		test_identity()
		test_mcp_flow_arguments()
		test_flows()
	finally:
		shutil.rmtree(scratch, ignore_errors=True)
	print("")
	if failures:
		print("{} FAILURE(S): {}".format(len(failures), ", ".join(failures)))
		return 1
	print("all checks passed")
	return 0


if __name__ == "__main__":
	sys.exit(main())
