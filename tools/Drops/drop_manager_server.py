"""
drop_manager_server.py

Simple HTTP server for the Drop Manager.
Provides API endpoints to read/write drop data from GameConfigs.db.

Usage: python drop_manager_server.py
Then open http://localhost:8080 in your browser.
"""

import http.server
import socketserver
import json
import sqlite3
from pathlib import Path
from urllib.parse import urlparse, parse_qs
import os

PORT = 8888
DB_PATH = Path(__file__).parent / '../../Binaries/Server/gamedata.db'
CONFIG_PATH = Path(__file__).parent / '../../Binaries/Server/server_config.json'
CHANGELOG_PATH = Path(__file__).parent / 'changelog.txt'

# Maps frontend setting keys to server_config.json paths
_SETTING_TO_CONFIG = {
    "primary-drop-rate": "primary",
    "gold-drop-rate": "gold",
    "secondary-drop-rate": "secondary",
}


class ReuseAddrTCPServer(socketserver.TCPServer):
    allow_reuse_address = True


class DropManagerHandler(http.server.SimpleHTTPRequestHandler):
    
    def __init__(self, *args, **kwargs):
        # Serve files from tools/Drops directory
        super().__init__(*args, directory=str(Path(__file__).parent), **kwargs)
    
    def do_GET(self):
        parsed = urlparse(self.path)

        if parsed.path == '/api/items':
            self.send_json(self.get_items())
        elif parsed.path == '/api/npcs':
            self.send_json(self.get_npcs())
        elif parsed.path == '/api/drops':
            self.send_json(self.get_drops())
        elif parsed.path == '/api/config':
            self.send_json(self.get_config())
        elif parsed.path == '/api/changelog':
            self.send_json(self.get_changelog())
        elif parsed.path == '/':
            self.send_html()
        else:
            super().do_GET()
    
    def do_POST(self):
        parsed = urlparse(self.path)
        content_length = int(self.headers.get('Content-Length', 0))
        body = self.rfile.read(content_length).decode('utf-8')
        data = json.loads(body) if body else {}
        
        if parsed.path == '/api/drops/add':
            result = self.add_drop(data)
            self.send_json(result)
        elif parsed.path == '/api/drops/remove':
            result = self.remove_drop(data)
            self.send_json(result)
        elif parsed.path == '/api/drops/update':
            result = self.update_drop(data)
            self.send_json(result)
        elif parsed.path == '/api/settings/update':
            result = self.update_settings(data)
            self.send_json(result)
        elif parsed.path == '/api/changelog/update':
            result = self.update_changelog(data)
            self.send_json(result)
        else:
            self.send_error(404)
    
    def send_html(self):
        html_path = Path(__file__).parent / 'drop_manager.html'
        with open(html_path, 'r', encoding='utf-8') as f:
            content = f.read()
        self.send_response(200)
        self.send_header('Content-Type', 'text/html; charset=utf-8')
        self.send_header('Cache-Control', 'no-cache, no-store, must-revalidate')
        self.end_headers()
        self.wfile.write(content.encode('utf-8'))

    def send_json(self, data):
        self.send_response(200)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Access-Control-Allow-Origin', '*')
        self.end_headers()
        self.wfile.write(json.dumps(data).encode('utf-8'))
    
    def get_conn(self):
        return sqlite3.connect(str(DB_PATH.resolve()))
    
    def get_items(self):
        try:
            conn = self.get_conn()
            cursor = conn.cursor()
            cursor.execute("SELECT item_id, name FROM items ORDER BY name")
            items = [{"id": row[0], "name": row[1]} for row in cursor.fetchall()]
            conn.close()
            return items
        except sqlite3.OperationalError:
            return []
    
    def get_npcs(self):
        try:
            conn = self.get_conn()
            cursor = conn.cursor()
            cursor.execute("""
                SELECT dt.drop_table_id, dt.name, dt.description
                FROM drop_tables dt
                ORDER BY dt.drop_table_id
            """)
            npcs = []
            for row in cursor.fetchall():
                # Strip 'drops_' prefix for cleaner display
                name = row[1]
                if name.startswith('drops_'):
                    name = name[6:]  # Remove 'drops_' prefix
                npcs.append({"id": row[0], "name": name, "description": row[2]})
            conn.close()
            return npcs
        except sqlite3.OperationalError:
            return []
    
    def get_drops(self):
        try:
            conn = self.get_conn()
            cursor = conn.cursor()
            cursor.execute("""
                SELECT de.drop_table_id, de.tier, de.item_id, de.weight, de.min_count, de.max_count, i.name
                FROM drop_entries de
                LEFT JOIN items i ON de.item_id = i.item_id
                ORDER BY de.drop_table_id, de.tier, de.weight DESC
            """)
            drops = []
            for row in cursor.fetchall():
                item_id = row[2]
                item_name = row[6]
                if item_id == 0:
                    item_name = "Nothing"
                elif not item_name:
                    item_name = f"Unknown({item_id})"

                drops.append({
                    "drop_table_id": row[0],
                    "tier": row[1],
                    "item_id": item_id,
                    "weight": row[3],
                    "min_count": row[4],
                    "max_count": row[5],
                    "item_name": item_name
                })
            conn.close()
            return drops
        except sqlite3.OperationalError:
            return []
    
    def get_config(self):
        """Read drop rate settings from server_config.json."""
        settings = {}
        try:
            with open(str(CONFIG_PATH.resolve()), 'r', encoding='utf-8') as f:
                cfg = json.load(f)
            rates = cfg.get("drop_rates", {})
            for setting_key, config_key in _SETTING_TO_CONFIG.items():
                if config_key in rates:
                    settings[setting_key] = str(rates[config_key])
        except (FileNotFoundError, json.JSONDecodeError):
            pass
        return {"settings": settings, "global_drops": []}
    
    def add_drop(self, data):
        try:
            conn = self.get_conn()
            cursor = conn.cursor()
            cursor.execute("""
                INSERT OR REPLACE INTO drop_entries (drop_table_id, tier, item_id, weight, min_count, max_count)
                VALUES (?, ?, ?, ?, ?, ?)
            """, (data['drop_table_id'], data['tier'], data['item_id'], data['weight'], 
                  data.get('min_count', 1), data.get('max_count', 1)))
            conn.commit()
            
            # Log to changelog
            npc_name = self._get_npc_name(cursor, data['drop_table_id'])
            item_name = self._get_item_name(cursor, data['item_id'])
            conn.close()
            self._log_drop_change(f"Added [{item_name}] to {npc_name} (Tier {data['tier']}, weight {data['weight']})")
            
            return {"success": True}
        except Exception as e:
            return {"success": False, "error": str(e)}
    
    def remove_drop(self, data):
        try:
            conn = self.get_conn()
            cursor = conn.cursor()
            
            # Get names before deleting
            npc_name = self._get_npc_name(cursor, data['drop_table_id'])
            item_name = self._get_item_name(cursor, data['item_id'])
            
            cursor.execute("""
                DELETE FROM drop_entries 
                WHERE drop_table_id = ? AND tier = ? AND item_id = ?
            """, (data['drop_table_id'], data['tier'], data['item_id']))
            conn.commit()
            conn.close()
            
            self._log_drop_change(f"Removed [{item_name}] from {npc_name} (Tier {data['tier']})")
            
            return {"success": True}
        except Exception as e:
            return {"success": False, "error": str(e)}
    
    def update_drop(self, data):
        try:
            conn = self.get_conn()
            cursor = conn.cursor()
            cursor.execute("""
                UPDATE drop_entries 
                SET weight = ?, min_count = ?, max_count = ?
                WHERE drop_table_id = ? AND tier = ? AND item_id = ?
            """, (data['weight'], data.get('min_count', 1), data.get('max_count', 1),
                  data['drop_table_id'], data['tier'], data['item_id']))
            conn.commit()
            conn.close()
            return {"success": True}
        except Exception as e:
            return {"success": False, "error": str(e)}
    
    def update_settings(self, data):
        """Update drop rate settings in server_config.json and log to changelog."""
        try:
            config_file = str(CONFIG_PATH.resolve())
            with open(config_file, 'r', encoding='utf-8') as f:
                cfg = json.load(f)

            rates = cfg.setdefault("drop_rates", {})
            changes = []
            for key, value in data.items():
                config_key = _SETTING_TO_CONFIG.get(key)
                if not config_key:
                    continue
                old_value = rates.get(config_key, 'N/A')
                rates[config_key] = float(value)
                changes.append(f"  {key}: {old_value} -> {value}")

            with open(config_file, 'w', encoding='utf-8') as f:
                json.dump(cfg, f, indent='\t', ensure_ascii=False)
                f.write('\n')

            # Auto-log to changelog
            if changes:
                from datetime import datetime
                timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
                entry = f"[{timestamp}] Settings updated:\n" + "\n".join(changes) + "\n\n"
                self._append_changelog(entry)

            return {"success": True}
        except Exception as e:
            return {"success": False, "error": str(e)}
    
    def get_changelog(self):
        """Read changelog from file"""
        try:
            if CHANGELOG_PATH.exists():
                return {"content": CHANGELOG_PATH.read_text(encoding='utf-8')}
            return {"content": ""}
        except Exception as e:
            return {"content": "", "error": str(e)}
    
    def update_changelog(self, data):
        """Write changelog to file"""
        try:
            CHANGELOG_PATH.write_text(data.get('content', ''), encoding='utf-8')
            return {"success": True}
        except Exception as e:
            return {"success": False, "error": str(e)}
    
    def _append_changelog(self, entry):
        """Append entry to changelog file"""
        try:
            existing = ""
            if CHANGELOG_PATH.exists():
                existing = CHANGELOG_PATH.read_text(encoding='utf-8')
            CHANGELOG_PATH.write_text(entry + existing, encoding='utf-8')
        except Exception:
            pass
    
    def _get_npc_name(self, cursor, drop_table_id):
        """Get NPC/drop table name by ID"""
        cursor.execute("SELECT name FROM drop_tables WHERE drop_table_id = ?", (drop_table_id,))
        row = cursor.fetchone()
        if row:
            name = row[0]
            if name.startswith('drops_'):
                name = name[6:]
            return name
        return f"Table#{drop_table_id}"
    
    def _get_item_name(self, cursor, item_id):
        """Get item name by ID"""
        if item_id == 0:
            return "Nothing"
        cursor.execute("SELECT name FROM items WHERE item_id = ?", (item_id,))
        row = cursor.fetchone()
        return row[0] if row else f"Item#{item_id}"
    
    def _log_drop_change(self, message):
        """Log a drop change with timestamp"""
        from datetime import datetime
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        entry = f"[{timestamp}] {message}\n"
        self._append_changelog(entry)


def main():
    os.chdir(Path(__file__).parent)
    
    with ReuseAddrTCPServer(("", PORT), DropManagerHandler) as httpd:
        print(f"Drop Manager running at http://localhost:{PORT}")
        print("Press Ctrl+C to stop")
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nServer stopped")


if __name__ == "__main__":
    main()
