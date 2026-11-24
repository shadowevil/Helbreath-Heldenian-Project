# decisions: World/screen rendering and the client agent MCP

*Sidecar of `.claude/docs/decisions.md` (index, scope rule and entry format live there). Each entry is a settled user ruling with the user's exact quote.*

## d-024 World and screen space are drawn separately

- Date: 2026-11-23.
- Quotes: "UI/floating text and screens that are not maingame should be screenspace always never drawn at world resolution 800x600"; "Should remain uniform however the action bar must always take up the same percentage of screen space as 800x600 would"; selected answers "Native-res TTF", "Overlays on top", "Approve, start phase 1".
- Decided: the world renders at 800x600 and is upscaled; UI, floating text and non-game screens render in screen space with native-resolution text and a uniform UI scale; the HUD action bar spans the world rect; world-anchored overlays draw above late world effects.
- Recorded in: `.claude/design/plans/world-screen-split.md`.

## d-027 Client agent MCP bridge uses SDL's debug guards

- Date: 2026-11-23.
- Quote: "After we finish all of the tickets, lets begin implementing SDL's MCP server for The Heldenien Project"; "Same debug guards as SDL".
- Decided: the bridge compiles only under `DEF_CLAUDE_TESTING`, as in Helbreath-SDL. A shared header derives that macro from `_DEBUG` (SDL's is `src/shared/util/claude_testing.h`), and nothing defines it by hand. Work starts after the current tickets.
- Recorded in: HH-0032.

## d-028 Client agent MCP: ports, location, server commands

- Date: 2026-11-24.
- Quote (answers chosen by the user): bridge ports "8891-8898"; Python location ".claude/tools/agent_mcp"; SDL's server-side debug commands (kill, status, god) "Defer".
- Decided: the client bridge listens on 127.0.0.1, ports 8891-8898, apart from SDL's 8791-8798. The ported Python lives in `.claude/tools/agent_mcp/`. Server-dependent commands are left out until the user decides on them.
- Recorded in: HH-0032; `.claude/design/plans/client-agent-mcp.md`.

## d-029 Full-width dims and bars cover the whole window in stretch

- Date: 2026-11-24.
- Quote (answer chosen by the user): "Whole window" for the dimmed backgrounds behind pop-ups and the chat and message bars on a widescreen window in stretch mode.
- Decided: UI fills that span the full 800x600 design width cover the whole window, matching the stretched world; other UI keeps the uniform 4:3 scale.
- Recorded in: HH-0034.
