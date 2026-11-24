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

## d-031 Test accounts and characters without asking

- Date: 2026-11-24.
- Quote: "You may create test accounts and characters as needed. No need to ask. Just keep track of them and reuse as needed".
- Decided: agents create throwaway test accounts and characters on the local game server without asking. Each one is recorded in `.claude/local/test_accounts.json` (gitignored), and existing entries are reused before a new one is made. The given context was the running local Debug server in `game/server`.
- Recorded in: `.claude/local/test_accounts.json`; memory `test-accounts`.

## d-032 The minimap uses SDL's three-tier pyramid

- Date: 2026-11-24.
- Quotes: "Using SDL's minimaps can we replace our minimap functionality with SDL's version which uses 3 tier minimaps? We don't need the 2k version though"; answers chosen by the user: "Hide it" (maps without SDL art) and "JPG q92 (Recommended)" (image format).
- Decided: the minimap shows SDL's baked levels 01-03 per map (level 00, the 2096 px level, is left out), stored in `staged/` as JPG quality 92. Maps without SDL art show no minimap.
- Recorded in: HH-0045.

## d-033 Two-tier minimap; the top tier becomes the parchment map view

- Date: 2026-11-24.
- Quote: "Minimap stays as it is, but has two levels, the lower quality full map zoom level. A step up from that which is what we have now. Then we bake 1 parchment at what the tier 3 minimap is now, and replace that so now instead of 3 tier minimap it's 2 tier with a dedicated parchment map view?"
- Decided: the corner minimap keeps SDL's two lower levels (whole map, mid) and drops the finest level; that level's role moves to one baked parchment per map, shown as the dedicated parchment map view. Supersedes d-032's three tiers.
- Recorded in: HH-0048.
