#!/usr/bin/env python3
"""
Database migration script: Strip guild-related columns from character databases.

Removes these columns from the 'characters' table in account .db files:
  - guild_name
  - guild_guid
  - guild_rank
  - fightzone_number
  - reserve_time
  - fightzone_ticket_number

Usage:
  python migrate_strip_guild_columns.py <path_to_accounts_dir>
  python migrate_strip_guild_columns.py Binaries/Server/accounts

The script is safe and idempotent - it skips databases where the columns
have already been removed. A .bak backup is created before any modification.

Requires SQLite >= 3.35.0 (for ALTER TABLE DROP COLUMN).
"""

import sqlite3
import shutil
import sys
from pathlib import Path

COLUMNS_TO_DROP = [
    "guild_name",
    "guild_guid",
    "guild_rank",
    "fightzone_number",
    "reserve_time",
    "fightzone_ticket_number",
]

TABLE_NAME = "characters"


def get_column_names(cursor: sqlite3.Cursor, table: str) -> list[str]:
    cursor.execute(f"PRAGMA table_info({table})")
    return [row[1] for row in cursor.fetchall()]


def migrate_database(db_path: Path) -> bool:
    """Strip guild columns from a single database. Returns True if modified."""
    conn = sqlite3.connect(str(db_path))
    try:
        cursor = conn.cursor()

        # Check if table exists
        cursor.execute(
            "SELECT name FROM sqlite_master WHERE type='table' AND name=?",
            (TABLE_NAME,),
        )
        if not cursor.fetchone():
            print(f"  SKIP (no '{TABLE_NAME}' table): {db_path.name}")
            return False

        existing_columns = get_column_names(cursor, TABLE_NAME)
        columns_present = [c for c in COLUMNS_TO_DROP if c in existing_columns]

        if not columns_present:
            print(f"  SKIP (columns already removed): {db_path.name}")
            return False

        # Back up before modifying
        backup_path = db_path.with_suffix(".db.guild_migration_bak")
        if not backup_path.exists():
            shutil.copy2(db_path, backup_path)
            print(f"  Backup: {backup_path.name}")

        # Use ALTER TABLE DROP COLUMN (SQLite >= 3.35.0).
        # This preserves PRIMARY KEY, NOT NULL, DEFAULT, FOREIGN KEY constraints.
        for col in columns_present:
            cursor.execute(f"ALTER TABLE {TABLE_NAME} DROP COLUMN {col}")

        conn.commit()

        # Verify
        new_columns = get_column_names(cursor, TABLE_NAME)
        removed = set(existing_columns) - set(new_columns)
        print(f"  OK: {db_path.name} — dropped {sorted(removed)}")
        return True

    except Exception as e:
        print(f"  ERROR: {db_path.name} — {e}")
        conn.rollback()
        return False
    finally:
        conn.close()


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <accounts_directory>")
        print(f"Example: {sys.argv[0]} Binaries/Server/accounts")
        sys.exit(1)

    # Check SQLite version
    ver = tuple(int(x) for x in sqlite3.sqlite_version.split("."))
    if ver < (3, 35, 0):
        print(f"Error: SQLite {sqlite3.sqlite_version} is too old. Need >= 3.35.0 for DROP COLUMN.")
        sys.exit(1)

    accounts_dir = Path(sys.argv[1])
    if not accounts_dir.is_dir():
        print(f"Error: '{accounts_dir}' is not a directory")
        sys.exit(1)

    db_files = sorted(accounts_dir.glob("*.db"))
    if not db_files:
        print(f"No .db files found in {accounts_dir}")
        sys.exit(0)

    print(f"Found {len(db_files)} database(s) in {accounts_dir}")
    print(f"Columns to strip: {COLUMNS_TO_DROP}")
    print()

    modified = 0
    for db_path in db_files:
        if migrate_database(db_path):
            modified += 1

    print()
    print(f"Done. {modified}/{len(db_files)} database(s) modified.")


if __name__ == "__main__":
    main()
