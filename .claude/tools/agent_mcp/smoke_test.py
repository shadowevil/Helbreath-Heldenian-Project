"""Smoke test for agent_mcp.py: the MCP handshake, the channel matcher, the grep filter, and a
full journal_query / wait_for round trip against a FAKE bridge standing in for the client.

The point of the fake bridge is that this runs with NO game running and no dev servers, so the
server's own plumbing - capabilities, tool schemas, reply demultiplexing, rendering, the dropped
event warning - is checked without standing up a client. It does NOT verify the client side of the
contract; only a real debug client does that (.claude/docs/agent_bridge_playbook.md).

It binds port 8899 rather than the real 8891-8898 deliberately, so running it can never steal the socket
from a debug client the user is testing on.

Run from the repo root:  python .claude/tools/agent_mcp/smoke_test.py
Exits non-zero on any failure, like the .ps1 verify scripts in .claude/tools/scripts/.
"""

import json
import os
import shutil
import socket
import struct
import subprocess
import sys
import threading
import time
import zlib

REPO = os.environ.get("HB_REPO") or os.getcwd()  # run from the repo root
# the MCP server under test saves its screenshots here (inside the repo, gitignored), deleted at the end
SHOT_DIR = os.path.join(REPO, ".claude", "scratch", "agent_mcp_smoke_shots")
SERVER = None
PORT = 8899  # outside 8891-8898: never collide with a real debug client or a discovery scan

failures = []


def check(label, condition, detail=""):
    if condition:
        print("  PASS  {}".format(label))
    else:
        print("  FAIL  {}  {}".format(label, detail))
        failures.append(label)


# ---------------------------------------------------------------- unit: channel matcher + filter

sys.path.insert(0, os.path.join(REPO, ".claude", "tools", "agent_mcp"))
os.environ["HB_AGENT_PORT"] = str(PORT)
import agent_mcp  # noqa: E402

print("channel_matches (segment prefix, per the contract)")
check("'state' matches 'state.player'", agent_mcp.channel_matches("state", "state.player"))
check("'client' matches 'client.warnings'", agent_mcp.channel_matches("client", "client.warnings"))
check("'client' matches 'client.errors'", agent_mcp.channel_matches("client", "client.errors"))
check("'net' matches 'net.errors'", agent_mcp.channel_matches("net", "net.errors"))
check("'net.packet' matches itself", agent_mcp.channel_matches("net.packet", "net.packet"))
check("'stat' does NOT match 'state.player' (the documented trap)",
      not agent_mcp.channel_matches("stat", "state.player"))
check("'state.player' does not match shorter 'state'",
      not agent_mcp.channel_matches("state.player", "state"))
check("'*' matches anything", agent_mcp.channel_matches("*", "ui.window"))
check("empty selector matches anything", agent_mcp.channel_matches("", "ui.window"))

print("event_subject (what a filter is matched against)")
event = {"seq": 1, "t_ms": 100, "frame": 5, "channel": "client.warnings", "type": "sound_refused",
         "data": {"id": "char_footstep_run", "source": 8, "reason": "per_source_cap"}}
subject = agent_mcp.event_subject(event)
check("starts with channel/type", subject.startswith("client.warnings/sound_refused"), subject)
check("carries the payload id", "char_footstep_run" in subject, subject)

print("severity mapping - the diagnostic channel IS the level")
check("client.errors maps to error",
      agent_mcp.journal_pump._level_for({"channel": "client.errors"}) == "error")
check("net.errors maps to error",
      agent_mcp.journal_pump._level_for({"channel": "net.errors"}) == "error")
check("client.warnings maps to warning",
      agent_mcp.journal_pump._level_for({"channel": "client.warnings"}) == "warning")
check("net.warnings maps to warning",
      agent_mcp.journal_pump._level_for({"channel": "net.warnings"}) == "warning")
check("an observational channel maps to info",
      agent_mcp.journal_pump._level_for({"channel": "audio.sound"}) == "info")

print("subscription filtering")
# No bridge: set_subscription and _passes never touch it, and the thread only starts on subscribe.
pump = agent_mcp.journal_pump(None, "smoke")
pump.set_subscription(["client"], None, None)
check("channel selection keeps client.warnings", pump._passes(event))
check("channel selection drops ui.window",
      not pump._passes({"channel": "ui.window", "type": "window_opened", "data": {}}))
pump.set_subscription([], "char_footstep_run", None)
check("filter keeps a payload match", pump._passes(event))
pump.set_subscription([], "char_footstep_walk", None)
check("filter drops a non-match", not pump._passes(event))
pump.set_subscription([], None, "per_source_cap")
check("exclude drops a match", not pump._passes(event))
pump.set_subscription([], "char_footstep_run", "per_source_cap")
check("exclude beats filter on a tie", not pump._passes(event))
pump.set_subscription([], "CHAR_FOOTSTEP_RUN", None)
check("filter is case-insensitive", pump._passes(event))

print("hb_wait_for trail (HB-0125)")
many = {"satisfied": False, "state": {}, "cursor": 99, "dropped": 0,
        "events": [{"seq": n, "t_ms": n, "frame": n, "channel": "state.player", "type": "tick", "data": {}}
                   for n in range(1, 31)]}
default_text = agent_mcp.format_wait_for(many)
check("the default trail lists only the last {} events".format(agent_mcp.WAIT_FOR_TRAIL_DEFAULT),
      default_text.count("state.player/tick") == agent_mcp.WAIT_FOR_TRAIL_DEFAULT and "[30ms" in default_text
      and "[20ms" not in default_text, default_text)
check("the trimmed trail says how to widen it", "the last 10 of 30" in default_text, default_text)
check("trail None lists every event", agent_mcp.format_wait_for(many, None).count("state.player/tick") == 30)
silent = agent_mcp.format_wait_for(many, 0)
check("trail 0 lists none and says so", "state.player/tick" not in silent and "not listed" in silent, silent)
check("trail_limit maps true / false / N / omitted",
      agent_mcp.trail_limit({"trail": True}) is None and agent_mcp.trail_limit({"trail": False}) == 0
      and agent_mcp.trail_limit({"trail": 3}) == 3
      and agent_mcp.trail_limit({}) == agent_mcp.WAIT_FOR_TRAIL_DEFAULT)
try:
    agent_mcp.trail_limit({"trail": "all"})
    check("trail_limit refuses a string", False)
except ValueError:
    check("trail_limit refuses a string", True)

print("image_collector (HB-0119)")
collector = agent_mcp.image_collector(2)
check("images are numbered and capped",
      [collector.add("a", "image/png"), collector.add("b", "image/png"), collector.add("c", "image/png")]
      == [1, 2, None] and len(collector.blocks()) == 2)

print("hb_screenshot options without Pillow")
saved_image = agent_mcp.Image
agent_mcp.Image = None
options, problem = agent_mcp.parse_screenshot_options({"scale": 0.5})
check("scale without Pillow is a clear error", options is None and "Pillow" in problem, problem)
options, problem = agent_mcp.parse_screenshot_options({"format": "jpg"})
check("jpg without Pillow is a clear error", options is None and "Pillow" in problem, problem)
options, problem = agent_mcp.parse_screenshot_options({"name": "plain"})
check("a plain capture needs no Pillow", problem is None and options["processed"] is False, problem)
agent_mcp.Image = saved_image

print("port holders (hb_clients register)")
canned = """
  Proto  Local Address          Foreign Address        State           PID
  TCP    127.0.0.1:8795         0.0.0.0:0              LISTENING       1234
  TCP    127.0.0.1:8795         127.0.0.1:52000        ESTABLISHED     1234
  TCP    127.0.0.1:52000        127.0.0.1:8795         ESTABLISHED     5678
  TCP    127.0.0.1:8796         0.0.0.0:0              LISTENING       999
"""
saved_run_quiet = agent_mcp.run_quiet
saved_platform = agent_mcp.sys.platform
agent_mcp.run_quiet = lambda command: canned if command[0] == "netstat" else ""
agent_mcp.sys.platform = "win32"
holders = agent_mcp.port_holders("127.0.0.1", 8795)
agent_mcp.run_quiet = saved_run_quiet
agent_mcp.sys.platform = saved_platform
check("netstat rows give the listener and the connected peer",
      holders == {"listeners": [1234], "peers": [5678]}, holders)

print("unknown keys")
check("a journal key outside the contract is named, with a hint",
      "object_id" in (agent_mcp.unknown_keys_problem(
          "hb_journal_drain", {"object_id": 5, "from": 1}, agent_mcp.JOURNAL_DRAIN_KEYS,
          agent_mcp.JOURNAL_DRAIN_HINTS) or "")
      and "'from' -> 'from_ms'" in agent_mcp.unknown_keys_problem(
          "hb_journal_drain", {"from": 1}, agent_mcp.JOURNAL_DRAIN_KEYS, agent_mcp.JOURNAL_DRAIN_HINTS))
check("known journal keys pass",
      agent_mcp.unknown_keys_problem("hb_journal_drain", {"max": 5, "client": "c1"},
                                     agent_mcp.JOURNAL_DRAIN_KEYS) is None)


def write_png(path, width, height):
    """A valid RGB gradient PNG, stdlib only (the fake client's screenshot)."""
    def chunk(kind, data):
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)
    rows = b""
    for y in range(height):
        rows += b"\x00" + bytes(value for x in range(width)
                                for value in (x * 255 // max(1, width - 1), y * 255 // max(1, height - 1), 128))
    with open(path, "wb") as handle:
        handle.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
                     + chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b""))


# ---------------------------------------------------------------- fake bridge

# What the fake implements: the whole SDL-era set, so every tool is listed. A test narrows it to phase 1.
ALL_COMMANDS = ["ping", "get_state", "probe_point", "screenshot", "mouse_move", "click", "mouse_down", "mouse_up",
                "wheel", "key", "text", "set_text", "modifiers", "wait_for", "widgets", "widget_click",
                "journal_query", "journal_drain", "journal_config", "entities", "tile_to_screen", "walk_to",
                "close_window", "set_audio", "inject_message", "set_time_of_day", "set_god", "set_staff_tags",
                "fishing_auto_strike", "path_bench", "press_bench", "kill_player", "set_status", "set_item_tint",
                "set_tab_width_cap", "reload_pivots", "reload_lights", "light_clock", "self_tests"]
PHASE1_COMMANDS = ["ping", "get_state", "probe_point", "screenshot", "mouse_move", "click", "mouse_down", "mouse_up",
                   "wheel", "key", "text", "set_text", "modifiers", "wait_for"]


# The argument keys the client accepts per command (agent_bridge.cpp's command table); any other key is refused
# the way the client refuses it. widget_click also takes the older "id" for the SDL-id tools that are still hidden.
ACCEPTED_KEYS = {
    "ping": "", "get_state": "", "screenshot": "name path", "probe_point": "x y space",
    "mouse_move": "x y space timeout_ms", "click": "x y space button count shift ctrl timeout_ms",
    "mouse_down": "x y space button timeout_ms", "mouse_up": "x y space button timeout_ms",
    "wheel": "x y space notches timeout_ms", "key": "name shift ctrl alt repeat timeout_ms",
    "text": "value timeout_ms", "set_text": "value clear timeout_ms", "modifiers": "shift ctrl alt timeout_ms",
    "wait_for": "conditions timeout_ms since", "widgets": "",
    "widget_click": "name id button count shift ctrl timeout_ms",
    "journal_query": "channels types exclude_types object_id message_contains from_ms to_ms after_seq before_seq max",
    "journal_drain": "channels types exclude_types object_id message_contains since max",
    "journal_config": "channels capacity test_log", "entities": "kinds radius max on_screen",
    "tile_to_screen": "x y", "walk_to": "x y timeout_ms wait", "close_window": "name dialog_id",
    "set_audio": "master music effects",
}


class fake_bridge(threading.Thread):
    """Accepts one connection and answers commands the way agent_bridge is contracted to."""

    def __init__(self, port):
        super().__init__(daemon=True)
        self.listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.listener.bind(("127.0.0.1", port))
        self.listener.listen(1)
        self.seen = []
        self.commands = list(ALL_COMMANDS)
        self.protocol = 5
        self.native_widgets = False  # answer widgets in this client's own shape (name, rect, masked, ...)
        self.book_open = False   # the spell book dialog (hb_cast_at opens and closes it)
        self.circle = 0          # its circle page
        self.cast_outcome = "resolved"   # what a cast's aim ends in: resolved | refused | timeout

    def run(self):
        conn, _ = self.listener.accept()
        buf = b""
        while True:
            try:
                chunk = conn.recv(65536)
            except OSError:
                return
            if not chunk:
                return
            buf += chunk
            while b"\n" in buf:
                line, _, buf = buf.partition(b"\n")
                if not line.strip():
                    continue
                request = json.loads(line.decode("utf-8"))
                self.seen.append(request)
                conn.sendall((json.dumps(self.answer(request)) + "\n").encode("utf-8"))

    def answer(self, request):
        cmd = request.get("cmd")
        rid = request.get("id")
        if cmd in ACCEPTED_KEYS:
            unknown = sorted(set(request.get("args") or {}) - set(ACCEPTED_KEYS[cmd].split()))
            if unknown:
                return {"id": rid, "ok": False, "error": "{}: unknown argument '{}'".format(cmd, unknown[0])}
        if cmd == "ping":
            return {"id": rid, "ok": True, "result": {"build": "0.0.1-alpha:TEST", "bridge_protocol": self.protocol,
                                                      "commands": list(self.commands)}}
        if cmd == "journal_query" and (request.get("args") or {}).get("channels") == ["state.multi"]:
            after = (request.get("args") or {}).get("after_seq") or 0
            events = [{"seq": seq, "t_ms": seq, "frame": seq, "channel": "state.multi", "type": "tick", "data": {"k": seq % 2}}
                      for seq in range(105, after, -1)]
            return {"id": rid, "ok": True, "result": {"live": False, "newest_seq": 105, "events": events, "more_older": False}}
        if cmd == "journal_query" and (request.get("args") or {}).get("channels") == ["state.few"]:
            after = (request.get("args") or {}).get("after_seq") or 0
            events = [{"seq": 202, "t_ms": 202, "frame": 202, "channel": "state.few", "type": "tick", "data": {}}] if after < 202 else []
            return {"id": rid, "ok": True, "result": {"live": False, "newest_seq": 202, "events": events, "more_older": False}}
        if cmd == "journal_query" and (request.get("args") or {}).get("types") and (request.get("args") or {}).get("after_seq") is not None \
                and "spell_cast_resolved" in ((request.get("args") or {}).get("types") or []):
            return {"id": rid, "ok": True, "result": {"live": False, "newest_seq": 904, "more_older": False, "events": [
                {"seq": 903, "t_ms": 3000, "frame": 90, "channel": "visual.effect", "type": "spell_cast_resolved",
                 "data": {"cast": 5, "caster": 33, "spell": 1, "beats": 2}},
                {"seq": 902, "t_ms": 2000, "frame": 80, "channel": "visual.effect", "type": "spell_release_sent",
                 "data": {"target_id": 19, "spell": "heal", "on_self": False}},
                {"seq": 901, "t_ms": 1000, "frame": 70, "channel": "visual.effect", "type": "spell_cast_begin",
                 "data": {"caster": 33, "spell": 1, "cast_ms": 1280}}]}}
        if cmd == "journal_query" and (request.get("args") or {}).get("types") and (request.get("args") or {}).get("after_seq") is not None \
                and "spell_hit" in ((request.get("args") or {}).get("types") or []):
            return {"id": rid, "ok": True, "result": {"live": False, "newest_seq": 904, "more_older": False, "events": [
                {"seq": 904, "t_ms": 3100, "frame": 91, "channel": "visual.damage", "type": "spell_hit",
                 "data": {"cast": 5, "caster": 33, "target": 19, "spell": 1, "amount": 12, "killed": False}}]}}
        if cmd == "journal_query" and (request.get("args") or {}).get("types") and (request.get("args") or {}).get("after_seq") is not None:
            return {"id": rid, "ok": True, "result": {"live": False, "newest_seq": 904, "more_older": False, "events": []}}
        if cmd == "journal_query":
            return {"id": rid, "ok": True, "result": {
                "live": True, "window": {}, "now_ms": 1200, "newest_seq": 41,
                "oldest_seq_retained": 1, "oldest_t_ms_retained": 0,
                "more_older": True, "older_before_seq": 40, "raw": 2, "events": [
                    {"seq": 41, "t_ms": 1150, "frame": 19, "channel": "client.warnings",
                     "type": "sound_refused",
                     "data": {"id": "char_footstep_run", "source": 8, "reason": "per_source_cap"}},
                    {"seq": 40, "t_ms": 1000, "frame": 10, "channel": "audio.sound",
                     "type": "sound_play", "data": {"id": "char_footstep_run", "source": 8}},
                ]}}
        if cmd == "wait_for":
            conditions = (request.get("args") or {}).get("conditions") or []
            first = conditions[0] if conditions else {}
            if first.get("channel") == "state.multi":
                return {"id": rid, "ok": True, "result": {
                    "satisfied": True, "matched": {"index": 0, "condition": first, "event": {
                        "seq": 101, "t_ms": 101, "frame": 101, "channel": "state.multi", "type": "tick", "data": {"k": 1}}},
                    "state": {"screen": "Screen_OnGame"}, "cursor": 106, "dropped": 0, "events": []}}
            if first.get("channel") == "state.few":
                return {"id": rid, "ok": True, "result": {
                    "satisfied": True, "matched": {"index": 0, "condition": first, "event": {
                        "seq": 201, "t_ms": 201, "frame": 201, "channel": "state.few", "type": "tick", "data": {}}},
                    "state": {"screen": "Screen_OnGame"}, "cursor": 202, "dropped": 0, "events": []}}
            if first.get("type") == "spell_cast_begin":
                return {"id": rid, "ok": True, "result": {
                    "satisfied": True, "matched": {"index": 0, "condition": first, "event": {
                        "seq": 901, "t_ms": 1000, "frame": 70, "channel": "visual.effect", "type": "spell_cast_begin",
                        "data": {"caster": 33, "spell": 1, "cast_ms": 1280}}},
                    "state": {}, "cursor": 902, "dropped": 0, "events": []}}
            if first.get("type") == "spell_cast_resolved":
                if self.cast_outcome == "refused":
                    return {"id": rid, "ok": True, "result": {
                        "satisfied": True, "matched": {"index": 3, "condition": conditions[3], "event": {
                            "seq": 903, "t_ms": 3000, "frame": 90, "channel": "net.packet", "type": "packet_in",
                            "data": {"message": "action_notice"}}}, "state": {}, "cursor": 904, "dropped": 0, "events": []}}
                if self.cast_outcome == "timeout":
                    return {"id": rid, "ok": True, "result": {"satisfied": False, "state": {}, "cursor": 904,
                                                              "dropped": 0, "events": []}}
                return {"id": rid, "ok": True, "result": {
                    "satisfied": True, "matched": {"index": 0, "condition": first, "event": {
                        "seq": 903, "t_ms": 3000, "frame": 90, "channel": "visual.effect", "type": "spell_cast_resolved",
                        "data": {"cast": 5, "caster": 33, "spell": 1, "beats": 2}}}, "state": {}, "cursor": 904,
                    "dropped": 0, "events": []}}
            if first.get("type") == "spell_hit":
                return {"id": rid, "ok": True, "result": {"satisfied": True, "matched": {"index": 0, "condition": first},
                                                          "state": {}, "cursor": 905, "dropped": 0, "events": []}}
            if first.get("state"):
                # a STATE condition reports what was observed
                return {"id": rid, "ok": True, "result": {
                    "satisfied": True,
                    "matched": {"index": 0, "condition": first, "observed": "Screen_OnGame"},
                    "state": {"screen": "Screen_OnGame"},
                    "cursor": 42, "dropped": 0, "events": []}}
            if first.get("type") == "walk_arrived":
                goal = first.get("match") or {}
                if goal.get("goal_x") == 77:
                    return {"id": rid, "ok": True, "result": {
                        "satisfied": True, "matched": {"index": 1, "condition": conditions[1], "event": {
                            "seq": 50, "t_ms": 2000, "frame": 30, "channel": "state.player", "type": "walk_stopped",
                            "data": {"tile_x": 76, "tile_y": 5, "goal_x": 77, "goal_y": 5, "arrived": False,
                                     "reason": "blocked"}}},
                        "state": {"screen": "Screen_OnGame"}, "cursor": 51, "dropped": 0, "events": []}}
                if goal.get("goal_x") == 88:
                    return {"id": rid, "ok": True, "result": {
                        "satisfied": False, "state": {"screen": "Screen_OnGame"}, "cursor": 51, "dropped": 0,
                        "events": []}}
                return {"id": rid, "ok": True, "result": {
                    "satisfied": True, "matched": {"index": 0, "condition": first, "event": {
                        "seq": 50, "t_ms": 2000, "frame": 30, "channel": "state.player", "type": "walk_arrived",
                        "data": {"tile_x": goal.get("goal_x"), "tile_y": goal.get("goal_y"),
                                 "goal_x": goal.get("goal_x"), "goal_y": goal.get("goal_y"), "arrived": True}}},
                    "state": {"screen": "Screen_OnGame"}, "cursor": 51, "dropped": 0, "events": []}}
            if first.get("channel") == "client.warnings":
                # an EVENT condition reports the event that fired it
                return {"id": rid, "ok": True, "result": {
                    "satisfied": True,
                    "matched": {"index": 0, "condition": first, "event": {
                        "seq": 41, "t_ms": 1150, "frame": 19, "channel": "client.warnings",
                        "type": "sound_refused",
                        "data": {"id": "char_footstep_run", "reason": "per_source_cap"}}},
                    "state": {"screen": "Screen_OnGame"},
                    "cursor": 42, "dropped": 0, "events": []}}
            if first.get("channel") == "state.player":
                # a long trail: 30 events seen while the wait ran
                return {"id": rid, "ok": True, "result": {
                    "satisfied": False, "state": {"screen": "Screen_OnGame"}, "cursor": 99, "dropped": 0,
                    "events": [{"seq": n, "t_ms": n, "frame": n, "channel": "state.player", "type": "tick",
                                "data": {}} for n in range(1, 31)]}}
            return {"id": rid, "ok": True, "result": {
                "satisfied": False, "state": {"screen": "Screen_Login"},
                "cursor": 42, "dropped": 0, "events": []}}
        if cmd == "get_state":
            return {"id": rid, "ok": True, "result": {
                "screen": "Screen_Login", "transitioning": False,
                "audio": {"master_muted": True, "master": 80, "music": 50, "effects": 70},
                "memory": {"working_set_kb": 524288, "commit_kb": 600000, "peak_working_set_kb": 600000,
                           "clip_cache_bytes": 12582912, "clip_cache_entries": 120,
                           "clip_cache_budget_bytes": 67108864, "tile_sheet_cache_bytes": 4194304,
                           "minimap_cache_bytes": 524288}}}
        if cmd == "set_audio":
            args = request.get("args") or {}
            return {"id": rid, "ok": True, "result": {
                "master": args.get("master", 100), "music": args.get("music", 80), "effects": args.get("effects", 70),
                "persisted": False}}
        if cmd == "widgets" and self.native_widgets:
            return {"id": rid, "ok": True, "result": {"surface": "Screen_Login", "overlay": False, "transitioning": False,
                                                      "widgets": [
                {"name": "login.name", "kind": "text_input", "owner": "screen:Screen_Login", "enabled": True,
                 "visible": True, "focused": False, "rect": {"x": 300, "y": 200, "w": 160, "h": 20},
                 "masked": False, "length": 4},
                {"name": "login.password", "kind": "text_input", "owner": "screen:Screen_Login", "enabled": True,
                 "visible": True, "focused": True, "rect": {"x": 300, "y": 230, "w": 160, "h": 20}, "masked": True},
                {"name": "login.ok", "kind": "button", "owner": "screen:Screen_Login", "enabled": True,
                 "visible": True, "focused": False, "rect": {"x": 300, "y": 280, "w": 80, "h": 22}}]}}
        if cmd == "widgets" and self.book_open:
            rows = [{"id": "spell_book.title", "kind": "grip", "label": "Spells", "owner": "dialog:spell_book",
                     "x": 315, "y": 72, "w": 222, "h": 34, "enabled": True}]
            if self.circle == 1:
                rows.append({"id": "spell_book.spell_energy_bolt", "kind": "list_item", "label": "Energy Bolt",
                             "owner": "dialog:spell_book", "x": 327, "y": 117, "w": 199, "h": 29, "enabled": True, "value": "off"})
            else:
                rows.append({"id": "spell_book.spell_heal", "kind": "list_item", "label": "Heal",
                             "owner": "dialog:spell_book", "x": 327, "y": 117, "w": 199, "h": 29, "enabled": True, "value": "off"})
                rows.append({"id": "spell_book.spell_recall", "kind": "list_item", "label": "Recall",
                             "owner": "dialog:spell_book", "x": 327, "y": 146, "w": 199, "h": 29, "enabled": False, "value": "off"})
            return {"id": rid, "ok": True, "result": {"ui_width": 853, "ui_height": 480, "widgets": rows}}
        if cmd == "widgets":
            return {"id": rid, "ok": True, "result": {"ui_width": 853, "ui_height": 480, "widgets": [
                {"id": "inventory.slot0", "kind": "slot", "label": "", "owner": "dialog:inventory", "x": 10, "y": 20,
                 "w": 32, "h": 32, "enabled": True, "slot": 0, "item_id": 115, "quantity": 2, "aim": {"x": 14, "y": 26}},
                {"id": "inventory.slot1", "kind": "slot", "label": "", "owner": "dialog:inventory", "x": 50, "y": 20,
                 "w": 32, "h": 32, "enabled": True, "slot": 1, "quantity": 0},
                {"id": "inventory.slot30", "kind": "slot", "label": "", "owner": "dialog:inventory", "x": 50, "y": 900,
                 "w": 32, "h": 32, "enabled": True, "slot": 30, "quantity": 0, "visible": False},
                {"id": "settings.sound_master_volume", "kind": "slider", "label": "Master", "owner": "dialog:settings",
                 "x": 100, "y": 60, "w": 120, "h": 12, "enabled": True, "value": "80%",
                 "track": {"x": 100, "y": 64, "w": 120, "h": 4}, "thumb": {"x": 196, "y": 60, "w": 8, "h": 12}},
                {"id": "settings.language", "kind": "dropdown", "label": "Language", "owner": "dialog:settings",
                 "x": 100, "y": 90, "w": 120, "h": 16, "enabled": True, "options": ["English", "Deutsch"],
                 "selected": 0}]}}
        if cmd == "widget_click":
            args = request.get("args") or {}
            wanted = args.get("name", args.get("id"))
            if wanted == "inventory.slot30":
                return {"id": rid, "ok": False, "error": "widget is scrolled out of view: inventory.slot30"}
            if wanted == "hud.spellbook":
                self.book_open = not self.book_open
                self.circle = 0
            if str(wanted or "").startswith("spell_book.circle"):
                self.circle = int(wanted[len("spell_book.circle"):])
            return {"id": rid, "ok": True, "result": {"widget": wanted, "enabled": True, "x": 1, "y": 2,
                                                      "button": args.get("button", "left")}}
        if cmd == "screenshot":
            args = request.get("args") or {}
            if "path" in args:
                return {"id": rid, "ok": False, "error": "screenshot takes 'name'; 'path' is refused"}
            os.makedirs(SHOT_DIR, exist_ok=True)
            write_png(os.path.join(SHOT_DIR, args.get("name")), 192, 108)
            return {"id": rid, "ok": True, "result": {"name": args.get("name"), "width": 192, "height": 108}}
        if cmd == "entities":
            return {"id": rid, "ok": True, "result": {
                "player_tile": [134, 37], "map": "default", "total": 3, "count": 3, "truncated": False, "entities": [
                    {"object_id": 19, "kind": "npc", "name": "gandalf", "type": 15, "tile": [136, 36], "distance": 2,
                     "on_screen": True, "dead": False, "point": {"logical": [470, 220], "window": [1060, 480]},
                     "npc_config_id": 7},
                    {"object_id": 44, "kind": "creature", "name": "slime", "type": 10, "tile": [150, 40], "distance": 16,
                     "on_screen": False, "dead": False, "point": {"logical": [999, 300], "window": [2100, 700]}},
                    {"kind": "item", "name": "dagger", "item_id": 115, "tile": [134, 38], "distance": 1,
                     "on_screen": True, "point": {"logical": [426, 265], "window": [960, 590]}},
                ]}}
        if cmd == "tile_to_screen":
            args = request.get("args") or {}
            if args.get("x") == 999:
                return {"id": rid, "ok": True, "result": {"tile": [999, 5], "in_bounds": False, "on_screen": False,
                                                          "logical": [0, 0], "window": [0, 0], "player_tile": [134, 37]}}
            return {"id": rid, "ok": True, "result": {
                "tile": [args.get("x"), args.get("y")], "in_bounds": True, "on_screen": True, "logical": [460, 240],
                "window": [1034, 540], "player_tile": [134, 37], "resolves_to": [args.get("x"), args.get("y")],
                "round_trip": True}}
        if cmd == "walk_to":
            args = request.get("args") or {}
            if args.get("x") == 99:
                return {"id": rid, "ok": False, "error": "walk_to refused: (99,9) is outside the map"}
            status = {77: "blocked", 88: "timeout"}.get(args["x"], "arrived" if args.get("wait") else "walking")
            reply = {"status": status, "goal": [args["x"], args["y"]], "from": [134, 37]}
            if args.get("wait"):
                reply.update({"at": [args["x"] - (1 if status == "blocked" else 0), args["y"]], "map": "default",
                              "elapsed_ms": 1200})
                if status == "blocked":
                    reply["reason"] = "no step led nearer for 2.5 s"
            return {"id": rid, "ok": True, "result": reply}
        if cmd == "set_time_of_day":
            t01 = (request.get("args") or {}).get("t01")
            return {"id": rid, "ok": True, "result": {
                "sky_time_override": None if t01 == "off" else t01, "sky_time01": 0.1 if t01 == "off" else t01,
                "map_light": "cycle"}}
        if cmd == "set_god":
            on = bool((request.get("args") or {}).get("on"))
            return {"id": rid, "ok": True, "result": {"god": on, "flags_wanted": 3 if on else 1, "sent": True}}
        if cmd == "close_window":
            name = (request.get("args") or {}).get("name")
            return {"id": rid, "ok": True, "result": {"closed": [{"name": name, "dialog_id": 2}] if name == "inventory" else []}}
        if cmd == "set_staff_tags":
            return {"id": rid, "ok": True, "result": {"visible": bool((request.get("args") or {}).get("visible"))}}
        if cmd == "fishing_auto_strike":
            args = request.get("args") or {}
            return {"id": rid, "ok": True, "result": {"on": args.get("on"), "delay_ms": args.get("delay_ms")}}
        if cmd == "path_bench":
            args = request.get("args") or {}
            return {"id": rid, "ok": True, "result": {
                "map": "travelerzone", "size": [128, 128], "teleports": 4, "occupants": 2,
                "repeats": args.get("repeats", 3), "fixture": args.get("fixture"),
                "cases": [{"label": "occupied", "start": [10, 10], "goal": [18, 10], "goal_occupied": True}],
                "rows": [{"label": "occupied", "candidate": "astar_hashed", "reach_check": False,
                          "verdict": "not_checked", "first_ms": 240.0, "median_ms": 230.0, "searches": 2,
                          "nodes": 20000, "scanned": 0, "budget_spent": True, "steps": 7, "cost": 70,
                          "end": [17, 10]},
                         {"label": "occupied", "candidate": "jump_point", "reach_check": True,
                          "verdict": "goal_occupied", "first_ms": 0.4, "median_ms": 0.2, "searches": 1,
                          "nodes": 3, "scanned": 40, "budget_spent": False, "steps": 7, "cost": 70,
                          "end": [17, 10]}],
                "mismatches": []}}
        if cmd == "inject_message":
            args = request.get("args") or {}
            return {"id": rid, "ok": True, "result": {"type": args.get("type"),
                                                      "queued": len(args.get("rows") or [None])}}
        if cmd == "click":
            return {"id": rid, "ok": True, "result": {"consumed_by": {"kind": "window", "owner": "inventory"}}}
        if cmd in ("mouse_down", "mouse_up"):
            return {"id": rid, "ok": True, "result": {}}
        if cmd == "probe_point" and (request.get("args") or {}).get("x") == 99:
            return {"id": rid, "ok": True, "result": {
                "logical": [99, 2], "ui_width": 853, "output_size": [192, 108], "window_size": [192, 108],
                "detail": {"player_tile": [5, 6], "front": {"kind": "window", "owner": "inventory"},
                           "covered_by": "inventory"}}}
        if cmd == "probe_point":
            return {"id": rid, "ok": True, "result": {
                "logical": [1, 2], "ui_width": 853, "output_size": [192, 108], "window_size": [192, 108],
                "detail": {"player_tile": [5, 6], "bubble": {"active": False}, "illusion_masks_identity": False,
                           "tile_blocking": {"passable": True, "objects": [], "object_count": 0}},
                "frame_times": {"window_frames": 600, "sections": {"render": {"p50_us": 900}}},
                "draw_stats": {"world_draws": 12, "ui_draws": 34}}}
        if cmd in ("key", "mouse_move", "set_text"):
            return {"id": rid, "ok": True, "result": {}}
        if cmd == "journal_config":
            return {"id": rid, "ok": True, "result": {
                "channels": {"audio.sound": False, "client.warnings": True}, "capacity": 4096}}
        return {"id": rid, "ok": False, "error": "unknown cmd " + str(cmd)}


bridge = fake_bridge(PORT)
bridge.start()

# ---------------------------------------------------------------- MCP over stdio

print("MCP server over stdio")
shutil.rmtree(SHOT_DIR, ignore_errors=True)  # a failed earlier run leaves its captures behind
proc = subprocess.Popen(
    [sys.executable, os.path.join(REPO, ".claude", "tools", "agent_mcp", "agent_mcp.py")],
    stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
    env=dict(os.environ, HB_AGENT_PORT=str(PORT), HB_SCREENSHOT_DIR=SHOT_DIR, HB_DEAD_CLIENT_GRACE_S="0",
             HB_COMMANDS_TTL_S="3600", HB_ENABLE_SDL_FLOWS="1"))


def rpc(method, params=None, rid=[0]):
    rid[0] += 1
    message = {"jsonrpc": "2.0", "id": rid[0], "method": method}
    if params is not None:
        message["params"] = params
    proc.stdin.write((json.dumps(message) + "\n").encode("utf-8"))
    proc.stdin.flush()
    # skip any notification frames to find our answer
    while True:
        line = proc.stdout.readline()
        if not line:
            raise RuntimeError("server exited")
        reply = json.loads(line.decode("utf-8"))
        if reply.get("id") == rid[0]:
            return reply


init = rpc("initialize", {"protocolVersion": "2025-06-18"})
caps = init["result"]["capabilities"]
check("advertises tools", "tools" in caps, caps)
check("advertises logging", "logging" in caps, caps)
check("advertises resources with subscribe",
      caps.get("resources", {}).get("subscribe") is True, caps)

tools = {tool["name"] for tool in rpc("tools/list")["result"]["tools"]}
for expected in ("hb_journal_drain", "hb_journal_config", "hb_wait_for", "hb_stream",
                 "hb_subscribe", "hb_batch", "hb_path_bench", "hb_inject"):
    check("tool {} is listed".format(expected), expected in tools, sorted(tools))

resources = rpc("resources/list")["result"]["resources"]
check("journal resource is listed",
      any(r["uri"] == agent_mcp.JOURNAL_RESOURCE_URI for r in resources), resources)

ping = rpc("tools/call", {"name": "hb_ping", "arguments": {}})
check("hb_ping reaches the fake bridge",
      "0.0.1-alpha:TEST" in ping["result"]["content"][0]["text"],
      ping["result"]["content"][0]["text"])

print("tools follow the client's command list")
check("hb_ping reports the bridge protocol and command count",
      "bridge_protocol 5" in ping["result"]["content"][0]["text"] and
      "{} command(s)".format(len(ALL_COMMANDS)) in ping["result"]["content"][0]["text"], ping["result"]["content"][0]["text"])
bridge.commands = list(PHASE1_COMMANDS)
rpc("tools/call", {"name": "hb_ping", "arguments": {}})  # refreshes the cached commands
narrow = {tool["name"] for tool in rpc("tools/list")["result"]["tools"]}
check("a phase-1 client shows its input tools", {"hb_click", "hb_screenshot", "hb_get_state", "hb_key"} <= narrow, sorted(narrow))
check("tools whose command the client lacks are hidden",
      not narrow & {"hb_widgets", "hb_flow", "hb_journal_drain", "hb_entities", "hb_walk_to", "hb_set_god"}, sorted(narrow))
check("tools with no wire command stay", {"hb_batch", "hb_clients", "hb_parallel"} <= narrow, sorted(narrow))
refused = rpc("tools/call", {"name": "hb_widgets", "arguments": {}})["result"]
check("a hidden tool is refused when called anyway",
      refused.get("isError") and "does not implement" in refused["content"][0]["text"], refused)
check("the refusal sent nothing to the client", not [r for r in bridge.seen if r["cmd"] == "widgets"], bridge.seen[-3:])
bridge.commands = list(ALL_COMMANDS)
rpc("tools/call", {"name": "hb_ping", "arguments": {}})
check("a client that lists more commands shows more tools",
      "hb_widgets" in {tool["name"] for tool in rpc("tools/list")["result"]["tools"]})

for protocol, warns in ((2, False), (3, False), (4, False), (5, False), (6, True)):
    bridge.protocol = protocol
    text = rpc("tools/call", {"name": "hb_ping", "arguments": {}})["result"]["content"][0]["text"]
    check("bridge_protocol {} {}".format(protocol, "is warned about" if warns else "is accepted without a warning"),
          ("WARNING" in text) == warns and "bridge_protocol {}".format(protocol) in text, text)
bridge.protocol = 2
shot_before = len([r for r in bridge.seen if r["cmd"] == "screenshot"])
rpc("tools/call", {"name": "hb_screenshot", "arguments": {"name": "smoke_wire"}})
wire = [r for r in bridge.seen if r["cmd"] == "screenshot"][shot_before:]
check("the screenshot goes over the wire as a bare name, never a path",
      len(wire) == 1 and set(wire[0]["args"]) == {"name"} and os.path.basename(wire[0]["args"]["name"]) == wire[0]["args"]["name"]
      and wire[0]["args"]["name"].endswith(".png"), wire)
check("and the raw capture is gone once processed",
      not [f for f in os.listdir(SHOT_DIR) if f.startswith(agent_mcp.SCREENSHOT_RAW_PREFIX)], os.listdir(SHOT_DIR))

typed = rpc("tools/call", {"name": "hb_set_text", "arguments": {"value": "hunter2-secret"}})["result"]["content"][0]["text"]
check("hb_set_text never echoes the value it typed", "hunter2" not in typed and "14 chars" in typed, typed)
typed = rpc("tools/call", {"name": "hb_text", "arguments": {"value": "hunter2-secret"}})["result"]["content"][0]["text"]
check("hb_text never echoes the value it typed", "hunter2" not in typed, typed)

drain = rpc("tools/call", {"name": "hb_journal_drain", "arguments": {}})
text = drain["result"]["content"][0]["text"]
check("drain renders one line per event", "client.warnings/sound_refused" in text, text)
check("drain shows the real inter-event interval", "1000ms" in text and "1150ms" in text, text)
check("drain is headed as the live view", "live (newest first), now t=1200ms" in text, text)
check("drain puts the NEWEST event at the top",
      text.index("sound_refused") < text.index("sound_play"), text)
check("drain says how to page older", "before_seq=40" in text, text)
sent = [r for r in bridge.seen if r["cmd"] == "journal_query"]
check("drain sends the non-consuming journal_query", bool(sent), [r["cmd"] for r in bridge.seen])

wait = rpc("tools/call", {"name": "hb_wait_for",
                          "arguments": {"conditions": [{"channel": "client.errors"}],
                                        "timeout_ms": 1000}})
text = wait["result"]["content"][0]["text"]
check("a timeout reads as a result, not an error",
      "TIMED OUT" in text and "not an error" in text, text)

print("  -- the satisfied and timed-out payloads differ --")
ev = rpc("tools/call", {"name": "hb_wait_for",
                        "arguments": {"conditions": [{"channel": "client.warnings",
                                                      "type": "sound_refused"}],
                                      "timeout_ms": 1000}})
text = ev["result"]["content"][0]["text"]
check("an event match reports the event that FIRED it",
      "fired by:" in text and "char_footstep_run" in text, text)
check("an event match carries the settle-time state", "state at settle:" in text, text)

st = rpc("tools/call", {"name": "hb_wait_for",
                        "arguments": {"conditions": [{"state": "screen",
                                                      "equals": "Screen_OnGame"}],
                                      "timeout_ms": 1000}})
text = st["result"]["content"][0]["text"]
check("a state match reports the OBSERVED value",
      "observed:" in text and "Screen_OnGame" in text, text)
check("a state match does not claim an event fired", "fired by:" not in text, text)

text = wait["result"]["content"][0]["text"]
check("a timeout still reports how far things got",
      "state at settle:" in text and "Screen_Login" in text, text)

bad = rpc("tools/call", {"name": "hb_wait_for",
                         "arguments": {"conditions": [], "timeout_ms": 1000}})
check("empty conditions is rejected", bad["result"].get("isError") is True, bad)

over = rpc("tools/call", {"name": "hb_wait_for",
                          "arguments": {"conditions": [{"channel": "x"}],
                                        "timeout_ms": agent_mcp.WAIT_FOR_MAX_MS + 1}})
check("an over-long timeout is rejected", over["result"].get("isError") is True, over)

sub = rpc("tools/call", {"name": "hb_subscribe",
                         "arguments": {"channels": ["client.errors", "net.errors"]}})
check("hb_subscribe can set a standing watch on every failure",
      "client.errors" in sub["result"]["content"][0]["text"]
      and "net.errors" in sub["result"]["content"][0]["text"], sub)

badre = rpc("tools/call", {"name": "hb_subscribe", "arguments": {"filter": "([unclosed"}})
check("a bad regex is a named error, not an empty stream",
      badre["result"].get("isError") is True
      and "bad regex" in badre["result"]["content"][0]["text"], badre)

print("  -- batching (the latency fix) --")
batch = rpc("tools/call", {"name": "hb_batch", "arguments": {"steps": [
    {"tool": "hb_ping", "arguments": {}},
    {"tool": "hb_journal_drain", "arguments": {}},
    {"tool": "hb_ping", "arguments": {}},
]}})
text = batch["result"]["content"][0]["text"]
check("a batch runs every step in one call",
      text.count("ok") >= 3 and "batch complete" in text, text)
check("a batch reports its wall time", "step(s) in" in text, text)

nested = rpc("tools/call", {"name": "hb_batch", "arguments": {"steps": [
    {"tool": "hb_batch", "arguments": {"steps": [{"tool": "hb_ping"}]}},
]}})
check("a batch cannot nest", nested["result"].get("isError") is True, nested)

notallowed = rpc("tools/call", {"name": "hb_batch", "arguments": {"steps": [
    {"tool": "hb_stream", "arguments": {"enabled": True}},
]}})
check("session-state tools are refused in a batch",
      notallowed["result"].get("isError") is True, notallowed)

# a bad step LATE in the batch must be caught before ANY step runs
prevalidated = rpc("tools/call", {"name": "hb_batch", "arguments": {"steps": [
    {"tool": "hb_ping", "arguments": {}},
    {"tool": "hb_not_a_tool", "arguments": {}},
]}})
check("a bad step is caught before the batch half-runs",
      prevalidated["result"].get("isError") is True
      and "step 1" in prevalidated["result"]["content"][0]["text"], prevalidated)

stops = rpc("tools/call", {"name": "hb_batch", "arguments": {"steps": [
    {"tool": "hb_ping", "arguments": {}},
    {"tool": "hb_wait_for", "arguments": {"conditions": [], "timeout_ms": 100}},
    {"tool": "hb_ping", "arguments": {}},
]}})
text = stops["result"]["content"][0]["text"]
check("a batch stops at the first failure and says so",
      stops["result"].get("isError") is True and "BATCH FAILED at step 1" in text, text)
check("a stopped batch says the later steps did not run", "NOT run" in text, text)

empty = rpc("tools/call", {"name": "hb_batch", "arguments": {"steps": []}})
check("an empty batch is rejected", empty["result"].get("isError") is True, empty)

toomany = rpc("tools/call", {"name": "hb_batch", "arguments": {
    "steps": [{"tool": "hb_ping", "arguments": {}}] * (agent_mcp.BATCH_MAX_STEPS + 1)}})
check("an over-long batch is rejected", toomany["result"].get("isError") is True, toomany)

print("  -- hb_flow (hb_driver over this server's own connection) --")
check("tool hb_flow is listed", "hb_flow" in tools, sorted(tools))
flow = rpc("tools/call", {"name": "hb_flow", "arguments": {"flow": "wait_screen", "screen": "Screen_Login"}})
text = flow["result"]["content"][0]["text"]
check("hb_flow runs a flow over the server's own bridge connection",
      not flow["result"].get("isError") and "flow 'wait_screen' ok" in text and "Screen_Login" in text, text)
badflow = rpc("tools/call", {"name": "hb_flow", "arguments": {"flow": "no_such_flow"}})
check("an unknown flow is a named error", badflow["result"].get("isError") is True
      and "unknown flow" in badflow["result"]["content"][0]["text"], badflow)
noident = rpc("tools/call", {"name": "hb_flow", "arguments": {"flow": "login"}})
check("a flow missing its identity_file says so", noident["result"].get("isError") is True
      and "identity_file" in noident["result"]["content"][0]["text"], noident)
check("hb_flow is allowed inside hb_batch", "hb_flow" in agent_mcp.BATCH_ALLOWED)

hyphen = rpc("tools/call", {"name": "hb_flow", "arguments": {"flow": "wait-screen", "screen": "Screen_Login"}})
check("a hyphenated flow name is accepted",
      not hyphen["result"].get("isError") and "flow 'wait_screen' ok" in hyphen["result"]["content"][0]["text"], hyphen)
wrapped = rpc("tools/call", {"name": "hb_flow", "arguments": {"flow": "click_tile", "args": {"x": 1, "y": 2}}})
text = wrapped["result"]["content"][0]["text"]
check("an args wrapper is refused, not dropped",
      wrapped["result"].get("isError") is True and "args" in text and "top-level" in text, text)
strayflow = rpc("tools/call", {"name": "hb_flow", "arguments": {"flow": "step", "direction": "n", "x": 3}})
check("a key from another flow is refused", strayflow["result"].get("isError") is True
      and "does not take: x" in strayflow["result"]["content"][0]["text"], strayflow)
noxy = rpc("tools/call", {"name": "hb_flow", "arguments": {"flow": "click_tile", "x": 3}})
check("a missing required key is named", noxy["result"].get("isError") is True
      and "needs y" in noxy["result"]["content"][0]["text"], noxy)

print("  -- hb_flow and screenshot images inside hb_batch (HB-0119) --")
flowbatch = rpc("tools/call", {"name": "hb_batch", "arguments": {"steps": [
    {"tool": "hb_flow", "arguments": {"flow": "wait-screen", "screen": "Screen_Login"}},
    {"tool": "hb_screenshot", "arguments": {"name": "smoke_batch"}},
]}})
content = flowbatch["result"]["content"]
check("hb_flow runs as a batch step", not flowbatch["result"].get("isError") and "flow 'wait_screen' ok" in content[0]["text"],
      content[0]["text"])
check("a screenshot step returns its image", [block["type"] for block in content] == ["text", "image"]
      and content[1]["mimeType"] == "image/png" and len(content[1]["data"]) > 100, [b["type"] for b in content])
check("the step says the image is attached", "[image 1 attached below]" in content[0]["text"], content[0]["text"])

badflowbatch = rpc("tools/call", {"name": "hb_batch", "arguments": {"steps": [
    {"tool": "hb_ping", "arguments": {}},
    {"tool": "hb_flow", "arguments": {"flow": "click_tile", "args": {"x": 1}}},
]}})
check("a bad hb_flow step is caught before the batch runs", badflowbatch["result"].get("isError") is True
      and "step 1" in badflowbatch["result"]["content"][0]["text"]
      and "BATCH FAILED" not in badflowbatch["result"]["content"][0]["text"], badflowbatch)

shots = rpc("tools/call", {"name": "hb_batch", "arguments": {"steps": [
    {"tool": "hb_screenshot", "arguments": {"name": "smoke_cap_{}".format(n)}}
    for n in range(agent_mcp.BATCH_MAX_IMAGES + 1)]}})
content = shots["result"]["content"]
check("a batch returns at most {} images".format(agent_mcp.BATCH_MAX_IMAGES),
      sum(1 for block in content if block["type"] == "image") == agent_mcp.BATCH_MAX_IMAGES
      and "image NOT attached" in content[0]["text"], content[0]["text"])

print("  -- hb_screenshot options --")
plain = rpc("tools/call", {"name": "hb_screenshot", "arguments": {"name": "smoke_plain"}})
content = plain["result"]["content"]
check("a plain capture is a PNG image plus a saved: path",
      content[0]["type"] == "image" and content[0]["mimeType"] == "image/png"
      and content[1]["text"].startswith("saved: ") and content[1]["text"].endswith("smoke_plain.png"), content[-1])
try:
    from PIL import Image as pil_image
except ImportError:
    pil_image = None
if pil_image is None:
    check("scale without Pillow is a clear error (server)", True)
else:
    scaled = rpc("tools/call", {"name": "hb_screenshot", "arguments": {
        "name": "smoke_scaled", "region": {"x": 0, "y": 0, "w": 96, "h": 54}, "scale": 0.5, "format": "jpg"}})
    content = scaled["result"]["content"]
    saved = content[1]["text"].split("saved: ")[1].split(" [")[0]
    with pil_image.open(saved) as picture:
        size, picture_format = picture.size, picture.format
    check("region + scale + jpg crops, shrinks and re-encodes", not scaled["result"].get("isError")
          and size == (48, 27) and picture_format == "JPEG" and content[0]["mimeType"] == "image/jpeg"
          and "192x108 -> 48x27 jpg" in content[1]["text"], (size, picture_format, content[1]["text"]))
    check("the raw capture is removed", not any(name.startswith(agent_mcp.SCREENSHOT_RAW_PREFIX)
                                                for name in os.listdir(SHOT_DIR)), os.listdir(SHOT_DIR))
    explicit = rpc("tools/call", {"name": "hb_screenshot", "arguments": {
        "path": "sub/exact.png", "scale": 0.25, "inline": False}})
    content = explicit["result"]["content"]
    check("path is honoured exactly and inline:false returns text only",
          len(content) == 1 and os.path.isfile(os.path.join(SHOT_DIR, "sub", "exact.png"))
          and "inline false" in content[0]["text"], content)
    outside = rpc("tools/call", {"name": "hb_screenshot", "arguments": {
        "path": os.path.join(os.path.dirname(REPO), "outside_the_repo.png")}})
    check("a path outside the repo is refused", outside["result"].get("isError") is True
          and "inside the repo" in outside["result"]["content"][0]["text"], outside)
    toobig = rpc("tools/call", {"name": "hb_screenshot", "arguments": {"region": {"x": 150, "y": 0, "w": 100, "h": 10}}})
    check("a region past the capture is a named error", toobig["result"].get("isError") is True
          and "lies outside the 192x108" in toobig["result"]["content"][0]["text"], toobig)
badscale = rpc("tools/call", {"name": "hb_screenshot", "arguments": {"scale": 2}})
check("an upscale is refused", badscale["result"].get("isError") is True, badscale)

print("  -- hb_probe is compact; hb_wait_for trims its trail; strict journal keys --")
probe = rpc("tools/call", {"name": "hb_probe", "arguments": {"x": 1, "y": 2}})
text = probe["result"]["content"][0]["text"]
check("a probe omits the frame-time block and is one line",
      "frame_times" not in text and "player_tile" in text and text.count("\n") <= 1, text)
check("a probe also leaves out the verbose detail blocks, and says so",
      '"bubble"' not in text and "illusion_masks" not in text and '"objects"' not in text
      and "left out bubble" in text, text)
probe = rpc("tools/call", {"name": "hb_probe", "arguments": {"x": 1, "y": 2, "frame_times": True}})
check("frame_times:true brings it back", "frame_times" in probe["result"]["content"][0]["text"], probe)
probe = rpc("tools/call", {"name": "hb_probe", "arguments": {"x": 1, "y": 2, "full": True}})
check("full:true brings the verbose blocks back", '"bubble"' in probe["result"]["content"][0]["text"], probe)

trail = rpc("tools/call", {"name": "hb_wait_for", "arguments": {
    "conditions": [{"channel": "state.player"}], "timeout_ms": 1000, "since": 0}})
text = trail["result"]["content"][0]["text"]
check("hb_wait_for lists a short trail by default", text.count("state.player/tick") == 10
      and "the last 10 of 30" in text, text)
full = rpc("tools/call", {"name": "hb_wait_for", "arguments": {
    "conditions": [{"channel": "state.player"}], "timeout_ms": 1000, "trail": True}})
check("trail:true lists them all", full["result"]["content"][0]["text"].count("state.player/tick") == 30)
none = rpc("tools/call", {"name": "hb_wait_for", "arguments": {
    "conditions": [{"channel": "state.player"}], "timeout_ms": 1000, "trail": False}})
check("trail:false lists none", "state.player/tick" not in none["result"]["content"][0]["text"])
badtrail = rpc("tools/call", {"name": "hb_wait_for", "arguments": {
    "conditions": [{"channel": "state.player"}], "timeout_ms": 1000, "trail": "lots"}})
check("a bad trail is an error", badtrail["result"].get("isError") is True, badtrail)

stray = rpc("tools/call", {"name": "hb_journal_drain", "arguments": {"entity": 7}})
check("hb_journal_drain refuses an unknown key", stray["result"].get("isError") is True
      and "entity" in stray["result"]["content"][0]["text"], stray)
strict_batch = rpc("tools/call", {"name": "hb_batch", "arguments": {"steps": [
    {"tool": "hb_ping", "arguments": {}}, {"tool": "hb_journal_drain", "arguments": {"count": 5}}]}})
check("an unknown journal key in a batch is caught up front", strict_batch["result"].get("isError") is True
      and "'count' -> 'max'" in strict_batch["result"]["content"][0]["text"], strict_batch)

covered = rpc("tools/call", {"name": "hb_probe", "arguments": {"x": 99, "y": 2}})
text = covered["result"]["content"][0]["text"]
check("a covered probe point says what takes the click", "COVERED: a click here reaches window inventory" in text, text)

click = rpc("tools/call", {"name": "hb_click", "arguments": {"x": 5, "y": 6}})
check("hb_click prints what consumed the click",
      click["result"]["content"][0]["text"].endswith("-> consumed by window inventory"), click)
down = rpc("tools/call", {"name": "hb_mouse_down", "arguments": {"x": 5, "y": 6}})
check("a reply without consumed_by prints nothing extra (an older client)",
      "consumed" not in down["result"]["content"][0]["text"], down)
check("format_consumed degrades on a missing or malformed field",
      agent_mcp.format_consumed({}) == "" and agent_mcp.format_consumed({"consumed_by": "x"}) == ""
      and agent_mcp.format_consumed(None) == "")
config = rpc("tools/call", {"name": "hb_batch", "arguments": {"steps": [
    {"tool": "hb_journal_config", "arguments": {}}]}})
check("hb_journal_config is a batch step", not config["result"].get("isError"), config)

print("  -- world tools (client batch 2) --")
entities = rpc("tools/call", {"name": "hb_entities", "arguments": {"kinds": ["npc", "creature", "item"]}})
text = entities["result"]["content"][0]["text"]
check("hb_entities is one compact line per entity, nearest first", not entities["result"].get("isError")
      and text.splitlines()[0].startswith("player (134,37) - 3 of 3 entities") and len(text.splitlines()) == 4, text)
check("an on-screen row names its tile, distance and window point",
      "#19 npc gandalf (136,36) d2 at window (1060,480)" in text, text)
check("an off-screen row says so", "#44 creature slime (150,40) d16 OFF SCREEN" in text, text)
check("an item row carries its item id", "#item 115 item dagger (134,38) d1" in text, text)
wire = [r for r in bridge.seen if r["cmd"] == "entities"][-1]["args"]
check("the harness caps rows by default and passes only the client's keys",
      wire == {"max": agent_mcp.ENTITIES_DEFAULT_MAX, "kinds": ["npc", "creature", "item"]}, wire)
check("raw:true returns the client's JSON",
      '"entities"' in rpc("tools/call", {"name": "hb_entities", "arguments": {"raw": True}})["result"]["content"][0]["text"])
for bad in ({"kinds": ["dragon"]}, {"kinds": ["object"]}, {"radius_tiles": 5}):
    badkind = rpc("tools/call", {"name": "hb_entities", "arguments": bad})
    check("hb_entities refuses {}".format(bad), badkind["result"].get("isError") is True, badkind)

seen_before = len(bridge.seen)
clicked = rpc("tools/call", {"name": "hb_click_entity", "arguments": {"object_id": 19}})
text = clicked["result"]["content"][0]["text"]
sent_click = [r for r in bridge.seen[seen_before:] if r["cmd"] == "click"]
check("hb_click_entity clicks the entity's WINDOW point with a button and no SDL-only key",
      not clicked["result"].get("isError") and sent_click and sent_click[0]["args"]["space"] == "window"
      and (sent_click[0]["args"]["x"], sent_click[0]["args"]["y"]) == (1060, 480)
      and sent_click[0]["args"]["button"] == "left" and "right" not in sent_click[0]["args"]
      and "consumed by window inventory" in text, (text, sent_click))
seen_before = len(bridge.seen)
blocked = rpc("tools/call", {"name": "hb_click_entity", "arguments": {"object_id": 44}})
text = blocked["result"]["content"][0]["text"]
check("hb_click_entity refuses an off-screen entity and sends no click", blocked["result"].get("isError") is True
      and "off screen" in text and not [r for r in bridge.seen[seen_before:] if r["cmd"] == "click"], text)
item = rpc("tools/call", {"name": "hb_click_entity", "arguments": {"object_id": 7001}})
check("an item is not clicked by entity", item["result"].get("isError") is True
      and "no entity with object_id 7001" in item["result"]["content"][0]["text"], item)
missing = rpc("tools/call", {"name": "hb_click_entity", "arguments": {"object_id": 5}})
check("an unknown object_id is a named error", missing["result"].get("isError") is True
      and "no entity with object_id 5" in missing["result"]["content"][0]["text"], missing)

tts = rpc("tools/call", {"name": "hb_tile_to_screen", "arguments": {"x": 140, "y": 38}})
text = tts["result"]["content"][0]["text"]
check("hb_tile_to_screen is one line with the pixel and the round trip",
      text.startswith("tile (140,38) -> window (1034,540) logical (460,240), on screen, a click there resolves to (140,38)")
      and "\n" not in text, text)
off = rpc("tools/call", {"name": "hb_tile_to_screen", "arguments": {"x": 999, "y": 5}})
check("a tile outside the map says so", "OUTSIDE the map" in off["result"]["content"][0]["text"], off)

seen_before = len(bridge.seen)
walk = rpc("tools/call", {"name": "hb_walk_to", "arguments": {"x": 140, "y": 38}})
text = walk["result"]["content"][0]["text"]
wire = [r["args"] for r in bridge.seen[seen_before:] if r["cmd"] == "walk_to"][-1]
check("hb_walk_to returns at the start by default, sending wait false and no timeout",
      "walk_to (140,38) from (134,37): walking" in text and wire == {"x": 140, "y": 38, "wait": False}, (text, wire))
walked = rpc("tools/call", {"name": "hb_walk_to", "arguments": {"x": 140, "y": 38, "wait": True, "timeout_ms": 3000}})
text = walked["result"]["content"][0]["text"]
wire = [r["args"] for r in bridge.seen if r["cmd"] == "walk_to"][-1]
check("hb_walk_to wait:true sends wait and timeout_ms and reports the arrival",
      wire == {"x": 140, "y": 38, "wait": True, "timeout_ms": 3000} and "arrived at (140,38)" in text
      and "after 1200 ms" in text, (text, wire))
stopped = rpc("tools/call", {"name": "hb_walk_to", "arguments": {"x": 77, "y": 5, "wait": True}})
check("a blocked walk is a result with its reason, not an error", not stopped["result"].get("isError")
      and "blocked at (76,5)" in stopped["result"]["content"][0]["text"]
      and "no step led nearer" in stopped["result"]["content"][0]["text"], stopped)
timed = rpc("tools/call", {"name": "hb_walk_to", "arguments": {"x": 88, "y": 5, "wait": True, "timeout_ms": 500}})
check("a walk that times out says so", ": timeout" in timed["result"]["content"][0]["text"], timed)
refused = rpc("tools/call", {"name": "hb_walk_to", "arguments": {"x": 99, "y": 9, "wait": True}})
check("a refused walk_to is an error carrying the client's reason", refused["result"].get("isError") is True
      and "walk_to refused" in refused["result"]["content"][0]["text"], refused)
nocoord = rpc("tools/call", {"name": "hb_walk_to", "arguments": {"x": 1}})
check("hb_walk_to without y is refused", nocoord["result"].get("isError") is True, nocoord)
sdl_only = rpc("tools/call", {"name": "hb_walk_to", "arguments": {"x": 1, "y": 2, "cancel_on_timeout": False}})
check("the SDL-only cancel_on_timeout is refused", sdl_only["result"].get("isError") is True
      and "cancel_on_timeout" in sdl_only["result"]["content"][0]["text"], sdl_only)

day = rpc("tools/call", {"name": "hb_set_time_of_day", "arguments": {"t01": 0.75}})
check("hb_set_time_of_day sets the override", "override 0.75" in day["result"]["content"][0]["text"], day)
off_day = rpc("tools/call", {"name": "hb_set_time_of_day", "arguments": {"t01": "off"}})
check("'off' returns to the server clock", "OFF" in off_day["result"]["content"][0]["text"], off_day)
bad_day = rpc("tools/call", {"name": "hb_set_time_of_day", "arguments": {"t01": 2}})
check("an out-of-range time is refused before the wire", bad_day["result"].get("isError") is True, bad_day)

god_on = rpc("tools/call", {"name": "hb_set_god", "arguments": {"on": True}})
wire = [r for r in bridge.seen if r["cmd"] == "set_god"][-1]["args"]
check("hb_set_god sends {on: true} and reports the flags", wire == {"on": True}
      and "god ON" in god_on["result"]["content"][0]["text"] and "0x03" in god_on["result"]["content"][0]["text"], god_on)
god_off = rpc("tools/call", {"name": "hb_set_god", "arguments": {"on": False}})
check("hb_set_god off reports god off", "god off" in god_off["result"]["content"][0]["text"], god_off)
for bad in ({}, {"on": "yes"}, {"on": True, "extra": 1}):
    refused_god = rpc("tools/call", {"name": "hb_set_god", "arguments": bad})
    check("hb_set_god refuses {}".format(bad), refused_god["result"].get("isError") is True, refused_god)

tags_off = rpc("tools/call", {"name": "hb_set_staff_tags", "arguments": {"visible": False}})
tags_wire = [r for r in bridge.seen if r["cmd"] == "set_staff_tags"][-1]["args"]
check("hb_set_staff_tags sends {visible: false} and reports hidden", tags_wire == {"visible": False}
      and "HIDDEN" in tags_off["result"]["content"][0]["text"], tags_off)
for bad in ({}, {"visible": "no"}):
    refused_tags = rpc("tools/call", {"name": "hb_set_staff_tags", "arguments": bad})
    check("hb_set_staff_tags refuses {}".format(bad), refused_tags["result"].get("isError") is True, refused_tags)

closed = rpc("tools/call", {"name": "hb_close_window", "arguments": {"name": "inventory"}})
check("hb_close_window sends the name and reports what closed", [r for r in bridge.seen if r["cmd"] == "close_window"][-1]["args"] == {"name": "inventory"}
      and "closed: inventory (#2)" in closed["result"]["content"][0]["text"], closed)
nothing = rpc("tools/call", {"name": "hb_close_window", "arguments": {"name": "all"}})
check("nothing closing is a plain result", not nothing["result"].get("isError") and "nothing closed" in nothing["result"]["content"][0]["text"], nothing)
by_id = rpc("tools/call", {"name": "hb_close_window", "arguments": {"dialog_id": 6}})
check("hb_close_window takes a dialog_id", [r for r in bridge.seen if r["cmd"] == "close_window"][-1]["args"] == {"dialog_id": 6}, by_id)
for bad in ({}, {"name": "inventory", "dialog_id": 6}):
    refused_close = rpc("tools/call", {"name": "hb_close_window", "arguments": bad})
    check("hb_close_window refuses {}".format(bad), refused_close["result"].get("isError") is True, refused_close)

flat_batch = rpc("tools/call", {"name": "hb_batch", "arguments": {"steps": [{"tool": "hb_click", "x": 5, "y": 6}]}})
check("hb_batch refuses flat step fields instead of clicking (0,0)", flat_batch["result"].get("isError") is True
      and "arguments" in flat_batch["result"]["content"][0]["text"], flat_batch)

seen_before = len(bridge.seen)
drag = rpc("tools/call", {"name": "hb_drag_slot", "arguments": {"from": "inventory.slot0", "to_x": 300, "to_y": 200, "steps": 2}})
drag_cmds = [(r["cmd"], r["args"].get("x"), r["args"].get("y")) for r in bridge.seen[seen_before:] if r["cmd"] in ("mouse_down", "mouse_move", "mouse_up")]
check("hb_drag_slot presses on the aim point, moves in legs, releases at the drop",
      drag_cmds == [("mouse_down", 14, 26), ("mouse_move", 157, 113), ("mouse_move", 300, 200), ("mouse_up", 300, 200)], drag_cmds)
empty_drag = rpc("tools/call", {"name": "hb_drag_slot", "arguments": {"from": "inventory.slot1", "to_x": 1, "to_y": 1}})
check("hb_drag_slot refuses an empty slot", empty_drag["result"].get("isError") is True, empty_drag)

seen_before = len(bridge.seen)
chat = rpc("tools/call", {"name": "hb_send_chat", "arguments": {"value": "/who", "open": True}})
chat_cmds = [(r["cmd"], (r["args"] or {}).get("name") or (r["args"] or {}).get("value")) for r in bridge.seen[seen_before:] if r["cmd"] in ("key", "set_text")]
check("hb_send_chat opens, clears and types, then submits", chat_cmds == [("key", "enter"), ("set_text", "/who"), ("key", "enter")], chat_cmds)
check("hb_send_chat sends clear as the boolean true (the bridge's contract), not a count",
      [r for r in bridge.seen[seen_before:] if r["cmd"] == "set_text"][-1]["args"].get("clear") is True, chat)

auto = rpc("tools/call", {"name": "hb_fishing_auto_strike", "arguments": {"on": "pull", "delay_ms": 120}})
auto_wire = [r for r in bridge.seen if r["cmd"] == "fishing_auto_strike"][-1]["args"]
check("hb_fishing_auto_strike sends {on, delay_ms} and reports them", auto_wire == {"on": "pull", "delay_ms": 120}
      and "auto strike pull (delay 120 ms)" in auto["result"]["content"][0]["text"], auto)
for bad in ({}, {"on": "now"}, {"on": "pull", "delay_ms": 9000}, {"on": "off", "extra": 1}):
    refused_auto = rpc("tools/call", {"name": "hb_fishing_auto_strike", "arguments": bad})
    check("hb_fishing_auto_strike refuses {}".format(bad), refused_auto["result"].get("isError") is True, refused_auto)

bench = rpc("tools/call", {"name": "hb_path_bench", "arguments": {
    "fixture": ".claude/scratch/client/smoke.fixture", "repeats": 2}})
bench_wire = [r for r in bridge.seen if r["cmd"] == "path_bench"][-1]["args"]
check("hb_path_bench tabulates the rows and reports the correctness check", not bench["result"].get("isError")
      and "jump_point" in bench["result"]["content"][0]["text"]
      and "correctness: every candidate matches" in bench["result"]["content"][0]["text"], bench)
check("hb_path_bench sends the fixture as an absolute path and the repeats", os.path.isabs(bench_wire.get("fixture", ""))
      and bench_wire.get("repeats") == 2, bench_wire)
outside = rpc("tools/call", {"name": "hb_path_bench", "arguments": {"fixture": "C:/Windows/Temp/x.fixture"}})
check("a fixture outside the repo is refused before the wire", outside["result"].get("isError") is True, outside)
many = rpc("tools/call", {"name": "hb_path_bench", "arguments": {"repeats": 99}})
check("hb_path_bench repeats past the client's cap is refused", many["result"].get("isError") is True, many)
injected = rpc("tools/call", {"name": "hb_inject", "arguments": {
    "type": "script_strings", "rows": [{"id": 1, "text": "hello"}]}})
inject_wire = [r for r in bridge.seen if r["cmd"] == "inject_message"][-1]["args"]
check("hb_inject passes its arguments straight through and reports what was queued",
      not injected["result"].get("isError") and "1 message(s) queued" in injected["result"]["content"][0]["text"]
      and inject_wire == {"type": "script_strings", "rows": [{"id": 1, "text": "hello"}]}, (injected, inject_wire))
raw_injected = rpc("tools/call", {"name": "hb_inject", "arguments": {"type": "raw", "id": 512, "payload": "0000ff"}})
raw_wire = [r for r in bridge.seen if r["cmd"] == "inject_message"][-1]["args"]
check("hb_inject raw passes its id and hex payload straight through",
      not raw_injected["result"].get("isError") and raw_wire == {"type": "raw", "id": 512, "payload": "0000ff"},
      (raw_injected, raw_wire))
bad_inject = rpc("tools/call", {"name": "hb_inject", "arguments": {"type": "nonsense"}})
check("hb_inject with an unknown type is refused before the wire", bad_inject["result"].get("isError") is True,
      bad_inject)

look = rpc("tools/call", {"name": "hb_batch", "arguments": {"steps": [
    {"tool": "hb_entities", "arguments": {"max": 2}},
    {"tool": "hb_tile_to_screen", "arguments": {"x": 1, "y": 2}},
    {"tool": "hb_click_entity", "arguments": {"object_id": 19}},
    {"tool": "hb_walk_to", "arguments": {"x": 3, "y": 4, "wait": True}},
    {"tool": "hb_set_time_of_day", "arguments": {"t01": 0.5}}]}})
check("every world tool is a batch step", not look["result"].get("isError")
      and "step 4 [hb_set_time_of_day]: ok" in look["result"]["content"][0]["text"], look["result"]["content"][0]["text"])
badstep = rpc("tools/call", {"name": "hb_batch", "arguments": {"steps": [
    {"tool": "hb_ping", "arguments": {}}, {"tool": "hb_walk_to", "arguments": {"x": 1, "tile": 2}}]}})
check("a bad world-tool step is caught before the batch runs", badstep["result"].get("isError") is True
      and "BATCH FAILED" not in badstep["result"]["content"][0]["text"], badstep)

filtered = rpc("tools/call", {"name": "hb_journal_drain", "arguments": {"object_id": 19, "message_contains": "hello"}})
wire = [r for r in bridge.seen if r["cmd"] == "journal_query"][-1]["args"]
check("hb_journal_drain passes object_id and message_contains to the client",
      not filtered["result"].get("isError") and wire.get("object_id") == 19 and wire.get("message_contains") == "hello", wire)

print("widgets and flows in this client's own shape")
bridge.native_widgets = True
listing = rpc("tools/call", {"name": "hb_widgets", "arguments": {}})["result"]["content"][0]["text"]
check("hb_widgets lists this client's widgets by name with their rect, and a masked field only as masked",
      listing.splitlines()[0].startswith("Screen_Login - 3 widget(s)") and "login.name" in listing and "300,200 160x20" in listing
      and "login.password" in listing and "= (masked)" in listing and "= 4 chars" in listing, listing)
filtered = rpc("tools/call", {"name": "hb_widgets", "arguments": {"prefix": "login.o"}})["result"]["content"][0]["text"]
check("hb_widgets filters by name prefix", "login.ok" in filtered and "login.name" not in filtered, filtered)
seen_before = len(bridge.seen)
clicked = rpc("tools/call", {"name": "hb_click_widget", "arguments": {"id": "login.ok"}})
wire = [r["args"] for r in bridge.seen[seen_before:] if r["cmd"] == "widget_click"][-1]
check("hb_click_widget sends the widget as 'name', never 'id', and no SDL-only key",
      wire == {"name": "login.ok", "button": "left", "shift": False, "ctrl": False}
      and "left-clicked widget 'login.ok'" in clicked["result"]["content"][0]["text"], (wire, clicked))
for bad in ({"flow": "create_account"}, {"flow": "logout"}, {"flow": "mute"}, {"flow": "login", "password": "x"},
            {"flow": "login", "identity": "x"}, {"flow": "enter", "slot": 1, "args": {}}):
    refused_flow = rpc("tools/call", {"name": "hb_flow", "arguments": bad})["result"]
    check("hb_flow refuses {}".format(bad), refused_flow.get("isError") is True, refused_flow)
no_credentials = rpc("tools/call", {"name": "hb_flow", "arguments": {"flow": "login"}})["result"]
check("hb_flow login without credentials says what it needs and never asks for a password argument",
      no_credentials.get("isError") is True and "test_account" in no_credentials["content"][0]["text"], no_credentials)
bridge.native_widgets = False

print("journal tools match the client's wire keys (bridge_protocol 4)")
journal_before = len(bridge.seen)
rpc("tools/call", {"name": "hb_journal_drain", "arguments": {
    "channels": ["ui.screen"], "types": ["screen_changed"], "exclude_types": ["screen_active"], "max": 50,
    "after_seq": 7, "before_seq": 90, "from_ms": 5, "to_ms": 900, "object_id": 3, "message_contains": "x"}})
wire = [r for r in bridge.seen[journal_before:] if r["cmd"] == "journal_query"][-1]["args"]
check("hb_journal_drain sends the selector flat to journal_query, with the window keys",
      wire == {"channels": ["ui.screen"], "types": ["screen_changed"], "exclude_types": ["screen_active"], "max": 50,
               "after_seq": 7, "before_seq": 90, "from_ms": 5, "to_ms": 900, "object_id": 3, "message_contains": "x"}, wire)
combat = rpc("tools/call", {"name": "hb_journal_drain", "arguments": {"combat": True}})["result"]
check("the SDL-only combat view is gone (an unknown key is refused)", combat.get("isError") and "combat" in combat["content"][0]["text"], combat)
described = {tool["name"]: tool["description"] for tool in agent_mcp.TOOLS}
check("the journal tool texts name this client's channels and none of SDL's",
      all("ui.screen" in described[name] and "client.errors" in described[name] for name in ("hb_journal_drain", "hb_subscribe"))
      and not any(word in described[name] for name in ("hb_journal_drain", "hb_subscribe", "hb_journal_config")
                  for word in ("visual.damage", "audio.sound", "state.entity", "perf.frame")),
      [described[name][:80] for name in ("hb_journal_drain", "hb_subscribe")])
check("the error tap watches client.errors only", agent_mcp.TAP_CHANNELS == ("client.errors",), agent_mcp.TAP_CHANNELS)

seen_before = len(bridge.seen)
cfg = rpc("tools/call", {"name": "hb_journal_config", "arguments": {"channels": {"net": False}, "capacity": 1000, "test_log": "warn"}})["result"]
wire = [r for r in bridge.seen[seen_before:] if r["cmd"] == "journal_config"][-1]["args"]
check("hb_journal_config passes channels, capacity and test_log through",
      not cfg.get("isError") and wire == {"channels": {"net": False}, "capacity": 1000, "test_log": "warn"}, (cfg, wire))
seen_before = len(bridge.seen)
bad_log = rpc("tools/call", {"name": "hb_journal_config", "arguments": {"test_log": "info"}})["result"]
check("a test_log other than warn / error is refused before the wire",
      bad_log.get("isError") and not [r for r in bridge.seen[seen_before:] if r["cmd"] == "journal_config"], bad_log)

seen_before = len(bridge.seen)
zero = rpc("tools/call", {"name": "hb_wait_for", "arguments": {"conditions": [{"channel": "chat.message", "type": "chat_line",
                          "match": {"text": "hi"}}], "timeout_ms": 0, "since": 12}})["result"]
wire = [r["args"] for r in bridge.seen[seen_before:] if r["cmd"] == "wait_for" and r["args"]["conditions"][0]["channel"] == "chat.message"][-1]
check("hb_wait_for takes timeout_ms 0 with an event condition, and sends since",
      not zero.get("isError") and wire["timeout_ms"] == 0 and wire["since"] == 12
      and wire["conditions"][0]["match"] == {"text": "hi"}, (zero, wire))
seen_before = len(bridge.seen)
zero_state = rpc("tools/call", {"name": "hb_wait_for", "arguments": {
    "conditions": [{"state": "screen", "equals": "Screen_Login"}], "timeout_ms": 0}})["result"]
check("timeout_ms 0 stays refused for a state condition, before the wire",
      zero_state.get("isError") and not [r for r in bridge.seen[seen_before:] if r["cmd"] == "wait_for"
                                       and r["args"]["conditions"][0].get("state")], zero_state)
cond = {"channel": "chat.message", "message_contains": "hello", "object_id": 19}
rpc("tools/call", {"name": "hb_wait_for", "arguments": {"conditions": [cond], "timeout_ms": 500}})
wire = [r["args"] for r in bridge.seen if r["cmd"] == "wait_for"
        and r["args"]["conditions"][0].get("channel") == "chat.message"][-1]
check("a wait_for condition's object_id / message_contains reach the client untouched",
      wire["conditions"] == [cond], wire)

audio_read = rpc("tools/call", {"name": "hb_set_audio", "arguments": {}})
text = audio_read["result"]["content"][0]["text"]
wire = [r for r in bridge.seen if r["cmd"] == "set_audio"][-1]["args"]
check("hb_set_audio with no arguments reads back", wire == {} and not audio_read["result"].get("isError")
      and text.startswith("audio is: master 100, music 80, effects 70"), (wire, text))
audio_set = rpc("tools/call", {"name": "hb_set_audio", "arguments": {"master": 40, "music": 30}})
text = audio_set["result"]["content"][0]["text"]
wire = [r for r in bridge.seen if r["cmd"] == "set_audio"][-1]["args"]
check("hb_set_audio sends only the volumes given and says the change is live only",
      wire == {"master": 40, "music": 30} and "audio now: master 40, music 30" in text and "live only" in text, (wire, text))
for bad in ({"master": 101}, {"music": "loud"}, {"master_muted": True}, {"persist": True}, {"volume": 5}):
    refused_audio = rpc("tools/call", {"name": "hb_set_audio", "arguments": bad})
    check("hb_set_audio refuses {}".format(bad), refused_audio["result"].get("isError") is True, refused_audio)
batched = rpc("tools/call", {"name": "hb_batch", "arguments": {"steps": [
    {"tool": "hb_set_audio", "arguments": {"master": 20}}, {"tool": "hb_get_state", "arguments": {}}]}})
check("hb_set_audio is a batch step", not batched["result"].get("isError")
      and "step 0 [hb_set_audio]: ok" in batched["result"]["content"][0]["text"], batched["result"]["content"][0]["text"])

state = rpc("tools/call", {"name": "hb_get_state", "arguments": {}})
text = state["result"]["content"][0]["text"]
check("hb_get_state is compact by default (HB-0625): scalars packed, audio one line, memory left out",
      "screen=Screen_Login" in text and "audio: master MUTED, volumes master 80 music 50 effects 70" in text
      and "(memory left out" in text and "working set" not in text and "{" not in text, text)
state = rpc("tools/call", {"name": "hb_get_state", "arguments": {"verbose": True}})
text = state["result"]["content"][0]["text"]
check("hb_get_state verbose folds audio and memory into one line each",
      "audio: master MUTED, volumes master 80 music 50 effects 70" in text
      and "memory: working set 512.0 MB (peak 585.9), commit 585.9 MB; clip cache 12.0 / 64.0 MB budget (120 entries)" in text
      and '"master_muted"' not in text and '"working_set_kb"' not in text, text)
raw_state = rpc("tools/call", {"name": "hb_get_state", "arguments": {"raw": True}})
check("raw:true gives the client's whole JSON", '"working_set_kb": 524288' in raw_state["result"]["content"][0]["text"],
      raw_state)

widgets = rpc("tools/call", {"name": "hb_widgets", "arguments": {}})
text = widgets["result"]["content"][0]["text"]
check("hb_widgets shows an item slot, an empty slot, a hidden slot, a slider and a dropdown on one line each",
      "slot 0 item #115 x2" in text and "slot 1 empty" in text
      and "[HIDDEN - scrolled out of view" in text and "track (100,64 120x4) thumb (196,60 8x12)" in text
      and "options [English | Deutsch] selected 0" in text, text)
flow_schema = next(t for t in agent_mcp.TOOLS if t["name"] == "hb_flow")["inputSchema"]["properties"]["flow"]["description"]
from hb_driver import mcp_flows  # noqa: E402
check("hb_flow's schema lists every valid flow (HB-0696)", all(name in flow_schema for name in mcp_flows.flow_names),
      flow_schema)
by_prefix = rpc("tools/call", {"name": "hb_widgets", "arguments": {"prefix": "inventory.slot3"}})
text = by_prefix["result"]["content"][0]["text"]
check("hb_widgets prefix keeps only matching ids and says it filtered (HB-0693)",
      "inventory.slot30" in text and "inventory.slot0" not in text and "filtered from" in text, text)
by_screen = rpc("tools/call", {"name": "hb_widgets", "arguments": {"screen": "nosuchscreen"}})
check("hb_widgets screen filter with no match lists none",
      "0 widget(s)" in by_screen["result"]["content"][0]["text"], by_screen)
terse = rpc("tools/call", {"name": "hb_batch", "arguments": {"terse": True, "steps": [
    {"tool": "hb_widgets", "arguments": {}},
    {"tool": "hb_widgets", "arguments": {"prefix": "inventory.slot0"}, "return": True}]}})
text = terse["result"]["content"][0]["text"]
check("hb_batch terse drops step output unless the step says return:true (HB-0694)",
      "step 0 [hb_widgets]: ok" in text and "slot 1 empty" not in text and "inventory.slot0" in text
      and "batch complete: 2 step(s)" in text, text)
for spelling in ("Enter", "Return", "ESC", "F5"):
    rpc("tools/call", {"name": "hb_key", "arguments": {"name": spelling}})
sent_keys = [r["args"]["name"] for r in bridge.seen if r["cmd"] == "key"][-4:]
check("hb_key maps Enter/Return/ESC to the client's names and passes F-keys (HB-0696)",
      sent_keys == ["enter", "enter", "escape", "f5"], sent_keys)
rpc("tools/call", {"name": "hb_journal_drain", "arguments": {"limit": 7}})
check("hb_journal_drain accepts limit as max (HB-0696)",
      [r for r in bridge.seen if r["cmd"] == "journal_query"][-1]["args"].get("max") == 7)
probe_plain = rpc("tools/call", {"name": "hb_probe", "arguments": {"x": 1, "y": 2}})
probe_stats = rpc("tools/call", {"name": "hb_probe", "arguments": {"x": 1, "y": 2, "draw_stats": True}})
check("hb_probe drops draw_stats unless asked (HB-0698)",
      "draw_stats" not in probe_plain["result"]["content"][0]["text"]
      and "draw_stats" in probe_stats["result"]["content"][0]["text"])
scrolled = rpc("tools/call", {"name": "hb_click_widget", "arguments": {"id": "inventory.slot30"}})
text = scrolled["result"]["content"][0]["text"]
check("clicking a scrolled-out widget is refused with a scroll hint", scrolled["result"].get("isError") is True
      and "scrolled out of view: inventory.slot30" in text and "hb_wheel" in text, text)
slot_click = rpc("tools/call", {"name": "hb_click_widget", "arguments": {"id": "inventory.slot0"}})
check("a visible slot is clicked by its id", not slot_click["result"].get("isError")
      and "inventory.slot0" in slot_click["result"]["content"][0]["text"], slot_click)
walk_flow = rpc("tools/call", {"name": "hb_flow", "arguments": {"flow": "walk_to", "x": 1, "y": 2}})
text = walk_flow["result"]["content"][0]["text"]
check("hb_flow walk_to is gone and the error names the tool", walk_flow["result"].get("isError") is True
      and "hb_walk_to" in text, text)

print("  -- hb_wait_for max_events (HB-0361) --")
many = rpc("tools/call", {"name": "hb_wait_for", "arguments": {
    "conditions": [{"channel": "state.multi"}], "timeout_ms": 1000, "max_events": 4}})
text = many["result"]["content"][0]["text"]
check("max_events collects the first match from the wait and the rest from the journal after it",
      "SATISFIED: 4 of 4 matching event(s)" in text and "#101 " in text and "#104 " in text and "#105" not in text
      and "journal read through seq 105" in text, text)
queried = [r["args"] for r in bridge.seen if r["cmd"] == "journal_query" and (r["args"].get("channels") == ["state.multi"])]
check("the journal is read AFTER the first match (none can be lost between calls)", queried and queried[0]["after_seq"] == 101,
      queried)
filtered_many = rpc("tools/call", {"name": "hb_wait_for", "arguments": {
    "conditions": [{"channel": "state.multi", "match": {"k": 1}}], "timeout_ms": 1000, "max_events": 3}})
text = filtered_many["result"]["content"][0]["text"]
check("a match filter is applied to the journal's events too (only odd seqs: 101, 103, 105)",
      "SATISFIED: 3 of 3" in text and "#101 " in text and "#103 " in text and "#105 " in text and "#102 " not in text, text)
short = rpc("tools/call", {"name": "hb_wait_for", "arguments": {
    "conditions": [{"channel": "state.few"}], "timeout_ms": 600, "max_events": 4}})
text = short["result"]["content"][0]["text"]
check("fewer matches than asked is a timeout result that lists what happened",
      not short["result"].get("isError") and "TIMED OUT with 2 of 4 matching event(s)" in text
      and "#201 " in text and "#202 " in text, text)
none = rpc("tools/call", {"name": "hb_wait_for", "arguments": {
    "conditions": [{"channel": "chat.message", "message_contains": "nothing"}], "timeout_ms": 500, "max_events": 2}})
check("no match at all says 0 of N", "TIMED OUT with 0 of 2" in none["result"]["content"][0]["text"], none)
for bad_arguments, why in (({"conditions": [{"state": "screen", "equals": "x"}], "max_events": 2}, "a state condition"),
                           ({"conditions": [{"channel": "a"}], "max_events": 0}, "zero"),
                           ({"conditions": [{"channel": "a"}], "max_events": 9999}, "too many"),
                           ({"conditions": [{"channel": "a"}], "max_events": "two"}, "a word")):
    refused_many = rpc("tools/call", {"name": "hb_wait_for", "arguments": dict(bad_arguments, timeout_ms=500)})
    check("max_events with {} is refused".format(why), refused_many["result"].get("isError") is True, refused_many)
single = rpc("tools/call", {"name": "hb_wait_for", "arguments": {
    "conditions": [{"channel": "state.multi"}], "timeout_ms": 500, "max_events": 1}})
check("max_events 1 is the ordinary first-hit wait", "SATISFIED by condition 0" in single["result"]["content"][0]["text"], single)
badbatch = rpc("tools/call", {"name": "hb_batch", "arguments": {"steps": [
    {"tool": "hb_wait_for", "arguments": {"conditions": [{"channel": "a"}], "timeout_ms": 100, "max_events": -1}}]}})
check("a bad max_events is caught before a batch runs", badbatch["result"].get("isError") is True, badbatch)

print("  -- hb_cast_at (HB-0361) --")
cast = rpc("tools/call", {"name": "hb_cast_at", "arguments": {"object_id": 19, "spell": "heal"}})
text = cast["result"]["content"][0]["text"]
clicks = [r["args"] for r in bridge.seen if r["cmd"] == "click"][-1]
check("hb_cast_at arms the spell, clicks the entity's window point and reports the cast",
      not cast["result"].get("isError") and "armed heal: spell_cast_begin seen (cast_ms 1280)" in text
      and clicks == {"x": 1060, "y": 480, "space": "window"} and "-> consumed by window inventory" in text
      and "outcome RESOLVED - LANDED on #19 (1 hit event(s))" in text and "spell_release_sent" in text
      and "spell_hit" in text and "spell book was opened for the arm and closed again" in text, text)
check("the book is closed again afterwards (it would cover the aim point)", not bridge.book_open)
check("a client without armed_spell is said to be judged by the journal only", "reports no armed spell" in text, text)
named_cast = rpc("tools/call", {"name": "hb_cast_at", "arguments": {"name": "Gandalf", "spell": "heal"}})
check("hb_cast_at resolves a target by name at call time (HB-0706)",
      not named_cast["result"].get("isError") and "#19" in named_cast["result"]["content"][0]["text"], named_cast)
stale_cast = rpc("tools/call", {"name": "hb_cast_at", "arguments": {"object_id": 9999, "spell": "heal"}})
text = stale_cast["result"]["content"][0]["text"]
check("hb_cast_at on an unknown id names the stale-id cause and the nearest names (HB-0706)",
      stale_cast["result"].get("isError") is True and "go stale" in text and "gandalf" in text, text)
bridge.cast_outcome = "refused"
refused_cast = rpc("tools/call", {"name": "hb_cast_at", "arguments": {"object_id": 19, "spell": "Heal"}})
text = refused_cast["result"]["content"][0]["text"]
check("a refused aim is a result (REFUSED), the label works as the spell name",
      not refused_cast["result"].get("isError") and "outcome REFUSED (server notice)" in text, text)
bridge.cast_outcome = "timeout"
timed_cast = rpc("tools/call", {"name": "hb_cast_at", "arguments": {"object_id": 19, "spell": "heal", "timeout_ms": 700}})
check("no end event is TIMED OUT, named with the wait", "outcome TIMED OUT after 700 ms" in
      timed_cast["result"]["content"][0]["text"], timed_cast)
bridge.cast_outcome = "resolved"
paged = rpc("tools/call", {"name": "hb_cast_at", "arguments": {"object_id": 19, "spell": "energy_bolt"}})
check("a spell on another circle page is found by paging the circle tabs",
      not paged["result"].get("isError") and "armed energy_bolt" in paged["result"]["content"][0]["text"], paged)
unknown_spell = rpc("tools/call", {"name": "hb_cast_at", "arguments": {"object_id": 19, "spell": "fireball"}})
text = unknown_spell["result"]["content"][0]["text"]
check("an unknown spell names the rows seen", unknown_spell["result"].get("isError") is True
      and "no row for 'fireball'" in text and "Heal" in text and "Recall" in text, text)
check("...and the book is closed again after that failure too", not bridge.book_open)
locked = rpc("tools/call", {"name": "hb_cast_at", "arguments": {"object_id": 19, "spell": "recall"}})
check("a disabled (locked) spell row is refused", locked["result"].get("isError") is True
      and "disabled" in locked["result"]["content"][0]["text"], locked)
blocked_cast = rpc("tools/call", {"name": "hb_cast_at", "arguments": {"object_id": 44, "spell": "heal"}})
check("an off-screen entity is refused naming why",
      blocked_cast["result"].get("isError") is True and "is off screen" in blocked_cast["result"]["content"][0]["text"],
      blocked_cast)
nospell = rpc("tools/call", {"name": "hb_cast_at", "arguments": {"object_id": 19}})
check("without spell on a client that does not report the armed one, it says so",
      nospell["result"].get("isError") is True and "does not report the armed spell" in nospell["result"]["content"][0]["text"],
      nospell)
for bad_cast in ({"spell": "heal"}, {"object_id": "x"}, {"object_id": 1, "spell": " "}, {"object_id": 1, "timeout_ms": 0},
                 {"object_id": 1, "target": 2}):
    refused_args = rpc("tools/call", {"name": "hb_cast_at", "arguments": bad_cast})
    check("hb_cast_at refuses {}".format(bad_cast), refused_args["result"].get("isError") is True, refused_args)
check("armed_spell reading: absent, empty, named, nested", agent_mcp.armed_spell({}) == (False, "")
      and agent_mcp.armed_spell({"armed_spell": None}) == (True, "")
      and agent_mcp.armed_spell({"armed_spell": "Heal"}) == (True, "heal")
      and agent_mcp.armed_spell({"spell_armed": {"name": "spell_Energy Bolt"}}) == (True, "energy_bolt"))
inbatch = rpc("tools/call", {"name": "hb_batch", "arguments": {"steps": [
    {"tool": "hb_cast_at", "arguments": {"object_id": 19, "spell": "heal"}}]}})
check("hb_cast_at is a batch step", not inbatch["result"].get("isError")
      and "step 0 [hb_cast_at]: ok" in inbatch["result"]["content"][0]["text"], inbatch["result"]["content"][0]["text"])

print("  -- hb_screenshot burst (HB-0361) --")
burst = rpc("tools/call", {"name": "hb_screenshot", "arguments": {"burst": 6, "interval_ms": 20, "name": "smoke_burst"}})
content = burst["result"]["content"]
files = sorted(name for name in os.listdir(SHOT_DIR) if name.startswith("smoke_burst_"))
check("a burst saves every frame as <name>_01.._06", files == ["smoke_burst_0{}.png".format(n) for n in range(1, 7)], files)
check("a burst returns at most 4 images (first, last, evenly spaced) and says which",
      [block["type"] for block in content].count("image") == 4 and "returning 4 of 6 images: frames 1, 3, 4, 6" in content[0]["text"]
      and "burst of 6: 6 files" in content[0]["text"], content[0]["text"])
quiet = rpc("tools/call", {"name": "hb_screenshot", "arguments": {"burst": 3, "interval_ms": 0, "inline": False, "name": "smoke_quiet"}})
check("inline:false saves the burst and returns no image", len(quiet["result"]["content"]) == 1
      and len([n for n in os.listdir(SHOT_DIR) if n.startswith("smoke_quiet_")]) == 3, quiet)
if pil_image is not None:
    jpg_burst = rpc("tools/call", {"name": "hb_screenshot", "arguments": {
        "burst": 2, "interval_ms": 0, "format": "jpg", "scale": 0.5, "name": "smoke_bjpg"}})
    check("jpg and scale apply to every frame of a burst", [n for n in sorted(os.listdir(SHOT_DIR)) if n.startswith("smoke_bjpg_")]
          == ["smoke_bjpg_01.jpg", "smoke_bjpg_02.jpg"] and jpg_burst["result"]["content"][1]["mimeType"] == "image/jpeg"
          and "192x108 -> 96x54 jpg" in jpg_burst["result"]["content"][0]["text"], jpg_burst)
for bad_burst, why in (({"burst": 99}, "99 frames"), ({"burst": 0}, "0 frames"), ({"burst": "many"}, "a word"),
                       ({"interval_ms": 100}, "an interval without a burst"), ({"burst": 3, "interval_ms": -1}, "a negative gap"),
                       ({"burst": 3, "interval_ms": 99999}, "a huge gap")):
    refused_burst = rpc("tools/call", {"name": "hb_screenshot", "arguments": bad_burst})
    check("burst: {} is refused".format(why), refused_burst["result"].get("isError") is True, refused_burst)
check("pick_burst_images: all when few, first/last/even when many",
      agent_mcp.pick_burst_images(3) == [0, 1, 2] and agent_mcp.pick_burst_images(4) == [0, 1, 2, 3]
      and agent_mcp.pick_burst_images(30) == [0, 10, 19, 29] and agent_mcp.pick_burst_images(5, 1) == [0])


print("  -- HB-0718 tooling passes --")
section_probe = rpc("tools/call", {"name": "hb_probe", "arguments": {"x": 1, "y": 2, "sections": ["render", "nope"]}})
section_text = section_probe["result"]["content"][0]["text"]
check("hb_probe sections implies frame_times, keeps only the named sections and lists unknown ones",
      '"render"' in section_text and "frame_times_unknown_sections" in section_text and "nope" in section_text, section_text)
none_probe = rpc("tools/call", {"name": "hb_probe", "arguments": {"x": 1, "y": 2, "sections": ["other"]}})
check("hb_probe sections drops the sections that were not asked for", '"render"' not in none_probe["result"]["content"][0]["text"].split("frame_times_unknown")[0].split('"sections"')[-1],
      none_probe)
reuse_dir = os.path.join(SHOT_DIR, "reuse_probe")
os.makedirs(reuse_dir, exist_ok=True)
open(os.path.join(reuse_dir, "b_01.png"), "wb").close()
check("burst_paths numbers around existing files, and reuses them when asked to replace",
      agent_mcp.burst_paths(os.path.join(reuse_dir, "b.png"), 2)[0].endswith("b_01_1.png")
      and agent_mcp.burst_paths(os.path.join(reuse_dir, "b.png"), 2, replace=True)[0].endswith("b_01.png"), reuse_dir)
report = agent_mcp.cap_owner_report(SHOT_DIR)
check("the cap refusal names who holds the bytes (subfolders and loose files)",
      "Who holds it:" in report and "reuse_probe/" in report and "(loose files)" in report, report)
shutil.rmtree(reuse_dir, ignore_errors=True)
attempts = []


def flaky():
    attempts.append(1)
    if len(attempts) < 3:
        raise PermissionError("held")
    return "ok"


check("retry_io retries a held file and then succeeds", agent_mcp.retry_io(flaky) == "ok" and len(attempts) == 3, attempts)


class stub_bridge:
    """A bridge that only knows screenshot: writes the fake PNG, for the in-process cap tests."""
    def request(self, cmd, args=None, timeout=None):
        write_png(os.path.join(cap_dir, args["name"]), 192, 108)  # the client writes under the screenshots folder
        return {"ok": True, "result": {"name": args["name"]}}


class stub_target:
    handle = "c1"
    bridge = stub_bridge()

    def address(self):
        return "stub"


cap_dir = os.path.join(REPO, ".claude", "scratch", "agent_mcp_smoke_cap")
shutil.rmtree(cap_dir, ignore_errors=True)
os.makedirs(cap_dir)
frame = os.path.join(cap_dir, "probe.png")
write_png(frame, 192, 108)
frame_bytes = os.path.getsize(frame)
os.remove(frame)
real_cap = os.environ.get("HB_SCREENSHOT_CAP_BYTES")


def burst_in_cap_dir(count, cap):
    os.environ["HB_SCREENSHOT_CAP_BYTES"] = str(cap)
    os.environ["HB_SCREENSHOT_DIR"] = cap_dir  # raw captures are named in screenshot_dir()
    options, problem = agent_mcp.parse_screenshot_options({"burst": count, "interval_ms": 0})
    return agent_mcp.capture_burst(stub_target, options, os.path.join(cap_dir, "capped.png"), cap_dir)


try:
    roomy = burst_in_cap_dir(3, agent_mcp.SCREENSHOT_RAW_HEADROOM_BYTES + frame_bytes * 3 + 100)
    check("a burst that fits under the cap is kept", not roomy.get("isError")
          and len(os.listdir(cap_dir)) == 3 and "now holds" in roomy["content"][0]["text"], roomy)
    for name in os.listdir(cap_dir):
        os.remove(os.path.join(cap_dir, name))
    tight = burst_in_cap_dir(5, agent_mcp.SCREENSHOT_RAW_HEADROOM_BYTES + frame_bytes * 3)
    check("a burst whose frames would not fit is REFUSED after the first frame and keeps nothing",
          tight.get("isError") is True and "REFUSED" in tight["content"][0]["text"] and "would need" in tight["content"][0]["text"]
          and os.listdir(cap_dir) == [], (tight, os.listdir(cap_dir)))
    with open(os.path.join(cap_dir, "old.bin"), "wb") as handle:
        handle.write(b"x" * (frame_bytes * 4))
    full = burst_in_cap_dir(2, agent_mcp.SCREENSHOT_RAW_HEADROOM_BYTES + frame_bytes * 2)
    check("a folder with no room for a raw capture is refused before any capture",
          full.get("isError") is True and "no room for a capture" in full["content"][0]["text"]
          and os.listdir(cap_dir) == ["old.bin"], (full, os.listdir(cap_dir)))
finally:
    os.environ.pop("HB_SCREENSHOT_DIR", None)
    if real_cap is None:
        os.environ.pop("HB_SCREENSHOT_CAP_BYTES", None)
    else:
        os.environ["HB_SCREENSHOT_CAP_BYTES"] = real_cap
    shutil.rmtree(cap_dir, ignore_errors=True)

print("  -- mouse buttons (HB-0625) --")
for tool, cmd in (("hb_click", "click"), ("hb_mouse_down", "mouse_down"), ("hb_mouse_up", "mouse_up")):
    pressed = rpc("tools/call", {"name": tool, "arguments": {"x": 10, "y": 20, "button": "middle"}})
    seen = [r for r in bridge.seen if r["cmd"] == cmd]
    check("{} button:'middle' reaches the client as button middle".format(tool),
          not pressed["result"].get("isError") and bool(seen) and seen[-1]["args"].get("button") == "middle"
          and "middle" in pressed["result"]["content"][0]["text"], (pressed, seen[-1:]))
right = rpc("tools/call", {"name": "hb_click", "arguments": {"x": 10, "y": 20, "right": True}})
seen = [r for r in bridge.seen if r["cmd"] == "click"]
check("the older right:true still sends button right", seen[-1]["args"].get("button") == "right", seen[-1:])
for bad in ({"x": 1, "y": 2, "button": "fourth"}, {"x": 1, "y": 2, "right": True, "button": "middle"}):
    refused = rpc("tools/call", {"name": "hb_click", "arguments": bad})
    check("hb_click refuses {}".format(bad), refused["result"].get("isError") is True, refused)

print("  -- hb_clients --")
dup = rpc("tools/call", {"name": "hb_clients", "arguments": {"action": "register", "port": PORT, "client": "twin"}})
text = dup["result"]["content"][0]["text"]
check("registering a second handle on a held port returns the held handle as success (HB-0695)",
      not dup["result"].get("isError") and "already registered as 'c1'" in text and "twin" not in text.split("\n")[0],
      text)

ghost = rpc("tools/call", {"name": "hb_clients", "arguments": {"action": "register", "port": PORT + 6, "client": "ghost"}})
check("registering a silent port says when it would be dropped",
      "Kept for now" in ghost["result"]["content"][0]["text"], ghost)
listed = rpc("tools/call", {"name": "hb_clients", "arguments": {"action": "list"}})
text = listed["result"]["content"][0]["text"]
check("a registered client with nothing listening is dropped by the next list (HB-0625)",
      "GONE - forgotten" in text and "ghost" in text, text)
again = rpc("tools/call", {"name": "hb_ping", "arguments": {"client": "ghost"}})
check("the dropped client is no longer registered", again["result"].get("isError") is True
      and "unknown client handle 'ghost'" in again["result"]["content"][0]["text"], again)
rpc("tools/call", {"name": "hb_clients", "arguments": {"action": "register", "port": PORT + 6, "client": "ghost2"}})
gone = rpc("tools/call", {"name": "hb_ping", "arguments": {"client": "ghost2"}})
check("a call to a client whose process is gone drops it and says so", gone["result"].get("isError") is True
      and "registration was dropped" in gone["result"]["content"][0]["text"], gone)
rpc("tools/call", {"name": "hb_clients", "arguments": {"action": "default", "client": "c1"}})

cfg = rpc("tools/call", {"name": "hb_journal_config",
                         "arguments": {"channels": {"audio.sound": False}}})
check("journal_config passes channels through",
      "audio.sound" in cfg["result"]["content"][0]["text"], cfg)

sent = [r for r in bridge.seen if r["cmd"] == "journal_config"]
check("the wire carried 'channels', not 'classes'",
      bool(sent) and "channels" in sent[-1]["args"], sent)

forgot = rpc("tools/call", {"name": "hb_clients", "arguments": {"action": "forget", "client": "c1"}})
text = forgot["result"]["content"][0]["text"]
check("forgetting the last client does not re-register a stale c1",
      "0 client(s) registered" in text and "nothing is registered now" in text, text)
after = rpc("tools/call", {"name": "hb_ping", "arguments": {}})
check("a call with no client registered says how to register one", after["result"].get("isError") is True
      and "no client is registered" in after["result"]["content"][0]["text"], after)

print("bridge protocol 2 or 3, discovery safety, tools built on SDL widget ids")
check("protocol 5 is the current one, 2 to 4 are still accepted",
      agent_mcp.EXPECTED_BRIDGE_PROTOCOL == 5 and agent_mcp.SUPPORTED_BRIDGE_PROTOCOLS == (2, 3, 4, 5))

all_commands = frozenset(ALL_COMMANDS)
for tool in ("hb_drag_slot", "hb_cast_at"):
    check("{} stays unavailable even when the client lists widgets and widget_click (SDL ids)".format(tool),
          "not ported" in (agent_mcp.unavailable_reason(tool, all_commands) or ""), agent_mcp.unavailable_reason(tool, all_commands))
check("the retargeted tools (hb_widgets, hb_click_widget, hb_flow, the world tools) are available once their commands are listed",
      all(agent_mcp.unavailable_reason(tool, all_commands) is None for tool in
          ("hb_widgets", "hb_click_widget", "hb_flow", "hb_send_chat", "hb_entities", "hb_walk_to", "hb_tile_to_screen",
           "hb_close_window", "hb_set_audio", "hb_click_entity")))
check("hb_flow needs the commands its flows use", "tile_to_screen" in " ".join(agent_mcp.TOOL_COMMANDS["hb_flow"]))
in_process = {tool["name"] for tool in agent_mcp.TOOLS if agent_mcp.unavailable_reason(tool["name"], all_commands) is None}
check("tools/list leaves out the SDL-widget-id tools for a full client",
      not in_process & {"hb_drag_slot", "hb_cast_at"} and {"hb_widgets", "hb_flow", "hb_click_widget", "hb_entities"} <= in_process, sorted(in_process))

outside = rpc("tools/call", {"name": "hb_clients", "arguments": {"action": "discover", "ports": [PORT + 5]}})["result"]
check("discover refuses a port outside 8891-8898", outside.get("isError") and "only scans ports 8891-8898" in outside["content"][0]["text"], outside)
remote = rpc("tools/call", {"name": "hb_clients", "arguments": {"action": "register", "port": 8895, "host": "10.1.2.3"}})["result"]
check("register refuses a non-loopback host", remote.get("isError") and "loopback" in remote["content"][0]["text"], remote)


class passive_fake(threading.Thread):
    """Accepts any number of connections, answers ping and get_state, counts them and notes who closed first."""

    def __init__(self, port):
        super().__init__(daemon=True)
        self.listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.listener.bind(("127.0.0.1", port))
        self.listener.listen(4)
        self.connections = 0
        self.closed_by_peer = 0
        self.cmds = []

    def run(self):
        while True:
            try:
                conn, _ = self.listener.accept()
            except OSError:
                return
            self.connections += 1
            conn.settimeout(5)
            buf = b""
            try:
                while True:
                    chunk = conn.recv(4096)
                    if not chunk:
                        self.closed_by_peer += 1
                        break
                    buf += chunk
                    while b"\n" in buf:
                        line, _, buf = buf.partition(b"\n")
                        request = json.loads(line.decode("utf-8"))
                        self.cmds.append(request["cmd"])
                        result = {"build": "0.0.9-test", "bridge_protocol": 3} if request["cmd"] == "ping" else {"screen": "Screen_Splash"}
                        conn.sendall((json.dumps({"id": request["id"], "ok": True, "result": result}) + "\n").encode("utf-8"))
            except OSError:
                pass
            conn.close()


scan_first, scan_last = agent_mcp.BRIDGE_PORT_FIRST, agent_mcp.BRIDGE_PORT_LAST
real_port_holders = agent_mcp.port_holders
good, busy = passive_fake(8901), passive_fake(8902)
good.start()
busy.start()
for existing_client in list(agent_mcp.REGISTRY.all()):
    agent_mcp.REGISTRY.forget(existing_client.handle)  # the in-process registry must not dial the held fake on 8899
try:
    agent_mcp.BRIDGE_PORT_FIRST, agent_mcp.BRIDGE_PORT_LAST = 8900, 8903
    agent_mcp.port_holders = lambda host, port: {"listeners": [1], "peers": [4242]} if port == 8902 else real_port_holders(host, port)
    scanned = agent_mcp.run_clients({"action": "discover"})["content"][0]["text"]
finally:
    agent_mcp.BRIDGE_PORT_FIRST, agent_mcp.BRIDGE_PORT_LAST = scan_first, scan_last
    agent_mcp.port_holders = real_port_holders
time.sleep(0.3)
check("discover reports a bridge it found, as not registered, with build, protocol and screen",
      "8901 answering (not registered): build 0.0.9-test, bridge_protocol 3, screen Screen_Splash" in scanned, scanned)
check("discover pinged it once on a throwaway connection that it closed itself",
      good.connections == 1 and good.cmds == ["ping", "get_state"] and good.closed_by_peer == 1, (good.connections, good.cmds, good.closed_by_peer))
check("discover registered nothing", agent_mcp.REGISTRY.count() == 0 and "0 client(s) registered" in scanned, scanned)
check("discover skips a port netstat shows connected to a foreign pid, without connecting to it",
      "8902 skipped" in scanned and "pid 4242" in scanned and busy.connections == 0, (scanned, busy.connections))

proc.stdin.close()
proc.wait(timeout=5)
shutil.rmtree(SHOT_DIR, ignore_errors=True)

print("")
if failures:
    print("{} FAILURE(S): {}".format(len(failures), ", ".join(failures)))
    sys.exit(1)
print("all checks passed")
