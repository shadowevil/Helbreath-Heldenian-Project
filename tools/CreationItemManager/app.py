"""
Creation Item Manager Tool for Helbreath GameConfigs.db
Hosts a web interface to manage character creation starting items per class.

Usage: python app.py
Then open http://localhost:8084 in your browser.
"""

import sqlite3
import os
import json
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse

DB_PATH = os.path.join(os.path.dirname(__file__), "..", "..", "Binaries", "Server", "gamedata.db")

CLASS_NAMES = {0: "All Classes", 1: "Warrior", 2: "Mage", 3: "Master"}

def get_db():
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    return conn

def ensure_table():
    """Create the character_creation_items table if it doesn't exist."""
    conn = get_db()
    conn.execute(
        "CREATE TABLE IF NOT EXISTS character_creation_items ("
        " class_type INTEGER NOT NULL,"
        " item_id INTEGER NOT NULL,"
        " count INTEGER NOT NULL DEFAULT 1,"
        " item_color INTEGER NOT NULL DEFAULT 0,"
        " lifespan INTEGER NOT NULL DEFAULT 0,"
        " is_equipped INTEGER NOT NULL DEFAULT 0,"
        " gender_limit INTEGER NOT NULL DEFAULT 0,"
        " sort_order INTEGER NOT NULL DEFAULT 0,"
        " PRIMARY KEY (class_type, item_id, gender_limit)"
        ")"
    )
    conn.commit()
    conn.close()

HTML_PAGE = r"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Creation Item Manager</title>
<style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
        font-family: 'Segoe UI', Tahoma, sans-serif;
        background: #181620;
        color: #ddd8d0;
        min-height: 100vh;
    }
    .navbar {
        background: linear-gradient(to right, #7a5c34, #3a3248);
        padding: 12px 24px;
        border-bottom: 2px solid #3a3248;
        display: flex;
        align-items: center;
        gap: 16px;
        position: sticky;
        top: 0;
        z-index: 100;
        flex-shrink: 0;
    }
    .navbar h1 {
        font-size: 20px;
        color: #fff;
        white-space: nowrap;
    }
    .navbar-actions { display: flex; gap: 8px; align-items: center; margin-left: auto; }
    .btn {
        padding: 8px 16px;
        border: none;
        border-radius: 4px;
        cursor: pointer;
        font-size: 13px;
        font-weight: 600;
        transition: background 0.2s;
    }
    .btn-primary { background: #c8a850; color: #181620; }
    .btn-primary:hover { background: #b89840; }
    .btn-save { background: #6e5530; color: #fff; }
    .btn-save:hover { background: #7e6538; }
    .btn-danger { background: #8b2828; color: #fff; }
    .btn-danger:hover { background: #a03030; }
    .btn-small { font-size: 11px; padding: 4px 10px; }
    .btn-icon {
        background: transparent;
        border: 1px solid #332e3a;
        color: #887868;
        width: 26px;
        height: 26px;
        padding: 0;
        font-size: 14px;
        line-height: 24px;
        text-align: center;
        border-radius: 3px;
        cursor: pointer;
    }
    .btn-icon:hover { background: #3a3248; color: #ddd8d0; }
    .btn-icon:disabled { opacity: 0.25; cursor: default; }
    .btn-icon:disabled:hover { background: transparent; color: #887868; }
    .btn-outline {
        background: transparent;
        border: 1px solid #3a3248;
        color: #ddd8d0;
    }
    .btn-outline:hover { background: #3a3248; }
    .status {
        padding: 8px 16px;
        font-size: 13px;
        text-align: center;
        display: none;
    }
    .status.success { display: block; background: #2e2818; color: #d8ccb0; }
    .status.error { display: block; background: #3a1818; color: #e0c0c0; }
    .container {
        display: flex;
        margin: 0 auto;
        padding: 16px;
        gap: 16px;
        height: calc(100vh - 57px);
    }
    .panel {
        background: #22202c;
        border: 1px solid #3a3248;
        border-radius: 6px;
        overflow: hidden;
        display: flex;
        flex-direction: column;
    }
    .panel-header {
        padding: 12px 16px;
        background: #3a3248;
        font-size: 14px;
        font-weight: 600;
        display: flex;
        justify-content: space-between;
        align-items: center;
        flex-shrink: 0;
    }
    .panel-body { padding: 12px; overflow-y: auto; flex: 1; }

    /* Class list (left panel) */
    .class-list-panel { width: 200px; flex-shrink: 0; }
    .class-list { list-style: none; }
    .class-list li {
        padding: 12px 14px;
        cursor: pointer;
        border-bottom: 1px solid #181620;
        transition: background 0.15s;
    }
    .class-list li:hover { background: #181620; }
    .class-list li.selected { background: #181620; border-left: 3px solid #c8a850; }
    .class-list li .class-name { font-size: 14px; font-weight: 600; }
    .class-list li .class-count { color: #686058; font-size: 12px; margin-top: 2px; }
    .class-list li .class-icon {
        display: inline-block;
        width: 10px;
        height: 10px;
        border-radius: 50%;
        margin-right: 6px;
    }
    .icon-all { background: #888; }
    .icon-warrior { background: #c85050; }
    .icon-mage { background: #5080d0; }
    .icon-master { background: #c8a850; }

    /* Detail (middle) panel */
    .detail-panel { flex: 1; min-width: 0; }
    .detail-empty {
        text-align: center;
        padding: 60px 20px;
        color: #686058;
        font-size: 15px;
    }
    .section { margin-bottom: 16px; }
    .section-title {
        font-size: 13px;
        text-transform: uppercase;
        color: #887868;
        margin-bottom: 8px;
        padding-bottom: 4px;
        border-bottom: 1px solid #2e2838;
    }

    /* Items filter */
    #detailContent.visible {
        display: flex;
        flex: 1;
        flex-direction: column;
    }

    .items-filter {
        padding: 6px 10px;
        background: #181620;
        border: 1px solid #332e3a;
        color: #ddd8d0;
        border-radius: 4px;
        font-size: 13px;
        margin-bottom: 8px;
        width: 100%;
    }
    .items-filter:focus { outline: none; border-color: #c8a850; }
    .items-filter::placeholder { color: #686058; }
    .items-scroll { overflow-y: auto; flex: 1; min-height: 0; }

    /* Items table */
    .items-table { width: 100%; border-collapse: collapse; }
    .items-table th {
        text-align: left;
        font-size: 11px;
        text-transform: uppercase;
        color: #887868;
        padding: 6px 8px;
        border-bottom: 1px solid #2e2838;
        position: sticky;
        top: 0;
        background: #22202c;
        z-index: 1;
    }
    .items-table td { padding: 4px 8px; font-size: 13px; vertical-align: middle; }
    .items-table tr:hover { background: #181620; }
    .items-table .col-order { width: 36px; color: #686058; text-align: center; }
    .items-table .col-id { width: 50px; color: #887868; }
    .items-table .col-name { }
    .items-table .col-count { width: 55px; }
    .items-table .col-color { width: 55px; }
    .items-table .col-lifespan { width: 70px; }
    .items-table .col-equip { width: 50px; text-align: center; }
    .items-table .col-gender { width: 80px; }
    .items-table .col-actions { width: 90px; text-align: right; white-space: nowrap; }
    .items-table .col-actions .btn-icon { margin-left: 2px; }
    .items-table tr.dragging { opacity: 0.4; }
    .items-table tr.drag-over td { border-top: 2px solid #c8a850; }

    .inline-input {
        width: 100%;
        padding: 3px 6px;
        background: #181620;
        border: 1px solid #332e3a;
        color: #ddd8d0;
        border-radius: 3px;
        font-size: 12px;
        text-align: center;
    }
    .inline-input:focus { outline: none; border-color: #c8a850; }
    .inline-select {
        width: 100%;
        padding: 3px 4px;
        background: #181620;
        border: 1px solid #332e3a;
        color: #ddd8d0;
        border-radius: 3px;
        font-size: 12px;
        cursor: pointer;
    }
    .inline-select:focus { outline: none; border-color: #c8a850; }
    .inline-checkbox {
        width: 16px;
        height: 16px;
        cursor: pointer;
        accent-color: #c8a850;
    }

    /* Right panel: all items catalog */
    .catalog-panel { width: 300px; flex-shrink: 0; }
    .catalog-search {
        padding: 8px 12px;
        background: #181620;
        border: none;
        border-bottom: 1px solid #3a3248;
        color: #ddd8d0;
        font-size: 13px;
        width: 100%;
    }
    .catalog-search:focus { outline: none; }
    .catalog-search::placeholder { color: #686058; }
    .catalog-list { list-style: none; }
    .catalog-list li {
        padding: 6px 12px;
        cursor: pointer;
        border-bottom: 1px solid #1a1826;
        font-size: 13px;
        display: flex;
        align-items: center;
        gap: 8px;
        transition: background 0.1s;
    }
    .catalog-list li:hover { background: #181620; }
    .catalog-list li.in-class { opacity: 0.35; }
    .catalog-list li .cat-id { color: #887868; font-size: 11px; min-width: 40px; }
    .catalog-list li .cat-name { flex: 1; }
    .catalog-list li .cat-add {
        color: #6e5530;
        font-weight: 700;
        font-size: 16px;
        line-height: 1;
        opacity: 0;
        transition: opacity 0.1s;
    }
    .catalog-list li:hover .cat-add { opacity: 1; }
    .catalog-list li.in-class .cat-add { display: none; }
</style>
</head>
<body>
<nav class="navbar">
    <h1>Creation Item Manager</h1>
    <div class="navbar-actions">
        <span style="color:#887868;font-size:12px" id="totalCount"></span>
    </div>
</nav>
<div class="status" id="statusBar"></div>

<div class="container">
    <!-- Left: Class list -->
    <div class="panel class-list-panel">
        <div class="panel-header">Classes</div>
        <div class="panel-body" style="padding:0">
            <ul class="class-list" id="classList"></ul>
        </div>
    </div>

    <!-- Middle: Items for selected class -->
    <div class="panel detail-panel" id="detailPanel">
        <div class="panel-header" id="detailHeader">Creation Items</div>
        <div class="panel-body" style="display:flex;flex-direction:column">
            <div class="detail-empty" id="detailEmpty">Select a class from the list</div>
            <div id="detailContent" style="display:none">
                <input type="text" class="items-filter" id="itemsFilter"
                    placeholder="Filter items..." oninput="renderItems()">
                <div class="items-scroll">
                <table class="items-table" id="itemsTable">
                    <thead><tr>
                        <th class="col-order">#</th>
                        <th class="col-id">ID</th>
                        <th class="col-name">Name</th>
                        <th class="col-count">Count</th>
                        <th class="col-color">Color</th>
                        <th class="col-lifespan">Lifespan</th>
                        <th class="col-equip">Equip</th>
                        <th class="col-gender">Gender</th>
                        <th class="col-actions"></th>
                    </tr></thead>
                    <tbody id="itemsBody"></tbody>
                </table>
                </div>
            </div>
        </div>
    </div>

    <!-- Right: All items catalog -->
    <div class="panel catalog-panel">
        <div class="panel-header">All Items</div>
        <input type="text" class="catalog-search" id="catalogSearch"
            placeholder="Filter items..." oninput="renderCatalog()">
        <div class="panel-body" style="padding:0">
            <ul class="catalog-list" id="catalogList"></ul>
        </div>
    </div>
</div>

<script>
const CLASS_DEFS = [
    { id: 0, name: 'All Classes', icon: 'icon-all' },
    { id: 1, name: 'Warrior',     icon: 'icon-warrior' },
    { id: 2, name: 'Mage',        icon: 'icon-mage' },
    { id: 3, name: 'Master',      icon: 'icon-master' }
];

let creationItems = [];  // flat list from server
let allItems = [];       // item catalog
let selectedClass = null;

// Drag state
let dragRowKey = null;
let dragCatalogItemId = null;

async function loadData() {
    const resp = await fetch('/api/creation_items');
    const data = await resp.json();
    creationItems = data.creation_items;
    allItems = data.all_items;
    renderClassList();
    if (selectedClass !== null) {
        selectClass(selectedClass);
    }
    renderCatalog();
    document.getElementById('totalCount').textContent = creationItems.length + ' total creation items';
}

function itemsForClass(classId) {
    return creationItems.filter(i => i.class_type === classId);
}

function renderClassList() {
    const ul = document.getElementById('classList');
    ul.innerHTML = '';
    for (const cls of CLASS_DEFS) {
        const count = itemsForClass(cls.id).length;
        const li = document.createElement('li');
        li.className = (cls.id === selectedClass) ? 'selected' : '';
        li.innerHTML = `
            <div class="class-name"><span class="class-icon ${cls.icon}"></span>${cls.name}</div>
            <div class="class-count">${count} item(s)</div>
        `;
        li.onclick = () => selectClass(cls.id);
        ul.appendChild(li);
    }
}

function selectClass(classId) {
    selectedClass = classId;
    const cls = CLASS_DEFS.find(c => c.id === classId);

    document.getElementById('detailEmpty').style.display = 'none';
    const dc = document.getElementById('detailContent');
    dc.style.display = '';
    dc.classList.add('visible');
    document.getElementById('detailHeader').textContent = cls.name + ' — Creation Items';
    document.getElementById('itemsFilter').value = '';

    renderItems();
    renderClassList();
    renderCatalog();
}

function renderItems() {
    if (selectedClass === null) return;
    const items = itemsForClass(selectedClass);
    const filter = (document.getElementById('itemsFilter').value || '').toLowerCase();

    const tbody = document.getElementById('itemsBody');
    tbody.innerHTML = '';
    const len = items.length;
    for (let i = 0; i < len; i++) {
        const entry = items[i];
        const itemName = entry.item_name || ('Unknown(' + entry.item_id + ')');
        if (filter && !itemName.toLowerCase().includes(filter) && !String(entry.item_id).includes(filter)) continue;

        const tr = document.createElement('tr');
        const key = entryKey(entry);
        tr.setAttribute('data-key', key);
        tr.draggable = true;

        const isFirst = (i === 0);
        const isLast = (i === len - 1);

        const genderSelect = `<select class="inline-select" onchange="updateField('${key}','gender_limit',this.value)">
            <option value="0" ${entry.gender_limit===0?'selected':''}>Any</option>
            <option value="1" ${entry.gender_limit===1?'selected':''}>Male</option>
            <option value="2" ${entry.gender_limit===2?'selected':''}>Female</option>
        </select>`;

        tr.innerHTML = `
            <td class="col-order">${i + 1}</td>
            <td class="col-id">${entry.item_id}</td>
            <td class="col-name">${escapeHtml(itemName)}</td>
            <td class="col-count"><input type="number" class="inline-input" value="${entry.count}" min="1"
                onchange="updateField('${key}','count',this.value)"></td>
            <td class="col-color"><input type="number" class="inline-input" value="${entry.item_color}" min="0"
                onchange="updateField('${key}','item_color',this.value)"></td>
            <td class="col-lifespan"><input type="number" class="inline-input" value="${entry.lifespan}" min="0"
                onchange="updateField('${key}','lifespan',this.value)" title="0 = use item max lifespan"></td>
            <td class="col-equip"><input type="checkbox" class="inline-checkbox" ${entry.is_equipped?'checked':''}
                onchange="updateField('${key}','is_equipped',this.checked?1:0)"></td>
            <td class="col-gender">${genderSelect}</td>
            <td class="col-actions">
                <button class="btn-icon" onclick="moveItem('${key}','up')" ${isFirst?'disabled':''} title="Move up">&#9650;</button>
                <button class="btn-icon" onclick="moveItem('${key}','down')" ${isLast?'disabled':''} title="Move down">&#9660;</button>
                <button class="btn-icon" onclick="removeItem('${key}')" title="Remove" style="color:#8b2828">&times;</button>
            </td>
        `;

        // Drag events for reordering
        tr.addEventListener('dragstart', (e) => {
            dragRowKey = key;
            dragCatalogItemId = null;
            tr.classList.add('dragging');
            e.dataTransfer.effectAllowed = 'move';
        });
        tr.addEventListener('dragend', () => {
            tr.classList.remove('dragging');
            dragRowKey = null;
            clearDropHighlights();
        });
        tr.addEventListener('dragover', (e) => {
            e.preventDefault();
            e.dataTransfer.dropEffect = 'move';
            clearDropHighlights();
            tr.classList.add('drag-over');
        });
        tr.addEventListener('dragleave', () => {
            tr.classList.remove('drag-over');
        });
        tr.addEventListener('drop', (e) => {
            e.preventDefault();
            tr.classList.remove('drag-over');
            if (dragRowKey !== null && dragRowKey !== key) {
                dropItem(dragRowKey, key);
            } else if (dragCatalogItemId !== null) {
                addItemBefore(dragCatalogItemId, key);
                dragCatalogItemId = null;
            }
        });

        tbody.appendChild(tr);
    }

    setupTableDropZone();
}

function clearDropHighlights() {
    document.querySelectorAll('.drag-over').forEach(el => el.classList.remove('drag-over'));
}

function setupTableDropZone() {
    const table = document.getElementById('itemsTable');
    table.ondragover = (e) => {
        if (dragCatalogItemId !== null || dragRowKey !== null) {
            e.preventDefault();
            e.dataTransfer.dropEffect = 'move';
        }
    };
    table.ondrop = (e) => {
        if (e.target.closest('tr[data-key]')) return;
        e.preventDefault();
        if (dragCatalogItemId !== null) {
            addItem(dragCatalogItemId);
            dragCatalogItemId = null;
        }
    };
}

function renderCatalog() {
    const filter = (document.getElementById('catalogSearch').value || '').toLowerCase();
    const ul = document.getElementById('catalogList');
    ul.innerHTML = '';

    const classItems = itemsForClass(selectedClass);
    const classItemIds = new Set(classItems.map(i => i.item_id));

    for (const item of allItems) {
        if (filter && !item.name.toLowerCase().includes(filter) && !String(item.item_id).includes(filter)) continue;
        const inClass = classItemIds.has(item.item_id);
        const li = document.createElement('li');
        if (inClass) li.className = 'in-class';
        li.innerHTML = `
            <span class="cat-id">${item.item_id}</span>
            <span class="cat-name">${escapeHtml(item.name)}</span>
            <span class="cat-add">+</span>
        `;
        if (!inClass && selectedClass !== null) {
            li.draggable = true;
            li.onclick = () => addItem(item.item_id);
            li.addEventListener('dragstart', (e) => {
                dragCatalogItemId = item.item_id;
                dragRowKey = null;
                e.dataTransfer.effectAllowed = 'copy';
            });
            li.addEventListener('dragend', () => {
                dragCatalogItemId = null;
                clearDropHighlights();
            });
        }
        ul.appendChild(li);
    }
}

// Unique key for a creation item row
function entryKey(entry) {
    return entry.class_type + '_' + entry.item_id + '_' + entry.gender_limit;
}

function parseKey(key) {
    const parts = key.split('_');
    return { class_type: parseInt(parts[0]), item_id: parseInt(parts[1]), gender_limit: parseInt(parts[2]) };
}

async function addItem(itemId) {
    if (selectedClass === null) return;
    const resp = await fetch('/api/creation_items/add', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ class_type: selectedClass, item_id: itemId })
    });
    const data = await resp.json();
    if (data.ok) {
        showStatus('Item added.', 'success');
        await loadData();
    } else {
        showStatus('Error: ' + data.error, 'error');
    }
}

async function addItemBefore(itemId, beforeKey) {
    if (selectedClass === null) return;
    const before = parseKey(beforeKey);
    const resp = await fetch('/api/creation_items/add', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ class_type: selectedClass, item_id: itemId, before_key: before })
    });
    const data = await resp.json();
    if (data.ok) {
        showStatus('Item added.', 'success');
        await loadData();
    } else {
        showStatus('Error: ' + data.error, 'error');
    }
}

async function removeItem(key) {
    const k = parseKey(key);
    const resp = await fetch('/api/creation_items/remove', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(k)
    });
    const data = await resp.json();
    if (data.ok) {
        showStatus('Item removed.', 'success');
        await loadData();
    } else {
        showStatus('Error: ' + data.error, 'error');
    }
}

async function updateField(key, field, value) {
    const k = parseKey(key);
    const resp = await fetch('/api/creation_items/update', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ ...k, field: field, value: parseInt(value) })
    });
    const data = await resp.json();
    if (data.ok) {
        await loadData();
    } else {
        showStatus('Error: ' + data.error, 'error');
    }
}

async function moveItem(key, direction) {
    const k = parseKey(key);
    const resp = await fetch('/api/creation_items/move', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ ...k, direction: direction })
    });
    const data = await resp.json();
    if (data.ok) {
        await loadData();
    } else {
        showStatus('Error: ' + data.error, 'error');
    }
}

async function dropItem(draggedKey, targetKey) {
    const dragged = parseKey(draggedKey);
    const target = parseKey(targetKey);
    const resp = await fetch('/api/creation_items/move', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ ...dragged, before_key: target })
    });
    const data = await resp.json();
    if (data.ok) {
        await loadData();
    } else {
        showStatus('Error: ' + data.error, 'error');
    }
}

function showStatus(msg, type) {
    const bar = document.getElementById('statusBar');
    bar.textContent = msg;
    bar.className = 'status ' + type;
    setTimeout(() => { bar.className = 'status'; }, 4000);
}

function escapeHtml(s) {
    if (!s) return '';
    return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');
}

loadData();
</script>
</body>
</html>
"""

class RequestHandler(BaseHTTPRequestHandler):
    def log_message(self, format, *args):
        print(f"[{self.log_date_time_string()}] {format % args}")

    def do_GET(self):
        path = urlparse(self.path).path
        if path == "/":
            self._send_html(HTML_PAGE)
        elif path == "/api/creation_items":
            self._send_creation_items()
        else:
            self._send_error(404, "Not found")

    def do_POST(self):
        path = urlparse(self.path).path
        if path == "/api/creation_items/add":
            self._handle_add_item()
        elif path == "/api/creation_items/remove":
            self._handle_remove_item()
        elif path == "/api/creation_items/update":
            self._handle_update_item()
        elif path == "/api/creation_items/move":
            self._handle_move_item()
        else:
            self._send_error(404, "Not found")

    def _send_html(self, html):
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.end_headers()
        self.wfile.write(html.encode())

    def _send_json(self, data, status=200):
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.end_headers()
        self.wfile.write(json.dumps(data).encode())

    def _send_error(self, code, msg):
        self._send_json({"ok": False, "error": msg}, code)

    def _read_body(self):
        length = int(self.headers.get("Content-Length", 0))
        return self.rfile.read(length)

    def _send_creation_items(self):
        conn = get_db()

        # All items for catalog
        all_items_rows = conn.execute(
            "SELECT item_id, name FROM items ORDER BY name"
        ).fetchall()
        all_items = [{"item_id": r["item_id"], "name": r["name"]} for r in all_items_rows]

        # Item names lookup
        item_names = {r["item_id"]: r["name"] for r in all_items_rows}

        # Creation items
        rows = conn.execute(
            "SELECT class_type, item_id, count, item_color, lifespan,"
            " is_equipped, gender_limit, sort_order"
            " FROM character_creation_items"
            " ORDER BY class_type, sort_order, item_id"
        ).fetchall()
        conn.close()

        creation_items = []
        for r in rows:
            creation_items.append({
                "class_type": r["class_type"],
                "item_id": r["item_id"],
                "item_name": item_names.get(r["item_id"], f"Unknown({r['item_id']})"),
                "count": r["count"],
                "item_color": r["item_color"],
                "lifespan": r["lifespan"],
                "is_equipped": r["is_equipped"],
                "gender_limit": r["gender_limit"],
                "sort_order": r["sort_order"],
            })

        self._send_json({"creation_items": creation_items, "all_items": all_items})

    def _handle_add_item(self):
        try:
            body = json.loads(self._read_body())
            class_type = int(body["class_type"])
            item_id = int(body["item_id"])
            before_key = body.get("before_key")
            gender_limit = 0

            conn = get_db()

            # Check if already exists
            exists = conn.execute(
                "SELECT 1 FROM character_creation_items"
                " WHERE class_type = ? AND item_id = ? AND gender_limit = ?",
                (class_type, item_id, gender_limit)
            ).fetchone()
            if exists:
                conn.close()
                self._send_json({"ok": False, "error": "Item already in this class"})
                return

            # Get current items for this class to determine sort_order
            rows = conn.execute(
                "SELECT item_id, gender_limit, sort_order FROM character_creation_items"
                " WHERE class_type = ? ORDER BY sort_order, item_id",
                (class_type,)
            ).fetchall()
            order = [(r["item_id"], r["gender_limit"]) for r in rows]

            if before_key is not None:
                before_id = int(before_key["item_id"])
                before_gl = int(before_key["gender_limit"])
                target = (before_id, before_gl)
                if target in order:
                    idx = order.index(target)
                    order.insert(idx, (item_id, gender_limit))
                else:
                    order.append((item_id, gender_limit))
            else:
                order.append((item_id, gender_limit))

            # Insert the new row
            conn.execute(
                "INSERT INTO character_creation_items"
                " (class_type, item_id, count, item_color, lifespan, is_equipped, gender_limit, sort_order)"
                " VALUES (?, ?, 1, 0, 0, 0, ?, 0)",
                (class_type, item_id, gender_limit)
            )

            # Re-write sort_order
            for i, (iid, gl) in enumerate(order):
                conn.execute(
                    "UPDATE character_creation_items SET sort_order = ?"
                    " WHERE class_type = ? AND item_id = ? AND gender_limit = ?",
                    (i, class_type, iid, gl)
                )
            conn.commit()
            conn.close()
            self._send_json({"ok": True})
        except Exception as e:
            self._send_json({"ok": False, "error": str(e)})

    def _handle_remove_item(self):
        try:
            body = json.loads(self._read_body())
            class_type = int(body["class_type"])
            item_id = int(body["item_id"])
            gender_limit = int(body["gender_limit"])

            conn = get_db()
            conn.execute(
                "DELETE FROM character_creation_items"
                " WHERE class_type = ? AND item_id = ? AND gender_limit = ?",
                (class_type, item_id, gender_limit)
            )

            # Re-normalize sort_order
            rows = conn.execute(
                "SELECT item_id, gender_limit FROM character_creation_items"
                " WHERE class_type = ? ORDER BY sort_order, item_id",
                (class_type,)
            ).fetchall()
            for idx, r in enumerate(rows):
                conn.execute(
                    "UPDATE character_creation_items SET sort_order = ?"
                    " WHERE class_type = ? AND item_id = ? AND gender_limit = ?",
                    (idx, class_type, r["item_id"], r["gender_limit"])
                )
            conn.commit()
            conn.close()
            self._send_json({"ok": True})
        except Exception as e:
            self._send_json({"ok": False, "error": str(e)})

    def _handle_update_item(self):
        """Update a single field on a creation item row."""
        try:
            body = json.loads(self._read_body())
            class_type = int(body["class_type"])
            item_id = int(body["item_id"])
            gender_limit = int(body["gender_limit"])
            field = body["field"]
            value = int(body["value"])

            allowed_fields = {"count", "item_color", "lifespan", "is_equipped", "gender_limit"}
            if field not in allowed_fields:
                self._send_json({"ok": False, "error": f"Cannot update field: {field}"})
                return

            conn = get_db()

            if field == "gender_limit":
                # Gender limit is part of the primary key, need special handling
                # Check if target row already exists
                exists = conn.execute(
                    "SELECT 1 FROM character_creation_items"
                    " WHERE class_type = ? AND item_id = ? AND gender_limit = ?",
                    (class_type, item_id, value)
                ).fetchone()
                if exists and value != gender_limit:
                    conn.close()
                    self._send_json({"ok": False, "error": "An entry with that gender already exists"})
                    return

                # Read full row, delete old, insert new
                row = conn.execute(
                    "SELECT * FROM character_creation_items"
                    " WHERE class_type = ? AND item_id = ? AND gender_limit = ?",
                    (class_type, item_id, gender_limit)
                ).fetchone()
                if row:
                    conn.execute(
                        "DELETE FROM character_creation_items"
                        " WHERE class_type = ? AND item_id = ? AND gender_limit = ?",
                        (class_type, item_id, gender_limit)
                    )
                    conn.execute(
                        "INSERT INTO character_creation_items"
                        " (class_type, item_id, count, item_color, lifespan, is_equipped, gender_limit, sort_order)"
                        " VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
                        (class_type, item_id, row["count"], row["item_color"],
                         row["lifespan"], row["is_equipped"], value, row["sort_order"])
                    )
            else:
                conn.execute(
                    f"UPDATE character_creation_items SET {field} = ?"
                    " WHERE class_type = ? AND item_id = ? AND gender_limit = ?",
                    (value, class_type, item_id, gender_limit)
                )

            conn.commit()
            conn.close()
            self._send_json({"ok": True})
        except Exception as e:
            self._send_json({"ok": False, "error": str(e)})

    def _handle_move_item(self):
        """Reorder: up/down or drag-drop before a target."""
        try:
            body = json.loads(self._read_body())
            class_type = int(body["class_type"])
            item_id = int(body["item_id"])
            gender_limit = int(body["gender_limit"])
            direction = body.get("direction")
            before_key = body.get("before_key")

            conn = get_db()
            rows = conn.execute(
                "SELECT item_id, gender_limit FROM character_creation_items"
                " WHERE class_type = ? ORDER BY sort_order, item_id",
                (class_type,)
            ).fetchall()
            order = [(r["item_id"], r["gender_limit"]) for r in rows]
            target = (item_id, gender_limit)

            if target not in order:
                conn.close()
                self._send_json({"ok": False, "error": "Item not found in class"})
                return

            idx = order.index(target)

            if direction == "up" and idx > 0:
                order[idx], order[idx - 1] = order[idx - 1], order[idx]
            elif direction == "down" and idx < len(order) - 1:
                order[idx], order[idx + 1] = order[idx + 1], order[idx]
            elif before_key is not None:
                before = (int(before_key["item_id"]), int(before_key["gender_limit"]))
                if before in order:
                    order.remove(target)
                    target_idx = order.index(before)
                    order.insert(target_idx, target)

            # Write new sort_order
            for i, (iid, gl) in enumerate(order):
                conn.execute(
                    "UPDATE character_creation_items SET sort_order = ?"
                    " WHERE class_type = ? AND item_id = ? AND gender_limit = ?",
                    (i, class_type, iid, gl)
                )
            conn.commit()
            conn.close()
            self._send_json({"ok": True})
        except Exception as e:
            self._send_json({"ok": False, "error": str(e)})


def main():
    port = 8084
    print(f"Creation Item Manager")
    print(f"Database: {os.path.abspath(DB_PATH)}")

    ensure_table()

    print(f"Serving on http://localhost:{port}")
    server = HTTPServer(("127.0.0.1", port), RequestHandler)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down.")
        server.server_close()

if __name__ == "__main__":
    main()
