# decisions: Build layout, deploy and generated files

*Sidecar of `.claude/docs/decisions.md` (index, scope rule and entry format live there). Each entry is a settled user ruling with the user's exact quote.*

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
