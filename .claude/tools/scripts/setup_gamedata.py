"""
setup_gamedata.py — One-shot script to set up formula tables in gamedata.db
and populate server_config.json from existing DB settings/realmlist data.

Usage:
    python Scripts/setup_gamedata.py                    # dry-run (default)
    python Scripts/setup_gamedata.py --apply            # apply changes
    python Scripts/setup_gamedata.py --apply --keep     # apply but keep old tables

Run from project root (Z:/Helbreath-3.82/).
"""

import argparse
import json
import os
import sqlite3
import sys

DB_PATH = os.path.join("Binaries", "Server", "gamedata.db")
CONFIG_PATH = os.path.join("Binaries", "Server", "server_config.json")


def create_formula_tables(db, dry_run):
    """Drop and recreate formula tables, seed expression-based data."""
    ddl = """
        DROP TABLE IF EXISTS formulas;
        DROP TABLE IF EXISTS formula_terms;
        DROP TABLE IF EXISTS formula_params;
        DROP TABLE IF EXISTS scaling_profiles;

        CREATE TABLE formulas (
            formula_id  TEXT PRIMARY KEY,
            expression  TEXT NOT NULL,
            description TEXT
        );

        CREATE TABLE scaling_profiles (
            profile_id  TEXT NOT NULL,
            bracket_min INTEGER NOT NULL,
            bracket_max INTEGER NOT NULL,
            multiplier  REAL NOT NULL DEFAULT 1.0,
            PRIMARY KEY (profile_id, bracket_min)
        );
    """

    seed = """
        INSERT INTO formulas(formula_id, expression, description) VALUES
            ('max_hp',            'vit * 3 + level * 2 + str * 0.5 + angelic_str * 0.5',                                            'Maximum health points'),
            ('max_mp',            'mag * 2 + angelic_mag * 2 + level * 2 + int * 0.5 + angelic_int * 0.5',                            'Maximum mana points'),
            ('max_sp',            'str * 2 + angelic_str * 2 + level * 2',                                                            'Maximum stamina points'),
            ('max_load',          'str * 5 + angelic_str * 5 + level * 5',                                                            'Maximum carry weight'),
            ('level_exp',         'level * (50 + level * trunc(level / 17) * trunc(level / 17))',                                      'XP cost for a single level'),
            ('hp_regen_max_roll',       'vit',                                'HP regen dice ceiling'),
            ('hp_regen_min_roll',       'vit / 2',                            'HP regen hard floor clamp'),
            ('hp_regen_roll_variance',  '30 * vit / (vit + 60)',              'HP regen bonus variance (diminishing returns)'),
            ('mp_regen_max_roll',       'mag + angelic_mag',                  'MP regen dice ceiling'),
            ('mp_regen_min_roll',       '(mag + angelic_mag) / 2',            'MP regen hard floor clamp'),
            ('mp_regen_roll_variance',  '30 * (mag + angelic_mag) / (mag + angelic_mag + 60)',  'MP regen bonus variance (diminishing returns)'),
            ('sp_regen_max_roll',       'vit / 3',                            'SP regen dice ceiling'),
            ('sp_regen_min_roll',       'vit / 6',                            'SP regen hard floor clamp'),
            ('sp_regen_roll_variance',  '30 * vit / (vit + 180)',             'SP regen bonus variance (diminishing returns)');
    """

    if dry_run:
        print("[DRY-RUN] Would create formula tables and seed data:")
        print("  Tables: formulas, scaling_profiles")
        print("  Formulas: max_hp, max_mp, max_sp, max_load, level_exp, hp_regen_*, mp_regen_*, sp_regen_*")
        return

    db.executescript(ddl)
    db.executescript(seed)
    db.commit()
    print("[OK] Created formula tables and seeded expression-based data")


def migrate_settings_to_json(db, dry_run):
    """Read settings + realmlist from DB, merge into server_config.json."""
    # Check if tables exist (already migrated in prior run)
    tables = {row[0] for row in db.execute("SELECT name FROM sqlite_master WHERE type='table'")}
    if "settings" not in tables:
        print("[SKIP] settings table not found (already migrated)")
        return
    if "realmlist" not in tables:
        print("[SKIP] realmlist table not found (already migrated)")
        return

    # Read current settings
    cursor = db.execute("SELECT key, value FROM settings")
    settings = {row[0]: row[1] for row in cursor}

    # Read realmlist
    cursor = db.execute("SELECT * FROM realmlist LIMIT 1")
    realm_row = cursor.fetchone()
    realm_cols = [desc[0] for desc in cursor.description]
    realm = dict(zip(realm_cols, realm_row)) if realm_row else {}

    # Load existing JSON config
    if os.path.exists(CONFIG_PATH):
        with open(CONFIG_PATH, "r") as f:
            config = json.load(f)
    else:
        config = {}

    def s(key, default):
        return settings.get(key, default)

    # Map settings to JSON structure
    config["drop_rates"] = {
        "primary":      float(s("primary-drop-rate", "1.0")),
        "gold":         float(s("gold-drop-rate", "1.0")),
        "secondary":    float(s("secondary-drop-rate", "1.0")),
        "rep_modifier": int(s("rep-drop-modifier", "0"))
    }
    config["timing"] = {
        "client_timeout_ms":  int(s("client-timeout-ms", "30000")),
        "stamina_regen_ms":   int(s("stamina-regen-interval", "10000")),
        "poison_damage_ms":   int(s("poison-damage-interval", "12000")),
        "health_regen_ms":    int(s("health-regen-interval", "15000")),
        "mana_regen_ms":      int(s("mana-regen-interval", "20000")),
        "hunger_consume_ms":  int(s("hunger-consume-interval", "60000")),
        "summon_duration_ms": int(s("summon-creature-duration", "300000")),
        "autosave_ms":        int(s("autosave-interval", "600000")),
        "lag_protection_ms":  int(s("lag-protection-interval", "7000"))
    }
    config["combat"] = {
        "enemy_kill_mode":   s("enemy-kill-mode", "classic"),
        "enemy_kill_adjust": int(s("enemy-kill-adjust", "0")),
        "slate_success_rate": int(s("slate-success-rate", "50")),
        "min_hit_ratio":     int(s("minimum-hit-ratio", "15")),
        "max_hit_ratio":     int(s("maximum-hit-ratio", "99"))
    }
    config["character"] = {
        "base_stat_value":     int(s("base-stat-value", "10")),
        "creation_stat_bonus": int(s("creation-stat-bonus", "4")),
        "levelup_stat_gain":   int(s("levelup-stat-gain", "3")),
        "max_level":           int(s("max-level", "180")),
        "starting_luck":       10
    }
    config["gameplay"] = {
        "nighttime_duration":       int(s("nighttime-duration", "30")),
        "starting_guild_rank":      int(s("starting-guild-rank", "12")),
        "grand_magic_mana_cost":    int(s("grand-magic-mana-consumption", "15")),
        "max_construction_points":  int(s("maximum-construction-points", "30000")),
        "max_summon_points":        int(s("maximum-summon-points", "30000")),
        "max_war_contribution":     int(s("maximum-war-contribution", "200000")),
        "max_bank_items":           int(s("max-bank-items", "200"))
    }
    config["raid_schedule"] = {
        "monday":    int(s("monday-raid-time", "-1")),
        "tuesday":   int(s("tuesday-raid-time", "-1")),
        "wednesday": int(s("wednesday-raid-time", "-1")),
        "thursday":  int(s("thursday-raid-time", "-1")),
        "friday":    int(s("friday-raid-time", "-1")),
        "saturday":  int(s("saturday-raid-time", "-1")),
        "sunday":    int(s("sunday-raid-time", "-1"))
    }

    # Add realmlist to JSON
    if realm:
        config["realm"] = {
            "name":                 realm.get("realm_name", "Apocalypse"),
            "login_listen_ip":      realm.get("login_listen_ip", "0.0.0.0"),
            "login_listen_port":    realm.get("login_listen_port", 2500),
            "game_listen_ip":       realm.get("game_server_listen_ip", "0.0.0.0"),
            "game_listen_port":     realm.get("game_server_listen_port", 9907),
            "game_connection_ip":   realm.get("game_server_connection_ip", ""),
            "game_connection_port": realm.get("game_server_connection_port", 0)
        }

    if dry_run:
        print("[DRY-RUN] Would write server_config.json:")
        print(json.dumps(config, indent="\t"))
        return

    with open(CONFIG_PATH, "w") as f:
        json.dump(config, f, indent="\t")
        f.write("\n")
    print(f"[OK] Wrote {CONFIG_PATH} from DB settings + realmlist")


def drop_old_tables(db, dry_run, keep):
    """Drop settings and realmlist tables from DB."""
    if keep:
        print("[SKIP] Keeping settings and realmlist tables (--keep)")
        return

    if dry_run:
        print("[DRY-RUN] Would drop tables: settings, realmlist")
        return

    db.execute("DROP TABLE IF EXISTS settings")
    db.execute("DROP TABLE IF EXISTS realmlist")
    db.commit()
    print("[OK] Dropped tables: settings, realmlist")


def main():
    parser = argparse.ArgumentParser(description="Set up formula tables and migrate settings to JSON")
    parser.add_argument("--apply", action="store_true", help="Apply changes (default is dry-run)")
    parser.add_argument("--keep", action="store_true", help="Keep old settings/realmlist tables")
    args = parser.parse_args()

    dry_run = not args.apply

    if not os.path.exists(DB_PATH):
        print(f"ERROR: {DB_PATH} not found. Run from project root.")
        sys.exit(1)

    if dry_run:
        print("=== DRY-RUN MODE (use --apply to execute) ===\n")

    db = sqlite3.connect(DB_PATH)

    try:
        # 1. Export settings + realmlist to server_config.json
        migrate_settings_to_json(db, dry_run)
        print()

        # 2. Create formula tables and seed data
        create_formula_tables(db, dry_run)
        print()

        # 3. Drop old tables
        drop_old_tables(db, dry_run, args.keep)
        print()

        if dry_run:
            print("=== No changes made. Use --apply to execute. ===")
        else:
            print("=== Setup complete ===")

    finally:
        db.close()


if __name__ == "__main__":
    main()
