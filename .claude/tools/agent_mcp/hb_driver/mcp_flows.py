"""The hb_flow MCP tool: run one hb_driver flow on a client the MCP server already holds.

The MCP server owns the client's only bridge connection, so the flow borrows its transport rather
than opening a second socket (which the client would leave unanswered in its accept backlog).

Not exposed here, on purpose: launching or stopping a client (the MCP server does not own a PID),
and account or character creation and deletion, the mute / logout / quit flows and the entry timing
(not ported to this client yet).

Credentials: `login` takes `test_account` (the name of an entry in the gitignored .claude/local/test_accounts.json,
or true for the first) or `identity_file` (a scratch identity JSON), or `env: true` for HB_TEST_ACCOUNT /
HB_TEST_PASSWORD. A password is never an argument and never appears in a reply.

Strict on purpose: a key the chosen flow does not take is an ERROR naming the keys it does take,
never silently dropped. Flow names accept hyphens (`click-tile`) as well as underscores.
"""

from . import bridge
from . import common
from . import flows
from . import identity
from . import world

flow_names = ("main_menu", "login", "select", "enter", "player_tile", "step", "click_tile", "wait_screen")
# Flows that are tools here (not flows), named so a caller who tries one is told where it went.
moved_to_tools = {"walk_to": "hb_walk_to {x, y, wait?} (the tool replaces the walk_to flow)",
                  "mute": "hb_set_audio {master?, music?, effects?}", "unmute": "hb_set_audio {master?, music?, effects?}"}
# Named so a caller is told they exist but are not ported, not that they are unknown.
not_ported = ("create_account", "create_character", "delete_character", "switch", "logout", "time_entry")

# Keys every flow accepts: the flow itself, the client selector the MCP layer resolves, the per-step timeout.
common_keys = ("flow", "client", "timeout_s")
credential_keys = ("identity_file", "test_account", "env", "force")
# The keys each flow takes besides those; a key outside the set is refused.
flow_keys = {
	"main_menu": (),
	"login": credential_keys,
	"select": ("name", "slot") + credential_keys[:3],
	"enter": ("name", "slot") + credential_keys[:3],
	"player_tile": (),
	"step": ("direction", "walk_fallback"),
	"click_tile": ("x", "y"),
	"wait_screen": ("screen",),
}
# The keys a flow cannot run without (a clear error here beats a KeyError deep in the flow).
flow_required = {
	"step": ("direction",),
	"click_tile": ("x", "y"),
	"wait_screen": ("screen",),
}
# Where a model tends to put an argument that belongs elsewhere.
key_hints = {
	"args": "arguments go top-level (e.g. {flow:'click_tile', x:1, y:2}), not inside an 'args' object",
	"identity": "the key is identity_file (a path under .claude/scratch/) or test_account",
	"password": "a password is never an argument: use test_account, env:true or identity_file",
	"dir": "the key is direction (n ne e se s sw w nw)",
}


def normalize_flow(name):
	"""`click-tile`, `Click_Tile` and `click_tile` are one flow."""
	return str(name or "").strip().lower().replace("-", "_")


def check_arguments(arguments):
	"""Validate an hb_flow call's arguments without running anything. Returns the canonical flow name,
	raises common.flow_error with a message naming the valid keys. Cheap: no bridge traffic."""
	flow = normalize_flow(arguments.get("flow"))
	if flow in moved_to_tools:
		raise common.flow_error("'{}' is not a flow: use {}".format(flow, moved_to_tools[flow]))
	if flow in not_ported:
		raise common.flow_error("flow '{}' is not ported to this client yet".format(flow))
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


def _ident(arguments, required=True):
	"""The credentials a flow was pointed at, or None (not required) when it was given none."""
	if arguments.get("test_account"):
		wanted = arguments["test_account"]
		return identity.test_account(None if wanted is True else str(wanted))
	if arguments.get("env"):
		return identity.from_env()
	if arguments.get("identity_file"):
		return identity.load_identity(str(arguments["identity_file"]))
	if required:
		raise common.flow_error("this flow needs test_account (a name from test_accounts.json, or true), env:true or "
		                        "identity_file (a path under .claude/scratch/)")
	return None


def _name(arguments):
	name = arguments.get("name")
	return str(name) if name else None


def run(arguments, transport):
	"""Dispatch one flow; returns a JSON-able dict. Raises common.flow_error on failure."""
	flow = check_arguments(arguments)
	return run_flow(flow, arguments, bridge.session(transport=transport))


def run_flow(flow, arguments, sess):
	timeout_s = float(arguments.get("timeout_s") or flows.default_timeout_s)
	if flow == "main_menu":
		return flows.go_to_main_menu(sess, timeout_s)
	if flow == "login":
		return flows.log_in(sess, _ident(arguments), timeout_s=timeout_s, force=bool(arguments.get("force")))
	if flow == "select":
		return flows.select_character(sess, _name(arguments), timeout_s, ident=_ident(arguments, required=False),
		                              slot=arguments.get("slot"))
	if flow == "enter":
		return flows.enter_world(sess, _name(arguments), ident=_ident(arguments, required=False), slot=arguments.get("slot"))
	if flow == "player_tile":
		return {"player_tile": list(world.player_tile(sess))}
	if flow == "step":
		return world.step(sess, str(arguments.get("direction") or ""), walk_fallback=_fallback(arguments))
	if flow == "click_tile":
		return world.click_tile(sess, (_int(arguments, "x"), _int(arguments, "y")))
	return sess.wait_screen(str(arguments.get("screen") or ""), timeout_s)


def _fallback(arguments):
	"""step's walk_fallback: absent or false = never walk, true = walk when no click reaches the tile."""
	value = arguments.get("walk_fallback")
	return None if value is None else bool(value)


def _int(arguments, key):
	try:
		return int(arguments[key])
	except (TypeError, ValueError):
		raise common.flow_error("'{}' must be an integer tile coordinate (got {!r})".format(key, arguments.get(key)))
