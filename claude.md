# Helbreath 3.82 - Project Guide

## Build

### Windows (Visual Studio / MSBuild)
```powershell
powershell -ExecutionPolicy Bypass -File Sources\build.ps1 -Target All -Config Debug
powershell -ExecutionPolicy Bypass -File Sources\build.ps1 -Target Game -Config Debug
powershell -ExecutionPolicy Bypass -File Sources\build.ps1 -Target Server -Config Debug
```
- Delete `build_*.log` before building for clean logs.
- Output: `Sources\Debug\Game_SFML_x64.exe` (client), `Sources\Debug\Server.exe` (server).
- x64 platform. `Sources\Helbreath.sln`. C++20 server and client.
- LNK4099 warnings (missing SFML/freetype PDBs) are cosmetic — ignore.
- SFML 3 libs are bundled in `Sources/Dependencies/SFML/`. To rebuild from source: `Scripts\build_sfml.bat`

### Linux (CMake — Server)
```bash
./Sources/build_server_linux.sh                 # Incremental debug build
./Sources/build_server_linux.sh release         # Release build
./Sources/build_server_linux.sh clean           # Delete build directory
./Sources/build_server_linux.sh clean release   # Clean then rebuild release
```
- CMakeLists.txt: `Sources/Server/CMakeLists.txt`
- Output: `Sources/Debug/HelbreathServer` or `Sources/Release/HelbreathServer` (mirrors Windows layout)
- Auto-reconfigures when switching between Debug and Release
- Deploy: `cp Sources/Debug/HelbreathServer Binaries/Server/ && chmod +x Binaries/Server/HelbreathServer`
- Run from: `cd Binaries/Server && ./HelbreathServer`

### Linux (CMake — Client)
```bash
./Sources/build_client_linux.sh                 # Incremental debug build
./Sources/build_client_linux.sh release         # Release build
./Sources/build_client_linux.sh clean           # Delete build directory
./Sources/build_client_linux.sh clean release   # Clean then rebuild release
```
- CMakeLists.txt: `Sources/Client/CMakeLists.txt`
- Output: `Sources/Debug/HelbreathClient` or `Sources/Release/HelbreathClient` (mirrors Windows layout)
- Auto-reconfigures when switching between Debug and Release
- SFML 3 is auto-fetched and built via CMake FetchContent (no manual SFML install needed)
- Prerequisites: `sudo apt install cmake g++ libx11-dev libxrandr-dev libxcursor-dev libgl1-mesa-dev libudev-dev libfreetype-dev`
- Deploy: `cp Sources/Debug/HelbreathClient Binaries/Game/ && chmod +x Binaries/Game/HelbreathClient`
- Run from: `cd Binaries/Game && ./HelbreathClient`

## Workflow

**Never use git commands for version control. The user handles all git operations. Backups use `.bak_<guid>` files via `bak.py`.**

**Exception:** `git diff` is allowed (and encouraged) as a **read-only verification tool** after refactoring. Use it to confirm no logic was accidentally lost or altered. This is verification, not version control.

For complex or multi-step tasks, maintain a session context file (`session_context.md` in the memory directory) with: current task, key decisions, files being worked on, and constraints. This survives context compaction. Delete it when the task is complete.

Two modes. **Default to Mode 1.** Use Mode 2 only when justified.

### Mode 1: Direct Edit (DEFAULT — 1-9 files)

```
bak.py guard <files>  →  Read/Edit tools  →  Build  →  bak.py commit (or revert)
```

1. **Guard** — `python bak.py guard <file1> [file2 ...]` — creates versioned checkpoint (.bak_<guid>).
2. **Edit** — Read and Edit tools.
3. **Simplify** — Run `/simplify` (a built-in skill that reviews changed code for reuse opportunities, quality issues, and efficiency problems, then fixes what it finds).
4. **Verify** — For refactors, run `git diff` on changed files to confirm no logic was lost or altered.
5. **Build** — `powershell -ExecutionPolicy Bypass -File Sources/build.ps1 -Target All -Config Debug`
6. **If build succeeds** — `python bak.py commit` — deletes all .bak files, accepts changes.
7. **If build fails** — choose:
   - `guard` again to checkpoint, then fix and rebuild (layer the fix).
   - `revert <id>` to undo a specific checkpoint, retry from previous.
   - `revert <id> <file1> [file2 ...]` to revert specific files from a checkpoint.

Rules:
- If the change is specific edits to specific lines → Mode 1.
- One logical change per guard-edit-build-commit cycle.
- Each `guard` creates a new checkpoint — layer fixes and peel back selectively.

### Mode 2: Python Script (BULK — 10+ files)

Must justify: "This touches N files with pattern X, script appropriate because Y."
See `CLAUDE_WORKFLOW.md` for script pattern, regex safety rules, and verification standards.

**Required verification before applying:**
1. `--dry-run` — preview all changes (full detail log to `Scripts/output/`).
2. `--verify` — scan for Shared/SFMLEngine collisions, C++ keyword conflicts, duplicate targets.
3. Only then run without flags to apply. See `PLANS/BulkScript_DryRun_Standards.md` for full spec.

Run `python bak.py --help` for full command reference (guard, status, revert, commit).

## Code Search

`grep.py` — two modes: brief (`-b`) for quick stdout, detailed for full-context log file.

```
python grep.py "pattern" -b                       # brief: one line per match to stdout
python grep.py "pattern"                          # detailed: context blocks to log file
python grep.py "pattern" -F --path Sources/Client # fixed string, scoped to directory
python grep.py "pattern" -C 8 -i -o results.log  # 8 context lines, case-insensitive
```

Brief (`-b`): prints `file:line | match` to stdout. Detailed (default): writes to `Scripts/output/grep_results.log` with context, enclosing scope, `>>>` markers. Output dir auto-clears at 10MB.

## Project Structure

- **Client** (`Sources/Client/`) — Game client, C++20. Cross-platform (Windows + Linux). Depends on SFMLEngine and CControls.
- **CControls** (`Sources/CControls/`) — UI control library, C++20. Pure logic, no game/engine dependencies. Static library (`cc::` namespace). See `PLANS/CControls_Library_Plan.md` for full design.
- **SFMLEngine** (`Sources/SFMLEngine/`) — Rendering abstraction over SFML. Cross-platform (Windows + Linux).
- **Server** (`Sources/Server/`) — Game server, C++20. Cross-platform (Windows + Linux). Standalone.
- **Shared** (`Sources/Dependencies/Shared/`) — Protocol, enums, items, packets, networking.
- `Binaries/Game/` — Client runtime (configs, sprites, sounds, maps, fonts).
- `Binaries/Server/` — Server runtime (SQLite DBs, accounts).

See `CLAUDE_ARCHITECTURE.md` for detailed client/server/shared architecture.

## Cross-Platform Rules

Both the client and server build and run on Windows and Linux. **All new code (client, server, shared, SFMLEngine) must be cross-platform:**
- **No Windows API calls** (`DeleteFile`, `wsprintf`, `GetCursorPos`, `POINT`, etc.) — use C++ standard library equivalents.
- **No MSVC-specific types/keywords** (`__int64` → `int64_t`, `__fastcall` → remove, `_TRUNCATE` → don't use).
- **No `#ifdef _WIN32` guarding standard headers** (e.g. `<filesystem>` must be included unconditionally).
- **Use `StringCompat.h`** for string function portability (`strtok_s` → `strtok_r`, `_stricmp` → `strcasecmp`).
- **All headers must include what they use** — GCC is stricter than MSVC (e.g. `<cstddef>` for `size_t`).
- **Case-sensitive filenames** — `#include` must match the exact case of the file on disk.
- **Linux filesystem is case-sensitive** — `Binaries/Server/mapdata/` must be lowercase.
- **Prefer `std::string`/`std::string_view`** over raw `char*`/`char[]` — avoids buffer overruns that silently work on MSVC but fail on GCC.

## Versioning

Three-track system managed from `Sources/version.cfg`. See `VERSION_STANDARDS.md` for full reference (version format, bump rules, generated files, platform details).

- **Compatibility** — Protocol version. Must match between client and server (major.minor.patch). Key test: **does the client need a code change to handle this?** If yes, bump. If the server is just sending an existing message more/less frequently, that's server-only — no compatibility bump.
- **Client** — Client identity. Displayed in window title and in-game overlay. Bump for client-only changes.
- **Server** — Server identity. Displayed in console banner and logs. Bump for server-only changes.
- Pre-build script `Sources/version_gen.py` generates `version_info.h`, `version_rc.h`, and `version.cmake` automatically.
- Edit `Sources/version.cfg` to change versions. Never edit generated files.
- **Build counters** (`build_counter_client.txt`, `build_counter_server.txt`) are per-project and incremented automatically by the build system. **Never pass `--increment-version` manually** — your builds are for compile verification only.

## UI Controls (CControls Library)

**All menu screen UI (buttons, textboxes, focus, tooltips) must use CControls** (`#include "CControls.h"`, namespace `cc::`). Do NOT write manual `is_mouse_in_rect()` hit-testing, manual `m_cur_focus` tracking, or manual per-control rendering loops.

Pattern for each screen:
- `cc::control_collection m_controls;` member in the screen class
- `on_initialize()`: create controls via `m_controls.add<cc::button>(...)`, set render handlers, callbacks, focus order, click sound
- `on_update()`: fill `cc::input_state` from engine input, call `m_controls.update(input, time_ms)`
- `on_render()`: draw background, call `m_controls.render()`, draw overlays

Key types: `cc::button`, `cc::textbox`, `cc::label`, `cc::toggle_button`, `cc::panel` (grouping), `cc::control_collection` (screen-level owner).

See `CCONTROLS_REFERENCE.md` for full API reference, render handler patterns, and usage examples.

## Modernization Direction

- Legacy C-style → C++20 while preserving game logic.
- Goal: OOP, polymorphism, templates, clearly scoped systems.
- Modernize code you touch. Don't gold-plate untouched code.

## Coding Standards

**Convention: `snake_case` throughout.** See `CODING_STANDARDS.md` for full reference (naming, formatting, ownership, error handling, enum patterns).

Key rules:
- **Tabs** for indentation, **Allman** braces.
- **Everything `snake_case`**: classes, methods, members, params, constants, enum values.
- **No prefixes**: `weather_manager` not `CWeatherManager`, `renderer` not `IRenderer`.
- **Members**: `m_snake_case`. Struct data members omit `m_`.
- **Constants**: `constexpr snake_case`. No new `#define` constants.
- **Enums**: Namespace-wrapped unscoped enum (`attack_type::normal`). `enum class` only when implicit conversion undesirable.
- **Namespaces**: `hb::<shared|client|server>::snake_case`.
- **Ownership**: `std::unique_ptr` only. Raw pointers non-owning. Prefer `&` references.
- **Error handling**: Exceptions for critical failures. `bool` for recoverable. No `goto`.
- **Null**: `nullptr` only. **Headers**: `#pragma once`. No `using namespace` in headers.
- **Wire protocol structs**: Exception — Hungarian without `m_` to match binary format.
- Legacy code retains old conventions until actively refactored. Do not reformat untouched code.

After every `bak.py commit`, update `CHANGELOG.md` with a brief summary. When the user requests a Discord post, write a player-facing changelog instead. See `CLAUDE_CHANGELOG.md` for both formats.

## Logging

`#include "Log.h"` → `hb::logger::error`, `warn`, `log`, `debug`. Channel logging with `hb::logger::log<log_channel::security>(...)`. Server has 15 channels, client has 2. **Client and Server only** — Shared/ must not use the logging system; use `std::printf` or standard I/O there. See `CLAUDE_LOGGING.md` for full reference (files, code examples, channel list, initialization).

## Agent Teams

When teams are enabled, follow the structured coordinator/wing model. See `CLAUDE_AGENTS.md` for full roles, communication rules, and workflow phases.

## Testing

No automated tests. Manual: run server, then client with configs in `Binaries/`.
For network changes, rebuild both client and server.
Both targets can be tested on Linux — server with `build_server_linux.sh`, client with `build_client_linux.sh`.
