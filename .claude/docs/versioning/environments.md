# Environments: DEV, TEST, PROD

Verified against `gen_build_id.py` `check_gates()`/`derive_build_id()` and `build.ps1`/`build_release.ps1`
(2026-11-23). Whether TEST or PROD is actually used is unverified (`standards.md`).

| | DEV | TEST | PROD |
|---|---|---|---|
| Displayed | `M.m.p-x:DEV.H+L` | `M.m.p-x:TEST.H+L` | `M.m.p-x:H+L` |
| Dirty tree | allowed (fingerprint Build ID) | refused | refused |
| Configuration | any | `Release` only | `Release` only |
| `--fresh-source` | not needed | required | required |
| Build ID | HEAD, or fingerprint when dirty | HEAD | HEAD |

`--fresh-source` is the operator's assertion that the source was freshly obtained from Git. Nothing on disk
can prove it, so without the assertion the build stops. It is the one required check that is not mechanical.

## What the generator checks for TEST and PROD

1. `--fresh-source` was passed.
2. Configuration is exactly `Release` (`ReleaseWithDebugInfo` is refused for TEST/PROD).
3. The whole-tree check: any `git status` entry anywhere in the repo (gitignored files never appear)
   stops the build, except exactly `KNOWN_TRACKED_ARTIFACTS` (six tracked `.lib` files, until the user
   untracks them in Phase H).
4. Source unmodified: any tracked change under `sources/` (except those artifacts), or an untracked entry
   under `sources/` that is not a build product, stops the build. Stricter than DEV's relevance filter,
   and not skipped by `--clean-verified`.
5. With `--clean-verified <sha>`: the sha must equal the full `HEAD` at compile time.

`build.ps1` runs the whole-tree check once with `--check-only` before MSBuild writes anything (MSBuild
modifies tracked files), then passes `--clean-verified <sha>` to each project so the check is not repeated
against the build's own output. A bare MSBuild or CMake run does the check itself.

If a check fails the build stops. After fixing a defect found in TEST: commit, fresh checkout, build again;
the new Build ID comes from the new HEAD. The release version need not change.

## Promotion

Ideally TEST and PROD build the exact same commit: only the label changes, the Build ID is identical.
(`1.5.0:TEST.41279+37580` becomes `1.5.0:41279+37580`.) Each channel has its own release seed, so the
two binaries still do not talk to each other's servers (`build-proof.md`).

## Linux

`sources/build_tools/versioning.cmake` does the same work for CMake. `HB_ENVIRONMENT` and `HB_FRESH_SOURCE`
are read once per configure and removed from the cache, so a TEST/PROD configure never carries into the next
one; a re-configure without them is DEV (a warning is printed). The `build_*_linux.sh` scripts do not do
TEST/PROD yet (stated in `versioning.cmake`); use `cmake -S ... -DHB_ENVIRONMENT=TEST -DHB_FRESH_SOURCE=ON`
by hand. Linux builds were not run for this doc: **unverified**.
