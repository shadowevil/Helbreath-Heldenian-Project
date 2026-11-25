# decisions (index)

*Assessed 2026-11-23 (docs standard sect. 1) and split the same day (HH-0025): the file was 15 KB, over the 10 KB goal (d-026). Entries are grouped by topic into six sidecars of 2-3.5 KB, so a lookup is this index plus one sidecar. Read this when you are about to change something that one of these rulings settles, or when asked "why is it like this".*

**Add a decision:** append the `## d-NNN Title` entry (date, exact quote, decided, recorded in) to the sidecar of its topic, then add one line here.

**Scope (user, 2026-11-23): "the decisions pipeline is strictly for set-in-stone decisions that have been made. Nothing else."** An entry is a settled decision the user made, in the user's own words. No proposals, assumed defaults, open questions, pending or deferred work, measurements, status, history of how a decision was reached, or choices an agent made unasked. No exact quote, no entry. Where the rule itself lives is the "Recorded in" line; this file never restates it.

Entry format: date, the user's exact quote, what was decided, where it is enforced or recorded.

Sidecars are under `.claude/docs/decisions/`. Each line: id, title - scope; sidecar; WHEN to read it.

## Versioning, build proof and databases (`decisions/versioning-and-database.md`)
- d-001 SDL versioning and a SQL source directory - Helbreath-SDL's versioning adopted in full; `sources/database/` is the source of truth. Read before version or database work.
- d-002 Clock-skew window exchange stays - the build proof keeps it. Read before touching the login proof.
- d-008 Old versioning files removed - the INI system and build counters are gone. Read before reintroducing a counter or INI.
- d-017 Operator-state tables preserved - `admins`, `banned_list`, `admin_command_permissions` rows carry over rebuilds. Read before changing `build_databases.py`.
- d-018 Build tools may run read-only git - the Build ID generator. Read before adding git calls to a build tool.
- d-019 Clock-skew retry - skew-refused login gets the server window and retries once. Read before changing login retry.
- d-020 Game-port gate bypass closed - versioned enter-game, same address and password. Read before changing the game-port gate.

## Build layout, deploy and generated files (`decisions/build-layout.md`)
- d-003 Layout: sources, bin, build, staged, game - what each folder holds; deploy assembles `game/`. Read before moving or writing build output.
- d-004 Static files change only in `staged/` - read before editing anything under `game/`.
- d-005 Client configs generated and persist - deploy never overwrites them. Read before changing deploy or client settings.
- d-006 Server configs generated with defaults and persist - same for `server_config.json`, `gameconfigs/`. Read before changing deploy or server config.
- d-007 Solution file at the repo root - `Helbreath.sln` location. Read before moving project files.

## Naming and the Phase R rename (`decisions/naming-and-phase-r.md`)
- d-009 Files and folders are snake_case - read before naming or renaming files.
- d-010 Phase R through a dry-run and a review - scripts are dry-run and reviewed first. Read before a bulk script.
- d-013 W7 is a global hook - the naming check. Read before changing hook enforcement of naming.
- d-021 Phase R naming choices - acronyms, interface `I`, deferred data, kept names. Read before renaming a file or type.

## Docs standard and CLAUDE.md size (`decisions/docs-and-claude-md.md`)
- d-011 Docs follow the global docs standard - lowercase-hyphenated, lean indexes plus sidecars. Read before adding or restructuring a doc.
- d-012 Tools location is each project's call - `.claude/tools/`, no hook. Read before placing a new tool.
- d-016 CLAUDE.md stays lean - short rule plus pointer. Read before growing CLAUDE.md.
- d-026 Docs linked from CLAUDE.md stay under 10 KB - audit, then split or keep with a dated reason. Read before a doc nears 10 KB.

## Git, tickets and how agents work (`decisions/git-tickets-workflow.md`)
- d-014 Git follows the global rules - the project's no-git rule is gone. Read before running git.
- d-015 Original-Helbreath is the source of truth - read before guessing at original behaviour, protocol or data.
- d-022 Commit to Development only; no push until told - read before any push.
- d-023 Adopt the ticket system - prefix, board, CLI location. Read before touching ticket configuration.
- d-025 Small tickets as tasks and sub-tasks - in_progress rules, comments, the user views only. Read before filing or moving a ticket.
- d-030 Finish the tickets; park in `user_review` only what screenshots or agents cannot verify - read before stopping work to wait on the user.

## World/screen rendering and the client agent MCP (`decisions/world-screen-and-mcp.md`)
- d-024 World and screen space drawn separately - 800x600 world upscaled; UI native-res and uniform. Read before changing rendering or UI scale.
- d-027 MCP bridge uses SDL's debug guards - `DEF_CLAUDE_TESTING` derived from `_DEBUG`. Read before touching the bridge build guard.
- d-028 MCP ports, location, server commands - 8891-8898, `.claude/tools/agent_mcp/`, server commands deferred. Read before changing the bridge.
- d-029 Full-width dims and bars cover the whole window in stretch - read before changing stretch-mode UI fills.
- d-031 Test accounts and characters without asking - create as needed on the local server, track and reuse them. Read before an in-game test.
- d-032 The minimap uses SDL's three-tier pyramid - levels 01-03 as JPG q92, no 2k level, hidden on maps without art. Read before changing the minimap or its images.
- d-033 Two-tier minimap; the top tier becomes the parchment map view - supersedes d-032's three tiers. Read before changing the minimap or the parchment view.

## UI look (`decisions/ui-look.md`)
- d-034 Text uses the classic drop shadow, rarely an outline - black copy +1 px right and down. Read before styling recreated text.
