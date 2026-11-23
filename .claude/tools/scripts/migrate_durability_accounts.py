"""
Migration: Rename 'cur_lifespan' column to 'cur_durability' in account databases.

Applies to: Binaries/Server/accounts/*.db
Tables: character_items, character_bank_items

Usage:
  python Scripts/migrate_durability_accounts.py --dry-run    # preview only
  python Scripts/migrate_durability_accounts.py --verify     # verify after migration
  python Scripts/migrate_durability_accounts.py              # apply migration
"""

import sqlite3
import sys
import os
import glob

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.join(SCRIPT_DIR, "..")
ACCOUNTS_DIR = os.path.join(ROOT_DIR, "Binaries", "Server", "accounts")
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


def get_create_sql(table):
	"""Return the new CREATE TABLE SQL with cur_durability column."""
	if table == "character_items":
		return (
			f"CREATE TABLE {table}_new ("
			" character_name TEXT NOT NULL,"
			" slot INTEGER NOT NULL,"
			" item_id INTEGER NOT NULL,"
			" count INTEGER NOT NULL,"
			" touch_effect_type INTEGER NOT NULL,"
			" touch_effect_value1 INTEGER NOT NULL,"
			" touch_effect_value2 INTEGER NOT NULL,"
			" touch_effect_value3 INTEGER NOT NULL,"
			" item_color INTEGER NOT NULL,"
			" spec_effect_value1 INTEGER NOT NULL,"
			" spec_effect_value2 INTEGER NOT NULL,"
			" spec_effect_value3 INTEGER NOT NULL,"
			" cur_durability INTEGER NOT NULL,"
			" custom_made INTEGER NOT NULL DEFAULT 0,"
			" prefix_type INTEGER NOT NULL DEFAULT 0,"
			" prefix_value INTEGER NOT NULL DEFAULT 0,"
			" secondary_type INTEGER NOT NULL DEFAULT 0,"
			" secondary_value INTEGER NOT NULL DEFAULT 0,"
			" enchant_bonus INTEGER NOT NULL DEFAULT 0,"
			" pos_x INTEGER NOT NULL,"
			" pos_y INTEGER NOT NULL,"
			" is_equipped INTEGER NOT NULL,"
			" PRIMARY KEY(character_name, slot),"
			" FOREIGN KEY(character_name) REFERENCES characters(character_name) ON DELETE CASCADE"
			")"
		)
	else:  # character_bank_items
		return (
			f"CREATE TABLE {table}_new ("
			" character_name TEXT NOT NULL,"
			" slot INTEGER NOT NULL,"
			" item_id INTEGER NOT NULL,"
			" count INTEGER NOT NULL,"
			" touch_effect_type INTEGER NOT NULL,"
			" touch_effect_value1 INTEGER NOT NULL,"
			" touch_effect_value2 INTEGER NOT NULL,"
			" touch_effect_value3 INTEGER NOT NULL,"
			" item_color INTEGER NOT NULL,"
			" spec_effect_value1 INTEGER NOT NULL,"
			" spec_effect_value2 INTEGER NOT NULL,"
			" spec_effect_value3 INTEGER NOT NULL,"
			" cur_durability INTEGER NOT NULL,"
			" custom_made INTEGER NOT NULL DEFAULT 0,"
			" prefix_type INTEGER NOT NULL DEFAULT 0,"
			" prefix_value INTEGER NOT NULL DEFAULT 0,"
			" secondary_type INTEGER NOT NULL DEFAULT 0,"
			" secondary_value INTEGER NOT NULL DEFAULT 0,"
			" enchant_bonus INTEGER NOT NULL DEFAULT 0,"
			" PRIMARY KEY(character_name, slot),"
			" FOREIGN KEY(character_name) REFERENCES characters(character_name) ON DELETE CASCADE"
			")"
		)


def migrate_account_db(db_path, dry_run):
	account_name = os.path.basename(db_path)
	conn = sqlite3.connect(db_path)
	conn.row_factory = sqlite3.Row
	changes = 0

	for table in ("character_items", "character_bank_items"):
		if not has_table(conn, table):
			continue

		has_old = has_column(conn, table, "cur_lifespan")
		has_new = has_column(conn, table, "cur_durability")

		if has_new and not has_old:
			log(f"  {account_name} {table}: already migrated")
			continue

		if not has_old:
			log(f"  {account_name} {table}: no 'cur_lifespan' column found, skipping")
			continue

		cur = conn.execute(f"SELECT * FROM {table}")
		rows = cur.fetchall()
		col_names = [desc[0] for desc in cur.description]

		log(f"  {account_name} {table}: {len(rows)} rows to migrate")

		if dry_run:
			for row in rows:
				log(f"    {row['character_name']} slot={row['slot']} item={row['item_id']} "
					f"cur_lifespan={row['cur_lifespan']}")
			changes += len(rows)
			continue

		# Rebuild table with renamed column
		new_create = get_create_sql(table)
		conn.execute(new_create)

		new_cols = [c if c != "cur_lifespan" else "cur_durability" for c in col_names]

		for row in rows:
			values = [row[c] for c in col_names]
			placeholders = ",".join("?" * len(values))
			col_list = ",".join(new_cols)
			conn.execute(f"INSERT INTO {table}_new ({col_list}) VALUES ({placeholders})", values)

		conn.execute(f"DROP TABLE {table}")
		conn.execute(f"ALTER TABLE {table}_new RENAME TO {table}")
		conn.commit()

		changes += len(rows)
		log(f"    Migrated {len(rows)} rows")

	conn.close()
	return changes


def migrate(dry_run):
	accounts_dir = os.path.abspath(ACCOUNTS_DIR)
	log(f"\n{'='*60}")
	log(f"Account databases: {accounts_dir}")
	log(f"{'='*60}")

	if not os.path.isdir(accounts_dir):
		log("No accounts directory found. Nothing to migrate.")
		return True

	db_files = sorted(glob.glob(os.path.join(accounts_dir, "*.db")))
	if not db_files:
		log("No account databases found.")
		return True

	log(f"Found {len(db_files)} account database(s)")
	total_rows = 0
	for db_file in db_files:
		rows = migrate_account_db(db_file, dry_run)
		total_rows += rows

	log(f"\nTotal rows migrated: {total_rows}")
	return True


def verify():
	log("\n=== VERIFICATION ===\n")
	errors = 0

	accounts_dir = os.path.abspath(ACCOUNTS_DIR)
	if not os.path.isdir(accounts_dir):
		log("No accounts directory. Skipping.")
		return True

	db_files = sorted(glob.glob(os.path.join(accounts_dir, "*.db")))
	if not db_files:
		log("No account databases found.")
		return True

	for db_file in db_files:
		account_name = os.path.basename(db_file)
		conn = sqlite3.connect(db_file)

		for table in ("character_items", "character_bank_items"):
			if not has_table(conn, table):
				continue

			if has_column(conn, table, "cur_lifespan"):
				log(f"FAIL: {account_name} {table} still has 'cur_lifespan' column")
				errors += 1
			else:
				log(f"OK: {account_name} {table} no old 'cur_lifespan' column")

			if has_column(conn, table, "cur_durability"):
				count = conn.execute(f"SELECT count(*) FROM {table}").fetchone()[0]
				log(f"OK: {account_name} {table} has 'cur_durability' column ({count} rows)")
			else:
				log(f"FAIL: {account_name} {table} missing 'cur_durability' column")
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
		log("=== migrate_durability_accounts.py [VERIFY] ===")
		ok = verify()
		log_path = os.path.join(OUTPUT_DIR, "migrate_durability_accounts_verify.log")
		with open(log_path, "w", encoding="utf-8") as f:
			f.write("\n".join(log_lines))
		log(f"\nLog written to: {log_path}")
		sys.exit(0 if ok else 1)

	mode = "DRY RUN" if dry_run else "APPLY"
	log(f"=== migrate_durability_accounts.py [{mode}] ===")

	migrate(dry_run)

	if dry_run:
		log("\nDry run complete. No changes made.")

	log_path = os.path.join(OUTPUT_DIR, f"migrate_durability_accounts_{'dry_run' if dry_run else 'apply'}.log")
	with open(log_path, "w", encoding="utf-8") as f:
		f.write("\n".join(log_lines))
	log(f"\nLog written to: {log_path}")


if __name__ == "__main__":
	main()
