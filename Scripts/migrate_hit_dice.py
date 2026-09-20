"""
Migration: Replace hit_dice with hp_min, hp_max, hold_resist in npc_configs.

Conversion formulas (from old spawn HP logic):
  hit_dice <= 5: hp_min = hit_dice + hit_dice,  hp_max = hit_dice * 4 + hit_dice
  hit_dice >  5: hp_min = hit_dice * 5 + 1,     hp_max = hit_dice * 5 + hit_dice
  hold_resist   = 10 if hit_dice > 50, else 0

Usage:
  python Scripts/migrate_hit_dice.py              # migrate gamedata.db
  python Scripts/migrate_hit_dice.py --dry-run    # preview only
"""

import sqlite3
import json
import sys
import os

DB_PATH = os.path.join(os.path.dirname(__file__), "..", "Binaries", "Server", "gamedata.db")
SNAPSHOT_PATH = os.path.join(os.path.dirname(__file__), "..", "tools", "NpcEditor", "original_npcs.json")


def convert(hit_dice):
	if hit_dice <= 5:
		hp_min = hit_dice + hit_dice
		hp_max = hit_dice * 4 + hit_dice
	else:
		hp_min = hit_dice * 5 + 1
		hp_max = hit_dice * 5 + hit_dice
	hold_resist = 10 if hit_dice > 50 else 0
	return hp_min, hp_max, hold_resist


def has_column(conn, table, column):
	cur = conn.execute(f"PRAGMA table_info({table})")
	return any(row[1] == column for row in cur.fetchall())


def migrate(dry_run=False):
	db_path = os.path.abspath(DB_PATH)
	print(f"Database: {db_path}")
	print(f"Mode: {'DRY RUN' if dry_run else 'APPLY'}")
	print()

	conn = sqlite3.connect(db_path)
	conn.row_factory = sqlite3.Row

	if not has_column(conn, "npc_configs", "hit_dice"):
		if has_column(conn, "npc_configs", "hp_min"):
			print("Already migrated (hp_min exists, hit_dice does not). Nothing to do.")
			conn.close()
			return
		print("ERROR: npc_configs table has neither hit_dice nor hp_min.")
		conn.close()
		sys.exit(1)

	# Read existing data
	rows = conn.execute("SELECT npc_id, name, hit_dice FROM npc_configs ORDER BY npc_id").fetchall()
	print(f"Found {len(rows)} NPC configs to migrate.\n")

	print(f"{'ID':>4}  {'Name':<25}  {'HD':>4}  {'hp_min':>7}  {'hp_max':>7}  {'hold_resist':>12}")
	print("-" * 72)
	for r in rows:
		hp_min, hp_max, hold_resist = convert(r["hit_dice"])
		print(f"{r['npc_id']:>4}  {r['name']:<25}  {r['hit_dice']:>4}  {hp_min:>7}  {hp_max:>7}  {hold_resist:>12}")

	if dry_run:
		print("\nDry run complete. No changes made.")
		conn.close()
		return

	print("\nApplying migration...")

	# Add new columns
	if not has_column(conn, "npc_configs", "hp_min"):
		conn.execute("ALTER TABLE npc_configs ADD COLUMN hp_min INTEGER NOT NULL DEFAULT 0")
	if not has_column(conn, "npc_configs", "hp_max"):
		conn.execute("ALTER TABLE npc_configs ADD COLUMN hp_max INTEGER NOT NULL DEFAULT 0")
	if not has_column(conn, "npc_configs", "hold_resist"):
		conn.execute("ALTER TABLE npc_configs ADD COLUMN hold_resist INTEGER NOT NULL DEFAULT 0")

	# Populate new columns from hit_dice
	for r in rows:
		hp_min, hp_max, hold_resist = convert(r["hit_dice"])
		conn.execute(
			"UPDATE npc_configs SET hp_min = ?, hp_max = ?, hold_resist = ? WHERE npc_id = ?",
			(hp_min, hp_max, hold_resist, r["npc_id"])
		)

	# Rebuild table without hit_dice using SQLite rename+recreate pattern
	# Get full column list minus hit_dice
	conn.commit()
	cols_info = conn.execute("PRAGMA table_info(npc_configs)").fetchall()
	keep_cols = [c[1] for c in cols_info if c[1] != "hit_dice"]

	cols_str = ", ".join(keep_cols)
	conn.execute(f"CREATE TABLE npc_configs_new AS SELECT {cols_str} FROM npc_configs")
	conn.execute("DROP TABLE npc_configs")
	conn.execute("ALTER TABLE npc_configs_new RENAME TO npc_configs")
	conn.commit()

	print(f"Migration complete. Dropped hit_dice column, added hp_min/hp_max/hold_resist.")

	# Update NpcEditor snapshot if it exists
	if os.path.exists(SNAPSHOT_PATH):
		print(f"\nUpdating NpcEditor snapshot: {os.path.abspath(SNAPSHOT_PATH)}")
		with open(SNAPSHOT_PATH, "r") as f:
			snapshot = json.load(f)

		updated = 0
		for npc_id_str, data in snapshot.items():
			if "hit_dice" in data:
				hd = data.pop("hit_dice")
				hp_min, hp_max, hold_resist = convert(hd)
				data["hp_min"] = hp_min
				data["hp_max"] = hp_max
				data["hold_resist"] = hold_resist
				updated += 1

		with open(SNAPSHOT_PATH, "w") as f:
			json.dump(snapshot, f, indent=2)
		print(f"Updated {updated} entries in snapshot.")

	conn.close()
	print("\nDone.")


if __name__ == "__main__":
	dry_run = "--dry-run" in sys.argv
	migrate(dry_run)
