# versioning (index)

*Replaces `version-standards.md` (the `version.cfg` / `version_gen.py` system, retired 2026-11-23). Adapted from
Helbreath-SDL's versioning docs; the MySQL parts are dropped because this project is SQLite in one process.
The binding rules are restated in CLAUDE.md "Versioning". Each sidecar says what it could not verify.*

- `versioning/standards.md` - the version format, the three components, the core rules, which environments exist; read first, and before changing anything about how versions are shown or assigned.
- `versioning/release-versioning.md` - when Major, Minor, Patch, alpha, beta and `protocol_version` move, and who decides; read before bumping a number or being asked to.
- `versioning/build-id.md` - how the `High+Low` Build ID is derived (clean and dirty) and decoded; read before touching `gen_build_id.py` or answering "which commit is this build".
- `versioning/environments.md` - DEV, TEST and PROD source rules, the required checks, and the Linux limits; read before a TEST or PROD build, or when a build refuses with a gate message.
- `versioning/build-proof.md` - the login gate: `protocol_version`, the HMAC build proof, the release seed, the clock-skew window exchange, and the notice-only check at game entry; read before changing login, packets that carry the token, or `version.h` proof constants.
- `versioning/implementation.md` - the moving parts: `version.h`/`version.cpp`, the generators, `versioning.cmake`, `layout.props`, `build.ps1`, `build_release.ps1`, `deploy.py`, generated file locations; read before changing the build or when a version/Build ID looks wrong.
- `versioning/version-display.md` - where each surface shows which component's version, and the exact text; read before adding or changing a place that prints a version.
- `versioning/sql-source.md` - the `sources/database/` source-of-truth directory (per-table `.sql` + `.data.sql`), extract and build scripts, operator state, the overwrite guard, the `sql` component; read before editing a table, a seed row, or running either script. Points at `sources/database/README.md` for the details.
