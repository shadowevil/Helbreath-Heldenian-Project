"""The hb_flow MCP tool: run one hb_driver flow on a client the MCP server already holds.

The MCP server owns the client's only bridge connection, so the flow borrows its transport rather
than opening a second socket (which the client would leave unanswered in its accept backlog).

Not exposed here, on purpose: launching or stopping a client (the MCP server does not own a PID),
the position write (it needs a snapshot and a logout proof - use the library or the CLI) and
identity creation (the password file is the caller's; pass its path).

Strict on purpose: a key the chosen flow does not take is an ERROR naming the keys it does take,
never silently dropped (an `args:{...}` wrapper used to be ignored and surface as a misleading
"needs identity_file"). Flow names accept hyphens (`click-tile`) as well as underscores.
"""

import os

from . import bridge
from . import common
from . import flows
from . import identity
from . import player_db
from . import server_logs
from . import world

flow_names = ("main_menu", "mute", "unmute", "create_account", "login", "create_character", "select", "enter",
              "switch", "logout", "delete_character", "player_tile", "step", "click_tile",
              "time_entry", "wait_screen")
# Flows that are tools here (not flows), named so a caller who tries one is told where it went.
moved_to_tools = {"walk_to": "hb_walk_to {x, y, wait?} (the tool replaces the walk_to flow)"}

# Keys every flow accepts: the flow itself, the client selector the MCP layer resolves, the per-step timeout.
common_keys = ("flow", "client", "timeout_s")
# The keys each flow takes besides those; a key outside the set is refused.
flow_keys = {
	"main_menu": (),
	"mute": ("client_json", "leave_session"),
	"unmute": ("client_json", "leave_session"),
	"create_account": ("identity_file",),
	"login": ("identity_file",),
	"create_character": ("identity_file", "name", "preset", "appearance"),
	"select": ("name",),
	"enter": ("name",),
	"switch": ("proof_file", "name", "save_timeout"),
	"logout": ("proof_file", "name", "save_timeout", "to"),
	"delete_character": ("identity_file", "name"),
	"player_tile": ("recalibrate",),
	"step": ("direction", "walk_fallback", "recalibrate"),
	"click_tile": ("x", "y", "recalibrate"),
	"time_entry": ("name", "capture_logs"),
	"wait_screen": ("screen",),
}
# The keys a flow cannot run without (a clear error here beats a KeyError deep in the flow).
flow_required = {
	"create_account": ("identity_file",),
	"login": ("identity_file",),
	"create_character": ("identity_file",),
	"select": ("name",),
	"delete_character": ("identity_file", "name"),
	"step": ("direction",),
	"click_tile": ("x", "y"),
	"wait_screen": ("screen",),
}
# Where a model tends to put an argument that belongs elsewhere.
key_hints = {
	"args": "arguments go top-level (e.g. {flow:'click_tile', x:1, y:2}), not inside an 'args' object",
	"identity": "the key is identity_file (a path under .claude/scratch/; relative = from the repo root)",
	"dir": "the key is direction (n ne e se s sw w nw)",
	"proof": "the key is proof_file (a path under .claude/scratch/); only the logout and switch flows write one",
}
# The cache key on the borrowed transport: one measured view per client connection.
calibration_attribute = "hb_view_calibration"


def normalize_flow(name):
	"""`click-tile`, `Click_Tile` and `click_tile` are one flow."""
	return str(name or "").strip().lower().replace("-", "_")


def check_arguments(arguments):
	"""Validate an hb_flow call's arguments without running anything. Returns the canonical flow name,
	raises common.flow_error with a message naming the valid keys. Cheap: no bridge traffic."""
	flow = normalize_flow(arguments.get("flow"))
	if flow in moved_to_tools:
		raise common.flow_error("'{}' is not a flow: use {}".format(flow, moved_to_tools[flow]))
	if flow not in flow_names:
		raise common.flow_error("unknown flow '{}'. Flows: {}".format(arguments.get("flow"), ", ".join(flow_names)))
	allowed = set(common_keys) | set(flow_keys[flow])
	unknown = sorted(key for key in arguments if key not in allowed)
	if unknown:
		lines = ["flow '{}' does not take: {}.".format(flow, ", ".join(unknown))]
		for key in unknown:
			if key in key_hints:
				lines.append("  {}: {}".format(key, key_hints[key]))
		lines.append("It takes: {}.".format(", ".join(sorted(allowed - {"flow", "client"})) or "(nothing)"))
		raise common.flow_error("\n".join(lines))
	missing = [key for key in flow_required.get(flow, ()) if arguments.get(key) in (None, "")]
	if missing:
		raise common.flow_error("flow '{}' needs {}".format(flow, ", ".join(missing)))
	return flow


def _ident(arguments):
	path = arguments.get("identity_file")
	if not path:
		raise common.flow_error("this flow needs identity_file (a path under .claude/scratch/; relative = from the repo root)")
	return identity.load_identity(str(path))


def _name(arguments, required=True):
	name = arguments.get("name")
	if required and not name:
		raise common.flow_error("this flow needs name")
	return str(name) if name else None


def _session(transport, arguments):
	"""A session on the borrowed transport, with the view calibration carried between calls: measuring
	it costs ~40 probes, which would otherwise be paid by every step in a batch. `recalibrate` drops it
	(after a camera zoom); an aim that drifts is corrected by the probe loop in world.aim_tile anyway."""
	sess = bridge.session(transport=transport)
	if not arguments.get("recalibrate"):
		sess.view_calibration = getattr(transport, calibration_attribute, None)
	return sess


def _keep_calibration(transport, sess):
	if sess.view_calibration is not None:
		try:
			setattr(transport, calibration_attribute, sess.view_calibration)
		except AttributeError:
			pass


def run(arguments, transport):
	"""Dispatch one flow; returns a JSON-able dict. Raises common.flow_error on failure."""
	flow = check_arguments(arguments)
	sess = _session(transport, arguments)
	try:
		return run_flow(flow, arguments, sess)
	finally:
		_keep_calibration(transport, sess)


def run_flow(flow, arguments, sess):
	timeout_s = float(arguments.get("timeout_s") or flows.default_timeout_s)
	if flow == "main_menu":
		return flows.go_to_main_menu(sess, timeout_s)
	if flow in ("mute", "unmute"):
		return flows.set_master_mute(sess, flow == "mute", client_json=arguments.get("client_json"),
		                             leave_session=bool(arguments.get("leave_session")), timeout_s=timeout_s)
	if flow == "create_account":
		return flows.create_account(sess, _ident(arguments), timeout_s=timeout_s)
	if flow == "login":
		return flows.log_in(sess, _ident(arguments), timeout_s=timeout_s)
	if flow == "create_character":
		return flows.create_character(sess, _ident(arguments), _name(arguments, False), arguments.get("preset"),
		                              arguments.get("appearance"), timeout_s=timeout_s)
	if flow == "select":
		return flows.select_character(sess, _name(arguments), timeout_s)
	if flow == "enter":
		return flows.enter_world(sess, _name(arguments, False))
	if flow in ("switch", "logout"):
		return _with_proof(sess, arguments, lambda: flows.switch_character(sess, timeout_s) if flow == "switch"
		                   else flows.log_out(sess, timeout_s, str(arguments.get("to") or "main_menu")))
	if flow == "delete_character":
		return flows.delete_character(sess, _ident(arguments), _name(arguments), timeout_s=timeout_s)
	if flow == "player_tile":
		return {"player_tile": world.player_tile(sess), "view": world.calibrate(sess).as_dict()}
	if flow == "step":
		return world.step(sess, str(arguments.get("direction") or ""), walk_fallback=_fallback(arguments))
	if flow == "click_tile":
		return world.click_tile(sess, (_int(arguments, "x"), _int(arguments, "y")))
	if flow == "time_entry":
		return world.time_entry(sess, _name(arguments, False), capture_logs=arguments.get("capture_logs", True))
	return sess.wait_screen(str(arguments.get("screen") or ""), timeout_s)


def _with_proof(sess, arguments, action):
	"""A logout or switch; with proof_file, also wait for the character's logout save and write the proof there
	(the character defaults to the one being played)."""
	proof_file = arguments.get("proof_file")
	if proof_file and not os.path.isabs(str(proof_file)):
		proof_file = os.path.normpath(os.path.join(common.repo_root, str(proof_file)))  # relative = from the repo root
	name = arguments.get("name")
	if proof_file and not name:
		name = sess.state().get("character")
	timeout = float(arguments.get("save_timeout") or server_logs.logout_save_wait_s)
	return player_db.logout_proof_around(action, proof_file, name, timeout)


def _fallback(arguments):
	"""step's walk_fallback: absent = automatic (only the click-on-self box), true = force, false = never."""
	value = arguments.get("walk_fallback")
	return None if value is None else bool(value)


def _int(arguments, key):
	try:
		return int(arguments[key])
	except (TypeError, ValueError):
		raise common.flow_error("'{}' must be an integer tile coordinate (got {!r})".format(key, arguments.get(key)))
