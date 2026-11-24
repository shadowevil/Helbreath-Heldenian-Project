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
	"""Enough of the client for the flows. All state is guarded by one lock."""

	def __init__(self, client_json, login_log, world_log):
		self.lock = threading.RLock()
		self.client_json = client_json
		self.login_log = login_log
		self.world_log = world_log
		self.started = time.monotonic()
		self.frame = 0
		self.seq = 0
		self.events = []
		self.waits = []
		self.screen = "Screen_MainMenu"
		self.pending_screen = None
		self.fade = 0
		self.overlay = None           # {"kind": busy|message, "left": frames, "then": fn, "dismiss": fn}
		self.fields = {}
		self.focus = None
		self.muted = False
		self.sound_selected = False
		self.accounts = {}
		self.characters = {}          # account -> [names]
		self.account = None
		self.selected = 0
		self.delete_panel = False
		self.create = {}
		self.dialog_open = False
		self.dialog_categories = ["system", "graphics", "interface", "gameplay", "sound", "debug"]
		self.dialog_page = 1          # opens on Graphics: switch_character must be found via cat_system
		self.character = None
		self.player = (134, 37)
		self.path = []
		self.move_wait = 0
		self.creature_tile = None
		self.quit = False
		self.overlay_widgets = True   # report overlay.ok / overlay.cancel (HB-0316); False = an older client build
		self.dismissals = []          # how each message was dismissed: "widget" or "point"
		self.swallow_clicks = False   # a world click that sends nothing (blocked tile, dialog, busy)
		self.max_steps = None         # a walk that stops this many steps in (blocked / interrupted)
		self.steps_taken = 0
		self.emit_walk_end = False    # the client's walk events on a plain click (client batch 2); walk_to always emits
		self.walk_to_active = False   # the walk in progress was ordered by walk_to
		self.has_walk_to = True       # the client knows walk_to / tile_to_screen (False = a batch-1 client)
		self.has_tile_to_screen = False
		self.walk_refusal = None      # a walk_to refusal message
		self.counts = {}              # wire commands received, by name
		self.cover_all = False        # with report_fronts: a window covers every point of the view
		self.has_set_audio = True     # the client knows set_audio (client batch 3-4)
		self.audio_volumes = {"master": 100, "music": 80, "effects": 70}
		self.hidden_ids = set()       # widget ids the client reports scrolled out of view
		self.emit_ready_markers = False   # world_ready / first_frame_presented / screen_active (client batch 3-4)
		self.probe_lag = False        # the probe's player_tile reads one tile short
		self.new_probe_fields = False # on_self_sprite + self_hides_pick on EVERY probe (self_hides_pick true on open ground)
		self.report_fronts = False    # the probe / click report front / covered_by / consumed_by (client batch 1)
		self.spawned = False
		self.spawn_after_swap = False
		self.spawn_in = 0
		self.next_character_id = 100
		self.character_ids = {}
		for path in (login_log, world_log):
			open(path, "w").close()
		self.persist()

	# --- plumbing ---

	def now_ms(self):
		return int((time.monotonic() - self.started) * 1000)

	def record(self, channel, event_type, **data):
		self.seq += 1
		self.events.append({"seq": self.seq, "t_ms": self.now_ms(), "frame": self.frame, "channel": channel,
		                    "type": event_type, "data": data})

	def log(self, path, text):
		stamp = time.strftime("%Y-%m-%d %H:%M:%S") + ".{:03d}".format(int(time.time() * 1000) % 1000)
		with open(path, "a") as handle:
			handle.write("[{}] [INFO] {}\n".format(stamp, text))

	def persist(self):
		with open(self.client_json, "w") as handle:
			json.dump({"audio": {"master_muted": self.muted}}, handle)

	def set_screen(self, name):
		self.pending_screen = name
		self.fade = fade_frames

	def ui_width(self):
		return world_ui_width if self.screen == "Screen_OnGame" else 640

	def state(self):
		out = {"screen": self.screen, "transitioning": self.fade > 0}
		if self.screen == "Screen_OnGame":
			out["character"] = self.character
			if self.spawned:
				out["object_id"] = 33
		return out

	# --- widgets ---

	def widgets(self):
		live = self.overlay is None and self.fade == 0
		s = self.screen
		out = []
		if self.overlay is not None and self.overlay_widgets:
			if self.overlay["kind"] == "message":
				out.append(widget("overlay.ok", "button", 270, 300, 100, 20, "OK"))
			else:
				out.append(widget("overlay.cancel", "button", 270, 300, 100, 20, "Cancel", enabled=False))
		if s == "Screen_MainMenu":
			for i, wid in enumerate(("login", "new_account", "settings", "exit")):
				out.append(widget("main_menu." + wid, "button", 270, 200 + 40 * i, 100, 20, wid, enabled=live))
		elif s == "Screen_Login":
			out.append(widget("login.account", "text_field", 231, 225, 177, 18, value=self.fields.get("login.account", ""), enabled=live))
			out.append(widget("login.password", "text_field", 231, 267, 177, 18, value="*" * len(self.fields.get("login.password", "")), enabled=live))
			out.append(widget("login.login", "button", 238, 334, 73, 19, enabled=live))
			out.append(widget("login.return", "button", 327, 334, 73, 19, enabled=live))
		elif s == "Screen_CreateAccount":
			for i, key in enumerate(("account", "password", "confirm", "email")):
				value = self.fields.get("account_create." + key, "")
				shown = "*" * len(value) if key in ("password", "confirm") else value
				out.append(widget("account_create." + key, "text_field", 380, 146 + 48 * i, 160, 18, value=shown, enabled=live))
			for i, key in enumerate(("create", "clear", "cancel")):
				out.append(widget("account_create." + key, "button", 340 + 86 * i, 372, 70, 20, enabled=live))
		elif s == "Screen_Settings":
			out.append(widget("settings.back", "button", 20, 440, 60, 20))
			out.append(widget("settings.cat_general", "list_item", 20, 60, 100, 20, value="off" if self.sound_selected else "on"))
			out.append(widget("settings.cat_sound", "list_item", 20, 84, 100, 20, value="on" if self.sound_selected else "off"))
			if self.sound_selected:
				out.append(widget("settings.sound_master", "slider", 300, 60, 200, 20, value="25"))
				out.append(widget("settings.sound_master_enabled", "checkbox", 510, 60, 20, 20, value="off" if self.muted else "on"))
		elif s == "Screen_SelectCharacter":
			names = self.characters.get(self.account, [])
			page = live and not self.delete_panel
			if self.delete_panel:
				out.append(widget("char_select.delete_yes", "button", 250, 300, 60, 20, enabled=live))
				out.append(widget("char_select.delete_no", "button", 330, 300, 60, 20, enabled=live))
			for slot in range(4):
				name = names[slot] if slot < len(names) else ""
				out.append(widget("char_select.slot{}".format(slot), "slot", 100 + 183 * (slot % 3), 100 + 80 * (slot // 3), 80, 80,
				                  name, "on" if slot == self.selected else "off", page and bool(name)))
			for i, key in enumerate(("start", "new", "delete", "change_password", "log_out")):
				out.append(widget("char_select." + key, "button", 40 + 124 * i, 437, 64, 20, enabled=page))
		elif s == "Screen_CreateNewCharacter":
			out.append(widget("char_create.name", "text_field", 200, 110, 90, 18, value=self.fields.get("char_create.name", ""), enabled=live))
			out.append(widget("char_create.gender_next", "button", 300, 150, 10, 12, value=str(self.create.get("gender", 0)), enabled=live))
			for i, label in enumerate(presets):
				out.append(widget("char_create.preset{}".format(i), "button", 40 + 80 * i, 420, 70, 20, label, enabled=live))
			ready = bool(self.fields.get("char_create.name")) and self.create.get("preset") is not None
			out.append(widget("char_create.create", "button", 400, 440, 60, 20, enabled=live and ready))
			out.append(widget("char_create.cancel", "button", 490, 440, 60, 20, enabled=live))
		elif s == "Screen_OnGame":
			out.append(widget("hud.settings", "button", 800, 450, 30, 25))
			if self.dialog_open:
				out.append(widget("settings.title", "grip", 300, 80, 250, 20))
				for page, name in enumerate(self.dialog_categories):
					out.append(widget("settings.cat_{}".format(name), "list_item", 300, 100 + 22 * page, 60, 20, value="on" if page == self.dialog_page else "off"))
				if self.dialog_page == 0:
					out.append(widget("settings.switch_character", "button", 320, 380, 100, 20))
					out.append(widget("settings.logout", "button", 440, 380, 80, 20))
		return {"ui_width": self.ui_width(), "ui_height": 480, "widgets": out}

	def activate(self, wid):
		"""A widget click (by id or resolved from a point), in the screen's own terms."""
		s = self.screen
		if not wid.endswith(("account", "password", "confirm", "email", ".name")):
			self.record("ui.button", "button_activated", via="ui_click")
		if wid.startswith(("login.account", "login.password", "account_create.account", "account_create.password",
		                   "account_create.confirm", "account_create.email", "char_create.name")):
			self.focus = wid
		elif wid == "overlay.ok":
			self.dismissals.append("widget")
			self.dismiss_message()
		elif wid == "main_menu.login":
			self.set_screen("Screen_Login")
		elif wid == "main_menu.new_account":
			self.fields = {}
			self.set_screen("Screen_CreateAccount")
		elif wid == "main_menu.settings":
			self.sound_selected = False
			self.set_screen("Screen_Settings")
		elif wid == "main_menu.exit":
			self.set_screen("Screen_Quit")
		elif wid == "login.return" or wid == "account_create.cancel" or wid == "settings.back":
			self.set_screen("Screen_MainMenu")
		elif wid == "settings.cat_sound":
			self.sound_selected = True
		elif wid == "settings.sound_master_enabled":
			self.muted = not self.muted
			self.persist()
		elif wid == "login.login":
			account = self.fields.get("login.account", "")
			password = self.fields.get("login.password", "")

			def done():
				if self.accounts.get(account) == password:
					self.log(self.login_log, "login ok: '{}' (0 characters)".format(account))
					self.account = account
					self.selected = 0
					self.overlay = None
					self.set_screen("Screen_SelectCharacter")
				else:
					self.log(self.login_log, "login rejected (bad credentials) for '{}'".format(account))
					self.overlay = {"kind": "message", "dismiss": None}
			self.overlay = {"kind": "busy", "left": busy_frames, "then": done}
		elif wid == "account_create.create":
			account = self.fields.get("account_create.account", "")

			def done():
				if account in self.accounts:
					self.log(self.login_log, "account create rejected (name taken): '{}'".format(account))
					self.overlay = {"kind": "message", "dismiss": None}
				else:
					self.accounts[account] = self.fields.get("account_create.password", "")
					self.log(self.login_log, "account created: '{}' (id {})".format(account, len(self.accounts)))
					self.overlay = {"kind": "message", "dismiss": lambda: self.set_screen("Screen_MainMenu")}
			self.overlay = {"kind": "busy", "left": busy_frames, "then": done}
		elif wid == "char_select.log_out":
			self.set_screen("Screen_MainMenu")
		elif wid.startswith("char_select.slot"):
			self.selected = int(wid[len("char_select.slot"):])
		elif wid == "char_select.new":
			self.fields = {}
			self.create = {}
			self.set_screen("Screen_CreateNewCharacter")
		elif wid == "char_select.delete":
			self.delete_panel = True
		elif wid == "char_select.delete_no":
			self.delete_panel = False
		elif wid == "char_select.delete_yes":
			name = self.characters[self.account][self.selected]

			def done():
				self.characters[self.account].remove(name)
				self.log(self.login_log, "character deleted: '{}' (id 1, account 1) - 0 ground item(s) freed".format(name))
				self.delete_panel = False
				self.selected = 0
				self.overlay = {"kind": "message", "dismiss": None}
			self.overlay = {"kind": "busy", "left": busy_frames, "then": done}
		elif wid == "char_select.start":
			name = self.characters[self.account][self.selected]

			def done():
				self.overlay = None
				self.character = name
				self.spawned = False
				self.set_screen("Screen_OnGame")
				self.record("ui.window", "window_opened", window="entering")
				if not self.spawn_after_swap:
					self.spawn()  # HB-0156: the world reply is drained during the fade
			self.overlay = {"kind": "busy", "left": busy_frames, "then": done}
		elif wid == "char_create.gender_next":
			self.create["gender"] = (self.create.get("gender", 0) + 1) % 2
		elif wid.startswith("char_create.preset"):
			self.create["preset"] = int(wid[len("char_create.preset"):])
		elif wid == "char_create.cancel":
			self.set_screen("Screen_SelectCharacter")
		elif wid == "char_create.create":
			name = self.fields.get("char_create.name", "")

			def done():
				taken = any(name in names for names in self.characters.values())
				if taken:
					self.log(self.login_log, "character create rejected (invalid input): '{}'".format(name))
					self.overlay = {"kind": "message", "dismiss": None}
				else:
					self.characters.setdefault(self.account, []).append(name)
					self.character_ids[name] = self.next_character_id
					self.next_character_id += 1
					self.log(self.login_log, "character created: '{}' (id {}, account 1)".format(name, self.character_ids[name]))
					self.overlay = {"kind": "message", "dismiss": lambda: self.set_screen("Screen_SelectCharacter")}
			self.overlay = {"kind": "busy", "left": busy_frames, "then": done}
		elif wid == "hud.settings":
			self.dialog_open = not self.dialog_open
		elif self.dialog_open and wid.startswith("settings.cat_"):
			self.dialog_page = self.dialog_categories.index(wid[len("settings.cat_"):])
		elif wid in ("settings.switch_character", "settings.logout"):
			cid = self.character_ids.get(self.character, 1)
			self.log(self.world_log, "save queued: character {} -> 'travelerzone' ({},{}) dir 4 - 3 statement(s)".format(cid, *self.player))
			self.log(self.world_log, "client session closed (0 session(s))")
			self.log(self.world_log, "persistence: batch 3 statement(s) in 1 unit(s)")
			self.dialog_open = False
			self.set_screen("Screen_SelectCharacter" if wid == "settings.switch_character" else "Screen_MainMenu")

	def start_walk(self, goal, source):
		self.path = [goal]
		self.walk_to_active = source == "walk_to"
		if self.emit_walk_end or self.walk_to_active:
			self.record("state.player", "walk_started", from_x=self.player[0], from_y=self.player[1], goal_x=goal[0],
			            goal_y=goal[1], path_len=max(abs(goal[0] - self.player[0]), abs(goal[1] - self.player[1])),
			            reachable=True, source=source)

	def end_walk(self, arrived):
		if self.emit_walk_end or self.walk_to_active:
			goal = self.path[0]
			if arrived:
				self.record("state.player", "walk_arrived", tile_x=self.player[0], tile_y=self.player[1],
				            goal_x=goal[0], goal_y=goal[1], arrived=True)
			else:
				self.record("state.player", "walk_stopped", tile_x=self.player[0], tile_y=self.player[1],
				            goal_x=goal[0], goal_y=goal[1], arrived=False, reason="blocked")
		self.path = []
		self.steps_taken = 0
		self.walk_to_active = False

	def dismiss_message(self):
		dismiss = self.overlay.get("dismiss")
		self.overlay = None
		if dismiss:
			dismiss()

	def point_click(self, x, y):
		if self.overlay is not None:
			if self.overlay["kind"] == "message":
				self.dismissals.append("point")
				self.dismiss_message()
			return
		if self.swallow_clicks and self.screen == "Screen_OnGame":
			return
		if self.screen == "Screen_OnGame":
			for item in self.widgets()["widgets"]:
				if item["x"] <= x <= item["x"] + item["w"] and item["y"] <= y <= item["y"] + item["h"]:
					self.activate(item["id"])
					return
			detail = self.probe_detail(x, y)
			if detail.get("on_hud") or detail.get("on_self_sprite"):
				return
			if detail.get("on_self_box"):
				return  # a click in the click-on-self box is a pickup, never a move
			self.start_walk(tuple(detail["walk_goal_tile"]), "order")
			return
		for item in self.widgets()["widgets"]:
			if item["x"] <= x <= item["x"] + item["w"] and item["y"] <= y <= item["y"] + item["h"] and item["enabled"]:
				self.activate(item["id"])
				return

	def probe_detail(self, x, y):
		if self.screen != "Screen_OnGame":
			return {}
		shown = (self.player[0] - 1, self.player[1] - 1) if self.probe_lag else self.player
		detail = {"player_tile": list(shown), "combat_stance": False}
		if y > 440:
			detail["on_hud"] = True
			return detail
		pitch_x = 32 * world_ui_width / 640.0
		tile = (self.player[0] + math.floor((x - world_ui_width / 2 + pitch_x / 2) / pitch_x),
		        self.player[1] + math.floor((y - 240 + 16) / 32.0))
		detail.update({"on_hud": False, "world_tile": list(tile), "walk_goal_tile": list(tile)})
		if self.report_fronts:
			# a window covers the right-hand strip of the view
			covered = self.cover_all or x > world_ui_width - 90
			detail["front"] = {"kind": "window" if covered else "world", "owner": "inventory" if covered else ""}
			if covered:
				detail["covered_by"] = "inventory"
		# the player's sprite covers its own tile and the one north of it
		if self.new_probe_fields:
			# the current client: both keys on every probe, self_hides_pick true on any uncontested ground
			detail.update({"on_self_sprite": False, "self_hides_pick": True})
		if self.new_probe_fields and tile in (self.player, (self.player[0], self.player[1] - 1)):
			detail["on_self_box"] = True   # the generous click-on-self box covers its own tile and the one north
		if tile in (self.player, (self.player[0], self.player[1] - 1)) and (y - 240) % 32 < 20:
			detail.update({"identifiable": True, "is_player": True, "on_self_sprite": True, "self_hides_pick": True})
		if self.creature_tile and tile == self.creature_tile:
			detail.update({"identifiable": True, "creature_id": 7, "attackable": True})
		return detail

	# --- frame ---

	def tick(self):
		with self.lock:
			self.frame += 1
			if self.overlay and self.overlay["kind"] == "busy":
				self.overlay["left"] -= 1
				if self.overlay["left"] <= 0:
					self.overlay["then"]()
			if self.fade > 0:
				self.fade -= 1
				if self.fade == fade_frames // 2 + 1 and self.pending_screen:
					previous = self.screen
					self.screen = self.pending_screen
					self.pending_screen = None
					self.on_enter_screen(previous)
					self.record("ui.screen", "screen_changed", **{"from": previous, "to": self.screen})
			if self.spawn_in > 0:
				self.spawn_in -= 1
				if self.spawn_in == 0:
					self.spawn()
			if self.screen == "Screen_Quit" and self.fade == 0:
				self.quit = True
			if self.path:
				self.move_wait += 1
				if self.move_wait == 1:
					target = self.path[0]
					step = (self.player[0] + (target[0] > self.player[0]) - (target[0] < self.player[0]),
					        self.player[1] + (target[1] > self.player[1]) - (target[1] < self.player[1]))
					self.record("state.player", "player_moved", from_x=self.player[0], from_y=self.player[1],
					            to_x=step[0], to_y=step[1], direction=3, running=False, predicted=True)
					self.record("net.packet", "packet_out", message="move_request", bytes=12)
					self.player = step
				elif self.move_wait >= move_answer_frames:
					self.record("net.packet", "packet_in", message="move_confirm", bytes=8)
					self.move_wait = 0
					self.steps_taken += 1
					target = self.path[0]
					if self.player == target or (self.max_steps is not None and self.steps_taken >= self.max_steps):
						self.end_walk(self.player == target)
			return self.settle()

	def on_enter_screen(self, previous):
		if self.screen == "Screen_OnGame" and self.spawn_after_swap:
			self.spawn_in = fade_frames

	def spawn(self):
		self.record("net.packet", "packet_in", message="world_enter_response", bytes=90)
		self.record("state.map", "map_changed", **{"from": "", "to": "travelerzone", "x": self.player[0], "y": self.player[1], "reason": "entry"})
		self.record("ui.window", "window_closed", window="entering", via="dismissed")
		self.spawned = True
		if self.emit_ready_markers:
			if self.screen == "Screen_OnGame":
				self.record("ui.screen", "first_frame_presented", screen="Screen_OnGame")
				self.record("ui.screen", "screen_active", screen="Screen_OnGame")
			self.record("ui.screen", "world_ready", map="travelerzone", tile_x=self.player[0], tile_y=self.player[1])

	def settle(self):
		replies = []
		for wait in list(self.waits):
			matched = None
			for index, condition in enumerate(wait["conditions"]):
				if "state" in condition:
					if self.state().get(condition["state"]) == condition["equals"]:
						matched = {"index": index, "condition": condition, "observed": condition["equals"]}
						break
					continue
				for event in self.events:
					if event["seq"] < wait["scan"]:
						continue
					if not (event["channel"] == condition["channel"] or event["channel"].startswith(condition["channel"] + ".")):
						continue
					if condition.get("type") and condition["type"] != event["type"]:
						continue
					if all(event["data"].get(k) == v for k, v in (condition.get("match") or {}).items()):
						matched = {"index": index, "condition": condition, "event": event}
						break
				if matched:
					break
			if matched or time.monotonic() >= wait["deadline"]:
				result = {"satisfied": matched is not None, "state": self.state(), "cursor": self.seq + 1, "dropped": 0,
				          "events": [e for e in self.events if e["seq"] >= wait["scan"]]}
				if matched:
					result["matched"] = matched
				replies.append((wait["conn"], {"id": wait["id"], "ok": True, "result": result}))
				self.waits.remove(wait)
		return replies

	# --- commands ---

	def command(self, conn, request):
		cmd = request.get("cmd")
		args = request.get("args") or {}
		rid = request.get("id")
		with self.lock:
			if cmd == "wait_for":
				self.waits.append({"id": rid, "conn": conn, "conditions": args["conditions"], "scan": self.seq + 1,
				                   "deadline": time.monotonic() + args["timeout_ms"] / 1000.0})
				return None
			try:
				result = self.run(cmd, args)
			except KeyError as error:
				return {"id": rid, "ok": False, "error": str(error)}
			if isinstance(result, str):
				return {"id": rid, "ok": False, "error": result}
			return {"id": rid, "ok": True, "result": result}

	def run(self, cmd, args):
		self.counts[cmd] = self.counts.get(cmd, 0) + 1
		if cmd == "tile_to_screen" and self.has_tile_to_screen:
			pitch_x = 32 * world_ui_width / 640.0
			tx, ty = args["x"], args["y"]
			lx = world_ui_width / 2 + (tx - self.player[0]) * pitch_x
			ly = 240 + (ty - self.player[1]) * 32
			on_screen = 0 <= lx <= world_ui_width and 0 <= ly <= 440
			reply = {"tile": [tx, ty], "in_bounds": True, "on_screen": on_screen,
			         "logical": [int(round(lx)), int(round(ly))],
			         "window": [int(round(lx * out_w / world_ui_width)), int(round(ly * out_h / 480.0))],
			         "player_tile": list(self.player)}
			if on_screen:
				detail = self.probe_detail(lx, ly)
				reply.update({"resolves_to": detail.get("world_tile"), "round_trip": detail.get("world_tile") == [tx, ty],
				              "front": {"kind": "world", "owner": "screen:in_game"}, "on_self": bool(detail.get("on_self_sprite"))})
			return reply
		if cmd == "set_audio" and self.has_set_audio:
			if "master_muted" in args:
				self.muted = bool(args["master_muted"])
			for key in self.audio_volumes:
				if key in args:
					self.audio_volumes[key] = int(args[key])
			if args.get("persist"):
				self.persist()
			return dict(self.audio_volumes, master_muted=self.muted, persisted=bool(args.get("persist")))
		if cmd == "walk_to" and self.has_walk_to:
			if self.walk_refusal:
				return "walk_to refused: " + self.walk_refusal
			goal = (args["x"], args["y"])
			if goal == self.player:
				self.path = [goal]
				self.walk_to_active = True
				self.record("state.player", "walk_started", from_x=goal[0], from_y=goal[1], goal_x=goal[0], goal_y=goal[1],
				            path_len=0, reachable=True, source="walk_to")
				self.end_walk(True)
				return {"goal": list(goal), "from": list(self.player), "map": "travelerzone", "status": "arrived",
				        "path_len": 0, "reachable": True}
			origin = list(self.player)
			self.start_walk(goal, "walk_to")
			return {"goal": list(goal), "from": origin, "map": "travelerzone", "status": "walking",
			        "path_len": max(abs(goal[0] - origin[0]), abs(goal[1] - origin[1])), "reachable": True}
		if cmd == "ping":
			return {"pong": True, "build": "0.0.0-selftest"}
		if cmd == "get_state":
			return self.state()
		if cmd == "widgets":
			return self.widgets()
		if cmd == "widget_click":
			found = next((w for w in self.widgets()["widgets"] if w["id"] == args["id"]), None)
			if found is None:
				return "unknown widget id: " + args["id"]
			if args["id"] in self.hidden_ids:
				return "widget is scrolled out of view: " + args["id"]
			if not found["enabled"]:
				return "widget is disabled: " + args["id"]
			self.activate(args["id"])
			return {"id": args["id"], "x": found["x"] + found["w"] // 2, "y": found["y"] + found["h"] // 2}
		if cmd == "click":
			x, y = args["x"], args["y"]
			if args.get("space") == "window":
				x, y = x * self.ui_width() / out_w, y * 480.0 / out_h
			self.point_click(x, y)
			if self.report_fronts and self.swallow_clicks:
				return {"consumed_by": {"kind": "window", "owner": "inventory"}}
			return {}
		if cmd == "set_text":
			if self.focus is None:
				return {}
			self.fields[self.focus] = args["value"][:10]
			return {}
		if cmd == "probe_point":
			return {"logical": [args["x"], args["y"]], "ui_width": self.ui_width(), "output_size": [out_w, out_h],
			        "window_size": [out_w, out_h], "detail": self.probe_detail(args["x"], args["y"])}
		if cmd == "journal_query":
			events = [e for e in self.events if e["seq"] > args.get("after_seq", 0)
			          and ("before_seq" not in args or e["seq"] < args["before_seq"])]
			if args.get("channels"):
				events = [e for e in events if any(e["channel"] == c or e["channel"].startswith(c + ".") for c in args["channels"])]
			events = list(reversed(events))
			limit = args.get("max", 200)
			page = events[:limit]
			result = {"live": not any(k in args for k in ("after_seq", "before_seq")), "now_ms": self.now_ms(),
			          "newest_seq": self.seq, "events": page, "more_older": len(events) > limit, "raw": len(page)}
			if result["more_older"]:
				result["older_before_seq"] = page[-1]["seq"]
			return result
		return "unknown cmd: " + str(cmd)


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


def test_mcp_flow_arguments():
	print("hb_flow argument checks (HB-0236 strictness)")
	check("a hyphenated name is canonicalized", mcp_flows.check_arguments({"flow": "click-tile", "x": 1, "y": 2}) == "click_tile")
	check("so is a mixed-case one", mcp_flows.check_arguments({"flow": "Wait-Screen", "screen": "x"}) == "wait_screen")
	expect_error("walk_to is a tool, not a flow, and the error names it",
	             lambda: mcp_flows.check_arguments({"flow": "walk-to", "x": 1, "y": 2}), "hb_walk_to")
	check("step takes walk_fallback", mcp_flows.check_arguments({"flow": "step", "direction": "n", "walk_fallback": True}) == "step")
	check("an absent walk_fallback is automatic, a given one is forced either way",
	      mcp_flows._fallback({}) is None and mcp_flows._fallback({"walk_fallback": True}) is True
	      and mcp_flows._fallback({"walk_fallback": False}) is False)
	check("client and timeout_s are accepted by every flow",
	      mcp_flows.check_arguments({"flow": "main_menu", "client": "c1", "timeout_s": 5}) == "main_menu")
	expect_error("an args wrapper is refused with the fix", lambda: mcp_flows.check_arguments(
		{"flow": "click_tile", "args": {"x": 1, "y": 2}}), "top-level")
	expect_error("a key another flow owns is refused", lambda: mcp_flows.check_arguments(
		{"flow": "step", "direction": "n", "x": 1}), "does not take: x")
	expect_error("the error names what the flow does take", lambda: mcp_flows.check_arguments(
		{"flow": "step", "direction": "n", "x": 1}), "direction")
	expect_error("a missing required key is named", lambda: mcp_flows.check_arguments({"flow": "click_tile", "x": 1}), "needs y")
	expect_error("an unknown flow lists the flows", lambda: mcp_flows.check_arguments({"flow": "nope"}), "main_menu")
	check("logout and switch take a proof_file (name and save_timeout too)",
	      mcp_flows.check_arguments({"flow": "logout", "proof_file": "x"}) == "logout"
	      and mcp_flows.check_arguments({"flow": "switch", "proof_file": "x", "name": "N", "save_timeout": 3}) == "switch")
	expect_error("other flows still refuse a proof_file", lambda: mcp_flows.check_arguments(
		{"flow": "enter", "proof_file": "x"}), "does not take")


def test_flows():
	print("flows against the fake client")
	files = os.path.join(scratch, "fake_bin")
	os.makedirs(files, exist_ok=True)
	game = fake_client(os.path.join(files, "client.json"), os.path.join(files, "login.log"), os.path.join(files, "world.log"))
	server = fake_server(game)
	saved_paths = (common.login_log_path, common.world_log_path, common.client_log_path)
	common.login_log_path, common.world_log_path = game.login_log, game.world_log
	common.client_log_path = os.path.join(files, "client.log")
	open(common.client_log_path, "w").close()
	try:
		sess = bridge.session.connect(server.port, timeout_s=5)
		check("connect answers ping", sess.ping().get("pong") is True)

		# wait_for_then registers the wait BEFORE acting, so an instant event is not missed
		game.screen = "Screen_SelectCharacter"
		outcome, _ = sess.wait_for_then([bridge.event_condition("ui.button", "button_activated")], 2000,
		                                "widget_click", {"id": "char_select.log_out"})
		check("wait_for_then catches the event its own action caused", outcome.satisfied, outcome.raw)
		sess.wait_screen("Screen_MainMenu", 5)
		outcome = sess.wait_for([bridge.state_condition("screen", "nowhere")], 200)
		check("a wait_for timeout is a result, not an error", outcome.satisfied is False)
		expect_error("wait_screen names where it is stuck", lambda: sess.wait_screen("nowhere", 0.3), "Screen_MainMenu")

		ident = identity.new_identity(os.path.join(scratch, "ident_flow.json"), characters=1)
		mute = flows.set_master_mute(sess, True, client_json=game.client_json)
		check("mute clicks the master tick by id and client.json follows", mute["clicked"] and game.muted
		      and common.read_json_file(game.client_json)["audio"]["master_muted"] is True, mute)
		again = flows.set_master_mute(sess, True, client_json=game.client_json)
		check("mute SETS the state: already muted means no click", again["clicked"] is False and game.muted, again)

		created = flows.create_account(sess, ident)
		check("create_account ends on the main menu with the account made", sess.screen() == "Screen_MainMenu"
		      and ident.account in game.accounts and identity.load_identity(ident.path).data["account_created"], created)
		check("the login log line is returned", created["log"] and "account created" in created["log"], created)
		expect_error("a second create is refused with the login server's reason",
		             lambda: flows.create_account(sess, ident), "name taken")
		check("after a refusal the flow cancelled back to the main menu", sess.screen() == "Screen_MainMenu")

		check("the account-created and name-taken messages were dismissed with overlay.ok by name",
		      game.dismissals and set(game.dismissals) == {"widget"}, game.dismissals)
		game.overlay_widgets = False
		game.dismissals.clear()
		expect_error("an older client (no overlay widgets) still has its refusal dismissed",
		             lambda: flows.create_account(sess, ident), "name taken")
		check("... by the widget-free point click", game.dismissals and set(game.dismissals) == {"point"}, game.dismissals)
		game.overlay_widgets = True

		bad = identity.identity(os.path.join(scratch, "ident_bad.json"),
		                        {"account": ident.account, "password": "wrongpass1"})
		expect_error("a bad password is reported from the login log", lambda: flows.log_in(sess, bad), "bad credentials")
		logged = flows.log_in(sess, ident)
		check("log_in from the LOGIN screen reaches character select", sess.screen() == "Screen_SelectCharacter", logged)

		name = ident.characters[0]["name"]
		made = flows.create_character(sess, ident, preset="warrior", appearance={"gender": 1})
		check("create_character picks the preset by label and ends on character select",
		      made["preset"] == "Warrior" and name in game.characters[ident.account], made)
		check("the identity marks it created", identity.load_identity(ident.path).created_character(name))
		second = ident.add_character()["name"]
		flows.create_character(sess, ident, second, preset=0)
		picked = flows.select_character(sess, second)
		check("select_character finds the card by name and confirms it", picked["slot"] == 1 and game.selected == 1, picked)
		expect_error("select_character refuses a name not on the account",
		             lambda: flows.select_character(sess, "Nobody"), "no character")

		entered = flows.enter_world(sess, name)
		check("enter_world selects by name, waits for the spawn, confirms the character",
		      entered["character"] == name and sess.screen() == "Screen_OnGame", entered)
		game.muted = False
		before_logins = game.counts.get("widget_click", 0)
		live_mute = flows.set_master_mute(sess, True, client_json=game.client_json)
		check("mute on a logged-in client uses set_audio: no logout, no widget clicks, proven by read-back and client.json",
		      live_mute["via"] == "set_audio" and game.muted and game.screen == "Screen_OnGame"
		      and game.counts.get("widget_click", 0) == before_logins and live_mute["audio"]["persisted"] is True
		      and live_mute["previous_muted"] is False, live_mute)
		live_unmute = flows.set_master_mute(sess, False)
		check("and unmute likewise, without persisting when no client.json is named",
		      not game.muted and live_unmute["via"] == "set_audio" and live_unmute["audio"]["persisted"] is False, live_unmute)
		game.has_set_audio = False
		expect_error("a logged-in client without set_audio is told to use leave_session",
		             lambda: flows.set_master_mute(sess, True), "leave_session")
		game.has_set_audio = True
		game.hidden_ids.add("hud.settings")
		expect_error("a widget the client reports scrolled out of view is refused with a scroll hint",
		             lambda: sess.click_widget("hud.settings"), "scroll the list")
		game.hidden_ids.clear()

		view = world.calibrate(sess)
		check("calibration measures the non-square pitch", abs(view.pitch_x - 42.66) < 3 and abs(view.pitch_y - 32) < 3,
		      view.as_dict())
		moved = world.step(sess, "e")
		check("step e: move_confirm and arrival", moved["outcome"] == "move_confirm" and moved["arrived"]
		      and game.player == (135, 37), moved)
		north = world.step(sess, "n")
		check("step n avoids the self-sprite pixel and still arrives", north["outcome"] == "move_confirm"
		      and game.player == (135, 36), north)
		check("step reports how the walk ended", moved["ended"] == "arrived" and moved["moves"] >= 1
		      and moved["last_move_to"] == (135, 37) and moved["tiles_short"] == 0, moved)
		game.creature_tile = (137, 36)
		far = world.click_tile(sess, (138, 36))
		check("click_tile walks several tiles", far["arrived"] and game.player == (138, 36), far)
		check("click_tile separates the FIRST answer from the arrival", far["outcome"] == "move_confirm"
		      and far["answer_event"]["data"]["message"] == "move_confirm" and far["moves"] >= 3
		      and far["last_move_to"] == (138, 36), far)

		game.probe_lag = True
		lagged = world.click_tile(sess, (137, 37))
		game.probe_lag = False
		check("a probe that reads one tile short does not turn an arrival into a failure",
		      lagged["arrived"] is True and lagged["player_tile"] != (137, 37) and lagged["last_move_to"] == (137, 37),
		      lagged)

		saved_settle = world.arrive_settle_s
		world.arrive_settle_s = 0.4
		game.max_steps = 2
		short = world.click_tile(sess, (141, 37))
		game.max_steps = None
		world.arrive_settle_s = saved_settle
		check("a walk that stops short says so, with the distance",
		      short["arrived"] is False and short["ended"] == "stopped" and short["tiles_short"] >= 1
		      and short["last_move_to"] == (139, 37) and game.player == (139, 37), short)

		game.emit_walk_end = True
		ended = world.click_tile(sess, (138, 37))
		game.emit_walk_end = False
		check("a client walk-end event, when present, is reported (the batch-2 hook)",
		      ended["arrived"] is True and (ended["walk_end"] is None or ended["walk_end"]["type"] == "walk_arrived"), ended)
		game.emit_walk_end = True
		game.max_steps = 1
		ended = world.click_tile(sess, (141, 37))
		game.emit_walk_end = False
		game.max_steps = None
		check("a walk_stopped event ends the wait as not arrived, with its reason",
		      ended["arrived"] is False and ended["ended"] == "stopped" and ended["reason"] == "blocked"
		      and ended["judged_by"] == "event" and ended["walk_end"]["data"]["arrived"] is False, ended)
		world.click_tile(sess, (138, 37))
		game.emit_walk_end = True
		arrived_by_event = world.click_tile(sess, (137, 37))
		game.emit_walk_end = False
		check("an arrival is judged by the walk_arrived event first",
		      arrived_by_event["arrived"] is True and arrived_by_event["judged_by"] == "event"
		      and arrived_by_event["walk_end"]["type"] == "walk_arrived", arrived_by_event)
		check("without walk events the journal decides", far["judged_by"] == "journal", far)
		stale = [{"channel": "state.player", "type": "walk_stopped", "seq": 1,
		          "data": {"tile_x": 1, "tile_y": 1, "goal_x": 9, "goal_y": 9, "arrived": False, "reason": "new_order"}},
		         {"channel": "state.player", "type": "walk_arrived", "seq": 2,
		          "data": {"tile_x": 5, "tile_y": 5, "goal_x": 5, "goal_y": 5, "arrived": True}}]
		summary = world.walk_summary(stale, (5, 5))
		check("another walk's end (the previous order's new_order stop) is not this walk's",
		      summary["walk_end"]["type"] == "walk_arrived" and summary["walk_end_arrived"] is True, summary)
		check("a stop for another goal alone leaves the walk unended",
		      world.walk_summary(stale[:1], (5, 5))["walk_end"] is None)

		goal = (7, 7)
		started = {"channel": "state.player", "type": "walk_started", "seq": 1,
		           "data": {"from_x": 6, "from_y": 7, "goal_x": 7, "goal_y": 7, "path_len": 1, "reachable": True}}
		moved = {"channel": "state.player", "type": "player_moved", "seq": 2,
		         "data": {"from_x": 6, "from_y": 7, "to_x": 7, "to_y": 7}}
		arrived_event = {"channel": "state.player", "type": "walk_arrived", "seq": 3,
		                 "data": {"tile_x": 7, "tile_y": 7, "goal_x": 7, "goal_y": 7, "arrived": True}}
		check("a walk_started with no end yet is open", world.walk_summary([started, moved], goal)["open"] is True)
		check("its end event closes it", world.walk_summary([started, moved, arrived_event], goal)["open"] is False)
		check("a walk_started for another goal does not open this one",
		      world.walk_summary([dict(started, data=dict(started["data"], goal_x=9))], goal)["open"] is False)

		class scripted_session:
			"""drain_journal answers each poll with the next scripted event list (the last one repeats)."""
			def __init__(self, polls):
				self.polls = polls
				self.reads = 0

			def drain_journal(self, **_):
				batch = self.polls[min(self.reads, len(self.polls) - 1)]
				self.reads += 1
				return batch

		real_player_tile = world.player_tile
		world.player_tile = lambda _sess: goal
		saved_poll = world.arrive_poll_s
		world.arrive_poll_s = 0.01
		try:
			early = scripted_session([[started, moved], [started, moved], [started, moved, arrived_event]])
			held = world.wait_walk(early, goal, 0)
			check("player_moved showing the target does not end a walk whose walk_started has no end yet: the walk "
			      "event decides", held["judged_by"] == "event" and held["ended"] == "arrived"
			      and held["walk_end"]["type"] == "walk_arrived" and early.reads >= 3, (held, early.reads))
			saved_grace = world.arrive_event_grace_s
			world.arrive_event_grace_s = 0.15
			never = world.wait_walk(scripted_session([[started, moved]]), goal, 0)
			world.arrive_event_grace_s = saved_grace
			check("a walk_started whose end never comes falls back to the journal after the grace",
			      never["judged_by"] == "journal" and never["arrived"] is True and never["walk_end"] is None, never)
			old_client = world.wait_walk(scripted_session([[moved]]), goal, 0)
			check("a client with no walk events is still judged by the journal at once",
			      old_client["judged_by"] == "journal" and old_client["arrived"] is True, old_client)
		finally:
			world.player_tile = real_player_tile
			world.arrive_poll_s = saved_poll

		print("walk_to, tile_to_screen and step n (client batch 2)")
		game.has_tile_to_screen = True
		sess.view_calibration = None
		before = dict(game.counts)
		aimed = world.click_tile(sess, (game.player[0] + 1, game.player[1]))
		spent = game.counts.get("probe_point", 0) - before.get("probe_point", 0)
		check("aiming from tile_to_screen replaces the ~40 calibration probes", aimed["arrived"] is True
		      and game.counts.get("tile_to_screen", 0) - before.get("tile_to_screen", 0) == 3 and spent < 15
		      and sess.view_calibration is None, (spent, game.counts))
		there = (game.player[0] - 3, game.player[1] + 2)
		walked = world.walk_to(sess, there)
		check("walk_to arrives by the client's own walk events", walked["via"] == "walk_to" and walked["arrived"] is True
		      and walked["judged_by"] == "event" and walked["status"] == "walking" and game.player == there, walked)
		here_again = world.walk_to(sess, game.player)
		check("walk_to the tile you stand on is arrived at once", here_again["status"] == "arrived"
		      and here_again["arrived"] is True and here_again["ended"] == "arrived", here_again)
		game.walk_refusal = "an open shop or conversation pins the player"
		expect_error("walk_to refused carries the client's reason", lambda: world.walk_to(sess, (140, 40)), "walk_to refused")
		game.walk_refusal = None

		game.new_probe_fields = True
		north = world.step(sess, "n")
		check("step n (the click-on-self box covers the tile north) walks via walk_to and says why",
		      north["via"] == "walk_to" and north["arrived"] is True and "click_blocked" in north
		      and game.player == (there[0], there[1] - 1), north)
		check("the automatic fallback names its reason", north.get("click_blocked_reason") == "self_box", north)
		expect_error("step n with walk_fallback off raises the no-clickable-point error",
		             lambda: world.step(sess, "n", walk_fallback=False), "no clickable point")
		game.report_fronts = True
		game.cover_all = True
		expect_error("a covered step fails with its reason instead of silently walking",
		             lambda: world.step(sess, "w"), "is covered")
		forced = world.step(sess, "w", walk_fallback=True)
		check("walk_fallback:true forces the walk for a covered tile and says why",
		      forced["via"] == "walk_to" and forced["arrived"] is True and forced["click_blocked_reason"] == "covered", forced)
		game.cover_all = False
		game.report_fronts = False
		game.has_walk_to = False
		expect_error("on a client without walk_to the click failure is the answer", lambda: world.step(sess, "n"),
		             "no clickable point")
		game.has_walk_to = True
		diagonal = world.step(sess, "ne")
		check("a diagonal step is still a real click", diagonal.get("via") != "walk_to" and diagonal["arrived"] is True, diagonal)
		game.new_probe_fields = False
		game.has_tile_to_screen = False

		game.report_fronts = True
		game.swallow_clicks = True
		silent_front = world.click_tile(sess, (137, 37), timeout_ms=300)
		game.swallow_clicks = False
		check("a click the client says a window consumed carries that in consumed_by and the hint",
		      silent_front["consumed_by"] == {"kind": "window", "owner": "inventory"}
		      and "consumed by window inventory" in silent_front["hint"], silent_front)
		expect_error("a tile the probe says is covered is refused by name, not clicked",
		             lambda: world.aim_tile(sess, (game.player[0] + 9, game.player[1])), "is covered")
		clear = world.click_tile(sess, (game.player[0] - 1, game.player[1]))
		check("with fronts reported, an uncovered tile is still walked", clear["arrived"] is True, clear)
		game.report_fronts = False

		game.new_probe_fields = True
		fresh = world.step(sess, "w")
		game.new_probe_fields = False
		check("a probe that sets self_hides_pick on open ground (the current client) does not block aiming",
		      fresh["outcome"] == "move_confirm" and fresh["arrived"] is True, fresh)

		game.swallow_clicks = True
		silent = world.click_tile(sess, (137, 37), timeout_ms=300)
		game.swallow_clicks = False
		check("a click that sends nothing is no_answer with a hint, not a bare null",
		      silent["outcome"] == "no_answer" and "no move_request left the client" in silent["hint"]
		      and silent["arrived"] is None, silent)
		world.click_tile(sess, (138, 36))
		expect_error("a tile under the HUD / off the view is refused, not clicked",
		             lambda: world.aim_tile(sess, (138, 60)), "no clickable point")

		proof = flows.read_config_mute(game.client_json)
		check("read_config_mute proves a config mute from client.json alone",
		      proof["muted"] is True and proof["clicked"] is False and proof["proof"] == "client.json", proof)
		game.screen = "Screen_OnGame"
		off_menu = flows.prove_launch_mute(sess, "config", game.client_json)
		check("a config mute is proven from client.json on a logged-in start screen (HB-0217)",
		      off_menu["proof"] == "client.json" and game.screen == "Screen_OnGame", off_menu)
		game.muted = False
		bridged = flows.prove_launch_mute(sess, "bridge", game.client_json)
		check("a bridge mute on a logged-in start screen is made with set_audio",
		      bridged["via"] == "set_audio" and game.muted and game.screen == "Screen_OnGame", bridged)
		game.muted = False
		game.persist()

		switched = flows.switch_character(sess)
		check("switch_character opens Settings, finds System, lands on character select",
		      switched["screen"] == "Screen_SelectCharacter", switched)
		watch_world = server_logs.log_watch(game.world_log)
		flows.enter_world(sess, name)
		flows.switch_character(sess)
		proof = server_logs.wait_logout_saved(watch_world, game.character_ids[name], 3)
		check("the logout save is proven from the world log", proof["x"] == 138 and proof["map"] == "travelerzone", proof)

		game.spawn_after_swap = True  # the pre-HB-0156 order must work too
		timing = world.time_entry(sess, name, capture_logs=True)
		game.spawn_after_swap = False
		points = timing["points_ms"]
		check("time_entry reports A, D, G, closed, H, J in order and a total",
		      timing["outcome"] == "move_confirm" and timing["total_ms"] is not None
		      and points["A_start_click"] <= points["D_entering_opened"] <= points["entering_closed"]
		      <= points["H_move_request"] <= points["J_move_answer"], timing)
		check("time_entry clicked one tile east through window space",
		      timing["click"]["space"] == "window" and timing["moved"]["to"][0] == timing["moved"]["from"][0] + 1, timing)

		check("without the client's markers the spawn frame stands in", timing["ready_marker"] == "spawn_frame"
		      and points["world_ready"] is None, timing)
		flows.switch_character(sess)
		game.emit_ready_markers = True
		game.spawn_after_swap = True
		timing = world.time_entry(sess, name, capture_logs=False)
		game.spawn_after_swap = False
		game.emit_ready_markers = False
		points = timing["points_ms"]
		check("time_entry uses world_ready / first_frame_presented / screen_active when the client sends them",
		      timing["ready_marker"] == "world_ready" and points["world_ready"] is not None
		      and points["first_frame_presented"] is not None and points["screen_active"] is not None
		      and timing["world_ready_at"]["map"] == "travelerzone"
		      and timing["spans_ms"]["start_to_world_ready"] == points["world_ready"] - points["A_start_click"]
		      and timing["spans_ms"]["world_ready_to_move_request"] is not None, timing)

		# the MCP logout / switch flows write the logout-save proof when given a proof_file (HB-0361)
		proof_path = os.path.join(scratch, "mcp_logout_proof.json")
		real_hooks = (common.world_log_path, player_db.db_connection, player_db.character_row)
		common.world_log_path = game.world_log
		player_db.db_connection = lambda config=None: None
		player_db.character_row = lambda db, who: {"id": game.character_ids[who], "account_id": 1, "account": "a"}
		try:
			game.character = name
			expect_error("a proof_file outside working/ is refused before the logout runs",
			             lambda: mcp_flows.run_flow("logout", {"flow": "logout", "proof_file": "C:/nowhere/p.json"}, sess),
			             "must live under")
			check("...and the client is still in the world", sess.screen() == "Screen_OnGame")
			result = mcp_flows.run_flow("logout", {"flow": "logout", "proof_file": proof_path}, sess)
			written = common.read_json_file(proof_path)
			check("the MCP logout flow writes the proof (the character defaults to the one played)",
			      result["logout_proof"] == written and result["logout_proof_file"] == proof_path
			      and written["x"] == game.player[0] and sess.screen() == "Screen_MainMenu", (result, written))
			flows.log_in(sess, ident)
			flows.enter_world(sess, name)
			relative = os.path.join(".claude", "scratch", "hb_driver_selftest", "mcp_switch_proof.json")
			switched = mcp_flows.run_flow("switch", {"flow": "switch", "proof_file": relative, "name": name,
			                                          "save_timeout": 3}, sess)
			check("switch takes a repo-relative proof_file and an explicit name",
			      os.path.isfile(os.path.join(common.repo_root, relative)) and "logout_proof" in switched
			      and switched["screen"] == "Screen_SelectCharacter", switched)
			plain = mcp_flows.run_flow("logout", {"flow": "logout"}, sess)
			check("without proof_file the flow writes none and adds nothing", "logout_proof" not in plain, plain)
		finally:
			common.world_log_path, player_db.db_connection, player_db.character_row = real_hooks
		check("log_out from the world reaches the main menu", sess.screen() == "Screen_MainMenu")
		to_login = flows.log_out(sess, to="login")
		check("log_out to='login' ends on the login screen (HB-0625)",
		      to_login["screen"] == "Screen_Login" and sess.screen() == "Screen_Login", to_login)
		expect_error("log_out refuses an unknown target", lambda: flows.log_out(sess, to="desktop"), "must be one of")
		flows.log_in(sess, ident)
		deleted = flows.delete_character(sess, ident, second)
		check("delete_character removes a character this tooling made", second not in game.characters[ident.account], deleted)
		expect_error("delete refuses a character this tooling did not make",
		             lambda: flows.delete_character(sess, ident, "Stranger"), "refused")
		game.muted = True
		game.persist()
		unmute = flows.set_master_mute(sess, False, client_json=game.client_json, leave_session=True)
		check("unmute with leave_session logs out first, then plays", not game.muted and unmute["clicked"], unmute)
		sess.close()

		print("CLI against the fake client")
		cli = [sys.executable, os.path.join(here, "hb_flow.py")]
		run = subprocess.run(cli + ["state", "--port", str(server.port)], capture_output=True, text=True, timeout=60)
		check("hb_flow.py state prints JSON", run.returncode == 0 and json.loads(run.stdout)["screen"] == "Screen_MainMenu",
		      run.stdout + run.stderr)
		ident_cli = os.path.join(scratch, "ident_cli.json")
		run = subprocess.run(cli + ["identity-new", "--file", ident_cli], capture_output=True, text=True, timeout=60)
		shown = json.loads(run.stdout) if run.returncode == 0 else {}
		secret = identity.load_identity(ident_cli).password if os.path.exists(ident_cli) else "?"
		check("identity-new prints the identity with the password masked",
		      shown.get("password") == "*" * 10 and secret not in run.stdout, run.stdout + run.stderr)
		run = subprocess.run(cli + ["create-account", "--port", str(server.port), "--identity", ident_cli],
		                     capture_output=True, text=True, timeout=60)
		check("hb_flow.py create-account works end to end", run.returncode == 0 and secret not in run.stdout,
		      run.stdout + run.stderr)
		run = subprocess.run(cli + ["login", "--port", str(server.port), "--identity", os.path.join(common.repo_root, "x.json")],
		                     capture_output=True, text=True, timeout=60)
		check("a CLI failure is ERROR on stderr and exit 1", run.returncode == 1 and run.stderr.startswith("ERROR:"),
		      run.stdout + run.stderr)
		run = subprocess.run(cli + ["--help"], capture_output=True, text=True, timeout=60)
		check("--help lists the flows", run.returncode == 0 and "time-entry" in run.stdout and "position-write" in run.stdout)
		relative_backup = os.path.join(".claude", "scratch", "hb_driver_selftest", "files_backup")
		run = subprocess.run(cli + ["restore-client-files", "--backup-dir", relative_backup], capture_output=True,
		                     text=True, timeout=60, cwd=os.path.dirname(common.repo_root))
		check("--backup-dir takes a path relative to the repo root, from any cwd", run.returncode == 0
		      and "client_files" in run.stdout, run.stdout + run.stderr)
		run = subprocess.run(cli + ["launch", "--port", "1", "--pid-file", os.path.join(common.repo_root, "pids.txt"),
		                            "--backup-dir", relative_backup], capture_output=True, text=True, timeout=60)
		check("a --pid-file outside working/ is refused before anything launches",
		      run.returncode == 1 and "--pid-file" in run.stderr, run.stdout + run.stderr)
	finally:
		common.login_log_path, common.world_log_path, common.client_log_path = saved_paths
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
