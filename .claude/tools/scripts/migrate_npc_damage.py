"""
Migrate npc_configs table from dice-based damage to min/max damage.

Renames columns:
  attack_dice_throw -> min_damage
  attack_dice_range -> max_damage

Converts values:
  min_damage = attack_dice_throw          (old dice minimum = throw * 1)
  max_damage = attack_dice_throw * attack_dice_range  (old dice maximum = throw * range)

Usage:
  python Scripts/migrate_npc_damage.py                    # default path
  python Scripts/migrate_npc_damage.py path/to/gamedata.db
  python Scripts/migrate_npc_damage.py --fix              # rebuild schema to match server canonical form
"""

import sqlite3
import sys
import os

DEFAULT_DB = os.path.join(os.path.dirname(__file__), "..", "Binaries", "Server", "gamedata.db")

# Canonical schema from GameConfigSqliteStore.cpp CreateTables().
# This is the single source of truth — column order, types, constraints, and defaults
# must exactly match the server code.
CANONICAL_CREATE = """CREATE TABLE npc_configs (
 npc_id INTEGER PRIMARY KEY,
 name TEXT NOT NULL,
 npc_type INTEGER NOT NULL,
 hp_min INTEGER NOT NULL,
 hp_max INTEGER NOT NULL,
 hold_resist INTEGER NOT NULL DEFAULT 0,
 defense_ratio INTEGER NOT NULL,
 hit_ratio INTEGER NOT NULL,
 min_bravery INTEGER NOT NULL,
 exp_min INTEGER NOT NULL,
 exp_max INTEGER NOT NULL,
 gold_min INTEGER NOT NULL,
 gold_max INTEGER NOT NULL,
 min_damage INTEGER NOT NULL,
 max_damage INTEGER NOT NULL,
 npc_size INTEGER NOT NULL,
 side INTEGER NOT NULL,
 action_limit INTEGER NOT NULL,
 action_time INTEGER NOT NULL,
 resist_magic INTEGER NOT NULL,
 magic_level INTEGER NOT NULL,
 day_of_week_limit INTEGER NOT NULL,
 chat_msg_presence INTEGER NOT NULL,
 target_search_range INTEGER NOT NULL,
 regen_time INTEGER NOT NULL,
 attribute INTEGER NOT NULL,
 abs_damage INTEGER NOT NULL,
 max_mana INTEGER NOT NULL,
 magic_hit_ratio INTEGER NOT NULL,
 attack_range INTEGER NOT NULL,
 drop_table_id INTEGER NOT NULL DEFAULT 0
)"""

# Canonical column names in order (for INSERT ... SELECT)
CANONICAL_COLUMNS = [
    "npc_id", "name", "npc_type", "hp_min", "hp_max", "hold_resist",
    "defense_ratio", "hit_ratio", "min_bravery", "exp_min", "exp_max",
    "gold_min", "gold_max", "min_damage", "max_damage", "npc_size",
    "side", "action_limit", "action_time", "resist_magic", "magic_level",
    "day_of_week_limit", "chat_msg_presence", "target_search_range",
    "regen_time", "attribute", "abs_damage", "max_mana", "magic_hit_ratio",
    "attack_range", "drop_table_id",
]


def rebuild_with_canonical_schema(cur, old_table, col_mapping):
    """
    Rebuild npc_configs using the canonical schema.
    old_table: name of the table to read from (e.g. "npc_configs_old")
    col_mapping: dict mapping canonical column name -> SQL expression to SELECT from old_table
    """
    cur.execute(CANONICAL_CREATE)

    select_parts = [col_mapping.get(col, col) for col in CANONICAL_COLUMNS]
    col_list = ", ".join(CANONICAL_COLUMNS)
    select_list = ", ".join(select_parts)

    cur.execute(f"INSERT INTO npc_configs ({col_list}) SELECT {select_list} FROM {old_table}")


def migrate(db_path: str) -> None:
    db_path = os.path.abspath(db_path)
    if not os.path.isfile(db_path):
        print(f"ERROR: database not found: {db_path}")
        sys.exit(1)

    conn = sqlite3.connect(db_path)
    cur = conn.cursor()

    # Check current columns
    cur.execute("PRAGMA table_info(npc_configs)")
    columns = {row[1] for row in cur.fetchall()}

    if "min_damage" in columns and "max_damage" in columns:
        print("Already migrated — min_damage/max_damage columns exist.")
        print("Use --fix to rebuild schema to canonical form if needed.")
        conn.close()
        return

    if "attack_dice_throw" not in columns or "attack_dice_range" not in columns:
        print("ERROR: expected columns attack_dice_throw/attack_dice_range not found.")
        print(f"  Columns present: {sorted(columns)}")
        conn.close()
        sys.exit(1)

    # Preview conversions
    cur.execute("SELECT npc_id, name, attack_dice_throw, attack_dice_range FROM npc_configs ORDER BY npc_id")
    rows = cur.fetchall()

    print(f"Migrating {len(rows)} NPC configs in {db_path}")
    print(f"{'NPC ID':<8} {'Name':<24} {'throw':<8} {'range':<8} {'min':<8} {'max':<8}")
    print("-" * 64)

    for npc_id, name, throw, rng in rows:
        min_dmg = throw
        max_dmg = throw * rng
        print(f"{npc_id:<8} {name:<24} {throw:<8} {rng:<8} {min_dmg:<8} {max_dmg:<8}")

    # Rename old table, create canonical schema, copy data with conversion
    cur.execute("ALTER TABLE npc_configs RENAME TO npc_configs_old")

    col_mapping = {
        "min_damage": "attack_dice_throw",
        "max_damage": "attack_dice_throw * attack_dice_range",
    }
    rebuild_with_canonical_schema(cur, "npc_configs_old", col_mapping)

    cur.execute("DROP TABLE npc_configs_old")

    conn.commit()

    # Verify
    cur.execute("SELECT COUNT(*) FROM npc_configs")
    count = cur.fetchone()[0]
    conn.close()

    print(f"\nMigration complete. {count} rows converted.")


def fix_schema(db_path: str) -> None:
    """Rebuild npc_configs with canonical schema, preserving all data as-is."""
    db_path = os.path.abspath(db_path)
    if not os.path.isfile(db_path):
        print(f"ERROR: database not found: {db_path}")
        sys.exit(1)

    conn = sqlite3.connect(db_path)
    cur = conn.cursor()

    cur.execute("PRAGMA table_info(npc_configs)")
    columns = {row[1] for row in cur.fetchall()}

    if "min_damage" not in columns or "max_damage" not in columns:
        print("ERROR: table hasn't been migrated yet. Run without --fix first.")
        conn.close()
        sys.exit(1)

    # Verify all canonical columns exist in current table
    canonical_set = set(CANONICAL_COLUMNS)
    missing = canonical_set - columns
    if missing:
        print(f"ERROR: missing columns in current table: {sorted(missing)}")
        conn.close()
        sys.exit(1)

    cur.execute("SELECT COUNT(*) FROM npc_configs")
    row_count = cur.fetchone()[0]

    print(f"Rebuilding schema for {row_count} rows in {db_path}")

    # Rename, recreate with canonical schema, copy data 1:1
    cur.execute("ALTER TABLE npc_configs RENAME TO npc_configs_old")
    rebuild_with_canonical_schema(cur, "npc_configs_old", {})
    cur.execute("DROP TABLE npc_configs_old")

    conn.commit()

    # Verify
    cur.execute("SELECT COUNT(*) FROM npc_configs")
    final_count = cur.fetchone()[0]

    cur.execute("SELECT sql FROM sqlite_master WHERE name='npc_configs'")
    schema = cur.fetchone()[0]
    conn.close()

    if final_count != row_count:
        print(f"ERROR: row count mismatch! Before={row_count}, After={final_count}")
        sys.exit(1)

    print(f"Schema rebuilt. {final_count} rows preserved.")
    print(f"Schema: {schema}")


if __name__ == "__main__":
    args = sys.argv[1:]

    if "--fix" in args:
        args.remove("--fix")
        db = args[0] if args else DEFAULT_DB
        fix_schema(db)
    else:
        db = args[0] if args else DEFAULT_DB
        migrate(db)
