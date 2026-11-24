# decisions: Versioning, build proof and databases

*Sidecar of `.claude/docs/decisions.md` (index, scope rule and entry format live there). Each entry is a settled user ruling with the user's exact quote.*

## d-001 Adopt SDL's versioning and a SQL source directory

- Date: 2026-11-23.
- Quote: "Every bit of SDL's versioning needs to be adopted - we should have a single source of truth SQL directory with every table having a sql file with it's structure and a side car file with it's prepopulated data".
- Decided: Helbreath-SDL's versioning standard is adopted in full, and `sources/database/` is the single source of truth for the databases, one `.sql` and one `.data.sql` per table.
- Recorded in: CLAUDE.md "Versioning"; `.claude/docs/versioning.md` and its sidecars; `sources/database/README.md`.

## d-002 The clock-skew window exchange stays

- Date: 2026-11-23.
- Quote: "Every bit" (from d-001); the user ruled that this means the build proof keeps SDL's clock-skew window exchange.
- Decided: the build proof keeps the clock-skew window exchange.
- Recorded in: `.claude/docs/versioning/standards.md`, `.claude/docs/versioning/build-proof.md`.

## d-008 Old versioning files are removed

- Date: 2026-11-23.
- Quote: "Clean up any leftover old versioning files as well".
- Decided: the INI-based version system and its build counters are removed, not kept alongside the new one.
- Recorded in: CLAUDE.md "Versioning"; `.claude/docs/versioning.md`.

## d-017 Operator-state tables are preserved across rebuilds

- Date: 2026-11-23.
- Quote (selected answer): "Operator state, preserved".
- Decided: `admins`, `banned_list` and `admin_command_permissions` keep their schema in the SQL directory, and their rows are carried over from the live DB on each rebuild.
- Recorded in: `sources/database/README.md`; `build_databases.py` (`OPERATOR_TABLES`).

## d-018 Build tools may run read-only git

- Date: 2026-11-23.
- Quote (selected answer): "Allow read-only git in build tools".
- Decided: build generators (the Build ID generator) run read-only git during a build.
- Recorded in: CLAUDE.md "Workflow"; `.claude/docs/versioning/build-id.md`.

## d-019 The build proof keeps the clock-skew retry

- Date: 2026-11-23.
- Quote (selected answer): "Add the skew exchange".
- Decided: a login refused only for clock skew gets a clock-skew reply carrying the server window, and the client retries once with that offset.
- Recorded in: `.claude/docs/versioning/build-proof.md`.

## d-020 The game-port gate bypass is closed

- Date: 2026-11-23.
- Quote (selected answer): "Fix it".
- Decided: the game port admits a player only after a versioned enter-game from the same address with the same password.
- Recorded in: `.claude/docs/versioning/build-proof.md`.
