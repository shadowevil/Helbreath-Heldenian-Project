"""
EXPORT CURRENT DROPS TO RESTORE SCRIPT
=======================================
This script reads the CURRENT state of GameConfigs.db and generates
a restore_all_drops.py script that can recreate that exact state.

Usage: python export_drops_snapshot.py
Output: Creates/overwrites restore_all_drops.py with current data
"""

import sqlite3
import os
from datetime import datetime

DB_PATH = '../../Binaries/Server/gamedata.db'
OUTPUT_PATH = 'restore_all_drops.py'

def export():
    if not os.path.exists(DB_PATH):
        print(f"Error: {DB_PATH} not found.")
        return
    
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()
    
    # Fetch all data
    cursor.execute("SELECT drop_table_id, name, description FROM drop_tables ORDER BY drop_table_id")
    tables = cursor.fetchall()
    
    cursor.execute("SELECT drop_table_id, tier, item_id, weight, min_count, max_count FROM drop_entries ORDER BY drop_table_id, tier, item_id")
    entries = cursor.fetchall()
    
    cursor.execute("SELECT npc_id, name, drop_table_id FROM npc_configs WHERE drop_table_id > 0 ORDER BY npc_id")
    configs = cursor.fetchall()
    
    settings = {}
    try:
        import json as _json
        with open('../../Binaries/Server/server_config.json', 'r') as _f:
            _cfg = _json.load(_f)
        _rates = _cfg.get('drop_rates', {})
        if 'gold' in _rates:
            settings['gold-drop-rate'] = str(_rates['gold'])
        if 'secondary' in _rates:
            settings['secondary-drop-rate'] = str(_rates['secondary'])
    except (FileNotFoundError, KeyError):
        pass
    
    conn.close()
    
    # Generate Python script
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    
    script = f'''"""
MASTER DROP RESTORE SCRIPT
===========================
Auto-generated snapshot from gamedata.db
Generated: {timestamp}

Usage: python restore_all_drops.py
"""

import sqlite3
import os

DB_PATH = '../../Binaries/Server/gamedata.db'

def restore():
    if not os.path.exists(DB_PATH):
        print(f"Error: {{DB_PATH}} not found.")
        return
    
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()
    
    print("Clearing existing drop data...")
    cursor.execute("DELETE FROM drop_entries")
    cursor.execute("DELETE FROM drop_tables")
    
    print("Restoring drop_tables...")
    DROP_TABLES = {repr(tables)}
    
    for tid, name, desc in DROP_TABLES:
        cursor.execute("INSERT OR REPLACE INTO drop_tables (drop_table_id, name, description) VALUES (?, ?, ?)", (tid, name, desc))
    
    print("Restoring drop_entries...")
    DROP_ENTRIES = {repr(entries)}
    
    for entry in DROP_ENTRIES:
        cursor.execute("""
            INSERT OR REPLACE INTO drop_entries (drop_table_id, tier, item_id, weight, min_count, max_count)
            VALUES (?, ?, ?, ?, ?, ?)
        """, entry)
    
    print("Linking NPCs to drop tables...")
    NPC_LINKS = {repr([(c[1], c[2]) for c in configs])}
    
    for npc_name, tid in NPC_LINKS:
        cursor.execute("UPDATE npc_configs SET drop_table_id = ? WHERE name = ?", (tid, npc_name))
    
    print("Setting global rates in server_config.json...")
    try:
        import json as _json
        cfg_path = '../../Binaries/Server/server_config.json'
        with open(cfg_path, 'r') as _f:
            _cfg = _json.load(_f)
        _cfg.setdefault('drop_rates', {{}})['gold'] = {float(settings.get('gold-drop-rate', '3500'))}
        _cfg.setdefault('drop_rates', {{}})['secondary'] = {float(settings.get('secondary-drop-rate', '400'))}
        with open(cfg_path, 'w') as _f:
            _json.dump(_cfg, _f, indent='\\t', ensure_ascii=False)
            _f.write('\\n')
    except FileNotFoundError:
        print("  Warning: server_config.json not found, skipping rate restore")
    
    conn.commit()
    conn.close()
    
    print("\\n✅ ALL DROPS RESTORED SUCCESSFULLY!")
    print(f"   - Drop tables: {{len(DROP_TABLES)}} entries")
    print(f"   - Drop entries: {{len(DROP_ENTRIES)}} entries")
    print(f"   - NPC links: {{len(NPC_LINKS)}} entries")

if __name__ == "__main__":
    restore()
'''
    
    with open(OUTPUT_PATH, 'w', encoding='utf-8') as f:
        f.write(script)
    
    print(f"✅ Snapshot exported to {OUTPUT_PATH}")
    print(f"   - Drop tables: {len(tables)}")
    print(f"   - Drop entries: {len(entries)}")
    print(f"   - NPC configs: {len(configs)}")
    print(f"\nRun 'python {OUTPUT_PATH}' to restore this exact state.")

if __name__ == "__main__":
    export()
