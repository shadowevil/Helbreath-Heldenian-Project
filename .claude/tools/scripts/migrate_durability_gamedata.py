"""
Migration: Rename 'lifespan' column to 'durability' in character_creation_items table.

Applies to: Binaries/Server/gamedata.db

Usage:
  python Scripts/migrate_durability_gamedata.py --dry-run    # preview only
  python Scripts/migrate_durability_gamedata.py --verify     # verify after migration
  python Scripts/migrate_durability_gamedata.py              # apply migration
"""

import sqlite3
import sys
import os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.join(SCRIPT_DIR, "..")
GAMEDATA_PATH = os.path.join(ROOT_DIR, "Binaries", "Server", "gamedata.db")
OUTPUT_DIR = os.path.join(SCRIPT_DIR, "output")

log_lines = []


def log(msg):
	print(msg)
	log_lines.append(msg)


def has_table(conn, table):
	cur = conn.execute("SELECT count(*) FROM sqlite_master WHERE type='table' AND name=?", (table,))
	return cur.fetchone()[0] > 0


def has_column(conn, table, column):
	cur = conn.execute(f"PRAGMA table_info({table})")
	return any(row[1] == column for row in cur.fetchall())


def migrate(dry_run):
	db_path = os.path.abspath(GAMEDATA_PATH)
	log(f"\n{'='*60}")
	log(f"gamedata.db: {db_path}")
	log(f"{'='*60}")

	if not os.path.exists(db_path):
		log("ERROR: gamedata.db not found!")
		return False

	conn = sqlite3.connect(db_path)
	conn.row_factory = sqlite3.Row

	table = "character_creation_items"

	if not has_table(conn, table):
		log(f"Table '{table}' does not exist. Nothing to migrate.")
		conn.close()
		return True

	# Check current state
	has_old = has_column(conn, table, "lifespan")
	has_new = has_column(conn, table, "durability")

	if has_new and not has_old:
		log(f"Already migrated: '{table}' has 'durability' column.")
		conn.close()
		return True

	if not has_old:
		log(f"Neither 'lifespan' nor 'durability' column found in '{table}'. Nothing to do.")
		conn.close()
		return True

	# Read existing data
	cur = conn.execute(f"SELECT * FROM {table}")
	rows = cur.fetchall()
	col_names = [desc[0] for desc in cur.description]

	log(f"\nMigrating '{table}': rename 'lifespan' -> 'durability' ({len(rows)} rows)")

	if dry_run:
		for row in rows:
			log(f"  class={row['class_type']} item={row['item_id']} lifespan={row['lifespan']}")
		log("\n[DRY RUN] No changes made.")
		conn.close()
		return True

	# Rebuild table with renamed column
	new_create = (
		f"CREATE TABLE {table}_new ("
		" class_type INTEGER NOT NULL,"
		" item_id INTEGER NOT NULL,"
		" count INTEGER NOT NULL DEFAULT 1,"
		" item_color INTEGER NOT NULL DEFAULT 0,"
		" durability INTEGER NOT NULL DEFAULT 0,"
		" is_equipped INTEGER NOT NULL DEFAULT 0,"
		" gender_limit INTEGER NOT NULL DEFAULT 0,"
		" sort_order INTEGER NOT NULL DEFAULT 0,"
		" PRIMARY KEY (class_type, item_id, gender_limit)"
		")"
	)

	conn.execute(new_create)

	# Copy data, renaming lifespan -> durability
	new_cols = [c if c != "lifespan" else "durability" for c in col_names]
	for row in rows:
		values = [row[c] for c in col_names]
		placeholders = ",".join("?" * len(values))
		col_list = ",".join(new_cols)
		conn.execute(f"INSERT INTO {table}_new ({col_list}) VALUES ({placeholders})", values)

	conn.execute(f"DROP TABLE {table}")
	conn.execute(f"ALTER TABLE {table}_new RENAME TO {table}")
	conn.commit()

	log(f"Migrated {len(rows)} rows successfully.")
	conn.close()
	return True


def verify():
	log("\n=== VERIFICATION ===\n")
	errors = 0

	db_path = os.path.abspath(GAMEDATA_PATH)
	if not os.path.exists(db_path):
		log("ERROR: gamedata.db not found!")
		return False

	conn = sqlite3.connect(db_path)
	table = "character_creation_items"

	if not has_table(conn, table):
		log(f"FAIL: Table '{table}' does not exist")
		conn.close()
		return False

	if has_column(conn, table, "lifespan"):
		log(f"FAIL: '{table}' still has old 'lifespan' column")
		errors += 1
	else:
		log(f"OK: '{table}' no longer has 'lifespan' column")

	if has_column(conn, table, "durability"):
		log(f"OK: '{table}' has 'durability' column")
		count = conn.execute(f"SELECT count(*) FROM {table}").fetchone()[0]
		log(f"  {count} rows present")
	else:
		log(f"FAIL: '{table}' missing 'durability' column")
		errors += 1

	conn.close()

	if errors == 0:
		log("\nVerification PASSED")
	else:
		log(f"\nVerification FAILED ({errors} error(s))")
	return errors == 0


def main():
	dry_run = "--dry-run" in sys.argv
	do_verify = "--verify" in sys.argv

	os.makedirs(OUTPUT_DIR, exist_ok=True)

	if do_verify:
		log("=== migrate_durability_gamedata.py [VERIFY] ===")
		ok = verify()
		log_path = os.path.join(OUTPUT_DIR, "migrate_durability_gamedata_verify.log")
		with open(log_path, "w", encoding="utf-8") as f:
			f.write("\n".join(log_lines))
		log(f"\nLog written to: {log_path}")
		sys.exit(0 if ok else 1)

	mode = "DRY RUN" if dry_run else "APPLY"
	log(f"=== migrate_durability_gamedata.py [{mode}] ===")

	migrate(dry_run)

	log_path = os.path.join(OUTPUT_DIR, f"migrate_durability_gamedata_{'dry_run' if dry_run else 'apply'}.log")
	with open(log_path, "w", encoding="utf-8") as f:
		f.write("\n".join(log_lines))
	log(f"\nLog written to: {log_path}")


if __name__ == "__main__":
	main()
