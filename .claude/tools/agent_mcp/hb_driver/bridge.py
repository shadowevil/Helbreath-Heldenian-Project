"""One connection to one debug client's agent bridge, plus the waits every flow is built from.

The transport is agent_mcp.bridge_client - the same reply demultiplexer the MCP server uses - so
the wire protocol lives in one place. A session either owns its own socket (a script or the CLI)
or borrows the MCP server's (the hb_flow tool), because a client accepts ONE bridge connection
at a time.

Wire argument names are the CLIENT's, not the MCP tool schemas' (agent_bridge_playbook/5-end-to-end.md): set_text and
text take `value`, key takes `name`, screenshot takes a bare `name` (saved under build/screenshots/).
"""

import os
import sys
import time

from . import common

if common.agent_mcp_dir not in sys.path:
	sys.path.insert(0, common.agent_mcp_dir)
import agent_mcp  # noqa: E402  (shares bridge_client with the MCP server)

default_host = agent_mcp.BRIDGE_HOST
command_timeout_s = agent_mcp.BRIDGE_TIMEOUT_SECONDS
# path_bench holds the client's frame for every timed run (HB-0398).
path_bench_timeout_s = agent_mcp.PATH_BENCH_TIMEOUT_SECONDS
# press_bench holds the client's frame while one creature is benched (HB-0748).
press_bench_timeout_s = agent_mcp.PRESS_BENCH_TIMEOUT_SECONDS
# The client clamps a wait to this; asking for more would be silently shortened.
wait_max_ms = agent_mcp.WAIT_FOR_MAX_MS
wait_slack_s = agent_mcp.WAIT_FOR_SLACK_SECONDS
wait_max_conditions = agent_mcp.WAIT_FOR_MAX_CONDITIONS
# A widget or screen poll; a frame is ~16-40 ms, so this never spins faster than the client answers.
poll_interval_s = 0.05
# How often connect() re-pings a client that is still starting up.
connect_retry_s = 0.25
# One journal_query page (the client's clamp); older pages are fetched with before_seq.
journal_page_max = 1000


class bridge_error(common.flow_error):
	"""The bridge refused a command or could not be reached."""


def event_condition(channel, event_type=None, match=None, object_id=None, message_contains=None):
	"""A wait_for EVENT condition. `match` keys must ALL equal the event's data; `object_id` (an entity id) and
	`message_contains` (text) are matched by the client."""
	condition = {"channel": channel}
	if event_type:
		condition["type"] = event_type
	if match:
		condition["match"] = dict(match)
	if object_id is not None:
		condition["object_id"] = int(object_id)
	if message_contains:
		condition["message_contains"] = str(message_contains)
	return condition


def state_condition(key, equals):
	"""A wait_for STATE condition on a flat get_state key (screen, transitioning, character, ...)."""
	return {"state": key, "equals": equals}


def packet_condition(message, direction="packet_in"):
	"""A world packet by wire message name (the journal's `type` is packet_in / packet_out)."""
	return event_condition("net.packet", direction, {"message": message})


def window_condition(window, event_type):
	"""A dialog window opening or closing (`window_opened` / `window_closed`)."""
	return event_condition("ui.window", event_type, {"window": window})


class wait_result:
	"""A settled wait_for. A timeout is a normal outcome: `satisfied` is False, nothing raised."""

	def __init__(self, raw):
		self.raw = raw
		self.satisfied = bool(raw.get("satisfied"))
		matched = raw.get("matched") or {}
		self.index = matched.get("index")
		self.condition = matched.get("condition")
		self.event = matched.get("event")
		self.observed = matched.get("observed")
		self.state = raw.get("state") or {}
		self.trail = raw.get("events") or []

	def summary(self):
		return {"satisfied": self.satisfied, "index": self.index, "condition": self.condition,
		        "event": self.event, "observed": self.observed, "screen": self.state.get("screen")}


class pending_command:
	"""A command already sent whose reply is collected later (see session.submit)."""

	def __init__(self, session, cmd, pending):
		self._session = session
		self._cmd = cmd
		self._pending = pending

	def result(self, timeout_s=None):
		try:
			reply = self._pending.wait(command_timeout_s if timeout_s is None else timeout_s)
		except RuntimeError as error:
			raise bridge_error("bridge at {}: {}".format(self._session.address(), error))
		return self._session.unwrap(self._cmd, reply)


class session:
	"""Drive one client. Every flow in hb_driver takes one of these as its first argument."""

	def __init__(self, port=None, host=default_host, transport=None):
		if transport is None and port is None:
			raise bridge_error("a session needs a port or a transport")
		self.port = port
		self.host = host
		# A borrowed transport (the MCP server's) is never shut down by us.
		self._owns_transport = transport is None
		self.transport = transport if transport is not None else agent_mcp.bridge_client(host, port)
		# world.calibrate() caches the view measurement here
		self.view_calibration = None

	@classmethod
	def connect(cls, port, host=default_host, timeout_s=30.0):
		"""Open a session and wait until the client answers ping (it may still be starting up)."""
		deadline = time.monotonic() + timeout_s
		last_error = None
		while time.monotonic() < deadline:
			candidate = cls(port=port, host=host)
			try:
				candidate.ping()
				return candidate
			except bridge_error as error:
				last_error = error
				candidate.close()
				time.sleep(connect_retry_s)
		raise bridge_error("no bridge answered on {}:{} within {:g}s ({})".format(host, port, timeout_s, last_error))

	def address(self):
		return "{}:{}".format(self.host, self.port) if self.port is not None else "(borrowed transport)"

	def close(self):
		if self._owns_transport:
			self.transport.shutdown()

	def __enter__(self):
		return self

	def __exit__(self, *exc):
		self.close()
		return False

	# --- raw commands ---------------------------------------------------------------------------

	def unwrap(self, cmd, reply):
		if not reply.get("ok", False):
			raise bridge_error("client rejected '{}': {}".format(cmd, reply.get("error", "unknown error")))
		return reply.get("result") or {}

	def call(self, cmd, args=None, timeout_s=None):
		"""Send one wire command and return its result dict; raises bridge_error on any failure."""
		try:
			reply = self.transport.request(cmd, args, timeout=command_timeout_s if timeout_s is None else timeout_s)
		except RuntimeError as error:
			raise bridge_error("bridge at {}: {}".format(self.address(), error))
		return self.unwrap(cmd, reply)

	def submit(self, cmd, args=None):
		"""Send without waiting. Lines reach the client in submit order, so a wait submitted here is
		registered before an action sent after it (a wait matches only events from registration on)."""
		try:
			return pending_command(self, cmd, self.transport.submit(cmd, args))
		except RuntimeError as error:
			raise bridge_error("bridge at {}: {}".format(self.address(), error))

	# --- primitives ---------------------------------------------------------------------------

	def ping(self):
		return self.call("ping")

	def state(self):
		return self.call("get_state")

	def screen(self):
		return self.state().get("screen")

	def widgets(self):
		"""{ui_width, ui_height, widgets:[{id, kind, label, x, y, w, h, enabled, value, owner}]}"""
		return self.call("widgets")

	def widget_map(self, listing=None):
		listing = listing if listing is not None else self.widgets()
		return {widget.get("id"): widget for widget in listing.get("widgets") or []}

	def widget(self, widget_id, listing=None):
		return self.widget_map(listing).get(widget_id)

	def click_widget(self, widget_id, right=False, shift=False, ctrl=False, button=None):
		"""Click a widget by id; the client refuses an unknown or DISABLED one (no silent no-op).
		`button` left|right|middle overrides `right`."""
		try:
			return self.call("widget_click", {"id": widget_id, "button": button or ("right" if right else "left"),
			                                  "shift": shift, "ctrl": ctrl})
		except bridge_error as error:
			hinted = agent_mcp.with_scroll_hint(str(error))
			if hinted != str(error):
				raise bridge_error(hinted) from None
			raise

	def set_audio(self, **args):
		"""set_audio {master_muted?, master?, music?, effects?, persist?}; no args reads back. Returns
		{master_muted, master, music, effects, persisted}."""
		return self.call("set_audio", {key: value for key, value in args.items() if value is not None})

	def path_bench(self, fixture=None, repeats=None):
		"""path_bench {fixture?, repeats?} (HB-0398): the pathfinder candidates timed on the live map; `fixture` is an
		ABSOLUTE path the client also saves the timed map to. Returns {map, cases, rows, mismatches, ...}. A bench
		holds the client's frame for every run, so it gets a long deadline."""
		args = {key: value for key, value in (("fixture", fixture), ("repeats", repeats)) if value is not None}
		return self.call("path_bench", args, timeout_s=path_bench_timeout_s)

	def press_bench(self, display_id=None, repeats=None, live_path=None):
		"""press_bench {display_id?, repeats?, live_path?} (HB-0748): no display_id lists the creatures, one benches its
		death-pose press fields (old brute force vs transform), live_path old / new switches the world's press path."""
		args = {key: value for key, value in (("display_id", display_id), ("repeats", repeats), ("live_path", live_path))
		        if value is not None}
		return self.call("press_bench", args, timeout_s=press_bench_timeout_s)

	def inject(self, **args):
		"""inject_message {type, ...} (HB-0395): server messages built by the client's own serializers, dispatched as
		if the world socket read them. Returns {type, queued}."""
		return self.call("inject_message", {key: value for key, value in args.items() if value is not None})

	def click(self, x, y, space=None, right=False, shift=False, ctrl=False, button=None):
		"""`button` left|right|middle overrides `right`."""
		args = {"x": int(x), "y": int(y), "button": button or ("right" if right else "left"), "shift": shift,
		        "ctrl": ctrl}
		if space:
			args["space"] = space
		return self.call("click", args)

	def mouse_down(self, x, y, space=None, button="left", shift=False, ctrl=False):
		"""Press and hold `button` (left|right|middle); pair with mouse_up."""
		args = {"x": int(x), "y": int(y), "button": button, "shift": shift, "ctrl": ctrl}
		if space:
			args["space"] = space
		return self.call("mouse_down", args)

	def mouse_up(self, x, y, space=None, button="left", shift=False, ctrl=False):
		args = {"x": int(x), "y": int(y), "button": button, "shift": shift, "ctrl": ctrl}
		if space:
			args["space"] = space
		return self.call("mouse_up", args)

	def mouse_move(self, x, y, space=None):
		args = {"x": int(x), "y": int(y)}
		if space:
			args["space"] = space
		return self.call("mouse_move", args)

	def key(self, name, shift=False, ctrl=False):
		return self.call("key", {"name": name, "shift": shift, "ctrl": ctrl})

	def text(self, value):
		"""APPENDS to the focused field."""
		return self.call("text", {"value": value})

	def set_text(self, value):
		"""Replaces the focused field's contents (clear, then type)."""
		return self.call("set_text", {"value": value})

	def probe(self, x, y, space=None):
		"""{logical, ui_width, output_size, window_size, detail} - `detail` is empty off-world."""
		args = {"x": int(x), "y": int(y)}
		if space:
			args["space"] = space
		return self.call("probe_point", args)

	def screenshot(self, name):
		"""Save a PNG named `name` (a bare '<name>.png') under <repo>/build/screenshots/. The reply gains
		`path`, the file's full path."""
		if os.path.basename(name) != name or not name.lower().endswith(".png"):
			raise bridge_error("screenshot takes a bare '<name>.png', not a path: {}".format(name))
		reply = dict(self.call("screenshot", {"name": name}))
		reply["path"] = os.path.join(agent_mcp.screenshot_dir(), name)
		return reply

	def modifiers(self, shift=None, ctrl=None, alt=None):
		args = {key: value for key, value in (("shift", shift), ("ctrl", ctrl), ("alt", alt)) if value is not None}
		return self.call("modifiers", args)

	# --- waits --------------------------------------------------------------------------------

	def _wait_args(self, conditions, timeout_ms, since):
		if not conditions:
			raise bridge_error("wait_for needs at least one condition")
		if len(conditions) > wait_max_conditions:
			raise bridge_error("wait_for takes at most {} conditions".format(wait_max_conditions))
		if timeout_ms <= 0 or timeout_ms > wait_max_ms:
			raise bridge_error("wait_for timeout_ms must be 1..{} (got {})".format(wait_max_ms, timeout_ms))
		args = {"conditions": list(conditions), "timeout_ms": int(timeout_ms)}
		if since is not None:
			args["since"] = int(since)
		return args

	def wait_for(self, conditions, timeout_ms, since=None):
		"""Block until ANY condition holds or the timeout passes. Event conditions match only events
		recorded after the wait is registered - put the wait BEFORE its trigger (wait_for_then)."""
		args = self._wait_args(conditions, timeout_ms, since)
		return wait_result(self.call("wait_for", args, timeout_s=timeout_ms / 1000.0 + wait_slack_s))

	def wait_for_then(self, conditions, timeout_ms, cmd, args=None):
		"""Register the wait, THEN send the action, then collect the wait. Returns (wait_result, action
		result). Both lines leave in one order on one socket, so the event cannot slip past the wait."""
		pending = self.submit("wait_for", self._wait_args(conditions, timeout_ms, None))
		action = self.call(cmd, args)
		return wait_result(pending.result(timeout_ms / 1000.0 + wait_slack_s)), action

	def wait_event(self, channel, event_type=None, match=None, timeout_ms=10000):
		return self.wait_for([event_condition(channel, event_type, match)], timeout_ms)

	def wait_screen(self, name, timeout_s=20.0, settled=True):
		"""Wait until get_state reports `name` and (settled) no screen transition is running."""
		deadline = time.monotonic() + timeout_s
		state = self.state()
		while state.get("screen") != name:
			remaining_ms = int((deadline - time.monotonic()) * 1000)
			if remaining_ms <= 0:
				raise common.flow_error("screen '{}' not reached within {:g}s (at '{}')".format(
					name, timeout_s, state.get("screen")))
			outcome = self.wait_for([state_condition("screen", name)], min(remaining_ms, wait_max_ms))
			state = outcome.state or self.state()
		while settled and state.get("transitioning"):
			if time.monotonic() >= deadline:
				raise common.flow_error("screen '{}' still transitioning after {:g}s".format(name, timeout_s))
			time.sleep(poll_interval_s)
			state = self.state()
		if state.get("screen") != name:
			raise common.flow_error("screen changed away from '{}' while settling (now '{}')".format(
				name, state.get("screen")))
		return state

	def wait_widget(self, widget_id, timeout_s=10.0, enabled=True):
		"""Poll hb_widgets until `widget_id` is present (and enabled, unless enabled=False)."""
		def found():
			widget = self.widget(widget_id)
			if widget is None:
				return None
			if enabled and not widget.get("enabled", True):
				return None
			if widget.get("visible") is False:
				return None  # scrolled out of its window's viewport: present, but a click would not reach it
			return widget
		widget = common.poll_until(found, timeout_s, poll_interval_s)
		if not widget:
			raise common.flow_error("widget '{}'{} did not appear within {:g}s on '{}'".format(
				widget_id, " (enabled)" if enabled else "", timeout_s, self.screen()))
		return widget

	# --- journal ------------------------------------------------------------------------------

	def journal(self, channels=None, types=None, exclude_types=None, max_events=None, after_seq=None,
	            before_seq=None, from_ms=None, to_ms=None, object_id=None, message_contains=None):
		"""One NON-consuming journal_query: live (newest first) with no window, history with one."""
		args = {}
		for key, value in (("max", max_events), ("after_seq", after_seq), ("before_seq", before_seq),
		                   ("from_ms", from_ms), ("to_ms", to_ms), ("object_id", object_id)):
			if value is not None:
				args[key] = int(value)
		if message_contains:
			args["message_contains"] = str(message_contains)
		for key, value in (("channels", channels), ("types", types), ("exclude_types", exclude_types)):
			if value:
				args[key] = list(value)
		return self.call("journal_query", args)

	def journal_clock(self):
		"""(newest_seq, now_ms): note it before an action, read after_seq=newest_seq afterwards."""
		result = self.journal(max_events=1)
		return int(result.get("newest_seq") or 0), int(result.get("now_ms") or 0)

	def newest_seq(self):
		return self.journal_clock()[0]

	def drain_journal(self, after_seq=0, channels=None, types=None):
		"""EVERY retained event after `after_seq`, OLDEST first, paging past the per-query cap.
		Non-consuming (journal_query), so the MCP streaming pump's cursor is never disturbed."""
		pages = []
		before = None
		while True:
			result = self.journal(channels=channels, types=types, max_events=journal_page_max,
			                      after_seq=after_seq, before_seq=before)
			pages.append(result.get("events") or [])
			if not result.get("more_older"):
				break
			before = result.get("older_before_seq")
		events = [event for page in pages for event in page]
		events.reverse()
		return events
