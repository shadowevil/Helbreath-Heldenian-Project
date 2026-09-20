"""
Standalone script to convert account databases from packed attribute bitmask
to individual columns (prefix_type, prefix_value, secondary_type, etc.).

Handles databases that still have the old single 'attribute' column in
character_items and/or character_bank_items tables.

Usage:
  python Scripts/convert_account_db.py --dry-run              # preview all DBs
  python Scripts/convert_account_db.py --dry-run shadowevil   # preview one account
  python Scripts/convert_account_db.py                        # convert all DBs
  python Scripts/convert_account_db.py shadowevil             # convert one account
  python Scripts/convert_account_db.py --verify               # verify all DBs

The script scans Binaries/Server/accounts/*.db by default.
Pass an account name (without .db) to target a specific database.
"""

import sqlite3
import sys
import os
import glob
import shutil

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.join(SCRIPT_DIR, "..")
ACCOUNTS_DIR = os.path.join(ROOT_DIR, "Binaries", "Server", "accounts")


def has_table(conn, table):
	cur = conn.execute("SELECT count(*) FROM sqlite_master WHERE type='table' AND name=?", (table,))
	return cur.fetchone()[0] > 0


def has_column(conn, table, column):
	cur = conn.execute(f"PRAGMA table_info({table})")
	return any(row[1] == column for row in cur.fetchall())


def get_columns(conn, table):
	cur = conn.execute(f"PRAGMA table_info({table})")
	return [row[1] for row in cur.fetchall()]


def unpack_attribute(attr):
	"""Unpack old bitmask attribute into individual fields (raw nibbles 0-15).
	Layout matches Item.h unpack_legacy_attribute():
	  bit 0:     custom_made
	  bits 8-11: secondary_value
	  bits 12-15: secondary_type
	  bits 16-19: prefix_value
	  bits 20-23: prefix_type
	  bits 28-31: enchant_bonus
	"""
	attr = attr & 0xFFFFFFFF
	custom_made = (attr & 0x00000001)
	secondary_value = (attr >> 8) & 0x0F
	secondary_type = (attr >> 12) & 0x0F
	prefix_value = (attr >> 16) & 0x0F
	prefix_type = (attr >> 20) & 0x0F
	enchant_bonus = (attr >> 28) & 0x0F
	return (custom_made, prefix_type, prefix_value, secondary_type, secondary_value, enchant_bonus)


# New table schemas (must match AccountSqliteStore.cpp)
NEW_SCHEMA = {
	"character_items": (
		"CREATE TABLE character_items ("
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
		" cur_lifespan INTEGER NOT NULL,"
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
	),
	"character_bank_items": (
		"CREATE TABLE character_bank_items ("
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
		" cur_lifespan INTEGER NOT NULL,"
		" custom_made INTEGER NOT NULL DEFAULT 0,"
		" prefix_type INTEGER NOT NULL DEFAULT 0,"
		" prefix_value INTEGER NOT NULL DEFAULT 0,"
		" secondary_type INTEGER NOT NULL DEFAULT 0,"
		" secondary_value INTEGER NOT NULL DEFAULT 0,"
		" enchant_bonus INTEGER NOT NULL DEFAULT 0,"
		" PRIMARY KEY(character_name, slot),"
		" FOREIGN KEY(character_name) REFERENCES characters(character_name) ON DELETE CASCADE"
		")"
	),
}

# Columns that exist after migration (order matters for INSERT)
NEW_ITEM_COLS = [
	"character_name", "slot", "item_id", "count",
	"touch_effect_type", "touch_effect_value1", "touch_effect_value2", "touch_effect_value3",
	"item_color", "spec_effect_value1", "spec_effect_value2", "spec_effect_value3",
	"cur_lifespan", "custom_made", "prefix_type", "prefix_value",
	"secondary_type", "secondary_value", "enchant_bonus",
	"pos_x", "pos_y", "is_equipped",
]

NEW_BANK_COLS = [
	"character_name", "slot", "item_id", "count",
	"touch_effect_type", "touch_effect_value1", "touch_effect_value2", "touch_effect_value3",
	"item_color", "spec_effect_value1", "spec_effect_value2", "spec_effect_value3",
	"cur_lifespan", "custom_made", "prefix_type", "prefix_value",
	"secondary_type", "secondary_value", "enchant_bonus",
]


def detect_status(conn, table):
	"""Detect the migration status of a table.
	Returns: 'missing', 'already_migrated', 'needs_migration', 'unknown'
	"""
	if not has_table(conn, table):
		return "missing"
	if has_column(conn, table, "prefix_type") and not has_column(conn, table, "attribute"):
		return "already_migrated"
	if has_column(conn, table, "attribute") and not has_column(conn, table, "prefix_type"):
		return "needs_migration"
	if has_column(conn, table, "attribute") and has_column(conn, table, "prefix_type"):
		return "unknown"  # both columns exist — unusual
	return "unknown"


def convert_table(conn, table, dry_run):
	"""Convert one table from packed attribute to individual columns.
	Returns (rows_converted, rows_with_nonzero_attr).
	"""
	new_cols = NEW_ITEM_COLS if table == "character_items" else NEW_BANK_COLS

	# Read all rows from old table
	cur = conn.execute(f"SELECT * FROM {table}")
	rows = cur.fetchall()
	old_col_names = [desc[0] for desc in cur.description]

	nonzero = 0
	converted_rows = []

	for row in rows:
		row_dict = dict(zip(old_col_names, row))
		attr = row_dict.get("attribute", 0)
		cm, pt, pv, st, sv, eb = unpack_attribute(attr)

		if attr != 0:
			nonzero += 1

		# Build new row values
		new_row = []
		for col in new_cols:
			if col == "custom_made":
				new_row.append(cm)
			elif col == "prefix_type":
				new_row.append(pt)
			elif col == "prefix_value":
				new_row.append(pv)
			elif col == "secondary_type":
				new_row.append(st)
			elif col == "secondary_value":
				new_row.append(sv)
			elif col == "enchant_bonus":
				new_row.append(eb)
			elif col in row_dict:
				new_row.append(row_dict[col])
			else:
				new_row.append(0)  # default for missing columns (pos_x, pos_y, is_equipped)

		converted_rows.append(new_row)

	if dry_run:
		# Preview non-zero conversions
		for row in rows:
			row_dict = dict(zip(old_col_names, row))
			attr = row_dict.get("attribute", 0)
			if attr != 0:
				cm, pt, pv, st, sv, eb = unpack_attribute(attr)
				attr_u = attr & 0xFFFFFFFF
				print(f"    {row_dict['character_name']} slot={row_dict['slot']} "
					  f"item={row_dict.get('item_id', '?')}: "
					  f"attr=0x{attr_u:08X} -> custom={cm} prefix={pt}/{pv} "
					  f"secondary={st}/{sv} enchant={eb}")
		return len(rows), nonzero

	# Apply: create new table, copy data, swap
	tmp_table = f"{table}_converted"

	# Drop temp table if it exists from a failed previous run
	conn.execute(f"DROP TABLE IF EXISTS {tmp_table}")

	# Create new table with _converted suffix
	create_sql = NEW_SCHEMA[table].replace(f"CREATE TABLE {table}", f"CREATE TABLE {tmp_table}")
	conn.execute(create_sql)

	# Insert converted rows
	placeholders = ",".join("?" * len(new_cols))
	col_list = ",".join(new_cols)
	for row_values in converted_rows:
		conn.execute(f"INSERT INTO {tmp_table} ({col_list}) VALUES ({placeholders})", row_values)

	# Swap tables
	conn.execute(f"DROP TABLE {table}")
	conn.execute(f"ALTER TABLE {tmp_table} RENAME TO {table}")

	return len(rows), nonzero


def process_db(db_path, dry_run):
	"""Process a single account database. Returns True if any work was done."""
	db_name = os.path.basename(db_path)
	conn = sqlite3.connect(db_path)
	conn.row_factory = None  # use tuples for speed

	any_work = False

	for table in ("character_items", "character_bank_items"):
		status = detect_status(conn, table)

		if status == "missing":
			print(f"  {db_name} {table}: table not found, skipping")
			continue

		if status == "already_migrated":
			print(f"  {db_name} {table}: already has individual columns, skipping")
			continue

		if status == "unknown":
			print(f"  {db_name} {table}: WARNING - unexpected schema (both attribute and prefix_type), skipping")
			continue

		# needs_migration
		any_work = True
		total, nonzero = convert_table(conn, table, dry_run)

		if dry_run:
			print(f"  {db_name} {table}: {total} rows ({nonzero} with non-zero attributes)")
		else:
			conn.commit()
			print(f"  {db_name} {table}: converted {total} rows ({nonzero} with non-zero attributes)")

	conn.close()
	return any_work


def verify_db(db_path):
	"""Verify a single account database has the correct schema."""
	db_name = os.path.basename(db_path)
	conn = sqlite3.connect(db_path)
	errors = 0

	for table in ("character_items", "character_bank_items"):
		if not has_table(conn, table):
			continue

		# Old attribute column should not exist
		if has_column(conn, table, "attribute"):
			print(f"  FAIL: {db_name} {table} still has old 'attribute' column")
			errors += 1

		# New columns should exist
		for col in ("custom_made", "prefix_type", "prefix_value",
					"secondary_type", "secondary_value", "enchant_bonus"):
			if not has_column(conn, table, col):
				print(f"  FAIL: {db_name} {table} missing column '{col}'")
				errors += 1

		# Value range checks
		if has_column(conn, table, "prefix_type"):
			cur = conn.execute(f"SELECT count(*) FROM {table} WHERE "
							   f"prefix_type < 0 OR prefix_type > 15 OR "
							   f"prefix_value < 0 OR prefix_value > 15 OR "
							   f"secondary_type < 0 OR secondary_type > 15 OR "
							   f"secondary_value < 0 OR secondary_value > 15 OR "
							   f"enchant_bonus < 0 OR enchant_bonus > 15")
			bad = cur.fetchone()[0]
			if bad > 0:
				print(f"  FAIL: {db_name} {table} has {bad} rows with values outside 0-15 range")
				errors += 1

			# Show summary
			cur = conn.execute(f"SELECT count(*) FROM {table}")
			total = cur.fetchone()[0]
			cur = conn.execute(f"SELECT count(*) FROM {table} WHERE "
							   f"prefix_type != 0 OR secondary_type != 0 OR enchant_bonus != 0")
			with_attrs = cur.fetchone()[0]
			print(f"  OK: {db_name} {table}: {total} rows ({with_attrs} with attributes)")

	conn.close()
	return errors


def main():
	args = [a for a in sys.argv[1:] if not a.startswith("--")]
	dry_run = "--dry-run" in sys.argv
	do_verify = "--verify" in sys.argv

	accounts_dir = os.path.abspath(ACCOUNTS_DIR)

	if not os.path.isdir(accounts_dir):
		print(f"ERROR: Accounts directory not found: {accounts_dir}")
		sys.exit(1)

	# Build list of DB files to process
	if args:
		# Specific account(s)
		db_files = []
		for name in args:
			if not name.endswith(".db"):
				name += ".db"
			path = os.path.join(accounts_dir, name)
			if os.path.exists(path):
				db_files.append(path)
			else:
				print(f"ERROR: Database not found: {path}")
				sys.exit(1)
	else:
		db_files = sorted(glob.glob(os.path.join(accounts_dir, "*.db")))

	if not db_files:
		print("No account databases found.")
		sys.exit(0)

	print(f"Found {len(db_files)} database(s) in {accounts_dir}")

	if do_verify:
		print("\n=== VERIFY ===\n")
		total_errors = 0
		for db_path in db_files:
			total_errors += verify_db(db_path)
		if total_errors == 0:
			print("\nAll databases OK.")
		else:
			print(f"\n{total_errors} error(s) found.")
			sys.exit(1)
		sys.exit(0)

	mode = "DRY RUN" if dry_run else "CONVERT"
	print(f"\n=== {mode} ===\n")

	if not dry_run:
		# Create backups before modifying
		for db_path in db_files:
			backup_path = db_path + ".bak"
			if not os.path.exists(backup_path):
				shutil.copy2(db_path, backup_path)
				print(f"  Backup: {os.path.basename(backup_path)}")

	any_converted = False
	for db_path in db_files:
		if process_db(db_path, dry_run):
			any_converted = True

	if not any_converted:
		print("\nAll databases already have the correct schema. Nothing to convert.")
	elif dry_run:
		print("\nDry run complete. No changes made.")
	else:
		print("\nConversion complete. Backup files saved as *.db.bak")


if __name__ == "__main__":
	main()
