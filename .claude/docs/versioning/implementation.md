# How this repo implements versioning

Verified by reading the files named (2026-11-23). Nothing was built or run for this doc: **the commands below
are unverified as executed**; the `build id:` output line comes from `build.ps1`'s log echo.

## Where the two halves live

| Half | Assigned by | Lives in |
|---|---|---|
| `Major.Minor.Patch` + maturity, `protocol_version` | a human | `sources/dependencies/shared/version.h` |
| Channel, Build ID, clean flag, commit | the build | `build_id_generated.h`, untracked, written per project |
| Release seed (Release builds) | `build_release.ps1` | `build/generated/<channel>/release_seed_generated.h` |

Old system (`version.cfg`, `version_gen.py`, `version_info.h`, `version_rc.h`, `version.cmake`, build counters)
is gone. Do not look for it.

## The moving parts

```
sources/build_tools/gen_build_id.py       derive the id, write build_id_generated.h
sources/build_tools/gen_release_seed.py   write the per-channel release seed (roll / ensure)
sources/build_tools/gen_version_info.py   write version_info.rc (VERSIONINFO) from version.h
sources/build_tools/versioning.cmake      the same three steps for the Linux CMake builds
sources/build_tools/layout.props / .cmake build/ and bin/ locations
sources/build_tools/deploy.py             assemble game/ from bin/ and staged/
sources/dependencies/shared/version.h    release lines, accessors, build-proof API
sources/dependencies/shared/version.cpp  builds every displayed string once; the ONLY includer of the generated headers
```

Windows: each of `Client.vcxproj` and `Server.vcxproj` has `GenerateBuildId` and `EnsureReleaseSeed`
(`BeforeTargets=ClCompile`) and `GenerateVersionInfo` (before resource compile); `EnsureReleaseSeed` skips Debug.
`HbEnvironment` defaults to `DEV`; `HbFreshSourceSwitch` and `HbCleanVerifiedSwitch` are appended verbatim to
the generator command. `DisableFastUpToDateCheck` is true because git state is not an MSBuild input. Every
generator writes only when the content changes, so an unchanged state does not recompile.

Generated Windows files: `build/msbuild/<project>/<Config>/<channel>/generated/` (`build_id_generated.h`,
`version_info.rc`), one folder per configuration AND channel so a DEV header cannot feed a TEST build.
Linux: `<CMake binary dir>/generated/<channel>/`. The seed folder is shared by client and server:
`build/generated/<channel>/`. The `gen_build_id.py` docstring still describes an older location
(`sources/<Project>/build/...`); trust the `.vcxproj` and `layout.props`.

A missing generated header is a hard build error on purpose.

## Build commands

`sources/build.ps1` (`-Target Game|Server|All`, `-Config Debug|Release`):

```powershell
powershell -ExecutionPolicy Bypass -File sources\build.ps1 -Target All -Config Debug
powershell -ExecutionPolicy Bypass -File sources\build.ps1 -Target All -Config Release -Environment TEST -FreshSource
powershell -ExecutionPolicy Bypass -File sources\build_release.ps1                          # DEV release pair, one seed
powershell -ExecutionPolicy Bypass -File sources\build_release.ps1 -Environment TEST -FreshSource
```

- `-Environment DEV|TEST|PROD` (default DEV) sets `/p:HbEnvironment`.
- `-FreshSource` becomes `--fresh-source` (the operator's assertion; `environments.md`).
- For TEST/PROD `build.ps1` first runs `gen_build_id.py --check-only` once, parses its `verified head: <sha>`
  line, and passes `--clean-verified <sha>` to every project, so the whole-tree check is
  not repeated against the build's own output. It refuses to start the build when the check fails.
- `build.ps1` echoes the unique `build id:` lines from the log, so a disagreement between projects is visible.
- `-NoDeploy` skips the deploy step; `-DeployDryRun` previews it.
- `build_release.ps1` rolls ONE seed, then runs `build.ps1 -Target All -Config Release`. It is the only path
  that rolls a seed. It does NOT deploy by default (`game/` is the dev game folder); `-Deploy` deploys the pair,
  `-DeployDryRun` previews. A failed build leaves the rolled seed; run again so nothing ships from a half pair.

Do not pass anything like `--increment-version`; there is nothing to increment.

## Deploy

`sources/build_tools/deploy.py --target client|server|all --config Debug|Release|ReleaseWithDebugInfo`
assembles `game/client` and `game/server` from `bin/<project>/<config>/` and `staged/`. It never writes or
deletes protected runtime state (account DBs, every `*.db`, logs, settings, `server_config.json`,
`gameconfigs/`, ...), backs up files it did not put there, and records a manifest in `game/<target>/.deploy/`.
Its docstring lists the full safety rules; read it there. The DB step (`--build-databases`) is behind
`DATABASE_HOOK_ENABLED = False` in `deploy.py`, so deploy currently builds no database (`sql-source.md`).

## Version resources

`gen_version_info.py --component client|server` parses the release from `version.h` and writes a VERSIONINFO
fragment that `game.rc` / `server.rc` include as `version_info.rc` (company, copyright and product strings are
constants in the script). The client uses `client_release`; the server exe uses `server_release`. The updater
(`AutoUpdater`) and installer/updater tools: whether they read `version.h` yet is **unverified** (plan lists
them as phase B/C work).

## Changing a release

Edit the three lines in `version.h` by hand (`release-versioning.md`), under `bak.py guard`. Nothing derives
from them automatically and bumping one does not touch the others. The generators and both `CMakeLists.txt`
read them by regex.

## When a build refuses

- "must come from source freshly obtained" -> TEST/PROD without `--fresh-source`.
- "must use the Release configuration" -> TEST/PROD with Debug.
- "require a clean working tree" / "unmodified source" -> uncommitted changes; commit, fresh checkout.
- compile error `a TEST or PROD build must be a Release (NDEBUG) build` or `release_seed.h holds no seed` or a
  seed channel mismatch -> a `static_assert` in `version.cpp`; roll the seed with `build_release.ps1`.
