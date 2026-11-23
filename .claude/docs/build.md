# Build, deploy and runtime layout

*Assessed 2026-11-23 (docs standard sect. 1): kept whole, about 6 KB. One continuous reference for running a build and knowing where its output goes; the sections cross-refer and a reader wants the commands and the layout together. Moved out of CLAUDE.md 2026-11-23. The binding rules stay stated in CLAUDE.md "Build and deploy".*

Contents: Windows (MSBuild) - Linux server - Linux client - Layout and deploy.

## Windows (Visual Studio / MSBuild)

```powershell
powershell -ExecutionPolicy Bypass -File sources\build.ps1 -Target All -Config Debug
powershell -ExecutionPolicy Bypass -File sources\build.ps1 -Target Game -Config Debug
powershell -ExecutionPolicy Bypass -File sources\build.ps1 -Target Server -Config Debug
powershell -ExecutionPolicy Bypass -File sources\build.ps1 -Target All -Config Debug -DeployDryRun  # build, preview deploy
powershell -ExecutionPolicy Bypass -File sources\build.ps1 -Target All -Config Debug -NoDeploy      # build only
```
- After a successful build, `build.ps1` deploys into `game/` (`-NoDeploy` skips it, `-DeployDryRun` only previews it). `build_release.ps1` does not deploy unless given `-Deploy` or `-DeployDryRun`.
- Logs: `build\logs\build_<target>.log` (rewritten on each build).
- Output: `bin\client\<Config>\Game_x64_win.exe`, `bin\server\<Config>\Server.exe`; libs in `bin\{sfmlengine,ccontrols,autoupdater}\<Config>\`. Intermediates and generated headers: `build\msbuild\<project>\<Config>\`.
- x64 platform. `Helbreath.sln` at the repo root (projects use `sources\build_tools\layout.props`, never `$(SolutionDir)`). C++20 server and client.
- LNK4099 warnings (missing SFML/freetype PDBs) are cosmetic: ignore.
- SFML 3 libs are bundled in `sources/dependencies/SFML/`. There is no rebuild script in this checkout (`Scripts\build_sfml.bat` is missing).
- Release pair, environments and the generators: `.claude/docs/versioning/implementation.md`.

## Linux (CMake, server)

```bash
./sources/build_server_linux.sh                 # Incremental debug build
./sources/build_server_linux.sh release         # Release build
./sources/build_server_linux.sh clean           # Delete build directory
./sources/build_server_linux.sh clean release   # Clean then rebuild release
./sources/build_server_linux.sh deploy-dry-run  # Build, then only preview the deploy (or: nodeploy)
```
- CMakeLists.txt: `sources/server/CMakeLists.txt`; build dir `build/linux/server/`
- Output: `bin/server/<Debug|Release>/HelbreathServer`
- Auto-reconfigures when switching between Debug and Release.
- Deploy runs after a successful build and assembles `game/server/`.
- Run from: `cd game/server && ./HelbreathServer`

## Linux (CMake, client)

```bash
./sources/build_client_linux.sh                 # Incremental debug build
./sources/build_client_linux.sh release         # Release build
./sources/build_client_linux.sh clean           # Delete build directory
./sources/build_client_linux.sh clean release   # Clean then rebuild release
./sources/build_client_linux.sh deploy-dry-run  # Build, then only preview the deploy (or: nodeploy)
```
- CMakeLists.txt: `sources/client/CMakeLists.txt`; build dir `build/linux/client/`
- Output: `bin/client/<Debug|Release>/Game_x64_linux`
- Auto-reconfigures when switching between Debug and Release.
- SFML 3 is auto-fetched and built via CMake FetchContent (no manual SFML install needed).
- Prerequisites: `sudo apt install cmake g++ libx11-dev libxrandr-dev libxcursor-dev libgl1-mesa-dev libudev-dev libfreetype-dev`
- Deploy runs after a successful build and assembles `game/client/`.
- Run from: `cd game/client && ./Game_x64_linux`
- First-time WSL setup and prerequisites in prose: `sources/readme_build_linux.md`.

## Layout and deploy (user, 2026-11-23)

"game is assembled by the build, staged is the source/reference - when we build we build the necessary bin files - then we deploy. Deploy means actually building the game/*/* directory with the staged files"

- **Build** produces `bin/<project>/<Config>/` (exes, libs; one folder per configuration because Debug and Release libs share a name). All other build artifacts go to repo-root `build/`: `build/msbuild/<project>/<Config>/` (objs, generated headers in `<channel>/generated/`), `build/generated/<channel>/` (release seed), `build/linux/<project>/` (CMake), `build/logs/`. `sources/` holds sources only. Layout definitions: `sources/build_tools/layout.props` and `layout.cmake`.
- **Deploy** (`sources/build_tools/deploy.py`) assembles `game/client/` and `game/server/` from `bin/`, `staged/` and the DBs generated from `sources/database/` (DB hook disabled until Phase E). Run by the build scripts; by hand: `python sources/build_tools/deploy.py --target client|server|all --config Debug [--dry-run]`. It copies only changed files, never writes or deletes protected runtime state, removes only files its manifest (`game/<target>/.deploy/manifest.json`) says it deployed, and backs up anything it replaces that it did not write. Always `--dry-run` first after changing it.
- **`staged/`** is the tracked source of truth for static files: `staged/client/` (contents, fonts, music, sounds, sprites), `staged/server/`, and `staged/shared/` (mapdata used by both). Edit static files there, never in `game/`.
- **`game/`, `bin/`, `build/`** are gitignored output. `game/` also holds runtime state that deploy must never wipe: account DBs (with password hashes), logs, and config files the programs generate with defaults at runtime and keep between runs (client `settings.json` etc., server `server_config.json`, `gameconfigs/`).
- **DBs** in `game/server/` are generated from `sources/database/`. `staged/` holds no DBs and no generated configs.
- Why it is this way: `.claude/docs/decisions.md` (build, bin, staged, game layout).
