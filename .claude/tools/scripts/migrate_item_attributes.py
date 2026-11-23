"""
Migration: Replace packed m_attribute bitmask with individual attribute columns.

Changes:
  1. gamedata.db: Create 4 new tables (attribute_prefix_types, attribute_secondary_types,
     attribute_pools, attribute_pool_entries) and seed with data.
  2. gamedata.db: Add attribute_pool_id column to items table and populate.
  3. Per-account DBs: Rebuild character_items and character_bank_items tables,
     replacing single 'attribute' column with 6 individual columns.
     Stores raw nibble values (0-15) directly; runtime code applies multipliers.

Usage:
  python Scripts/migrate_item_attributes.py --dry-run    # preview only
  python Scripts/migrate_item_attributes.py --verify     # verify after migration
  python Scripts/migrate_item_attributes.py              # apply migration
"""

import sqlite3
import sys
import os
import glob

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.join(SCRIPT_DIR, "..")
GAMEDATA_PATH = os.path.join(ROOT_DIR, "Binaries", "Server", "gamedata.db")
ACCOUNTS_DIR = os.path.join(ROOT_DIR, "Binaries", "Server", "accounts")
OUTPUT_DIR = os.path.join(SCRIPT_DIR, "output")

# --- Seed Data ---

PREFIX_TYPES = [
	# (prefix_id, name, display_name, effect_label, effect_format, min_value, max_value, weapon_color, multiplier)
	# min/max are final gameplay values (what the player sees). Raw nibble = value / multiplier.
	(0,  "None",           "",                 "",                          "",                                    0,   0, 0,  0),
	(1,  "Critical",       "Critical",         "Critical Hit Damage",       "+{value}",                            1,  13, 18, 1),
	(2,  "Poisoning",      "Poisoning",        "Poison Damage",             "+{value}",                           20,  65, 17, 5),
	(3,  "Righteous",      "Righteous",        "",                          "",                                    0,   0, 20, 0),
	(4,  "Reserved",       "",                 "",                          "",                                    0,   0, 0,  0),
	(5,  "Agile",          "Agile",            "Attack Speed",              "-1",                                  0,   0, 16, 0),
	(6,  "Light",          "Light",            "",                          "{value}% light",                      4,  52, 16, 4),
	(7,  "Sharp",          "Sharp",            "Damage added",              "",                                    1,  13, 19, 1),
	(8,  "Strong",         "Strong",           "Endurance",                 "+{value}%",                          14,  91, 16, 7),
	(9,  "Ancient",        "Ancient",          "Extra Damage added",        "",                                    1,  13, 21, 1),
	(10, "Special",        "Special",          "Magic Casting Probability", "+{value}%",                           3,  39, 5,  3),
	(11, "ManaConverting", "Mana Converting",  "",                          "Replace {value}% damage to mana",     1,   6, 0,  1),
	(12, "CritChance",     "Critical",         "Crit Increase Chance",      "{value}%",                            1,   6, 0,  1),
]

SECONDARY_TYPES = [
	# (secondary_id, name, effect_label, effect_format, min_value, max_value, multiplier)
	# min/max are final gameplay values (what the player sees). Raw nibble = value / multiplier.
	(0,  "None",              "",                          "",          0,  0,  0),
	(1,  "PoisonResistance",  "Poison Resistance",         "+{value}%", 21, 91, 7),
	(2,  "HittingProb",       "Hitting Probability",       "+{value}",  21, 91, 7),
	(3,  "DefenseRatio",      "Defense Ratio",             "+{value}",  21, 91, 7),
	(4,  "HPRecovery",        "HP recovery",               "{value}%",  7, 91,  7),
	(5,  "SPRecovery",        "SP recovery",               "{value}%",  7, 91,  7),
	(6,  "MPRecovery",        "MP recovery",               "{value}%",  7, 91,  7),
	(7,  "MagicResistance",   "Magic Resistance",          "+{value}%", 21, 91, 7),
	(8,  "PhysicalAbsorb",    "Physical Absorption",       "+{value}%", 9, 39,  3),
	(9,  "MagicAbsorb",       "Magic Absorption",          "+{value}%", 9, 39,  3),
	(10, "ConsecutiveAttack", "Consecutive Attack Damage", "+{value}",  1,  7,  1),
	(11, "ExperienceBonus",   "Experience",                "+{value}%", 20, 20, 10),
	(12, "GoldBonus",         "Gold",                      "+{value}%", 50, 50, 10),
]

POOLS = [
	# (pool_id, name, secondary_chance)
	(1, "Standard Melee Weapon", 40),
	(2, "Standard Armor", 40),
	(3, "Wand", 40),
]

POOL_ENTRIES = [
	# (pool_id, is_secondary, type_id, weight)
	(1, 0, 1,  1500),   # Critical
	(1, 0, 2,  1600),   # Poisoning
	(1, 0, 3,  2000),   # Righteous
	(1, 0, 5,  2000),   # Agile
	(1, 0, 6,  299),    # Light
	(1, 0, 7,  1600),   # Sharp
	(1, 0, 8,  700),    # Strong
	(1, 0, 9,  301),    # Ancient
	(1, 1, 2,  4999),   # HittingProb
	(1, 1, 10, 3500),   # ConsecutiveAttack
	(1, 1, 11, 501),    # ExperienceBonus
	(1, 1, 12, 1000),   # GoldBonus
	(2, 0, 6,  3000),   # Light
	(2, 0, 8,  5999),   # Strong
	(2, 0, 11, 555),    # ManaConverting
	(2, 0, 12, 446),    # CritChance
	(2, 1, 1,  3000),   # PoisonResistance
	(2, 1, 3,  1000),   # DefenseRatio
	(2, 1, 4,  1000),   # HPRecovery
	(2, 1, 5,  1500),   # SPRecovery
	(2, 1, 6,  1000),   # MPRecovery
	(2, 1, 7,  1900),   # MagicResistance
	(2, 1, 8,  400),    # PhysicalAbsorb
	(2, 1, 9,  201),    # MagicAbsorb
	(3, 0, 10, 10000),  # Special
	(3, 1, 2,  4999),   # HittingProb
	(3, 1, 10, 3500),   # ConsecutiveAttack
	(3, 1, 11, 501),    # ExperienceBonus
	(3, 1, 12, 1000),   # GoldBonus
]

# item_effect_type values that map to each pool
# Attack=1, AttackArrow=3, AttackMaxHPDown=19, AttackDefense=20, AttackSpecAbility=24
ATTACK_EFFECT_TYPES = (1, 3, 19, 20, 24)
DEFENSE_EFFECT_TYPE = 2
WAND_EFFECT_TYPE = 13  # AttackManaSave

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


def unpack_attribute(attr):
	"""Unpack old bitmask attribute into individual fields as raw nibbles (0-15).
	Runtime code applies multipliers at point of use (calc_total_item_effect, display)."""
	# Ensure unsigned 32-bit interpretation (SQLite may store as signed)
	attr = attr & 0xFFFFFFFF
	custom_made = (attr & 0x00000001)
	prefix_type = (attr >> 20) & 0x0F
	prefix_value = (attr >> 16) & 0x0F
	secondary_type = (attr >> 12) & 0x0F
	secondary_value = (attr >> 8) & 0x0F
	enchant_bonus = (attr >> 28) & 0x0F

	return (custom_made, prefix_type, prefix_value, secondary_type, secondary_value, enchant_bonus)


def migrate_gamedata(dry_run):
	db_path = os.path.abspath(GAMEDATA_PATH)
	log(f"\n{'='*60}")
	log(f"gamedata.db: {db_path}")
	log(f"{'='*60}")

	if not os.path.exists(db_path):
		log("ERROR: gamedata.db not found!")
		return False

	conn = sqlite3.connect(db_path)
	conn.row_factory = sqlite3.Row

	# Step 1a: Create and seed attribute_prefix_types
	_create_and_seed_table(conn, "attribute_prefix_types",
		"CREATE TABLE attribute_prefix_types ("
		" prefix_id INTEGER PRIMARY KEY,"
		" name TEXT NOT NULL,"
		" display_name TEXT NOT NULL,"
		" effect_label TEXT NOT NULL,"
		" effect_format TEXT NOT NULL,"
		" min_value INTEGER NOT NULL DEFAULT 0,"
		" max_value INTEGER NOT NULL DEFAULT 0,"
		" weapon_color INTEGER NOT NULL DEFAULT 0,"
		" multiplier INTEGER NOT NULL DEFAULT 1"
		")",
		"INSERT INTO attribute_prefix_types (prefix_id, name, display_name, effect_label, effect_format, min_value, max_value, weapon_color, multiplier) VALUES (?,?,?,?,?,?,?,?,?)",
		PREFIX_TYPES, dry_run)

	# Step 1a: Create and seed attribute_secondary_types
	_create_and_seed_table(conn, "attribute_secondary_types",
		"CREATE TABLE attribute_secondary_types ("
		" secondary_id INTEGER PRIMARY KEY,"
		" name TEXT NOT NULL,"
		" effect_label TEXT NOT NULL,"
		" effect_format TEXT NOT NULL,"
		" min_value INTEGER NOT NULL DEFAULT 0,"
		" max_value INTEGER NOT NULL DEFAULT 0,"
		" multiplier INTEGER NOT NULL DEFAULT 1"
		")",
		"INSERT INTO attribute_secondary_types (secondary_id, name, effect_label, effect_format, min_value, max_value, multiplier) VALUES (?,?,?,?,?,?,?)",
		SECONDARY_TYPES, dry_run)

	# Migrate existing tables: add multiplier column if missing
	_add_multiplier_column(conn, "attribute_prefix_types", "prefix_id", PREFIX_TYPES, 8, dry_run)
	_add_multiplier_column(conn, "attribute_secondary_types", "secondary_id", SECONDARY_TYPES, 6, dry_run)

	# Step 1a: Create and seed attribute_pools
	_create_and_seed_table(conn, "attribute_pools",
		"CREATE TABLE attribute_pools ("
		" pool_id INTEGER PRIMARY KEY,"
		" name TEXT NOT NULL,"
		" secondary_chance INTEGER NOT NULL DEFAULT 40"
		")",
		"INSERT INTO attribute_pools (pool_id, name, secondary_chance) VALUES (?,?,?)",
		POOLS, dry_run)

	# Step 1a: Create and seed attribute_pool_entries
	_create_and_seed_table(conn, "attribute_pool_entries",
		"CREATE TABLE attribute_pool_entries ("
		" pool_id INTEGER NOT NULL,"
		" is_secondary INTEGER NOT NULL,"
		" type_id INTEGER NOT NULL,"
		" weight INTEGER NOT NULL,"
		" PRIMARY KEY (pool_id, is_secondary, type_id),"
		" FOREIGN KEY (pool_id) REFERENCES attribute_pools(pool_id)"
		")",
		"INSERT INTO attribute_pool_entries (pool_id, is_secondary, type_id, weight) VALUES (?,?,?,?)",
		POOL_ENTRIES, dry_run)

	# Step 1b: Add attribute_pool_id to items table
	if has_column(conn, "items", "attribute_pool_id"):
		log("\nitems.attribute_pool_id column already exists")
	else:
		log("\nAdding attribute_pool_id column to items table...")
		if not dry_run:
			conn.execute("ALTER TABLE items ADD COLUMN attribute_pool_id INTEGER DEFAULT NULL")
			conn.commit()
			log("  Column added")
		else:
			log("  [DRY RUN] Would add attribute_pool_id column")

	# Populate attribute_pool_id based on item_effect_type
	log("\nPopulating attribute_pool_id based on item_effect_type...")
	placeholders = ",".join("?" * len(ATTACK_EFFECT_TYPES))

	# In dry-run mode, the column may not exist yet. Query by item_effect_type counts instead.
	col_exists = has_column(conn, "items", "attribute_pool_id")

	# Attack variants -> pool 1
	cur = conn.execute(
		f"SELECT count(*) FROM items WHERE item_effect_type IN ({placeholders})",
		ATTACK_EFFECT_TYPES
	)
	attack_count = cur.fetchone()[0]
	log(f"  Attack variants -> pool 1: {attack_count} items")
	if not dry_run and col_exists and attack_count > 0:
		conn.execute(
			f"UPDATE items SET attribute_pool_id = 1 WHERE item_effect_type IN ({placeholders})",
			ATTACK_EFFECT_TYPES
		)

	# Defense -> pool 2
	cur = conn.execute(
		"SELECT count(*) FROM items WHERE item_effect_type = ?",
		(DEFENSE_EFFECT_TYPE,)
	)
	defense_count = cur.fetchone()[0]
	log(f"  Defense -> pool 2: {defense_count} items")
	if not dry_run and col_exists and defense_count > 0:
		conn.execute("UPDATE items SET attribute_pool_id = 2 WHERE item_effect_type = ?", (DEFENSE_EFFECT_TYPE,))

	# AttackManaSave -> pool 3 (Wand)
	cur = conn.execute(
		"SELECT count(*) FROM items WHERE item_effect_type = ?",
		(WAND_EFFECT_TYPE,)
	)
	wand_count = cur.fetchone()[0]
	log(f"  Wand (AttackManaSave) -> pool 3: {wand_count} items")
	if not dry_run and col_exists and wand_count > 0:
		conn.execute("UPDATE items SET attribute_pool_id = 3 WHERE item_effect_type = ?", (WAND_EFFECT_TYPE,))

	if not dry_run:
		conn.commit()

	conn.close()
	return True


def _create_and_seed_table(conn, table_name, create_sql, insert_sql, data, dry_run):
	if has_table(conn, table_name):
		count = conn.execute(f"SELECT count(*) FROM {table_name}").fetchone()[0]
		log(f"\n{table_name} table already exists ({count} entries)")
		if count > 0:
			log("  Skipping seed (already populated)")
			return
		else:
			log("  Table empty - will seed")
			if not dry_run:
				for row in data:
					conn.execute(insert_sql, row)
				conn.commit()
				log(f"  Seeded {len(data)} entries")
			else:
				log(f"  [DRY RUN] Would seed {len(data)} entries")
	else:
		log(f"\nCreating {table_name} table...")
		if not dry_run:
			conn.execute(create_sql)
			for row in data:
				conn.execute(insert_sql, row)
			conn.commit()
			log(f"  Created and seeded {len(data)} entries")
		else:
			log(f"  [DRY RUN] Would create table and seed {len(data)} entries")


def _add_multiplier_column(conn, table_name, id_col, seed_data, multiplier_idx, dry_run):
	"""Add multiplier column to an existing table if it's missing, and populate from seed data."""
	if not has_table(conn, table_name):
		return  # Table doesn't exist yet — _create_and_seed_table will handle it
	if has_column(conn, table_name, "multiplier"):
		return  # Already has the column

	log(f"\nAdding multiplier column to {table_name}...")
	if dry_run:
		log(f"  [DRY RUN] Would add multiplier column and update values")
		return

	conn.execute(f"ALTER TABLE {table_name} ADD COLUMN multiplier INTEGER NOT NULL DEFAULT 1")
	for row in seed_data:
		type_id = row[0]
		multiplier = row[multiplier_idx]
		conn.execute(f"UPDATE {table_name} SET multiplier = ? WHERE {id_col} = ?", (multiplier, type_id))

	# Also update min/max to pre-multiplied gameplay values
	for row in seed_data:
		type_id = row[0]
		min_val = row[multiplier_idx - 2]  # min_value is 2 positions before multiplier
		max_val = row[multiplier_idx - 1]  # max_value is 1 position before multiplier
		conn.execute(f"UPDATE {table_name} SET min_value = ?, max_value = ? WHERE {id_col} = ?",
			(min_val, max_val, type_id))

	conn.commit()
	log(f"  Added multiplier column and updated {len(seed_data)} entries")


def migrate_account_db(db_path, dry_run):
	"""Migrate a single account database: replace attribute column with 6 individual columns."""
	account_name = os.path.basename(db_path)
	conn = sqlite3.connect(db_path)
	conn.row_factory = sqlite3.Row
	changes = 0

	for table in ("character_items", "character_bank_items"):
		if not has_table(conn, table):
			continue

		# Check if already migrated (has prefix_type column)
		if has_column(conn, table, "prefix_type"):
			log(f"  {account_name} {table}: already migrated (has prefix_type column)")
			continue

		# Check if old attribute column exists
		if not has_column(conn, table, "attribute"):
			log(f"  {account_name} {table}: no attribute column found, skipping")
			continue

		# Read all existing data
		cur = conn.execute(f"SELECT * FROM {table}")
		rows = cur.fetchall()
		col_names = [desc[0] for desc in cur.description]

		log(f"  {account_name} {table}: {len(rows)} rows to migrate")

		# Determine new column list (replace 'attribute' with 6 columns)
		new_cols = []
		for c in col_names:
			if c == "attribute":
				new_cols.extend(["custom_made", "prefix_type", "prefix_value",
								"secondary_type", "secondary_value", "enchant_bonus"])
			else:
				new_cols.append(c)

		# Build new CREATE TABLE SQL based on original schema, replacing attribute column
		original_sql = conn.execute(
			"SELECT sql FROM sqlite_master WHERE type='table' AND name=?", (table,)
		).fetchone()[0]

		# Build the new table SQL
		if table == "character_items":
			new_create = (
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
			)
		else:  # character_bank_items
			new_create = (
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
			)

		if dry_run:
			# Preview the conversions
			for row in rows:
				attr = row["attribute"]
				cm, pt, pv, st, sv, eb = unpack_attribute(attr)
				if attr != 0:
					attr_u = attr & 0xFFFFFFFF
					log(f"    {row['character_name']} slot={row['slot']} item={row['item_id']}: "
						f"attr=0x{attr_u:08X} -> cm={cm} pt={pt} pv={pv} st={st} sv={sv} eb={eb}")
					changes += 1
		else:
			# Create new table
			conn.execute(new_create)

			# Convert and insert rows
			for row in rows:
				attr = row["attribute"]
				cm, pt, pv, st, sv, eb = unpack_attribute(attr)

				# Build values for new row
				new_values = []
				for c in col_names:
					if c == "attribute":
						new_values.extend([cm, pt, pv, st, sv, eb])
					else:
						new_values.append(row[c])

				placeholders = ",".join("?" * len(new_values))
				col_list = ",".join(new_cols)
				conn.execute(f"INSERT INTO {table}_new ({col_list}) VALUES ({placeholders})", new_values)

				if attr != 0:
					changes += 1

			# Drop old table, rename new
			conn.execute(f"DROP TABLE {table}")
			conn.execute(f"ALTER TABLE {table}_new RENAME TO {table}")
			conn.commit()

	conn.close()
	return changes


def migrate_accounts(dry_run):
	accounts_dir = os.path.abspath(ACCOUNTS_DIR)
	log(f"\n{'='*60}")
	log(f"Account databases: {accounts_dir}")
	log(f"{'='*60}")

	if not os.path.isdir(accounts_dir):
		log("No accounts directory found. Skipping.")
		return True

	db_files = sorted(glob.glob(os.path.join(accounts_dir, "*.db")))
	if not db_files:
		log("No account databases found.")
		return True

	log(f"Found {len(db_files)} account database(s)")
	total_changes = 0
	for db_file in db_files:
		changes = migrate_account_db(db_file, dry_run)
		total_changes += changes

	log(f"\nTotal non-zero attributes converted: {total_changes}")
	return True


def verify():
	"""Verify migration was applied correctly."""
	log("\n=== VERIFICATION ===\n")
	errors = 0

	# Verify gamedata.db
	db_path = os.path.abspath(GAMEDATA_PATH)
	if not os.path.exists(db_path):
		log("ERROR: gamedata.db not found!")
		return False

	conn = sqlite3.connect(db_path)

	# Check new tables exist with correct row counts
	table_counts = {
		"attribute_prefix_types": len(PREFIX_TYPES),
		"attribute_secondary_types": len(SECONDARY_TYPES),
		"attribute_pools": len(POOLS),
		"attribute_pool_entries": len(POOL_ENTRIES),
	}
	for table, expected in table_counts.items():
		if not has_table(conn, table):
			log(f"FAIL: {table} table does not exist")
			errors += 1
		else:
			count = conn.execute(f"SELECT count(*) FROM {table}").fetchone()[0]
			if count == expected:
				log(f"OK: {table} has {count} entries")
			else:
				log(f"WARN: {table} has {count} entries (expected {expected})")

	# Check attribute_pool_id column exists
	if has_column(conn, "items", "attribute_pool_id"):
		log("OK: items.attribute_pool_id column exists")

		# Check pool assignments
		attack_ph = ",".join("?" * len(ATTACK_EFFECT_TYPES))
		cur = conn.execute(
			f"SELECT count(*) FROM items WHERE item_effect_type IN ({attack_ph}) AND attribute_pool_id = 1",
			ATTACK_EFFECT_TYPES
		)
		log(f"  Attack items with pool_id=1: {cur.fetchone()[0]}")

		cur = conn.execute("SELECT count(*) FROM items WHERE item_effect_type = ? AND attribute_pool_id = 2", (DEFENSE_EFFECT_TYPE,))
		log(f"  Defense items with pool_id=2: {cur.fetchone()[0]}")

		cur = conn.execute("SELECT count(*) FROM items WHERE item_effect_type = ? AND attribute_pool_id = 3", (WAND_EFFECT_TYPE,))
		log(f"  Wand items with pool_id=3: {cur.fetchone()[0]}")

		cur = conn.execute("SELECT count(*) FROM items WHERE attribute_pool_id IS NULL")
		log(f"  Items with no pool: {cur.fetchone()[0]}")
	else:
		log("FAIL: items.attribute_pool_id column does not exist")
		errors += 1

	conn.close()

	# Verify account databases
	accounts_dir = os.path.abspath(ACCOUNTS_DIR)
	if os.path.isdir(accounts_dir):
		db_files = sorted(glob.glob(os.path.join(accounts_dir, "*.db")))
		for db_file in db_files:
			account_name = os.path.basename(db_file)
			aconn = sqlite3.connect(db_file)

			for table in ("character_items", "character_bank_items"):
				if not has_table(aconn, table):
					continue

				# Check that attribute column is gone and new columns exist
				if has_column(aconn, table, "attribute"):
					log(f"FAIL: {account_name} {table} still has old 'attribute' column")
					errors += 1
				else:
					log(f"OK: {account_name} {table} no longer has 'attribute' column")

				new_columns = ["custom_made", "prefix_type", "prefix_value",
							   "secondary_type", "secondary_value", "enchant_bonus"]
				for col in new_columns:
					if has_column(aconn, table, col):
						log(f"OK: {account_name} {table}.{col} exists")
					else:
						log(f"FAIL: {account_name} {table}.{col} missing")
						errors += 1

				# Spot-check: all values should be in valid ranges
				if has_column(aconn, table, "prefix_type"):
					cur = aconn.execute(f"SELECT count(*) FROM {table} WHERE prefix_type < 0 OR prefix_type > 12")
					bad = cur.fetchone()[0]
					if bad > 0:
						log(f"FAIL: {account_name} {table} has {bad} rows with invalid prefix_type")
						errors += 1

					cur = aconn.execute(f"SELECT count(*) FROM {table} WHERE enchant_bonus < 0 OR enchant_bonus > 15")
					bad = cur.fetchone()[0]
					if bad > 0:
						log(f"FAIL: {account_name} {table} has {bad} rows with invalid enchant_bonus")
						errors += 1

			aconn.close()

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
		log("=== migrate_item_attributes.py [VERIFY] ===")
		ok = verify()
		log_path = os.path.join(OUTPUT_DIR, "migrate_item_attributes_verify.log")
		with open(log_path, "w", encoding="utf-8") as f:
			f.write("\n".join(log_lines))
		log(f"\nLog written to: {log_path}")
		sys.exit(0 if ok else 1)

	mode = "DRY RUN" if dry_run else "APPLY"
	log(f"=== migrate_item_attributes.py [{mode}] ===")

	ok = migrate_gamedata(dry_run)
	if ok:
		migrate_accounts(dry_run)

	if dry_run:
		log("\nDry run complete. No changes made.")

	log_path = os.path.join(OUTPUT_DIR, f"migrate_item_attributes_{'dry_run' if dry_run else 'apply'}.log")
	with open(log_path, "w", encoding="utf-8") as f:
		f.write("\n".join(log_lines))
	log(f"\nLog written to: {log_path}")


if __name__ == "__main__":
	main()
