"""Movement in the world: the player's tile, aiming a tile, clicking it, stepping, walking.

The client's own `tile_to_screen` says where a tile draws and proves the click lands on it (round_trip), so a
click is aimed by that one call instead of measuring a pixel pitch. The tile is 32 UI units. `walk_to` is the
client's own pathing with no pixel; its reply says how the walk ended.

Not ported: the entry-timing flow (it read the SDL journal milestones and server log lines).
"""

import time

from . import common

directions = {"n": (0, -1), "ne": (1, -1), "e": (1, 0), "se": (1, 1),
              "s": (0, 1), "sw": (-1, 1), "w": (-1, 0), "nw": (-1, -1)}
# One map tile in UI units (800x600 design space).
tile_pixels = 32
move_timeout_ms = 10000
arrive_poll_s = 0.1
walk_timeout_ms = 15000
# The tile north of the player sits under the click-on-self box in SDL; here a click on it is tried first and a
# walk is the fallback only when asked for.
no_progress_reason = "no_progress"


class no_clickable_point(common.flow_error):
	"""A tile has no pixel a click reaches it by (off screen, or the click lands on another tile)."""

	def __init__(self, message, tile=None, detail=None):
		super().__init__(message)
		self.tile = tile
		self.detail = detail


def player_tile(sess):
	"""(x, y) of the local player, from get_state."""
	state = sess.state()
	if not state.get("in_world"):
		raise common.flow_error("the client is not in the world (screen '{}')".format(state.get("screen")))
	return (int(state["player_x"]), int(state["player_y"]))


def aim_tile(sess, tile):
	"""The window pixel a click reaches `tile` by: {window, logical, tile_to_screen}. Raises no_clickable_point when
	the tile is off screen or a click there would resolve to another tile."""
	here = sess.tile_to_screen(tile[0], tile[1])
	if not here.get("in_bounds", True):
		raise no_clickable_point("tile ({},{}) is outside the map".format(tile[0], tile[1]), tile, here)
	if not here.get("on_screen"):
		raise no_clickable_point("tile ({},{}) is off screen (player at {})".format(
			tile[0], tile[1], tuple(here.get("player_tile") or ())), tile, here)
	if here.get("round_trip") is False:
		raise no_clickable_point("a click on tile ({},{}) resolves to {}".format(
			tile[0], tile[1], tuple(here.get("resolves_to") or ())), tile, here)
	return {"window": list(here["window"]), "logical": list(here["logical"]), "tile_to_screen": here}


def wait_arrival(sess, goal, timeout_ms):
	"""Poll the player's tile until it is `goal` or the time is up. Returns (arrived, tile)."""
	deadline = time.monotonic() + timeout_ms / 1000.0
	tile = player_tile(sess)
	while tuple(tile) != tuple(goal) and time.monotonic() < deadline:
		time.sleep(arrive_poll_s)
		tile = player_tile(sess)
	return tuple(tile) == tuple(goal), tile


def click_tile(sess, tile, arrive=True, timeout_ms=move_timeout_ms):
	"""Click the pixel of `tile` (click-to-move). With arrive, wait for the player to stand on it."""
	tile = (int(tile[0]), int(tile[1]))
	started = player_tile(sess)
	aim = aim_tile(sess, tile)
	clicked = sess.click(aim["window"][0], aim["window"][1], space="window")
	result = {"tile": list(tile), "from": list(started), "clicked_window": aim["window"], "consumed_by": clicked.get("consumed_by")}
	if arrive:
		arrived, now = wait_arrival(sess, tile, timeout_ms)
		result.update({"arrived": arrived, "player_tile": list(now)})
	return result


def walk_to(sess, tile, arrive=True, timeout_ms=walk_timeout_ms):
	"""The client's own pathing to `tile`. Returns its reply (status arrived / blocked / timeout / walking ...)."""
	return sess.walk_to(int(tile[0]), int(tile[1]), wait=arrive, timeout_ms=timeout_ms)


def step(sess, direction, arrive=True, timeout_ms=move_timeout_ms, walk_fallback=None):
	"""One tile in `direction` (n ne e se s sw w nw) by a click on it; walk_fallback true walks instead when no click
	can reach the tile, false never walks, None (default) never walks either."""
	if direction not in directions:
		raise common.flow_error("direction must be one of {}".format(" ".join(sorted(directions))))
	here = player_tile(sess)
	goal = (here[0] + directions[direction][0], here[1] + directions[direction][1])
	try:
		return click_tile(sess, goal, arrive, timeout_ms)
	except no_clickable_point:
		if not walk_fallback:
			raise
		return walk_to(sess, goal, arrive, timeout_ms)


def time_entry(sess, name=None, click=None, capture_logs=True, timeout_ms=45000):
	raise common.flow_error("time_entry is not ported to this client yet")
