# Build ID

Verified against `sources/build_tools/gen_build_id.py` (2026-11-23).

The Build ID identifies the source state a build was made from. It is not a counter. Display form
`<High>+<Low>`, each 0-65535.

## Clean tree

First 8 hex characters of `HEAD` -> `uint32` -> `High = value >> 16`, `Low = value & 0xFFFF`.

Decode: `value = High * 65536 + Low`, print as 8 hex digits, and `git show <prefix>`. The full sha stays
authoritative (`hb::shared::version::commit_sha()`).

## Dirty DEV tree

SHA-256 over `HEAD` plus, for every relevant changed or untracked file, its status letter, path, original
path (renames) and git blob id (`git hash-object`, which applies the `.gitattributes` line-ending
normalisation, so Windows and Linux agree). First 8 hex characters, split the same way. Paths are sorted
ordinally.

- Same state gives the same id; changing the state changes it; reverting exactly restores the old id.
- Reproducible, not reversible: it cannot be turned back into the uncommitted files.
- **"Relevant" means under `sources/`**, minus build products: `sources/Debug|Release|ReleaseDbg/`,
  per-project `*_x64/` folders, any `/build/` or `/build_linux/` folder, `.bak_*` checkpoints, `*.log`.
  Editing docs, `staged/`, or `.claude/` does not change the Build ID. This scope applies to DEV only.
  The exact list is `IGNORED_PATTERNS` in the script; read it there.

Because `bak.py guard` creates `.bak_*` files, guarding or committing a checkpoint does not change the id.

## Injection

`gen_build_id.py` writes `build_id_generated.h` (channel id and label, High, Low, `source_clean`, `commit_sha`)
into an untracked build folder before every compile. Only `shared/version.cpp` includes it. It rewrites the
file only when the value changes, so an unchanged state does not recompile. A missing header is a hard build
error on purpose: a binary never ships an invented Build ID. See `implementation.md` for the folder.

## Git use

The generator runs read-only git (`rev-parse`, `status`, `hash-object` without `-w`) with
`--no-optional-locks`. Build generators may run this read-only git during a build (user ruling
2026-11-23); git use by agents follows the global rules.
