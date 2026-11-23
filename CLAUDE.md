# Helbreath 3.82 - Project Guide

## Docs (index)

Docs: `.claude/docs/` (standard: `~/.claude/docs/documentation_standards.md`). **Linked docs stay under 10 KB, else audit for an index split** (d-026; `documentation.md`, read before growing a doc). Plans: `.claude/design/plans/`. Scratch: `.claude/scratch/` (gitignored).
- `build.md` - build commands, outputs, deploy, `bin/` `build/` `staged/` `game/` layout; read before building or deploying.
- `tickets.md` - ticket system rules and CLI reference (release text); read for field or CLI detail.
- `workflow.md` - Mode 1 cycle detail, Mode 2 pattern, regex safety; read before a guarded edit sequence or bulk script.
- `bulk-scripts-dry-run.md` - `--dry-run`/`--verify` spec; read when writing a Mode 2 script.
- `code-search.md` - `grep.py` usage; read before using it.
- `architecture.md` - project structure, client/server/shared design; read before working across subsystems.
- `coding-standards.md` - index to full style rules (formatting, naming, language use); read when a rule below is not specific enough.
- `ccontrols-reference.md` - index to the CControls API (core, text input, layout); read before building or changing a menu screen.
- `versioning.md` - versioning index; read before any version, Build ID, login-gate or `sources/database/` work.
- `logging.md` - log files and channels; read before adding logging.
- `changelog-format.md` - internal and Discord formats; read before writing `CHANGELOG.md`.
- `agents.md` - agent-team roles and phases; read before running a team.
- `ticket-workflow.md` - filing, task/sub-task structure, statuses with agents, the viewer; read before filing or moving a ticket.
- `decisions.md` - the user's settled rulings with quotes; read before changing what a ruling covers.

(All under `.claude/docs/`.)

**Helbreath source of truth** (user, 2026-11-23): the Original-Helbreath GitHub organization (<https://github.com/orgs/Original-Helbreath/repositories>) is the source of truth for all of Helbreath. Consult it before guessing at original game behaviour, protocol, data or formulas.

## Build and deploy

Commands and layout: `.claude/docs/build.md`. Windows: `powershell -ExecutionPolicy Bypass -File sources\build.ps1 -Target All -Config Debug` (`-NoDeploy` = compile check). Linux: `sources/build_{server,client}_linux.sh`.
- **Build** writes `bin/<project>/<Config>/`; every other artifact goes to repo-root `build/`. `sources/` holds sources only.
- **Deploy** assembles `game/client/` and `game/server/` from `bin/`, `staged/` and DBs generated from `sources/database/`; `--dry-run` first after changing it.
- **`staged/` is the tracked source of truth for static files; edit them there, never in `game/`.** `game/`, `bin/` and `build/` are gitignored output.
- **Never wipe runtime state in `game/`:** account DBs (password hashes), logs, and generated configs (client `settings.json`, server `server_config.json`, `gameconfigs/`). Deploy must not overwrite them.

## Workflow

- **Git follows the global rules** (user, 2026-11-23): local commits and working-tree git (`git mv`, `git rm`, `git add`) are allowed; commit on `Development` as needed, never push to `origin` until the user says so (d-022); PRs only when asked; never pull, `reset --hard`, force, or rewrite history. Use `git diff` after refactoring to confirm no logic was lost. The Build ID generator runs read-only git (`git rev-parse`, `git status`) during a build.
- **Workflow tools** live in `.claude/tools/` (`bak.py`, `grep.py`), one-off scripts in `.claude/tools/scripts/`. Product tooling stays with its code: `tools/`, `sources/build_tools/`, `sources/database/`.
- **Tickets track all work** (`HH-`, user, 2026-11-23; d-025): file as you work; small tickets, larger work as a task with blocking sub-tasks; `in_progress` only while an agent works that ticket. Practice: `.claude/docs/ticket-workflow.md`. CLI `python .claude/tools/tickets/tickets.py` (`author.py` to file), viewer on 18550 kept open in the in-app browser, skill `tickets`.
- **Default to Mode 1** (1-9 files): `bak.py guard <files>` (only files under `sources/`), edit with Read/Edit, `/simplify`, build, a separate read-only review agent (`ADVISOR: FORBIDDEN`) with the diff and the goal, then `bak.py commit <id>` (or `revert`). One logical change per cycle. Details: `.claude/docs/workflow.md`.
- **Mode 2** (script, 10+ files) only when justified ("This touches N files with pattern X, script appropriate because Y"). Required before applying: `--dry-run`, then `--verify`, then apply. Details: `.claude/docs/workflow.md`, `.claude/docs/bulk-scripts-dry-run.md`.
- After every `bak.py commit`, update `CHANGELOG.md`. A Discord post is a player-facing changelog on request.
- Multi-step tasks keep `session_context.md` in the memory directory; delete it when done.
- Search C++ with `python .claude/tools/grep.py` (`.claude/docs/code-search.md`); anything else with the Grep tool.

## Cross-Platform Rules

Client and server build and run on Windows and Linux. **All new code (client, server, shared, sfml_engine) must be cross-platform:**
- **No Windows API calls** (`DeleteFile`, `wsprintf`, `GetCursorPos`, `POINT`, etc.): use the C++ standard library.
- **No MSVC-specific types/keywords** (`__int64` -> `int64_t`, `__fastcall`, `_TRUNCATE`).
- **No `#ifdef _WIN32` guarding standard headers** (`<filesystem>` is included unconditionally).
- **Use `string_compat.h`** (`strtok_s` -> `strtok_r`, `_stricmp` -> `strcasecmp`).
- **Headers include what they use** (GCC is stricter: `<cstddef>` for `size_t`).
- **Case-sensitive filenames:** `#include` matches the file on disk; `staged/shared/mapdata/` must be lowercase.
- **Prefer `std::string`/`std::string_view`** over raw `char*`/`char[]`.

## Versioning

SDL's standard; index and sidecars `.claude/docs/versioning.md` (read before any version, Build ID, login-gate or `sources/database/` work). Format `Major.Minor.Patch[-alpha|-beta]:<CHANNEL>.<High>+<Low>`; PROD drops the channel label.
- **Three hand-assigned releases:** `client_release`, `server_release` (login server included), `sql_release`, one line each in `sources/dependencies/shared/version.h`. Moving one never moves another. **Never bump a release without the user's explicit decision**, nor as a side effect.
- **`protocol_version`** (same file) must match for login. Bump only when the client needs a code change to talk to the server; a server that only sends an existing message more or less often needs no bump. Rebuild both sides after a bump.
- **The Build ID is generated, never edited or committed** (`sources/build_tools/gen_build_id.py`); nor are the generated headers (`*_generated.h`, `version_info.rc`). The old INI system and build counters are retired.
- **Environments never leak:** DEV is the default. TEST and PROD need `-Config Release`, a clean tree and `-FreshSource`; the generator refuses otherwise. Do not work around a refusal.
- **Release builds:** build the pair with `sources\build_release.ps1`, the only path that rolls the release seed. A Release server admits only a client with the same Build ID and a valid build proof; Debug admits all. Never print, log or commit the seed.
- **Databases:** `sources/database/` is the source of truth. Extract an edited deployed `.db` back (`extract_databases.py`) before the next `build_databases.py`, which refuses to overwrite unextracted edits. Stop the server before building databases. Never put player data in it.
- Your builds are for compile verification only; they increment nothing.

## UI Controls (CControls)

**All menu screen UI (buttons, textboxes, focus, tooltips) must use CControls** (`#include "ccontrols.h"`, namespace `cc::`). Do NOT write manual `is_mouse_in_rect()` hit-testing, manual `m_cur_focus` tracking, or manual per-control rendering loops. One `cc::control_collection m_controls;` per screen: create controls in `on_initialize()`, `m_controls.update(input, time_ms)` in `on_update()`, `m_controls.render()` in `on_render()`. Pattern and API: `.claude/docs/ccontrols-reference.md`.

## Coding Standards

**`snake_case` throughout** (full rules: `.claude/docs/coding-standards.md`). Modernize legacy C-style to C++20 in code you touch; don't gold-plate or reformat untouched code.
- **Tabs**, **Allman** braces. **No prefixes** (`weather_manager`, not `CWeatherManager`; `renderer`, not `IRenderer`).
- **Members** `m_snake_case`; struct data members omit `m_`. **Constants** `constexpr snake_case`, no new `#define` constants.
- **Enums:** namespace-wrapped unscoped (`attack_type::normal`); `enum class` only when implicit conversion is undesirable.
- **Namespaces** `hb::<shared|client|server>::snake_case`. **Headers** `#pragma once`, no `using namespace`. **Null** is `nullptr` only.
- **Ownership:** `std::unique_ptr` only; raw pointers non-owning; prefer `&`. **Errors:** exceptions for critical failures, `bool` for recoverable, no `goto`.
- **Wire protocol structs** are the exception: Hungarian without `m_`, to match the binary format.
- **File and folder names follow the global naming rule** (`~/.claude/CLAUDE.md`): C++ and Python files and folders `snake_case`, Markdown docs lowercase-hyphenated.

## Logging

`#include "log.h"` -> `hb::logger::error`, `warn`, `log`, `debug`; channels via `hb::logger::log<log_channel::security>(...)`. **Client and Server only:** `shared/` must not use the logging system (use `std::printf` or standard I/O).

## Agent Teams

Follow the coordinator/wing model (`.claude/docs/agents.md`). **The Coordinator is always the main session; only it spawns agents** (user, 2026-11-23). Briefs: `.claude/agents/` (`researcher`, `editor`, `reviewer`).

## Testing

No automated tests. Manual: deploy, run the server from `game/server/` and the client from `game/client/`; rebuild both for network changes.
