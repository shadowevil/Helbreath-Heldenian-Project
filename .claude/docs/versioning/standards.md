# Versioning standards

Read first. Facts verified against `sources/dependencies/shared/version.h` and `sources/build_tools/`
(2026-11-23).

## Format

```text
Pre-release:        Major.Minor.Patch-(alpha|beta):<CHANNEL>.<High>+<Low>
Stable DEV / TEST:  Major.Minor.Patch:<CHANNEL>.<High>+<Low>
Production:         Major.Minor.Patch:<High>+<Low>        (PROD drops the channel label)
```

Examples: `0.2.52-alpha:DEV.41279+37580`, `1.5.0:TEST.41279+37580`, `1.5.0:41279+37580`.
Progression: `0.0.1-alpha -> 0.x.x-alpha -> 0.x.x-beta -> 1.0.0`.

## Three components, one Build ID

There is no project version. **client**, **server** and **sql** each carry a hand-assigned release,
declared on three lines in `sources/dependencies/shared/version.h` (`hb::shared::version`):

```cpp
inline constexpr release client_release{0, 2, 52, maturity::alpha};
inline constexpr release server_release{0, 1, 24, maturity::alpha};
inline constexpr release sql_release{0, 0, 1, maturity::alpha};
```

(values as of 2026-11-23, carried over from the old `version.cfg`; sql is new.) The login server runs
inside the server exe, so `server` covers both. Moving one release never moves another. `gen_version_info.py`
and both `CMakeLists.txt` parse these lines by regex, so keep the `inline constexpr release <name>{M, m, p, maturity::<x>};`
shape.

The **Build ID is shared**: one build, one source state, so all three components report the same
`<High>+<Low>`. See `build-id.md`.

`protocol_version` (a `uint16_t`, currently `1`) is a wire value, not a release. It gates login
(`build-proof.md`).

## Core rules

1. `Major.Minor.Patch` is assigned by hand, per component, and never automatically.
2. The Build ID is generated at build time and never written into tracked source.
3. DEV may build from a dirty working tree.
4. TEST and PROD need a clean tree, the `Release` configuration and `--fresh-source`; the generator
   refuses otherwise (`environments.md`).
5. Environment rules must not leak into one another: each channel has its own generated-header folder and
   its own release seed.
6. PROD omits its label from the displayed version.

> Release versions describe the significance of a release, per component. The Build ID identifies the source
> state that produced them all.

## Environments here

DEV is the only environment this project builds in day to day. TEST and PROD are implemented and
enforced by the generator, and build scripts accept `-Environment TEST|PROD`, but whether a TEST/PROD
environment (staging, publish target, clean build machine) is stood up is **unverified**: nothing in the
repo says either way. Treat them as rules that exist so they cannot drift into DEV.

## Differences from SDL

- SQLite, one process: no `hb_schema` link hash, no `HB_DB_USER`, no agent-bridge ping, no Schemata model
  fingerprint. The DB side is `sql-source.md`.
- The build proof keeps SDL's clock-skew window exchange (user ruling 2026-11-23).
- Binding mid-branch rule: see `release-versioning.md` for `protocol_version`.
