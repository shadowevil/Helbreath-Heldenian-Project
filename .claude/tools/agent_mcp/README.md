# agent_mcp - client agent bridge (MCP server)

Lets an agent drive and inspect a **running debug client** natively (screenshots, synthetic
input, state queries) instead of relaunching it per check.

> **Heldenian port (HH-0039, plan `.claude/design/plans/client-agent-mcp.md`).** Ported from Helbreath-SDL's
> `agent_mcp`. Differences that override what follows:
>
> - Ports are **8891-8898** (default 8891, `HB_AGENT_PORT` moves it; `hb_clients discover` scans exactly that range).
> - Launch with `python .claude/tools/agent_mcp/agent_mcp.py` (repo-root `.mcp.json`, server `helbreath-client`).
> - Client flag is `--agent-bridge-port=N`. The client has no `--asset-root` or `--start-screen`; it runs from `game/client`.
> - **`ping` returns `commands[]`; `tools/list` shows only the tools whose wire commands the client lists**, and a hidden
> tool is refused if called. Phase 1 clients therefore show input, state and screenshot tools only; widgets, journal,
> world, flows and the other SDL-only tools appear as the client gains those commands (`TOOL_COMMANDS` in `agent_mcp.py`).
> - `hb_screenshot` sends a bare file name; the client saves it under `<repo>/build/screenshots/` and the server reads it
> from there. The client ignores `region_*`; Pillow (optional) crops.
> - Screen ids are the screen class names: `Screen_Splash`, `Screen_Login`, `Screen_MainMenu`, `Screen_OnGame`, ...
> - `hb_set_text` and `hb_text` never echo the typed value (it may be a password).
> - `hb_driver` flows (`hb_flow`) still carry SDL widget ids and server log names; they need the widgets command
> (phase 2) and a rewrite (phase 4) before they work. Settings file is `settings.json`, not `client.json`.

## How it fits together

```
agent (MCP host)  <--stdio JSON-RPC-->  agent_mcp.py  <--localhost TCP 8891-->  debug client "c1"
                                        (this dir)         |                    (agent_bridge.*)
                                                           +---- TCP 8892 -->   debug client "c2"  ...
```

- `agent_mcp.py` - a stdlib-only Python MCP server (no pip deps). Translates MCP tool calls into
  one-line JSON commands over a TCP socket.
- The client half is `sources/client/agent_bridge.{h,cpp}`, compiled **only in debug builds**
  (`DEF_CLAUDE_TESTING`, auto-derived from `_DEBUG`). Release builds contain none of it.
- The bridge is **on by default** in a live debug run. Disable with `--no-agent-bridge`; move it
  off port 8891 with `--agent-bridge-port=N` (also honored by the server via `HB_AGENT_PORT`).
- **The client LISTENS and this server dials out**, one connection per client (a client accepts one
  at a time). So several clients at once is several ports - see "Driving several clients" below.

This bridge is the ONLY client testing method (user rule, 2026-07-10). It replaced the earlier
batch/injection harnesses; any new testing capability hangs off the bridge rather than beside it.

## Use it

1. Build + run a **debug** client (from `game/client`), in a **real window** - not
   `SDL_VIDEODRIVER=offscreen`, which silently drops it onto the software renderer while the bridge keeps
   answering (`.claude/docs/agent_bridge_playbook/2-launch-a-client.md`). It logs `agent bridge: listening on port 8891`.
2. The server is registered in the repo-root `.mcp.json` (as `helbreath-client`); approve/enable it in
   the agent host, then **restart the host** so it launches `python .claude/tools/agent_mcp/agent_mcp.py`.
   The tools then appear as `mcp__helbreath-client__hb_*`. (Project `.mcp.json` servers load as tools at
   session start - they do NOT show in the desktop app's "Connectors" pane, which is for remote connectors.)
3. Call the tools:

| tool | what it does |
|---|---|
| `hb_ping` | confirm the client is reachable (returns the build string) |
| `hb_self_tests` | run the client self-tests (content cache, item affixes, player models) once; per suite passed / total and every FAIL line. Call in the world, after a client build |
| `hb_get_state` | active screen id, transition flag, cursor, mouse position; in the world also `character`, `object_id`, `spawned`, `sky_time01`, `sky_time_override`, `map {id,name,light,weather}`, `map_id`, `modal_open`, `windows[]`, and two one-line summaries: `audio:` (master muted/playing, the three volumes) and `memory:` (working set, commit, clip / tile-sheet / minimap caches in MB). The default reply is a COMPACT summary (scalars packed, nested values one short line each, memory left out); `verbose:true` the indented JSON with audio / memory folded, `raw:true` the client's whole JSON |
| `hb_probe` `{x,y,space?,frame_times?}` | diagnostic: resolve a point -> logical coord + (in-world) world tile, attackable creature, player tile/stance, HUD/stance-button hit-test. Compact: one JSON line without the verbose blocks (`bubble`, `teleport_smoke`, `minimap_view`, `walk_behind`, `ground_fields`) or the frame-time report - `full:true` / `frame_times:true` bring them back; a point another surface takes from the world adds a `COVERED` line (`front` / `covered_by`) |
| `hb_widgets` | list every clickable widget on screen - stable id, label, logical bounds, enabled - so a button is found by NAME instead of a remembered pixel. Rows carry what their kind adds: an inventory / character-sheet slot (`inventory.slot0..N`, `character_sheet.slot0..10`) shows `slot N item #id xQty` or `empty`; a Settings slider (`settings.sound_<master\|ambiance\|effect\|music\|ui>_volume`) its `track` / `thumb` and readout, its checkbox `..._enabled`; a dropdown its `options` and `selected` (and, open, the option rows). A row scrolled out of its window's viewport is flagged `[HIDDEN - scrolled out of view]` |
| `hb_screenshot` `{name?,path?,region?,scale?,format?,quality?,inline?,burst?,interval_ms?}` | capture the current frame: returned inline **and** auto-saved to `build/screenshots`. `region {x,y,w,h}` (output pixels), `scale` (0.05-1, Lanczos), `format` png/jpg, `quality`, `path` (exact file, inside the repo) and `inline:false`; `burst` N (2..30) + `interval_ms` takes N captures in a row, every file saved, at most 4 images returned, the 20 MB cap enforced - see "Screenshots" |
| `hb_mouse_move` `{x,y,space?}` | move the cursor |
| `hb_click` `{x,y,space?,button?,right?,shift?,ctrl?}` | move + press/release with `button` left (default) / right / middle (`right:true` = right) (in-world: ground=walk, creature=attack). The reply ends `-> consumed by <kind> <owner>` when the client reports what took the click (world, screen, window, modal, minimap, big_map, action_bar, npc_bar, shop_lock, dead, widget) |
| `hb_click_widget` `{id,button?,right?,shift?,ctrl?}` | click a widget BY NAME (id from `hb_widgets`); resolved and clicked in the same frame, so the coordinate can never be stale. An unknown or disabled id is refused, and so is a `[HIDDEN]` one (`widget is scrolled out of view: <id>`), with a hint to scroll the list (`hb_wheel`) first |
| `hb_mouse_down` `{x,y,space?,button?,right?,shift?,ctrl?}` | move + press and HOLD (half a click - always pair it with a release); reports `consumed by` like `hb_click` |
| `hb_mouse_up` `{x,y,space?,button?,right?,shift?,ctrl?}` | move + release, ending a drag at this point |
| `hb_key` `{name,shift?,ctrl?}` | press a named key (e.g. `tab` toggles combat stance in-world) |
| `hb_modifiers` `{shift?,ctrl?,alt?}` | HOLD a modifier across later commands (Alt crit, Shift run/dash, held Ctrl); a latch ORed over the real keyboard - call bare to read it back |
| `hb_text` `{value}` | type text into the focused field (APPENDS) |
| `hb_set_text` `{value}` | replace the focused field's contents (clear, then type) |
| `hb_wheel` `{x,y,space?,notches}` | scroll the mouse wheel (minimap / camera zoom in-world) |
| `hb_wait` `{ms}` | let an animation/transition settle (server-side sleep) |
| `hb_batch` `{steps:[{tool,arguments}]}` | run a SEQUENCE in one call; stops at the first failure. A screenshot step returns its image (at most 4 per batch); `hb_flow` is a valid step |
| `hb_wait_for` `{conditions,timeout_ms,since?,trail?,max_events?}` | block until the game says it is ready, instead of guessing a sleep. Lists the last 10 trail events by default; `trail` true = all, false = none, N = last N. An event condition also takes `object_id` and `message_contains` (matched by the client). `max_events` N (1..500) keeps waiting for N matching events and returns ALL of them (oldest first, each with its condition index; `k of N` on a timeout) instead of stopping on the first: the first match returns from the client's wait at once and the rest are read from the journal after it, so none is lost between calls. Event conditions only |
| `hb_journal_drain` `{channels?,types?,exclude_types?,max?,from_ms?,to_ms?,after_seq?,before_seq?,object_id?,message_contains?}` | read the event log newest-first: live with no window, history with one; consumes nothing. Any other key is an error naming the valid ones |
| `hb_journal_config` `{channels?,capacity?}` | enable/disable event classes, resize the ring |
| `hb_subscribe` `{enabled,channels?,filter?,exclude?}` | choose what reaches you while streaming |
| `hb_stream` `{enabled}` | live event notifications on/off |
| `hb_kill_player` | ask the SERVER to kill the character being played (the only way to reach a death-gated behaviour) |
| `hb_set_status` | ask the SERVER to put a status (name or id, `seconds`, optional `target_id`) on the character or a creature in view; debug builds only |
| `hb_reload_pivots` `{archive?,entry?,dir?}` | re-read sprite-sheet pivots from the watched `.hba` tree and apply them to the running client in place (debug builds); reports exactly which pivots moved |
| `hb_entities` `{kinds?,radius?,max?,on_screen?,raw?}` | what is around the player, NEAREST FIRST, one line each: `object_id`, kind, raw name, tile, distance, hp, `attackable` / `interactable`, and either `-> <click_action> at window (x,y)` or `NOT CLICKABLE: <blocked_by>`. `max` defaults to 25 |
| `hb_click_entity` `{object_id,right?,shift?,ctrl?}` | click an entity BY id: its point is resolved through `hb_entities`, refused (naming `blocked_by`) when it is not clickable, clicked with `space:"window"`; replies with the expected `click_action` and `consumed by`. A ground item is not clickable by entity - walk onto it and click yourself |
| `hb_tile_to_screen` `{x,y,raw?}` | where a map TILE is on screen (window + logical pixel), what a click there resolves to (`round_trip`), what is in front of it and whether it is the player's own sprite - the inverse of `hb_probe` |
| `hb_walk_to` `{x,y,wait?,timeout_ms?}` | walk to a tile by the client's own pathing (no pixel, any tile, even off screen or north of you). `wait:true` blocks for this goal's `walk_arrived` / `walk_stopped` and says how it ended. Refused with the client's reason (dead, teleporting, shop open, casting, outside the map); a blocked or unreachable tile walks as near as it can, never over a teleport (HB-0765) |
| `hb_set_time_of_day` `{t01}` | hold the client's sky clock at `t01` (0..1, 0.5 = noon) or `"off"` for the server's; client-side only |
| `hb_set_god` `{on}` | bot god mode on or off for the attached agent, through the `agent_flags` the server holds (refused without the bot flag, cleared on detach, re-sent on world entry and kept across a logout and re-entry). Works on any screen: before the world is entered it stores the wish and replies `sent:false`. Replies `god`, `flags_wanted`, `sent`; `hb_get_state` carries `agent_god` in the world. Debug only (HB-0724) |
| `hb_set_staff_tags` `{visible}` | show or hide the GM / bot tags: the orange tag over a character in the world and the tag leading a GM or bot chat line. Hide before a screenshot so the tag does not cover the shot. Client-local, default shown, needs the entered world. Replies `visible`. Debug only (HB-0871) |
| `hb_close_window` `{name}` | close an open window by registry name, `minimap`, or `all`; replies what closed. Needs the entered world. Debug only (HB-0718) |
| `hb_drag_slot` `{from, to_x+to_y or to_widget, steps?}` | drag an inventory item by slot: presses on the slot widget aim point (a solid pixel of the art), moves in legs, releases. Batchable (HB-0718) |
| `hb_send_chat` `{value, open?, submit?}` | open the chat box (`open`), clear it, type the line, press Enter. Batchable (HB-0718) |
| `hb_fishing_auto_strike` `{on, delay_ms?}` | arm a one-shot strike of the own fishing bobber on the next `pull` (a catch inside the window) or `nibble` (a spook), `delay_ms` after it (0..5000); `off` drops it. Needs an armed rod. A pull window (450-1050 ms) is shorter than a model round trip, so a test lands a catch this way. `hb_get_state` carries `fishing` (line, bobber click point, `cast_target`, `auto_strike`, `last`, `viewers` with each other bobber's `under`) and a flat `fishing_line`. Debug only (HB-0715) |
| `hb_cast_at` `{object_id,spell?,timeout_ms?}` | cast a spell AT an entity by id: arm `spell` through the spell book (opens it, pages the circle tabs, clicks the row, closes it) or use the armed one when `hb_get_state` reports `armed_spell`, resolve the entity's current point through `entities`, click it, wait for the cast. Reports whether the arm was confirmed (`spell_cast_begin`, `cast_ms`), `consumed_by`, and the cast's journal events (`spell_release_sent`, `spell_cast_resolved`, `spell_hit` = LANDED, `spell_cast_cancelled`, `cast_click_ignored`, the server's notice). A refusal or timeout is a result; an unreachable target is refused BEFORE arming |
| `hb_path_bench` `{fixture?,repeats?,raw?}` | time the click-to-move pathfinder candidates (A* shipped, A* flat, Jump Point Search; reachability pre-check off and on) on the live map from the player's tile - short, long, unreachable and occupied-goal requests - as a table with the correctness check (rows whose path cost or end differs from the shipped A*); `fixture` (inside the repo) also saves the timed map for `path_bench.exe` (HB-0398). Debug only; holds the client's frame while it runs |
| `hb_press_bench` `{display_id?,repeats?,live_path?,raw?}` | time the grass press field of every creature's last death frame, 8 directions: the shipped distance transform against the old brute force, with the max strength / push difference between the two fields and the cells differing; one row per creature, largest silhouette first (no `display_id` = every registered creature, one bridge request each). `live_path` `old` / `new` instead switches what the world presses with (`old` = every death frame its own brute-force shape, the pre-HB-0748 path, for an A/B). CLI: `hb_flow.py press-bench [--display-id N] [--repeats N] [--live-path old\|new]` (HB-0748). Debug only; holds the client's frame while a creature is benched; not batchable (a whole-registry bench outruns the 180 s batch budget) |
| `hb_inject` `{type,...}` | dispatch server messages built by the client's serializers as if the world socket read them (HB-0395): `script_strings {rows}`, `entity_subtitle_changed {object_id,slot,subtitle_id,colour?}`, `script_text {string_id,audience}`, `raw {id,payload?}` (a u16 message id, then the body as hex bytes exactly as given - malformed on purpose if you like; `id` 512 is `entity_motion_bundle`); replies `{type, queued}` |
| `hb_set_audio` `{master_muted?,master?,music?,effects?,persist?}` | set the client's audio live on ANY screen, in the world included: `master_muted` (bool), volumes as whole numbers 0..100, `persist:true` also saves them to `client.json` (default: lasts until exit). No arguments reads back. Replies with what is now set; `hb_get_state` carries the same as `audio:`. Batchable. This is the in-world mute (`hb_flow` `mute` / `unmute` use it when the client is logged in) |
| `hb_clients` `{action?,client?,port?,host?,ports?}` | list / discover / register / forget clients, or set which one an omitted selector means. Forgetting or discovering the last client leaves nothing registered; `register` names who holds a port that does not answer; a client whose port has NO listener (process gone) is dropped automatically by `list` or by a failed call |
| `hb_parallel` `{groups}` or `{clients,steps}` | release several clients from one barrier so they act at the same moment |
| `hb_flow` `{flow, identity_file?, name?, preset?, direction?, x?, y?, screen?, ...}` | run one whole `hb_driver` flow (log in, create a character, enter, step, switch, time an entry, ...) over this server's own connection - see "Reusable flows" below. Top-level arguments only, checked per flow; names take `_` or `-` |

**Use `hb_batch` by default for anything longer than one action.** The bridge answers each command
within a frame (~16ms), so the cost of driving the client is the number of TOOL CALLS, not the number
of actions - ten clicks in one batch are effectively free next to ten separate calls.

## Reusable flows (`hb_driver`, `hb_flow.py`)

Do not write a scratch driver. `hb_driver/` is the tracked, parameterized library of the flows every
agent used to rebuild. It drives the client over the same wire as this server and shares this server's
`bridge_client`. It adds nothing to the client. `hb_flow.py` is a thin CLI over it, and the `hb_flow`
tool runs the in-session flows through the MCP server.

| module | what it gives you |
|---|---|
| `bridge` | `session(port)` / `session.connect(port)`: `call`, `submit`, every primitive, `wait_for` (ANY), `wait_for_then` (the wait is registered BEFORE the action), `wait_screen`, `wait_widget`, `wait_event`, `journal_clock`, `drain_journal` (non-consuming, oldest first) |
| `client_process` | `client_process(port, exe, ...)`: launch from the EXISTING exe path (the bridge is checked by its log string, so a Release exe is refused), `wait_bridge`, `stop` (its own PID only), window restore; `stop_recorded(pid_file)` for PIDs a previous process launched |
| `client_files` | `client_files_guard`: byte copies of `client.json` and `remembered_account.txt`, restored byte-identical and checked by SHA-256; `premute(client_json)` |
| `identity` | `new_identity(path)` / `load_identity` / `delete_identity`: a throwaway account, password and characters in a JSON file under `working/`. Only `public()` output is ever printed (the password masked) |
| `flows` | `go_to_main_menu`, `set_master_mute`, `create_account`, `log_in`, `create_character`, `select_character`, `delete_character`, `enter_world`, `switch_character`, `log_out`, `quit_client` |
| `world` | `player_tile`, `calibrate`, `aim_tile`, `click_tile`, `step`, `time_entry` |
| `server_logs` | `log_watch`, `wait_logout_saved` (save queued, then session closed, then persistence batch) |
| `player_db` | `read_position`, and `write_position` under every `test.md` guard |
| `running` | `launch_client(port)`: a context manager that saves the files, mutes through `client.json`, launches, and on ANY exit stops that PID and then restores the files |

```python
import sys; sys.path.insert(0, r"<repo>\src\tools\agent_mcp")
from hb_driver import flows, identity, running, world
ident = identity.new_identity(r"<repo>\.claude\scratch\<role>\<task>\ident.json")
with running.launch_client(port=8892, exe=r"<repo>\game\client\Game_x64_win.exe") as run:
	flows.create_account(run.session, ident)
	flows.log_in(run.session, ident)
	name = flows.create_character(run.session, ident, preset="Warrior")["name"]
	flows.enter_world(run.session, name)
	world.step(run.session, "e")
```

`python .claude/tools/agent_mcp/hb_flow.py --help` lists the CLI flows. Every state that must outlive one
CLI call lives in a file under `working/`: the identity, the pid file, the client-file backup and the
logout proof. Path arguments (`--backup-dir`, `--pid-file`, `--proof-file`, `--identity`, `--file`) may be
relative: they resolve against the repo root, not the cwd; so does the MCP `identity_file`. `logout` ends on the
main menu, or with `to:"login"` (CLI `--to login`) on the login screen. Over MCP, the `switch` and `logout` flows take
`proof_file` (a path under `working/`, relative = from the repo root), `name` (default: the character being played)
and `save_timeout` to write the same logout-save proof - the world-log lines proving the save committed - and return
it as `logout_proof` / `logout_proof_file`; a bad path is refused before the logout runs. Over MCP, `hb_flow` takes its arguments
top-level and refuses a key the flow does not take (an `args:{...}` wrapper, a stray `x` on `step`) with the
list of keys it does take. Tests:
- `python -I .claude/tools/agent_mcp/hb_driver_selftest.py` runs the library against an in-process fake client;
- `python .claude/tools/agent_mcp/smoke_test.py` covers the MCP server and the `hb_flow` tool against a fake bridge.
  Run both from the repo root.

How the flows handle the traps that cost earlier runs time:
- **Modal messages** ("Account created", "name taken") report `overlay.ok`, listed first in `hb_widgets`
  (a request still in flight reports `overlay.cancel`, and the entering popup `entering.cancel` /
  `entering.back`). While one is up, the page's widgets report disabled. The flows click `overlay.ok` by
  name; a client build without the overlay widgets is dismissed with a click on a point no widget covers.
  Enter is never used: on character select, Enter enters the world. The refusal reason is read from
  `loginserver.log`.
- **Master mute** has two routes. Logged in (the world, character select / create) the flow calls the
  bridge's `set_audio` - no logout - and proves it by the reply's read-back (and `client.json`, saved with
  `persist`, when one is named): `via: set_audio`. On the main menu it clicks the Settings tick
  `settings.sound_master_enabled` (an older build: any `settings.*master*_enabled`), under the category
  `settings.cat_*sound*`, whose value `on` means sound plays: `via: settings`. `leave_session` forces the
  Settings route by logging out first. Either way the flow SETS the state rather than toggling it. The Settings
  rows are all reported by id now (`settings.<name>`), in the world too.
- **Entering the world**: the ready marker is `screen_changed {to: Screen_OnGame}`, followed by
  `object_id` in `get_state`. The spawn may land before the screen swap or after it (HB-0156).
- **Tiles are aimed by probe.** The pitch is measured by bisecting tile boundaries, not assumed: 34 x 25.5
  logical px at 853x480, measured 2026-09-30. A point on the HUD, on your own sprite or on a creature is
  never clicked. Over MCP the measurement is kept per client connection (a step inside a batch does not pay
  ~40 probes again); pass `recalibrate:true` after a camera zoom.
- **A walk is judged by the client's own walk events.** Every walk - a plain click included - journals
  `state.player` `walk_started {from_x, from_y, goal_x, goal_y, path_len, reachable, source}` and ends with
  `walk_arrived {tile_x, tile_y, goal_x, goal_y, arrived:true}` or `walk_stopped {..., arrived:false, reason}`
  (reason: `new_order`, `blocked`, `interrupted`, `map_changed`, `dead`, `in_range`). `click_tile` /
  `step` / `walk_to` report the FIRST answer (`outcome`, `answer_event` - a multi-step walk is confirmed step
  by step, so that is not the arrival), then follow the walk until it ends: `arrived`, `ended` (`arrived` |
  `stopped` | `timeout`), `reason` (a stop's reason), `judged_by` (`event` first; `journal` on a client without
  walk events, where the newest `player_moved` and the probe decide and 2 s without progress is a stop with
  reason `no_progress`), `player_tile` (the probe), `last_move_to`, `moves` and `tiles_short`. Only the event
  FOR THIS GOAL counts: the previous order's own `new_order` stop, fired by the new click, has another goal.
  `player_moved` lands a frame BEFORE `walk_arrived`, so while a `walk_started` for this goal has no end yet the
  journal is not the verdict (a single-tile step included: `step` is `judged_by: event` with `walk_end` set);
  only a client whose end event never arrives falls back to the journal after one second at the target.
  A `no_answer` carries a `hint`.
- **`walk_to` vs a click.** The `hb_walk_to` tool (and `hb_flow.py walk-to` on the command line; there is no
  `walk_to` flow over MCP) orders the walk through the client's own pathing: no pixel, so it works for a tile
  off screen, under a window, or directly NORTH of the player (a click there lands in the click-on-self box
  and is a pickup). `step` is a real click. Only that click-on-self case falls back to `walk_to` on its own
  (`via: walk_to`, `click_blocked`, `click_blocked_reason: self_box`); a tile covered by a window or the HUD, off
  screen or blocked FAILS with its reason, so a click test is never silently turned into a walk.
  `walk_fallback:true` (`--walk-fallback`) forces the walk for any such failure, `false` (`--no-walk-fallback`)
  never walks. Use `click_tile` to test what a real click does. Aiming itself now
  starts from `tile_to_screen` (three calls) instead of measuring a pitch (~40 probes) and falls back to the
  measurement on a client without it.
- **Launch mute proof.** `hb_flow.py launch` / `running.launch_client` read the Settings tick back on the main
  menu; on a logged-in start screen (`charselect`, `charcreate`) a config mute is proven from client.json's
  `audio.master_muted`, and a bridge mute is made with `set_audio` (`flows.prove_launch_mute`).
- **Map-entry timing** (`time_entry`) reads the client's own readiness markers from `ui.screen`:
  `screen_active` (the fade-in is over), `first_frame_presented` and `world_ready {map, tile_x, tile_y}` (the end
  of the frame that first drew the entered world, HB-0159). They appear in `points_ms` with the spans
  `start_to_world_ready`, `response_to_world_ready` and `world_ready_to_move_request`, and `ready_marker` says
  which was used (`world_ready`, or `spawn_frame` on a client without it). The older milestones are kept.
- **A character is selected by its card label**, with one click at most. A second click on the same card
  is a double-click, and that enters the world.

## Driving several clients

Every tool above takes an optional **`client`** handle. **Omit it and nothing changes**: the call goes
to the only registered client, or to the session default when several are registered. A one-client
session never needs any of this.

- **One client per port.** Launch extras with `--agent-bridge-port=N` from `game/client`. Handles are
  fixed by port - 8891 is `c1`, 8892 is `c2`, up to `c8` - so a handle written into a recipe keeps
  meaning the same client. `hb_clients {action:'register', client:'corwin', port:8893}` names one
  yourself, and a later discovery scan leaves a hand-named handle alone.
- **`hb_clients {action:'discover'}`** scans 8891-8898, registers what answers, drops auto-registered
  entries that no longer do, and reports each client's port, reachability and current SCREEN; `hb_get_state` carries
  `character` and `object_id` once a client is in the world, which says which character each one is playing.
- **`hb_batch` across clients is a sequence.** Steps run strictly in order whichever client each names,
  so a batch that spans clients is never simultaneous and never atomic.
- **`hb_parallel` is the same-moment tool** - `{groups:[{client, steps}]}`, or `{clients:[...], steps}`
  when the steps are identical (they must not be when a coordinate is involved: clients can come up at
  different window sizes). Sockets are connected and warmed first, then every group is released from
  one barrier onto its own thread. **Measured over 8 real clients, 2026-09-05: dispatch skew
  0.08-0.39 ms**, and a **first-reply spread of ~13 ms** - which is the per-client frame pump, since a
  bridge command is answered by the frame that handles it. That second number is dispatch-to-ACK, so a
  first step that starts a heavy transition inflates it (a screen change measured 1060 ms). **Server-
  side ordering is not controlled at all**: this narrows a race, it does not decide one.
- Replies gain a `[c2]` tag and screenshots a `_c2` filename suffix only once more than one client is
  registered, so the single-client transcript is unchanged.
- The journal is per client: its own pump, its own cursor, its own `hb://journal/<handle>/recent`
  resource (the bare `hb://journal/recent` follows the default client). `hb_stream` and `hb_subscribe`
  accept `client:'*'` to apply to every registered client at once.
- **Restart the MCP host** after this file's tool list changes - project MCP servers load their tools
  at session start.

## Coordinates

Two spaces, selected per mouse call by `space`:
- **`logical`** (default) - the game's logical UI space (640x480 for menus; `ui_width`x480 in-world,
  where `ui_width` widens to fill the window).
- **`window`** (aliases `output`, `screenshot`) - **the pixel you measured off a screenshot.** A
  screenshot is captured at the render OUTPUT resolution, so its pixels are output pixels; the bridge
  maps them with `output_to_logical` (output size + logical presentation rect), which is correct even
  under **high-DPI** where output pixels != window points (the window is created HIGH_PIXEL_DENSITY, so
  on a scaled display the screenshot is LARGER than the window's point size). Use this in-world and for
  any "click what I see" action. (Real human mouse input, by contrast, arrives in window points and uses
  `window_to_logical` - a separate path, unchanged.)

Injected input runs through the same event path as real input (see `sources/client/agent_bridge.cpp`). Use
`hb_probe` to verify a pixel resolves where you expect (and, in-world, which tile/creature it hits)
before clicking - it also reports `output_size` vs `window_size` so you can see the DPI factor. Note the
camera is player-locked, so a fixed screen pixel maps to a *different* tile after the player moves -
re-`hb_probe` (or re-screenshot) between moves rather than reusing a stale coordinate.

## Killing the player on purpose (`hb_kill_player`)

Debug builds only, both sides. It exists because otherwise **nothing gated on player death is testable
at all**: the server sets `client_session::dead` only when a creature lands a lethal blow, and reaching
a creature from the town spawn is a multi-map walk with no return teleport - which leaves the dead chat
gate, the revive flow, corpse rendering for observers and the death UI unreachable.

The client sends `debug_kill_request` (message `0x00F1`, no payload) and **sets nothing locally**. The
server kills through its own existing `kill_player` path and pushes the ordinary
`attack_event{attacker_id=0, killed=1, damage=<hp removed>}` + `entity_hp{hp=0}` back, which is what
puts the client into the dead state. So the resulting state is a real death, not a simulated one, and
the two sides can never disagree about whether the player is dead. `attacker_id=0` is the "no entity"
sentinel: nothing struck the player, so no swing pose or swing sound plays for anyone.

Practical notes:
- The tool returns when the request is **sent**, not when the player is dead - allow a round-trip
  (`hb_wait {ms:500}`) before reading `hb_get_state` or taking a screenshot.
- Only an entered, LIVING session is honoured; the server ignores a corpse asking again, and the
  client refuses outright when it is not in the world (`kill_player needs an entered world with a
  live server connection`).
- Get back up with the **Revive** button on the death window, exactly as after a real death.

## Events and the journal

The client records what happens into a ring buffer; you either PULL it (`hb_journal_drain`) or have
it PUSHED (`hb_stream` + `hb_subscribe`). This is how you answer "did the game actually do it"
without screenshotting and guessing.

**Channels.** Observational: `state.player`, `state.map`, `state.entity` (and its half
`state.entity.motion`: `entity_moved` / `entity_stopped`), `audio.sound`,
`visual.damage`, `visual.effect`, `net.packet`, `chat.message`, `ui.screen`, `ui.window`,
`ui.button`. Newer events: `ui.window/click_consumed {x,y,button,kind,owner}` (a click a window, modal, bar or
map took from the world), `audio.sound/music_start|music_stop {id,reason}`, `state.map/weather_changed`, and
`window_closed`'s `via` is `close`, `dismissed` or `esc`. Diagnostic: `client.warnings`, `client.errors`, `net.warnings`, `net.errors`. A selector
matches by **dotted-segment prefix**, so `net` takes `net.packet` and both `net` diagnostics, while
`stat` matches nothing (it is not a whole segment of `state.player`).
`['client.errors','net.errors']` is a standing watch on everything that goes wrong.
`net.warnings` is declared but silent. `net.packet` events cover both sockets: the login socket's carry
`socket:"login"` (the world's do not), and a world `packet_in` has `read_us` and `read_to_drain_us` (the
read and the read-to-drain latency). `net.errors` has `connect_failed` and `request_failed {reason, elapsed_ms}`.
`ui.screen` also carries `screen_active`, `first_frame_presented` and `world_ready {map, tile_x, tile_y}`
(what `time_entry` reads). `message_contains` matches an event's `message` (a packet name) or `text` (a chat line).

**The row shape.** `{seq, t_ms, frame, channel, type, data}`. A run of CONSECUTIVE events with
identical `channel`, `type` and `data` collapses into ONE row that adds `count`, `seq_last` and
`t_ms_last`; a run of one carries none of those and looks exactly as it always did. This exists
because a busy scene blew the tool's output cap - 155,616 characters at `max:2000` - and a filtered
drain of a repetitive scene measured 17,195 -> 296 characters once collapsed.

**`count` is authoritative and cannot be derived.** A filtered drain coalesces the FILTERED
sequence, so a run's seqs are not contiguous: a 104-event footstep run spanned seq 751..1263.
`seq_last - seq + 1` would say 513. Always read `count`.

**Reading is live-first and consumes nothing** (user design, 2026-09-28: "we want to always see
the newest event, specifying a time frame would scope us to the history, removing the specified
timeframe would return us back to the live"). `hb_journal_drain` sends the client's `journal_query`:

- **No window = live.** The newest `max` matching events (client default 200), NEWEST FIRST, headed
  `live (newest first), now t=...ms - newest_seq N`.
- **A window = history.** `from_ms` / `to_ms` (journal ms) and/or `after_seq` / `before_seq`, newest
  first within it, headed `history window ... - omit from/to to return to live`. Page older with the
  reply's `older_before_seq` as `before_seq`.
- **What did my action produce:** note the header's `newest_seq`, act, then read with `after_seq` set
  to it - or `hb_wait_for` the event itself.
- `max` counts RAW events, not rows. A read never moves a cursor or clears the eviction count, so any
  number of reads, filtered any way, cannot hide an event from the next one.

The consuming `journal_drain` wire command survives for ONE caller: the streaming pump
(`hb_stream` / `hb_subscribe`, polling every `JOURNAL_POLL_SECONDS`), which owns a private cursor.
Nothing folds into it any more.

**`hb_journal_config` with no arguments is a read-back** of the resolved channel map.

## Known limitations (and how to work around them)

- **Clicking an entity is two bridge commands.** `hb_click_entity` resolves the point with `entities`, then
  clicks it; a fast mover can step off the point in between (the `click_action` it reports is what the client
  EXPECTED, not proof it landed - read `consumed_by` and the journal). Ground items are never clickable by
  entity: they are picked up by standing on the tile and clicking the player's own sprite.
- **Screenshots are full-window PNGs (3.4-3.8 MB each measured in-world and at character select; a busier scene can be larger).** `build/screenshots/` has a hard 20MB cap
  (user rule); prune back under it before finishing, and only your own captures.
- **A drag is `hb_mouse_down` -> `hb_mouse_move` (as many as it needs) -> `hb_mouse_up`.** `hb_click`
  still presses and releases at ONE point in one frame and can never express a drag; the two halves
  are separate commands for that. Put the whole gesture in ONE `hb_batch` - between two tool calls the
  client is left holding the button, and anything that fires on a release (or on losing focus) can go
  off in the gap. The pair is what makes the drag-only surfaces reachable: the chat resize grip, the
  bag window's item tooltip (drawn mid-drag only), and cross-window item drags.
- **The arrow keys do not move the player.** They are accepted and do nothing in-world; movement is
  click-only. A consequence: walking into a wall to provoke a server `move_reject` has no reachable
  input path, because click-to-move pre-filters blocked tiles client-side.
- **The sky is readable and settable; sub-tile position is not.** `hb_get_state` carries `sky_time01`,
  `sky_time_override` and `map {light, weather}`, and `hb_set_time_of_day` holds the client's sky clock (a map
  that does not follow the day cycle ignores it). There is still no sub-tile position anywhere:
  `state.player`'s movement event is a tile-boundary DELTA by design, not a per-frame position, so
  render-smoothness questions are not answerable from here at any sampling rate.

- **`hb_widgets` covers UI, not the world.** It reports what a screen and its dialogs declare as
  interactive. The world has its own tools: `hb_entities` (creatures, NPCs, players, ground items),
  `hb_tile_to_screen`, `hb_click_entity`, `hb_walk_to`, and `space:"window"` + `hb_probe` for a raw pixel.
- **A latched modifier outlives your test.** `hb_modifiers` holds a key down until something releases
  it, so a forgotten `{"alt": true}` makes every later attack critical and the next test inherits a
  client that is lying to it. Release in the same batch that latches, and `hb_modifiers` with no
  arguments reads back what is held.

Resolved by later additions: field replacement -> `hb_set_text`; clicking what you see + verifying
translation -> `space:"window"` + `hb_probe`; hardcoded menu coordinates -> `hb_widgets` +
`hb_click_widget`; held modifiers (Alt criticals, Shift run) -> `hb_modifiers`. `hb_wait_for`'s event
condition works and always did - the key is `channel`, and an earlier revision of this file said
`class`, which is a key the client has never accepted.

## Screenshots

`hb_screenshot` saves each capture to `build/screenshots/` (resolved from this script's own
location, so the launch directory does not matter) and also returns it inline. Pass `name` to
label the file (`hb_screenshot {"name": "login_filled"}` -> `build/screenshots/login_filled.png`);
without it the stem is a timestamp (`shot_YYYYMMDD_HHMMSS.png`). Existing files are never
clobbered - a numeric suffix is appended. Override the directory with the `HB_SCREENSHOT_DIR`
env var. A full capture is a 4-6 MB PNG, and `build/screenshots` has a hard 20 MB cap, so shrink what
you keep:

- `region {x,y,w,h}` crops in OUTPUT pixels (the pixels a `space:"window"` click uses), `scale` (0.05-1)
  shrinks with a **Lanczos** filter after the crop, `format` is `png` (default) or `jpg`, `quality` 1-95
  (jpg, default 85). `path` saves to exactly that file (relative = under `build/screenshots`, absolute must be
  inside the repo; an existing file is replaced; give `name` or `path`, not both). `inline:false` saves
  without returning the image. The reply names the sizes: `saved: <file> [1920x1080 -> 480x270 jpg, 31 KB]`.
- These options are done in this server with **Pillow**, an OPTIONAL import - the rest of the server is
  stdlib-only. Without Pillow (`pip install pillow`) `region` / `scale` / `format jpg` return a clear error and
  a plain capture still works. The client's raw PNG is processed and removed, so the folder only holds what
  was asked for.
- Inside `hb_batch` / `hb_parallel` a screenshot step's image comes back with the reply, at most 4 images per
  call (a later step still saves its file and says it was not attached).
- The client always captures the whole output; a crop is made here, never in the client.
- **Bursts.** `hb_screenshot {burst: N, interval_ms}` (N 2..30, `interval_ms` 0..10000, default 250) takes N
  captures with every option applied (`jpg`, `scale`, `region`, `quality`), each saved as `<name>_01`, `_02`, ... and
  listed with its offset in ms. At most 4 images come back (the first, the last and evenly spaced ones; the reply
  names the frames) and `inline:false` returns none; every file is saved either way. `interval_ms` is the
  start-to-start gap and a FLOOR: the Debug client takes about 1.75 s to write one full-window PNG, so frames land
  about 2 s apart however small the interval (the reply shows the real offsets). The folder cap is enforced:
  a burst is REFUSED up front when `build/screenshots` has no room for one raw capture (6 MB headroom under the
  20 MB cap), and REFUSED after the first frame when N frames of that size would not fit - the frame it took is
  removed. Use `format jpg` with `scale` or `region` (a 0.4 scale jpg is ~55 KB) for long bursts.
