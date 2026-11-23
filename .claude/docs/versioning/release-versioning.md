# Release versioning

## Who bumps what

The release is never incremented automatically and never without the user's explicit decision. In SDL the
rule is "a version branch keeps its version; components are bumped only when a new branch is cut, and only
those the branch works on". This project adopted SDL's versioning wholesale (plan
`.claude/design/plans/sdl-versioning-and-sql-source.md`), so agents do not bump a release as a side effect
of a change and do not propose bumps mid-branch. **Unverified:** whether this project follows SDL's
"bump only when a branch is cut" cadence; the global CLAUDE.md says branching is loose per project. Until the
user says otherwise, ask before changing any of the three lines in `version.h`.

Bumping one component does not touch the others. Release numbers are edited by hand in
`sources/dependencies/shared/version.h` (one line each), under `bak.py guard` like any `sources/` edit.

## Major / Minor / Patch

Read against the component's own scope: a "new feature" for the client is a rendering or UI feature, for the
server a simulation feature, for sql a schema capability.

- **Major** (`1.4.2 -> 2.0.0`, resets Minor and Patch): overhauls, expansion-level or fundamental changes.
- **Minor** (`1.4.2 -> 1.5.0`, resets Patch): new features, items, creatures, maps, mechanics.
- **Patch** (`1.4.2 -> 1.4.3`): hotfixes, bug fixes, performance, UI adjustments, small balance changes,
  maintenance.

## Maturity

`alpha` while major systems change, `beta` while testing and polishing, no suffix from `1.0.0`.

## protocol_version (the compatibility gate)

An integer in `version.h` that must match exactly between client and server for login. It replaces the old
"Compatibility" track of `version.cfg`.

Test, unchanged from the old system: **does the client need a code change to handle this?** If yes, bump.
If the server only sends an existing message more or less often, it is server-only: no bump.

Bumping it is allowed mid-branch (plan sect. 4 default, recorded as an exception to SDL's "branch keeps its
version" rule, because it is a wire value and not a release). Old clients sent `version_major=0` and are
rejected by `protocol_version=1`.

Bump it together with a rebuild of both client and server; there is no automatic reconciliation.
