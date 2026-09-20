"""
Migration: Create unified color_palette table and remap weapon/hair colors.

Changes:
  1. gamedata.db: Create color_palette table with 48 entries (items 0-15, weapons 16-21, hair 32-47)
  2. gamedata.db: Remap weapon item_color in items table (9->14, 4->17, 6->19)
  3. Per-account DBs: Remap character_items + character_bank_items item_color for weapons
  4. Per-account DBs: Remap characters.hair_color and characters.haircolor += 32

Usage:
  python Scripts/migrate_color_palette.py --dry-run    # preview only
  python Scripts/migrate_color_palette.py --verify     # verify after migration
  python Scripts/migrate_color_palette.py              # apply migration
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

# Unified palette entries: (color_id, r, g, b)
PALETTE = [
	# Items (0-15)
	(0, 0, 0, 0),
	(1, 80, 80, 192),
	(2, 144, 144, 112),
	(3, 255, 208, 48),
	(4, 240, 32, 0),
	(5, 16, 112, 16),
	(6, 160, 160, 160),
	(7, 80, 144, 160),
	(8, 240, 96, 176),
	(9, 176, 112, 176),
	(10, 0, 64, 112),
	(11, 235, 215, 180),
	(12, 176, 176, 96),
	(13, 160, 160, 16),
	(14, 144, 16, 16),
	(15, 128, 128, 128),
	# Weapons (16-21)
	(16, 128, 128, 160),
	(17, 128, 192, 128),
	(18, 255, 176, 16),
	(19, 150, 160, 225),
	(20, 255, 255, 255),
	(21, 240, 192, 240),
	# Hair (32-47)
	(32, 180, 80, 60),
	(33, 220, 150, 60),
	(34, 210, 180, 120),
	(35, 80, 170, 80),
	(36, 90, 120, 220),
	(37, 70, 70, 170),
	(38, 160, 80, 175),
	(39, 60, 60, 60),
	(40, 170, 120, 160),
	(41, 180, 160, 110),
	(42, 80, 160, 180),
	(43, 160, 130, 200),
	(44, 200, 120, 110),
	(45, 120, 130, 210),
	(46, 120, 180, 120),
	(47, 150, 155, 110),
]

# Weapon item_color remapping: old_color -> new_color
# Only for items where equip_pos IN (7, 8, 9) — weapons
# Old Weapons[] palette indices 1-9 must remap to unified palette weapon indices 14-21.
# Non-weapon items keep the same indices (they used Items[] which occupies 0-15 in unified palette).
WEAPON_COLOR_REMAP = {
	1: 16,   # Agile prefix (old Weapons[1] LightBlue) -> WeaponLightBlue
	2: 16,   # Light prefix / Custom-Item (old Weapons[2] LightBlue) -> WeaponLightBlue
	3: 16,   # Strong prefix (old Weapons[3] LightBlue) -> WeaponLightBlue
	4: 17,   # Poisoning prefix (old Weapons[4] Green) -> WeaponGreen
	5: 18,   # Critical prefix (old Weapons[5] Critical) -> WeaponCritical
	6: 19,   # Sharp prefix / DK-DM base (old Weapons[6] HeavyBlue) -> WeaponHeavyBlue
	7: 20,   # Righteous prefix (old Weapons[7] White) -> WeaponWhite
	8: 21,   # Ancient prefix (old Weapons[8] Violet) -> WeaponViolet
	9: 14,   # Blood weapons (old Weapons[9] HeavyRed) -> Red (deduped, same RGB)
	# color 11 stays at 11 — Tan is same in both palettes
}

# Weapon equip positions
WEAPON_EQUIP_POS = (7, 8, 9)

log_lines = []

def log(msg):
	print(msg)
	log_lines.append(msg)


def has_table(conn, table):
	cur = conn.execute("SELECT count(*) FROM sqlite_master WHERE type='table' AND name=?", (table,))
	return cur.fetchone()[0] > 0


def get_weapon_item_ids(gamedata_conn):
	"""Get all item_ids that are weapons (equip_pos in 7,8,9) from gamedata."""
	cur = gamedata_conn.execute(
		"SELECT item_id FROM items WHERE equip_pos IN (7, 8, 9)"
	)
	return {row[0] for row in cur.fetchall()}


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

	# Step 1: Create and seed color_palette table
	if has_table(conn, "color_palette"):
		count = conn.execute("SELECT count(*) FROM color_palette").fetchone()[0]
		log(f"color_palette table already exists ({count} entries)")
		if count > 0:
			log("  Skipping palette seed (already populated)")
		else:
			log("  Table empty — will seed")
			if not dry_run:
				for entry in PALETTE:
					conn.execute("INSERT INTO color_palette (color_id, r, g, b) VALUES (?, ?, ?, ?)", entry)
				conn.commit()
				log(f"  Seeded {len(PALETTE)} palette entries")
			else:
				log(f"  [DRY RUN] Would seed {len(PALETTE)} palette entries")
	else:
		log("Creating color_palette table...")
		if not dry_run:
			conn.execute(
				"CREATE TABLE color_palette ("
				" color_id INTEGER PRIMARY KEY,"
				" r INTEGER NOT NULL,"
				" g INTEGER NOT NULL,"
				" b INTEGER NOT NULL"
				")"
			)
			for entry in PALETTE:
				conn.execute("INSERT INTO color_palette (color_id, r, g, b) VALUES (?, ?, ?, ?)", entry)
			conn.commit()
			log(f"  Created and seeded {len(PALETTE)} palette entries")
		else:
			log(f"  [DRY RUN] Would create table and seed {len(PALETTE)} palette entries")

	# Step 2: Remap weapon item_color in items table
	log("\nRemapping weapon item_color in items table...")
	total_remapped = 0
	for old_color, new_color in WEAPON_COLOR_REMAP.items():
		cur = conn.execute(
			"SELECT item_id, name, item_color FROM items WHERE equip_pos IN (7, 8, 9) AND item_color = ?",
			(old_color,)
		)
		rows = cur.fetchall()
		if rows:
			for r in rows:
				log(f"  item_id={r['item_id']:>4} '{r['name']:<30}' color {old_color} -> {new_color}")
			if not dry_run:
				conn.execute(
					"UPDATE items SET item_color = ? WHERE equip_pos IN (7, 8, 9) AND item_color = ?",
					(new_color, old_color)
				)
			total_remapped += len(rows)
		else:
			log(f"  No weapons with color {old_color} found")

	if not dry_run and total_remapped > 0:
		conn.commit()
	log(f"  Total weapon colors remapped: {total_remapped}")

	conn.close()
	return True


def migrate_account_db(db_path, weapon_ids, dry_run):
	"""Migrate a single account database."""
	account_name = os.path.basename(db_path)
	conn = sqlite3.connect(db_path)
	conn.row_factory = sqlite3.Row
	changes = 0

	# Remap weapon item_color in character_items and character_bank_items
	for table in ("character_items", "character_bank_items"):
		if not has_table(conn, table):
			continue

		for old_color, new_color in WEAPON_COLOR_REMAP.items():
			# Build placeholders for weapon item_ids
			if not weapon_ids:
				continue
			placeholders = ",".join("?" * len(weapon_ids))
			cur = conn.execute(
				f"SELECT character_name, slot, item_id, item_color FROM {table} "
				f"WHERE item_id IN ({placeholders}) AND item_color = ?",
				list(weapon_ids) + [old_color]
			)
			rows = cur.fetchall()
			for r in rows:
				log(f"    {account_name} {table}: char='{r['character_name']}' slot={r['slot']} "
					f"item_id={r['item_id']} color {old_color} -> {new_color}")
				changes += 1

			if not dry_run and rows:
				conn.execute(
					f"UPDATE {table} SET item_color = ? "
					f"WHERE item_id IN ({placeholders}) AND item_color = ?",
					[new_color] + list(weapon_ids) + [old_color]
				)

	# Remap hair_color += 32 in characters table
	if has_table(conn, "characters"):
		# Check hair_color column
		cur = conn.execute("PRAGMA table_info(characters)")
		cols = {row[1] for row in cur.fetchall()}

		if "hair_color" in cols:
			cur = conn.execute(
				"SELECT character_name, hair_color FROM characters WHERE hair_color < 32 AND hair_color >= 0"
			)
			rows = cur.fetchall()
			for r in rows:
				new_val = r["hair_color"] + 32
				log(f"    {account_name} characters: '{r['character_name']}' hair_color {r['hair_color']} -> {new_val}")
				changes += 1
			if not dry_run and rows:
				conn.execute("UPDATE characters SET hair_color = hair_color + 32 WHERE hair_color < 32 AND hair_color >= 0")

		if "haircolor" in cols:
			cur = conn.execute(
				"SELECT character_name, haircolor FROM characters WHERE haircolor < 32 AND haircolor >= 0"
			)
			rows = cur.fetchall()
			for r in rows:
				new_val = r["haircolor"] + 32
				log(f"    {account_name} characters: '{r['character_name']}' haircolor {r['haircolor']} -> {new_val}")
				changes += 1
			if not dry_run and rows:
				conn.execute("UPDATE characters SET haircolor = haircolor + 32 WHERE haircolor < 32 AND haircolor >= 0")

	if not dry_run and changes > 0:
		conn.commit()
	conn.close()
	return changes


def migrate_accounts(weapon_ids, dry_run):
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
		changes = migrate_account_db(db_file, weapon_ids, dry_run)
		total_changes += changes

	log(f"\nTotal account changes: {total_changes}")
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

	# Check color_palette exists and has entries
	if not has_table(conn, "color_palette"):
		log("FAIL: color_palette table does not exist")
		errors += 1
	else:
		count = conn.execute("SELECT count(*) FROM color_palette").fetchone()[0]
		if count == len(PALETTE):
			log(f"OK: color_palette has {count} entries")
		else:
			log(f"WARN: color_palette has {count} entries (expected {len(PALETTE)})")

	# Check no weapon items have old color indices
	for old_color in WEAPON_COLOR_REMAP:
		cur = conn.execute(
			"SELECT count(*) FROM items WHERE equip_pos IN (7, 8, 9) AND item_color = ?",
			(old_color,)
		)
		count = cur.fetchone()[0]
		if count > 0:
			log(f"FAIL: {count} weapon(s) still have old color {old_color}")
			errors += 1
		else:
			log(f"OK: No weapons with old color {old_color}")

	conn.close()

	# Verify account databases
	accounts_dir = os.path.abspath(ACCOUNTS_DIR)
	weapon_ids = set()
	if os.path.exists(os.path.abspath(GAMEDATA_PATH)):
		gconn = sqlite3.connect(os.path.abspath(GAMEDATA_PATH))
		weapon_ids = get_weapon_item_ids(gconn)
		gconn.close()

	if os.path.isdir(accounts_dir):
		db_files = sorted(glob.glob(os.path.join(accounts_dir, "*.db")))
		for db_file in db_files:
			account_name = os.path.basename(db_file)
			aconn = sqlite3.connect(db_file)

			# Check hair colors are >= 32
			if has_table(aconn, "characters"):
				cur = aconn.execute("PRAGMA table_info(characters)")
				cols = {row[1] for row in cur.fetchall()}

				for col in ("hair_color", "haircolor"):
					if col not in cols:
						continue
					cur = aconn.execute(f"SELECT count(*) FROM characters WHERE {col} < 32 AND {col} > 0")
					count = cur.fetchone()[0]
					if count > 0:
						log(f"FAIL: {account_name} has {count} character(s) with {col} < 32")
						errors += 1
					else:
						log(f"OK: {account_name} all {col} values >= 32 (or 0)")

			# Check weapon items don't have old color indices
			if weapon_ids:
				for table in ("character_items", "character_bank_items"):
					if not has_table(aconn, table):
						continue
					placeholders = ",".join("?" * len(weapon_ids))
					for old_color in WEAPON_COLOR_REMAP:
						cur = aconn.execute(
							f"SELECT count(*) FROM {table} "
							f"WHERE item_id IN ({placeholders}) AND item_color = ?",
							list(weapon_ids) + [old_color]
						)
						count = cur.fetchone()[0]
						if count > 0:
							log(f"FAIL: {account_name} {table} has {count} weapon(s) with old color {old_color}")
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
		log("=== migrate_color_palette.py [VERIFY] ===")
		ok = verify()
		log_path = os.path.join(OUTPUT_DIR, "migrate_color_palette_verify.log")
		with open(log_path, "w", encoding="utf-8") as f:
			f.write("\n".join(log_lines))
		log(f"\nLog written to: {log_path}")
		sys.exit(0 if ok else 1)

	mode = "DRY RUN" if dry_run else "APPLY"
	log(f"=== migrate_color_palette.py [{mode}] ===")

	# Get weapon item_ids from gamedata for account DB remapping
	if os.path.exists(GAMEDATA_PATH):
		gconn = sqlite3.connect(GAMEDATA_PATH)
		weapon_ids = get_weapon_item_ids(gconn)
		gconn.close()
		log(f"Found {len(weapon_ids)} weapon item_id(s) in gamedata")
	else:
		weapon_ids = set()
		log("WARNING: gamedata.db not found, cannot determine weapon item_ids")

	ok = migrate_gamedata(dry_run)
	if ok:
		migrate_accounts(weapon_ids, dry_run)

	if dry_run:
		log("\nDry run complete. No changes made.")

	log_path = os.path.join(OUTPUT_DIR, f"migrate_color_palette_{'dry_run' if dry_run else 'apply'}.log")
	with open(log_path, "w", encoding="utf-8") as f:
		f.write("\n".join(log_lines))
	log(f"\nLog written to: {log_path}")


if __name__ == "__main__":
	main()
