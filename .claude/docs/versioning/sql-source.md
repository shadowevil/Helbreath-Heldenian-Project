# SQL source directory (`sources/database/`)

The single source of truth for the server databases. `gamedata.db` and `mapinfo.db` are generated from it.
**The full reference is `sources/database/README.md`; this file points and summarises, it does not repeat.**
Facts taken from that README and the script headers (2026-11-23); no script was run for this doc.

## Layout

```
sources/database/
  gamedata/  mapinfo/  account/     one <table>.sql + one <table>.data.sql per table
  extract_databases.py              .db -> this directory (opens the .db read-only and immutable)
  verify_roundtrip.py               this directory -> in-memory DB, compared with the originals
  build_databases.py                this directory -> gamedata.db + mapinfo.db (+ schema_sql_generated.h)
```

- `<t>.sql`: `CREATE TABLE` verbatim from `sqlite_master`, then its indexes and triggers.
- `<t>.data.sql`: prepopulated rows, one `INSERT` per line sorted by primary key, explicit ids, blobs as
  `X'hex'`. An empty table has an empty sidecar. `_sqlite_sequence.data.sql` holds AUTOINCREMENT counters.
- Load order: every `.sql`, then every `.data.sql`, then the sequence file.
- Files are UTF-8 and stored verbatim; some contain embedded CR on purpose, and `.gitattributes` marks them
  `-text`. Never normalise their line endings.

## Rules

- Edit this directory, then rebuild. An edit made to a deployed `.db` (for example by `tools/`) must be
  written back with `extract_databases.py` before the next build, or the build refuses to overwrite it.
- Keep one table, one `.sql`, one `.data.sql`.
- `account/` holds structure only (from `accounts/shadowevil.db`; the extractor fails if another account DB's
  schema differs). Its data sidecars stay empty on purpose: never put player data or password hashes here.

## Scripts (see README for every flag)

```
python sources/database/extract_databases.py [--db gamedata|mapinfo|account] [--dry-run]
python sources/database/verify_roundtrip.py
python sources/database/build_databases.py [--db ...] [--channel DEV|TEST|PROD] [--force] [--verify]
```

`build_databases.py` writes the DBs to `game/server/` by default and an untracked `schema_sql_generated.h`
(default `build/generated/`) with a schema text and fingerprint per DB and the sql release, channel and
version text. It adds to each DB's `meta` table: `sql_release` (parsed from `version.h`), `schema_fingerprint`,
`channel`, `version`. No timestamps, so an unchanged source rebuilds to an identical DB.

## Operator state

`admins`, `banned_list` and `admin_command_permissions` (gamedata) are written at runtime by server commands.
They have structure and sidecar files like any table, but their live rows are carried from the existing target
DB into every rebuild and are left out of the overwrite guard. The sidecars only seed a DB that does not yet
exist. A schema change that drops a column holding data in those tables is refused unless `--force`.

## Overwrite guard

After each build `<db>.generated.json` records a content digest. A target is replaced only when it is missing,
already identical, identical except the generated `meta` keys, or still matches its record. Anything else may
hold edits that exist nowhere else: the build refuses and says to run the extractor first; `--force` discards
those edits. A stale `-wal`, `-shm` or `-journal` beside a target is refused too (`--force` deletes it).
**Stop the server before building**, on any OS. Details and the interrupted-install restore rules: README.

## The sql component

`sql_release` in `version.h` (`0.0.1-alpha` at 2026-11-23) is the third hand-assigned release. It is stamped
into each DB's `meta` table and shown in the server banner and `/version`. Schema or seed changes are what move
it, by the user's decision (`release-versioning.md`).

## Not yet wired (unverified, per the plan)

- `deploy.py` does not run the generator: `DATABASE_HOOK_ENABLED = False`. Run `build_databases.py` by hand.
- Whether the server verifies `schema_fingerprint`, `sql_release` and channel at start-up, stopped creating
  tables, and applies account-DB migrations (plan phase F) is **unverified**; the plan lists it as later work.
- `.gitignore` coverage of `game/server/*.db` and `accounts/`: see the plan, phase G.
