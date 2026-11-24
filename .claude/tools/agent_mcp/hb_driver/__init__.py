"""hb_driver - reusable, parameterized flows that drive a debug client over the agent bridge.

The bridge is the ONLY client testing surface (CLAUDE.md); this adds nothing to the client. It is
a driver over the same wire the MCP server speaks, sharing its transport (agent_mcp.bridge_client).

	import sys; sys.path.insert(0, r"<repo>/.claude/tools/agent_mcp")
	from hb_driver import *

	ident = identity.new_identity(r"<repo>/.claude/scratch/<role>/<task>/ident.json")
	with running.launch_client(port=8892) as run:           # launched, bridge up, muted
		sess = run.session
		flows.create_account(sess, ident)
		flows.log_in(sess, ident)
		flows.create_character(sess, ident, preset="Warrior")
		flows.enter_world(sess, ident.characters[0]["name"])
		world.step(sess, "e")
		flows.switch_character(sess)
	# client stopped (only that pid), client.json restored byte-identical

Modules: bridge (session + waits), client_process (launch / stop one pid), client_files (byte
restore), identity (throwaway account file), flows (menus, account, characters, world entry,
mute), world (movement, entry timing), server_logs (log watches, logout-save proof), player_db
(position read / guarded write), running (launch_client). CLI: hb_flow.py beside this package.
"""

from . import bridge
from . import client_files
from . import client_process
from . import common
from . import flows
from . import identity
from . import player_db
from . import running
from . import server_logs
from . import world

__all__ = ["bridge", "client_files", "client_process", "common", "flows", "identity", "player_db", "running",
           "server_logs", "world"]
