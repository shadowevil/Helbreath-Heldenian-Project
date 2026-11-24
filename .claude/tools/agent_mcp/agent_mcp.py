#!/usr/bin/env python
"""Helbreath (Heldenian) client agent bridge - MCP server.

An out-of-process MCP server (stdlib only, no pip dependencies) that lets an agent drive and
inspect a RUNNING debug client. It speaks MCP (JSON-RPC 2.0 over stdio) to the agent host and
translates each tool call into a one-line JSON command over a localhost TCP socket to the
client's in-process agent_bridge (sources/client/agent_bridge.*, compiled only in debug builds).

Launch is handled by the host from .mcp.json; this script is never run by hand. The client must
already be running a debug build (the bridge listens on 127.0.0.1:8891 by default). If nothing is
listening, tool calls return a friendly error rather than failing hard.

SEVERAL clients at once. Each debug client is its own LISTENER on its own port
(`--agent-bridge-port=N`), and this server dials out to each one, so driving 6-8 characters at the
same time is a matter of registering their ports here - no client change. Every tool takes an
optional `client` handle; omit it and the call goes to the single registered client (or the session
default when there are several), which is exactly what it has always done.

Wire protocol to the client (newline-delimited JSON, one object each way):
    -> {"id": <n>, "cmd": "<name>", "args": {...}}
    <- {"id": <n>, "ok": true, "result": {...}}  |  {"id": <n>, "ok": false, "error": "..."}
"""

import base64
import json
import os
import re
import socket
import subprocess
import sys
import threading
import time

try:
	# Optional: only the hb_screenshot scale / region / format options need it. Everything else, and a
	# plain capture, stays stdlib-only.
	from PIL import Image
except ImportError:
	Image = None

# --- configuration -----------------------------------------------------------------------------

BRIDGE_HOST = "127.0.0.1"

# This client's bridge ports are 8891-8898 (decision d-028), one per running client. A discovery scan
# walks exactly that range, whatever the default is, so it never reaches another project's clients.
BRIDGE_PORT_FIRST = 8891
BRIDGE_PORT_LAST = 8898
DISCOVER_PORT_SPAN = BRIDGE_PORT_LAST - BRIDGE_PORT_FIRST + 1

# The port the FIRST client is expected on. A single-client session never has to think about ports at
# all: this one is registered at startup as handle "c1" (HB_AGENT_PORT moves it, and so picks which client
# an unqualified call drives; the client accepts ONE bridge connection at a time, by design).
BRIDGE_DEFAULT_PORT = int(os.environ.get("HB_AGENT_PORT", str(BRIDGE_PORT_FIRST)))

# A discovery probe waits this long for a client to answer. Shorter than an ordinary command: a port
# that is listening but is not our bridge would otherwise hold the scan for the full command timeout.
# (The probes run concurrently, so a whole scan costs one of these, not eight.)
DISCOVER_PING_SECONDS = 2.0

# How long an ordinary command may take. Generous for a per-frame reply, and unrelated to a
# wait_for's own deadline, which the caller passes explicitly.
BRIDGE_TIMEOUT_SECONDS = 5.0

# path_bench (HB-0398) holds the client's frame for every timed run: a Debug bench of the shipped A* on an
# unreachable goal runs to seconds, so it gets its own deadline. The repeats cap MATCHES the client's
# (agent_bridge: max_path_bench_repeats).
PATH_BENCH_TIMEOUT_SECONDS = 120.0
PATH_BENCH_MAX_REPEATS = 20
# press_bench (HB-0748) times one creature's eight death-pose press fields per request, the old brute force
# included: a Debug run of the largest takes seconds. The repeats cap MATCHES the client's
# (agent_bridge: max_press_bench_repeats).
PRESS_BENCH_TIMEOUT_SECONDS = 120.0
PRESS_BENCH_MAX_REPEATS = 20
# self_tests holds the client's frame while all three suites run; the player model suite decodes both models,
# about 5 s in Debug, so the default deadline would cut the reply off.
SELF_TESTS_TIMEOUT_SECONDS = 60.0
PRESS_LIVE_PATHS = ("old", "new")
# The buttons a click-shaped tool can press (the client's agent_bridge::button_names).
MOUSE_BUTTONS = ("left", "right", "middle")
# A fresh registration is never auto-dropped until its port has been seen listening once or this long has passed,
# so a client registered just before it launches survives its startup (HB-0625 review).
DEAD_CLIENT_GRACE_SECONDS = float(os.environ.get("HB_DEAD_CLIENT_GRACE_S", "60"))  # the smoke test sets 0
# inject_message's types (HB-0395), the client's closed set (agent_bridge: inject_type_*).
INJECT_TYPES = ("script_strings", "entity_subtitle_changed", "script_text", "raw")
INJECT_AUDIENCES = ("notify", "announce")

# Separate and shorter: a connect either succeeds promptly against a local listener or nothing is
# there. Folding this into the command timeout made a missing client look like a slow one.
BRIDGE_CONNECT_SECONDS = 2.0

# Must MATCH the client's own clamp (agent_bridge: max_wait_timeout_ms = 60000). Accepting more
# would be worse than rejecting it: the client would silently clamp, reply at 60s, and the caller
# would believe it had waited as long as it asked. A clear error beats a quiet lie.
WAIT_FOR_MAX_MS = 60000
WAIT_FOR_SLACK_SECONDS = 5.0

# The client rejects more than this outright; enforced here too so the error names the cap.
# hb_wait_for max_events: the most matches one call collects, and how often the journal is polled for the rest.
WAIT_FOR_MAX_EVENTS = 500
WAIT_MANY_POLL_SECONDS = 0.05
WAIT_MANY_PAGE = 200
# hb_cast_at: how long to wait for the cast to resolve after the aim (a wind-up is ~1.3 s plus the server's answer).
CAST_WAIT_DEFAULT_MS = 6000
WAIT_FOR_MAX_CONDITIONS = 16

# hb_wait_for lists this many of the MOST RECENT trail events by default; `trail` overrides (true = all,
# false = none, N = last N). A since:0 wait used to return the whole journal (596 lines, HB-0125).
WAIT_FOR_TRAIL_DEFAULT = 10

# --- journal streaming ---------------------------------------------------------------------------
# The pump polls the client's journal on this interval and republishes what it finds as MCP
# notifications. This is the ONE place polling remains, and it is the right place for it: the client
# retains events in its ring buffer, so a slow pump delays a notification but never loses an event -
# which is exactly what an agent polling get_state could not say.
JOURNAL_POLL_SECONDS = 0.25
# Matches the client's journal_default_drain_max. Affordable at this size because the client
# collapses a run of consecutive identical events into ONE row carrying `count` - a filtered
# drain of a repetitive scene measured 17,195 -> 296 characters. `max` still counts RAW events
# on the client, so this is 1000 events, not 1000 rows.
JOURNAL_DRAIN_MAX = 1000

# What the pump keeps for resources/read. The client's ring is the real buffer; this is a window on
# the recent past so a resource read does not have to consume events the transcript already showed.
# Counts ROWS, and a row may be a coalesced run, so this window now spans more real events than it
# used to for the same number.
JOURNAL_CACHE_EVENTS = 512

JOURNAL_RESOURCE_URI = "hb://journal/recent"


def screenshot_dir():
	"""Where captured frames land: <repo>/build/screenshots (resolved from this script's own location, so
	it is correct regardless of the launch cwd). The client writes into the same folder and takes a bare
	file name only; HB_SCREENSHOT_DIR exists for the fake-client tests and must match a real client's."""
	override = os.environ.get("HB_SCREENSHOT_DIR")
	if override:
		return override
	# agent_mcp.py -> agent_mcp -> tools -> .claude -> <repo root>
	here = os.path.abspath(__file__)
	repo_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(here))))
	return os.path.join(repo_root, "build", "screenshots")

# One hb_batch / hb_parallel call attaches at most this many screenshot images (HB-0119); a step past
# the cap still saves its file and says so. Images are the expensive part of a reply.
BATCH_MAX_IMAGES = 4

# hb_probe's compact reply drops these `detail` blocks (each is dozens of values a probe rarely needs);
# `full:true` returns everything. A tile_blocking block keeps its flags and loses an empty `objects` list.
PROBE_VERBOSE_KEYS = ("bubble", "teleport_smoke", "minimap_view", "walk_behind", "ground_fields")

# hb_screenshot: JPEG quality for format jpg, the smallest scale a downscale may take, the formats it
# writes, and the prefix of the raw capture the client writes (Pillow derives the requested file from it).
# The client takes a bare name made of letters, digits, '.', '_' and '-' that does not start with '.', so the
# prefix has no leading dot.
SCREENSHOT_JPEG_QUALITY = 85
SCREENSHOT_MIN_SCALE = 0.05
SCREENSHOT_FORMATS = {"png": ("png", "image/png"), "jpg": ("jpg", "image/jpeg"), "jpeg": ("jpg", "image/jpeg")}
SCREENSHOT_RAW_PREFIX = "hbraw_"
SCREENSHOT_WRITE_WAIT_S = 15.0   # the client encodes off-thread: how long to wait for the finished file (Debug is slow)
SCREENSHOT_WRITE_POLL_S = 0.01
SCREENSHOT_FAILED_SUFFIX = ".failed"  # a worker failure marker (this client encodes inline and writes none)
SCREENSHOT_REPLY_SECONDS = 15.0  # the client answers on the next presented frame, 10 s at most, so allow for that
late_captures = []  # raw paths whose wait timed out: the client may still rename them in, so the next call sweeps them
# hb_screenshot burst: the most frames one call takes, the default and the longest gap between two, the cap on
# the screenshots folder (user rule: 20 MB, hard; HB_SCREENSHOT_CAP_BYTES lowers it for tests) and the room
# one raw capture needs while it is being processed (a full-window PNG is 3.4-5.7 MB).
SCREENSHOT_BURST_MAX = 30
SCREENSHOT_BURST_INTERVAL_MS = 250
SCREENSHOT_BURST_INTERVAL_MAX_MS = 10000
SCREENSHOT_DIR_CAP_BYTES = 20 * 1024 * 1024
SCREENSHOT_RAW_HEADROOM_BYTES = 6 * 1024 * 1024
# A capture file is briefly held by the client's worker, a virus scanner or the indexer (Windows: PermissionError);
# opening, moving or removing it is retried this many times, this far apart, before it counts as a failure.
SCREENSHOT_IO_RETRIES = 8
SCREENSHOT_IO_RETRY_S = 0.1
# How many of the folder's biggest owners (a subfolder, or the loose files) a cap refusal lists.
SCREENSHOT_OWNER_ROWS = 6


def repo_root():
	"""The repository root: agent_mcp.py -> agent_mcp -> tools -> .claude -> <root>."""
	here = os.path.abspath(__file__)
	return os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(here))))


# MCP protocol version we implement; we echo the client's requested version when it sends one.
DEFAULT_PROTOCOL_VERSION = "2025-06-18"

SERVER_INFO = {"name": "helbreath-client", "version": "0.1.0"}


def log(message):
	"""Diagnostics go to stderr; stdout is reserved for JSON-RPC frames."""
	sys.stderr.write("[hb-agent-mcp] " + message + "\n")
	sys.stderr.flush()


# --- client bridge connection ------------------------------------------------------------------


class bridge_client:
	"""A lazily-(re)connected socket to the client's agent_bridge, with line framing.

	Replies are demultiplexed by their `id` on a dedicated reader thread rather than by "the next
	line is mine". That is what makes a HELD reply possible: a wait_for can sit unanswered for
	thirty seconds while a journal drain on another thread completes in the meantime, and each
	caller still gets its own reply. The single-line-at-a-time reader this replaced would have
	handed the drain's reply to whoever asked first.
	"""

	def __init__(self, host, port):
		self.host = host
		self.port = port
		self._sock = None
		self._buffer = b""
		self._next_id = 1
		self._send_lock = threading.Lock()   # serializes sends and id allocation
		self._slots_lock = threading.Lock()  # guards _slots
		self._slots = {}                     # id -> [threading.Event, reply dict or None]
		self._reader = None
		self._generation = 0                 # bumped on every drop, so stale readers exit quietly

	def address(self):
		return "{}:{}".format(self.host, self.port)

	# --- connection lifecycle ---

	def _connect_locked(self):
		"""Open the socket and start a reader thread. Caller holds _send_lock."""
		if self._sock is not None:
			return
		sock = socket.create_connection((self.host, self.port), timeout=BRIDGE_CONNECT_SECONDS)
		# No socket timeout once connected: the reader thread blocks on recv indefinitely and each
		# caller enforces its OWN deadline on its event. A socket timeout here would abort a
		# legitimately long wait_for.
		sock.settimeout(None)
		self._sock = sock
		self._buffer = b""
		self._generation += 1
		self._reader = threading.Thread(
			target=self._read_loop, args=(sock, self._generation), daemon=True,
			name="hb-bridge-reader-{}".format(self.port))
		self._reader.start()
		log("connected to client bridge at {}".format(self.address()))

	def ensure_connected(self):
		"""Open the socket now if it is not already open.

		hb_parallel calls this BEFORE its barrier: a TCP handshake inside the release window would
		show up as dispatch skew that has nothing to do with the clients.
		"""
		with self._send_lock:
			self._connect_locked()

	def _drop(self, reason):
		"""Close the socket and fail every waiting caller, so nobody blocks on a dead bridge."""
		with self._send_lock:
			if self._sock is not None:
				try:
					self._sock.close()
				except OSError:
					pass
			self._sock = None
			self._buffer = b""
			self._generation += 1
		with self._slots_lock:
			pending = list(self._slots.items())
			self._slots.clear()
		for _, slot in pending:
			slot[1] = {"ok": False, "error": "bridge connection lost: {}".format(reason)}
			slot[0].set()

	def is_connected(self):
		return self._sock is not None

	# --- the reader thread ---

	def _read_loop(self, sock, generation):
		"""Read lines forever, routing each by id to the caller waiting on it."""
		buffer = b""
		try:
			while True:
				chunk = sock.recv(65536)
				if not chunk:
					raise OSError("bridge closed the connection")
				buffer += chunk
				while b"\n" in buffer:
					line, _, buffer = buffer.partition(b"\n")
					line = line.strip()
					if line:
						self._deliver(line)
		except (OSError, ValueError) as error:
			if generation == self._generation:
				self._drop(str(error))
		# a stale generation means _drop already ran for this socket; exit silently

	def _deliver(self, raw_line):
		try:
			reply = json.loads(raw_line.decode("utf-8"))
		except ValueError:
			log("dropping malformed bridge line")
			return
		reply_id = reply.get("id")
		with self._slots_lock:
			slot = self._slots.pop(reply_id, None)
		if slot is None:
			# Nobody is waiting: either the caller already timed out, or the bridge sent something
			# unsolicited. Neither is fatal - log it so it is not invisible.
			log("unmatched bridge reply id {}".format(reply_id))
			return
		slot[1] = reply
		slot[0].set()

	# --- requests ---

	def request(self, cmd, args=None, timeout=None):
		"""Send one command and return the parsed reply dict.

		`timeout` is this caller's own deadline in seconds (default BRIDGE_TIMEOUT_SECONDS). A
		wait_for passes its own, longer, one. Raises RuntimeError on transport failure or timeout so
		callers can surface a friendly message rather than an unhandled socket error.
		"""
		return self.submit(cmd, args).wait(timeout)

	def submit(self, cmd, args=None):
		"""Send one command WITHOUT waiting and return a pending_reply to collect it later.

		The line is on the socket when this returns, so a wait_for submitted here and an action sent
		after it reach the client in that order - the "register the wait, then act" pattern that a
		blocking request cannot express (hb_driver uses it). Raises RuntimeError on transport failure.
		"""
		event = threading.Event()
		slot = [event, None]
		message_id = None
		try:
			with self._send_lock:
				self._connect_locked()  # inside the lock+try so a refused connect becomes the error below
				message_id = self._next_id
				self._next_id += 1
				with self._slots_lock:
					self._slots[message_id] = slot
				payload = json.dumps({"id": message_id, "cmd": cmd, "args": args or {}}) + "\n"
				self._sock.sendall(payload.encode("utf-8"))
		except OSError as error:
			if message_id is not None:
				with self._slots_lock:
					self._slots.pop(message_id, None)
			self._drop(str(error))
			raise RuntimeError("bridge transport error: {}".format(error))
		return pending_reply(self, cmd, message_id, slot)

	def _abandon(self, message_id):
		with self._slots_lock:
			self._slots.pop(message_id, None)

	def shutdown(self):
		"""Close the socket for good (a client being forgotten). Safe to call twice."""
		self._drop("client forgotten")


class pending_reply:
	"""A command already on the wire whose reply has not been collected yet (bridge_client.submit)."""

	def __init__(self, bridge, cmd, message_id, slot):
		self._bridge = bridge
		self._cmd = cmd
		self._message_id = message_id
		self._slot = slot

	def wait(self, timeout=None):
		"""The reply dict. Same deadline and errors as bridge_client.request."""
		deadline = BRIDGE_TIMEOUT_SECONDS if timeout is None else timeout
		if not self._slot[0].wait(deadline):
			self._bridge._abandon(self._message_id)
			raise RuntimeError("bridge did not answer '{}' within {:g}s".format(self._cmd, deadline))
		reply = self._slot[1]
		if reply is None:
			raise RuntimeError("bridge reply was lost")
		return reply


# --- stdout: one writer, one lock --------------------------------------------------------------


class rpc_writer:
	"""Serializes every JSON-RPC frame written to stdout.

	Two things write now - the request path on the main thread and the journal pump on its own -
	and interleaved partial lines would corrupt the host's parse with no useful error. Every write
	goes through here; nothing else touches stdout.
	"""

	def __init__(self, stream):
		self._stream = stream
		self._lock = threading.Lock()

	def send(self, message):
		payload = (json.dumps(message) + "\n").encode("utf-8")
		with self._lock:
			try:
				self._stream.write(payload)
				self._stream.flush()
			except OSError:
				pass  # host closed stdout; main() notices on its own next read


WRITER = rpc_writer(sys.stdout.buffer)


# --- journal pump: bridge events -> MCP notifications ------------------------------------------


class journal_pump:
	"""Drains the client's event journal on an interval and republishes it to the MCP host.

	Only runs once a host has subscribed (or set a log level), so a session that never asks for
	events pays nothing and no client connection is held open needlessly.

	One pump per client: each client has its own ring buffer and its own read cursor, so a shared
	pump would have had to interleave two cursors and would have got both wrong.

	The cursor is the pump's ALONE. It is the only consumer of the client's journal_drain; every
	other read (hb_journal_drain, hb_wait_for) is a non-consuming query that neither reads nor moves
	it, so nothing has to be folded back in.
	"""

	def __init__(self, bridge, handle):
		self._bridge = bridge
		self._handle = handle
		self._thread = None
		self._stop = threading.Event()
		self._lock = threading.Lock()
		self._cursor = None
		self._recent = []       # the JOURNAL_CACHE_EVENTS most recent events, oldest first
		self._dropped_total = 0
		self._subscribed = False
		self._notify_events = False
		# The subscription: which channels survive, and the grep filter over the rendered line.
		self._channels = []      # empty = every channel
		self._filter = None      # compiled regex, keep-if-matches
		self._exclude = None     # compiled regex, drop-if-matches (wins over _filter)

	# --- control ---

	def set_subscription(self, channels, filter_pattern, exclude_pattern):
		"""Install the channel selection and grep filter. Compiled here so a bad pattern is a tool
		error at the call site rather than a silently-empty stream later."""
		compiled_filter = re.compile(filter_pattern, re.IGNORECASE) if filter_pattern else None
		compiled_exclude = re.compile(exclude_pattern, re.IGNORECASE) if exclude_pattern else None
		with self._lock:
			self._channels = list(channels or [])
			self._filter = compiled_filter
			self._exclude = compiled_exclude

	def describe_subscription(self):
		with self._lock:
			return {
				"channels": list(self._channels) or ["*"],
				"filter": self._filter.pattern if self._filter else None,
				"exclude": self._exclude.pattern if self._exclude else None,
				"streaming": self._notify_events,
				"subscribed_resource": self._subscribed,
			}

	def _passes(self, event):
		"""Caller holds _lock. Channel selection first, then exclude, then filter."""
		if self._channels:
			channel = event.get("channel", "")
			if not any(channel_matches(selector, channel) for selector in self._channels):
				return False
		if self._exclude is None and self._filter is None:
			return True
		subject = event_subject(event)
		if self._exclude is not None and self._exclude.search(subject):
			return False  # exclude wins on a tie: "this channel except the heartbeat"
		if self._filter is not None and not self._filter.search(subject):
			return False
		return True

	def set_streaming(self, enabled):
		"""Turn event notifications on or off. Idempotent."""
		with self._lock:
			self._notify_events = enabled
			want = self._subscribed or self._notify_events
		self._ensure_running(want)

	def set_subscribed(self, enabled):
		with self._lock:
			self._subscribed = enabled
			want = self._subscribed or self._notify_events
		self._ensure_running(want)

	def _ensure_running(self, want):
		if want and self._thread is None:
			self._stop.clear()
			self._thread = threading.Thread(
				target=self._run, daemon=True, name="hb-journal-pump-{}".format(self._handle))
			self._thread.start()
			log("journal pump started for {}".format(self._handle))
		elif not want and self._thread is not None:
			self._stop.set()
			self._thread = None
			log("journal pump stopped for {}".format(self._handle))

	def is_running(self):
		return self._thread is not None

	def shutdown(self):
		"""Stop the pump thread (a client being forgotten)."""
		self._stop.set()
		self._thread = None

	# --- the cache, for resources/read ---

	def snapshot(self):
		with self._lock:
			return {
				"cursor": self._cursor,
				"dropped_total": self._dropped_total,
				"streaming": self._notify_events,
				"subscribed": self._subscribed,
				"channels": list(self._channels) or ["*"],
				"filter": self._filter.pattern if self._filter else None,
				"exclude": self._exclude.pattern if self._exclude else None,
				"events": list(self._recent),
			}

	# --- the loop ---

	def _run(self):
		while not self._stop.wait(JOURNAL_POLL_SECONDS):
			try:
				args = {"max": JOURNAL_DRAIN_MAX}
				with self._lock:
					if self._cursor is not None:
						args["since"] = self._cursor
				reply = self._bridge.request("journal_drain", args)
			except RuntimeError:
				continue  # no client, or it went away: try again next tick rather than dying
			if not reply.get("ok", False):
				continue
			result = reply.get("result") or {}
			events = result.get("events") or []
			dropped = int(result.get("dropped") or 0)
			with self._lock:
				if result.get("cursor") is not None:
					self._cursor = result["cursor"]
				self._dropped_total += dropped
				# The cache holds what the subscription selected, so a resources/read shows the same
				# view the notifications did rather than a second, wider one.
				passing = [event for event in events if self._passes(event)]
				if passing:
					self._recent.extend(passing)
					if len(self._recent) > JOURNAL_CACHE_EVENTS:
						del self._recent[:-JOURNAL_CACHE_EVENTS]
				notify = self._notify_events
				subscribed = self._subscribed
			events = passing
			if dropped > 0:
				# Never silent: a dropped event reads as "nothing happened", which is the one
				# failure mode this whole feature exists to remove.
				self._emit_log("warning", {
					"kind": "journal_dropped",
					"dropped": dropped,
					"note": "events were evicted from the client ring buffer before being read",
				})
			if events and notify:
				for event in events:
					self._emit_log(self._level_for(event), event)
			if events and subscribed:
				WRITER.send({
					"jsonrpc": "2.0",
					"method": "notifications/resources/updated",
					"params": {"uri": journal_uri(self._handle)},
				})

	@staticmethod
	def _level_for(event):
		"""Map an event onto an MCP log level so a host can filter by severity.

		The diagnostic channels ARE the severity, so this is a direct read rather than a peek into
		the payload - which is the point of grouping them that way.
		"""
		channel = event.get("channel") or ""
		if channel.endswith(".errors"):
			return "error"
		if channel.endswith(".warnings"):
			return "warning"
		return "info"

	def _emit_log(self, level, payload):
		# The handle is folded into the payload ONLY when there is more than one client to tell
		# apart - a single-client session's notifications stay byte-identical to what they have
		# always been, and the moment a second client exists every event says which one it is from.
		if REGISTRY.count() > 1:
			payload = dict(payload)
			payload["client"] = self._handle
		WRITER.send({
			"jsonrpc": "2.0",
			"method": "notifications/message",
			"params": {"level": level, "logger": "hb-journal", "data": payload},
		})


# --- the error tap ---------------------------------------------------------------------------------
# Failures must reach the driving agent IN THE MOMENT, attached to whatever it was doing, rather than
# waiting to be found by a later drain or a hand-read of client.log (user-directed 2026-09-03).
#
# It reads through wait_for with a zero timeout and a never-matching condition, because that path is
# NON-CONSUMING - verified against a live client. journal_drain would be catastrophic here: an
# untruncated drain sets the client's read cursor to m_next_seq and resets the dropped counter, so it
# consumes EVERY event including the ones a channel filter excluded. A tap built on it would silently
# eat the agent's own journal. (A dedicated journal_peek command would be cleaner; this works today.)
TAP_CHANNELS = ("client.errors",)
TAP_NEVER_MATCHES = {"channel": "tap.never_matches"}
TAP_MAX_REPORTED = 8


class error_tap:
	"""One per client - the cursor it holds is a position in THAT client's ring buffer."""

	def __init__(self, bridge):
		self._bridge = bridge
		self._cursor = None
		self._lock = threading.Lock()

	def arm(self):
		"""Set the cursor to now, so a tap reports only what happens from here on."""
		with self._lock:
			if self._cursor is not None:
				return
		try:
			reply = self._bridge.request("wait_for", {"conditions": [TAP_NEVER_MATCHES], "timeout_ms": 0})
		except RuntimeError:
			return
		result = reply.get("result") or {}
		with self._lock:
			self._cursor = result.get("cursor")

	def collect(self):
		"""Return error events since the last check, or [] - never raises, never blocks."""
		try:
			with self._lock:
				since = self._cursor
			args = {"conditions": [TAP_NEVER_MATCHES], "timeout_ms": 0}
			if since is not None:
				args["since"] = since
			reply = self._bridge.request("wait_for", args, timeout=BRIDGE_TIMEOUT_SECONDS)
			if not reply.get("ok", False):
				return []
			result = reply.get("result") or {}
			with self._lock:
				self._cursor = result.get("cursor", since)
			if since is None:
				return []  # first call only established the cursor
			return [event for event in (result.get("events") or [])
			        if (event.get("channel") or "") in TAP_CHANNELS]
		except Exception:
			return []  # a diagnostic must never be the thing that breaks the call it rode along with


# --- the client registry -------------------------------------------------------------------------
# A running client is a LISTENER on its own port and this server dials out to it, so N clients is N
# ports and N connections - nothing on the client side had to change. Everything that used to be a
# module-level singleton (the socket, the journal pump, the error tap) now hangs off one of these.


class unknown_client(Exception):
	"""A handle that names no registered client. Carries the message shown to the caller."""


def handle_for_port(port):
	"""The stable auto handle for a port: c1 for the default port, c2 for the next, and so on.

	Stable matters more than pretty here - c3 stays c3 whether or not c1 is running, so a handle
	written down in a test recipe keeps meaning the same client.
	"""
	offset = port - BRIDGE_DEFAULT_PORT
	if 0 <= offset < DISCOVER_PORT_SPAN:
		return "c{}".format(offset + 1)
	return "p{}".format(port)


class agent_client:
	"""One running debug client: where it listens, its connection, its pump and its error tap."""

	def __init__(self, handle, host, port, auto):
		self.handle = handle
		self.host = host
		self.port = port
		# auto = registered by the startup bootstrap or a discovery scan rather than by hand. Only
		# auto entries are pruned by a later scan; a handle someone named themselves is theirs.
		self.auto = auto
		self.registered_at = time.monotonic()
		self.seen_listening = False  # its port has listened at least once since registration
		self.commands = None         # frozenset of the wire commands its ping lists; None = not known
		self.commands_at = 0.0
		self.bridge = bridge_client(host, port)
		self.pump = journal_pump(self.bridge, handle)
		self.tap = error_tap(self.bridge)

	def address(self):
		return "{}:{}".format(self.host, self.port)

	def close(self):
		self.pump.shutdown()
		self.bridge.shutdown()


class client_registry:
	"""Which clients this session knows about, and which one an unqualified call goes to."""

	def __init__(self):
		self._lock = threading.RLock()
		self._clients = {}   # handle -> agent_client
		self._order = []     # registration order; the listing order too
		self._default = None

	def bootstrap(self):
		"""Seed the default client, so a one-client session never touches hb_clients at all."""
		self.register(handle_for_port(BRIDGE_DEFAULT_PORT), BRIDGE_HOST, BRIDGE_DEFAULT_PORT, auto=True)

	def register(self, handle, host, port, auto=False):
		with self._lock:
			existing = self._clients.get(handle)
			if existing is not None:
				if existing.host == host and existing.port == port:
					if not auto:
						existing.auto = False  # naming it by hand promotes it out of pruning
					return existing
				self._remove_locked(handle)
			client = agent_client(handle, host, port, auto)
			self._clients[handle] = client
			self._order.append(handle)
			if self._default is None:
				self._default = handle
			return client

	def _remove_locked(self, handle):
		client = self._clients.pop(handle, None)
		if client is None:
			return None
		self._order.remove(handle)
		if self._default == handle:
			self._default = self._order[0] if self._order else None
		client.close()
		return client

	def forget(self, handle):
		with self._lock:
			return self._remove_locked(handle)

	def get(self, handle):
		with self._lock:
			client = self._clients.get(handle)
			if client is not None:
				return client
			known = ", ".join("{} ({})".format(name, self._clients[name].address())
			                  for name in self._order) or "(none)"
		raise unknown_client(
			"unknown client handle '{}'. Registered: {}. Add one with hb_clients "
			"{{action:'register', client:'{}', port:<N>}}, or find the running clients with "
			"hb_clients {{action:'discover'}}.".format(handle, known, handle))

	def all(self):
		with self._lock:
			return [self._clients[name] for name in self._order]

	def find_by_address(self, host, port):
		"""The client already registered on this address, whatever handle it was given.

		A discovery scan asks this first: registering a SECOND handle for a port someone already
		named would open a second socket to a client that accepts one at a time, and the newcomer
		would sit unanswered in the accept backlog and be reported dead.
		"""
		with self._lock:
			for handle in self._order:
				client = self._clients[handle]
				if client.host == host and client.port == port:
					return client
		return None

	def count(self):
		with self._lock:
			return len(self._order)

	def default_handle(self):
		with self._lock:
			return self._default

	def set_default(self, handle):
		client = self.get(handle)
		with self._lock:
			self._default = handle
		return client

	def implicit(self):
		"""The client an omitted selector means.

		One registered client -> that one, which is the entire single-client world and must stay
		byte-for-byte what it was. Several -> the session default (the bootstrap client until
		hb_clients {action:'default'} moves it).
		"""
		with self._lock:
			if len(self._order) == 1:
				return self._clients[self._order[0]]
			if self._default is not None and self._default in self._clients:
				chosen = self._clients[self._default]
				if not chosen.seen_listening:
					# HB-0695: a default that never answered must not shadow a client that has.
					live = [self._clients[name] for name in self._order if self._clients[name].seen_listening]
					if live:
						return live[0]
				return chosen
			if self._order:
				return self._clients[self._order[0]]
		raise unknown_client(
			"no client is registered. Register one with hb_clients {action:'register', port:<N>} "
			"or scan for running clients with hb_clients {action:'discover'}.")


REGISTRY = client_registry()
REGISTRY.bootstrap()


def resolve_client(arguments, fallback=None):
	"""Pick the client a call addresses: an explicit 'client' handle, else the implicit one."""
	handle = arguments.get("client")
	if handle:
		return REGISTRY.get(str(handle))
	if fallback is not None:
		return fallback
	return REGISTRY.implicit()


def resolve_clients(arguments, fallback=None):
	"""Same, but '*' means every registered client. Only the session-state tools accept it."""
	handle = arguments.get("client")
	if handle and str(handle) == "*":
		clients = REGISTRY.all()
		if not clients:
			raise unknown_client("no client is registered, so '*' selects nothing.")
		return clients
	return [resolve_client(arguments, fallback)]


def label(client):
	"""'[c2] ' once several clients are registered, '' while there is only one.

	The single-client transcript stays exactly what it has always been - the playbook and a lot of
	muscle memory depend on that - and the moment a second client exists every line names its source.
	"""
	return "[{}] ".format(client.handle) if REGISTRY.count() > 1 else ""


def headed(client, text):
	"""Name the client on its own line above a multi-line render, leaving the render untouched."""
	if REGISTRY.count() <= 1:
		return text
	return "client {}\n{}".format(client.handle, text)


def journal_uri(handle):
	"""The per-client journal resource URI. The default client also answers the legacy bare URI."""
	return "hb://journal/{}/recent".format(handle)


# --- MCP tool definitions ----------------------------------------------------------------------

TOOLS = [
	{
		"name": "hb_batch",
		"description": "Run a SEQUENCE of client actions in ONE call. Use this by default for anything longer than a "
		               "single action - it is the difference between driving the game at human speed and at one model "
		               "round trip per click. The bridge answers each command within a frame (~16ms), so the cost of "
		               "driving the client is the number of TOOL CALLS, not the actions; ten clicks in one batch are "
		               "effectively free next to ten separate calls. Steps run in order and STOP at the first "
		               "failure, reporting which step failed and that later steps did not run. Interleave hb_wait_for "
		               "steps instead of hb_wait so the sequence advances the moment the game is ready rather than "
		               "after a padded guess. Example - the whole login, one call: click the account field, hb_set_text "
		               "the name, click password, hb_set_text the password, hb_key enter, hb_wait_for "
		               "{conditions:[{state:screen,equals:Screen_SelectCharacter}],timeout_ms:10000}, click the "
		               "slot, click Start, hb_wait_for {conditions:[{state:screen,equals:Screen_OnGame}],timeout_ms:10000}. Cannot nest, and cannot carry the streaming or "
		               "subscription tools (those are session state, not actions). A screenshot step returns its IMAGE "
		               "inline (at most 4 per batch; later ones are still saved, and the step says so), so "
		               "'look, act, look' is one call; hb_flow is batchable too (one tile step, then a "
		               "screenshot, in one call). MULTI-CLIENT: a batch runs "
		               "against ONE client - the batch's own 'client', else the default - and a step may name a "
		               "different 'client' to interleave several. Steps still run STRICTLY IN ORDER, one at a "
		               "time, so a batch spanning clients is a sequence, never simultaneous and never atomic: "
		               "use hb_parallel when two clients must act at the same moment.",
		"inputSchema": {
			"type": "object",
			"properties": {
				"steps": {
					"type": "array",
					"description": "ordered steps, each {tool: '<hb_ tool name>', arguments: {...}} - the tool "
					               "name goes under 'tool' and its own arguments under 'arguments', never at the "
					               "step's top level; with terse:true add return:true to a step whose output you want",
					"items": {
						"type": "object",
						"properties": {
							"tool": {"type": "string", "description": "the tool to run for this step"},
							"arguments": {"type": "object", "description": "that tool's own arguments"},
							"return": {"type": "boolean", "description": "terse mode only: keep this step's output"},
						},
						"required": ["tool"],
					},
				},
				"terse": {
					"type": "boolean",
					"description": "reply with only each step's status line, errors and the output of steps "
					               "marked return:true (default false: every step's full output). Screenshot "
					               "images are still attached.",
				},
			},
			"required": ["steps"],
		},
	},
	{
		"name": "hb_parallel",
		"description": "Make SEVERAL clients act at the SAME MOMENT - the two-players-onto-one-tile test and "
		               "anything else where the interleaving is the thing under test. Each group is one client "
		               "and its own steps (same step shape as hb_batch); every group's socket is connected and "
		               "warmed FIRST, then all groups are released together from a barrier and run on their own "
		               "threads. WHAT IS GUARANTEED: the first command of each group leaves this process within "
		               "a measured spread, and the reply reports that spread in ms (single-digit ms typical on "
		               "localhost) - so you get a NUMBER, not a promise. WHAT IS NOT: each client only acts when "
		               "its own next frame pumps the bridge, which adds up to one frame (~16ms at 60fps) of "
		               "independent per-client jitter, and the order the SERVER ends up processing them in is "
		               "not controlled at all. If exact server-side ordering is what you are testing, this "
		               "narrows the window - it does not decide the race. Not atomic: a group that fails stops "
		               "its own steps while the others carry on, and every group's outcome is reported.",
		"inputSchema": {
			"type": "object",
			"properties": {
				"groups": {
					"type": "array",
					"description": "one entry per client: {client: '<handle>', steps: [{tool, arguments}]}",
					"items": {
						"type": "object",
						"properties": {
							"client": {"type": "string", "description": "the client handle this group drives"},
							"steps": {"type": "array", "items": {"type": "object"}},
						},
						"required": ["client", "steps"],
					},
				},
				"clients": {
					"type": "array",
					"description": "shorthand: run the SAME top-level 'steps' on each of these handles "
					               "(use 'groups' when the steps differ per client, which they must whenever "
					               "a coordinate is involved - clients can come up at different window sizes)",
					"items": {"type": "string"},
				},
				"steps": {
					"type": "array",
					"description": "the shared steps for the 'clients' shorthand",
					"items": {"type": "object"},
				},
			},
		},
	},
	{
		"name": "hb_clients",
		"description": "Manage the set of running clients this session can drive. Each debug client listens on "
		               "its OWN port (launch extras with --agent-bridge-port N) and this server dials out to "
		               "each, so several characters can be driven at once. Actions: 'list' (default) - every "
		               "registered client, its port, whether it answers right now, and the screen it is on, "
		               "which is usually how you tell them apart; 'discover' - scan the default port and the "
		               "seven above it, register whatever answers, and drop auto-registered entries that no "
		               "longer do; 'register' {port, client?} - add one by hand (the handle defaults to c1..c8 "
		               "by port; a port another handle already holds, or that something else is connected "
		               "to, is named in the reply); 'forget' {client}; 'default' {client} - which client an "
		               "omitted selector means. Forgetting or discovering the last client leaves NOTHING "
		               "registered (no stale c1 comes back); register one to continue. With exactly one "
		               "client registered nothing needs a selector at all, which is the single-client case "
		               "unchanged.",
		"inputSchema": {
			"type": "object",
			"properties": {
				"action": {"type": "string", "enum": ["list", "discover", "register", "forget", "default"],
				           "description": "default 'list'"},
				"client": {"type": "string", "description": "handle, for register / forget / default"},
				"port": {"type": "integer", "description": "port for 'register'"},
				"host": {"type": "string", "description": "host for 'register' (default 127.0.0.1)"},
				"ports": {"type": "array", "items": {"type": "integer"},
				          "description": "explicit ports for 'discover'; omit to scan the default span"},
			},
		},
	},
	{
		"name": "hb_ping",
		"description": "Check the debug client is up and reachable through the bridge. Returns the build string.",
		"inputSchema": {"type": "object", "properties": {}},
	},
	{
		"name": "hb_self_tests",
		"description": "Run the client self-tests (content cache, item affixes, player models) once and return per "
		               "suite the passed / total count and every FAIL line. They no longer run at world entry; call "
		               "this after a client build, once the client is in the world (archives and cache loaded).",
		"inputSchema": {"type": "object", "properties": {}},
	},
	{
		"name": "hb_get_state",
		"description": "Read the live client state: which screen is active (e.g. Screen_Login, Screen_OnGame), "
		               "whether a fade transition is in flight, cursor visibility, and the tracked mouse position. "
		               "In the world it also reports character (the character name) and object_id; both are "
		               "absent on a menu screen, so they identify which character a client is playing. In the "
		               "world it also carries spawned (the first world frame has run, so a probe is meaningful), "
		               "sky_time01, map {id, name, light, weather}, map_id (flat, for a wait_for condition), "
		               "modal_open and windows[] (the open dialogs), audio (master_muted and the three volumes, one "
		               "line) and memory (working set, commit, clip / tile-sheet / minimap caches in MB, one "
		               "line). The DEFAULT reply is a compact summary: scalars a few per line, each nested value on one "
		               "short line (a window list as its names), memory left out; verbose:true gives the "
		               "indented JSON with audio / memory folded, raw:true the whole JSON.",
		"inputSchema": {"type": "object", "properties": {
			"verbose": {"type": "boolean", "description": "the full indented state (audio and memory one line each)"},
			"raw": {"type": "boolean", "description": "the client's full JSON, audio and memory as objects"}}},
	},
	{
		"name": "hb_probe",
		"description": "Diagnostic: resolve a point and report what is under it - the logical coord, the "
		               "output_size vs window_size (DPI factor; output larger when the display is scaled), "
		               "ui_width, and in-world the world tile, the picked entity_id (creature_id only when it is "
		               "not a player), attackability ("
		               "attackable), the player's tile/stance, and HUD/stance-button hit-tests. Use to verify "
		               "coordinate translation and to aim clicks precisely. The reply is COMPACT: one JSON "
		               "line without the verbose blocks (bubble, teleport_smoke, minimap_view, walk_behind, "
		               "ground_fields) and without the client's frame-time report; full:true returns every "
		               "block, frame_times:true the frame-time report. A point a window, modal, bar or map "
		               "takes from the world adds a COVERED line (the probe's front / covered_by). player_motion: the "
		               "own body's action id (4 = run), moving, slide_done, move_goal, path_left (HB-0820).",
		"inputSchema": {
			"type": "object",
			"properties": {
				"x": {"type": "integer"},
				"y": {"type": "integer"},
				"space": {"type": "string", "enum": ["logical", "window"], "description": "coordinate space of x,y (default logical)"},
				"frame_times": {"type": "boolean", "description": "include the client's frame-time report (default false)"},
				"sections": {"type": "array", "items": {"type": "string"},
				             "description": "with frame_times: keep only these named sections of the report (e.g. ['render']); "
				                            "an unknown name is listed in the reply. Implies frame_times"},
				"full": {"type": "boolean", "description": "include the verbose detail blocks and draw_stats too (default false)"},
				"draw_stats": {"type": "boolean", "description": "include the renderer's draw-call counters (default false)"},
			},
			"required": ["x", "y"],
		},
	},
	{
		"name": "hb_widgets",
		"description": "List what is on screen and CLICKABLE - every interactive widget of the active screen "
		               "and any open dialog, each with a stable id, its visible label, its logical bounds and "
		               "whether it is enabled. Use this instead of remembered coordinates: pair it with "
		               "hb_click_widget and a menu button is clicked by NAME, so a moved button stops silently "
		               "breaking a script. Ordered front to back, in the order a click is hit-tested (modal "
		               "dialogs, then normal dialogs in z-order, then the screen), so the first match for a "
		               "point is what a click would actually reach. Bounds are LOGICAL coords - a dialog's are "
		               "already mapped out of its own scaled space, so they are clickable as reported. "
		               "A MASKED field reports 'value' as asterisks of the right length, never the "
		               "plaintext - the length is what tells you a set_text landed, and a live password "
		               "has no business in a tool result. TO FIND ONE widget (there is no hb_find): pass "
		               "'prefix' (id prefix), 'screen' (owner name) or 'contains' (id or label text) so "
		               "the reply is a few rows, not the whole screen.",
		"inputSchema": {"type": "object", "properties": {
			"screen": {"type": "string", "description": "only widgets whose owner (screen or dialog name) contains "
			                                            "this text, e.g. 'login' or 'inventory'"},
			"prefix": {"type": "string", "description": "only widgets whose id starts with this, e.g. 'settings.'"},
			"contains": {"type": "string", "description": "only widgets whose id or label contains this text"},
		}},
	},
	{
		"name": "hb_screenshot",
		"description": "Capture the current client frame: returns it as an image AND auto-saves it under "
		               "build/screenshots (a full 1920x1080 PNG is 4-6 MB against a 20 MB folder cap, so crop "
		               "or shrink what you keep). Options: 'name' labels the file (else a timestamp); 'path' "
		               "saves to exactly that file (relative = under build/screenshots, absolute must be inside "
		               "the repo; an existing file is overwritten); 'region' {x,y,w,h} crops the capture in "
		               "OUTPUT pixels (the same pixels a space:'window' click uses); 'scale' (0.05-1) shrinks "
		               "it with a Lanczos filter, after the crop; 'format' png (default) or jpg; 'quality' "
		               "1-95 for jpg (default 85); 'inline' false saves without returning the image. "
		               "region / scale / format / quality need the Pillow package on the MCP host: without "
		               "it they return an error and a plain capture still works. 'burst' N (2..30) with "
		               "'interval_ms' takes N captures back to back with every option applied, saves EVERY file "
		               "as <name>_01, _02, ... and returns at most %d images (the first, the last and evenly "
		               "spaced ones between; inline false returns none), listing each file with its offset in "
		               "ms. The folder is capped at 20 MB: a burst is REFUSED up front when the folder has no "
		               "room for a raw capture, and refused after the first frame when N frames of that size "
		               "would not fit (its file is removed) - use jpg, scale or region to make frames "
		               "small." % BATCH_MAX_IMAGES,
		"inputSchema": {
			"type": "object",
			"properties": {
				"name": {"type": "string", "description": "optional filename stem (no extension); default is a timestamp"},
				"path": {"type": "string", "description": "exact file to save (png / jpg / jpeg extension); "
				                                          "relative = under build/screenshots"},
				"region": {"type": "object", "description": "crop box in output pixels: {x, y, w, h}",
				           "properties": {"x": {"type": "integer"}, "y": {"type": "integer"},
				                          "w": {"type": "integer"}, "h": {"type": "integer"}}},
				"scale": {"type": "number", "description": "0.05-1; Lanczos downscale applied after the crop"},
				"format": {"type": "string", "enum": ["png", "jpg"], "description": "default png"},
				"quality": {"type": "integer", "description": "jpg quality 1-95 (default 85)"},
				"inline": {"type": "boolean", "description": "return the image in the reply (default true)"},
				"burst": {"type": "integer", "description": "take this many captures in a row (2..{}); see the "
				                                           "description".format(SCREENSHOT_BURST_MAX)},
				"interval_ms": {"type": "integer", "description": "burst: start-to-start gap between captures "
				                                                  "(default {}, 0..{}); a capture slower than "
				                                                  "this goes back to back".format(
					SCREENSHOT_BURST_INTERVAL_MS, SCREENSHOT_BURST_INTERVAL_MAX_MS)},
			},
		},
	},
	{
		"name": "hb_mouse_move",
		"description": "Move the cursor without clicking. Coords are logical (640x480 / ui_width) by default; "
		               "pass space='window' to give output-pixel coords read directly off a screenshot.",
		"inputSchema": {
			"type": "object",
			"properties": {
				"x": {"type": "integer"},
				"y": {"type": "integer"},
				"space": {"type": "string", "enum": ["logical", "window"], "description": "coordinate space (default logical)"},
			},
			"required": ["x", "y"],
		},
	},
	{
		"name": "hb_click",
		"description": "Click a position (move there, then press/release). Coords are logical (640x480 / "
		               "ui_width) by default; pass space='window' to click output pixels read off a screenshot. "
		               "In-world, a left click on ground walks there and a left click on a creature attacks it.",
		"inputSchema": {
			"type": "object",
			"properties": {
				"x": {"type": "integer"},
				"y": {"type": "integer"},
				"space": {"type": "string", "enum": ["logical", "window"], "description": "coordinate space (default logical)"},
				"right": {"type": "boolean", "description": "right button instead of left (default false); same as button:'right'"},
				"button": {"type": "string", "enum": list(MOUSE_BUTTONS), "description": "left (default), right or middle"},
				"shift": {"type": "boolean", "description": "hold shift (default false)"},
				"ctrl": {"type": "boolean", "description": "hold ctrl (default false; Alt is the crit modifier in-world)"},
			},
			"required": ["x", "y"],
		},
	},
	{
		"name": "hb_click_widget",
		"description": "Click a widget BY NAME - the id hb_widgets reports - instead of by coordinate. The id "
		               "is resolved and clicked inside the SAME client frame, so the coordinate can never be "
		               "stale the way a remembered pixel is; the reply names the point it clicked. An unknown "
		               "id is an error naming it, and so is a DISABLED widget - a click that silently does "
		               "nothing is the exact failure this removes. Prefer this over hb_click for any button, "
		               "field, tab or checkbox; hb_click remains the tool for the world and for a raw pixel.",
		"inputSchema": {
			"type": "object",
			"properties": {
				"id": {"type": "string", "description": "widget id from hb_widgets, e.g. 'login.login'"},
				"right": {"type": "boolean", "description": "right button instead of left (default false); same as button:'right'"},
				"button": {"type": "string", "enum": list(MOUSE_BUTTONS), "description": "left (default), right or middle"},
				"shift": {"type": "boolean", "description": "decorate this one click with shift (default false)"},
				"ctrl": {"type": "boolean", "description": "decorate this one click with ctrl (default false)"},
			},
			"required": ["id"],
		},
	},
	{
		"name": "hb_mouse_down",
		"description": "Press and HOLD a mouse button at a position, without releasing it. The other half of a "
		               "drag: hb_mouse_down -> hb_mouse_move (any number) -> hb_mouse_up. Coords are logical "
		               "(640x480 / ui_width) by default; pass space='window' for output pixels read off a "
		               "screenshot. The button stays down until hb_mouse_up - always release it.",
		"inputSchema": {
			"type": "object",
			"properties": {
				"x": {"type": "integer"},
				"y": {"type": "integer"},
				"space": {"type": "string", "enum": ["logical", "window"], "description": "coordinate space (default logical)"},
				"right": {"type": "boolean", "description": "right button instead of left (default false); same as button:'right'"},
				"button": {"type": "string", "enum": list(MOUSE_BUTTONS), "description": "left (default), right or middle"},
			},
			"required": ["x", "y"],
		},
	},
	{
		"name": "hb_mouse_up",
		"description": "Release a held mouse button at a position (moves there first, so this is the last leg "
		               "of a drag). Pair it with hb_mouse_down; the button it releases must be the one pressed. "
		               "Coords are logical by default; pass space='window' for output pixels off a screenshot.",
		"inputSchema": {
			"type": "object",
			"properties": {
				"x": {"type": "integer"},
				"y": {"type": "integer"},
				"space": {"type": "string", "enum": ["logical", "window"], "description": "coordinate space (default logical)"},
				"right": {"type": "boolean", "description": "right button instead of left (default false); same as button:'right'"},
				"button": {"type": "string", "enum": list(MOUSE_BUTTONS), "description": "left (default), right or middle"},
			},
			"required": ["x", "y"],
		},
	},
	{
		"name": "hb_key",
		"description": "Press a named key. Names (case-insensitive): up down left right enter escape backspace "
		               "delete home end tab space insert page_up page_down, the letters a-z, the digits 0-9, "
		               "f1-f12, plus minus, and the modifier keys shift ctrl alt. Aliases: return = enter, "
		               "esc = escape, del = delete, pgup / pgdn = page_up / page_down, '+' and '-' = plus "
		               "and minus.",
		"inputSchema": {
			"type": "object",
			"properties": {
				"name": {"type": "string", "description": "key name (see description)"},
				"shift": {"type": "boolean"},
				"ctrl": {"type": "boolean"},
			},
			"required": ["name"],
		},
	},
	{
		"name": "hb_modifiers",
		"description": "HOLD shift, ctrl or alt down across later commands - the only way to reach held-modifier "
		               "behaviour, since the client has no key-up event and the per-frame refresh from the real "
		               "keyboard overwrites anything a single event carries. This is what makes the Alt critical "
		               "attack, Shift run/dash and held-Ctrl force attack testable at all. It is a LATCH ORed "
		               "over the real keyboard, so a key omitted keeps the latch it had (latching alt does not "
		               "release a held shift) and false RELEASES the latch rather than forcing the key off - "
		               "someone typing on the real keyboard still works while a latch is on. Call with no "
		               "arguments to read back what is currently held. RELEASE WHAT YOU LATCH: a forgotten alt "
		               "makes every later attack critical, and the next test inherits it. hb_get_state "
		               "also reports the latch, as three FLAT keys latch_shift / latch_ctrl / latch_alt - "
		               "flat because hb_wait_for's state condition resolves a TOP-LEVEL key, so "
		               "{\"state\":\"latch_alt\",\"equals\":true} works and a nested object could not.",
		"inputSchema": {
			"type": "object",
			"properties": {
				"shift": {"type": "boolean", "description": "hold shift (true) / release the latch (false); omit to leave unchanged"},
				"ctrl": {"type": "boolean", "description": "hold ctrl (true) / release the latch (false); omit to leave unchanged"},
				"alt": {"type": "boolean", "description": "hold alt (true) / release the latch (false); omit to leave unchanged"},
			},
		},
	},
	{
		"name": "hb_text",
		"description": "Type a string of text into the focused field (APPENDS; use hb_set_text to replace).",
		"inputSchema": {
			"type": "object",
			"properties": {"value": {"type": "string", "description": "UTF-8 text to enter"}},
			"required": ["value"],
		},
	},
	{
		"name": "hb_set_text",
		"description": "Replace the focused field's contents (clear to end, then type). Use for a field that may "
		               "already hold text - e.g. a login username prefilled by 'remember username'.",
		"inputSchema": {
			"type": "object",
			"properties": {"value": {"type": "string", "description": "UTF-8 text to set"}},
			"required": ["value"],
		},
	},
	{
		"name": "hb_wheel",
		"description": "Scroll the mouse wheel at a position. Positive notches scroll up / zoom in (minimap or "
		               "camera zoom in-world); negative scroll down / zoom out. Coords logical by default; "
		               "space='window' for output pixels.",
		"inputSchema": {
			"type": "object",
			"properties": {
				"x": {"type": "integer"},
				"y": {"type": "integer"},
				"space": {"type": "string", "enum": ["logical", "window"]},
				"notches": {"type": "integer", "description": "vertical notches; + = up/zoom-in, - = down/zoom-out"},
			},
			"required": ["notches"],
		},
	},
	{
		"name": "hb_wait",
		"description": "Sleep for a number of milliseconds to let an animation or transition settle before the next "
		               "screenshot or action. Handled in this server, not the client. Prefer hb_wait_for when you are "
		               "waiting for something specific to happen - a blind sleep either wastes time or misses it.",
		"inputSchema": {
			"type": "object",
			"properties": {"ms": {"type": "integer", "description": "milliseconds to wait (max 10000)"}},
			"required": ["ms"],
		},
	},
	{
		"name": "hb_wait_for",
		"description": "Wait until something actually happens, then return with the trail of what did. Give one or "
		               "more conditions; the first to match ends the wait and is named in the reply. Two condition "
		               "forms: an EVENT condition {channel, type?, match?{key: value}} matches a journal event (a state condition "
		               "already true returns at once); a STATE condition {state, equals} matches the live value "
		               "hb_get_state would report, e.g. {\"state\":\"screen\",\"equals\":\"Screen_OnGame\"}. A "
		               "timeout is a normal outcome, not an error (timeout_ms 0 is allowed for event conditions: a look at "
		               "what already happened) - satisfied:false with the events seen meanwhile, "
		               "which is how you verify something did NOT happen. Use this instead of hb_wait + poll. The reply lists "
		               "only the last 10 events of the trail by default (the event that fired is always shown); "
		               "'trail' widens it (true = every event, N = the last N) or silences it (false). An EVENT condition also "
			               "takes object_id (an entity id) and message_contains (text) beside channel / type / match, "
			               "matched by the client: {channel:'chat.message', message_contains:'hello'}. "
			               "max_events:N turns it into a COUNT: the reply lists every matching event seen (up to "
			               "N, oldest first, with the condition that matched each) and says N of N or k of N on a "
			               "timeout - no more first-hit-only waits to re-issue. It needs event conditions only "
			               "(a state condition is refused); the first match returns at once and later ones are "
			               "read from the journal, so none is missed between calls.",
		"inputSchema": {
			"type": "object",
			"properties": {
				"conditions": {
					"type": "array",
					"description": "any-of; the first match wins and its index is reported",
					"items": {"type": "object"},
				},
				"timeout_ms": {"type": "integer",
				               "description": "how long to wait before giving up (0..{}; 0 only with event "
				                              "conditions)".format(WAIT_FOR_MAX_MS)},
				"since": {"type": "integer",
				          "description": "seq to start the reply's trail from (e.g. a newest_seq from "
				                         "hb_journal_drain, plus one); omit to start at the wait itself"},
				"trail": {"description": "trail events to list: false = none, true = all, an integer N = the "
				                         "last N (default 10)"},
				"max_events": {"type": "integer",
				               "description": "keep waiting until this many matching events have happened (or "
				                              "timeout_ms is up) and return ALL of them, instead of stopping on "
				                              "the first (1..{}; 1 or omitted = the first)".format(WAIT_FOR_MAX_EVENTS)},
			},
			"required": ["conditions", "timeout_ms"],
		},
	},
	{
		"name": "hb_journal_drain",
		"description": "Read the client's event log like an event viewer: NEWEST event at the top. With no window "
		               "it is the LIVE view - the newest 'max' events as of now. Give a window to look at "
		               "history instead - 'from_ms' / 'to_ms' (journal ms, the t_ms every event carries; the "
		               "reply's header states now t=...) and/or 'after_seq' / 'before_seq' - and omit it again "
		               "to return to live. Reading consumes NOTHING: any number of reads, filtered any way, "
		               "never hide an event from the next one, so there is no cursor to carry. To see what an "
		               "action produced, note the header's newest_seq first and read with after_seq set to "
		               "it. To page older, pass the reply's older_before_seq as before_seq. This client's "
		               "channels and event types: ui.screen (screen_changed, screen_active), ui.window "
		               "(window_opened, window_closed, overlay_opened, overlay_closed, click_consumed), state.map "
		               "(map_changed), state.player (player_entered), net.packet (packet_in, packet_out; the "
		               "message name is in data), chat.message (chat_line, chat_send), and the diagnostic "
		               "channels client.warnings / client.errors (log_line). A run of CONSECUTIVE events with "
		               "identical channel, type and data collapses into ONE row carrying 'count', 'seq_last' and "
		               "'t_ms_last' (seq is the run's oldest event); use 'count' - under a filter a run's seqs are "
		               "not contiguous. 'max' counts RAW events, not rows. If the window starts before the oldest "
		               "retained event the header says so: the ring evicts its oldest events, so very old "
		               "history can be gone.",
		"inputSchema": {
			"type": "object",
			"properties": {
				"channels": {
					"type": "array",
					"description": "restrict to these channel selectors; a selector matches by dotted-segment "
					               "prefix, so 'audio' selects audio.sound",
					"items": {"type": "string"},
				},
				"types": {"type": "array", "items": {"type": "string"},
				          "description": "only these EXACT event types, e.g. ['screen_changed','chat_line']. A "
				                         "packet's type is packet_in / packet_out; its message name is in data. "
				                         "Omit for all."},
				"exclude_types": {"type": "array", "items": {"type": "string"},
				                  "description": "drop these event types. Applied after 'types'."},
				"max": {"type": "integer",
				        "description": "how many RAW events to return, newest first (client default 200)"},
				"limit": {"type": "integer", "description": "alias of 'max' (used only when 'max' is absent)"},
				"from_ms": {"type": "integer",
				            "description": "history: only events at or after this journal t_ms"},
				"to_ms": {"type": "integer",
				          "description": "history: only events at or before this journal t_ms"},
				"after_seq": {"type": "integer",
				              "description": "only events with seq > this - pass a newest_seq noted before an "
				                             "action to see exactly what it produced"},
				"before_seq": {"type": "integer",
				               "description": "only events with seq < this - pass older_before_seq to page older"},
				"object_id": {"type": "integer",
				              "description": "only events about this entity (the client matches the event data's "
				                             "object / entity / creature / attacker / target ids)"},
				"message_contains": {"type": "string",
				                     "description": "only events whose message or text field (a packet name, a chat line) contains this"},
			},
		},
	},
	{
		"name": "hb_journal_config",
		"description": "Turn individual event classes on or off in the client's journal, or resize its ring buffer. "
		               "Use to silence a noisy class so it cannot crowd out the events you care about during a long "
		               "test. Returns the resulting configuration, so you never have to assume the write took.",
		"inputSchema": {
			"type": "object",
			"properties": {
				"channels": {
					"type": "object",
					"description": "channel selector -> enabled, e.g. {\"net\": false} to silence net.packet, or "
					               "{\"chat\": false, \"ui.window\": true}. A selector that matches no channel is "
					               "refused. Channels: ui.screen, ui.window, state.map, state.player, net.packet, "
					               "chat.message, client.warnings, client.errors",
				},
				"capacity": {"type": "integer", "description": "new ring buffer capacity in events"},
				"test_log": {"type": "string", "enum": ["warn", "error"],
				             "description": "log one line at this level, to watch it land in client.warnings / "
				                            "client.errors (a check of the log sink)"},
			},
		},
	},
	{
		"name": "hb_subscribe",
		"description": "Choose which events reach you while streaming: pick channels and grep them. Observational "
		               "channels are ui.screen, ui.window, state.map, state.player, net.packet, chat.message; "
		               "DIAGNOSTIC channels are client.warnings and client.errors - so ['client'] is a standing watch "
		               "on everything the client complains about. A selector matches by dotted-segment prefix, so "
		               "'ui' takes ui.screen and ui.window. 'filter' "
		               "keeps only events matching a regex and 'exclude' drops those that match (exclude wins). Both "
		               "are matched, case-insensitively, against the event's rendered line: '<channel>/<type> "
		               "<compact json data>' - so 'Screen_Login' hits an id inside the payload and "
		               "'^ui[.]screen/' anchors on the channel. Call with no arguments to clear the filter.",
		"inputSchema": {
			"type": "object",
			"properties": {
				"channels": {
					"type": "array",
					"description": "channel selectors to stream; omit or ['*'] for everything",
					"items": {"type": "string"},
				},
				"filter": {"type": "string", "description": "regex; keep only events whose rendered line matches"},
				"exclude": {"type": "string", "description": "regex; drop events whose rendered line matches"},
			},
		},
	},
	{
		"name": "hb_stream",
		"description": "Turn live event notifications on or off for this session. While on, this server drains the "
		               "client's journal continuously and republishes each event to the host as it arrives, so events "
		               "show up without you calling hb_journal_drain. The client's ring buffer is still the real "
		               "record - streaming never loses an event that a drain would have caught.",
		"inputSchema": {
			"type": "object",
			"properties": {"enabled": {"type": "boolean", "description": "true to start streaming, false to stop"}},
			"required": ["enabled"],
		},
	},
	{
		"name": "hb_reload_pivots",
		"description": "Re-read sprite-sheet PIVOTS from the watched .hba tree and apply them to the running "
		               "client's loaded sheets in place, so an anchor authored in the HBA Workbench appears "
		               "without a restart. Debug builds only. Pixels and frame rects are never touched: a sheet "
		               "whose rects or frame count changed is skipped and reported, not half-applied - that is a "
		               "texture re-upload, which this deliberately does not do. The client also watches the tree "
		               "itself every 500ms and runs this same path, so a save usually applies on its own; call "
		               "this to force a reload or to read back exactly which pivots moved.",
		"inputSchema": {
			"type": "object",
			"properties": {
				"archive": {"type": "string",
				            "description": "archive base or file name ('tiles' or 'tiles.hba'); omit for every "
				                           ".hba in the watched directory"},
				"entry": {"type": "string",
				          "description": "an exact entry path ('sprites/objects/03') or a group prefix "
				                         "('sprites/objects'); omit for every sheet"},
				"dir": {"type": "string",
				        "description": "override the watched directory; omit to use the one already resolved "
				                       "(default <asset_root>/../../stage/client/data). A directory given here "
				                       "is remembered for the auto-watch too."},
			},
		},
	},
	{
		"name": "hb_kill_player",
		"description": "TESTING KILL: ask the SERVER to kill the character being played, so that anything "
		               "gated on player death can be tested at all - the dead chat gate, the revive flow, "
		               "corpse rendering for observers, the death UI. Without it none of that is reachable: "
		               "a real death needs a creature's lethal blow, and the town spawn is a multi-map walk "
		               "from anything that can land one. Debug builds only, on both sides. The client sets "
		               "NOTHING locally - it sends a request and the server kills through its ordinary "
		               "kill_player path, so death arrives by exactly the route a real death uses. Only "
		               "works in the entered world (a corpse asking again is ignored by the server). "
		               "Returns as soon as the request is SENT: the death itself lands a round-trip later, "
		               "so observe it with hb_journal_drain / hb_screenshot after a short hb_wait.",
		"inputSchema": {"type": "object", "properties": {}},
	},
	{
		"name": "hb_entities",
		"description": "List what is around the player, NEAREST FIRST: players, NPCs, creatures (dead ones flagged) and "
		               "ground items, each with its object_id (an item has item_id instead), name, tile, "
		               "distance, whether it is on screen, and the point (logical + window pixels) where it "
		               "draws. One compact line per entity (raw:true for the JSON); max defaults to 25. "
		               "Items are never clicked by entity: stand on the tile (hb_walk_to). Pair with "
		               "hb_click_entity. The reply also gives player_tile, map, total and truncated.",
		"inputSchema": {
			"type": "object",
			"properties": {
				"kinds": {"type": "array", "items": {"type": "string", "enum": ["player", "npc", "creature", "item"]},
				          "description": "only these kinds (default: all four)"},
				"radius": {"type": "integer", "description": "only entities within this many tiles (Chebyshev)"},
				"max": {"type": "integer", "description": "at most this many rows, nearest first (default 25)"},
				"on_screen": {"type": "boolean", "description": "only entities inside the view"},
				"raw": {"type": "boolean", "description": "return the client's JSON instead of the compact lines"},
			},
		},
	},
	{
		"name": "hb_click_entity",
		"description": "Click an entity BY object_id (from hb_entities) instead of by pixel: the point it draws at "
		               "is read from hb_entities, then clicked with space:'window'. REFUSED when it is off screen "
		               "or is a ground item. The reply says what consumed the click. Two bridge commands, so a "
		               "fast mover can step off its point in between; re-run it.",
		"inputSchema": {
			"type": "object",
			"properties": {
				"object_id": {"type": "integer", "description": "the entity's object_id from hb_entities"},
				"right": {"type": "boolean", "description": "right button instead of left (default false)"},
				"shift": {"type": "boolean", "description": "hold shift (default false)"},
				"ctrl": {"type": "boolean", "description": "hold ctrl (default false)"},
			},
			"required": ["object_id"],
		},
	},
	{
		"name": "hb_tile_to_screen",
		"description": "Where a map TILE is on screen: its window and logical pixel, whether it is in bounds / "
		               "on screen, and - when on screen - which tile a click there resolves to (round_trip), what "
		               "is in front of it (world, a window, the bar ...), whether it is the player's own sprite, "
		               "and any entity there. The exact inverse of hb_probe; use it to aim instead of measuring "
		               "a pitch.",
		"inputSchema": {
			"type": "object",
			"properties": {
				"x": {"type": "integer", "description": "map tile x"},
				"y": {"type": "integer", "description": "map tile y"},
				"raw": {"type": "boolean", "description": "return the client's JSON"},
			},
			"required": ["x", "y"],
		},
	},
	{
		"name": "hb_walk_to",
		"description": "Walk to a map TILE by the client's own pathing (the order a ground click gives), with no "
		               "pixel to aim: it works for a tile off screen. With wait:true it blocks until the walk "
		               "settles and reports status arrived, blocked (with a reason), timeout or superseded; "
		               "otherwise it returns at once with status walking. Refused with a named reason when the "
		               "player is dead, teleporting, knocked back or busy, or the tile is outside the map. A "
		               "timeout stops the walk where the player stands. It is not a click: to test what a real "
		               "click does, use hb_click.",
		"inputSchema": {
			"type": "object",
			"properties": {
				"x": {"type": "integer", "description": "map tile x"},
				"y": {"type": "integer", "description": "map tile y"},
				"wait": {"type": "boolean", "description": "block until the walk settles (default false)"},
				"timeout_ms": {"type": "integer", "description": "with wait: how long to wait (default 15000, "
				                                                 "max {})".format(WAIT_FOR_MAX_MS)},
			},
			"required": ["x", "y"],
		},
	},
	{
		"name": "hb_set_time_of_day",
		"description": "Force the client's sky clock for look-and-light tests: t01 in [0, 1) (0 and 1 are "
		               "midnight, 0.5 noon) holds the sky at that time; 'off' returns to the server's clock. "
		               "Client-side only (the server's world clock is untouched). Replies with the override, "
		               "the sky_time01 now drawn and the map's light mode (a map that does not follow the "
		               "cycle ignores the sky time). hb_get_state reports sky_time_override.",
		"inputSchema": {
			"type": "object",
			"properties": {
				"t01": {"description": "a number 0..1, or the string 'off'"},
			},
			"required": ["t01"],
		},
	},
	{
		"name": "hb_light_clock",
		"description": "Pin the client's lighting clock for a deterministic light capture (HB-0720): lamp flicker, "
		               "sway and the light-frame time (a campfire's frame) all read this one clock; while pinned the grass "
		               "wind is held too and grass presses (body and trail) freeze, so two pinned captures match pixel for "
		               "pixel (HB-0818). seconds = a number "
		               "pins it there; null (or 'off') unpins it. Replies pinned (bool) and seconds. "
		               "Client-side only, debug builds only; needs the entered world. The reply also carries ambient_seconds.",
		"inputSchema": {
			"type": "object",
			"properties": {
				"seconds": {"description": "a number of seconds to pin the clock at, or null / 'off' to unpin"},
			},
			"required": ["seconds"],
		},
	},
	{
		"name": "hb_reload_lights",
		"description": "Re-read the light emitters now (HB-0720, debug builds only): the client's live override file "
		               "bin/client/dev/light_emitters.json when it exists, else the archive's data/light_emitters.json. "
		               "The client also polls the override every 500 ms, so this just makes it immediate and reports. "
		               "Works on any screen. Replies source (override | archive), override_path, objects, tiles, "
		               "map_spots, lights, skipped, warnings and generation, or ok:false with an error.",
		"inputSchema": {"type": "object", "properties": {}},
	},
	{
		"name": "hb_set_god",
		"description": "Turn bot god mode on or off for this attached testing agent (HB-0724): the client folds it "
		               "into the agent_flags it tells the server, which refuses god without the bot flag and drops it "
		               "when the agent detaches. Kept across world re-entry and a logout while attached. Works on any "
		               "screen: before the world is entered it stores the wish and replies sent:false. Replies with "
		               "god, the flags now wanted and whether they were sent. hb_get_state reports agent_god "
		               "in the world. Debug only.",
		"inputSchema": {
			"type": "object",
			"properties": {
				"on": {"type": "boolean", "description": "true = god mode on, false = off"},
			},
			"required": ["on"],
		},
	},
	{
		"name": "hb_set_staff_tags",
		"description": "Show or hide the GM and bot tags (HB-0871): the orange GM / bot tag drawn over a character in "
		               "the world, and the tag leading a GM or bot chat line. Hide them before a screenshot so they "
		               "do not cover what is being shot. Client-local, default shown, needs the entered world. "
		               "Replies visible. Debug only.",
		"inputSchema": {
			"type": "object",
			"properties": {
				"visible": {"type": "boolean", "description": "true = tags shown (default), false = hidden"},
			},
			"required": ["visible"],
		},
	},
	{
		"name": "hb_close_window",
		"description": "Close an open dialog by name (the 'owner' of its rows in hb_widgets, e.g. 'inventory'), by "
		               "integer dialog_id, or 'all' (every window that can close; the always-on HUD stays). Replies "
		               "the windows it really closed. A name that is not a dialog is refused. Needs the entered "
		               "world. Debug only.",
		"inputSchema": {
			"type": "object",
			"properties": {"name": {"type": "string", "description": "dialog name, or 'all'"},
			               "dialog_id": {"type": "integer", "description": "dialog id, instead of a name"}},
		},
	},
	{
		"name": "hb_drag_slot",
		"description": "Drag an inventory item BY SLOT, aimed at the item's own art: hb_widgets reports an 'aim' "
		               "point on a solid pixel of the art that the real pick resolves to THIS item (a press on a "
		               "transparent pixel of the slot's box drags the window instead). The tool presses there, "
		               "moves in 'steps' legs, and releases at the drop: 'to_x'/'to_y' (logical), or 'to_widget' "
		               "(another widget id; its centre). An item with no reachable aim point is an error naming it. "
		               "One call replaces down / move / up and the slot lookup. Batchable.",
		"inputSchema": {
			"type": "object",
			"properties": {
				"from": {"type": "string", "description": "the slot widget id from hb_widgets, e.g. 'slot3'"},
				"to_x": {"type": "integer", "description": "drop x, logical"},
				"to_y": {"type": "integer", "description": "drop y, logical"},
				"to_widget": {"type": "string", "description": "drop onto this widget's centre instead of to_x/to_y"},
				"steps": {"type": "integer", "description": "move legs between press and release (default 4)"},
			},
			"required": ["from"],
		},
	},
	{
		"name": "hb_send_chat",
		"description": "Send a chat line or chat command reliably: opens the chat box when open is true "
		               "(Enter), CLEARS whatever it already holds (hb_text would append to it), types 'value', "
		               "and presses Enter. Omit open when the box is already open. submit:false stops before "
		               "the Enter. The server's reply to a command arrives as a chat_command_reply journal event "
		               "carrying the shown text (HB-0718). Batchable.",
		"inputSchema": {
			"type": "object",
			"properties": {
				"value": {"type": "string", "description": "the line, e.g. a slash command or plain chat"},
				"open": {"type": "boolean", "description": "press Enter first to open the chat box (default false)"},
				"submit": {"type": "boolean", "description": "press Enter after typing (default true)"},
			},
			"required": ["value"],
		},
	},
	{
		"name": "hb_set_tab_width_cap",
		"description": "Cap the width every dialog's bottom tab strip may use (HB-0794), so the tab overflow arrows "
		               "can be exercised with today's dialogs. Omit width to lift the cap. Hidden tabs are reported by "
		               "hb_widgets with visible=false; the arrows as the tab_previous / tab_next buttons while they "
		               "show. Debug only.",
		"inputSchema": {
			"type": "object",
			"properties": {
				"width": {"type": "integer", "description": "the cap in logical pixels, at least 200 (room for both "
				                                            "arrows and a tab); omit to lift it"},
			},
		},
	},
	{
		"name": "hb_fishing_auto_strike",
		"description": "Arm a strike of the own fishing bobber (HB-0715) for a deterministic test: on the next pull "
		               "(a catch, when struck inside the window) or the next nibble (a spook), after delay_ms. One shot: "
		               "it disarms once it fires. A pull window is 450-1050 ms, too short for a model round trip, so "
		               "this is how a test lands a catch. Needs an armed rod (use the rod item first); 'off' drops it. "
		               "hb_get_state's fishing object reports auto_strike, the line, the bobber's click point, a "
		               "castable spot in reach (cast_target) and the last result. Debug only.",
		"inputSchema": {
			"type": "object",
			"properties": {
				"on": {"type": "string", "enum": ["pull", "nibble", "off"],
				       "description": "the event to strike on, or off"},
				"delay_ms": {"type": "integer", "minimum": 0, "maximum": 5000,
				             "description": "ms after that event (default 0)"},
			},
			"required": ["on"],
		},
	},
	{
		"name": "hb_path_bench",
		"description": "Time the click-to-move pathfinder candidates (HB-0398) on the LIVE map from where the player "
		               "stands: a short and a long walk, a click on ground nothing can reach and a click on a creature, "
		               "each under A* (shipped), A* flat and Jump Point Search, with the reachability pre-check off "
		               "then on. Replies with a table (first run ms - a cold scratch -, warm median ms, nodes expanded, "
		               "tiles scanned, steps, path cost, the pre-check's verdict) and the rows whose path cost or end "
		               "differs from the shipped A*'s (the correctness check). 'fixture' (a path inside the repo, e.g. "
		               ".claude/scratch/client/default.fixture) also saves the timed map, for the path_bench "
		               "tool to replay in Release. Debug only; the client's frame is held while it runs.",
		"inputSchema": {
			"type": "object",
			"properties": {
				"fixture": {"type": "string", "description": "save the timed map here (repo-relative or absolute, "
				                                              "inside the repo)"},
				"repeats": {"type": "integer", "description": "timed runs per row, 1..{} (default 3)".format(
					PATH_BENCH_MAX_REPEATS)},
				"raw": {"type": "boolean", "description": "reply with the client's JSON instead of the table"},
			},
		},
	},
	{
		"name": "hb_press_bench",
		"description": "Time the grass press field of every creature's LAST death frame (HB-0748), all 8 directions: "
		               "the shipped distance transform against the old brute force, with the largest difference "
		               "between the two fields. Replies with one row per creature, sorted by silhouette size (largest "
		               "mask, cells, edge pixels, death frames, worst and median new and old microseconds per shape, the "
		               "speed-up, max strength / push difference and the cells differing). 'display_id' benches one "
		               "creature; omitted, every registered creature, one request each. 'live_path' old / new instead "
		               "switches what the world presses with: old = every death frame its own shape, built by the brute "
		               "force (the pre-HB-0748 path, for an A/B); new = the shipped. Debug only; the client's frame is "
		               "held while a creature is benched.",
		"inputSchema": {
			"type": "object",
			"properties": {
				"display_id": {"type": "integer", "description": "bench only this creature (entity registry id)"},
				"repeats": {"type": "integer", "description": "timed runs per shape and build, the median kept, "
				                                              "1..{} (default 3)".format(PRESS_BENCH_MAX_REPEATS)},
				"live_path": {"type": "string", "enum": list(PRESS_LIVE_PATHS),
				              "description": "switch the world's press path instead of benching"},
				"raw": {"type": "boolean", "description": "reply with the client's JSON instead of the table"},
			},
		},
	},
	{
		"name": "hb_inject",
		"description": "Dispatch server messages built by the client's own protocol serializers as if the world socket "
		               "had read them (HB-0395), to test what the client does with content nothing in play sends yet. "
		               "type 'script_strings' {rows: [{id, text}]} replaces the whole script-string table, as the "
		               "server's cache update does; 'entity_subtitle_changed' {object_id, slot, subtitle_id, colour?: "
		               "[r,g,b]} (no colour = the default gold); 'script_text' {string_id, audience: 'notify' | "
		               "'announce'}; 'raw' {id, payload?} sends message id `id` (u16, protocol.h message_id) then "
		               "the payload hex bytes exactly as given, unchecked, so a malformed message (a bad count, a "
		               "trailing byte) reaches the real parser. The messages run on the next pump. Replies "
		               "{type, queued}.",
		"inputSchema": {
			"type": "object",
			"properties": {
				"type": {"type": "string", "enum": list(INJECT_TYPES)},
				"rows": {"type": "array", "description": "script_strings: [{id, text}, ...]"},
				"object_id": {"type": "integer", "description": "entity_subtitle_changed: the entity"},
				"slot": {"type": "integer", "description": "entity_subtitle_changed: the subtitle slot"},
				"subtitle_id": {"type": "integer", "description": "entity_subtitle_changed: the script string id"},
				"colour": {"type": "array", "description": "entity_subtitle_changed: [r, g, b] (optional)"},
				"string_id": {"type": "integer", "description": "script_text: the script string id"},
				"audience": {"type": "string", "enum": list(INJECT_AUDIENCES)},
				"id": {"type": "integer", "description": "raw: the message id (u16), e.g. 512 = entity_motion_bundle"},
				"payload": {"type": "string", "description": "raw: the body after the id, hex, two digits a byte"},
			},
			"required": ["type"],
		},
	},
	{
		"name": "hb_cast_at",
		"description": "Cast a spell AT AN ENTITY by id (from hb_entities) - the two clicks of a cast without aiming "
		               "a tile: arm the spell, resolve the entity's CURRENT window point through `entities`, click it "
		               "(refused naming blocked_by when it is not clickable), then wait for the cast to resolve. "
		               "'spell' is the spell book row - its internal name ('heal', 'energy_bolt'), 'spell_heal' or "
		               "the label ('Heal'); the book is opened, the right circle page found, the row clicked and the "
		               "book closed again (it would otherwise cover the aim point). Omit 'spell' to use the spell "
		               "already armed, but only when hb_get_state reports it (armed_spell); a client that does not "
		               "say cannot be asked, so pass 'spell' there. Do not pass 'spell' for one that is already "
		               "armed: a second click on its row may disarm it. The reply names whether the arm was "
		               "confirmed (visual.effect spell_cast_begin and its cast_ms), what consumed the aim click, and "
		               "the cast's journal events: spell_release_sent, spell_cast_resolved, spell_hit on the target "
		               "(LANDED), spell_cast_cancelled, cast_click_ignored, the server's notice (a refused aim - "
		               "out of range, no valid target - leaves the spell armed). A refusal or a timeout is a "
		               "result, not an error.",
		"inputSchema": {
			"type": "object",
			"properties": {
				"object_id": {"type": "integer", "description": "the target entity (hb_entities); ids go stale, so "
				                                                "prefer 'name'"},
				"name": {"type": "string", "description": "target by entity name, resolved AT CALL TIME to the "
				                                          "nearest match (exact, else contains; no ground items). "
				                                          "Used when object_id is absent or no longer known"},
				"spell": {"type": "string", "description": "spell book row to arm (omit to use the armed one)"},
				"wait": {"type": "boolean", "description": "false = click the aim and return at once, without waiting "
				                                           "for the cast to resolve, so a burst can capture the "
				                                           "charge (default true)"},
				"timeout_ms": {"type": "integer", "description": "how long to wait for the cast to resolve after the "
				                                                 "aim (default {}, max {})".format(
					CAST_WAIT_DEFAULT_MS, WAIT_FOR_MAX_MS)},
			},
		},
	},
	{
		"name": "hb_set_audio",
		"description": "Set the client's live audio volumes: master / music / effects (whole numbers 0..100). With NO "
		               "arguments it reads back. Replies with what is now set. It changes the running mixer only; "
		               "settings.json is not written.",
		"inputSchema": {
			"type": "object",
			"properties": {
				"master": {"type": "integer", "description": "master volume 0..100"},
				"music": {"type": "integer", "description": "music volume 0..100"},
				"effects": {"type": "integer", "description": "effects volume 0..100"},
			},
		},
	},
	{
		"name": "hb_set_status",
		"description": "TESTING STATUS (debug builds only, both sides): ask the SERVER to put a status on the played "
		               "character, or on a creature in its view with target_id. `status` is an internal name (as in "
		               "the status cache, e.g. poisonous_aura) or a numeric id; an unknown one is refused by the "
		               "client before anything is sent. `seconds` is the duration (0 or omitted = until cleared, "
		               "or death). The client sets NOTHING locally - the status arrives on the server's ordinary "
		               "status messages and is drawn by the shipping path, so this is how a visual aura (a puff "
		               "trail) is put on screen on demand. Returns as soon as the request is SENT; observe with "
		               "hb_screenshot after a short hb_wait, or the journal's puff_spawned (visual.effect).",
		"inputSchema": {
			"type": "object",
			"properties": {
				"status": {"type": ["string", "integer"], "description": "status internal name, or its id"},
				"seconds": {"type": "number", "description": "duration in seconds; 0 / omitted = until cleared"},
				"target_id": {"type": "integer", "description": "object_id of a creature in view; omitted = yourself"},
			},
			"required": ["status"],
		},
	},
	{
		"name": "hb_set_item_tint",
		"description": "TESTING ITEM TINT (debug builds only, client side): force a recolour onto item art (HB-0441) - "
		               "the luminance-preserving tint through each frame's tint mask, or the whole opaque frame when "
		               "it has none. `display_id` picks one item display (omitted = every item); `rgb` is [r,g,b] "
		               "0..255, or \"off\" to clear. `mode` \"add\" (HB-0569) lays signed offsets -255..255 over the "
		               "art instead (the 1999 item colours); default \"luminance\". Local only: nothing is sent to "
		               "the server.",
		"inputSchema": {
			"type": "object",
			"properties": {
				"display_id": {"type": "integer", "description": "item display id; omitted = every item"},
				"rgb": {"type": ["array", "string"], "description": "[r, g, b] 0..255 (-255..255 in add mode), or \"off\""},
				"mode": {"type": "string", "enum": ["luminance", "add"], "description": "default luminance"},
			},
			"required": ["rgb"],
		},
	},
	{
		"name": "hb_flow",
		"description": "Run one whole hb_driver FLOW on this client in ONE call - each drives the UI by widget name, "
		               "waits on the game (never a blind sleep) and proves where it landed, or fails saying where it "
		               "stopped. Flows: main_menu (back to the main menu from a menu screen), login (main menu or "
		               "login screen -> character select; credentials by test_account, env or identity_file - the "
		               "password never appears in a reply), select / enter (slot or name; enter waits for the world "
		               "and proves the character), player_tile, step (direction n..nw: a click on the neighbouring "
		               "tile, walk_fallback:true walks when no click reaches it), click_tile (x, y), wait_screen "
		               "(screen, e.g. Screen_Login). To walk to a tile use the hb_walk_to TOOL. Not ported yet: "
		               "account and character creation, delete, logout, mute, quit. Flow names take underscores or "
		               "hyphens. Arguments are TOP-LEVEL and checked per flow: a key the flow does not take is an "
		               "error naming the keys it does. Batchable (inside hb_batch). Blocks until the flow ends; "
		               "returns its result as JSON.",
		"inputSchema": {
			"type": "object",
			"properties": {
				"flow": {"type": "string", "description": "which flow: main_menu, login, select, enter, player_tile, "
				                                          "step, click_tile, wait_screen (hyphens also accepted)"},
				"test_account": {"description": "login / select / enter: a local test account from the gitignored "
				                                ".claude/local/test_accounts.json - its name, or true for the first"},
				"env": {"type": "boolean", "description": "login: take the account from HB_TEST_ACCOUNT / "
				                                          "HB_TEST_PASSWORD (HB_TEST_CHARACTERS lists the slots)"},
				"identity_file": {"type": "string", "description": "login: an hb_driver identity JSON under "
				                                                   ".claude/scratch/ (relative = from the repo root)"},
				"force": {"type": "boolean", "description": "login: take over an account that is logged in elsewhere"},
				"name": {"type": "string", "description": "select / enter: the character (its position on the account "
				                                          "is its slot; enter proves the name)"},
				"slot": {"type": "integer", "description": "select / enter: character slot 1..4 (default 1)"},
				"direction": {"type": "string", "description": "step: n ne e se s sw w nw"},
				"walk_fallback": {"type": "boolean", "description": "step: walk instead when no click can reach the tile"},
				"x": {"type": "integer", "description": "click_tile: world tile x"},
				"y": {"type": "integer", "description": "click_tile: world tile y"},
				"screen": {"type": "string", "description": "wait_screen: screen id, e.g. Screen_Login"},
				"timeout_s": {"type": "number", "description": "per-step timeout (default 20)"},
			},
			"required": ["flow"],
		},
	},
]

# Every tool takes the same optional client selector, added here rather than hand-copied into 21
# schemas - one place to change, and no tool can be missed. hb_clients and hb_parallel address
# clients through their own arguments and are left alone.
CLIENT_SELECTOR_EXEMPT = {"hb_clients", "hb_parallel"}
CLIENT_SELECTOR_DOC = ("which running client to drive (handle from hb_clients). Omit for the only "
                       "registered client, or the session default when several are registered.")
CLIENT_SELECTOR_ANY_DOC = CLIENT_SELECTOR_DOC + " '*' applies this to EVERY registered client."

for _tool in TOOLS:
	if _tool["name"] in CLIENT_SELECTOR_EXEMPT:
		continue
	_schema = _tool.setdefault("inputSchema", {"type": "object", "properties": {}})
	_schema.setdefault("properties", {})["client"] = {
		"type": "string",
		"description": (CLIENT_SELECTOR_ANY_DOC if _tool["name"] in ("hb_stream", "hb_subscribe")
		                else CLIENT_SELECTOR_DOC),
	}


def resource_list():
	"""The journal, one resource per client, plus the legacy bare URI for the default client.

	Built on demand rather than fixed at import: registering a client adds its journal, and a host
	that re-lists after a discovery scan sees it.
	"""
	resources = [{
		"uri": JOURNAL_RESOURCE_URI,
		"name": "Recent game events",
		"description": "The most recent events the client journal produced, newest last, as JSON. Subscribe to be "
		               "notified when new events arrive. Reads the DEFAULT client ({}).".format(
			               REGISTRY.default_handle() or "none"),
		"mimeType": "application/json",
	}]
	for client in REGISTRY.all():
		resources.append({
			"uri": journal_uri(client.handle),
			"name": "Recent game events ({})".format(client.handle),
			"description": "The client journal for {} on {}, newest last, as JSON.".format(
				client.handle, client.address()),
			"mimeType": "application/json",
		})
	return resources


def client_for_resource(uri):
	"""Resolve a journal resource URI to a client, or None if it is not one of ours."""
	if uri == JOURNAL_RESOURCE_URI:
		return REGISTRY.implicit()
	for client in REGISTRY.all():
		if uri == journal_uri(client.handle):
			return client
	return None


def announce_resource_list_changed():
	"""Tell the host the resource list moved - a client was registered or forgotten."""
	WRITER.send({"jsonrpc": "2.0", "method": "notifications/resources/list_changed"})


TOOL_NAMES = {tool["name"] for tool in TOOLS}

# --- tools follow the client's command list -------------------------------------------------------
# `ping` returns commands[]: the wire commands the connected client implements. A tool is listed (tools/list)
# and callable only when the client lists EVERY wire command the tool needs, so a client that has no widgets or
# journal yet shows no hb_widgets, hb_flow or hb_journal_drain instead of tools that answer "unknown cmd".
# A tool missing from this table needs no wire command of its own (hb_batch, hb_parallel, hb_clients, hb_wait).
# 3 adds widgets, widget_click, consumed_by and the probe's front; 4 adds the journal (journal_query, journal_drain,
# journal_config and wait_for's event form); 5 adds the world commands (entities, tile_to_screen, walk_to,
# close_window, set_audio) and get_state / probe_point's in-world keys
SUPPORTED_BRIDGE_PROTOCOLS = (2, 3, 4, 5)
EXPECTED_BRIDGE_PROTOCOL = 5  # the current one
TOOL_COMMANDS = {
	"hb_ping": ("ping",),
	"hb_get_state": ("get_state",),
	"hb_probe": ("probe_point",),
	"hb_screenshot": ("screenshot",),
	"hb_mouse_move": ("mouse_move",),
	"hb_click": ("click",),
	"hb_mouse_down": ("mouse_down",),
	"hb_mouse_up": ("mouse_up",),
	"hb_wheel": ("wheel",),
	"hb_key": ("key",),
	"hb_text": ("text",),
	"hb_set_text": ("set_text",),
	"hb_modifiers": ("modifiers",),
	"hb_wait_for": ("wait_for",),
	"hb_send_chat": ("set_text", "key"),
	"hb_widgets": ("widgets",),
	"hb_click_widget": ("widgets", "widget_click"),
	"hb_drag_slot": ("widgets", "mouse_down", "mouse_move", "mouse_up"),
	"hb_journal_drain": ("journal_query",),
	"hb_journal_config": ("journal_config",),
	"hb_subscribe": ("journal_drain",),
	"hb_stream": ("journal_drain",),
	"hb_entities": ("entities",),
	"hb_click_entity": ("entities", "click"),
	"hb_tile_to_screen": ("tile_to_screen",),
	"hb_walk_to": ("walk_to",),
	"hb_close_window": ("close_window",),
	"hb_set_audio": ("set_audio",),
	"hb_inject": ("inject_message",),
	"hb_cast_at": ("widgets", "widget_click", "journal_query", "get_state", "click"),
	"hb_flow": ("widgets", "widget_click", "get_state", "set_text", "click", "tile_to_screen", "walk_to"),
	"hb_set_time_of_day": ("set_time_of_day",),
	"hb_kill_player": ("kill_player",),
	"hb_set_status": ("set_status",),
	"hb_set_god": ("set_god",),
	"hb_set_staff_tags": ("set_staff_tags",),
	"hb_fishing_auto_strike": ("fishing_auto_strike",),
	"hb_set_item_tint": ("set_item_tint",),
	"hb_set_tab_width_cap": ("set_tab_width_cap",),
	"hb_reload_pivots": ("reload_pivots",),
	"hb_reload_lights": ("reload_lights",),
	"hb_light_clock": ("light_clock",),
	"hb_path_bench": ("path_bench",),
	"hb_press_bench": ("press_bench",),
	"hb_self_tests": ("self_tests",),
}
# What tools/list assumes while no client answers: the commands of bridge_protocol 2's first release.
FALLBACK_COMMANDS = frozenset(("ping", "get_state", "screenshot", "probe_point", "mouse_move", "click", "mouse_down",
                               "mouse_up", "wheel", "key", "text", "set_text", "modifiers", "wait_for"))
# Tools still built on Helbreath-SDL widget ids (hud.spellbook, inventory slot rows, ...). They stay hidden and
# refused, whatever widgets the client reports, until they are retargeted. hb_flow and hb_click_widget now use
# this client's names (login.name, select_character.slot1, ...). HB_ENABLE_SDL_FLOWS=1 re-enables them
# for the fake-client tests only.
NOT_PORTED_TOOLS = frozenset(("hb_drag_slot", "hb_cast_at"))
SDL_TOOLS_ENABLED = os.environ.get("HB_ENABLE_SDL_FLOWS") == "1"
COMMANDS_TTL_SECONDS = float(os.environ.get("HB_COMMANDS_TTL_S", "30"))  # the smoke test raises it
listed_tool_names = None  # what the last tools/list returned; None until the host has asked


def missing_commands(tool_name, commands):
	"""The wire commands `tool_name` needs that `commands` lacks (empty when it is usable)."""
	return [cmd for cmd in TOOL_COMMANDS.get(tool_name, ()) if cmd not in commands]


def unavailable_reason(tool_name, commands):
	"""Why `tool_name` cannot be used with a client listing `commands` (None = it can; commands None = not known)."""
	if tool_name in NOT_PORTED_TOOLS and not SDL_TOOLS_ENABLED:
		return "{} is built on Helbreath-SDL widget ids and is not ported to this client yet (phase 4)".format(tool_name)
	missing = missing_commands(tool_name, commands) if commands is not None else []
	if missing:
		return "{} needs the wire command(s) {} which this client does not implement yet".format(
			tool_name, ", ".join(missing))
	return None


def note_commands(client, result):
	"""Remember the commands a ping result lists on `client`; tell the host when the visible tool set moved."""
	listed = result.get("commands") if isinstance(result, dict) else None
	if not isinstance(listed, list):
		return client.commands
	known = frozenset(str(name) for name in listed)
	client.commands_at = time.monotonic()
	if known != client.commands:
		client.commands = known
		if listed_tool_names is not None and client is REGISTRY.implicit():
			WRITER.send({"jsonrpc": "2.0", "method": "notifications/tools/list_changed"})
	return client.commands


def client_commands(client):
	"""The wire commands `client` lists, or None when that is not known (it does not answer, or is too old to
	list them). Cached for COMMANDS_TTL_SECONDS; a stale answer beats none when the client stops answering."""
	if client.commands is not None and time.monotonic() - client.commands_at < COMMANDS_TTL_SECONDS:
		return client.commands
	try:
		reply = client.bridge.request("ping", {}, timeout=DISCOVER_PING_SECONDS)
	except RuntimeError:
		return client.commands
	if reply.get("ok", False):
		return note_commands(client, reply.get("result"))
	return client.commands


def visible_tools():
	"""TOOLS minus those whose wire commands the default client lacks (FALLBACK_COMMANDS while none answers)."""
	global listed_tool_names
	client = REGISTRY.implicit()
	commands = client_commands(client) if client is not None else None
	if commands is None:
		commands = FALLBACK_COMMANDS
	tools = [tool for tool in TOOLS if unavailable_reason(tool["name"], commands) is None]
	listed_tool_names = {tool["name"] for tool in tools}
	return tools


# --- tool dispatch -----------------------------------------------------------------------------


def text_content(text):
	return [{"type": "text", "text": text}]


# The client's refusal for a list row laid out but outside its window's viewport (`visible: false` in widgets).
SCROLLED_OUT_NEEDLE = "scrolled out of view"
SCROLL_HINT = ("scroll the list (hb_wheel over its window) until hb_widgets no longer flags the widget HIDDEN, "
               "then retry")


def with_scroll_hint(message):
	"""The client's scrolled-out refusal with what to do about it; any other message unchanged."""
	if SCROLLED_OUT_NEEDLE in message and SCROLL_HINT not in message:
		return "{} - {}".format(message, SCROLL_HINT)
	return message


def click_button(arguments):
	"""The button a click-shaped tool presses: `button` (left|right|middle) or the older `right` bool.
	Returns (name, error); the client gets `button` and refuses the pair if they disagree."""
	name = str(arguments.get("button") or "").strip().lower()
	right = bool(arguments.get("right", False))
	if not name:
		return ("right" if right else "left"), None
	if name not in MOUSE_BUTTONS:
		return None, "unknown button '{}' - use {}".format(name, ", ".join(MOUSE_BUTTONS))
	if right and name != "right":
		return None, "'right':true contradicts button '{}'".format(name)
	return name, None


def tool_error(text):
	return {"content": text_content(text), "isError": True}


def require_bridge(client, cmd, args=None, timeout=None):
	"""Run a bridge command on one client, mapping a dead bridge to a readable message."""
	try:
		reply = client.bridge.request(cmd, args, timeout=timeout)
	except RuntimeError as error:
		if drop_if_dead(client):
			return None, ("Client '{}' not reachable at {} ({}) and nothing listens there any more: the client is "
			              "gone, so its registration was dropped (HB-0625). {}".format(
				              client.handle, client.address(), error, NO_CLIENTS_NOTE if REGISTRY.count() == 0 else
				              "hb_clients lists what is left."))
		return None, ("Client '{}' not reachable at {} ({}). Is a DEBUG client running there? "
		              "The bridge is on by default in debug; --no-agent-bridge disables it and "
		              "--agent-bridge-port=N moves it. hb_clients {{action:'discover'}} lists what "
		              "is answering.".format(client.handle, client.address(), error))
	if not reply.get("ok", False):
		return None, "Client rejected '{}': {}".format(cmd, reply.get("error", "unknown error"))
	return reply.get("result", {}), None


def event_subject(event):
	"""The string a subscription filter is matched against, and the tail of the rendered line.

	Stated in one place because a filter tested against the wrong string fails silently: it is
	`<channel>/<type> <compact json data>`, so a pattern can anchor on the channel or hit a value
	inside the payload.
	"""
	data = event.get("data")
	rendered = "" if data in (None, {}) else " " + json.dumps(data, separators=(",", ":"))
	return "{}/{}{}".format(event.get("channel", "?"), event.get("type", "?"), rendered)


def format_event(event):
	"""One event as a single readable line: the timestamp, what it was, and its payload.

	A COALESCED row - the client collapses a run of consecutive events with identical channel, type
	and data into one - carries `count`, `seq_last` and `t_ms_last`. It renders as `xN` plus the run's
	end time, because the DURATION a run covers is what a reader would otherwise lose when only the
	first event's timestamp survives. A run of one carries none of those fields and renders exactly as
	it always did.

	`count` is authoritative and cannot be derived: `seq_last - seq + 1` is NOT the run length,
	because a filtered drain coalesces the FILTERED sequence and those seqs are not contiguous (a
	104-event run spanning seq 751..1263 was measured on an audio.sound-only drain).
	"""
	count = event.get("count") or 1
	run = "" if count < 2 else " x{} ->{}ms".format(count, event.get("t_ms_last", "?"))
	return "[{}ms f{}{}] {}".format(
		event.get("t_ms", "?"), event.get("frame", "?"), run, event_subject(event))


def channel_matches(selector, channel):
	"""Segment-prefix match, per the contract: `state` matches `state.player`, `stat` does not.

	Compared segment by segment rather than with startswith, which would let `stat` match
	`state.player` - the exact bug the contract calls out.
	"""
	if not selector or selector == "*":
		return True
	want = selector.split(".")
	have = (channel or "").split(".")
	return len(have) >= len(want) and have[:len(want)] == want


def format_reload_pivots(result):
	"""Render a pivot reload. A no-op has to be obviously distinguishable from a real reload, and a
	SKIPPED sheet has to be loud - it means the art changed in a way this cannot apply, so the client
	is still drawing the old geometry and the author would otherwise think the edit landed."""
	lines = []
	scanned = result.get("sheets_scanned", 0)
	changed = result.get("sheets_changed", 0)
	moved = result.get("pivots_changed", 0)
	where = result.get("dir")
	lines.append("{} pivot(s) moved across {} of {} sheet(s) scanned{}".format(
		moved, changed, scanned, "  [{}]".format(where) if where else ""))
	if not moved and not result.get("skipped"):
		lines.append("no change - the sheets on disk match what the client already has")
	for sheet in result.get("sheets") or []:
		if not sheet.get("moved"):
			continue
		lines.append("  {}/{}  slot {}  {} frame(s), {} moved{}".format(
			sheet.get("archive", "?"), sheet.get("entry", "?"), sheet.get("slot", "?"),
			sheet.get("frames", "?"), sheet.get("moved", 0),
			"  (shadow bakes dropped)" if sheet.get("shadows_dropped") else ""))
		for change in sheet.get("changes") or []:
			frm, to = change.get("from") or [], change.get("to") or []
			lines.append("      frame {}: ({},{}) -> ({},{})".format(
				change.get("frame", "?"), *(list(frm) + list(to))[:4]))
		if sheet.get("changes_omitted"):
			lines.append("      ... {} more not listed".format(sheet["changes_omitted"]))
	for skip in result.get("skipped") or []:
		lines.append("  SKIPPED {}/{}: {}".format(
			skip.get("archive", "?"), skip.get("entry", "?"), skip.get("reason", "?")))
	if result.get("read_error"):
		lines.append("  read error: {}".format(result["read_error"]))
	return "\n".join(lines)


def format_journal(result):
	"""Render a journal query, NEWEST FIRST - an event log, not a transcript. The header says which
	view this is: live, or a history window and how to get back to live. Each line leads with the seq
	(a run shows its first and last) because after_seq / before_seq address events by it."""
	events = result.get("events") or []
	window = result.get("window") or {}
	now_ms = result.get("now_ms", "?")
	lines = []
	if result.get("live", not window):
		lines.append("live (newest first), now t={}ms - newest_seq {}".format(now_ms, result.get("newest_seq")))
	else:
		bounds = []
		if "from_ms" in window or "to_ms" in window:
			bounds.append("t={}..{}ms".format(window.get("from_ms", "start"), window.get("to_ms", "now")))
		if "after_seq" in window:
			bounds.append("seq > {}".format(window["after_seq"]))
		if "before_seq" in window:
			bounds.append("seq < {}".format(window["before_seq"]))
		lines.append("history window {} (newest first), now t={}ms, newest_seq {} - omit from/to to return "
		             "to live".format(", ".join(bounds), now_ms, result.get("newest_seq")))
		oldest_t = result.get("oldest_t_ms_retained")
		if "from_ms" in window and oldest_t is not None and window["from_ms"] < oldest_t:
			lines.append("NOTE: the ring's oldest retained event is t={}ms (seq {}) - anything earlier in "
			             "this window was evicted.".format(oldest_t, result.get("oldest_seq_retained")))
	lines.append("{} row(s), {} event(s)".format(len(events), result.get("raw", len(events))))
	for event in events:
		seq = event.get("seq", "?")
		span = "#{}".format(seq) if event.get("seq_last") is None else "#{}-{}".format(seq, event["seq_last"])
		lines.append("{} {}".format(span, format_event(event)))
	if not events:
		lines.append("(no matching events in this view)")
	if result.get("more_older"):
		lines.append("OLDER EVENTS EXIST: page with before_seq={} (keep the same filters).".format(
			result.get("older_before_seq")))
	return "\n".join(lines)


def condition_query(condition, after_seq):
	"""The journal_query arguments that find events a wait_for EVENT condition would match."""
	args = {"after_seq": after_seq, "max": WAIT_MANY_PAGE}
	if condition.get("channel"):
		args["channels"] = [str(condition["channel"])]
	if condition.get("type"):
		args["types"] = [str(condition["type"])]
	if condition.get("object_id") is not None:
		args["object_id"] = int(condition["object_id"])
	if condition.get("message_contains"):
		args["message_contains"] = str(condition["message_contains"])
	return args


def condition_accepts(condition, event):
	"""The part of a condition the journal query cannot filter: every `match` key equals the event's data."""
	data = event.get("data") or {}
	return all(data.get(key) == value for key, value in (condition.get("match") or {}).items())


def collect_matches(target, conditions, after_seq):
	"""Events after `after_seq` that match any condition, oldest first, as (condition index, event); and the
	newest seq the journal had when asked (everything up to it was seen). (None, error) on a bridge error."""
	found = {}
	newest = after_seq
	for index, condition in enumerate(conditions):
		before = None
		while True:
			args = condition_query(condition, after_seq)
			if before is not None:
				args["before_seq"] = before
			result, error = require_bridge(target, "journal_query", args)
			if error:
				return None, error
			newest = max(newest, int(result.get("newest_seq") or 0))
			for event in result.get("events") or []:
				if condition_accepts(condition, event) and event.get("seq") not in found:
					found[event.get("seq")] = (index, event)
			if not result.get("more_older"):
				break
			before = result.get("older_before_seq")
	return ([found[seq] for seq in sorted(found)], newest), None


def run_wait_many(arguments, args, target, requested_ms):
	"""hb_wait_for with max_events > 1. The client's wait_for answers the FIRST match at once (so nothing is
	missed to a polling gap); the rest are read from the non-consuming journal after that event until N are
	counted or the time is up. A run of identical events the client folded into one row counts as its `count`."""
	wanted = parse_int(arguments["max_events"])
	started = time.monotonic()
	first, error = require_bridge(target, "wait_for", args,
	                              timeout=(requested_ms / 1000.0) + WAIT_FOR_SLACK_SECONDS)
	if error:
		return tool_error(error)
	if not first.get("satisfied"):
		return {"content": text_content(headed(target, "TIMED OUT with 0 of {} matching event(s) - none matched "
		                                                "within {} ms. This is a result, not an error.\n"
		                                                "state at settle: {}".format(
			wanted, requested_ms, json.dumps(first.get("state"), separators=(",", ":")))))}
	matched = first.get("matched") or {}
	rows = [(matched.get("index", 0), matched.get("event") or {})]
	# read on from the first match itself: events that landed in the same frame are in the journal, not lost
	seen = int(rows[0][1].get("seq_last") or rows[0][1].get("seq") or (int(first.get("cursor") or 1) - 1))
	counted = rows[0][1].get("count") or 1
	dropped = int(first.get("dropped") or 0)
	deadline = started + requested_ms / 1000.0
	conditions = args["conditions"]
	while counted < wanted:
		collected, error = collect_matches(target, conditions, seen)
		if error:
			return tool_error(error)
		more, newest = collected
		for index, event in more:
			if counted >= wanted:
				break
			rows.append((index, event))
			counted += event.get("count") or 1
		seen = newest
		if counted >= wanted or time.monotonic() >= deadline:
			break
		time.sleep(WAIT_MANY_POLL_SECONDS)
	took_ms = int((time.monotonic() - started) * 1000)
	lines = []
	if counted >= wanted:
		lines.append("SATISFIED: {} of {} matching event(s) in {} ms".format(counted, wanted, took_ms))
	else:
		lines.append("TIMED OUT with {} of {} matching event(s) after {} ms - the {} below are what happened. This is "
		             "a result, not an error.".format(counted, wanted, requested_ms, counted))
	for index, event in rows:
		span = "#{}".format(event.get("seq", "?")) if event.get("seq_last") is None else "#{}-{}".format(
			event["seq"], event["seq_last"])
		lines.append("  [condition {}] {} {}".format(index, span, format_event(event)))
	if first.get("state") is not None:
		lines.append("state at the first match: " + json.dumps(first["state"], separators=(",", ":")))
	if dropped:
		lines.append("WARNING: {} event(s) evicted unread while waiting.".format(dropped))
	lines.append("journal read through seq {}".format(seen))
	return {"content": text_content(headed(target, "\n".join(lines)))}


def trail_limit(arguments):
	"""hb_wait_for's `trail` argument as a row limit: None = every event, 0 = none, N = the last N.
	Omitted means WAIT_FOR_TRAIL_DEFAULT. Raises ValueError (naming the accepted forms) on anything else."""
	raw = arguments.get("trail")
	if raw is None:
		return WAIT_FOR_TRAIL_DEFAULT
	if raw is True:
		return None
	if raw is False:
		return 0
	if isinstance(raw, int) and raw >= 0:
		return raw
	raise ValueError("'trail' must be true (every event), false (none) or a non-negative integer (the last N); "
	                 "got {!r}".format(raw))


def format_wait_for(result, trail=WAIT_FOR_TRAIL_DEFAULT):
	"""Render a wait. States the outcome first - a timeout is a legitimate answer, so it must not
	read like a failure. `trail` limits the listed events to the last N (None = all, 0 = none): a wait
	that started at since:0 would otherwise list the whole journal (HB-0125)."""
	events = result.get("events") or []
	lines = []
	if result.get("satisfied"):
		matched = result.get("matched") or {}
		lines.append("SATISFIED by condition {}: {}".format(
			matched.get("index"), json.dumps(matched.get("condition"), separators=(",", ":"))))
		# What actually fired, not merely that something did - this is what saves a follow-up call.
		if matched.get("event") is not None:
			lines.append("  fired by: " + format_event(matched["event"]))
		if matched.get("observed") is not None:
			lines.append("  observed: " + json.dumps(matched["observed"], separators=(",", ":")))
	else:
		lines.append("TIMED OUT - none of the conditions matched. This is a result, not an error: "
		             "if you were checking that something does NOT happen, this is your pass.")
	# Present on both outcomes: the resulting state after a match, or how far things got after a
	# timeout. Either way it removes a follow-up get_state.
	if result.get("state") is not None:
		lines.append("state at settle: " + json.dumps(result["state"], separators=(",", ":")))
	dropped = int(result.get("dropped") or 0)
	if dropped > 0:
		lines.append("WARNING: {} event(s) evicted unread while waiting - the trail is incomplete.".format(dropped))
	lines.append("cursor {} - {} event(s) meanwhile".format(result.get("cursor"), len(events)))
	if result.get("truncated"):
		lines.append("MORE PENDING: the trail was truncated - hb_journal_drain {{after_seq: {}}} for the rest."
		             .format(int(result.get("cursor") or 1) - 1))
	shown = events if trail is None else (events[-trail:] if trail else [])
	if len(shown) < len(events):
		if shown:
			lines.append("trail: the last {} of {} event(s) - trail:true lists every one, trail:false none".format(
				len(shown), len(events)))
		else:
			lines.append("trail: not listed (trail:false) - trail:true or a number lists the {} event(s)".format(
				len(events)))
	lines.extend(format_event(event) for event in shown)
	return "\n".join(lines)


def normalize_widgets(result):
	"""This client's widgets reply in the shape the tools and flows read: each row gains id (its name), x, y, w, h
	(its rect) and, for a text field, value ('(masked)' or its length - never the text). The native keys stay."""
	rows = []
	for row in result.get("widgets") or []:
		row = dict(row)
		row["id"] = row.get("name", row.get("id"))
		rect = row.get("rect") or {}
		for key in ("x", "y", "w", "h"):
			if key in rect:
				row[key] = rect[key]
		if row.get("masked"):
			row["value"] = "(masked)"
		elif row.get("length") is not None:
			row["value"] = "{} chars".format(row["length"])
		rows.append(row)
	normalized = dict(result)
	normalized["widgets"] = rows
	return normalized


def filter_widgets(result, arguments):
	"""hb_widgets' optional filters (HB-0693): 'screen' = owner substring, 'prefix' = id prefix,
	'contains' = substring of the id or label; all case-insensitive. The unfiltered total is kept."""
	screen = str(arguments.get("screen") or "").strip().lower()
	prefix = str(arguments.get("prefix") or "").strip().lower()
	contains = str(arguments.get("contains") or "").strip().lower()
	widgets = result.get("widgets") or []
	if not (screen or prefix or contains):
		return result
	kept = []
	for widget in widgets:
		widget_id = str(widget.get("id", "")).lower()
		if screen and screen not in str(widget.get("owner", "")).lower():
			continue
		if prefix and not widget_id.startswith(prefix):
			continue
		if contains and contains not in widget_id and contains not in str(widget.get("label", "")).lower():
			continue
		kept.append(widget)
	filtered = dict(result)
	filtered["widgets"] = kept
	filtered["filtered_from"] = len(widgets)
	return filtered


def format_widgets(result):
	"""Render a widget report grouped by the surface each widget belongs to.

	Raw JSON would be right and unaffordable - thirty widgets of nine keys each is a wall the caller
	has to read to find one button. Grouping by owner and giving each widget one line keeps the thing
	a caller actually wants (the id, and roughly where it is) on one row, in the front-to-back order
	the client reported, which is the order a click resolves them in.
	"""
	widgets = result.get("widgets") or []
	lines = ["{} - {} widget(s), front to back (a click resolves the first match)".format(
		result.get("surface") or "no screen", len(widgets))]
	if result.get("filtered_from") is not None:
		lines[0] += " [filtered from {}]".format(result["filtered_from"])
	if not widgets:
		lines.append("  (none - this screen reports no interactive widgets)")
		return "\n".join(lines)
	owner = None
	for widget in widgets:
		if widget.get("owner") != owner:
			owner = widget.get("owner")
			lines.append("  {}".format(owner))
		row = "    {:<28} {:<11} ({},{} {}x{})".format(
			widget.get("id", "?"), widget.get("kind", "?"),
			widget.get("x"), widget.get("y"), widget.get("w"), widget.get("h"))
		if widget.get("label"):
			row += '  "{}"'.format(widget["label"])
		if widget.get("value") is not None:
			row += "  = {}".format(widget["value"])
		row += format_widget_extras(widget)
		# A widget that is present but dead is a finding in its own right, so it is never dropped
		# from the list - it is flagged, and hb_click_widget refuses it rather than clicking nothing.
		if not widget.get("enabled", True):
			row += "  [DISABLED]"
		# Another surface takes a click at its centre (HB-0122); hb_click_widget refuses it too.
		if widget.get("covered_by"):
			row += "  [COVERED by {}]".format(widget["covered_by"])
		# Laid out but scrolled out of its window's viewport: a click would not reach it (the client refuses).
		if widget.get("visible") is False:
			row += "  [HIDDEN - scrolled out of view; scroll the list to reveal it]"
		lines.append(row)
	return "\n".join(lines)


def format_rect(box):
	"""A {x, y, w, h} rect as 'x,y wxh'."""
	if not isinstance(box, dict):
		return "?"
	return "{},{} {}x{}".format(box.get("x"), box.get("y"), box.get("w"), box.get("h"))


def format_widget_extras(widget):
	"""The optional fields of a widget row on one tail: an inventory / character-sheet slot (slot, item_id,
	quantity), a slider's track and thumb, a dropdown's options and selection."""
	extra = ""
	if widget.get("slot") is not None:
		if widget.get("item_id") is None:
			extra += "  slot {} empty".format(widget["slot"])
		else:
			extra += "  slot {} item #{} x{}".format(widget["slot"], widget["item_id"], widget.get("quantity", 1))
	if widget.get("aim") is not None:
		extra += "  aim ({},{})".format(widget["aim"].get("x"), widget["aim"].get("y"))
	if widget.get("track") is not None:
		extra += "  track ({}) thumb ({})".format(format_rect(widget["track"]), format_rect(widget.get("thumb")))
	if widget.get("options"):
		extra += "  options [{}]".format(" | ".join(str(option) for option in widget["options"]))
		if widget.get("selected") is not None:
			extra += " selected {}".format(widget["selected"])
		if widget.get("option_rects"):
			extra += " (open: {} option rows from #{}, in option_rects)".format(
				len(widget["option_rects"]), widget.get("option_first", 0))
	elif widget.get("selected") is not None:
		extra += "  selected {}".format(widget["selected"])
	return extra


def filter_frame_sections(result, wanted):
	"""A probe reply whose frame_times.sections holds only the `wanted` names; (reply, names that do not exist)."""
	report = result["frame_times"]
	sections = report.get("sections")
	if not isinstance(sections, dict):
		return result, []
	wanted = [str(name) for name in wanted] if isinstance(wanted, list) else [str(wanted)]
	kept = dict(report)
	kept["sections"] = {name: value for name, value in sections.items() if name in wanted}
	filtered = dict(result)
	filtered["frame_times"] = kept
	return filtered, [name for name in wanted if name not in sections]


def compact_probe(result):
	"""A probe reply without the verbose blocks: (compact result, names of what was left out)."""
	detail = result.get("detail")
	if not isinstance(detail, dict):
		return result, []
	left_out = [key for key in PROBE_VERBOSE_KEYS if key in detail]
	trimmed = {key: value for key, value in detail.items() if key not in PROBE_VERBOSE_KEYS}
	for key in [key for key, value in trimmed.items() if key.startswith("illusion_") and value is False]:
		del trimmed[key]
	blocking = trimmed.get("tile_blocking")
	if isinstance(blocking, dict):
		trimmed["tile_blocking"] = {key: value for key, value in blocking.items()
		                            if not (key == "objects" and not value) and not (key == "object_count" and not value)}
	compact = dict(result)
	compact["detail"] = trimmed
	return compact, left_out


def format_entity(entity):
	"""One entity row on one line: id, kind, name, tile, distance, and where it draws."""
	name = str(entity.get("name") or "?")
	tile = entity.get("tile") or ["?", "?"]
	ident = entity.get("object_id") if entity.get("kind") != "item" else "item {}".format(entity.get("item_id"))
	row = "#{} {} {} ({},{}) d{}".format(ident, entity.get("kind"), name, tile[0], tile[1], entity.get("distance"))
	if entity.get("dead"):
		row += " DEAD"
	window = (entity.get("point") or {}).get("window")
	if not entity.get("on_screen"):
		row += " OFF SCREEN"
	elif window:
		row += " at window ({},{})".format(window[0], window[1])
	return row


def format_entities(result):
	entities = result.get("entities") or []
	tile = result.get("player_tile") or ["?", "?"]
	lines = ["player ({},{}) - {} of {} entities, nearest first{}".format(
		tile[0], tile[1], result.get("count", len(entities)), result.get("total", len(entities)),
		" (TRUNCATED: raise max)" if result.get("truncated") else "")]
	lines.extend("  " + format_entity(entity) for entity in entities)
	if not entities:
		lines.append("  (none match)")
	return "\n".join(lines)


def format_tile_to_screen(result):
	tile = result.get("tile") or ["?", "?"]
	if not result.get("in_bounds", True):
		return "tile ({},{}) is OUTSIDE the map".format(tile[0], tile[1])
	window = result.get("window") or ["?", "?"]
	logical = result.get("logical") or ["?", "?"]
	text = "tile ({},{}) -> window ({},{}) logical ({},{})".format(tile[0], tile[1], window[0], window[1],
	                                                             logical[0], logical[1])
	if not result.get("on_screen"):
		return text + " - OFF SCREEN (player at {})".format(tuple(result.get("player_tile") or ()))
	resolved = result.get("resolves_to") or ["?", "?"]
	front = result.get("front") or {}
	text += ", on screen, a click there resolves to ({},{}){}".format(
		resolved[0], resolved[1], "" if result.get("round_trip") else " - NOT this tile")
	if front:
		text += ", front {}{}".format(front.get("kind", "?"), " " + str(front["owner"]) if front.get("owner") else "")
	if result.get("on_self"):
		text += ", ON THE PLAYER'S OWN SPRITE (a click picks up, never moves)"
	if result.get("entity_id") is not None:
		text += ", entity #{}".format(result["entity_id"])
	return text


def format_walk_to(reply):
	"""A walk_to reply: walking (started), or how a waited walk settled (arrived, blocked, timeout, superseded)."""
	goal = reply.get("goal") or ["?", "?"]
	origin = reply.get("from") or ["?", "?"]
	text = "walk_to ({},{}) from ({},{}): {}".format(goal[0], goal[1], origin[0], origin[1], reply.get("status", "?"))
	if reply.get("at"):
		text += " at ({},{})".format(reply["at"][0], reply["at"][1])
	if reply.get("map"):
		text += " on {}".format(reply["map"])
	if reply.get("reason"):
		text += " - {}".format(reply["reason"])
	if reply.get("elapsed_ms") is not None:
		text += " after {} ms".format(reply["elapsed_ms"])
	return text


def format_audio(audio):
	"""get_state.audio / a set_audio reply on one line."""
	if not isinstance(audio, dict):
		return "(none reported)"
	text = "master {}, volumes master {} music {} effects {}".format(
		"MUTED" if audio.get("master_muted") else "playing", audio.get("master"), audio.get("music"), audio.get("effects"))
	if "persisted" in audio:
		text += " (saved to client.json)" if audio["persisted"] else " (not saved)"
	return text


def format_memory(memory):
	"""get_state.memory on one line, sizes in MB."""
	if not isinstance(memory, dict):
		return "(none reported)"

	def mb(key, per=1024.0):
		value = memory.get(key)
		return "?" if not isinstance(value, (int, float)) else "{:.1f}".format(value / per)

	text = "working set {} MB (peak {}), commit {} MB; clip cache {} / {} MB budget ({} entries); tile sheets {} MB; minimap {} MB".format(
		mb("working_set_kb"), mb("peak_working_set_kb"), mb("commit_kb"),
		mb("clip_cache_bytes", per=1048576.0), mb("clip_cache_budget_bytes", per=1048576.0),
		memory.get("clip_cache_entries", "?"), mb("tile_sheet_cache_bytes", per=1048576.0),
		mb("minimap_cache_bytes", per=1048576.0))
	return text


# hb_get_state's compact form: a nested value longer than this is cut (verbose:true / raw:true show it whole).
STATE_COMPACT_VALUE_CHARS = 160
# scalar fields per summary line, so the line stays readable
STATE_COMPACT_FIELDS_PER_LINE = 6


def compact_state_value(value):
	"""A nested get_state value on one line: a list of named objects as its names, anything else as compact JSON,
	cut at STATE_COMPACT_VALUE_CHARS."""
	if isinstance(value, list) and value and all(isinstance(item, dict) for item in value):
		names = [str(item.get("id") or item.get("name") or item.get("dialog") or "?") for item in value]
		text = "[{}]".format(", ".join(names))
	else:
		text = json.dumps(value, separators=(",", ":"))
	if len(text) > STATE_COMPACT_VALUE_CHARS:
		text = text[:STATE_COMPACT_VALUE_CHARS] + "..."
	return text


def format_state_compact(result):
	"""get_state as a summary (HB-0625, the default): the scalar fields packed a few to a line, each nested one on
	its own short line, audio as its one line and memory left out (verbose:true for the old full form)."""
	scalars = [(key, value) for key, value in result.items()
	           if not isinstance(value, (dict, list)) and key not in ("audio", "memory")]
	lines = []
	for first in range(0, len(scalars), STATE_COMPACT_FIELDS_PER_LINE):
		lines.append("  ".join("{}={}".format(key, json.dumps(value) if isinstance(value, str) and " " in value
		                                      else value)
		                       for key, value in scalars[first:first + STATE_COMPACT_FIELDS_PER_LINE]))
	for key, value in result.items():
		if isinstance(value, (dict, list)) and key not in ("audio", "memory"):
			lines.append("{}: {}".format(key, compact_state_value(value)))
	if "audio" in result:
		lines.append("audio: " + format_audio(result["audio"]))
	if "memory" in result:
		lines.append("(memory left out - verbose:true or raw:true)")
	return "\n".join(lines)


def format_state(result):
	"""get_state as JSON, with audio and memory (the client's big blocks) folded into one line each."""
	rest = {key: value for key, value in result.items() if key not in ("audio", "memory")}
	text = json.dumps(rest, indent=2)
	if "audio" in result:
		text += "\naudio: " + format_audio(result["audio"])
	if "memory" in result:
		text += "\nmemory: " + format_memory(result["memory"])
	return text


def format_consumed(result):
	""" ' -> consumed by <kind> <owner>' from a click reply's `consumed_by` {kind, owner} (what took the
	click: world, screen, window, modal, minimap, big_map, action_bar, npc_bar, shop_lock, dead, widget);
	'' when the client does not report it (an older build) or the reply is not a dict."""
	taken = result.get("consumed_by") if isinstance(result, dict) else None
	if not isinstance(taken, dict) or not taken.get("kind"):
		return ""
	owner = taken.get("owner")
	return " -> consumed by {}{}".format(taken["kind"], " " + str(owner) if owner else "")


def with_space(arguments, args):
	"""Forward an optional coordinate-space selector to the client command."""
	if arguments.get("space"):
		args["space"] = str(arguments["space"])
	return args


# --- hb_screenshot options ---------------------------------------------------------------------------
# The client writes the PNG (HB-0692: it reads back and encodes only `region` when one is given, and writes
# off its main thread, renaming the finished file into place). scale / jpg - and region, for a client that
# does not crop itself - are done HERE with Pillow, an OPTIONAL import (the rest of this server is
# stdlib-only). The capture lands at a temporary raw name and is moved into place on success, so the
# 20 MB screenshots folder only ever holds what was asked for and a caller's existing file is never lost.


def pillow_problem(what):
	return ("hb_screenshot {} needs the Pillow package on the MCP host (pip install pillow), which is not "
	        "installed. A plain capture - no region / scale / format / quality - still works.".format(what))


def lanczos_filter():
	"""Lanczos, the sharp resample the art-scaling rule asks for (never plain linear)."""
	return getattr(getattr(Image, "Resampling", Image), "LANCZOS")


def parse_screenshot_options(arguments):
	"""Validate hb_screenshot's arguments without capturing anything. Returns (options, error text)."""
	options = {"path": None, "region": None, "scale": None, "format": "png", "quality": None, "inline": True,
	           "processed": False}
	path_format = None
	path = arguments.get("path")
	if path is not None:
		path = str(path).strip()
		if not path:
			return None, "hb_screenshot 'path' is empty"
		if str(arguments.get("name") or "").strip():
			return None, "hb_screenshot takes 'name' or 'path', not both"
		extension = os.path.splitext(path)[1].lower().lstrip(".")
		if extension not in SCREENSHOT_FORMATS:
			return None, "hb_screenshot 'path' needs a .png, .jpg or .jpeg extension (got {!r})".format(path)
		options["path"] = path
		path_format = SCREENSHOT_FORMATS[extension][0]
	requested = arguments.get("format")
	if requested is not None:
		key = str(requested).strip().lower()
		if key not in SCREENSHOT_FORMATS:
			return None, "hb_screenshot 'format' must be png or jpg (got {!r})".format(requested)
		if path_format and SCREENSHOT_FORMATS[key][0] != path_format:
			return None, "hb_screenshot 'format' {} disagrees with the extension of 'path'".format(key)
		options["format"] = SCREENSHOT_FORMATS[key][0]
	elif path_format:
		options["format"] = path_format

	region = arguments.get("region")
	if region is not None:
		if isinstance(region, dict) and all(key in region for key in ("x", "y", "w", "h")):
			values = [region[key] for key in ("x", "y", "w", "h")]
		elif isinstance(region, (list, tuple)) and len(region) == 4:
			values = list(region)
		else:
			return None, "hb_screenshot 'region' must be {x, y, w, h} in output pixels"
		try:
			x, y, w, h = (int(value) for value in values)
		except (TypeError, ValueError):
			return None, "hb_screenshot 'region' values must be integers"
		if w <= 0 or h <= 0 or x < 0 or y < 0:
			return None, "hb_screenshot 'region' needs x, y >= 0 and w, h > 0"
		options["region"] = (x, y, w, h)

	scale = arguments.get("scale")
	if scale is not None:
		try:
			scale = float(scale)
		except (TypeError, ValueError):
			return None, "hb_screenshot 'scale' must be a number"
		if not (SCREENSHOT_MIN_SCALE <= scale <= 1.0):
			return None, "hb_screenshot 'scale' must be {:g}..1 (a Lanczos DOWNscale; got {:g})".format(
				SCREENSHOT_MIN_SCALE, scale)
		if scale < 1.0:
			options["scale"] = scale

	quality = arguments.get("quality")
	if quality is not None:
		if options["format"] != "jpg":
			return None, "hb_screenshot 'quality' applies to format jpg only"
		try:
			quality = int(quality)
		except (TypeError, ValueError):
			return None, "hb_screenshot 'quality' must be an integer 1-95"
		if not (1 <= quality <= 95):
			return None, "hb_screenshot 'quality' must be 1-95 (got {})".format(quality)
		options["quality"] = quality

	if arguments.get("inline") is not None:
		options["inline"] = bool(arguments["inline"])

	needs = [label for label, wanted in (("region", options["region"]), ("scale", options["scale"]),
	                                     ("format jpg", options["format"] == "jpg")) if wanted]
	options["processed"] = bool(needs)
	if needs and Image is None:
		return None, pillow_problem(" / ".join(needs))
	options["burst"] = 1
	options["interval_ms"] = SCREENSHOT_BURST_INTERVAL_MS
	if arguments.get("burst") is not None:
		count = parse_int(arguments["burst"])
		if count is None or not 1 <= count <= SCREENSHOT_BURST_MAX:
			return None, "hb_screenshot 'burst' must be a whole number 1..{} (got {!r})".format(
				SCREENSHOT_BURST_MAX, arguments["burst"])
		options["burst"] = count
	if arguments.get("interval_ms") is not None:
		gap = parse_int(arguments["interval_ms"])
		if gap is None or not 0 <= gap <= SCREENSHOT_BURST_INTERVAL_MAX_MS:
			return None, "hb_screenshot 'interval_ms' must be a whole number 0..{} (got {!r})".format(
				SCREENSHOT_BURST_INTERVAL_MAX_MS, arguments["interval_ms"])
		if options["burst"] < 2:
			return None, "hb_screenshot 'interval_ms' only applies with a 'burst' of 2 or more"
		options["interval_ms"] = gap
	return options, None


def inside_any(path, roots):
	"""True when `path` resolves inside one of `roots` (case-folded: Windows paths)."""
	candidate = os.path.normcase(os.path.abspath(path))
	for root in roots:
		base = os.path.normcase(os.path.abspath(root))
		if candidate == base or candidate.startswith(base + os.sep):
			return True
	return False


def wait_for_capture(raw_path):
	"""The client answers before the PNG exists (it encodes on a worker and renames it into place). Error text or None."""
	deadline = time.monotonic() + SCREENSHOT_WRITE_WAIT_S
	failed_path = raw_path + SCREENSHOT_FAILED_SUFFIX
	while not os.path.exists(raw_path):
		if os.path.exists(failed_path):
			return "the client could not write the screenshot (see its log)"
		if time.monotonic() > deadline:
			late_captures.append(raw_path)
			return "the client accepted the screenshot but {} did not appear within {:g} s (see its log)".format(
				raw_path, SCREENSHOT_WRITE_WAIT_S)
		time.sleep(SCREENSHOT_WRITE_POLL_S)
	return None


def sweep_late_captures():
	"""Remove raw captures (and failure markers) that arrived after an earlier wait gave up."""
	while late_captures:
		late = late_captures.pop()
		for leftover in (late, late + SCREENSHOT_FAILED_SUFFIX):
			try:
				os.remove(leftover)
			except OSError:
				pass


def retry_io(action):
	"""Run a file action, retrying a PermissionError (a file another process still holds) a few times."""
	for attempt in range(SCREENSHOT_IO_RETRIES):
		try:
			return action()
		except PermissionError:
			if attempt == SCREENSHOT_IO_RETRIES - 1:
				raise
			time.sleep(SCREENSHOT_IO_RETRY_S)


def process_capture(raw_path, out_path, options, client_cropped=False):
	"""Crop, Lanczos-shrink and re-encode a raw PNG capture into `out_path`. Returns (summary, error).
	`client_cropped`: the client already cut the region, so the raw capture IS the crop."""
	try:
		def read_raw():
			with Image.open(raw_path) as opened:
				return opened.copy()
		picture = retry_io(read_raw)
	except (OSError, ValueError) as error:
		return None, "could not read the capture to process it: {}".format(error)
	full = picture.size
	if options["region"] and not client_cropped:
		x, y, w, h = options["region"]
		if x + w > full[0] or y + h > full[1]:
			return None, "hb_screenshot region {{x:{}, y:{}, w:{}, h:{}}} lies outside the {}x{} capture".format(
				x, y, w, h, full[0], full[1])
		picture = picture.crop((x, y, x + w, y + h))
	cropped = picture.size
	if options["scale"]:
		picture = picture.resize((max(1, int(round(cropped[0] * options["scale"]))),
		                          max(1, int(round(cropped[1] * options["scale"])))), lanczos_filter())
	try:
		if options["format"] == "jpg":
			picture.convert("RGB").save(out_path, "JPEG", quality=options["quality"] or SCREENSHOT_JPEG_QUALITY,
			                            optimize=True)
		else:
			picture.save(out_path, "PNG")
	except (OSError, ValueError) as error:
		return None, "could not write {}: {}".format(out_path, error)
	return "{}x{} -> {}x{} {}, {:.0f} KB".format(
		full[0], full[1], picture.size[0], picture.size[1], options["format"],
		os.path.getsize(out_path) / 1024.0), None


def capture_one(target, options, shot_path):
	"""One capture saved at `shot_path` with the options applied. Returns (saved path, ' [summary]', error)."""
	sweep_late_captures()
	# The client refuses paths: it takes a bare name and writes <repo>/build/screenshots/<name>, so the raw
	# capture is named here and found there. The reply comes after the file is in place; region_* is not sent
	# (this client ignores it), so any crop is Pillow's.
	raw_name = "{}{}_{}.png".format(SCREENSHOT_RAW_PREFIX, os.getpid(), time.time_ns())
	raw_path = os.path.join(os.path.abspath(screenshot_dir()), raw_name)
	summary = ""
	try:
		result, error = require_bridge(target, "screenshot", {"name": raw_name}, timeout=SCREENSHOT_REPLY_SECONDS)
		if error:
			return None, "", error
		error = wait_for_capture(raw_path)
		if error:
			return None, "", error
		client_cropped = False
		saved = shot_path
		if options["processed"]:
			detail, problem = process_capture(raw_path, shot_path, options, client_cropped)
			if problem:
				return None, "", problem
			summary = " [{}]".format(detail)
		else:
			try:
				retry_io(lambda: os.replace(raw_path, shot_path))  # only now does the caller's target change
			except OSError as move_error:
				return None, "", "could not move the capture to {}: {}".format(shot_path, move_error)
	finally:
		for leftover in (raw_path, raw_path + SCREENSHOT_FAILED_SUFFIX):
			try:
				retry_io(lambda: os.remove(leftover))
			except OSError:
				pass
	return saved, summary, None


def folder_bytes(folder):
	"""Total size of every file under `folder` (0 when it does not exist)."""
	total = 0
	for base, _dirs, names in os.walk(folder):
		for name in names:
			try:
				total += os.path.getsize(os.path.join(base, name))
			except OSError:
				pass
	return total


def screenshot_cap_bytes():
	try:
		return int(os.environ.get("HB_SCREENSHOT_CAP_BYTES") or SCREENSHOT_DIR_CAP_BYTES)
	except ValueError:
		return SCREENSHOT_DIR_CAP_BYTES


def cap_owner_report(shot_dir):
	"""Who holds the screenshots folder's bytes, biggest first: each subfolder (an agent's own, via a relative 'path'
	such as 'test/x.png') and the loose files at the top, with size, file count and age. Text for a refusal."""
	now = time.time()
	owners = {}
	for base, _dirs, names in os.walk(shot_dir):
		relative = os.path.relpath(base, shot_dir)
		owner = "(loose files)" if relative == "." else relative.split(os.sep)[0] + "/"
		for name in names:
			try:
				info = os.stat(os.path.join(base, name))
			except OSError:
				continue
			row = owners.setdefault(owner, {"bytes": 0, "files": 0, "newest": 0.0, "biggest": ("", 0)})
			row["bytes"] += info.st_size
			row["files"] += 1
			row["newest"] = max(row["newest"], info.st_mtime)
			if info.st_size > row["biggest"][1]:
				row["biggest"] = (name, info.st_size)
	ranked = sorted(owners.items(), key=lambda item: -item[1]["bytes"])[:SCREENSHOT_OWNER_ROWS]
	lines = ["  {:<16} {:>6.1f} MB, {} files, newest {:.0f} min old, biggest {} ({:.1f} MB)".format(
		owner, row["bytes"] / 1048576.0, row["files"], (now - row["newest"]) / 60.0, row["biggest"][0],
		row["biggest"][1] / 1048576.0) for owner, row in ranked]
	return "Who holds it:\n" + "\n".join(lines) + "\n(give each agent its own subfolder with path 'role/name.png' so its files can be pruned by owner)"


def burst_paths(shot_path, count, replace=False):
	"""The N files of a burst: the chosen path or name with _01, _02, ... before the extension. By default none
	existing (a numbered copy); `replace` (an explicit 'path', which the single shot also replaces) reuses the names."""
	stem, extension = os.path.splitext(shot_path)
	paths = []
	for number in range(1, count + 1):
		candidate = "{}_{:02d}{}".format(stem, number, extension)
		suffix = 1
		while (os.path.exists(candidate) and not replace) or candidate in paths:
			candidate = "{}_{:02d}_{}{}".format(stem, number, suffix, extension)
			suffix += 1
		paths.append(candidate)
	return paths


def pick_burst_images(count, limit=BATCH_MAX_IMAGES):
	"""Which 0-based frames of a burst are returned as images: the first, the last and evenly spaced ones."""
	if count <= limit:
		return list(range(count))
	if limit <= 1:
		return [0]
	return sorted({int(round(index * (count - 1) / float(limit - 1))) for index in range(limit)})


def capture_burst(target, options, shot_path, shot_dir):
	"""hb_screenshot burst: N captures start-to-start `interval_ms` apart, each saved, the folder cap honoured.

	The cap (build/screenshots, 20 MB) is checked before the first capture (room for a raw one) and after it
	(N frames of the first one's size must fit); a refusal removes what the burst wrote. Returns the tool result."""
	count = options["burst"]
	folder = os.path.dirname(shot_path)
	capped = inside_any(folder, [shot_dir])
	cap = screenshot_cap_bytes()
	used = folder_bytes(shot_dir) if capped else 0
	if capped and used + SCREENSHOT_RAW_HEADROOM_BYTES > cap:
		return tool_error("hb_screenshot burst REFUSED: {} holds {:.1f} MB of its {:.0f} MB cap, with no room for a "
		                  "capture - prune it (only your own shots) first.\n{}".format(
			shot_dir, used / 1048576.0, cap / 1048576.0, cap_owner_report(shot_dir)))
	paths = burst_paths(shot_path, count, replace=bool(options["path"]))
	started = time.monotonic()
	rows = []
	written = 0
	for index, path in enumerate(paths):
		due = started + index * options["interval_ms"] / 1000.0
		wait = due - time.monotonic()
		if wait > 0:
			time.sleep(wait)
		offset_ms = int((time.monotonic() - started) * 1000)
		saved, summary, error = capture_one(target, options, path)
		if error:
			text = "burst stopped at frame {} of {}: {}".format(index + 1, count, error)
			return {"content": text_content(label(target) + text + ("\n" + burst_listing(rows) if rows else "")),
			        "isError": True}
		size = os.path.getsize(saved)
		written += size
		rows.append({"path": saved, "summary": summary, "offset_ms": offset_ms, "size": size})
		if capped and index == 0 and used + size * count + SCREENSHOT_RAW_HEADROOM_BYTES > cap:
			try:
				os.remove(saved)
			except OSError:
				pass
			return tool_error("hb_screenshot burst REFUSED: {} frames of {:.2f} MB would need {:.1f} MB and {} holds "
			                  "{:.1f} of its {:.0f} MB cap. Use format jpg, a scale or a region, or fewer frames. "
			                  "(Nothing was kept.)\n{}".format(count, size / 1048576.0, size * count / 1048576.0, shot_dir,
			                                                  used / 1048576.0, cap / 1048576.0, cap_owner_report(shot_dir)))
	took_ms = int((time.monotonic() - started) * 1000)
	text = "{}burst of {}: {} files, {:.2f} MB, {} ms (interval {} ms)\n{}".format(
		label(target), count, len(rows), written / 1048576.0, took_ms, options["interval_ms"], burst_listing(rows))
	if capped:
		text += "\n{} now holds {:.1f} of {:.0f} MB".format(shot_dir, folder_bytes(shot_dir) / 1048576.0, cap / 1048576.0)
	if not options["inline"]:
		return {"content": text_content(text + "\n(images not returned: inline false)")}
	picked = pick_burst_images(count)
	if len(picked) < count:
		text += "\n(returning {} of {} images: frames {}; every file is saved)".format(
			len(picked), count, ", ".join(str(index + 1) for index in picked))
	blocks = text_content(text)
	mime_type = SCREENSHOT_FORMATS[options["format"]][1]
	for index in picked:
		try:
			with open(rows[index]["path"], "rb") as handle:
				data = base64.b64encode(handle.read()).decode("ascii")
		except OSError:
			continue
		blocks.append({"type": "image", "data": data, "mimeType": mime_type})
	return {"content": blocks}


def burst_listing(rows):
	return "\n".join("  {:>2}. +{}ms {}{}".format(number, row["offset_ms"], row["path"], row["summary"])
	                  for number, row in enumerate(rows, 1))


def capture_screenshot(target, arguments):
	"""hb_screenshot: capture, optionally crop / shrink / re-encode, save, and return the image."""
	options, problem = parse_screenshot_options(arguments)
	if problem:
		return tool_error(problem)
	shot_dir = os.path.abspath(screenshot_dir())
	extension = options["format"]
	mime_type = SCREENSHOT_FORMATS[extension][1]
	if options["path"]:
		# An explicit path is honoured exactly (an existing file is replaced), but only inside the repo
		# or the screenshots folder: this process must not become a way to write anywhere on the disk.
		shot_path = os.path.normpath(options["path"] if os.path.isabs(options["path"])
		                             else os.path.join(shot_dir, options["path"]))
		if not inside_any(shot_path, [repo_root(), shot_dir]):
			return tool_error("hb_screenshot 'path' must be inside the repo or {} (got {})".format(shot_dir, shot_path))
	else:
		requested = str(arguments.get("name", "")).strip()
		stem = "".join(ch for ch in requested if ch.isalnum() or ch in ("_", "-")) or time.strftime("shot_%Y%m%d_%H%M%S")
		# With several clients the handle goes IN THE FILENAME. Two clients captured in the same
		# second otherwise differ only by a "_1" suffix, and a shot you cannot attribute to a client
		# is evidence of nothing - never mind that they can come up at different window sizes.
		if REGISTRY.count() > 1:
			stem = "{}_{}".format(stem, target.handle)
		shot_path = os.path.join(shot_dir, "{}.{}".format(stem, extension))
		suffix = 1
		while os.path.exists(shot_path):  # never clobber an existing capture (same second / same name)
			shot_path = os.path.join(shot_dir, "{}_{}.{}".format(stem, suffix, extension))
			suffix += 1
	# the client writes the PNG to an absolute path we hand it (stbi_write_png does NOT create parent
	# dirs, so we make the folder first); both processes share this machine's filesystem
	try:
		os.makedirs(os.path.dirname(shot_path), exist_ok=True)
	except OSError as make_error:
		return tool_error("could not create screenshot dir {}: {}".format(os.path.dirname(shot_path), make_error))
	if options["burst"] > 1:
		return capture_burst(target, options, shot_path, shot_dir)
	saved, summary, error = capture_one(target, options, shot_path)
	if error:
		return tool_error(error)
	text = "{}saved: {}{}".format(label(target), saved, summary)
	if not options["inline"]:
		return {"content": text_content(text + " (image not returned: inline false)")}
	try:
		with open(saved, "rb") as handle:
			encoded = base64.b64encode(handle.read()).decode("ascii")
	except OSError as read_error:
		return tool_error("client saved the screenshot but it could not be read: {}".format(read_error))
	return {"content": [
		{"type": "image", "data": encoded, "mimeType": mime_type},
		{"type": "text", "text": text},
	]}


# --- batching ----------------------------------------------------------------------------------
# The whole point of this tool. The bridge answers within one frame (~16ms), so bridge latency is
# never what makes driving the client slow - it is that ONE MCP TOOL CALL COSTS ONE MODEL ROUND TRIP,
# a couple of seconds each. Logging in and reaching the world took 11 calls, of which two were blind
# `hb_wait` sleeps totalling 9.5 seconds that were guesses padded for safety.
#
# A batch collapses that into a single call: the local socket round trip is sub-millisecond, so the
# server can walk the steps itself. Combined with wait_for replacing the blind sleeps, the whole
# login sequence becomes one tool call that finishes the instant the game is actually ready.

# A batch runs unattended, so it is bounded: a runaway sequence must not hold the host for minutes.
BATCH_MAX_STEPS = 64
BATCH_MAX_TOTAL_SECONDS = 180.0

# Which tools a batch step may name. Deliberately excludes hb_batch (no nesting - it would make the
# bound above meaningless) and the streaming/subscription tools, which are session state rather than
# actions and have no business inside a sequence.
BATCH_ALLOWED = {
	"hb_ping", "hb_self_tests", "hb_get_state", "hb_probe", "hb_screenshot", "hb_mouse_move", "hb_click",
	"hb_mouse_down", "hb_mouse_up",
	"hb_key", "hb_text", "hb_set_text", "hb_wheel", "hb_wait", "hb_wait_for",
	"hb_journal_drain", "hb_journal_config", "hb_kill_player",
	# Widget discovery earns its place here more than anywhere else: "enumerate, then click the one
	# named Start" is a two-step that should never cost two model round trips, and hb_click_widget
	# re-resolves the id in the frame it clicks, so a batch step cannot act on a stale coordinate.
	"hb_widgets", "hb_click_widget", "hb_modifiers",
	# One whole flow (a tile step, a mute, a login) as a batch step: "step, journal, screenshot" is one
	# call. The flow borrows this server's own connection, so it interleaves with its neighbours safely.
	"hb_flow",
	# The world tools: look up what is around (entities, a tile's pixel), act on it (click an entity, walk to a
	# tile, set the sky), so "list, click, wait" is one call.
	"hb_entities", "hb_click_entity", "hb_tile_to_screen", "hb_walk_to", "hb_set_time_of_day", "hb_set_audio",
	"hb_light_clock", "hb_reload_lights", "hb_set_god", "hb_set_staff_tags", "hb_close_window", "hb_drag_slot", "hb_send_chat", "hb_cast_at", "hb_path_bench", "hb_inject", "hb_fishing_auto_strike",
	"hb_set_tab_width_cap",
	# hb_press_bench is NOT here: a whole-registry bench outruns the batch's 180 s budget
}

# hb_key: spellings callers guess, mapped to the client's key names (src/client/agent_bridge.h key_names).
KEY_ALIASES = {
	"return": "enter", "esc": "escape", "del": "delete", "pgup": "page_up", "pageup": "page_up",
	"pgdn": "page_down", "pagedown": "page_down", "pgdown": "page_down", "spacebar": "space", "ins": "insert",
	"+": "plus", "-": "minus", "control": "ctrl", "backspace": "backspace",
}


def canonical_key_name(name):
	"""hb_key's name, trimmed, lower-cased and de-aliased (Enter, Return, ESC -> enter, enter, escape)."""
	key = str(name).strip().lower().replace("-", "_") if len(str(name).strip()) > 1 else str(name).strip().lower()
	return KEY_ALIASES.get(key, key)


JOURNAL_DRAIN_KEYS = ("client", "channels", "types", "exclude_types", "max", "limit", "from_ms", "to_ms", "after_seq",
                      "before_seq", "object_id", "message_contains")
# Where a caller tends to put a journal argument that belongs under another name.
JOURNAL_DRAIN_HINTS = {
	"from": "from_ms", "to": "to_ms", "since": "after_seq", "after": "after_seq", "before": "before_seq",
	"count": "max", "type": "types", "channel": "channels", "entity_id": "object_id",
	"contains": "message_contains", "text": "message_contains",
}
# The world tools take a closed set of keys too; an unknown one is refused, never dropped.
WORLD_TOOL_KEYS = {
	"hb_entities": ("client", "kinds", "radius", "max", "on_screen", "raw"),
	"hb_click_entity": ("client", "object_id", "right", "shift", "ctrl"),
	"hb_tile_to_screen": ("client", "x", "y", "raw"),
	"hb_walk_to": ("client", "x", "y", "wait", "timeout_ms"),
	"hb_set_time_of_day": ("client", "t01"),
	"hb_set_audio": ("client", "master", "music", "effects"),
	"hb_light_clock": ("client", "seconds"),
	"hb_reload_lights": ("client",),
	"hb_set_god": ("client", "on"),
	"hb_set_staff_tags": ("client", "visible"),
	"hb_set_tab_width_cap": ("client", "width"),
	"hb_close_window": ("client", "name", "dialog_id"),
	"hb_fishing_auto_strike": ("client", "on", "delay_ms"),
	"hb_set_status": ("client", "status", "seconds", "target_id"),
	"hb_set_item_tint": ("client", "display_id", "rgb", "mode"),
	"hb_cast_at": ("client", "object_id", "name", "spell", "timeout_ms", "wait"),
	"hb_path_bench": ("client", "fixture", "repeats", "raw"),
	"hb_press_bench": ("client", "display_id", "repeats", "live_path", "raw"),
	"hb_inject": ("client", "type", "rows", "object_id", "slot", "subtitle_id", "colour", "string_id", "audience", "id",
	              "payload"),
}
AUDIO_VOLUME_KEYS = ("master", "music", "effects")
ENTITY_KINDS = ("player", "npc", "creature", "item")
# hb_entities replies at most this many rows unless `max` says otherwise (a busy map lists hundreds).
ENTITIES_DEFAULT_MAX = 25
# An unknown-target refusal lists this many of the nearest entities by name.
ENTITY_HINT_ROWS = 5
WALK_WAIT_DEFAULT_MS = 15000
# hb_cast_at: the wait for the spell_cast_begin, the extra look for the hit after the resolve, and the circle pages of the book.
CAST_ARM_WAIT_MS = 3000
# hb_cast_at: the longest wait for the wind-up to end (HB-0447: the aim opens only then), and its poll step.
CAST_AIM_OPEN_WAIT_MS = 5000
CAST_AIM_OPEN_POLL_S = 0.05
CAST_HIT_WAIT_MS = 1500
SPELL_BOOK_CIRCLES = 10
SPELL_BOOK_OPEN_WAIT_S = 2.0
# get_state keys a client may carry the armed spell under (HB-0361: not there yet; asked in this order).
ARMED_SPELL_KEYS = ("armed_spell", "spell_armed", "armed_spell_id")
# the journal events a cast leaves, listed in hb_cast_at's reply.
CAST_EVENT_FILTERS = (
	("visual.effect", ("spell_cast_begin", "spell_release_sent", "spell_cast_resolved", "spell_cast_cancelled",
	                   "spell_cancel_sent")),
	("visual.damage", ("spell_hit",)),
	("client.warnings", ("cast_click_ignored",)),
	("net.packet", ("action_notice",)),
)


def unknown_keys_problem(tool, arguments, allowed, hints=None):
	"""An error string when `arguments` carries a key outside `allowed`, else None. The point is that a
	misspelt or unsupported key is REFUSED: dropping it silently ran a different query than the one asked."""
	unknown = sorted(str(key) for key in arguments if key not in allowed)
	if not unknown:
		return None
	notes = []
	for key in unknown:
		if hints and key in hints:
			notes.append("'{}' -> '{}'".format(key, hints[key]))
	message = "{} does not take: {}. It takes: {}.".format(tool, ", ".join(unknown), ", ".join(allowed))
	if notes:
		message += " Did you mean " + ", ".join(notes) + "?"
	return message


def step_argument_problem(tool, arguments):
	"""Checks a step's arguments can run, before any step of a batch does. None when fine."""
	if not isinstance(arguments, dict):
		return "arguments must be an object"
	if tool == "hb_journal_drain":
		return unknown_keys_problem(tool, arguments, JOURNAL_DRAIN_KEYS, JOURNAL_DRAIN_HINTS)
	if tool in WORLD_TOOL_KEYS:
		return unknown_keys_problem(tool, arguments, WORLD_TOOL_KEYS[tool]) or world_tool_problem(tool, arguments)
	if tool == "hb_wait_for":
		try:
			trail_limit(arguments)
		except ValueError as error:
			return str(error)
		return max_events_problem(arguments)
	if tool == "hb_screenshot":
		return parse_screenshot_options(arguments)[1]
	if tool == "hb_flow":
		mcp_flows, problem = load_mcp_flows()
		if mcp_flows is None:
			return None  # the flow itself will report the broken library when it runs
		try:
			mcp_flows.check_arguments(arguments)
		except Exception as error:
			return str(error)
	return None


def max_events_problem(arguments):
	"""hb_wait_for's max_events argument: None when absent or valid, else what is wrong with it."""
	if arguments.get("max_events") is None:
		return None
	count = parse_int(arguments["max_events"])
	if count is None or not 1 <= count <= WAIT_FOR_MAX_EVENTS:
		return "hb_wait_for max_events must be a whole number 1..{} (got {!r})".format(
			WAIT_FOR_MAX_EVENTS, arguments["max_events"])
	if count > 1 and any(not isinstance(item, dict) or "state" in item for item in arguments.get("conditions") or []):
		return "hb_wait_for max_events counts EVENTS: every condition must be an event condition, not a state one"
	return None


def world_tool_problem(tool, arguments):
	"""Argument values a world tool cannot run without, checked before a batch starts. None when fine."""
	if tool in ("hb_walk_to", "hb_tile_to_screen"):
		for key in ("x", "y"):
			if parse_int(arguments.get(key)) is None:
				return "{} needs integer tile coordinates x and y (got {}={!r})".format(tool, key, arguments.get(key))
	if tool == "hb_click_entity" and parse_int(arguments.get("object_id")) is None:
		return "{} needs an integer object_id (from hb_entities)".format(tool)
	if tool == "hb_cast_at" and parse_int(arguments.get("object_id")) is None and not str(arguments.get("name") or "").strip():
		return "hb_cast_at needs an integer object_id (from hb_entities) or a 'name' resolved at call time"
	if tool == "hb_cast_at":
		if arguments.get("spell") is not None and not str(arguments["spell"]).strip():
			return "hb_cast_at 'spell' is empty"
		if arguments.get("wait") is not None and not isinstance(arguments["wait"], bool):
			return "hb_cast_at 'wait' must be true or false (got {!r})".format(arguments["wait"])
		timeout = parse_int(arguments.get("timeout_ms"))
		if arguments.get("timeout_ms") is not None and (timeout is None or not 0 < timeout <= WAIT_FOR_MAX_MS):
			return "hb_cast_at timeout_ms must be 1..{}".format(WAIT_FOR_MAX_MS)
	if tool == "hb_entities":
		kinds = arguments.get("kinds")
		if kinds is not None and (not isinstance(kinds, list) or any(kind not in ENTITY_KINDS for kind in kinds)):
			return "hb_entities 'kinds' must be a list of {}".format(", ".join(ENTITY_KINDS))
	if tool == "hb_set_audio":
		for key in AUDIO_VOLUME_KEYS:
			if key in arguments and (parse_int(arguments[key]) is None or not 0 <= parse_int(arguments[key]) <= 100):
				return "hb_set_audio {} must be a whole number 0..100 (got {!r})".format(key, arguments[key])
	if tool == "hb_light_clock":
		try:
			parse_light_clock(arguments.get("seconds", "missing"))
		except ValueError as error:
			return str(error)
	if tool == "hb_set_god" and not isinstance(arguments.get("on"), bool):
		return "hb_set_god needs 'on': true or false (got {!r})".format(arguments.get("on"))
	if tool == "hb_close_window":
		if not str(arguments.get("name") or "").strip() and parse_int(arguments.get("dialog_id")) is None:
			return "hb_close_window needs 'name' (a dialog name such as inventory, or 'all') or an integer 'dialog_id'"
		if str(arguments.get("name") or "").strip() and arguments.get("dialog_id") is not None:
			return "hb_close_window takes 'name' or 'dialog_id', not both"
	if tool == "hb_walk_to":
		timeout = parse_int(arguments.get("timeout_ms"))
		if arguments.get("timeout_ms") is not None and (timeout is None or not 0 < timeout <= WAIT_FOR_MAX_MS):
			return "hb_walk_to timeout_ms must be 1..{}".format(WAIT_FOR_MAX_MS)
	if tool == "hb_set_staff_tags" and not isinstance(arguments.get("visible"), bool):
		return "hb_set_staff_tags needs 'visible': true or false (got {!r})".format(arguments.get("visible"))
	if tool == "hb_fishing_auto_strike":
		if arguments.get("on") not in ("pull", "nibble", "off"):
			return "hb_fishing_auto_strike needs 'on': pull, nibble or off (got {!r})".format(arguments.get("on"))
		delay = parse_int(arguments.get("delay_ms", 0))
		if delay is None or not 0 <= delay <= 5000:
			return "hb_fishing_auto_strike delay_ms must be a whole number 0..5000 (got {!r})".format(
				arguments.get("delay_ms"))
	if tool == "hb_set_time_of_day":
		try:
			parse_time_of_day(arguments.get("t01"))
		except ValueError as error:
			return str(error)
	if tool == "hb_path_bench":
		return path_bench_problem(arguments)
	if tool == "hb_press_bench":
		repeats = parse_int(arguments.get("repeats"))
		if arguments.get("repeats") is not None and (repeats is None or not 1 <= repeats <= PRESS_BENCH_MAX_REPEATS):
			return "hb_press_bench repeats must be a whole number 1..{} (got {!r})".format(
				PRESS_BENCH_MAX_REPEATS, arguments["repeats"])
		if arguments.get("live_path") is not None and arguments["live_path"] not in PRESS_LIVE_PATHS:
			return "hb_press_bench live_path must be old or new (got {!r})".format(arguments["live_path"])
		if arguments.get("display_id") is not None and parse_int(arguments["display_id"]) is None:
			return "hb_press_bench display_id must be a whole number (got {!r})".format(arguments["display_id"])
	if tool == "hb_inject" and arguments.get("type") not in INJECT_TYPES:
		return "hb_inject type must be one of {} (got {!r})".format(", ".join(INJECT_TYPES), arguments.get("type"))
	return None


def path_bench_problem(arguments):
	"""hb_path_bench's repeats and fixture path, checked before anything runs. None when fine."""
	repeats = parse_int(arguments.get("repeats"))
	if arguments.get("repeats") is not None and (repeats is None or not 1 <= repeats <= PATH_BENCH_MAX_REPEATS):
		return "hb_path_bench repeats must be a whole number 1..{} (got {!r})".format(
			PATH_BENCH_MAX_REPEATS, arguments["repeats"])
	fixture = arguments.get("fixture")
	if fixture is not None:
		if not isinstance(fixture, str) or not fixture.strip():
			return "hb_path_bench fixture must be a file path"
		if not inside_any(fixture_path(fixture), [repo_root()]):
			return "hb_path_bench fixture must be inside the repo (scratch: .claude/scratch/<role>/<task>/)"
	return None


def fixture_path(value):
	"""A fixture path as the client needs it: absolute, a relative one taken from the repo root."""
	return value if os.path.isabs(value) else os.path.join(repo_root(), value)


def parse_int(value):
	"""An int from an int / integral float / numeric string, else None (bool is not a number here)."""
	if isinstance(value, bool) or value is None:
		return None
	try:
		return int(value)
	except (TypeError, ValueError):
		return None


def parse_time_of_day(value):
	"""hb_set_time_of_day's t01 as a float in [0, 1] or the string 'off'; ValueError names what is accepted."""
	if isinstance(value, str) and value.strip().lower() == "off":
		return "off"
	if isinstance(value, bool) or value is None:
		raise ValueError("hb_set_time_of_day needs t01: a number 0..1, or 'off'")
	try:
		number = float(value)
	except (TypeError, ValueError):
		raise ValueError("hb_set_time_of_day t01 must be a number 0..1 or 'off' (got {!r})".format(value))
	if not (0.0 <= number <= 1.0):
		raise ValueError("hb_set_time_of_day t01 must be within 0..1 (got {:g})".format(number))
	return number


def parse_light_clock(value):
	"""hb_light_clock's seconds as a finite float, or None to unpin ('off' / null); ValueError names what is accepted."""
	if value is None or (isinstance(value, str) and value.strip().lower() == "off"):
		return None
	if isinstance(value, bool) or value == "missing":
		raise ValueError("hb_light_clock needs seconds: a number, or null / 'off' to unpin")
	try:
		number = float(value)
	except (TypeError, ValueError):
		raise ValueError("hb_light_clock seconds must be a number or null / 'off' (got {!r})".format(value))
	if number != number or number in (float("inf"), float("-inf")):
		raise ValueError("hb_light_clock seconds must be finite (got {!r})".format(value))
	return number


def load_mcp_flows():
	"""hb_driver.mcp_flows, imported on first use (a fault in hb_driver must never stop the server), as
	(module, None) or (None, error text). The running module is registered as `agent_mcp` first:
	hb_driver imports agent_mcp for bridge_client, and without the alias that would load a second copy
	of this file (with its own registry)."""
	sys.modules.setdefault("agent_mcp", sys.modules[__name__])
	try:
		from hb_driver import mcp_flows
	except Exception as error:
		return None, "hb_driver could not be loaded: {}".format(error)
	return mcp_flows, None


class image_collector:
	"""The screenshot images one hb_batch / hb_parallel call returns (HB-0119), capped so a long batch
	cannot flood the reply. Thread-safe: every hb_parallel group shares one."""

	def __init__(self, limit=BATCH_MAX_IMAGES):
		self.limit = limit
		self._lock = threading.Lock()
		self._images = []

	def add(self, data, mime_type):
		"""The 1-based number the image will have in the reply, or None when the cap is reached."""
		with self._lock:
			if len(self._images) >= self.limit:
				return None
			self._images.append({"type": "image", "data": data, "mimeType": mime_type or "image/png"})
			return len(self._images)

	def blocks(self):
		with self._lock:
			return list(self._images)


# the keys a batch step itself may carry; everything else belongs under 'arguments'
STEP_KEYS = frozenset({"tool", "arguments", "args", "return"})


def validate_steps(steps, what):
	"""Check a whole step list before any of it runs. Returns an error string, or None.

	A typo in step 9 should not leave the client half-driven through steps 1-8 - and for hb_parallel
	it matters more still, because a group that fails validation after the barrier would have let the
	other clients act alone.
	"""
	if not isinstance(steps, list) or not steps:
		return "{} needs a non-empty 'steps' array".format(what)
	if len(steps) > BATCH_MAX_STEPS:
		return "{} takes at most {} steps ({} given)".format(what, BATCH_MAX_STEPS, len(steps))
	for index, step in enumerate(steps):
		if not isinstance(step, dict):
			return "{}: step {} is not an object".format(what, index)
		tool = step.get("tool")
		if tool not in BATCH_ALLOWED:
			return "{} step {}: '{}' cannot be used in a batch. Allowed: {}".format(
				what, index, tool, ", ".join(sorted(BATCH_ALLOWED)))
		flat = sorted(key for key in step if key not in STEP_KEYS)
		if flat:
			# HB-0718: flat fields used to be dropped, so a click ran at (0,0) instead of failing
			return ("{} step {} [{}]: field(s) {} sit at the step's top level and would be ignored - put them "
			        "under 'arguments', e.g. {{\"tool\":\"{}\",\"arguments\":{{...}}}}".format(
				        what, index, tool, ", ".join(flat), tool))
		step_args = step.get("arguments") or step.get("args") or {}
		problem = step_argument_problem(tool, step_args)
		if problem:
			return "{} step {} [{}]: {}".format(what, index, tool, problem)
		if step_args.get("client"):
			try:
				REGISTRY.get(str(step_args["client"]))
			except unknown_client as error:
				return "{} step {}: {}".format(what, index, error)
	return None


def terse_step_notes(outcome):
	"""A terse batch step keeps only the error tap's notes (every text block after the first, the result)."""
	blocks = [block.get("text", "") for block in outcome.get("content") or []
	          if block.get("type") == "text" and block.get("text")]
	return "\n".join(blocks[1:])


def execute_steps(steps, default_client, lines, tag="", marks=None, images=None, terse=False):
	"""Run a validated step list in order, stopping at the first failure.

	Appends one line per step to `lines` and returns (failed_index or None, elapsed_seconds).
	Stopping rather than pressing on is deliberate: a click that missed leaves the client in a state
	the rest of the sequence was not written for, and blundering through produces a confusing end
	state instead of a clear error.

	`marks` collects a perf_counter reading as each step's reply lands - hb_parallel reads the first
	of them, because when the client ANSWERED is when the client actually pumped the command, which
	is the only per-client timing this process can observe at all.

	`images` (an image_collector) receives each step's image blocks - a screenshot step's picture - up
	to its cap; a step past the cap says so and still has its saved file. Without one images are dropped.
	"""
	started = time.monotonic()
	for index, step in enumerate(steps):
		tool = step["tool"]
		step_args = step.get("arguments") or step.get("args") or {}
		elapsed = time.monotonic() - started
		if elapsed > BATCH_MAX_TOTAL_SECONDS:
			lines.append("{}step {} [{}]: ABORTED - sequence exceeded its {:g}s budget".format(
				tag, index, tool, BATCH_MAX_TOTAL_SECONDS))
			return index, time.monotonic() - started
		try:
			target = resolve_client(step_args, default_client)
			target.tap.arm()
			acted = dispatch_tool(tool, step_args, target)
			if marks is not None:
				marks.append(time.perf_counter())  # before the tap, which is a round trip of its own
			outcome = append_tapped_errors(target, acted)
		except Exception as error:  # a step must not kill the sequence or the server
			lines.append("{}step {} [{}]: ERROR {}".format(tag, index, tool, error))
			return index, time.monotonic() - started

		# EVERY text block: the first is the result, a later one is the error tap's note on this step.
		text = "\n".join(block.get("text", "") for block in outcome.get("content") or []
		                 if block.get("type") == "text" and block.get("text"))
		if outcome.get("isError"):
			lines.append("{}step {} [{}]: FAILED".format(tag, index, tool))
			lines.append(indent_block(text))
			return index, time.monotonic() - started

		lines.append("{}step {} [{}]: ok".format(tag, index, tool))
		if terse and not step.get("return"):
			text = terse_step_notes(outcome)
		if text:
			lines.append(indent_block(text))
		for block in outcome.get("content") or []:
			if block.get("type") != "image" or images is None:
				continue
			number = images.add(block.get("data"), block.get("mimeType"))
			if number is None:
				lines.append(indent_block("(image NOT attached: one batch returns at most {} - the file is saved)"
				                          .format(images.limit)))
			else:
				lines.append(indent_block("[image {} attached below]".format(number)))
	return None, time.monotonic() - started


def run_batch(arguments):
	"""Execute a sequence of steps in order against one client, stopping at the first failure.

	The batch's own 'client' (else the session default) is the fallback for every step; a step may
	name a different one. Steps still run strictly in order - a batch that spans clients is a
	SEQUENCE, not a simultaneous action, and hb_parallel is what exists for the latter.
	"""
	steps = arguments.get("steps")
	problem = validate_steps(steps, "hb_batch")
	if problem:
		return tool_error(problem)
	try:
		default_client = resolve_client(arguments)
	except unknown_client as error:
		return tool_error(str(error))

	lines = []
	images = image_collector()
	failed, elapsed = execute_steps(steps, default_client, lines, images=images, terse=bool(arguments.get("terse")))
	if failed is not None:
		lines.append("")
		lines.append("BATCH FAILED at step {} of {} - the remaining {} step(s) were NOT run, so the "
		             "client is left partway through the sequence.".format(
			             failed, len(steps), len(steps) - failed - 1))
		return {"content": text_content("\n".join(lines)) + images.blocks(), "isError": True}
	lines.append("")
	lines.append("batch complete: {} step(s) in {:.2f}s".format(len(steps), elapsed))
	return {"content": text_content("\n".join(lines)) + images.blocks()}


def indent_block(text):
	"""Indent a step's own output so the step boundaries stay readable in a long batch."""
	return "\n".join("    " + line for line in (text or "").splitlines())


# --- simultaneous dispatch across clients ----------------------------------------------------------
# The capability the multi-player tests are actually for: "A and B both step onto the same tile".
#
# What this can honestly offer is a NARROW, MEASURED window, not simultaneity. Every group's socket is
# connected and warmed before anything is sent, so no TCP handshake or first-touch cost lands inside
# the window; the groups then release from a barrier and each writes its first command immediately.
# The reply reports the measured spread between those writes.
#
# What it cannot offer: the client only acts when its own next frame pumps the bridge, which is up to
# one frame of independent per-client jitter (~16ms at 60fps), and the order the SERVER processes the
# resulting packets in is not controlled by anything here. This narrows a race; it does not decide one.

PARALLEL_MAX_GROUPS = 8
PARALLEL_BARRIER_SECONDS = 30.0
PARALLEL_JOIN_SLACK_SECONDS = 30.0


def parallel_groups(arguments):
	"""Normalize hb_parallel's two input shapes into [(client, steps)]. Returns (groups, error)."""
	raw = arguments.get("groups")
	if not raw:
		handles = arguments.get("clients")
		shared = arguments.get("steps")
		if not handles or not shared:
			return None, ("hb_parallel needs either 'groups' [{client, steps}] or the shorthand "
			              "'clients' [handles] + 'steps'")
		raw = [{"client": handle, "steps": shared} for handle in handles]
	if not isinstance(raw, list) or len(raw) < 2:
		return None, ("hb_parallel needs at least 2 groups - with one client there is nothing to "
		              "synchronize and hb_batch is the tool")
	if len(raw) > PARALLEL_MAX_GROUPS:
		return None, "hb_parallel takes at most {} groups ({} given)".format(PARALLEL_MAX_GROUPS, len(raw))

	groups = []
	seen = set()
	for index, entry in enumerate(raw):
		if not isinstance(entry, dict):
			return None, "group {} is not an object".format(index)
		handle = entry.get("client")
		if not handle:
			return None, "group {} has no 'client'".format(index)
		try:
			client = REGISTRY.get(str(handle))
		except unknown_client as error:
			return None, str(error)
		if client.handle in seen:
			# Two groups on one client would run on two threads against ONE socket and one screen -
			# an interleaving nobody could reason about. Put those steps in a single group.
			return None, "client '{}' appears in more than one group".format(client.handle)
		seen.add(client.handle)
		steps = entry.get("steps")
		problem = validate_steps(steps, "hb_parallel group {} ({})".format(index, client.handle))
		if problem:
			return None, problem
		for step_index, step in enumerate(steps):
			step_args = step.get("arguments") or step.get("args") or {}
			named = step_args.get("client")
			if named and str(named) != client.handle:
				# A group is ONE client, on one thread. Letting a step reach across would put two
				# threads on one socket for reasons nobody reading the call could reconstruct.
				return None, ("hb_parallel group {} drives '{}' but step {} names '{}' - a group is "
				              "one client; give that client its own group".format(
					              index, client.handle, step_index, named))
		groups.append((client, steps))
	return groups, None


def run_parallel(arguments):
	"""Release every group at one barrier, then report what each did and how far apart they went."""
	groups, problem = parallel_groups(arguments)
	if problem:
		return tool_error(problem)

	barrier = threading.Barrier(len(groups))
	images = image_collector()
	records = []
	for client, _steps in groups:
		records.append({"client": client, "lines": [], "released": None, "failed": None,
		                "elapsed": 0.0, "error": None, "marks": []})

	def worker(index, client, steps):
		record = records[index]
		try:
			# Everything that could cost time on the first command happens HERE, before the barrier:
			# the TCP connect, one round trip through the client's frame pump, and the error tap's
			# cursor. Leave any of it after the barrier and it is measured as dispatch skew.
			client.bridge.ensure_connected()
			reply = client.bridge.request("ping", {}, timeout=BRIDGE_TIMEOUT_SECONDS)
			if not reply.get("ok", False):
				raise RuntimeError(reply.get("error", "ping rejected"))
			client.tap.arm()
		except Exception as error:
			record["error"] = "NOT DISPATCHED - warm-up failed: {}".format(error)
			barrier.abort()  # nobody goes if anybody cannot: a half-released test is worse than none
			return
		try:
			barrier.wait(timeout=PARALLEL_BARRIER_SECONDS)
		except threading.BrokenBarrierError:
			if record["error"] is None:
				record["error"] = ("NOT DISPATCHED - another group could not be warmed up, so no "
				                   "client was released")
			return
		record["released"] = time.perf_counter()
		record["failed"], record["elapsed"] = execute_steps(
			steps, client, record["lines"], tag="[{}] ".format(client.handle), marks=record["marks"],
			images=images)

	threads = []
	for index, (client, steps) in enumerate(groups):
		thread = threading.Thread(target=worker, args=(index, client, steps), daemon=True,
		                          name="hb-parallel-{}".format(client.handle))
		threads.append(thread)
		thread.start()
	for thread in threads:
		thread.join(BATCH_MAX_TOTAL_SECONDS + PARALLEL_JOIN_SLACK_SECONDS)

	released = [record for record in records if record["released"] is not None]
	lines = []
	if not released:
		lines.append("NOTHING WAS DISPATCHED - the warm-up failed, so no client acted:")
		for record in records:
			lines.append("  [{}] {}".format(record["client"].handle,
			                                record["error"] or "no result (thread did not finish)"))
		return {"content": text_content("\n".join(lines)), "isError": True}

	earliest = min(record["released"] for record in released)
	latest = max(record["released"] for record in released)
	first = min(released, key=lambda record: record["released"])["client"].handle
	last = max(released, key=lambda record: record["released"])["client"].handle
	lines.append("released {} client(s) together from one barrier".format(len(released)))
	lines.append("dispatch skew: {:.2f} ms (first {}, last {})".format(
		(latest - earliest) * 1000.0, first, last))
	for record in records:
		if record["released"] is None:
			continue
		lines.append("  {} released at +{:.2f} ms".format(
			record["client"].handle, (record["released"] - earliest) * 1000.0))
	# The second number is the one that describes the CLIENTS rather than this process: a bridge
	# command is answered by the frame that handled it, so the spread of first replies is the
	# per-client frame jitter, measured rather than assumed. It is dispatch-to-ACK, so a first step
	# that kicks off a heavy transition inflates it - measured 13 ms for a plain key press across 8
	# clients, and 1060 ms when that press started a screen change on five of them.
	acked = [record for record in released if record["marks"]]
	if len(acked) > 1:
		first_acks = [record["marks"][0] for record in acked]
		ack_base = min(first_acks)
		lines.append("first-reply spread: {:.2f} ms ({})".format(
			(max(first_acks) - ack_base) * 1000.0,
			", ".join("{} +{:.2f}".format(record["client"].handle,
			                              (record["marks"][0] - ack_base) * 1000.0)
			          for record in sorted(acked, key=lambda item: item["marks"][0]))))
	lines.append("NOTE: the skew is when each group's FIRST command left THIS process. Each client "
	             "acts on its own next frame pump, which the first-reply spread measures - about one "
	             "frame (~13 ms measured across 8 clients) for a plain action, far more if that first "
	             "step starts a heavy transition, since the spread is dispatch-to-ACK. The order the "
	             "SERVER processes the resulting packets in is not controlled here at all. This "
	             "narrows a race; it does not decide one.")

	failed_any = False
	for record in records:
		handle = record["client"].handle
		lines.append("")
		if record["released"] is None:
			failed_any = True
			lines.append("[{}] {}".format(handle, record["error"] or "no result (thread did not finish)"))
			continue
		if record["failed"] is not None:
			failed_any = True
			lines.append("[{}] FAILED at step {} - its remaining steps did not run. The other "
			             "clients were NOT rolled back: a parallel run is not atomic.".format(
				             handle, record["failed"]))
		else:
			lines.append("[{}] all steps ok in {:.2f}s".format(handle, record["elapsed"]))
		lines.extend(record["lines"])

	outcome = {"content": text_content("\n".join(lines)) + images.blocks()}
	if failed_any:
		outcome["isError"] = True
	return outcome


# --- the client registry, as a tool ----------------------------------------------------------------


def client_status(client):
	"""Ping a registered client through its OWN connection and read its screen.

	Through its own connection on purpose: the client accepts ONE bridge connection at a time, so a
	second probe socket to an already-connected client would sit unanswered in the accept backlog and
	the client would be reported unreachable when it is fine.
	"""
	status = {"client": client, "ok": False, "detail": "", "screen": "", "build": ""}
	try:
		reply = client.bridge.request("ping", {}, timeout=DISCOVER_PING_SECONDS)
	except RuntimeError as error:
		status["detail"] = str(error)
		return status
	if not reply.get("ok", False):
		status["detail"] = reply.get("error", "ping rejected")
		return status
	result = reply.get("result") or {}
	status["ok"] = True
	status["build"] = str(result.get("build", ""))
	try:
		state_reply = client.bridge.request("get_state", {}, timeout=DISCOVER_PING_SECONDS)
		if state_reply.get("ok", False):
			status["screen"] = str((state_reply.get("result") or {}).get("screen", ""))
	except RuntimeError:
		pass  # the ping already answered; a missing screen is cosmetic
	return status


def passive_probe(port):
	"""Is a bridge answering on 127.0.0.1:`port`? A bare connect, ping + get_state, close at once: nothing is kept,
	so the client's one connection is free again. A port that `netstat` shows connected to ANOTHER process is
	skipped without connecting (a second socket would just queue behind it). Never raises."""
	status = {"port": port, "ok": False, "skipped": False, "detail": "", "build": "", "screen": "", "protocol": None}
	holders = port_holders(BRIDGE_HOST, port)
	foreign = [pid for pid in (holders or {}).get("peers", []) if pid != os.getpid()]
	if foreign:
		status["skipped"] = True
		status["detail"] = "in use, connected from pid {} (left alone)".format(", ".join(str(pid) for pid in foreign))
		return status
	try:
		sock = socket.create_connection((BRIDGE_HOST, port), timeout=BRIDGE_CONNECT_SECONDS)
	except OSError as error:
		status["detail"] = str(error)
		return status
	replies = {}
	try:
		sock.settimeout(DISCOVER_PING_SECONDS)
		sock.sendall(b'{"id": 1, "cmd": "ping", "args": {}}\n{"id": 2, "cmd": "get_state", "args": {}}\n')
		buffer = b""
		deadline = time.monotonic() + DISCOVER_PING_SECONDS
		while len(replies) < 2 and time.monotonic() < deadline:
			chunk = sock.recv(65536)
			if not chunk:
				break
			buffer += chunk
			while b"\n" in buffer:
				line, _, buffer = buffer.partition(b"\n")
				try:
					message = json.loads(line.decode("utf-8"))
				except ValueError:
					continue
				if isinstance(message, dict):
					replies[message.get("id")] = message
	except (OSError, ValueError) as error:
		status["detail"] = str(error)
	finally:
		try:
			sock.close()
		except OSError:
			pass
	ping = replies.get(1) or {}
	if ping.get("ok") is True:
		result = ping.get("result") or {}
		status["ok"] = True
		status["build"] = str(result.get("build", ""))
		status["protocol"] = result.get("bridge_protocol")
		state = replies.get(2) or {}
		if state.get("ok") is True:
			status["screen"] = str((state.get("result") or {}).get("screen", ""))
	elif not status["detail"]:
		status["detail"] = "no bridge answer"
	return status


def status_of_all(clients):
	"""Status for several clients at once - a serial scan of 8 ports is 8 timeouts back to back."""
	results = [None] * len(clients)

	def probe(index, client):
		results[index] = client_status(client)

	threads = [threading.Thread(target=probe, args=(index, client), daemon=True)
	           for index, client in enumerate(clients)]
	for thread in threads:
		thread.start()
	for thread in threads:
		thread.join(DISCOVER_PING_SECONDS * 3)
	return [result or {"client": clients[index], "ok": False, "detail": "probe did not finish",
	                   "screen": "", "build": ""}
	        for index, result in enumerate(results)]


# --- who holds a bridge port ---------------------------------------------------------------------------
# A client accepts ONE bridge connection, so a port that does not answer is usually held by a stale
# handle in this registry or by another process already connected to it (another MCP server, a
# hb_flow.py run). `hb_clients register` names the holder instead of leaving a bare timeout.

LOOPBACK_HOSTS = ("127.0.0.1", "localhost", "::1")
HOLDER_QUERY_SECONDS = 10
HOLDER_COMMAND_CHARS = 200


def run_quiet(command):
	"""stdout of a helper command ('' on any failure), without flashing a console window."""
	try:
		done = subprocess.run(command, capture_output=True, text=True, timeout=HOLDER_QUERY_SECONDS,
		                      creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
	except (OSError, subprocess.SubprocessError):
		return ""
	return done.stdout or ""


def port_holders(host, port):
	"""{listeners: [pid], peers: [pid]} for a local port from `netstat -ano`: the processes LISTENING on
	it and the processes CONNECTED to it. None when it cannot be read (not Windows, not loopback, no netstat)."""
	if sys.platform != "win32" or host not in LOOPBACK_HOSTS:
		return None
	output = run_quiet(["netstat", "-ano", "-p", "TCP"])
	if not output:
		return None
	suffix = ":{}".format(port)
	listeners = set()
	peers = set()
	for line in output.splitlines():
		parts = line.split()
		if len(parts) < 5 or parts[0].upper() != "TCP" or not parts[4].isdigit():
			continue
		local, foreign, state, pid = parts[1], parts[2], parts[3].upper(), int(parts[4])
		if local.endswith(suffix) and state == "LISTENING":
			listeners.add(pid)
		elif foreign.endswith(suffix) and state == "ESTABLISHED":
			peers.add(pid)  # the OTHER end of a connection to the port: whoever is connected to it
	return {"listeners": sorted(listeners), "peers": sorted(peers)}


def process_description(pid):
	"""'image, command line' for a pid, as much as can be read (''), to say WHICH python it is."""
	image = ""
	rows = run_quiet(["tasklist", "/FI", "PID eq {}".format(pid), "/FO", "CSV", "/NH"]).strip()
	if rows.startswith('"'):
		image = rows.split('","')[0].strip('"')
	command = run_quiet(["powershell", "-NoProfile", "-NonInteractive", "-Command",
	                     "(Get-CimInstance Win32_Process -Filter 'ProcessId={}').CommandLine".format(pid)]).strip()
	command = " ".join(command.split())
	if len(command) > HOLDER_COMMAND_CHARS:
		command = command[:HOLDER_COMMAND_CHARS] + "..."
	return "{}{}".format(image or "?", ", " + command if command else "")


def describe_port_holders(host, port):
	"""One line naming who holds `host:port` (see port_holders)."""
	holders = port_holders(host, port)
	if holders is None:
		return "who holds it could not be read (netstat is only used for a loopback port on Windows)"
	parts = []
	for pid in holders["listeners"]:
		parts.append("listening: pid {} ({})".format(pid, process_description(pid)))
	for pid in holders["peers"]:
		who = "this MCP server" if pid == os.getpid() else process_description(pid)
		parts.append("connected: pid {} ({})".format(pid, who))
	if not holders["listeners"]:
		parts.insert(0, "nothing is listening on {}:{}".format(host, port))
	return "; ".join(parts)


def client_is_dead(client):
	"""True only when nothing LISTENS on the client's port (its process is gone). A port held but not answering
	(a busy or connected-elsewhere client) is not dead, and an unreadable port table proves nothing."""
	holders = port_holders(client.host, client.port)
	if holders is None:
		return False
	if holders["listeners"]:
		client.seen_listening = True
		return False
	# the grace: a client registered ahead of its launch is not dead yet
	return client.seen_listening or time.monotonic() - client.registered_at >= DEAD_CLIENT_GRACE_SECONDS


def drop_if_dead(client):
	"""Forget a registered client whose process is gone (HB-0625: a dead c1 used to stay until forgotten by
	hand). Returns True when it was dropped."""
	if REGISTRY.find_by_address(client.host, client.port) is not client or not client_is_dead(client):
		return False
	REGISTRY.forget(client.handle)
	announce_resource_list_changed()
	return True


def drop_dead_clients(statuses):
	"""Drop every unreachable client in a status list whose port has no listener; returns their handles."""
	dropped = []
	for status in statuses:
		if status["ok"]:
			status["client"].seen_listening = True
		elif drop_if_dead(status["client"]):
			status["dropped"] = True
			dropped.append(status["client"].handle)
	return dropped


def format_client_list(statuses):
	default = REGISTRY.default_handle()
	lines = ["{} client(s) registered, default '{}'".format(
		sum(1 for status in statuses if not status.get("dropped")), default or "none")]
	for status in statuses:
		client = status["client"]
		mark = "*" if client.handle == default else " "
		if status["ok"]:
			lines.append("  {} {:<6} {:<20} ok            screen={:<24} build={}".format(
				mark, client.handle, client.address(), status["screen"] or "?", status["build"] or "?"))
		elif status.get("dropped"):
			lines.append("  {} {:<6} {:<20} GONE - forgotten (nothing listens on the port)".format(
				mark, client.handle, client.address()))
		else:
			lines.append("  {} {:<6} {:<20} UNREACHABLE   {}".format(
				mark, client.handle, client.address(), status["detail"]))
	lines.append("'*' is the client an omitted 'client' selector goes to. Launch another client with "
	             "--agent-bridge-port N, then hb_clients {action:'discover'}.")
	return "\n".join(lines)


# Said when the last client is gone: nothing is re-registered behind the caller's back (a stale c1 used to
# reappear and become the default), so the way forward has to be stated.
NO_CLIENTS_NOTE = ("nothing is registered now - register one (hb_clients {action:'register', port:<N>}) or launch "
                   "a client with --agent-bridge-port=N and run hb_clients {action:'discover'}.")


def run_clients(arguments):
	action = str(arguments.get("action") or "list").lower()

	if action == "list":
		clients = REGISTRY.all()
		if not clients:
			return {"content": text_content(
				"no clients registered. hb_clients {action:'discover'} scans ports {}-{}.".format(
					BRIDGE_PORT_FIRST, BRIDGE_PORT_LAST))}
		statuses = status_of_all(clients)
		drop_dead_clients(statuses)
		text = format_client_list(statuses)
		if REGISTRY.count() == 0:
			text += "\n" + NO_CLIENTS_NOTE
		return {"content": text_content(text)}

	if action == "discover":
		ports = arguments.get("ports")
		if ports:
			try:
				ports = [int(port) for port in ports]
			except (TypeError, ValueError):
				return tool_error("hb_clients discover 'ports' must be integers in {}-{}".format(BRIDGE_PORT_FIRST, BRIDGE_PORT_LAST))
			outside = [port for port in ports if not BRIDGE_PORT_FIRST <= port <= BRIDGE_PORT_LAST]
			if outside:
				return tool_error("hb_clients discover only scans ports {}-{}; refused: {}".format(
					BRIDGE_PORT_FIRST, BRIDGE_PORT_LAST, ", ".join(str(port) for port in outside)))
		else:
			ports = list(range(BRIDGE_PORT_FIRST, BRIDGE_PORT_LAST + 1))
		# PASSIVE: a port nobody registered gets a bare connect, a ping and a close, so it never holds a client's
		# one connection and never registers anything - drive one by naming it with hb_clients register. A port
		# already registered is asked through its own connection (a second socket would never be answered).
		found = [None] * len(ports)

		def scan(index, port):
			existing = REGISTRY.find_by_address(BRIDGE_HOST, port)
			if existing is not None:
				status = client_status(existing)
				found[index] = {"port": port, "ok": status["ok"], "detail": status["detail"], "build": status["build"],
				                "screen": status["screen"], "skipped": False, "handle": existing.handle,
				                "existing": existing, "protocol": None}
			else:
				found[index] = passive_probe(port)

		threads = [threading.Thread(target=scan, args=(index, port), daemon=True) for index, port in enumerate(ports)]
		for thread in threads:
			thread.start()
		for thread in threads:
			thread.join(DISCOVER_PING_SECONDS * 3 + HOLDER_QUERY_SECONDS)
		rows = [row or {"port": ports[index], "ok": False, "detail": "probe did not finish", "skipped": False,
		                "build": "", "screen": "", "protocol": None} for index, row in enumerate(found)]
		pruned = []
		for row in rows:
			existing = row.get("existing")
			if existing is not None and not row["ok"] and existing.auto:
				REGISTRY.forget(existing.handle)
				pruned.append(existing.handle)
		if pruned:
			announce_resource_list_changed()
		lines = ["scanned {} port(s): {}".format(len(ports), ", ".join(str(port) for port in ports))]
		for row in rows:
			if row["ok"]:
				lines.append("  {} answering{}: build {}{}{}".format(
					row["port"], " (registered as {})".format(row["handle"]) if row.get("handle") else " (not registered)",
					row["build"] or "?", ", bridge_protocol {}".format(row["protocol"]) if row.get("protocol") is not None else "",
					", screen {}".format(row["screen"]) if row["screen"] else ""))
			elif row["skipped"]:
				lines.append("  {} skipped: {}".format(row["port"], row["detail"]))
		if not any(row["ok"] or row["skipped"] for row in rows):
			lines.append("  nothing answered")
		if pruned:
			lines.append("dropped (no answer): {}".format(", ".join(pruned)))
		lines.append("Nothing was connected to or registered for the unregistered ports. To drive one: "
		             "hb_clients {action:'register', port:<N>}.")
		lines.append("")
		lines.append(format_client_list(status_of_all(REGISTRY.all())))
		if REGISTRY.count() == 0:
			lines.append(NO_CLIENTS_NOTE)
		return {"content": text_content("\n".join(lines))}

	if action == "register":
		port = arguments.get("port")
		if port is None:
			return tool_error("hb_clients register needs a 'port' (the client's --agent-bridge-port)")
		port = int(port)
		host = str(arguments.get("host") or BRIDGE_HOST)
		if host not in LOOPBACK_HOSTS:
			return tool_error("hb_clients register only takes a loopback host ({}), not '{}'".format(
				", ".join(LOOPBACK_HOSTS), host))
		named = arguments.get("client")
		handle = str(named) if named else handle_for_port(port)
		holder = REGISTRY.find_by_address(host, port)
		if holder is not None and holder.handle != handle:
			# A second handle on one address would open a second connection that the client (which accepts
			# one at a time) never answers, and read as a dead client. HB-0695: report the held handle as
			# success instead (the port IS registered), and keep the entry for good once someone named it.
			if named:
				holder.auto = False
			announce_resource_list_changed()
			statuses = status_of_all(REGISTRY.all())
			return {"content": text_content("port {} is already registered as '{}' ({}) - use handle '{}' "
			                                "(no second connection opened)\n{}".format(
				port, holder.handle, holder.address(), holder.handle, format_client_list(statuses)))}
		client = REGISTRY.register(handle, host, port, auto=not named)
		announce_resource_list_changed()
		statuses = status_of_all(REGISTRY.all())
		lines = ["registered '{}' -> {}".format(handle, client.address())]
		mine = next((status for status in statuses if status["client"] is client), None)
		if mine is not None and not mine["ok"]:
			lines.append("NOT ANSWERING ({}). {}: {}".format(
				mine["detail"], client.address(), describe_port_holders(host, port)))
			lines.append("A client accepts ONE bridge connection: if another process is connected, close it, or "
			             "launch this client on a free --agent-bridge-port.")
			lines.append("Kept for now: a registration is dropped automatically only once nothing listens on its port "
			             "AFTER it was seen listening, or {} s after registering.".format(DEAD_CLIENT_GRACE_SECONDS))
		lines.append(format_client_list(statuses))
		return {"content": text_content("\n".join(lines))}

	if action == "forget":
		handle = arguments.get("client")
		if not handle:
			return tool_error("hb_clients forget needs a 'client' handle")
		try:
			REGISTRY.get(str(handle))
		except unknown_client as error:
			return tool_error(str(error))
		REGISTRY.forget(str(handle))
		announce_resource_list_changed()
		text = "forgot '{}'\n{}".format(handle, format_client_list(status_of_all(REGISTRY.all())))
		if REGISTRY.count() == 0:
			text += "\n" + NO_CLIENTS_NOTE
		return {"content": text_content(text)}

	if action == "default":
		handle = arguments.get("client")
		if not handle:
			return tool_error("hb_clients default needs a 'client' handle")
		try:
			REGISTRY.set_default(str(handle))
		except unknown_client as error:
			return tool_error(str(error))
		return {"content": text_content("default client is now '{}' - calls that omit 'client' go "
		                                "there.".format(handle))}

	return tool_error("unknown hb_clients action '{}'. Use list, discover, register, forget or "
	                  "default.".format(action))


def run_world_tool(name, arguments, target):
	"""hb_entities, hb_click_entity, hb_tile_to_screen, hb_walk_to, hb_set_time_of_day."""
	problem = (unknown_keys_problem(name, arguments, WORLD_TOOL_KEYS[name]) or world_tool_problem(name, arguments))
	if problem:
		return tool_error(problem)

	if name == "hb_entities":
		result, error = require_bridge(target, "entities", entities_args(arguments))
		if error:
			return tool_error(error)
		text = json.dumps(result) if arguments.get("raw") else format_entities(result)
		return {"content": text_content(headed(target, text))}

	if name == "hb_tile_to_screen":
		result, error = require_bridge(target, "tile_to_screen", {"x": parse_int(arguments["x"]), "y": parse_int(arguments["y"])})
		if error:
			return tool_error(error)
		text = json.dumps(result) if arguments.get("raw") else format_tile_to_screen(result)
		return {"content": text_content(headed(target, text))}

	if name == "hb_set_time_of_day":
		wanted = parse_time_of_day(arguments.get("t01"))
		result, error = require_bridge(target, "set_time_of_day", {"t01": wanted})
		if error:
			return tool_error(error)
		override = result.get("sky_time_override")
		text = "sky time override {} (sky_time01 now {}, map light {})".format(
			"OFF - the server's clock" if override is None else "{:g}".format(override),
			result.get("sky_time01"), result.get("map_light"))
		return {"content": text_content(label(target) + text)}

	if name == "hb_reload_lights":
		result, error = require_bridge(target, "reload_lights", {})
		if error:
			return tool_error(error)
		if not result.get("ok"):
			return tool_error("reload_lights failed: {}".format(result.get("error") or json.dumps(result)))
		lines = ["lights reloaded from {} ({}): {} light(s) on {} object(s), {} tile(s), {} map spot(s); {} skipped; "
			"generation {}".format(result.get("source"), result.get("override_path") or "-", result.get("lights"),
			result.get("objects"), result.get("tiles"), result.get("map_spots"), result.get("skipped"),
			result.get("generation"))]
		lines += ["warning: {}".format(w) for w in (result.get("warnings") or [])]
		return {"content": text_content(label(target) + "\n".join(lines))}

	if name == "hb_light_clock":
		result, error = require_bridge(target, "light_clock", {"seconds": parse_light_clock(arguments.get("seconds"))})
		if error:
			return tool_error(error)
		pinned_seconds = result.get("seconds")
		pinned_text = "pinned at {:g} s".format(pinned_seconds) if isinstance(pinned_seconds, (int, float)) else "pinned"
		text = "light clock {} (ambient_seconds {})".format(pinned_text if result.get("pinned")
			else "unpinned (the live clock)", result.get("ambient_seconds"))
		return {"content": text_content(label(target) + text)}

	if name == "hb_set_staff_tags":
		result, error = require_bridge(target, "set_staff_tags", {"visible": arguments["visible"]})
		if error:
			return tool_error(error)
		text = "staff tags {}".format("shown" if result.get("visible") else "HIDDEN")
		return {"content": text_content(label(target) + text)}

	if name == "hb_close_window":
		args = {"name": str(arguments["name"]).strip()} if str(arguments.get("name") or "").strip() \
			else {"dialog_id": parse_int(arguments["dialog_id"])}
		result, error = require_bridge(target, "close_window", args)
		if error:
			return tool_error(error)
		closed = result.get("closed") or []
		text = "closed: {}".format(", ".join("{} (#{})".format(row.get("name"), row.get("dialog_id")) for row in closed)) \
			if closed else "nothing closed - no open window matched (hb_widgets lists the open ones)"
		return {"content": text_content(label(target) + text)}

	if name == "hb_set_god":
		result, error = require_bridge(target, "set_god", {"on": arguments["on"]})
		if error:
			return tool_error(error)
		text = "god {} (flags wanted {:#04x}, {})".format(
			"ON" if result.get("god") else "off", result.get("flags_wanted", 0),
			"sent" if result.get("sent") else "NOT sent yet - goes out when the link is up")
		return {"content": text_content(label(target) + text)}

	if name == "hb_set_tab_width_cap":
		args = {"width": parse_int(arguments["width"])} if "width" in arguments else {}
		result, error = require_bridge(target, "set_tab_width_cap", args)
		if error:
			return tool_error(error)
		cap = "lifted" if "width" not in args else "{} px".format(args["width"])
		return {"content": text_content(label(target) + "tab width cap " + cap + " " + json.dumps(result))}

	if name == "hb_fishing_auto_strike":
		args = {"on": arguments["on"], "delay_ms": parse_int(arguments.get("delay_ms", 0))}
		result, error = require_bridge(target, "fishing_auto_strike", args)
		if error:
			return tool_error(error)
		text = "fishing auto strike {} (delay {} ms)".format(result.get("on"), result.get("delay_ms"))
		return {"content": text_content(label(target) + text)}

	if name == "hb_set_audio":
		args = {key: parse_int(arguments[key]) for key in AUDIO_VOLUME_KEYS if key in arguments}
		result, error = require_bridge(target, "set_audio", args)
		if error:
			return tool_error(error)
		text = "audio {}: master {}, music {}, effects {}{}".format(
			"now" if args else "is", result.get("master"), result.get("music"), result.get("effects"),
			"" if result.get("persisted") else " (live only: settings.json is not written)")
		return {"content": text_content(label(target) + text)}

	if name == "hb_set_status":
		if arguments.get("status") in (None, ""):
			return tool_error("hb_set_status requires 'status' (an internal name or an id)")
		args = {"status": arguments["status"]}
		for key in ("seconds", "target_id"):
			if arguments.get(key) is not None:
				args[key] = arguments[key]
		result, error = require_bridge(target, "set_status", args)
		if error:
			return tool_error(error)
		return {"content": text_content(label(target) +
			"status request sent to the server: {} for {} on {} - it arrives on the ordinary status "
			"messages, so allow a round-trip before reading the state".format(
				result.get("status"),
				"until cleared" if not result.get("duration_ms") else "{:g}s".format(result["duration_ms"] / 1000.0),
				"object {}".format(result["target_id"]) if result.get("target_id") else "yourself"))}

	if name == "hb_set_item_tint":
		if arguments.get("rgb") in (None, ""):
			return tool_error("hb_set_item_tint requires 'rgb' ([r, g, b] or 'off')")
		args = {"rgb": arguments["rgb"]}
		if arguments.get("display_id") is not None:
			args["display_id"] = arguments["display_id"]
		if arguments.get("mode"):
			args["mode"] = arguments["mode"]
		result, error = require_bridge(target, "set_item_tint", args)
		if error:
			return tool_error(error)
		return {"content": text_content(label(target) + "item tint: " + json.dumps(result))}

	if name == "hb_path_bench":
		args = {}
		if arguments.get("fixture"):
			args["fixture"] = os.path.abspath(fixture_path(arguments["fixture"]))
		if arguments.get("repeats") is not None:
			args["repeats"] = parse_int(arguments["repeats"])
		result, error = require_bridge(target, "path_bench", args, timeout=PATH_BENCH_TIMEOUT_SECONDS)
		if error:
			return tool_error(error)
		text = json.dumps(result) if arguments.get("raw") else format_path_bench(result)
		return {"content": text_content(headed(target, text))}

	if name == "hb_press_bench":
		return run_press_bench(arguments, target)

	if name == "hb_inject":
		args = {key: arguments[key] for key in WORLD_TOOL_KEYS[name] if key != "client" and key in arguments}
		result, error = require_bridge(target, "inject_message", args)
		if error:
			return tool_error(error)
		return {"content": text_content(label(target) + "injected {}: {} message(s) queued for the next pump".format(
			result.get("type"), result.get("queued")))}

	if name == "hb_walk_to":
		return run_walk_to(arguments, target)

	if name == "hb_cast_at":
		return run_cast_at(arguments, target)

	return run_click_entity(arguments, target)


def run_press_bench(arguments, target):
	"""hb_press_bench: switch the live press path, or bench one creature or every registered one."""
	if arguments.get("live_path"):
		result, error = require_bridge(target, "press_bench", {"live_path": arguments["live_path"]})
		if error:
			return tool_error(error)
		return {"content": text_content(label(target) + "live press path now " + str(result.get("live_path")))}
	args = {}
	if arguments.get("repeats") is not None:
		args["repeats"] = parse_int(arguments["repeats"])
	if arguments.get("display_id") is not None:
		ids = [parse_int(arguments["display_id"])]
	else:
		listing, error = require_bridge(target, "press_bench", {})
		if error:
			return tool_error(error)
		ids = [row.get("display_id") for row in listing.get("creatures") or []]
	results = []
	failures = []
	for display_id in ids:
		result, error = require_bridge(target, "press_bench", dict(args, display_id=display_id),
		                               timeout=PRESS_BENCH_TIMEOUT_SECONDS)
		if error:
			failures.append("{}: {}".format(display_id, error))
			continue
		results.append(result)
	text = json.dumps(results) if arguments.get("raw") else format_press_bench(results, failures)
	return {"content": text_content(headed(target, text))}


def format_press_bench(results, failures=()):
	"""press_bench replies as a table, one row per creature, the largest silhouette first."""
	def median(values):
		ordered = sorted(values)
		return ordered[len(ordered) // 2] if ordered else 0.0

	table = []
	for result in results:
		rows = [row for row in result.get("rows") or [] if "skipped" not in row]
		if not rows:
			table.append((0, "{:<26} {:>4}  no death pose ({})".format(
				str(result.get("name"))[:26], result.get("display_id"),
				", ".join(sorted({row.get("skipped", "?") for row in result.get("rows") or []})))))
			continue
		largest = max(rows, key=lambda row: row["mask"][0] * row["mask"][1])
		new_us = [row["new_us"] for row in rows]
		old_us = [row["old_us"] for row in rows]
		line = "{:<26} {:>4} {:>8} {:>7} {:>6} {:>3} {:>9.1f} {:>9.1f} {:>10.1f} {:>10.1f} {:>7.1f}x {:>9.2e} {:>9.2e} {:>6}{}".format(
			str(result.get("name"))[:26], result.get("display_id"),
			"{}x{}".format(*largest["mask"]), largest["cells"], largest["edges"], result.get("death_frames"),
			max(new_us), median(new_us), max(old_us), median(old_us),
			median(old_us) / max(median(new_us), 1e-9), max(row["max_strength_diff"] for row in rows),
			max(row["max_push_diff"] for row in rows), sum(row["cells_differing"] for row in rows),
			"" if all(row["same_size"] for row in rows) else " SIZE MISMATCH")
		table.append((largest["mask"][0] * largest["mask"][1], line))
	table.sort(key=lambda entry: -entry[0])
	lines = ["press bench: {} creature(s), last death frame, 8 directions, median of {} run(s) per shape".format(
		len(results), results[0].get("repeats") if results else "?")]
	lines.append("{:<26} {:>4} {:>8} {:>7} {:>6} {:>3} {:>9} {:>9} {:>10} {:>10} {:>8} {:>9} {:>9} {:>6}".format(
		"creature", "id", "mask", "cells", "edges", "fr", "new_max", "new_med", "old_max", "old_med", "speedup",
		"d_streng", "d_push", "differ"))
	lines.extend(line for _, line in table)
	lines.append("times in microseconds per shape; mask / cells / edges are the largest direction's")
	if failures:
		lines.append("FAILED: " + "; ".join(failures))
	return "\n".join(lines)


def format_path_bench(result):
	"""hb_path_bench's reply as a table: one row per request, candidate and pre-check setting."""
	lines = ["path bench on {} ({}x{}, {} teleport tiles, {} occupants), {} run(s) per row{}".format(
		result.get("map"), *(result.get("size") or ["?", "?"]), result.get("teleports"), result.get("occupants"),
		result.get("repeats"), "; fixture " + result["fixture"] if result.get("fixture") else "")]
	for case in result.get("cases") or []:
		lines.append("  {:<12} {} -> {}{}".format(case.get("label"), case.get("start"), case.get("goal"),
		                                         " (occupied)" if case.get("goal_occupied") else ""))
	lines.append("{:<12} {:<13} {:<5} {:>9} {:>9} {:>6} {:>7} {:>5} {:>6}  {}".format(
		"case", "candidate", "reach", "first_ms", "median_ms", "nodes", "scanned", "steps", "cost", "verdict"))
	for row in result.get("rows") or []:
		lines.append("{:<12} {:<13} {:<5} {:>9.3f} {:>9.3f} {:>6} {:>7} {:>5} {:>6}  {}{}".format(
			row.get("label"), row.get("candidate"), "on" if row.get("reach_check") else "off", row.get("first_ms", 0.0),
			row.get("median_ms", 0.0), row.get("nodes"), row.get("scanned"), row.get("steps"), row.get("cost"),
			row.get("verdict"), " BUDGET" if row.get("budget_spent") else ""))
	mismatches = result.get("mismatches") or []
	rows = result.get("rows") or []
	if mismatches:
		lines.append("MISMATCH (cost or end differs from astar_hashed): " + ", ".join(
			"{}/{}/{}".format(rows[i].get("label"), rows[i].get("candidate"), "on" if rows[i].get("reach_check") else "off")
			for i in mismatches if 0 <= i < len(rows)))
	else:
		lines.append("correctness: every candidate matches astar_hashed's path cost and end, per case and setting")
	return "\n".join(lines)


def entities_args(arguments):
	"""hb_entities' arguments on the wire, with the harness's default row cap."""
	args = {"max": ENTITIES_DEFAULT_MAX}
	if arguments.get("kinds"):
		args["kinds"] = [str(kind) for kind in arguments["kinds"]]
	for key in ("radius", "max"):
		if arguments.get(key) is not None:
			args[key] = int(arguments[key])
	if arguments.get("on_screen"):
		args["on_screen"] = True
	return args


def match_entity_by_name(rows, wanted):
	"""The nearest non-item row (rows arrive nearest first) whose name equals `wanted`, else the nearest
	whose name contains it; case-insensitive. None when nothing matches."""
	wanted = str(wanted).strip().lower()
	if not wanted:
		return None
	creatures = [row for row in rows if row.get("kind") not in ("item", "object", "fish")]
	exact = next((row for row in creatures if str(row.get("name") or "").lower() == wanted), None)
	if exact is not None:
		return exact
	return next((row for row in creatures if wanted in str(row.get("name") or "").lower()), None)


def nearest_entity_names(rows):
	"""'#id kind name dN' for the closest few non-item rows, for an unknown-target hint."""
	creatures = [row for row in rows if row.get("kind") not in ("item", "object", "fish")][:ENTITY_HINT_ROWS]
	return "; ".join("#{} {} {} d{}".format(row.get("object_id"), row.get("kind"), row.get("name"), row.get("distance"))
	                 for row in creatures) or "(none in view)"


def find_clickable_entity(target, object_id, name=None):
	"""The entity row for `object_id` (else the nearest named `name`) with a click point. Returns
	(entity, described, None) or (None, None, a tool_error result): unknown, a ground item, blocked, or
	no point. The lookup is made now, so a name never goes stale (HB-0706)."""
	result, error = require_bridge(target, "entities", {})
	if error:
		return None, None, tool_error(error)
	rows = result.get("entities") or []
	entity = next((row for row in rows if row.get("object_id") == object_id), None) if object_id is not None else None
	if entity is None and name:
		entity = match_entity_by_name(rows, name)
		if entity is not None:
			object_id = entity.get("object_id")
	if entity is None:
		if object_id is None:
			return None, None, tool_error("{}no entity named {!r} is around ({} listed). Nearest: {}".format(
				label(target), name, result.get("total", 0), nearest_entity_names(rows)))
		return None, None, tool_error("{}no entity with object_id {} is known to the client right now ({} listed). "
		                              "Ids go stale between hb_entities and the action (the entity despawned, "
		                              "respawned or left view) - pass 'name' to resolve at call time. Nearest: {}".format(
			                              label(target), object_id, result.get("total", 0), nearest_entity_names(rows)))
	described = "#{} ({} {})".format(object_id, entity.get("kind"), entity.get("name"))
	if entity.get("kind") == "item":
		return None, None, tool_error("{}entity {} is a ground item: pick it up by standing on its tile {} "
		                              "(hb_walk_to) and clicking the player's own sprite".format(
			                              label(target), described, tuple(entity.get("tile") or ())))
	if not entity.get("on_screen"):
		return None, None, tool_error("{}entity {} is off screen (distance {}): walk nearer first".format(
			label(target), described, entity.get("distance")))
	if not (entity.get("point") or {}).get("window"):
		return None, None, tool_error("{}entity {} reported no window point".format(label(target), described))
	return entity, described, None


def run_click_entity(arguments, target):
	"""Resolve an entity's click point through `entities`, refuse a blocked one, click its window pixel."""
	object_id = parse_int(arguments.get("object_id"))
	entity, described, refusal = find_clickable_entity(target, object_id)
	if refusal:
		return refusal
	window = entity["point"]["window"]
	button = "right" if arguments.get("right") else "left"
	args = {"x": int(window[0]), "y": int(window[1]), "space": "window", "button": button,
	        "shift": bool(arguments.get("shift", False)), "ctrl": bool(arguments.get("ctrl", False))}
	clicked, error = require_bridge(target, "click", args)
	if error:
		return tool_error(error)
	return {"content": text_content("{}{}-clicked entity {} at window ({},{}){}".format(
		label(target), button, described, args["x"], args["y"], format_consumed(clicked)))}


def normalize_spell(value):
	"""A spell name in one form: lower case, spaces as underscores, the book's `spell_` / `spell_book.spell_` stripped."""
	text = str(value or "").strip().lower().replace(" ", "_")
	for prefix in ("spell_book.", "spell_"):
		if text.startswith(prefix):
			text = text[len(prefix):]
	return text


def armed_spell(state):
	"""(reported, name) from get_state: whether the client reports an armed spell at all, and which ('' = none).
	HB-0361 may add the field; a client without it reports (False, '')."""
	for key in ARMED_SPELL_KEYS:
		if key in state:
			value = state[key]
			if isinstance(value, dict):
				value = value.get("name") or value.get("id") or value.get("spell")
			return True, normalize_spell(value) if value not in (None, False, 0, "") else ""
	return False, ""


def spell_row(widgets, spell):
	"""The spell book row widget for `spell` (internal name or label), or None."""
	wanted = normalize_spell(spell)
	for widget in widgets:
		identifier = str(widget.get("id", ""))
		if ".spell_" not in identifier and not identifier.startswith("spell_"):
			continue
		if normalize_spell(identifier) == wanted or normalize_spell(widget.get("label")) == wanted:
			return widget
	return None


def journal_newest_seq(target):
	result, error = require_bridge(target, "journal_query", {"max": 1})
	return 0 if error else int(result.get("newest_seq") or 0)


def submit_wait(target, conditions, timeout_ms):
	"""A wait_for sent now and collected later with settle_wait (so it is registered BEFORE the trigger)."""
	try:
		return target.bridge.submit("wait_for", {"conditions": conditions, "timeout_ms": timeout_ms}), None
	except RuntimeError as error:
		return None, "Client '{}' not reachable at {} ({})".format(target.handle, target.address(), error)


def settle_wait(pending, timeout_ms):
	"""The result dict of a submitted wait ({} when the wait failed)."""
	try:
		return (pending.wait(timeout_ms / 1000.0 + WAIT_FOR_SLACK_SECONDS).get("result")) or {}
	except RuntimeError:
		return {}


def arm_spell(target, spell, lines):
	# TODO: still speaks SDL's widget "id" key; this client's widget_click takes "name". Gated (hb_cast_at is hidden).
	"""Arm `spell` through the spell book: open it, find the row (paging the circle tabs), click it, close the
	book again. Returns (armed confirmed, cast_ms, error text). `lines` collects what was done."""
	listing, error = require_bridge(target, "widgets")
	if error:
		return False, None, error
	opened = False
	if not any(str(item.get("id", "")).startswith("spell_book.") for item in listing.get("widgets") or []):
		_, error = require_bridge(target, "widget_click", {"id": "hud.spellbook", "button": "left", "shift": False,
		                                                  "ctrl": False})
		if error:
			return False, None, "could not open the spell book (hud.spellbook): {}".format(error)
		opened = True
		deadline = time.monotonic() + SPELL_BOOK_OPEN_WAIT_S
		while time.monotonic() < deadline:
			listing, error = require_bridge(target, "widgets")
			if error:
				return False, None, error
			if any(str(item.get("id", "")).startswith("spell_book.") for item in listing.get("widgets") or []):
				break
			time.sleep(0.1)
	row = spell_row(listing.get("widgets") or [], spell)
	seen = {str(item.get("label") or item.get("id")) for item in listing.get("widgets") or []
	        if ".spell_" in str(item.get("id", ""))}
	circle = 0
	while row is None and circle < SPELL_BOOK_CIRCLES:
		_, error = require_bridge(target, "widget_click", {"id": "spell_book.circle{}".format(circle),
		                                                  "button": "left", "shift": False, "ctrl": False})
		circle += 1
		if error:
			continue
		time.sleep(0.2)
		listing, error = require_bridge(target, "widgets")
		if error:
			break
		row = spell_row(listing.get("widgets") or [], spell)
		seen |= {str(item.get("label") or item.get("id")) for item in listing.get("widgets") or []
		         if ".spell_" in str(item.get("id", ""))}
	confirmed, cast_ms, problem = False, None, None
	if row is None:
		problem = "the spell book has no row for {!r} (rows seen: {})".format(spell, ", ".join(sorted(seen)) or "none")
	elif not row.get("enabled", True):
		problem = "the spell row {} is disabled (locked or not learned)".format(row.get("id"))
	else:
		pending, error = submit_wait(target, [{"channel": "visual.effect", "type": "spell_cast_begin"}], CAST_ARM_WAIT_MS)
		if error:
			problem = error
		else:
			_, error = require_bridge(target, "widget_click", {"id": row["id"], "button": "left", "shift": False,
			                                                  "ctrl": False})
			if error:
				problem = "could not click the spell row {}: {}".format(row["id"], with_scroll_hint(error))
			settled = settle_wait(pending, CAST_ARM_WAIT_MS)
			if settled.get("satisfied"):
				confirmed = True
				cast_ms = ((settled.get("matched") or {}).get("event") or {}).get("data", {}).get("cast_ms")
	if opened:
		require_bridge(target, "widget_click", {"id": "hud.spellbook", "button": "left", "shift": False, "ctrl": False})
		lines.append("spell book was opened for the arm and closed again")
	return confirmed, cast_ms, problem


def cast_events(target, after_seq):
	"""The cast's journal events after `after_seq`, oldest first, as (event, formatted line)."""
	events = {}
	for channel, types in CAST_EVENT_FILTERS:
		result, error = require_bridge(target, "journal_query", {"after_seq": after_seq, "channels": [channel],
		                                                          "types": list(types), "max": 100})
		if error:
			continue
		for event in result.get("events") or []:
			events[event.get("seq")] = event
	return [events[seq] for seq in sorted(events)]


def wait_cast_aim_open(target):
	"""True once hb_get_state's armed_spell reports aim_open (a client without the field counts as open)."""
	deadline = time.monotonic() + CAST_AIM_OPEN_WAIT_MS / 1000.0
	while True:
		state, error = require_bridge(target, "get_state")
		armed = (state or {}).get("armed_spell") if not error else None
		if not isinstance(armed, dict) or "aim_open" not in armed or armed.get("aim_open"):
			return True
		if time.monotonic() >= deadline:
			return False
		time.sleep(CAST_AIM_OPEN_POLL_S)


def run_cast_at(arguments, target):
	"""Arm a spell (or use the armed one), click an entity by id, and report the cast from the journal."""
	object_id = parse_int(arguments.get("object_id"))
	name = str(arguments["name"]).strip() if arguments.get("name") else None
	spell = str(arguments["spell"]).strip() if arguments.get("spell") is not None else None
	timeout_ms = parse_int(arguments.get("timeout_ms")) or CAST_WAIT_DEFAULT_MS
	state, error = require_bridge(target, "get_state")
	if error:
		return tool_error(error)
	reported, armed = armed_spell(state)
	lines = []
	armed_note = ""
	if spell is None:
		if not reported:
			return tool_error("{}hb_cast_at needs 'spell': this client does not report the armed spell in "
			                  "hb_get_state, so none can be assumed".format(label(target)))
		if not armed:
			return tool_error("{}no spell is armed (hb_get_state armed_spell is empty): pass 'spell'".format(label(target)))
		armed_note = "using the armed spell {}".format(armed)
	# refuse an unreachable target BEFORE arming (an armed spell is a state the next click inherits)
	first, _, refusal = find_clickable_entity(target, object_id, name)
	if refusal:
		return refusal
	object_id = first.get("object_id")  # pinned: the post-wind-up lookup follows the same entity
	name = None  # a vanished id must refuse, never retarget a same-named entity
	seq0 = journal_newest_seq(target)
	if spell is not None:
		if reported and armed and armed == normalize_spell(spell):
			armed_note = "{} was already armed".format(armed)
		else:
			confirmed, cast_ms, problem = arm_spell(target, spell, lines)
			if problem:
				return tool_error("{}arming {!r} failed: {}".format(label(target), spell, problem))
			armed_note = "armed {}: {}".format(spell, "spell_cast_begin seen (cast_ms {})".format(cast_ms) if confirmed
			                                   else "NO spell_cast_begin within {} ms - the arm is unconfirmed".format(
				                                   CAST_ARM_WAIT_MS))
	# HB-0447: an aim click during the wind-up is ignored, so wait for the client to report the aim open.
	if not wait_cast_aim_open(target):
		lines.append("the aim did not open within {} ms; clicking anyway".format(CAST_AIM_OPEN_WAIT_MS))
	entity, described, refusal = find_clickable_entity(target, object_id, name)
	if refusal:
		return refusal
	window = entity["point"]["window"]
	if arguments.get("wait") is False:
		# HB-0718: the aim click only, so the caller can capture the charge / release; the cast is read later from the journal
		clicked, error = require_bridge(target, "click", {"x": int(window[0]), "y": int(window[1]), "space": "window"})
		if error:
			return tool_error(error)
		return {"content": text_content("{}cast at {}: {}; aim click at window ({},{}){}; NOT waiting for the cast "
		                                "(wait:false) - read spell_cast_resolved / spell_hit from hb_journal_drain".format(
			label(target), described, armed_note, window[0], window[1], format_consumed(clicked)))}
	conditions = [{"channel": "visual.effect", "type": "spell_cast_resolved"},
	              {"channel": "visual.effect", "type": "spell_cast_cancelled"},
	              {"channel": "client.warnings", "type": "cast_click_ignored"},
	              {"channel": "net.packet", "type": "packet_in", "match": {"message": "action_notice"}}]
	pending, error = submit_wait(target, conditions, timeout_ms)
	if error:
		return tool_error(error)
	started = time.monotonic()
	clicked, error = require_bridge(target, "click", {"x": int(window[0]), "y": int(window[1]), "space": "window"})
	if error:
		settle_wait(pending, timeout_ms)
		return tool_error(error)
	settled = settle_wait(pending, timeout_ms)
	took_ms = int((time.monotonic() - started) * 1000)
	ender = ((settled.get("matched") or {}).get("event") or {}) if settled.get("satisfied") else {}
	outcome = {"spell_cast_resolved": "RESOLVED", "spell_cast_cancelled": "CANCELLED",
	           "cast_click_ignored": "AIM IGNORED", "packet_in": "REFUSED (server notice)"}.get(ender.get("type"), "TIMED OUT")
	if outcome == "RESOLVED":
		hit, _ = submit_wait(target, [{"channel": "visual.damage", "type": "spell_hit", "match": {"target": object_id}}],
		                     CAST_HIT_WAIT_MS)
		if hit is not None:
			settle_wait(hit, CAST_HIT_WAIT_MS)
	events = cast_events(target, seq0)
	hits = [event for event in events if event.get("type") == "spell_hit"
	        and (event.get("data") or {}).get("target") == object_id]
	if outcome == "RESOLVED":
		outcome += " - LANDED on #{} ({} hit event(s))".format(object_id, len(hits)) if hits else " - no spell_hit on #{} seen".format(object_id)
	if outcome == "TIMED OUT":
		outcome += " after {} ms with no resolve, refusal or ignored-click event".format(timeout_ms)
	head = "{}cast at {}: {}; aim click at window ({},{}){}; outcome {} ({} ms)".format(
		label(target), described, armed_note, window[0], window[1], format_consumed(clicked), outcome, took_ms)
	if not reported:
		head += "\n(this client reports no armed spell in hb_get_state, so the arm is judged by the journal only)"
	body = [head] + ["  " + line for line in lines]
	for event in events:
		body.append("  #{} {}".format(event.get("seq"), format_event(event)))
	if not events:
		body.append("  (no cast events in the journal since the arm)")
	return {"content": text_content("\n".join(body))}


DRAG_SLOT_DEFAULT_STEPS = 4
DRAG_SLOT_MAX_STEPS = 20


def widget_center(widget):
	return widget["x"] + widget["w"] // 2, widget["y"] + widget["h"] // 2


def run_drag_slot(arguments, target):
	"""Press on the slot's art (its 'aim' point), move in legs, release at the drop point."""
	slot_id = str(arguments.get("from") or "").strip()
	if not slot_id:
		return tool_error("hb_drag_slot needs 'from': a slot widget id from hb_widgets")
	steps = parse_int(arguments.get("steps")) if arguments.get("steps") is not None else DRAG_SLOT_DEFAULT_STEPS
	if steps is None or not 1 <= steps <= DRAG_SLOT_MAX_STEPS:
		return tool_error("hb_drag_slot steps must be 1..{}".format(DRAG_SLOT_MAX_STEPS))
	result, error = require_bridge(target, "widgets")
	if error:
		return tool_error(error)
	by_id = {widget.get("id"): widget for widget in result.get("widgets") or []}
	source = by_id.get(slot_id)
	if source is None:
		return tool_error("{}unknown widget id: {} (hb_widgets lists the slots)".format(label(target), slot_id))
	if source.get("kind") != "slot" or source.get("item_id") is None:
		return tool_error("{}{} is not a slot holding an item".format(label(target), slot_id))
	if source.get("aim") is None:
		return tool_error("{}{} has no aim point: no solid pixel of its art resolves to it (hidden under another "
		                  "item?)".format(label(target), slot_id))
	start = (source["aim"]["x"], source["aim"]["y"])
	if arguments.get("to_widget"):
		drop_widget = by_id.get(str(arguments["to_widget"]))
		if drop_widget is None:
			return tool_error("{}unknown to_widget: {}".format(label(target), arguments["to_widget"]))
		end = widget_center(drop_widget)
	elif parse_int(arguments.get("to_x")) is not None and parse_int(arguments.get("to_y")) is not None:
		end = (parse_int(arguments["to_x"]), parse_int(arguments["to_y"]))
	else:
		return tool_error("hb_drag_slot needs 'to_x' and 'to_y', or 'to_widget'")
	_, error = require_bridge(target, "mouse_down", {"x": start[0], "y": start[1], "button": "left"})
	if error:
		return tool_error(error)
	for leg in range(1, steps + 1):
		_, error = require_bridge(target, "mouse_move", {"x": start[0] + (end[0] - start[0]) * leg // steps,
		                                                 "y": start[1] + (end[1] - start[1]) * leg // steps})
		if error:
			require_bridge(target, "mouse_up", {"x": start[0], "y": start[1], "button": "left"})
			return tool_error(error)
	released, error = require_bridge(target, "mouse_up", {"x": end[0], "y": end[1], "button": "left"})
	if error:
		return tool_error(error)
	return {"content": text_content("{}dragged {} from art ({},{}) to ({},{}) [logical]{}".format(
		label(target), slot_id, start[0], start[1], end[0], end[1], format_consumed(released)))}


def run_send_chat(arguments, target):
	"""Open the chat box (optionally), clear it, type the line, submit it."""
	value = str(arguments.get("value", ""))
	if not value:
		return tool_error("hb_send_chat needs a non-empty 'value'")
	if arguments.get("open"):
		_, error = require_bridge(target, "key", {"name": "enter"})
		if error:
			return tool_error(error)
	_, error = require_bridge(target, "set_text", {"value": value, "clear": True})
	if error:
		return tool_error(error)
	submit = arguments.get("submit") is not False
	if submit:
		_, error = require_bridge(target, "key", {"name": "enter"})
		if error:
			return tool_error(error)
	return {"content": text_content("{}chat line of {} chars {}".format(label(target), len(value),
	                                                                        "sent" if submit else "typed (not submitted)"))}


def run_walk_to(arguments, target):
	"""walk_to: the client answers when the walk settles (wait:true) or at once (default)."""
	x, y = parse_int(arguments["x"]), parse_int(arguments["y"])
	wait = bool(arguments.get("wait"))
	timeout_ms = parse_int(arguments.get("timeout_ms")) or WALK_WAIT_DEFAULT_MS
	args = {"x": x, "y": y, "wait": wait}
	if wait:
		args["timeout_ms"] = timeout_ms
	reply, error = require_bridge(target, "walk_to", args, timeout=(timeout_ms / 1000.0) + WAIT_FOR_SLACK_SECONDS if wait else None)
	if error:
		return tool_error(error)
	# a block or a timeout is a RESULT, not an error: the text says which
	return {"content": text_content(label(target) + format_walk_to(reply))}


def run_flow_tool(arguments, target):
	"""hb_flow: one hb_driver flow over THIS server's connection to the client (a client accepts one).

	Imported on first use, so a fault in hb_driver can never stop the server or its other tools. The
	running module is registered as `agent_mcp` first: hb_driver imports agent_mcp for bridge_client,
	and without the alias that would load a second copy of this file (with its own registry).
	"""
	mcp_flows, problem = load_mcp_flows()
	if mcp_flows is None:  # a broken library must not break the server
		return tool_error(problem)
	flow = mcp_flows.normalize_flow(arguments.get("flow", ""))
	try:
		result = mcp_flows.run(arguments, target.bridge)
	except Exception as error:
		return tool_error("{}flow '{}' failed: {}".format(label(target), flow, error))
	text = json.dumps(result, indent=2, sort_keys=True, ensure_ascii=True, default=str)
	return {"content": text_content(headed(target, "flow '{}' ok\n{}".format(flow, text)))}


def append_tapped_errors(client, outcome):
	"""Fold any errors that occurred on this client during this call into its own result."""
	events = client.tap.collect()
	if not events:
		return outcome
	lines = ["", "!! {}{} ERROR(S) OCCURRED IN THE CLIENT DURING THIS CALL:".format(
		label(client), len(events))]
	for event in events[:TAP_MAX_REPORTED]:
		lines.append("   " + format_event(event))
	if len(events) > TAP_MAX_REPORTED:
		lines.append("   ... and {} more - hb_journal_drain {{channels:['client.errors']}}"
		             .format(len(events) - TAP_MAX_REPORTED))
	content = list(outcome.get("content") or [])
	content.append({"type": "text", "text": "\n".join(lines)})
	tapped = dict(outcome)
	tapped["content"] = content
	return tapped


def call_tool(name, arguments):
	"""Dispatch a tool and fold in any client errors that happened while it ran.

	Sequences (hb_batch, hb_parallel) tap per STEP, inside their own runner, so an error is
	attributed to the action that caused it; everything else taps once here, on its own client.
	"""
	arguments = arguments or {}
	# These address clients themselves - a batch per step, hb_parallel per group, hb_clients by
	# handle, and the session-state pair which accept '*' for every client. They also touch no
	# bridge command of their own, so there is nothing here for the tap to attribute.
	if name in ("hb_batch", "hb_parallel", "hb_clients", "hb_stream", "hb_subscribe"):
		return dispatch_tool(name, arguments)
	try:
		target = resolve_client(arguments)
	except unknown_client as error:
		return tool_error(str(error))
	target.tap.arm()
	return append_tapped_errors(target, dispatch_tool(name, arguments, target))


def dispatch_tool(name, arguments, target=None):
	if target is not None:
		commands = client_commands(target)
		reason = unavailable_reason(name, commands)
		if reason:
			return tool_error("{} (client '{}'{})".format(
				reason, target.handle, ", ping lists {} commands".format(len(commands)) if commands is not None else ""))
	return _dispatch_tool(name, arguments, target)


def _dispatch_tool(name, arguments, target=None):
	"""Run one tool. `target` is the already-resolved client when the caller resolved it."""
	arguments = arguments or {}

	# The three that address clients themselves rather than being addressed AT one.
	if name == "hb_batch":
		return run_batch(arguments)

	if name == "hb_parallel":
		return run_parallel(arguments)

	if name == "hb_clients":
		return run_clients(arguments)

	if name == "hb_wait":
		# Handled here, not in the client, so it is the one action that needs no client at all.
		milliseconds = max(0, min(int(arguments.get("ms", 0)), 10000))
		time.sleep(milliseconds / 1000.0)
		return {"content": text_content("waited {} ms".format(milliseconds))}

	# The session-state pair take '*' for every client; everything else addresses exactly one.
	if name in ("hb_subscribe", "hb_stream"):
		try:
			targets = resolve_clients(arguments, target)
		except unknown_client as error:
			return tool_error(str(error))
	else:
		try:
			target = resolve_client(arguments, target)
		except unknown_client as error:
			return tool_error(str(error))

	if name == "hb_subscribe":
		channels = [str(item) for item in (arguments.get("channels") or []) if str(item) != "*"]
		lines = []
		for client in targets:
			try:
				client.pump.set_subscription(channels, arguments.get("filter"), arguments.get("exclude"))
			except re.error as error:
				# Name the fault: a bad pattern that silently matched nothing would look like a dead game.
				return tool_error("bad regex: {}".format(error))
			lines.append(headed(client, "subscription set\n" +
			                    json.dumps(client.pump.describe_subscription(), indent=2)))
		lines.append("\nPatterns are matched against '<channel>/<type> <compact json data>'.")
		return {"content": text_content("\n".join(lines))}

	if name == "hb_stream":
		enabled = bool(arguments.get("enabled", False))
		for client in targets:
			client.pump.set_streaming(enabled)
		# Naming the clients is only meaningful once there is more than one; a single-client session
		# reads exactly what it always did.
		who = (" for " + ", ".join(client.handle for client in targets)) if REGISTRY.count() > 1 else ""
		if enabled:
			return {"content": text_content(
				"streaming on{} - draining the client journal every {:g}s and republishing each event as a "
				"notification. The client's ring buffer remains the record, so nothing is lost between ticks."
				.format(who, JOURNAL_POLL_SECONDS))}
		return {"content": text_content("streaming off{}".format(who))}

	if name == "hb_wait_for":
		conditions = arguments.get("conditions") or []
		if not isinstance(conditions, list) or not conditions:
			return tool_error("hb_wait_for needs a non-empty 'conditions' array")
		requested_ms = int(arguments.get("timeout_ms", 0))
		state_form = any(isinstance(item, dict) and "state" in item for item in conditions)
		if requested_ms < 0 or (requested_ms == 0 and state_form):
			return tool_error("hb_wait_for needs a positive 'timeout_ms' (0 is allowed only with event conditions)")
		if requested_ms > WAIT_FOR_MAX_MS:
			return tool_error("timeout_ms {} exceeds the {} ms maximum the client will hold a wait for"
			                  .format(requested_ms, WAIT_FOR_MAX_MS))
		if len(conditions) > WAIT_FOR_MAX_CONDITIONS:
			return tool_error("at most {} conditions ({} given)".format(
				WAIT_FOR_MAX_CONDITIONS, len(conditions)))
		try:
			trail = trail_limit(arguments)
		except ValueError as error:
			return tool_error("hb_wait_for: {}".format(error))
		problem = max_events_problem(arguments)
		if problem:
			return tool_error(problem)
		args = {"conditions": conditions, "timeout_ms": requested_ms}
		if arguments.get("since") is not None:
			args["since"] = int(arguments["since"])
		if (parse_int(arguments.get("max_events")) or 1) > 1 and requested_ms > 0:
			return run_wait_many(arguments, args, target, requested_ms)
		# The client answers its own timeout, so allow slack beyond it before calling the transport
		# dead - otherwise a wait that legitimately ran its full course reads as a broken bridge.
		result, error = require_bridge(
			target, "wait_for", args, timeout=(requested_ms / 1000.0) + WAIT_FOR_SLACK_SECONDS)
		if error:
			return tool_error(error)
		return {"content": text_content(headed(target, format_wait_for(result, trail)))}

	if name == "hb_flow":
		return run_flow_tool(arguments, target)

	if name in WORLD_TOOL_KEYS:
		return run_world_tool(name, arguments, target)

	if name == "hb_reload_pivots":
		args = {}
		for key in ("archive", "entry", "dir"):
			if arguments.get(key):
				args[key] = str(arguments[key])
		result, error = require_bridge(target, "reload_pivots", args)
		if error:
			return tool_error(error)
		return {"content": text_content(headed(target, format_reload_pivots(result)))}

	if name == "hb_kill_player":
		# The request only leaves the client here; the death arrives from the server frames later, as the
		# ordinary attack_event + entity_hp. Say so in the reply so nobody reads "ok" as "already dead"
		# and screenshots the living character.
		_sent, error = require_bridge(target, "kill_player")
		if error:
			return tool_error(error)
		return {"content": text_content(label(target) +
			"kill request sent to the server - death arrives on the wire (attack_event killed=1 + "
			"entity_hp hp=0), so allow a round-trip before reading the state")}

	if name == "hb_journal_drain":
		# The name is kept so no caller breaks, but it is a QUERY now: live with no window, history with
		# one, and it consumes nothing - so it has no cursor to resume from and nothing to fold back.
		problem = unknown_keys_problem(name, arguments, JOURNAL_DRAIN_KEYS, JOURNAL_DRAIN_HINTS)
		if problem:
			return tool_error(problem)
		args = {}
		for key in ("max", "from_ms", "to_ms", "after_seq", "before_seq", "object_id"):
			if arguments.get(key) is not None:
				args[key] = int(arguments[key])
		if "max" not in args and arguments.get("limit") is not None:
			args["max"] = int(arguments["limit"])  # HB-0696: 'limit' is an alias of 'max'
		if arguments.get("message_contains"):
			args["message_contains"] = str(arguments["message_contains"])
		if arguments.get("channels"):
			args["channels"] = [str(item) for item in arguments["channels"]]
		for key in ("types", "exclude_types"):
			if arguments.get(key):
				args[key] = [str(item) for item in arguments[key]]
		result, error = require_bridge(target, "journal_query", args)
		if error:
			return tool_error(error)
		return {"content": text_content(headed(target, format_journal(result)))}

	if name == "hb_journal_config":
		args = {}
		if isinstance(arguments.get("channels"), dict):
			args["channels"] = {str(k): bool(v) for k, v in arguments["channels"].items()}
		if arguments.get("capacity") is not None:
			args["capacity"] = int(arguments["capacity"])
		if arguments.get("test_log") is not None:
			if arguments["test_log"] not in ("warn", "error"):
				return tool_error("hb_journal_config test_log must be \"warn\" or \"error\"")
			args["test_log"] = arguments["test_log"]
		# No arguments is a READ-BACK, not a mistake: the client applies nothing and answers with the
		# resolved concrete channel map, which is the only way to list the channels from the bridge.
		# This used to be rejected, so the channel list had to be read out of agent_journal.h instead.
		result, error = require_bridge(target, "journal_config", args)
		if error:
			return tool_error(error)
		return {"content": text_content(headed(target, json.dumps(result, indent=2)))}

	if name == "hb_widgets":
		result, error = require_bridge(target, "widgets")
		if error:
			return tool_error(error)
		result = filter_widgets(normalize_widgets(result), arguments)
		return {"content": text_content(headed(target, format_widgets(result)))}

	if name == "hb_click_widget":
		widget_id = str(arguments.get("id", "")).strip()
		if not widget_id:
			return tool_error("hb_click_widget needs an 'id' - call hb_widgets to see what is on screen")
		button, error = click_button(arguments)
		if error:
			return tool_error(error)
		args = {
			"name": widget_id,
			"button": button,
			"shift": bool(arguments.get("shift", False)),
			"ctrl": bool(arguments.get("ctrl", False)),
		}
		result, error = require_bridge(target, "widget_click", args)
		if error:
			return tool_error(with_scroll_hint(error))
		# Echo the resolved point: it is what a screenshot can be checked against, and it is the only
		# evidence that the id landed where the caller believed it would.
		return {"content": text_content("{}{}-clicked widget '{}' at ({}, {}) [logical]{}".format(
			label(target), button, result.get("widget", widget_id), result.get("x"), result.get("y"),
			format_consumed(result)))}

	if name == "hb_modifiers":
		# Only the keys GIVEN are sent, so the client can leave the others latched as they were. A
		# call with none of them is therefore a read-back of what is currently held.
		args = {}
		for key in ("shift", "ctrl", "alt"):
			if arguments.get(key) is not None:
				args[key] = bool(arguments[key])
		result, error = require_bridge(target, "modifiers", args)
		if error:
			return tool_error(error)
		held = [key for key in ("shift", "ctrl", "alt") if result.get(key)]
		# Name what is held rather than printing the map: a latch left on is the failure mode here,
		# and "held: alt" reads as a warning where {"alt": true, ...} reads as a receipt.
		return {"content": text_content("{}held: {}".format(
			label(target), " + ".join(held) if held else "nothing (no modifier latched)"))}

	if name == "hb_ping":
		result, error = require_bridge(target, "ping")
		if error:
			return tool_error(error)
		note_commands(target, result)
		listed = result.get("commands")
		text = "{}ok - build: {}, bridge_protocol {}, {} command(s)".format(
			label(target), result.get("build", "unknown"), result.get("bridge_protocol", "?"),
			len(listed) if isinstance(listed, list) else "?")
		if result.get("bridge_protocol") not in SUPPORTED_BRIDGE_PROTOCOLS:
			text += "\nWARNING: this server speaks bridge_protocol {}; the client reports {}".format(
				" or ".join(str(number) for number in SUPPORTED_BRIDGE_PROTOCOLS), result.get("bridge_protocol", "none"))
		return {"content": text_content(text)}

	if name == "hb_self_tests":
		result, error = require_bridge(target, "self_tests", timeout=SELF_TESTS_TIMEOUT_SECONDS)
		if error:
			return tool_error(error)
		lines = ["self-tests: {}".format("ok" if result.get("ok") else "FAIL")]
		for suite in result.get("suites", []):
			lines.append("  {}: {}/{} {}".format(suite.get("name"), suite.get("passed", 0), suite.get("total", 0),
			                                     "ok" if suite.get("ok") else "FAIL"))
			lines.extend("    " + fail for fail in suite.get("fails", []))
		return {"content": text_content(headed(target, "\n".join(lines)))}

	if name == "hb_get_state":
		result, error = require_bridge(target, "get_state")
		if error:
			return tool_error(error)
		if arguments.get("raw"):
			text = json.dumps(result, indent=2)
		elif arguments.get("verbose"):
			text = format_state(result)
		else:
			text = format_state_compact(result)
		return {"content": text_content(headed(target, text))}

	if name == "hb_probe":
		args = with_space(arguments, {"x": int(arguments.get("x", 0)), "y": int(arguments.get("y", 0))})
		result, error = require_bridge(target, "probe_point", args)
		if error:
			return tool_error(error)
		# Compact by default: the client appends its whole frame-time report to EVERY probe (dozens of
		# lines) and a probe is called many times a run. frame_times:true brings it back.
		wanted_sections = arguments.get("sections")
		if not (arguments.get("frame_times") or wanted_sections):
			result = {key: value for key, value in result.items() if key != "frame_times"}
		elif wanted_sections and isinstance(result.get("frame_times"), dict):
			result, unknown_sections = filter_frame_sections(result, wanted_sections)
			if unknown_sections:
				result["frame_times_unknown_sections"] = unknown_sections
		# HB-0698: the draw-call counters are ~2 KB a probe; only draw_stats:true or full:true brings them back.
		if not (arguments.get("draw_stats") or arguments.get("full")):
			result = {key: value for key, value in result.items() if key != "draw_stats"}
		left_out = []
		if not arguments.get("full"):
			result, left_out = compact_probe(result)
		text = json.dumps(result)
		if left_out:
			text += "\n(compact: left out {} - full:true for everything)".format(", ".join(left_out))
		# `front` / `covered_by` (client batch 1) say what a click here reaches first; when it is not the
		# world, say so on its own line - the JSON line alone buries the one fact that explains a dead click.
		detail = result.get("detail") if isinstance(result.get("detail"), dict) else {}
		front = detail.get("front") if isinstance(detail.get("front"), dict) else {}
		if detail.get("covered_by") or (front.get("kind") not in (None, "world")):
			text += "\nCOVERED: a click here reaches {} {}, not the world".format(
				front.get("kind", "?"), detail.get("covered_by") or front.get("owner") or "")
		return {"content": text_content(headed(target, text))}

	if name == "hb_screenshot":
		return capture_screenshot(target, arguments)

	if name == "hb_mouse_move":
		args = with_space(arguments, {"x": int(arguments.get("x", 0)), "y": int(arguments.get("y", 0))})
		_, error = require_bridge(target, "mouse_move", args)
		if error:
			return tool_error(error)
		return {"content": text_content("{}moved to ({}, {}) [{}]".format(
			label(target), arguments.get("x"), arguments.get("y"), arguments.get("space", "logical")))}

	if name == "hb_click":
		button, error = click_button(arguments)
		if error:
			return tool_error(error)
		args = with_space(arguments, {
			"x": int(arguments.get("x", 0)),
			"y": int(arguments.get("y", 0)),
			"button": button,
			"shift": bool(arguments.get("shift", False)),
			"ctrl": bool(arguments.get("ctrl", False)),
		})
		result, error = require_bridge(target, "click", args)
		if error:
			return tool_error(error)
		return {"content": text_content("{}{}-clicked ({}, {}) [{}]{}".format(
			label(target), button, args["x"], args["y"], arguments.get("space", "logical"),
			format_consumed(result)))}

	# The two halves of a click. They share everything but the wire command, so they share the body:
	# a drag is mouse_down, then any number of hb_mouse_move steps, then mouse_up.
	if name in ("hb_mouse_down", "hb_mouse_up"):
		button, error = click_button(arguments)
		if error:
			return tool_error(error)
		args = with_space(arguments, {
			"x": int(arguments.get("x", 0)),
			"y": int(arguments.get("y", 0)),
			"button": button,
			"shift": bool(arguments.get("shift", False)),
			"ctrl": bool(arguments.get("ctrl", False)),
		})
		command = "mouse_down" if name == "hb_mouse_down" else "mouse_up"
		args.pop("shift", None)  # the client's mouse_down / mouse_up take no modifiers: latch them with hb_modifiers
		args.pop("ctrl", None)
		if arguments.get("shift") or arguments.get("ctrl"):
			return tool_error("{} takes no shift / ctrl: hold them with hb_modifiers first".format(name))
		result, error = require_bridge(target, command, args)
		if error:
			return tool_error(error)
		verb = "pressed" if command == "mouse_down" else "released"
		return {"content": text_content("{}{} {} at ({}, {}) [{}]{}".format(
			label(target), verb, button, args["x"], args["y"], arguments.get("space", "logical"),
			format_consumed(result)))}

	if name == "hb_key":
		args = {
			"name": canonical_key_name(arguments.get("name", "")),
			"shift": bool(arguments.get("shift", False)),
			"ctrl": bool(arguments.get("ctrl", False)),
			"repeat": bool(arguments.get("repeat", False)),
		}
		_, error = require_bridge(target, "key", args)
		if error:
			return tool_error(error)
		return {"content": text_content("{}pressed key '{}'".format(label(target), args["name"]))}

	if name == "hb_text":
		_, error = require_bridge(target, "text", {"value": str(arguments.get("value", ""))})
		if error:
			return tool_error(error)
		return {"content": text_content("{}typed {} chars".format(
			label(target), len(str(arguments.get("value", "")))))}

	if name == "hb_drag_slot":
		return run_drag_slot(arguments, target)

	if name == "hb_send_chat":
		return run_send_chat(arguments, target)

	if name == "hb_set_text":
		_, error = require_bridge(target, "set_text", {"value": str(arguments.get("value", ""))})
		if error:
			return tool_error(error)
		# the value is never echoed: the focused field may be a password
		return {"content": text_content("{}set field ({} chars)".format(
			label(target), len(str(arguments.get("value", "")))))}

	if name == "hb_wheel":
		args = with_space(arguments, {
			"x": int(arguments.get("x", 0)),
			"y": int(arguments.get("y", 0)),
			"notches": int(arguments.get("notches", 0)),
		})
		_, error = require_bridge(target, "wheel", args)
		if error:
			return tool_error(error)
		return {"content": text_content("{}wheel {} notch(es) at ({}, {})".format(
			label(target), args["notches"], args["x"], args["y"]))}

	return tool_error("unknown tool: {}".format(name))


# --- JSON-RPC / MCP plumbing -------------------------------------------------------------------


def make_result(request_id, result):
	return {"jsonrpc": "2.0", "id": request_id, "result": result}


def make_error(request_id, code, message):
	return {"jsonrpc": "2.0", "id": request_id, "error": {"code": code, "message": message}}


def handle_request(message):
	"""Return a response dict, or None for notifications (no id / nothing to answer)."""
	method = message.get("method")
	request_id = message.get("id")
	params = message.get("params") or {}

	if method == "initialize":
		protocol_version = params.get("protocolVersion", DEFAULT_PROTOCOL_VERSION)
		return make_result(request_id, {
			"protocolVersion": protocol_version,
			# logging: journal events are republished as notifications/message.
			# resources with subscribe: the journal is readable and subscribable as a resource.
			"capabilities": {
				# listChanged: the tool set follows the connected client's ping commands[]
				"tools": {"listChanged": True},
				"logging": {},
				# listChanged: registering or forgetting a client adds/removes its journal resource.
				"resources": {"subscribe": True, "listChanged": True},
			},
			"serverInfo": SERVER_INFO,
		})

	if method == "notifications/initialized":
		return None  # notification: no response

	if method == "ping":
		return make_result(request_id, {})

	if method == "logging/setLevel":
		# A host setting a level is asking to receive events; treat anything but "emergency" as
		# opting in. This is the hook that starts the pump for hosts that never call hb_stream.
		# It is session-wide, so it applies to every registered client rather than guessing one.
		level = str(params.get("level", "info"))
		for client in REGISTRY.all():
			client.pump.set_streaming(level != "emergency")
		return make_result(request_id, {})

	if method == "resources/list":
		return make_result(request_id, {"resources": resource_list()})

	if method == "resources/read":
		uri = params.get("uri")
		try:
			client = client_for_resource(uri)
		except unknown_client as error:
			return make_error(request_id, -32602, str(error))
		if client is None:
			return make_error(request_id, -32602, "unknown resource: {}".format(uri))
		return make_result(request_id, {"contents": [{
			"uri": uri,
			"mimeType": "application/json",
			"text": json.dumps(client.pump.snapshot(), indent=2),
		}]})

	if method == "resources/subscribe":
		try:
			client = client_for_resource(params.get("uri"))
		except unknown_client as error:
			return make_error(request_id, -32602, str(error))
		if client is None:
			return make_error(request_id, -32602, "unknown resource: {}".format(params.get("uri")))
		client.pump.set_subscribed(True)
		return make_result(request_id, {})

	if method == "resources/unsubscribe":
		try:
			client = client_for_resource(params.get("uri"))
		except unknown_client:
			client = None
		if client is not None:
			client.pump.set_subscribed(False)
		return make_result(request_id, {})

	if method == "tools/list":
		return make_result(request_id, {"tools": visible_tools()})

	if method == "tools/call":
		name = params.get("name")
		if name not in TOOL_NAMES:
			return make_error(request_id, -32602, "unknown tool: {}".format(name))
		try:
			return make_result(request_id, call_tool(name, params.get("arguments") or {}))
		except Exception as error:  # a tool failure must not kill the server
			log("tool '{}' raised: {}".format(name, error))
			return make_result(request_id, tool_error("internal error: {}".format(error)))

	if request_id is None:
		return None  # unknown notification: ignore
	return make_error(request_id, -32601, "method not found: {}".format(method))


def main():
	log("starting (default client '{}' at {}:{}; ports {}-{} are scannable with hb_clients discover)"
	    .format(REGISTRY.default_handle(), BRIDGE_HOST, BRIDGE_DEFAULT_PORT,
	            BRIDGE_PORT_FIRST, BRIDGE_PORT_LAST))
	stdin = sys.stdin.buffer
	stdout = sys.stdout.buffer
	while True:
		raw = stdin.readline()
		if not raw:
			break  # host closed stdin -> exit
		line = raw.decode("utf-8").strip()
		if not line:
			continue
		try:
			message = json.loads(line)
		except ValueError:
			log("dropping non-JSON line")
			continue
		response = handle_request(message)
		if response is not None:
			stdout.write((json.dumps(response) + "\n").encode("utf-8"))
			stdout.flush()
	log("stdin closed, exiting")


if __name__ == "__main__":
	main()
