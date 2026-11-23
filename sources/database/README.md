# Database SQL source

*Assessed 2026-11-23 (docs standard sect. 1): kept whole, about 11 KB, just over the 10 KB review line. It documents one tool set next to the code it describes; the layout, operator-state, extract and build sections presuppose each other and are read together before editing a table or running a script. `.claude/docs/versioning/sql-source.md` is the index entry that points here. Paths checked against disk 2026-11-23.*

Contents: Layout - Rules - Operator state - Account databases - Extract and verify - Build the databases.

This directory is the single source of truth for the server databases. `gamedata.db` and
`mapinfo.db` under `game/server/` (the deployed game) are generated from it by `build_databases.py`.
An edit made directly to a deployed `.db` (by `tools/` or by hand) must be written back here with the
extractor before the next build, or the build refuses to overwrite it.

## Layout

```
sources/database/
  gamedata/   one pair of files per table of gamedata.db
  mapinfo/    one pair of files per table of mapinfo.db (lowercase name: the code opens mapinfo.db)
  account/    structure of the per-player account databases (accounts/<name>.db)
  extract_databases.py   .db -> this directory (read-only on the .db)
  verify_roundtrip.py    this directory -> in-memory DB, compared with the originals
  build_databases.py     this directory -> gamedata.db + mapinfo.db (+ schema_sql_generated.h)
```

Per table `<t>`:

- `<t>.sql` - the `CREATE TABLE` exactly as stored in `sqlite_master`, then that table's
  indexes and triggers as stored. A view (none exist today) gets its own `<view>.sql`, with
  its triggers, and no data sidecar.
- `<t>.data.sql` - prepopulated rows, one `INSERT INTO "<t>" (<columns>) VALUES (...);` per
  line, sorted by primary key (rowid if there is none), explicit ids included, blobs as
  `X'hex'`, floats via `repr()`, newlines inside text rebuilt with `char()`. An empty table
  has an empty sidecar.
- `_sqlite_sequence.data.sql` (per DB, when it has AUTOINCREMENT tables) - the counters;
  it starts with `DELETE FROM sqlite_sequence;` because loading rows already creates entries.

Load order: every `.sql`, then every `.data.sql`, then `_sqlite_sequence.data.sql`.

## Rules

- This directory is the source of truth. Edits made to a deployed `.db` (for example by `tools/`)
  must be written back with the extractor, then the DB rebuilt.
- Keep the layout: one table, one `.sql`, one `.data.sql`. Auto-indexes are not stored.
- Files are read and written as UTF-8 and stored verbatim. Some `.sql` files contain embedded
  CR characters (copied exactly from `sqlite_master`) and `mapinfo/map_item_events.data.sql`
  has non-ASCII bytes; both are intentional. `.gitattributes` marks `sources/database/**/*.sql`
  as `-text` so git never rewrites them. Do not normalize line endings in these files.

## Operator state

`admins`, `banned_list` and `admin_command_permissions` (gamedata) are written at runtime by
server commands. They have a structure file and a data sidecar like every other table, but
their live rows are operator state: `build_databases.py` copies their rows from the existing
target DB into every rebuild (columns matched by name), and leaves them out of the overwrite
guard, so a server writing to them never blocks a build. If the schema here drops or renames one of
their columns and that column holds data in the target (for example `admins.approved_ip`, which
would fall back to `0.0.0.0` and weaken admin IP locking), the build refuses unless `--force`; an
empty dropped column is only reported. The sidecars here only seed a DB that does not exist yet.

## Account databases

Account databases hold per-player runtime data (including password hashes). Only their
structure is extracted (from `accounts/shadowevil.db`; every other `accounts/*.db` is
cross-checked and the extractor fails if any schema differs). Every `account/*.data.sql` stays empty on purpose; never put player data here.

## Extract and verify

```
python sources/database/extract_databases.py                    # all DBs
python sources/database/extract_databases.py --db gamedata      # one DB
python sources/database/extract_databases.py --dry-run          # report only
python sources/database/verify_roundtrip.py                     # exit code 1 on any FAIL
```

The extractor opens databases with `mode=ro&immutable=1` and never writes to a `.db`; it only
rewrites a file when its content changed, and warns about files it no longer produces.
The verifier compares, per table, the `sqlite_master` sql text, the row count and a SHA-256
over the rows ordered by primary key (structure only for account).

## Build the databases (build_databases.py)

```
python sources/database/build_databases.py                          # gamedata.db + mapinfo.db into game/server/
python sources/database/build_databases.py --db gamedata            # one DB
python sources/database/build_databases.py --output-dir DIR --header-out FILE
python sources/database/build_databases.py --channel TEST           # DEV (default), TEST or PROD
python sources/database/build_databases.py --force                  # discard edits in a changed DB, delete stale -wal/-shm/-journal
python sources/database/build_databases.py --discard-backup         # create a DB although an interrupted install left a backup
python sources/database/build_databases.py --verify                 # build into scratch, compare with game/server/
```

- Builds `gamedata.db` and `mapinfo.db` only. Account DBs are runtime state (password hashes) and are
  never built or touched. Python standard library only, ASCII console output.
- Writes the untracked `schema_sql_generated.h` (default `build/generated/`, override with
  `--header-out`): per DB a `constexpr std::string_view` schema text (the structure files concatenated
  in ordinal order) and `*_fingerprint`, plus `sql_release_text`, `channel_label` and `version_text`, in
  `hb::server::schema`. The fingerprint is a sha256 over the structure (`<table>.sql`) files in ordinal
  order, with line endings normalized to LF; data sidecars are not part of it.
- Each DB's `meta` table keeps its sidecar rows (`schema_version` stays until the server stops reading
  it) and gains `sql_release` (parsed from `sources/dependencies/shared/version.h`),
  `schema_fingerprint`, `channel` and `version` (`0.0.1-alpha:DEV`; PROD drops the channel label). No
  timestamps, so an unchanged source rebuilds to an identical DB and a second run is a no-op.
- **Stop the server before building, on any OS.** On Windows a DB the server holds open cannot be
  replaced (the build fails and restores everything). On Linux the rename succeeds and the server
  keeps writing to the old, unlinked file, so those writes are lost. Just before replacing, the build
  tries `BEGIN EXCLUSIVE` on each existing target (0.5 s timeout) and refuses one that is busy, but an
  idle server holds no lock, so this probe cannot prove the server is stopped.
- Overwrite guard: after each build `<db>.generated.json` (beside the DB) records a content digest of
  the generated DB (the same structure text + per-table row hashes that `verify_roundtrip.py` uses,
  without the operator-state rows). A target is replaced only when it is missing, already equals the
  fresh build (kept, not rewritten), equals it except for the generated `meta` keys (adopted, e.g. a DB
  you just extracted, or a `--channel` switch), or still matches its record. Anything else may hold
  edits that exist nowhere else: the build refuses and says to run `extract_databases.py` (which reads
  `game/server/`) and build again, which then adopts the DB; `--force` discards the edits instead. A
  generation record that is not a JSON object counts as missing.
- A `-wal`, `-shm` or `-journal` file beside a target (create or replace) is always refused, because
  SQLite would replay it into the new DB: stop the server / checkpoint first. `--force` deletes exactly
  those files, logs each one, and builds clean (whatever they held is lost). They are deleted only
  after every target passed the in-use probe, so a busy target keeps them. Targets without stale files
  are probed first, because opening one that has them lets SQLite run its own recovery on it (roll back
  a hot journal, merge or drop a `-wal`: the same recovery the server runs on its next open). So a run
  refused because a clean target is busy never touches a stale one; only when both DBs have stale files
  and the second is busy can the first already have been recovered by SQLite before the refusal.
- Install: each DB is built to a temp file beside its target, and every guard is checked before any
  file is touched, so a refusal leaves every target as it was. Then each existing target is moved to
  `.<db>.bak<pid>`, the new file renamed in, and the records written; if any step fails or is
  interrupted with Ctrl-C (a target in use shows as "is the server running?") every backup and record
  is restored. The restore works from what is on disk (a backup that exists goes back; a target that
  did not exist before is removed), so an interruption right after a rename is undone too. Only if a restore itself fails (or the process is killed mid-install) is a DB left
  changed, and the error names the backup file that holds the old one. A missing target with a
  `.<db>.bak<pid>` beside it is then refused, even with `--force`, so the operator rows are not
  silently reset to the seed: rename the backup back to `<db>`, or pass `--discard-backup` to build
  from the seed rows (the backup file is kept).
- One build per output folder at a time: the build creates `<output-dir>/.build_databases.lock`
  exclusively (holding `pid <n>`) and removes it when it exits, but only while it still holds that
  pid; a failed or interrupted lock write removes the file at once. A second run refuses while it
  exists; if a killed run left it behind, the message names the file to delete.
- Temp files an interrupted run left beside a target (`.<db>.tmp<pid>`, its `-journal`, and
  `<db>.generated.json.tmp<pid>`) are deleted at the start of the next build; a leftover
  `.<db>.bak<pid>` is only reported, since it may be the only copy of an old DB.
- The source SQL runs with `ATTACH` denied (which also blocks `VACUUM INTO`), so it can only write the
  DB being built. One DB's schema text may be at most 65,000 bytes (the header's string literal limit);
  the header splits long lines into literal pieces of at most 4,000 characters.
- `--verify` builds into `.claude/scratch/build_databases/verify/` (or `--output-dir`) and compares per
  table the `sqlite_master` text, row count and content hash with the existing DBs (`--compare-dir`,
  default `game/server/`, opened read-only and immutable). It refuses an output folder that is the
  compare folder or `game/server/` (checked by file identity, so 8.3 short names and links do not get
  past it), and one holding a DB without a matching generation record, even with `--force`. The
  operator tables are carried from the `--compare-dir` DB, and the output DBs are always reinstalled
  so those rows are current. `meta` is the one table that is not
  hash-compared: every original row except the generated keys must be kept and the generated keys
  must exist. Exit status 1 on any FAIL.
