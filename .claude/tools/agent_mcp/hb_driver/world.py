"""In-world movement and the map-entry timing measurement.

Movement is click-only (the arrow keys do nothing in-world) and the camera is player-locked, so a
tile is aimed by PROBING: estimate the pixel from a calibrated pitch, ask hb_probe which tile a
real click there would walk to (`walk_goal_tile`), correct, and only click once the probe agrees.
A point on the HUD, on the player's own sprite (a click there picks up) or on a creature/NPC (a
click there attacks or talks) is never clicked.

Only tiles inside the view can be clicked; a long trip needs HB-0103 (walk to a map tile).
"""

import time

from . import bridge
from . import common
from . import flows
from . import server_logs

directions = {"n": (0, -1), "ne": (1, -1), "e": (1, 0), "se": (1, 1),
              "s": (0, 1), "sw": (-1, 1), "w": (-1, 0), "nw": (-1, -1)}
# World tiles are 32 source pixels (shared/game/tile_geometry.h tile_pixels).
tile_pixels = 32
# Calibration probes this far from the view centre (logical px): wide enough that a one-tile
# floor error is a small share of the measured span, inside the view on both axes.
calibration_span_x = 192
calibration_span_y = 160
# Coarse steps across a span when looking for the tile boundaries that are then bisected.
calibration_steps = 8
aim_iterations = 6
# Inside a tile, alternative aim points as a fraction of the pitch (away from sprites and edges).
aim_offsets = ((0.0, 0.0), (0.0, 0.3), (0.3, 0.0), (-0.3, 0.0), (0.0, -0.3), (0.3, 0.3), (-0.3, 0.3))
move_timeout_ms = 10000
# A walk is judged by its PROGRESS, not a fixed clock: the deadline is pushed out by arrive_progress_s
# each time the player moves, up to arrive_max_s in all, and a walk that makes no progress for
# arrive_settle_s has stopped (short of the target, or blocked).
arrive_progress_s = 10.0
arrive_max_s = 60.0
arrive_settle_s = 2.0
arrive_poll_s = 0.1
# A walk_started whose end event never arrives (a client fault): at the target this long, accept the journal.
arrive_event_grace_s = 1.0
# The client's walk events on state.player (client batch 2): every walk journals walk_started, then ends with
# walk_arrived {tile_x, tile_y, goal_x, goal_y, arrived: true} or walk_stopped {..., arrived: false, reason}
# (reason: new_order, refused, blocked, interrupted, map_changed, dead, in_range). They are judged FIRST; on a
# client without them the journal's own player_moved events and the probe decide instead.
walk_event_channel = "state.player"
walk_end_event_types = ("walk_arrived", "walk_stopped")
walk_started_event_type = "walk_started"
moved_event_type = "player_moved"
# What a walk that makes no progress is reported as stopped for, when no walk_stopped event says why.
no_progress_reason = "no_progress"
entry_timeout_ms = 45000
entry_move_timeout_ms = 15000
# After the move answer, let the frame that drained it finish journaling before reading back.
entry_settle_s = 0.25


def _detail(probe):
	return probe.get("detail") or {}


def probe_detail(sess, x, y, space=None):
	return _detail(sess.probe(x, y, space))


def player_tile(sess):
	"""(x, y) of the local player - the probe reports it for any in-world point."""
	detail = probe_detail(sess, 0, 0)
	if "player_tile" not in detail:
		raise common.flow_error("no player_tile: the client is not in the world (screen '{}')".format(sess.screen()))
	return tuple(detail["player_tile"])


class calibration:
	"""Where the view centre is and how many logical pixels one tile spans on each axis."""

	def __init__(self, ui_width, centre, centre_tile, pitch_x, pitch_y):
		self.ui_width = ui_width
		self.centre = centre
		self.centre_tile = centre_tile
		self.pitch_x = pitch_x
		self.pitch_y = pitch_y

	def estimate(self, tile):
		return (int(round(self.centre[0] + (tile[0] - self.centre_tile[0]) * self.pitch_x)),
		        int(round(self.centre[1] + (tile[1] - self.centre_tile[1]) * self.pitch_y)))

	def as_dict(self):
		return {"ui_width": self.ui_width, "centre": self.centre, "centre_tile": self.centre_tile,
		        "pitch_x": round(self.pitch_x, 2), "pitch_y": round(self.pitch_y, 2)}


def _axis_tile(sess, centre, axis, pixel):
	"""The world tile coordinate on `axis` under `pixel` on that axis (the other axis at the centre)."""
	x, y = (pixel, centre[1]) if axis == 0 else (centre[0], pixel)
	detail = probe_detail(sess, x, y)
	if "world_tile" not in detail:
		raise common.flow_error("calibration probe at ({},{}) hit the HUD or the client is not in the world: {}".format(
			x, y, detail))
	return detail["world_tile"][axis]


def _boundary(sess, centre, axis, inside, outside, inside_tile):
	"""Bisect to the first pixel (walking from `inside` toward `outside`) whose tile is not inside_tile."""
	while abs(outside - inside) > 1:
		middle = (inside + outside) // 2
		if _axis_tile(sess, centre, axis, middle) == inside_tile:
			inside = middle
		else:
			outside = middle
	return outside, _axis_tile(sess, centre, axis, outside)


def _axis_pitch(sess, centre, span, axis):
	"""Pixels per tile on one axis: the distance between the first and last tile boundaries inside
	the span, over the tiles between them - exact to a pixel, where span / tile-count carries up to a
	whole tile of floor error."""
	step = span // calibration_steps
	pixels = [centre[axis] + step * index for index in range(calibration_steps + 1)]
	tiles = [_axis_tile(sess, centre, axis, pixel) for pixel in pixels]
	boundaries = []
	for index in range(1, len(pixels)):
		if tiles[index] != tiles[index - 1]:
			boundaries.append(_boundary(sess, centre, axis, pixels[index - 1], pixels[index], tiles[index - 1]))
	if len(boundaries) < 2:
		raise common.flow_error("calibration found fewer than two tile boundaries on axis {}".format(axis))
	(first, first_tile), (last, last_tile) = boundaries[0], boundaries[-1]
	return abs((last - first) / float(last_tile - first_tile))


def calibrate(sess, refresh=False):
	"""Measure the view (centre tile and pitch per axis). Cached on the session per ui_width; pass
	refresh=True after a camera zoom. Costs ~40 probes (~1 frame each) the first time."""
	ui_width = int(sess.probe(0, 0).get("ui_width") or 0)
	if ui_width <= 0:
		raise common.flow_error("probe reported no ui_width")
	cached = getattr(sess, "view_calibration", None)
	centre = (ui_width // 2, common.ui_height // 2)
	if cached is not None and cached.ui_width == ui_width and not refresh:
		cached.centre_tile = tuple(probe_detail(sess, *centre)["world_tile"])
		return cached
	middle = probe_detail(sess, *centre)
	if "world_tile" not in middle:
		raise common.flow_error("the view centre probe has no world_tile: {}".format(middle))
	# y is measured upward from the centre: downward runs into the HUD
	pitch_x = _axis_pitch(sess, centre, calibration_span_x, 0)
	pitch_y = _axis_pitch(sess, centre, -calibration_span_y, 1)
	sess.view_calibration = calibration(ui_width, centre, tuple(middle["world_tile"]), pitch_x, pitch_y)
	return sess.view_calibration


def covered_by(detail):
	"""What takes a click at this probed point instead of the world ('window inventory'), or ''. Reads the
	client's `front` {kind, owner} / `covered_by` (client batch 1); '' when the client reports neither."""
	front = detail.get("front") if isinstance(detail.get("front"), dict) else {}
	owner = detail.get("covered_by") or ""
	if front.get("kind") not in (None, "world", "screen") or owner:
		return "{} {}".format(front.get("kind") or "?", owner or front.get("owner") or "").strip()
	return ""


def _clickable(detail, tile, allow_entities):
	if detail.get("on_hud") or "walk_goal_tile" not in detail:
		return False
	if covered_by(detail):
		return False
	if tuple(detail["walk_goal_tile"]) != tuple(tile):
		return False
	# The client now reports on_self_sprite (a pixel-perfect hit on the player's art) on every probe, and
	# self_hides_pick is TRUE on any ground nothing else contests - so only on_self_sprite means "this
	# click would be a pickup". An older client reported just self_hides_pick, and only when it applied.
	# on_self_box is the generous click-on-self box (one tile wide, 52 px up): a click there is a pickup.
	if detail.get("on_self_sprite") or detail.get("on_self_box"):
		return False
	if "on_self_sprite" not in detail and detail.get("self_hides_pick"):
		return False
	if not allow_entities and (detail.get("attackable") or detail.get("interactable")):
		return False
	return True


# The probe fields worth quoting in an error; the whole probe is dozens of blocks of diagnostics.
probe_summary_keys = ("on_hud", "on_self_sprite", "on_self_box", "self_hides_pick", "front", "covered_by", "walk_goal_tile",
                      "world_tile", "attackable", "interactable", "creature_id")


def probe_summary(detail):
	return {key: detail[key] for key in probe_summary_keys if key in detail}


class no_clickable_point(common.flow_error):
	"""No point in the tile reaches the world with a click. `reason` is why: self_box (under the player's own
	click-on-self box - the tile north of you), covered (a window or the HUD takes the click) or other (off
	screen, blocked, under a sprite). Only self_box makes step() fall back to walk_to on its own."""

	def __init__(self, message, reason="other"):
		super().__init__(message)
		self.reason = reason


def tile_view(sess, tile):
	"""A calibration around `tile` from the client's own tile_to_screen (exact: three calls instead of ~40
	probes), or None - an older client without the command, or a tile off screen."""
	try:
		here = sess.call("tile_to_screen", {"x": int(tile[0]), "y": int(tile[1])})
		if not here.get("on_screen") or not here.get("logical"):
			return None
		east = sess.call("tile_to_screen", {"x": int(tile[0]) + 1, "y": int(tile[1])})
		south = sess.call("tile_to_screen", {"x": int(tile[0]), "y": int(tile[1]) + 1})
	except bridge.bridge_error:
		return None
	pitch_x = abs(east["logical"][0] - here["logical"][0])
	pitch_y = abs(south["logical"][1] - here["logical"][1])
	if pitch_x <= 0 or pitch_y <= 0:
		return None
	return calibration(None, tuple(here["logical"]), tuple(tile), float(pitch_x), float(pitch_y))


def aim_tile(sess, tile, view=None, allow_entities=False):
	"""A logical point whose click walks to `tile`, proven by probe. Returns (x, y, probe detail)."""
	view = view or tile_view(sess, tile) or calibrate(sess)
	x, y = view.estimate(tile)
	detail = {}
	for _ in range(aim_iterations):
		detail = probe_detail(sess, x, y)
		got = detail.get("walk_goal_tile")
		if got is None:
			break  # on the HUD: fall through to the in-tile alternatives
		if tuple(got) == tuple(tile):
			break
		x += int(round((tile[0] - got[0]) * view.pitch_x))
		y += int(round((tile[1] - got[1]) * view.pitch_y))
	covered = set()
	self_box = False
	on_tile = False
	for fraction_x, fraction_y in aim_offsets:
		candidate = (int(round(x + fraction_x * view.pitch_x)), int(round(y + fraction_y * view.pitch_y)))
		detail = probe_detail(sess, *candidate)
		if _clickable(detail, tile, allow_entities):
			return candidate[0], candidate[1], detail
		if tuple(detail.get("walk_goal_tile") or ()) == tuple(tile):
			on_tile = True
			if covered_by(detail):
				covered.add(covered_by(detail))
			elif detail.get("on_self_sprite") or detail.get("on_self_box"):
				self_box = True
	if covered:
		raise no_clickable_point("tile {} is covered: a click there would go to {}, not the world (close it, "
		                         "pick another tile, or hb_walk_to it; step walk_fallback:true walks instead)"
		                         .format(tile, ", ".join(sorted(covered))), "covered")
	if self_box and on_tile:
		raise no_clickable_point("no clickable point for tile {}: it is under the player's own click-on-self box (the "
		                         "tile north of you), where a click is a pickup, never a move (last probe {})"
		                         .format(tile, probe_summary(detail)), "self_box")
	raise no_clickable_point("no clickable point for tile {} (last probe {}); it may be off-screen, blocked, "
	                         "under the HUD or under a sprite - hb_walk_to walks there without a click"
	                         .format(tile, probe_summary(detail)))


def _tile_of(data):
	"""(x, y) from an event payload (player_moved's to_x / to_y, a walk event's tile_x / tile_y); None if absent."""
	for keys in (("to_x", "to_y"), ("tile_x", "tile_y")):
		if all(isinstance(data.get(key), int) for key in keys):
			return (data[keys[0]], data[keys[1]])
	return None


def walk_summary(events, target):
	"""What the state.player events (oldest first, after the order) say about a walk to `target`.

	`last_to` is the newest player_moved's tile (the journal's own integer tile - the probe's player_tile can
	read a tile off at the end of a step), `moves` counts them. `walk_end` is the newest walk_arrived /
	walk_stopped FOR THIS GOAL (another walk's end - the previous order's new_order stop, fired by this very
	click - carries a different goal and is skipped), with `walk_end_arrived` from its `arrived` flag and
	`reason` from a stop. None while the client has sent no such event (an older client).
	`open` is True while a walk_started for this goal has no end after it: the walk's own end event is still
	to come, so the journal's player_moved is not yet the verdict (it lands first, one frame early).
	"""
	summary = {"moves": 0, "last_to": None, "walk_end": None, "walk_end_arrived": None, "reason": None, "open": False}
	for event in events:
		if event.get("channel") != walk_event_channel:
			continue
		data = event.get("data") or {}
		kind = event.get("type")
		if kind == moved_event_type:
			summary["moves"] += event.get("count") or 1
			tile = _tile_of(data)
			if tile is not None:
				summary["last_to"] = tile
		elif kind == walk_started_event_type:
			goal = (data.get("goal_x"), data.get("goal_y"))
			if None in goal or goal == tuple(target):
				summary.update({"walk_end": None, "walk_end_arrived": None, "reason": None, "open": True})
		elif kind in walk_end_event_types:
			goal = (data.get("goal_x"), data.get("goal_y"))
			if None not in goal and goal != tuple(target):
				continue
			summary["open"] = False
			summary["walk_end"] = event
			summary["walk_end_arrived"] = bool(data.get("arrived", kind == "walk_arrived"))
			summary["reason"] = data.get("reason")
	return summary


def _distance(a, b):
	return max(abs(a[0] - b[0]), abs(a[1] - b[1]))


def wait_walk(sess, target, seq0):
	"""Follow a walk until it ends. Returns {arrived, ended, reason, judged_by, player_tile, last_move_to,
	moves, tiles_short, walk_end}.

	`ended` is arrived / stopped / timeout. The client's walk_arrived / walk_stopped event for this goal is
	judged FIRST (`judged_by: event`, and a stop carries its `reason`); without one - an older client - the
	journal's newest player_moved and the probe's tile decide (`judged_by: journal`), a walk with no progress
	for arrive_settle_s is stopped (reason no_progress), and one still moving at arrive_max_s is a timeout.
	"""
	target = tuple(target)
	types = [moved_event_type, walk_started_event_type] + list(walk_end_event_types)
	started = time.monotonic()
	progress_at = started
	deadline = started + arrive_progress_s
	last_signature = None
	at_target_since = None
	while True:
		events = sess.drain_journal(after_seq=seq0, channels=[walk_event_channel], types=types)
		summary = walk_summary(events, target)
		tile = player_tile(sess)
		now = time.monotonic()
		ended = judged_by = reason = None
		if tile == target or summary["last_to"] == target:
			at_target_since = at_target_since if at_target_since is not None else now
		if summary["walk_end"] is not None:
			ended = "arrived" if summary["walk_end_arrived"] else "stopped"
			judged_by = "event"
			reason = summary["reason"]
		elif (tile == target or summary["last_to"] == target) and (not summary["open"] or (
				at_target_since is not None and now - at_target_since >= arrive_event_grace_s)):
			# a walk_started with no end yet holds this verdict back, up to a grace if the end never comes
			ended, judged_by = "arrived", "journal"
		else:
			signature = (summary["moves"], tile)
			if signature != last_signature:
				last_signature = signature
				progress_at = now
				deadline = min(started + arrive_max_s, now + arrive_progress_s)
			elif now - progress_at >= arrive_settle_s:
				ended, judged_by, reason = "stopped", "journal", no_progress_reason
			if ended is None and now >= deadline:
				ended, judged_by = "timeout", "journal"
		if ended:
			arrived = ended == "arrived"
			best = summary["last_to"] if summary["last_to"] is not None else tile
			end_tile = _tile_of((summary["walk_end"] or {}).get("data") or {})
			if end_tile is not None:
				best = end_tile
			return {"arrived": arrived, "ended": ended, "reason": reason, "judged_by": judged_by,
			        "player_tile": tile, "last_move_to": summary["last_to"], "moves": summary["moves"],
			        "tiles_short": 0 if arrived else _distance(best, target), "walk_end": summary["walk_end"]}
		time.sleep(arrive_poll_s)


def _no_answer_hint(trail, consumed=None):
	"""Why a click got no move answer, from the click's own `consumed_by` (when the client reports it)
	and the events seen while it waited."""
	if isinstance(consumed, dict) and consumed.get("kind") not in (None, "world", "screen"):
		return "the click was consumed by {} {}, not the world".format(consumed["kind"], consumed.get("owner") or "")
	sent = any(event.get("channel") == "net.packet" and event.get("type") == "packet_out"
	           and (event.get("data") or {}).get("message") == "move_request" for event in trail)
	if sent:
		return "move_request left the client but no move_confirm / move_reject came back (server slow, or net.packet journalling is off)"
	return ("no move_request left the client: the click was swallowed (blocked tile, an open dialog, the HUD, "
	        "or the player is busy) - or net.packet journalling is off")


def click_tile(sess, tile, arrive=True, timeout_ms=move_timeout_ms, view=None, allow_entities=False):
	"""Click-to-move to a visible tile and wait for the server's FIRST answer (move_confirm or
	move_reject). With arrive=True, then follow the walk until it ends (wait_walk).

	Result: from, target, pixel, outcome (the FIRST answer), answer_event (that first answer - a multi-step
	walk is confirmed step by step, so it is not the arrival), arrived (True / False / None when not
	followed), and when followed: ended, player_tile (the probe), last_move_to (the journal), moves,
	tiles_short and walk_end (the client's walk-end event, when it sends one). A no_answer carries a hint.
	"""
	start = player_tile(sess)
	x, y, detail = aim_tile(sess, tile, view, allow_entities)
	seq0, _ = sess.journal_clock()
	conditions = [bridge.packet_condition("move_confirm"), bridge.packet_condition("move_reject")]
	answer, clicked = sess.wait_for_then(conditions, timeout_ms, "click", {"x": x, "y": y})
	consumed = (clicked or {}).get("consumed_by")
	outcome = "no_answer"
	if answer.satisfied:
		outcome = "move_confirm" if answer.index == 0 else "move_reject"
	result = {"from": start, "target": tuple(tile), "pixel": (x, y), "outcome": outcome,
	          "answer_event": answer.event, "arrived": None}
	if consumed:
		result["consumed_by"] = consumed
	if outcome == "no_answer":
		result["hint"] = _no_answer_hint(answer.trail, consumed)
	if arrive and outcome == "move_confirm":
		result.update(wait_walk(sess, tile, seq0))
	return result


def walk_to(sess, tile, arrive=True):
	"""Walk to a map tile by the client's own pathing (`walk_to`): no pixel is aimed, so it works for a tile
	off screen or under the player's own click-on-self box. Not a click - use click_tile to test what a click
	does. Raises bridge_error with the client's `walk_to refused: ...` reason when it will not walk.

	Result: via, from, target, status (walking / arrived), path_len, reachable and - when followed (arrive) -
	wait_walk's fields (arrived, ended, reason, player_tile, ...).
	"""
	start = player_tile(sess)
	seq0, _ = sess.journal_clock()
	reply = sess.call("walk_to", {"x": int(tile[0]), "y": int(tile[1])})
	result = {"via": "walk_to", "from": start, "target": tuple(tile), "status": reply.get("status"),
	          "path_len": reply.get("path_len"), "reachable": reply.get("reachable"), "map": reply.get("map"),
	          "arrived": None}
	if arrive:
		result.update(wait_walk(sess, tile, seq0))
	return result


def step(sess, direction, arrive=True, timeout_ms=move_timeout_ms, walk_fallback=None):
	"""One tile in a compass direction (n, ne, e, se, s, sw, w, nw), by a real click.

	The tile directly north of the player lies under the click-on-self box (a click there is a pickup), so no
	click can reach it. walk_fallback None (default, automatic) makes just THAT step with walk_to and says so
	(via walk_to, click_blocked); any other failure (covered by a window or the HUD, off screen, blocked)
	raises with its reason, so a click test is never silently turned into a walk. True forces the walk_to
	fallback for every no-clickable-point failure; False never falls back.
	"""
	if direction not in directions:
		raise common.flow_error("direction must be one of {}".format(sorted(directions)))
	here = player_tile(sess)
	delta = directions[direction]
	target = (here[0] + delta[0], here[1] + delta[1])
	try:
		return click_tile(sess, target, arrive, timeout_ms)
	except no_clickable_point as blocked:
		if walk_fallback is False or (walk_fallback is None and blocked.reason != "self_box"):
			raise
		try:
			result = walk_to(sess, target, arrive)
		except bridge.bridge_error as error:
			if "unknown cmd" in str(error):
				raise blocked  # an older client has no walk_to: the click failure is the honest answer
			raise
		result["click_blocked"] = str(blocked).split(" (last probe")[0]
		result["click_blocked_reason"] = blocked.reason
		return result


# --- map-entry timing ----------------------------------------------------------------------------

def _first(events, predicate, after_ms=None):
	for event in events:
		if (after_ms is None or event.get("t_ms", 0) >= after_ms) and predicate(event):
			return event
	return None


def _is(channel, event_type=None, **data):
	def predicate(event):
		if event.get("channel") != channel or (event_type and event.get("type") != event_type):
			return False
		payload = event.get("data") or {}
		return all(payload.get(key) == value for key, value in data.items())
	return predicate


def _t(event):
	return event.get("t_ms") if event else None


def _span(start, end):
	return (end - start) if start is not None and end is not None else None


def entry_click_point(sess):
	"""The tile EAST of the player in OUTPUT pixels, from the output size (known on any screen).
	The player is drawn at the view centre; one tile is tile_pixels scaled by output height / 480.
	Equivalent to the timing run's logical (458,240) at 1920x1080, but correct at any size."""
	width, height = sess.probe(0, 0).get("output_size") or (0, 0)
	if not width or not height:
		raise common.flow_error("probe reported no output_size")
	return {"x": width // 2 + int(round(tile_pixels * height / common.ui_height)), "y": height // 2, "space": "window"}


def entry_milestones(events):
	"""The timing run's journal points from events OLDEST first (all_maps_timing.md, table A..J)."""
	start = _first(events, _is("ui.button", "button_activated"))
	opened = _first(events, _is("ui.window", "window_opened", window="entering"))
	response = _first(events, _is("net.packet", "packet_in", message="world_enter_response"))
	map_changed = _first(events, _is("state.map", "map_changed"))
	screen = _first(events, _is("ui.screen", "screen_changed", to=flows.in_game))
	closed = _first(events, _is("ui.window", "window_closed", window="entering"))
	# the client's own readiness markers (HB-0159, client batch 3-4): the in-game screen fully shown, its first
	# presented frame, and the end of the frame that first drew the entered world. An older client has none,
	# and the spawn frame (below) stands in.
	active = _first(events, _is("ui.screen", "screen_active", screen=flows.in_game), _t(screen))
	presented = _first(events, _is("ui.screen", "first_frame_presented", screen=flows.in_game), _t(screen))
	ready = _first(events, _is("ui.screen", "world_ready"))
	spawn_frame = closed.get("frame") if closed else None
	frame_events = [event for event in events if spawn_frame is not None and event.get("frame") == spawn_frame]
	frame_end = max((event.get("t_ms_last", event.get("t_ms", 0)) for event in frame_events), default=None)
	after_frame = next((event for event in events if spawn_frame is not None and (event.get("frame") or 0) > spawn_frame), None)
	move_out = _first(events, _is("net.packet", "packet_out", message="move_request"), _t(closed))
	answer = _first(events, lambda event: event.get("channel") == "net.packet" and event.get("type") == "packet_in"
	                and (event.get("data") or {}).get("message") in ("move_confirm", "move_reject"), _t(move_out))
	moved = _first(events, _is("state.player", "player_moved"), _t(move_out) if move_out else None)
	revive = _first(events, _is("ui.window", "window_opened", window="revive"))
	attacks = [event for event in events if _is("net.packet", "packet_in", message="attack_event")(event)]
	points = {"A_start_click": _t(start), "D_entering_opened": _t(opened), "G_world_enter_response": _t(response),
	          "map_changed": _t(map_changed), "screen_changed": _t(screen), "entering_closed": _t(closed),
	          "screen_active": _t(active), "first_frame_presented": _t(presented), "world_ready": _t(ready),
	          "spawn_frame_last_event": frame_end, "next_frame_first_event": _t(after_frame),
	          "H_move_request": _t(move_out), "J_move_answer": _t(answer), "revive_opened": _t(revive)}
	outcome = "timeout"
	if answer:
		outcome = answer["data"]["message"]
	if revive and (answer is None or revive["t_ms"] <= answer["t_ms"]):
		outcome = "killed"
	return {
		"points_ms": points,
		"spans_ms": {"start_to_entering": _span(points["A_start_click"], points["D_entering_opened"]),
		             "entering_to_response": _span(points["D_entering_opened"], points["G_world_enter_response"]),
		             "response_to_closed": _span(points["G_world_enter_response"], points["entering_closed"]),
		             "closed_to_frame_end": _span(points["entering_closed"], frame_end),
		             "frame_end_to_move_request": _span(frame_end, points["H_move_request"]),
		             "start_to_world_ready": _span(points["A_start_click"], points["world_ready"]),
		             "response_to_world_ready": _span(points["G_world_enter_response"], points["world_ready"]),
		             "world_ready_to_move_request": _span(points["world_ready"], points["H_move_request"]),
		             "move_round_trip": _span(points["H_move_request"], points["J_move_answer"])},
		"total_ms": _span(points["A_start_click"], points["J_move_answer"]) if outcome != "killed" else None,
		"outcome": outcome,
		"spawn_frame": spawn_frame,
		"ready_marker": "world_ready" if ready else "spawn_frame",
		"world_ready_at": ({"map": (ready.get("data") or {}).get("map"),
		                    "tile": ((ready.get("data") or {}).get("tile_x"), (ready.get("data") or {}).get("tile_y"))}
		                   if ready else None),
		"map_changed_to": (map_changed.get("data") or {}).get("to") if map_changed else None,
		"spawn_tile": ((map_changed.get("data") or {}).get("x"), (map_changed.get("data") or {}).get("y")) if map_changed else None,
		"moved": ({"from": (moved["data"].get("from_x"), moved["data"].get("from_y")),
		           "to": (moved["data"].get("to_x"), moved["data"].get("to_y"))} if moved else None),
		"attack_events": len(attacks),
	}


# Log lines worth keeping per entry, by log.
entry_log_needles = {
	"login": ("client connected (", "enter-game granted", "enter-game"),
	"world": ("preload reserved", "client connected (", "player entered:"),
	"client": ("entered world", "map load timing", "tile atlas", "minimap:", "[WARN", "[ERROR"),
}


def _aligned(milestones, logs):
	"""journal ms <- log wall ms, anchored on the client log's `entered world` vs journal map_changed
	(the same function stamps both, in_game_screen.cpp apply_spawn). None when either is missing."""
	anchor = next((record for record in logs.get("client", []) if "entered world" in record["text"]), None)
	map_ms = milestones["points_ms"].get("map_changed")
	if anchor is None or map_ms is None:
		return None
	offset = anchor["wall_ms"] - map_ms
	return {name: [{"journal_ms": record["wall_ms"] - offset, "line": record["line"]} for record in records]
	        for name, records in logs.items()}


def time_entry(sess, name=None, click=None, capture_logs=True, timeout_ms=entry_timeout_ms,
               move_timeout_ms=entry_move_timeout_ms):
	"""Time one map entry: Start click -> the first confirmed move. Run from character select.

	Method (after all_maps_timing.md "How one entry is timed"): note the journal clock, register a
	wait on `screen_changed {to: in_game_screen}` (or the revive window) and click Start in one socket
	write; when it fires, poll get_state until `object_id` shows the spawn applied (it may land before
	or after the swap - HB-0156); then register a wait on move_confirm / move_reject (or revive) and
	click the tile east of the player in the same write; then read the journal and the logs. The
	client's own markers (`screen_active`, `first_frame_presented`, `world_ready {map, tile}`, HB-0159)
	are reported as milestones with their spans (`start_to_world_ready`, `world_ready_to_move_request`);
	a client without them falls back to the spawn frame (`ready_marker`). The `entering` window is still
	REPORTED as a milestone, not used as a trigger.
	`click` overrides the point ({x, y, space}).
	Returns milestones (journal ms = client uptime), spans, total_ms, outcome (move_confirm,
	move_reject, killed, no_entry, timeout) and, with capture_logs, the log lines aligned to the
	journal clock.
	"""
	if name:
		flows.select_character(sess, name)
	point = click or entry_click_point(sess)
	watches = {}
	if capture_logs:
		watches = {"login": server_logs.log_watch(common.login_log_path),
		           "world": server_logs.log_watch(common.world_log_path),
		           "client": server_logs.log_watch(common.client_log_path)}
	seq0, now0 = sess.journal_clock()
	entered, _ = sess.wait_for_then(flows.entry_conditions(), timeout_ms, "widget_click", {"id": "char_select.start"})
	if entered.satisfied and entered.index == 0:
		# the click must follow the spawn, whichever side of the screen swap it landed on (HB-0156)
		flows.wait_spawned(sess, timeout_ms / 1000.0)
		move_conditions = [bridge.packet_condition("move_confirm"), bridge.packet_condition("move_reject"),
		                   bridge.window_condition("revive", "window_opened")]
		sess.wait_for_then(move_conditions, move_timeout_ms, "click", dict(point))
	time.sleep(entry_settle_s)
	events = sess.drain_journal(after_seq=seq0)
	result = entry_milestones(events)
	if not entered.satisfied:
		result["outcome"] = "no_entry"
	result.update({"journal_seq0": seq0, "journal_now0_ms": now0, "click": point, "events_read": len(events),
	               "character": name or sess.state().get("character")})
	if capture_logs:
		logs = {}
		for key, watch in watches.items():
			logs[key] = [record for record in watch.records()
			             if any(needle in record["line"] for needle in entry_log_needles[key])]
		result["logs"] = {key: [record["line"] for record in records] for key, records in logs.items()}
		result["logs_aligned"] = _aligned(result, logs)
	return result
