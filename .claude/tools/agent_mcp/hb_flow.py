"""hb_flow - one-off CLI over the hb_driver flows (the library is the main surface).

	python .claude/tools/agent_mcp/hb_flow.py <flow> [--port N] [options]
	python .claude/tools/agent_mcp/hb_flow.py --help            # every flow
	python .claude/tools/agent_mcp/hb_flow.py <flow> --help     # its options

Each call is its own process, so state that must outlive it lives in files under .claude/scratch/: the
identity (account + password), the pid file (what this tooling launched), the client-file backup
(client.json byte copy) and the logout proof (for a position write). Output is JSON on stdout, ASCII
only; a failure prints `ERROR: ...` on stderr and exits 1. A password is never printed.

Typical run (scratch = .claude/scratch/<role>/<task>):
	hb_flow.py identity-new   --file <scratch>/ident.json
	hb_flow.py launch         --port 8892 --pid-file <scratch>/pids.txt --backup-dir <scratch>/client_files
	hb_flow.py create-account --port 8892 --identity <scratch>/ident.json
	hb_flow.py login          --port 8892 --identity <scratch>/ident.json
	hb_flow.py create-character --port 8892 --identity <scratch>/ident.json --preset Warrior
	hb_flow.py enter          --port 8892 --name <character>
	hb_flow.py step           --port 8892 --dir e
	hb_flow.py switch         --port 8892 --proof-file <scratch>/proof.json --name <character>
	hb_flow.py stop           --pid-file <scratch>/pids.txt --backup-dir <scratch>/client_files
"""

import argparse
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from hb_driver import bridge  # noqa: E402
from hb_driver import client_files  # noqa: E402
from hb_driver import client_process  # noqa: E402
from hb_driver import common  # noqa: E402
from hb_driver import flows  # noqa: E402
from hb_driver import identity  # noqa: E402
from hb_driver import player_db  # noqa: E402
from hb_driver import server_logs  # noqa: E402
from hb_driver import world  # noqa: E402

# The shared transport logs connects to stderr for the MCP server; a one-off CLI run does not want it.
bridge.agent_mcp.log = lambda message: None


def repo_path(value):
	"""A path argument as an absolute path: a relative one is resolved against the repo root (not the
	cwd), so `.claude/scratch/x/backup` works from anywhere."""
	return value if os.path.isabs(value) else os.path.normpath(os.path.join(common.repo_root, value))


def _session(args):
	return bridge.session.connect(args.port, args.host, timeout_s=args.connect_timeout)


def _proof_around(args, action):
	"""Run `action(sess)` with a world-log watch marked first; when --proof-file is given, wait for the
	character's logout save and write the proof there."""
	def run():
		with _session(args) as sess:
			return action(sess)
	return player_db.logout_proof_around(run, args.proof_file, args.name, args.save_timeout, args.db_config)


# --- one function per flow -------------------------------------------------------------------------

def run_launch(args):
	# both checked BEFORE anything launches: a bad --pid-file used to fail only after the client was up
	common.require_scratch_path(args.backup_dir, "--backup-dir")
	common.require_scratch_path(args.pid_file, "--pid-file")
	guard = client_files.client_files_guard(client_files.default_client_files(), args.backup_dir)
	hashes = guard.save()
	process = client_process.client_process(args.port, exe=args.exe, start_screen=args.start_screen,
	                                        extra_args=args.extra or [], pid_file=args.pid_file)
	client_json = os.path.join(process.work_dir, common.client_json_name)
	process.launch(detach=True)
	try:
		sess = process.wait_bridge()
		minimized = process.window_minimized()
		process.restore_window()
		mute = None  # the launch mute is not ported: use hb_set_audio
		ping = sess.ping()
		sess.close()
	except Exception:
		process.stop()
		guard.restore()
		raise
	return {"pid": process.pid, "port": process.port, "exe": process.exe, "command": process.command_line(),
	        "build": ping.get("build"), "window_minimized_at_launch": minimized, "mute": mute,
	        "client_files_saved": hashes, "backup_dir": args.backup_dir, "pid_file": args.pid_file}


def run_stop(args):
	report = {"stopped": client_process.stop_recorded(args.pid_file, args.pid)}
	if args.backup_dir:
		report["client_files"] = client_files.restore_from_backup(args.backup_dir)
	return report


def run_restore_client_files(args):
	return {"client_files": client_files.restore_from_backup(args.backup_dir)}


def run_identity_new(args):
	made = identity.new_identity(args.file, args.account_prefix, args.character_prefix, args.characters, args.email)
	return made.public()


def run_identity_show(args):
	return identity.load_identity(args.file).public()


def run_identity_delete(args):
	return {"deleted": identity.delete_identity(args.file), "file": args.file}


def run_state(args):
	with _session(args) as sess:
		return sess.state()


def run_widgets(args):
	with _session(args) as sess:
		return sess.widgets()


def run_wait_screen(args):
	with _session(args) as sess:
		return sess.wait_screen(args.screen, args.timeout)


def run_wait_for(args):
	with _session(args) as sess:
		return sess.wait_for(json.loads(args.conditions), args.timeout_ms).raw


def run_journal(args):
	with _session(args) as sess:
		return {"events": sess.drain_journal(args.after_seq, args.channels)}


def run_main_menu(args):
	with _session(args) as sess:
		return flows.go_to_main_menu(sess)


def _mute(args, muted):
	with _session(args) as sess:
		return flows.set_master_mute(sess, muted, client_json=args.client_json, leave_session=args.leave_session)


def run_mute(args):
	return _mute(args, True)


def run_unmute(args):
	return _mute(args, False)


def run_create_account(args):
	with _session(args) as sess:
		return flows.create_account(sess, identity.load_identity(args.identity))


def _credentials(args, required=True):
	"""The account to log in with: --identity (a scratch file), --test-account [NAME] (the gitignored local test
	accounts) or --env (HB_TEST_ACCOUNT / HB_TEST_PASSWORD). The password is never printed."""
	if getattr(args, "identity", None):
		return identity.load_identity(args.identity)
	if getattr(args, "test_account", None) is not None:
		return identity.test_account(None if args.test_account == "first" else args.test_account)
	if getattr(args, "env", False):
		return identity.from_env()
	if required:
		raise common.flow_error("give --identity FILE, --test-account [NAME] or --env")
	return None


def run_login(args):
	with _session(args) as sess:
		return flows.log_in(sess, _credentials(args), force=args.force)


def run_create_character(args):
	appearance = json.loads(args.appearance) if args.appearance else None
	with _session(args) as sess:
		return flows.create_character(sess, identity.load_identity(args.identity), args.name, args.preset, appearance)


def run_select(args):
	with _session(args) as sess:
		return flows.select_character(sess, args.name, ident=_credentials(args, False), slot=args.slot)


def run_enter(args):
	with _session(args) as sess:
		return flows.enter_world(sess, args.name, ident=_credentials(args, False), slot=args.slot)


def run_switch(args):
	return _proof_around(args, flows.switch_character)


def run_logout(args):
	return _proof_around(args, lambda sess: flows.log_out(sess, to=args.to))


def run_quit(args):
	return _proof_around(args, flows.quit_client)


def run_delete_character(args):
	with _session(args) as sess:
		return flows.delete_character(sess, identity.load_identity(args.identity), args.name)


def run_position_read(args):
	name = args.name
	if not name and args.identity:
		listed = [entry["name"] for entry in identity.load_identity(args.identity).characters]
		if len(listed) != 1:
			raise common.flow_error("--identity lists {} characters ({}); pass --name".format(len(listed), ", ".join(listed) or "none"))
		name = listed[0]
	if not name:
		raise common.flow_error("position-read needs --name (or --identity with one character)")
	return player_db.read_position(player_db.db_connection(args.db_config), name)


def run_position_write(args):
	proof = common.read_json_file(args.proof_file)
	return player_db.write_position(player_db.db_connection(args.db_config), identity.load_identity(args.identity),
	                                args.name, args.map, args.x, args.y, args.direction, logout_proof=proof,
	                                snapshot_label=args.snapshot_label, snapshot_set=args.snapshot_set)


def run_player_tile(args):
	with _session(args) as sess:
		return {"player_tile": list(world.player_tile(sess))}


def run_step(args):
	with _session(args) as sess:
		fallback = False if args.no_walk_fallback else (True if args.walk_fallback else None)
		return world.step(sess, args.dir, arrive=not args.no_arrive, walk_fallback=fallback)


def run_click_tile(args):
	with _session(args) as sess:
		return world.click_tile(sess, (args.x, args.y), arrive=not args.no_arrive)


def run_walk_to(args):
	with _session(args) as sess:
		return world.walk_to(sess, (args.x, args.y), arrive=not args.no_arrive)


def run_path_bench(args):
	if args.fixture:
		common.require_scratch_path(args.fixture, "--fixture")
	with _session(args) as sess:
		return sess.path_bench(fixture=args.fixture, repeats=args.repeats)


def run_press_bench(args):
	with _session(args) as sess:
		if args.live_path:
			return sess.press_bench(live_path=args.live_path)
		ids = [args.display_id] if args.display_id is not None else [
			row["display_id"] for row in sess.press_bench().get("creatures") or []]
		results = [sess.press_bench(display_id=display_id, repeats=args.repeats) for display_id in ids]
		return {"results": results, "table": bridge.agent_mcp.format_press_bench(results).split("\n")}


def run_time_entry(args):
	click = None
	if args.click_x is not None and args.click_y is not None:
		click = {"x": args.click_x, "y": args.click_y}
		if args.click_space:
			click["space"] = args.click_space
	with _session(args) as sess:
		return world.time_entry(sess, args.name, click, capture_logs=not args.no_logs)


# --- argument parsing ------------------------------------------------------------------------------

def build_parser():
	parser = argparse.ArgumentParser(prog="hb_flow.py", description="Drive a debug client over the agent bridge.")
	flows_parser = parser.add_subparsers(dest="flow", required=True, metavar="<flow>")

	def add(name, handler, help_text, bridge_port=True):
		sub = flows_parser.add_parser(name, help=help_text, description=help_text)
		sub.set_defaults(handler=handler)
		if bridge_port:
			sub.add_argument("--port", type=int, required=True, help="the client's --agent-bridge-port")
			sub.add_argument("--host", default=bridge.default_host)
			sub.add_argument("--connect-timeout", type=float, default=10.0)
		return sub

	def add_proof(sub):
		sub.add_argument("--proof-file", type=repo_path, help="write the logout-save proof here (under .claude/scratch/); needs --name")
		sub.add_argument("--name", help="the character that is logging out (for --proof-file)")
		sub.add_argument("--save-timeout", type=float, default=server_logs.logout_save_wait_s)
		sub.add_argument("--db-config", default=None, help="json with the database block (default worldserver.json)")
		return sub

	sub = add("launch", run_launch, "launch a debug client (existing exe path), wait for its bridge, mute it")
	sub.add_argument("--pid-file", required=True, type=repo_path,
	                 help="append 'pid port' here (under .claude/scratch/; relative = from the repo root)")
	sub.add_argument("--backup-dir", required=True, type=repo_path,
	                 help="byte copies of client.json etc. (under .claude/scratch/; relative = from the repo root)")
	sub.add_argument("--exe", help="client exe (default: the deployed game/client exe)")
	sub.add_argument("--start-screen", default=client_process.default_start_screen)
	sub.add_argument("--mute", choices=("none",), default="none", help="launch muting is not ported: use hb_set_audio")
	sub.add_argument("--extra", nargs=argparse.REMAINDER, help="extra client arguments (last)")

	sub = add("stop", run_stop, "stop PIDs this tooling launched (from the pid file) and restore client files",
	          bridge_port=False)
	sub.add_argument("--pid-file", required=True, type=repo_path)
	sub.add_argument("--pid", type=int, help="only this one (default: every pid in the file)")
	sub.add_argument("--backup-dir", type=repo_path, help="restore client.json etc. from this launch backup and verify")

	sub = add("restore-client-files", run_restore_client_files, "restore client.json etc. from a backup dir",
	          bridge_port=False)
	sub.add_argument("--backup-dir", required=True, type=repo_path)

	sub = add("identity-new", run_identity_new, "generate a throwaway account/password/characters file",
	          bridge_port=False)
	sub.add_argument("--file", required=True, type=repo_path)
	sub.add_argument("--account-prefix", default=identity.default_account_prefix)
	sub.add_argument("--character-prefix", default=identity.default_character_prefix)
	sub.add_argument("--characters", type=int, default=1)
	sub.add_argument("--email", default="")
	for name, handler, text in (("identity-show", run_identity_show, "print an identity (password masked)"),
	                            ("identity-delete", run_identity_delete, "delete an identity file")):
		add(name, handler, text, bridge_port=False).add_argument("--file", required=True, type=repo_path)

	add("state", run_state, "get_state")
	sub = add("path-bench", run_path_bench, "time the click-to-move pathfinder candidates on the live map (HB-0398)")
	sub.add_argument("--fixture", type=repo_path, help="also save the timed map here (under .claude/scratch/), for path_bench.exe")
	sub.add_argument("--repeats", type=int, help="timed runs per row (client default 3, max 20)")
	sub = add("press-bench", run_press_bench, "time the grass press fields of every creature's death pose (HB-0748)")
	sub.add_argument("--display-id", type=int, help="only this creature (entity registry id)")
	sub.add_argument("--repeats", type=int, help="timed runs per shape (client default 3, max 20)")
	sub.add_argument("--live-path", choices=bridge.agent_mcp.PRESS_LIVE_PATHS, help="switch the world's press path instead")
	add("widgets", run_widgets, "the widgets on screen")
	sub = add("wait-screen", run_wait_screen, "wait for a screen id (settled)")
	sub.add_argument("--screen", required=True)
	sub.add_argument("--timeout", type=float, default=flows.default_timeout_s)
	sub = add("wait-for", run_wait_for, "hb_wait_for: ANY of the JSON conditions")
	sub.add_argument("--conditions", required=True, help='JSON list, e.g. [{"state":"screen","equals":"Screen_Login"}]')
	sub.add_argument("--timeout-ms", type=int, default=10000)
	sub = add("journal", run_journal, "every retained journal event after a seq, oldest first (non-consuming)")
	sub.add_argument("--after-seq", type=int, default=0)
	sub.add_argument("--channels", nargs="*")
	add("main-menu", run_main_menu, "walk back to the main menu from anywhere")
	for name, handler, text in (("mute", run_mute, "master muted: the Settings tick on the main menu, set_audio when logged in"),
	                            ("unmute", run_unmute, "master playing (same routes as mute)")):
		sub = add(name, handler, text)
		sub.add_argument("--client-json", default=os.path.join(common.client_dir, common.client_json_name))
		sub.add_argument("--leave-session", action="store_true", help="use the Settings screen even when logged in: log out first (default: set_audio, no logout)")

	def add_credentials(sub, required):
		sub.add_argument("--identity", type=repo_path, help="a scratch identity file (under .claude/scratch/)")
		sub.add_argument("--test-account", nargs="?", const="first", default=None, metavar="NAME",
		                 help="a local test account from .claude/local/test_accounts.json (default: the first)")
		sub.add_argument("--env", action="store_true", help="HB_TEST_ACCOUNT / HB_TEST_PASSWORD from the environment")
		return sub

	add("create-account", run_create_account, "New Account page (not ported)").add_argument("--identity", required=True, type=repo_path)
	sub = add_credentials(add("login", run_login, "log in (main menu or login screen) -> character select"), True)
	sub.add_argument("--force", action="store_true", help="take over an account that is logged in elsewhere")
	sub = add("create-character", run_create_character, "create a character on the logged-in account")
	sub.add_argument("--identity", required=True, type=repo_path)
	sub.add_argument("--name")
	sub.add_argument("--preset", help="archetype label (e.g. Warrior) or index")
	sub.add_argument("--appearance", help='JSON, e.g. {"gender": 1, "hair_style": 2}')
	for name, handler, text in (("select", run_select, "select a character slot (--slot, or --name with credentials)"),
	                            ("enter", run_enter, "enter the world (selecting --slot / --name first)")):
		sub = add_credentials(add(name, handler, text), False)
		sub.add_argument("--name")
		sub.add_argument("--slot", type=int, help="character slot 1..4 (default 1)")
	for name, handler, text in (("switch", run_switch, "in-world -> character select"),
	                            ("logout", run_logout, "-> main menu (--to login: on to the login screen)"),
	                            ("quit", run_quit, "main menu > Exit")):
		sub = add_proof(add(name, handler, text))
		if name == "logout":
			sub.add_argument("--to", choices=flows.log_out_targets, default="main_menu",
			                 help="where to end: main_menu (default) or login")
	sub = add("delete-character", run_delete_character, "delete a character this tooling created")
	sub.add_argument("--identity", required=True, type=repo_path)
	sub.add_argument("--name", required=True)

	sub = add("position-read", run_position_read, "hb_player character_position", bridge_port=False)
	sub.add_argument("--name", help="the character; with --identity it defaults to that identity's only character")
	sub.add_argument("--identity", type=repo_path, help="read the character named in this identity file")
	sub.add_argument("--db-config", default=None)
	sub = add("position-write", run_position_write, "guarded character_position write (test.md)", bridge_port=False)
	sub.add_argument("--identity", required=True, type=repo_path)
	sub.add_argument("--proof-file", required=True, type=repo_path)
	for flag in ("--name", "--map"):
		sub.add_argument(flag, required=True)
	sub.add_argument("--x", type=int, required=True)
	sub.add_argument("--y", type=int, required=True)
	sub.add_argument("--direction", type=int)
	sub.add_argument("--snapshot-label", help="take a db_snapshot.py set of hb_player with this label first")
	sub.add_argument("--snapshot-set", help="or name an existing set")
	sub.add_argument("--db-config", default=None)

	add("player-tile", run_player_tile, "the player's tile and the view calibration")
	sub = add("step", run_step, "one tile: n ne e se s sw w nw")
	sub.add_argument("--dir", required=True, choices=sorted(world.directions))
	sub.add_argument("--no-arrive", action="store_true")
	sub.add_argument("--no-walk-fallback", action="store_true",
	                 help="never walk (walk_to) instead of clicking: fail even for the tile north of you")
	sub.add_argument("--walk-fallback", action="store_true",
	                 help="walk (walk_to) whenever no click can reach the tile; by default only the click-on-self "
	                      "box (the tile north of you) does")
	sub = add("click-tile", run_click_tile, "click-to-move to a visible tile")
	sub.add_argument("--x", type=int, required=True)
	sub.add_argument("--y", type=int, required=True)
	sub.add_argument("--no-arrive", action="store_true")
	sub = add("walk-to", run_walk_to, "walk to a map tile by the client's own pathing (no pixel; any tile)")
	sub.add_argument("--x", type=int, required=True)
	sub.add_argument("--y", type=int, required=True)
	sub.add_argument("--no-arrive", action="store_true")
	sub = add("time-entry", run_time_entry, "time Start -> first confirmed move (from character select)")
	sub.add_argument("--name")
	sub.add_argument("--no-logs", action="store_true")
	sub.add_argument("--click-x", type=int)
	sub.add_argument("--click-y", type=int)
	sub.add_argument("--click-space", choices=("logical", "window"))
	return parser


def main(argv=None):
	args = build_parser().parse_args(argv)
	try:
		result = args.handler(args)
	except (common.flow_error, ValueError, OSError) as error:
		sys.stderr.write("ERROR: {}\n".format(str(error).encode("ascii", "replace").decode("ascii")))
		return 1
	sys.stdout.write(common.ascii_json(result) + "\n")
	return 0


if __name__ == "__main__":
	sys.exit(main())
