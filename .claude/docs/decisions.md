# decisions

*Assessed 2026-11-23 (docs standard sect. 1): kept whole, one file. Each entry is a few hundred bytes, so sidecars would be fragments (sect. 6); read the Contents line, then the one entry you need. Read this when you are about to change something that one of these rulings settles, or when asked "why is it like this".*

**Scope (user, 2026-11-23): "the decisions pipeline is strictly for set-in-stone decisions that have been made. Nothing else."** An entry is a settled decision the user made, in the user's own words. No proposals, assumed defaults, open questions, pending or deferred work, measurements, status, history of how a decision was reached, or choices an agent made unasked. No exact quote, no entry. Where the rule itself lives is the "Recorded in" line; this file never restates it.

Entry format: date, the user's exact quote, what was decided, where it is enforced or recorded.

Contents: d-001 SDL versioning - d-002 clock-skew exchange - d-003 sources/bin/build/staged/game layout - d-004 mapdata and static files - d-005 client configs - d-006 server configs - d-007 solution file at the root - d-008 old versioning files - d-009 snake_case files and folders - d-010 Phase R method - d-011 docs follow the global docs standard - d-012 tools location - d-013 W7 global hook - d-014 git follows global rules - d-015 Helbreath source of truth - d-016 CLAUDE.md size.

## d-001 Adopt SDL's versioning and a SQL source directory

- Date: 2026-11-23.
- Quote: "Every bit of SDL's versioning needs to be adopted - we should have a single source of truth SQL directory with every table having a sql file with it's structure and a side car file with it's prepopulated data".
- Decided: Helbreath-SDL's versioning standard is adopted in full, and `sources/database/` is the single source of truth for the databases, one `.sql` and one `.data.sql` per table.
- Recorded in: CLAUDE.md "Versioning"; `.claude/docs/versioning.md` and its sidecars; `sources/database/README.md`.

## d-002 The clock-skew window exchange stays

- Date: 2026-11-23.
- Quote: "Every bit" (from d-001); the user ruled that this means the build proof keeps SDL's clock-skew window exchange.
- Decided: the build proof keeps the clock-skew window exchange.
- Recorded in: `.claude/docs/versioning/standards.md`, `.claude/docs/versioning/build-proof.md`.

## d-003 Layout: sources, bin, build, staged, game; deploy

- Date: 2026-11-23.
- Quotes: "Can we also clean up our Sources folder to be just that sources? Not build artifacts?"; "We should move all build artifacts into a build folder in the base directory, then a separate bin/<project>/*.* for actual end-user files."; "bin is the build binaries"; "game is assembled by the build, staged is the source/reference - when we build we build the necessary bin files - then we deploy. Deploy means actually building the game/*/* directory with the staged files".
- Decided: `sources/` holds sources only; build artifacts go to `build/`, binaries to `bin/<project>/`; `staged/` is the tracked source of static files; `game/` is assembled by deploy.
- Recorded in: CLAUDE.md "Build and deploy"; `.claude/docs/build.md` "Layout and deploy"; enforced by `sources/build_tools/deploy.py`.

## d-004 Static files change only in staged/

- Date: 2026-11-23.
- Quote: "that way any changes always stay in sync, and staged is the only place we make modifications to static files that aren't compiled".
- Decided: `staged/shared/mapdata` is the source and deploy copies it to `game/`; static files are edited in `staged/` only.
- Recorded in: CLAUDE.md "Build and deploy"; `.claude/docs/build.md`.

## d-005 Client configs are generated and persist

- Date: 2026-11-23.
- Quote: "Those are generated and persist throughout builds".
- Decided: client configs such as `settings.json` are generated at runtime, nothing for them goes in `staged/`, and deploy never overwrites them.
- Recorded in: CLAUDE.md "Build and deploy"; `.claude/docs/build.md`; enforced by `sources/build_tools/deploy.py` protected paths.

## d-006 Server configs are generated with defaults and persist

- Date: 2026-11-23.
- Quote: "These should be generated with default values upon runtime and persisted through consecutive runs".
- Decided: `server_config.json` and `gameconfigs/` are generated with defaults at runtime and kept between runs; deploy never overwrites them.
- Recorded in: CLAUDE.md "Build and deploy"; `.claude/docs/build.md`; enforced by `sources/build_tools/deploy.py` protected paths.

## d-007 The solution file lives at the repo root

- Date: 2026-11-23.
- Quote: "yes it should" (answering whether `Helbreath.sln` moves to the repo root).
- Decided: `Helbreath.sln` sits at the repo root.
- Recorded in: `.claude/docs/build.md`.

## d-008 Old versioning files are removed

- Date: 2026-11-23.
- Quote: "Clean up any leftover old versioning files as well".
- Decided: the INI-based version system and its build counters are removed, not kept alongside the new one.
- Recorded in: CLAUDE.md "Versioning"; `.claude/docs/versioning.md`.

## d-009 Files and folders are snake_case

- Date: 2026-11-23.
- Quote: "We also want to convert everything to snake casing files and folders".
- Decided: project source files and folders are renamed to snake_case; the project rule was then replaced by the global naming rule, and the user kept the rename.
- Recorded in: CLAUDE.md "Coding standards" (naming bullet); `~/.claude/CLAUDE.md` "File and folder names".

## d-010 Phase R runs through a dry-run and a review

- Date: 2026-11-23.
- Quote: "Do whatever you need to do. Scripts and changes should be ran through a dry-run and reviewed before properly executing."
- Decided: the rename script is dry-run and reviewed before it is applied.
- Recorded in: CLAUDE.md "Workflow" (Mode 2); `.claude/docs/workflow.md`.

## d-011 Project docs follow the global docs standard

- Date: 2026-11-23.
- Quote: "follow the global docs"; and for this cleanup, "documentation clean up and standardization, following the global requirements of index files and reference files including a decisions file".
- Decided: Markdown docs and their sidecar folders are lowercase-hyphenated; docs are lean indexes plus sidecars; this file exists.
- Recorded in: CLAUDE.md "Docs (index)"; `~/.claude/docs/documentation_standards.md`.

## d-012 The tools location is each project's call

- Date: 2026-11-23.
- Quote: "W6 shouldn't be a hook - it should be up to the project to define it - but global should have a recommendation".
- Decided: no hook enforces where workflow tools live; this project uses `.claude/tools/`, with one-off scripts in `.claude/tools/scripts/`.
- Recorded in: CLAUDE.md "Workflow".

## d-013 W7 is a global hook

- Date: 2026-11-23.
- Quote: "W7 - this should be a global hook".
- Decided: the file-and-folder naming check is a global hook (W7), following the global naming rule.
- Recorded in: `~/.claude/CLAUDE.md` "File and folder names"; the global hook.

## d-014 Git follows the global rules

- Date: 2026-11-23.
- Quote: "Remove the git restriction from the project and fall back to the global rules".
- Decided: the project's "agents never run git" rule is gone; the global git rules apply.
- Recorded in: CLAUDE.md "Workflow"; `~/.claude/CLAUDE.md` "Safety".

## d-015 Original-Helbreath is the Helbreath source of truth

- Date: 2026-11-23.
- Quote: "Everything in there can answer just about anything regarding Helbreath".
- Decided: the Original-Helbreath GitHub organization is the source of truth for original game behaviour, protocol, data and formulas.
- Recorded in: CLAUDE.md "Helbreath source of truth"; `sources/README.md`.

## d-016 CLAUDE.md stays lean

- Date: 2026-11-23.
- Quote: "CLAUDE.md should not be 20kb".
- Decided: CLAUDE.md keeps each binding rule as a short statement plus a pointer; detail lives in `.claude/docs/`.
- Recorded in: CLAUDE.md "Docs (index)".

## d-017 Operator-state tables are preserved across rebuilds

- Date: 2026-11-23.
- Quote (selected answer): "Operator state, preserved".
- Decided: `admins`, `banned_list` and `admin_command_permissions` keep their schema in the SQL directory, and their rows are carried over from the live DB on each rebuild.
- Recorded in: `sources/database/README.md`; `build_databases.py` (`OPERATOR_TABLES`).

## d-018 Build tools may run read-only git

- Date: 2026-11-23.
- Quote (selected answer): "Allow read-only git in build tools".
- Decided: build generators (the Build ID generator) run read-only git during a build.
- Recorded in: CLAUDE.md "Workflow"; `.claude/docs/versioning/build-id.md`.

## d-019 The build proof keeps the clock-skew retry

- Date: 2026-11-23.
- Quote (selected answer): "Add the skew exchange".
- Decided: a login refused only for clock skew gets a clock-skew reply carrying the server window, and the client retries once with that offset.
- Recorded in: `.claude/docs/versioning/build-proof.md`.

## d-020 The game-port gate bypass is closed

- Date: 2026-11-23.
- Quote (selected answer): "Fix it".
- Decided: the game port admits a player only after a versioned enter-game from the same address with the same password.
- Recorded in: `.claude/docs/versioning/build-proof.md`.

## d-021 Phase R naming choices

- Date: 2026-11-23.
- Quotes (selected answers): "Join acronym", "Drop the I", "Defer data", "Project files and CHANGELOG".
- Decided: acronyms stay one word (`sfml_engine`, `ccontrols`); interface headers drop the `I` (`renderer.h`); `staged/` data files are not renamed in Phase R; `Helbreath.sln`, the `.vcxproj`/`.filters` files and `CHANGELOG.md` keep their names.
- Recorded in: `.claude/tools/scripts/rename_to_naming_rule.decisions.json`.

## d-022 Commit to Development only; no push until the user says so

- Date: 2026-11-23.
- Quote: "From now on only push to Development - don't push to origin until I say so".
- Decided: work is committed locally on the `Development` branch only; nothing is pushed to `origin` until the user explicitly says so.
- Recorded in: CLAUDE.md "Workflow".

## d-023 Adopt the ticket system

- Date: 2026-11-23.
- Quote: "I want you to also adopt the ticketing system from @[ticket-master]"; selected answers "HH- / Heldenian", ".claude/tools/tickets", areas "client, server, shared, database, tooling, docs".
- Decided: ticket system 1.5.4 with prefix `HH-`, board "Heldenian Tickets" on port 18550, CLI in `.claude/tools/tickets`.
- Recorded in: `.claude/tickets/config.json`; `.claude/docs/tickets.md`; `.claude/skills/tickets/SKILL.md`.

## d-024 World and screen space are drawn separately

- Date: 2026-11-23.
- Quotes: "UI/floating text and screens that are not maingame should be screenspace always never drawn at world resolution 800x600"; "Should remain uniform however the action bar must always take up the same percentage of screen space as 800x600 would"; selected answers "Native-res TTF", "Overlays on top", "Approve, start phase 1".
- Decided: the world renders at 800x600 and is upscaled; UI, floating text and non-game screens render in screen space with native-resolution text and a uniform UI scale; the HUD action bar spans the world rect; world-anchored overlays draw above late world effects.
- Recorded in: `.claude/design/plans/world-screen-split.md`.

## d-025 Work is tracked in small tickets, as tasks and sub-tasks

- Date: 2026-11-23.
- Quotes: "Make sure we are filing tickets as we work, as the tickets are how we keep track of our work. Tickets should be small and multiple tickets for larger work"; "Broken into tasks/sub-tasks".
- Decided: every piece of work gets a ticket when it starts; larger work is a task ticket whose small sub-task tickets block it.
- Added the same day: "Only put tickets in progress if an agent is actively working on them. A master ticket can remain in in-progress as long as agents are actively working on sub tickets. But only put sub tickets in progress if an agent is working on them". A sub-task is `in_progress` only while an agent works it; otherwise it stays `open`.
- Added the same day: "use comments in the tickets where deemed necessary such as work pausing or holding". Pauses, holds, resumes and hand-overs get a ticket comment.
- Added the same day: "I also cannot interact with the ticket board, only view. So all questions or issues should come to me". Clarified: "Well issues that request me". Questions and issues that need the user go to them in chat; their answers are recorded on the ticket. Other issues are only tracked on the board.

## d-026 Docs linked from CLAUDE.md stay under 10 KB

- Date: 2026-11-23.
- Quote: "the goal is to keep documentation files under 10kb when linked to the CLAUDE.md - if it gets bigger than that it needs to be audited and checked to see if it needs to have an index system to keep it lean".
- Decided: a linked doc over 10 KB gets an audit ticket and is split into an index plus sidecars, or kept whole with a dated reason.
- Recorded in: `.claude/docs/documentation.md`; CLAUDE.md "Docs (index)".
- Recorded in: CLAUDE.md "Workflow"; `.claude/docs/ticket-workflow.md`; `.claude/docs/workflow.md`; `.claude/docs/agents.md`.

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
