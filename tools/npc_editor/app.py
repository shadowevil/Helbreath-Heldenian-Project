"""
NPC Editor Tool for Helbreath gamedata.db
Hosts a web interface to edit NPC stats with staging/commit/revert support.
"""

import sqlite3
import os
import json
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse

DB_PATH = os.path.join(os.path.dirname(__file__), "..", "..", "Binaries", "Server", "gamedata.db")
SNAPSHOT_PATH = os.path.join(os.path.dirname(__file__), "original_npcs.json")

MUTABLE_COLUMNS = [
    "name", "npc_type", "npc_size", "side",
    "hp_min", "hp_max", "hold_resist", "defense_ratio", "hit_ratio",
    "min_damage", "max_damage", "attack_range", "abs_damage",
    "resist_magic", "magic_level", "max_mana", "magic_hit_ratio",
    "exp_min", "exp_max", "gold_min", "gold_max",
    "min_bravery", "action_limit", "action_time",
    "target_search_range", "regen_time", "day_of_week_limit",
    "chat_msg_presence", "attribute",
    "drop_table_id",
]

ALL_COLUMNS = ["npc_id"] + MUTABLE_COLUMNS


def get_db():
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    return conn


def save_snapshot():
    """Save a snapshot of all NPC rows for revert support."""
    conn = get_db()
    rows = conn.execute(f"SELECT {', '.join(ALL_COLUMNS)} FROM npc_configs ORDER BY npc_id").fetchall()
    conn.close()
    data = {}
    for r in rows:
        data[str(r["npc_id"])] = {col: r[col] for col in ALL_COLUMNS}
    with open(SNAPSHOT_PATH, "w") as f:
        json.dump(data, f, indent=2)
    return data


def load_snapshot():
    """Load the saved snapshot, creating it if it doesn't exist."""
    if not os.path.exists(SNAPSHOT_PATH):
        return save_snapshot()
    with open(SNAPSHOT_PATH, "r") as f:
        return json.load(f)


# ---------------------------------------------------------------------------
# Embedded HTML
# ---------------------------------------------------------------------------

HTML_PAGE = r"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>NPC Editor</title>
<style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
        font-family: 'Segoe UI', Tahoma, sans-serif;
        background: #181620;
        color: #ddd8d0;
        height: 100vh;
        display: flex;
        flex-direction: column;
        overflow: hidden;
    }

    /* ---------- Header ---------- */
    .header {
        background: linear-gradient(to right, #7a5c34, #3a3248);
        padding: 12px 24px;
        border-bottom: 2px solid #3a3248;
        flex-shrink: 0;
    }
    .header h1 { font-size: 20px; color: #fff; }

    /* ---------- Status bar ---------- */
    .status {
        padding: 8px 16px;
        font-size: 13px;
        text-align: center;
        display: none;
        flex-shrink: 0;
    }
    .status.success { display: block; background: #2e2818; color: #d8ccb0; }
    .status.error { display: block; background: #3a1818; color: #e0c0c0; }

    /* ---------- Action bar ---------- */
    .action-bar {
        background: #22202c;
        padding: 10px 20px;
        border-bottom: 1px solid #3a3248;
        display: flex;
        align-items: center;
        gap: 10px;
        flex-wrap: wrap;
        flex-shrink: 0;
    }
    .btn {
        padding: 7px 14px;
        border: none;
        border-radius: 4px;
        cursor: pointer;
        font-size: 13px;
        font-weight: 600;
        transition: background 0.2s;
        white-space: nowrap;
    }
    .btn-save { background: #6e5530; color: #fff; }
    .btn-save:hover { background: #7e6538; }
    .btn-revert { background: #8b2828; color: #fff; }
    .btn-revert:hover { background: #a03030; }
    .btn-snapshot { background: #3a3468; color: #fff; }
    .btn-snapshot:hover { background: #2e2a56; }
    .changes-badge {
        background: #c8a850;
        color: #181620;
        border-radius: 12px;
        padding: 2px 8px;
        font-size: 12px;
        font-weight: 700;
        display: none;
    }
    .action-sep { width: 1px; height: 24px; background: #3a3248; }
    .search-input, .filter-select {
        padding: 7px 10px;
        border: 1px solid #3a3248;
        border-radius: 4px;
        background: #181620;
        color: #ddd8d0;
        font-size: 13px;
    }
    .search-input { width: 200px; }
    .search-input:focus, .filter-select:focus { outline: none; border-color: #c8a850; }
    .search-input::placeholder { color: #686058; }

    /* ---------- Main panels ---------- */
    .panels {
        display: flex;
        flex: 1;
        overflow: hidden;
    }

    /* ---------- Left: Table ---------- */
    .left-panel {
        width: 55%;
        overflow-y: auto;
        border-right: 1px solid #3a3248;
    }
    .npc-table {
        width: 100%;
        border-collapse: collapse;
    }
    .npc-table thead th {
        background: #22202c;
        padding: 8px 10px;
        text-align: left;
        font-size: 11px;
        text-transform: uppercase;
        color: #887868;
        border-bottom: 2px solid #3a3248;
        position: sticky;
        top: 0;
        z-index: 10;
        cursor: pointer;
        user-select: none;
        white-space: nowrap;
    }
    .npc-table thead th:hover { color: #c8a850; }
    .npc-table thead th .sort-arrow { font-size: 10px; margin-left: 4px; color: #c8a850; }
    .npc-table tbody tr {
        border-bottom: 1px solid #2e2838;
        cursor: pointer;
    }
    .npc-table tbody tr:hover { background: #282434; }
    .npc-table tbody tr.selected { background: #2a2640; }
    .npc-table tbody tr.modified { border-left: 3px solid #c8a850; }
    .npc-table tbody tr.modified.selected { background: #2c2820; }
    .npc-table td {
        padding: 5px 10px;
        font-size: 13px;
        white-space: nowrap;
    }
    .npc-table td:first-child { color: #887868; width: 45px; }

    /* ---------- Right: Detail ---------- */
    .right-panel {
        width: 45%;
        overflow-y: auto;
        padding: 16px 20px;
    }
    .detail-placeholder {
        color: #585048;
        text-align: center;
        margin-top: 80px;
        font-size: 14px;
    }
    .detail-header {
        font-size: 16px;
        color: #c8a850;
        margin-bottom: 16px;
        padding-bottom: 8px;
        border-bottom: 1px solid #3a3248;
    }
    .section {
        margin-bottom: 18px;
    }
    .section-title {
        font-size: 12px;
        text-transform: uppercase;
        color: #887868;
        letter-spacing: 0.5px;
        margin-bottom: 8px;
        cursor: pointer;
        user-select: none;
    }
    .section-title:hover { color: #c8a850; }
    .section-grid {
        display: grid;
        grid-template-columns: 1fr 1fr;
        gap: 6px 16px;
    }
    .field-row {
        display: flex;
        align-items: center;
        gap: 8px;
    }
    .field-label {
        font-size: 12px;
        color: #887868;
        min-width: 120px;
        white-space: nowrap;
    }
    .field-input {
        flex: 1;
        padding: 5px 8px;
        background: #14121c;
        border: 1px solid #332e3a;
        color: #ddd8d0;
        border-radius: 3px;
        font-size: 13px;
        min-width: 0;
    }
    .field-input:focus { outline: none; border-color: #c8a850; }
    .field-input.changed { border-color: #c8a850; background: #1e1a10; }
    .field-input[readonly] { color: #686058; cursor: default; }
    .field-wide { grid-column: 1 / -1; }
</style>
</head>
<body>

<div class="header"><h1>NPC Editor</h1></div>
<div class="status" id="statusBar"></div>

<div class="action-bar">
    <button class="btn btn-save" onclick="saveChanges()">Save Changes</button>
    <span class="changes-badge" id="changesBadge">0</span>
    <button class="btn btn-revert" onclick="revertAll()">Revert All</button>
    <button class="btn btn-snapshot" onclick="takeSnapshot()">Snapshot</button>
    <div class="action-sep"></div>
    <input type="text" class="search-input" id="searchBox" placeholder="Search by name...">
    <select class="filter-select" id="filterMode">
        <option value="all">All NPCs</option>
        <option value="modified">Modified Only</option>
    </select>
</div>

<div class="panels">
    <div class="left-panel">
        <table class="npc-table">
            <thead>
                <tr id="tableHead"></tr>
            </thead>
            <tbody id="tableBody"></tbody>
        </table>
    </div>
    <div class="right-panel" id="detailPanel">
        <div class="detail-placeholder">Click an NPC to edit</div>
    </div>
</div>

<script>
// ---- State ----
let npcs = [];
let originals = {};
let pendingChanges = {};
let selectedNpcId = null;
let sortColumn = 'npc_id';
let sortAsc = true;
let filterMode = 'all';
let searchText = '';

const TABLE_COLS = [
    { key: 'npc_id', label: 'ID' },
    { key: 'name', label: 'Name' },
    { key: 'hp_min', label: 'HP Min' },
    { key: 'hp_max', label: 'HP Max' },
    { key: 'exp_min', label: 'Exp Min' },
    { key: 'exp_max', label: 'Exp Max' },
    { key: 'gold_min', label: 'Gold Min' },
    { key: 'gold_max', label: 'Gold Max' },
    { key: 'side', label: 'Side' },
];

const FIELD_GROUPS = [
    { title: 'Identity', fields: [
        { key: 'name', label: 'Name', type: 'text' },
        { key: 'npc_type', label: 'NPC Type', type: 'number' },
        { key: 'npc_size', label: 'Size', type: 'number' },
        { key: 'side', label: 'Side', type: 'number' },
    ]},
    { title: 'Combat', fields: [
        { key: 'hp_min', label: 'HP Min', type: 'number' },
        { key: 'hp_max', label: 'HP Max', type: 'number' },
        { key: 'hold_resist', label: 'Hold Resist %', type: 'number' },
        { key: 'defense_ratio', label: 'Defense Ratio', type: 'number' },
        { key: 'hit_ratio', label: 'Hit Ratio', type: 'number' },
        { key: 'min_damage', label: 'Min Damage', type: 'number' },
        { key: 'max_damage', label: 'Max Damage', type: 'number' },
        { key: 'attack_range', label: 'Attack Range', type: 'number' },
        { key: 'abs_damage', label: 'Abs Damage', type: 'number' },
    ]},
    { title: 'Magic', fields: [
        { key: 'resist_magic', label: 'Resist Magic', type: 'number' },
        { key: 'magic_level', label: 'Magic Level', type: 'number' },
        { key: 'max_mana', label: 'Max Mana', type: 'number' },
        { key: 'magic_hit_ratio', label: 'Magic Hit Ratio', type: 'number' },
    ]},
    { title: 'Rewards', fields: [
        { key: 'exp_min', label: 'Exp Min', type: 'number' },
        { key: 'exp_max', label: 'Exp Max', type: 'number' },
        { key: 'gold_min', label: 'Gold Min', type: 'number' },
        { key: 'gold_max', label: 'Gold Max', type: 'number' },
    ]},
    { title: 'Behavior', fields: [
        { key: 'min_bravery', label: 'Min Bravery', type: 'number' },
        { key: 'action_limit', label: 'Action Limit', type: 'number' },
        { key: 'action_time', label: 'Action Time', type: 'number' },
        { key: 'target_search_range', label: 'Target Search Range', type: 'number' },
        { key: 'regen_time', label: 'Regen Time', type: 'number' },
        { key: 'day_of_week_limit', label: 'Day of Week Limit', type: 'number' },
        { key: 'chat_msg_presence', label: 'Chat Msg Presence', type: 'number' },
        { key: 'attribute', label: 'Attribute', type: 'number' },
    ]},
    { title: 'Links', fields: [
        { key: 'drop_table_id', label: 'Drop Table ID', type: 'number' },
    ]},
];

// ---- Helpers ----
function escapeHtml(s) {
    if (s == null) return '';
    return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');
}

function getNpcValue(npc, field) {
    const id = String(npc.npc_id);
    if (pendingChanges[id] && field in pendingChanges[id]) {
        return pendingChanges[id][field];
    }
    return npc[field];
}

function getOrigValue(npcId, field) {
    const orig = originals[String(npcId)];
    return orig ? orig[field] : null;
}

function isNpcModified(npcId) {
    const id = String(npcId);
    if (pendingChanges[id] && Object.keys(pendingChanges[id]).length > 0) return true;
    // Also check DB vs snapshot
    const npc = npcs.find(n => n.npc_id === npcId);
    const orig = originals[id];
    if (!npc || !orig) return false;
    for (const col of Object.keys(orig)) {
        if (col === 'npc_id') continue;
        if (String(npc[col]) !== String(orig[col])) return true;
    }
    return false;
}

function isFieldChanged(npcId, field) {
    const id = String(npcId);
    const npc = npcs.find(n => n.npc_id === npcId);
    const orig = originals[id];
    if (!npc || !orig) return false;
    const current = getNpcValue(npc, field);
    return String(current) !== String(orig[field]);
}

function countChanges() {
    let count = 0;
    for (const npc of npcs) {
        if (isNpcModified(npc.npc_id)) count++;
    }
    return count;
}

function updateBadge() {
    const count = countChanges();
    const badge = document.getElementById('changesBadge');
    if (count > 0) {
        badge.textContent = count;
        badge.style.display = 'inline';
    } else {
        badge.style.display = 'none';
    }
}

function showStatus(msg, type) {
    const bar = document.getElementById('statusBar');
    bar.textContent = msg;
    bar.className = 'status ' + type;
    setTimeout(() => { bar.className = 'status'; }, 4000);
}

// ---- Data loading ----
async function loadNpcs() {
    const resp = await fetch('/api/npcs');
    const data = await resp.json();
    npcs = data.npcs;
    originals = data.originals;
    pendingChanges = {};
    renderTable();
    updateBadge();
    if (selectedNpcId !== null) renderDetail(selectedNpcId);
}

// ---- Table rendering ----
function renderTableHead() {
    const tr = document.getElementById('tableHead');
    tr.innerHTML = '';
    for (const col of TABLE_COLS) {
        const th = document.createElement('th');
        let label = escapeHtml(col.label);
        if (sortColumn === col.key) {
            label += `<span class="sort-arrow">${sortAsc ? '\u25B2' : '\u25BC'}</span>`;
        }
        th.innerHTML = label;
        th.onclick = () => {
            if (sortColumn === col.key) {
                sortAsc = !sortAsc;
            } else {
                sortColumn = col.key;
                sortAsc = true;
            }
            renderTable();
        };
        tr.appendChild(th);
    }
}

function renderTable() {
    renderTableHead();
    const tbody = document.getElementById('tableBody');
    tbody.innerHTML = '';

    let filtered = npcs.filter(npc => {
        if (searchText && !String(npc.name).toLowerCase().includes(searchText)) return false;
        if (filterMode === 'modified' && !isNpcModified(npc.npc_id)) return false;
        return true;
    });

    filtered.sort((a, b) => {
        let va = getNpcValue(a, sortColumn);
        let vb = getNpcValue(b, sortColumn);
        if (typeof va === 'string') va = va.toLowerCase();
        if (typeof vb === 'string') vb = vb.toLowerCase();
        if (va < vb) return sortAsc ? -1 : 1;
        if (va > vb) return sortAsc ? 1 : -1;
        return 0;
    });

    for (const npc of filtered) {
        const tr = document.createElement('tr');
        const modified = isNpcModified(npc.npc_id);
        if (modified) tr.classList.add('modified');
        if (npc.npc_id === selectedNpcId) tr.classList.add('selected');

        for (const col of TABLE_COLS) {
            const td = document.createElement('td');
            td.textContent = getNpcValue(npc, col.key);
            tr.appendChild(td);
        }

        tr.onclick = () => {
            selectedNpcId = npc.npc_id;
            renderTable();
            renderDetail(npc.npc_id);
        };

        tbody.appendChild(tr);
    }
}

// ---- Detail panel ----
function renderDetail(npcId) {
    const panel = document.getElementById('detailPanel');
    const npc = npcs.find(n => n.npc_id === npcId);
    if (!npc) {
        panel.innerHTML = '<div class="detail-placeholder">NPC not found</div>';
        return;
    }

    let html = `<div class="detail-header">NPC #${npc.npc_id} &mdash; ${escapeHtml(getNpcValue(npc, 'name'))}</div>`;

    // Read-only npc_id
    html += `<div class="section">
        <div class="section-grid"><div class="field-row field-wide">
            <span class="field-label">NPC ID</span>
            <input class="field-input" type="number" value="${npc.npc_id}" readonly>
        </div></div>
    </div>`;

    for (const group of FIELD_GROUPS) {
        html += `<div class="section">`;
        html += `<div class="section-title">${escapeHtml(group.title)}</div>`;
        html += `<div class="section-grid">`;
        for (const f of group.fields) {
            const val = getNpcValue(npc, f.key);
            const changed = isFieldChanged(npcId, f.key) ? ' changed' : '';
            const inputType = f.type === 'text' ? 'text' : 'number';
            const wide = f.key === 'name' ? ' field-wide' : '';
            html += `<div class="field-row${wide}">
                <span class="field-label">${escapeHtml(f.label)}</span>
                <input class="field-input${changed}" type="${inputType}"
                    value="${escapeHtml(String(val))}"
                    data-npc="${npcId}" data-field="${f.key}"
                    oninput="onFieldChange(this)">
            </div>`;
        }
        html += `</div></div>`;
    }

    panel.innerHTML = html;
}

function onFieldChange(input) {
    const npcId = parseInt(input.dataset.npc);
    const field = input.dataset.field;
    const id = String(npcId);
    const npc = npcs.find(n => n.npc_id === npcId);
    if (!npc) return;

    // Determine the new value
    const fieldDef = FIELD_GROUPS.flatMap(g => g.fields).find(f => f.key === field);
    let newVal;
    if (fieldDef && fieldDef.type === 'text') {
        newVal = input.value;
    } else {
        newVal = input.value === '' ? 0 : parseInt(input.value) || 0;
    }

    // Compare to original snapshot value
    const origVal = getOrigValue(npcId, field);
    const dbVal = npc[field];

    if (!pendingChanges[id]) pendingChanges[id] = {};

    // If new value matches the DB value and there's no pending for this field, skip
    // If new value matches original, remove from pending
    if (String(newVal) === String(origVal) && String(dbVal) === String(origVal)) {
        delete pendingChanges[id][field];
        if (Object.keys(pendingChanges[id]).length === 0) delete pendingChanges[id];
    } else {
        pendingChanges[id][field] = newVal;
    }

    // Update highlight
    const isChanged = isFieldChanged(npcId, field);
    input.classList.toggle('changed', isChanged);

    updateBadge();
    renderTable();
}

// ---- API actions ----
async function saveChanges() {
    // Build change list: pending changes + DB-vs-snapshot diffs
    const changes = [];
    const seen = new Set();

    // Pending changes first
    for (const [id, fields] of Object.entries(pendingChanges)) {
        for (const [field, value] of Object.entries(fields)) {
            changes.push({ npc_id: parseInt(id), field, value });
            seen.add(id + ':' + field);
        }
    }

    // Also include any DB-vs-snapshot diffs that don't have pending overrides
    for (const npc of npcs) {
        const id = String(npc.npc_id);
        const orig = originals[id];
        if (!orig) continue;
        for (const col of Object.keys(orig)) {
            if (col === 'npc_id') continue;
            if (seen.has(id + ':' + col)) continue;
            if (String(npc[col]) !== String(orig[col])) {
                // DB already differs from snapshot — this was saved before, no need to re-save
            }
        }
    }

    if (changes.length === 0) {
        showStatus('No pending changes to save.', 'error');
        return;
    }

    const resp = await fetch('/api/update', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ changes })
    });
    const data = await resp.json();
    if (data.ok) {
        showStatus('Saved ' + data.count + ' field(s) across ' + (new Set(changes.map(c => c.npc_id))).size + ' NPC(s).', 'success');
        await loadNpcs();
    } else {
        showStatus('Error: ' + data.error, 'error');
    }
}

async function revertAll() {
    if (!confirm('Revert ALL NPCs to their snapshot baseline?')) return;
    const resp = await fetch('/api/revert', { method: 'POST' });
    const data = await resp.json();
    if (data.ok) {
        showStatus('Reverted ' + data.count + ' NPC(s) to snapshot baseline.', 'success');
        await loadNpcs();
    } else {
        showStatus('Error: ' + data.error, 'error');
    }
}

async function takeSnapshot() {
    if (!confirm('Save current DB state as the new snapshot baseline? This replaces the previous snapshot.')) return;
    const resp = await fetch('/api/snapshot', { method: 'POST' });
    const data = await resp.json();
    if (data.ok) {
        showStatus('New snapshot saved with ' + data.count + ' NPCs.', 'success');
        await loadNpcs();
    } else {
        showStatus('Error: ' + data.error, 'error');
    }
}

// ---- Event bindings ----
document.getElementById('searchBox').addEventListener('input', function() {
    searchText = this.value.toLowerCase();
    renderTable();
});

document.getElementById('filterMode').addEventListener('change', function() {
    filterMode = this.value;
    renderTable();
});

// ---- Init ----
loadNpcs();
</script>
</body>
</html>
"""

# ---------------------------------------------------------------------------
# Request handler
# ---------------------------------------------------------------------------

class RequestHandler(BaseHTTPRequestHandler):
    def log_message(self, format, *args):
        print(f"[{self.log_date_time_string()}] {format % args}")

    def do_GET(self):
        path = urlparse(self.path).path
        if path == "/":
            self._send_html(HTML_PAGE)
        elif path == "/api/npcs":
            self._send_npcs()
        else:
            self._send_error(404, "Not found")

    def do_POST(self):
        path = urlparse(self.path).path
        if path == "/api/update":
            self._handle_update()
        elif path == "/api/revert":
            self._handle_revert()
        elif path == "/api/snapshot":
            self._handle_snapshot()
        else:
            self._send_error(404, "Not found")

    # ---- Response helpers ----

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

    # ---- GET /api/npcs ----

    def _send_npcs(self):
        conn = get_db()
        rows = conn.execute(
            f"SELECT {', '.join(ALL_COLUMNS)} FROM npc_configs ORDER BY npc_id"
        ).fetchall()
        conn.close()
        snapshot = load_snapshot()
        npc_list = [{col: r[col] for col in ALL_COLUMNS} for r in rows]
        self._send_json({"npcs": npc_list, "originals": snapshot})

    # ---- POST /api/update ----

    def _handle_update(self):
        try:
            body = json.loads(self._read_body())
            changes = body.get("changes", [])
            if not changes:
                self._send_json({"ok": False, "error": "No changes provided"})
                return

            # Group changes by npc_id
            grouped = {}
            for c in changes:
                npc_id = int(c["npc_id"])
                field = str(c["field"])
                value = c["value"]
                if field not in MUTABLE_COLUMNS:
                    self._send_json({"ok": False, "error": f"Invalid field: {field}"})
                    return
                grouped.setdefault(npc_id, {})[field] = value

            conn = get_db()
            total = 0
            for npc_id, fields in grouped.items():
                set_parts = []
                values = []
                for f, v in fields.items():
                    set_parts.append(f"{f} = ?")
                    values.append(v)
                values.append(npc_id)
                sql = f"UPDATE npc_configs SET {', '.join(set_parts)} WHERE npc_id = ?"
                conn.execute(sql, values)
                total += len(fields)
            conn.commit()
            conn.close()
            self._send_json({"ok": True, "count": total})
        except Exception as e:
            self._send_json({"ok": False, "error": str(e)})

    # ---- POST /api/revert ----

    def _handle_revert(self):
        try:
            snapshot = load_snapshot()
            conn = get_db()
            count = 0
            for npc_id_str, orig_row in snapshot.items():
                npc_id = int(npc_id_str)
                cur = conn.execute(
                    f"SELECT {', '.join(ALL_COLUMNS)} FROM npc_configs WHERE npc_id = ?",
                    (npc_id,)
                ).fetchone()
                if not cur:
                    continue
                diffs = {}
                for col in MUTABLE_COLUMNS:
                    if str(cur[col]) != str(orig_row.get(col, cur[col])):
                        diffs[col] = orig_row[col]
                if diffs:
                    set_parts = [f"{f} = ?" for f in diffs]
                    values = list(diffs.values()) + [npc_id]
                    conn.execute(
                        f"UPDATE npc_configs SET {', '.join(set_parts)} WHERE npc_id = ?",
                        values
                    )
                    count += 1
            conn.commit()
            conn.close()
            self._send_json({"ok": True, "count": count})
        except Exception as e:
            self._send_json({"ok": False, "error": str(e)})

    # ---- POST /api/snapshot ----

    def _handle_snapshot(self):
        try:
            data = save_snapshot()
            self._send_json({"ok": True, "count": len(data)})
        except Exception as e:
            self._send_json({"ok": False, "error": str(e)})


def main():
    port = 8083
    print("NPC Editor Tool")
    print(f"Database: {os.path.abspath(DB_PATH)}")
    print(f"Serving on http://localhost:{port}")
    server = HTTPServer(("127.0.0.1", port), RequestHandler)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down.")
        server.server_close()


if __name__ == "__main__":
    main()
