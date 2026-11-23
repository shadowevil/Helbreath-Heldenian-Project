"""
Shop Manager Tool for Helbreath GameConfigs.db
Hosts a web interface to manage NPC shop inventories and mappings.

Usage: python app.py
Then open http://localhost:8081 in your browser.
"""

import sqlite3
import os
import json
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse

DB_PATH = os.path.join(os.path.dirname(__file__), "..", "..", "Binaries", "Server", "gamedata.db")

def get_db():
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    return conn

def ensure_sort_order_column():
    """Add sort_order column to shop_items if it doesn't exist (migration for older DBs)."""
    conn = get_db()
    columns = [row[1] for row in conn.execute("PRAGMA table_info(shop_items)").fetchall()]
    if "sort_order" not in columns:
        conn.execute("ALTER TABLE shop_items ADD COLUMN sort_order INTEGER NOT NULL DEFAULT 0")
        # Assign initial sort_order per shop based on item_id order
        rows = conn.execute("SELECT shop_id, item_id FROM shop_items ORDER BY shop_id, item_id").fetchall()
        shop_idx = {}
        for r in rows:
            sid = r["shop_id"]
            idx = shop_idx.get(sid, 0)
            conn.execute("UPDATE shop_items SET sort_order = ? WHERE shop_id = ? AND item_id = ?",
                         (idx, sid, r["item_id"]))
            shop_idx[sid] = idx + 1
        conn.commit()
        print("Migration: added sort_order column to shop_items")
    conn.close()

HTML_PAGE = r"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Shop Manager</title>
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

    /* Shop list */
    .shop-list-panel { width: 240px; flex-shrink: 0; }
    .shop-list { list-style: none; }
    .shop-list li {
        padding: 10px 12px;
        cursor: pointer;
        border-bottom: 1px solid #181620;
        transition: background 0.15s;
    }
    .shop-list li:hover { background: #181620; }
    .shop-list li.selected { background: #181620; border-left: 3px solid #c8a850; }
    .shop-list li .shop-id { color: #887868; font-size: 12px; }
    .shop-list li .shop-name { font-size: 14px; }
    .shop-list li .shop-npcs { color: #686058; font-size: 11px; margin-top: 2px; }

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

    /* NPC mapping table */
    .mapping-table { width: 100%; border-collapse: collapse; }
    .mapping-table th {
        text-align: left;
        font-size: 11px;
        text-transform: uppercase;
        color: #887868;
        padding: 6px 8px;
        border-bottom: 1px solid #2e2838;
    }
    .mapping-table td { padding: 6px 8px; font-size: 13px; }
    .mapping-table tr:hover { background: #181620; }
    .add-mapping-row {
        display: flex;
        gap: 8px;
        margin-top: 10px;
        align-items: center;
    }
    .add-mapping-row input {
        padding: 6px 10px;
        background: #181620;
        border: 1px solid #332e3a;
        color: #ddd8d0;
        border-radius: 4px;
        font-size: 13px;
        width: 100px;
    }
    .add-mapping-row input:focus { outline: none; border-color: #c8a850; }

    /* Shop items filter */
    .shop-items-filter {
        padding: 6px 10px;
        background: #181620;
        border: 1px solid #332e3a;
        color: #ddd8d0;
        border-radius: 4px;
        font-size: 13px;
        margin-bottom: 8px;
        width: 100%;
    }
    .shop-items-filter:focus { outline: none; border-color: #c8a850; }
    .shop-items-filter::placeholder { color: #686058; }
    .shop-items-scroll { overflow-y: auto; flex: 1; min-height: 0; }
    .items-table tr.drop-target td { border-bottom: 2px solid #c8a850; }

    /* Shop items list table */
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
    .items-table td { padding: 4px 8px; font-size: 13px; }
    .items-table tr:hover { background: #181620; }
    .items-table .col-order { width: 36px; color: #686058; text-align: center; }
    .items-table .col-id { width: 55px; color: #887868; }
    .items-table .col-actions { width: 100px; text-align: right; white-space: nowrap; }
    .items-table .col-actions .btn-icon { margin-left: 2px; }
    .items-table .drag-handle {
        cursor: grab;
        color: #555;
        font-size: 14px;
        user-select: none;
    }
    .items-table .drag-handle:hover { color: #887868; }
    .items-table tr.dragging { opacity: 0.4; }
    .items-table tr.drag-over td { border-top: 2px solid #c8a850; }
    /* Right panel — all items catalog */
    .catalog-panel { width: 320px; flex-shrink: 0; }
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
    .catalog-list li.in-shop { opacity: 0.35; }
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
    .catalog-list li.in-shop .cat-add { display: none; }

    /* New shop dialog */
    .modal-overlay {
        position: fixed; top: 0; left: 0; right: 0; bottom: 0;
        background: rgba(0,0,0,0.6);
        display: none;
        justify-content: center;
        align-items: center;
        z-index: 200;
    }
    .modal-overlay.visible { display: flex; }
    .modal {
        background: #22202c;
        border: 1px solid #3a3248;
        border-radius: 8px;
        padding: 24px;
        width: 350px;
    }
    .modal h3 { margin-bottom: 16px; font-size: 16px; color: #c8a850; }
    .modal label { display: block; margin-bottom: 4px; font-size: 12px; color: #887868; }
    .modal input {
        width: 100%;
        padding: 8px 10px;
        background: #181620;
        border: 1px solid #332e3a;
        color: #ddd8d0;
        border-radius: 4px;
        font-size: 14px;
        margin-bottom: 12px;
    }
    .modal input:focus { outline: none; border-color: #c8a850; }
    .modal-actions { display: flex; gap: 8px; justify-content: flex-end; }
</style>
</head>
<body>
<nav class="navbar">
    <h1>Shop Manager</h1>
    <div class="navbar-actions">
        <button class="btn btn-primary" onclick="showNewShopModal()">New Shop</button>
    </div>
</nav>
<div class="status" id="statusBar"></div>

<div class="container">
    <!-- Left: Shop list -->
    <div class="panel shop-list-panel">
        <div class="panel-header">Shops</div>
        <div class="panel-body" style="padding:0">
            <ul class="shop-list" id="shopList"></ul>
        </div>
    </div>

    <!-- Middle: Shop detail -->
    <div class="panel detail-panel" id="detailPanel">
        <div class="panel-header" id="detailHeader">Shop Details</div>
        <div class="panel-body">
            <div class="detail-empty" id="detailEmpty">Select a shop from the list</div>
            <div id="detailContent" style="display:none">
                <div class="section">
                    <div class="section-title">NPC Mappings</div>
                    <table class="mapping-table" id="mappingTable">
                        <thead><tr><th>NPC Config ID</th><th>NPC Name</th><th></th></tr></thead>
                        <tbody id="mappingBody"></tbody>
                    </table>
                    <div class="add-mapping-row">
                        <input type="number" id="addNpcConfigId" placeholder="NPC config ID" min="0">
                        <button class="btn btn-save btn-small" onclick="addMapping()">Add NPC</button>
                    </div>
                </div>
                <div class="section" style="flex:1;display:flex;flex-direction:column;min-height:0">
                    <div class="section-title">Shop Items <span id="itemCount" style="color:#686058;font-size:11px;text-transform:none"></span></div>
                    <input type="text" class="shop-items-filter" id="shopItemsFilter"
                        placeholder="Filter shop items..." oninput="renderShopItems()">
                    <div class="shop-items-scroll">
                    <table class="items-table" id="itemsTable">
                        <thead><tr><th class="col-order">#</th><th class="col-id">ID</th><th>Name</th><th class="col-actions"></th></tr></thead>
                        <tbody id="itemsBody"></tbody>
                    </table>
                    </div>
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

<!-- New Shop Modal -->
<div class="modal-overlay" id="newShopModal">
    <div class="modal">
        <h3>Create New Shop</h3>
        <label>Shop ID</label>
        <input type="number" id="newShopId" placeholder="e.g. 10" min="1">
        <div class="modal-actions">
            <button class="btn btn-outline" onclick="hideNewShopModal()">Cancel</button>
            <button class="btn btn-primary" onclick="createShop()">Create</button>
        </div>
    </div>
</div>

<script>
let shops = [];
let allItems = [];
let selectedShopId = null;

// -- Drag state --
let dragItemId = null;       // reorder within shop
let dragCatalogItemId = null; // drag from catalog to shop

async function loadData() {
    const resp = await fetch('/api/shops');
    const data = await resp.json();
    shops = data.shops;
    allItems = data.all_items;
    renderShopList();
    if (selectedShopId !== null) {
        selectShop(selectedShopId);
    }
    renderCatalog();
}

function renderShopList() {
    const ul = document.getElementById('shopList');
    ul.innerHTML = '';
    for (const shop of shops) {
        const li = document.createElement('li');
        li.className = (shop.shop_id === selectedShopId) ? 'selected' : '';
        const npcNames = shop.npcs.map(n => n.npc_name || ('Config ' + n.npc_config_id)).join(', ');
        li.innerHTML = `
            <div class="shop-id">Shop #${shop.shop_id}</div>
            <div class="shop-name">${shop.items.length} item(s)</div>
            <div class="shop-npcs">${npcNames || 'No NPCs assigned'}</div>
        `;
        li.onclick = () => selectShop(shop.shop_id);
        ul.appendChild(li);
    }
}

function selectShop(shopId) {
    selectedShopId = shopId;
    const shop = shops.find(s => s.shop_id === shopId);
    if (!shop) return;

    document.getElementById('detailEmpty').style.display = 'none';
    document.getElementById('detailContent').style.display = 'block';
    document.getElementById('detailHeader').textContent = 'Shop #' + shopId;
    document.getElementById('shopItemsFilter').value = '';

    // NPC mappings
    const tbody = document.getElementById('mappingBody');
    tbody.innerHTML = '';
    for (const npc of shop.npcs) {
        const tr = document.createElement('tr');
        tr.innerHTML = `
            <td>${npc.npc_config_id}</td>
            <td>${escapeHtml(npc.npc_name || 'Unknown')}</td>
            <td><button class="btn btn-danger btn-small" onclick="removeMapping(${npc.npc_config_id})">Remove</button></td>
        `;
        tbody.appendChild(tr);
    }

    // Items list
    renderShopItems(shop);
    renderShopList();
    renderCatalog();
}

function renderShopItems(shop) {
    if (!shop) shop = shops.find(s => s.shop_id === selectedShopId);
    if (!shop) return;

    const filter = (document.getElementById('shopItemsFilter').value || '').toLowerCase();
    document.getElementById('itemCount').textContent = '(' + shop.items.length + ')';
    const tbody = document.getElementById('itemsBody');
    tbody.innerHTML = '';
    const len = shop.items.length;
    for (let i = 0; i < len; i++) {
        const item = shop.items[i];
        if (filter && !item.name.toLowerCase().includes(filter) && !String(item.item_id).includes(filter)) continue;
        const tr = document.createElement('tr');
        tr.setAttribute('data-item-id', item.item_id);
        tr.draggable = true;

        const isFirst = (i === 0);
        const isLast = (i === len - 1);

        tr.innerHTML = `
            <td class="col-order">${i + 1}</td>
            <td class="col-id">${item.item_id}</td>
            <td>${escapeHtml(item.name)}</td>
            <td class="col-actions">
                <button class="btn-icon" onclick="moveItem(${item.item_id},'up')" ${isFirst ? 'disabled' : ''} title="Move up">&#9650;</button>
                <button class="btn-icon" onclick="moveItem(${item.item_id},'down')" ${isLast ? 'disabled' : ''} title="Move down">&#9660;</button>
                <button class="btn-icon" onclick="removeItem(${item.item_id})" title="Remove" style="color:#8b2828">&times;</button>
            </td>
        `;

        // Drag events — reorder within shop
        tr.addEventListener('dragstart', (e) => {
            dragItemId = item.item_id;
            dragCatalogItemId = null;
            tr.classList.add('dragging');
            e.dataTransfer.effectAllowed = 'move';
        });
        tr.addEventListener('dragend', () => {
            tr.classList.remove('dragging');
            dragItemId = null;
            clearDropHighlights();
        });
        // Drop target — accepts both shop reorder and catalog drops
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
            if (dragItemId !== null && dragItemId !== item.item_id) {
                dropItem(dragItemId, item.item_id);
            } else if (dragCatalogItemId !== null) {
                addItemAt(dragCatalogItemId, item.item_id);
                dragCatalogItemId = null;
            }
        });

        tbody.appendChild(tr);
    }

    // Drop zone at the end of the table for appending
    setupTableDropZone();
}

function clearDropHighlights() {
    document.querySelectorAll('.drag-over, .drop-target').forEach(el => {
        el.classList.remove('drag-over');
        el.classList.remove('drop-target');
    });
}

function setupTableDropZone() {
    const table = document.getElementById('itemsTable');
    // Allow dropping on the table itself (append to end)
    table.ondragover = (e) => {
        if (dragCatalogItemId !== null || dragItemId !== null) {
            e.preventDefault();
            e.dataTransfer.dropEffect = 'move';
        }
    };
    table.ondrop = (e) => {
        // Only handle if not dropped on a specific row (those handle themselves)
        if (e.target.closest('tr[data-item-id]')) return;
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

    const shop = shops.find(s => s.shop_id === selectedShopId);
    const shopItemIds = new Set(shop ? shop.items.map(i => i.item_id) : []);

    for (const item of allItems) {
        if (filter && !item.name.toLowerCase().includes(filter) && !String(item.item_id).includes(filter)) continue;
        const inShop = shopItemIds.has(item.item_id);
        const li = document.createElement('li');
        if (inShop) li.className = 'in-shop';
        li.innerHTML = `
            <span class="cat-id">${item.item_id}</span>
            <span class="cat-name">${escapeHtml(item.name)}</span>
            <span class="cat-add">+</span>
        `;
        if (!inShop && selectedShopId !== null) {
            li.draggable = true;
            li.onclick = () => addItem(item.item_id);
            li.addEventListener('dragstart', (e) => {
                dragCatalogItemId = item.item_id;
                dragItemId = null;
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

async function addItem(itemId) {
    if (selectedShopId === null) return;
    const resp = await fetch('/api/shop/add_item', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ shop_id: selectedShopId, item_id: itemId })
    });
    const data = await resp.json();
    if (data.ok) {
        showStatus('Item added.', 'success');
        await loadData();
    } else {
        showStatus('Error: ' + data.error, 'error');
    }
}

async function removeItem(itemId) {
    if (selectedShopId === null) return;
    const resp = await fetch('/api/shop/remove_item', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ shop_id: selectedShopId, item_id: itemId })
    });
    const data = await resp.json();
    if (data.ok) {
        showStatus('Item removed.', 'success');
        await loadData();
    } else {
        showStatus('Error: ' + data.error, 'error');
    }
}

async function moveItem(itemId, direction) {
    if (selectedShopId === null) return;
    const resp = await fetch('/api/shop/move_item', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ shop_id: selectedShopId, item_id: itemId, direction: direction })
    });
    const data = await resp.json();
    if (data.ok) {
        await loadData();
    } else {
        showStatus('Error: ' + data.error, 'error');
    }
}

async function dropItem(draggedItemId, targetItemId) {
    if (selectedShopId === null) return;
    const resp = await fetch('/api/shop/move_item', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ shop_id: selectedShopId, item_id: draggedItemId, before_item_id: targetItemId })
    });
    const data = await resp.json();
    if (data.ok) {
        await loadData();
    } else {
        showStatus('Error: ' + data.error, 'error');
    }
}

async function addItemAt(itemId, beforeItemId) {
    if (selectedShopId === null) return;
    const resp = await fetch('/api/shop/add_item', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ shop_id: selectedShopId, item_id: itemId, before_item_id: beforeItemId })
    });
    const data = await resp.json();
    if (data.ok) {
        showStatus('Item added.', 'success');
        await loadData();
    } else {
        showStatus('Error: ' + data.error, 'error');
    }
}

async function addMapping() {
    if (selectedShopId === null) return;
    const input = document.getElementById('addNpcConfigId');
    const npcConfigId = parseInt(input.value);
    if (isNaN(npcConfigId) || npcConfigId < 0) {
        showStatus('Enter a valid NPC config ID.', 'error');
        return;
    }
    const resp = await fetch('/api/shop/add_mapping', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ shop_id: selectedShopId, npc_config_id: npcConfigId })
    });
    const data = await resp.json();
    if (data.ok) {
        input.value = '';
        showStatus('NPC mapping added.', 'success');
        await loadData();
    } else {
        showStatus('Error: ' + data.error, 'error');
    }
}

async function removeMapping(npcConfigId) {
    const resp = await fetch('/api/shop/remove_mapping', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ npc_config_id: npcConfigId })
    });
    const data = await resp.json();
    if (data.ok) {
        showStatus('NPC mapping removed.', 'success');
        await loadData();
    } else {
        showStatus('Error: ' + data.error, 'error');
    }
}

function showNewShopModal() {
    document.getElementById('newShopModal').classList.add('visible');
    document.getElementById('newShopId').value = '';
    document.getElementById('newShopId').focus();
}
function hideNewShopModal() {
    document.getElementById('newShopModal').classList.remove('visible');
}

async function createShop() {
    const shopId = parseInt(document.getElementById('newShopId').value);
    if (isNaN(shopId) || shopId < 1) {
        showStatus('Enter a valid shop ID.', 'error');
        return;
    }
    if (shops.find(s => s.shop_id === shopId)) {
        showStatus('Shop #' + shopId + ' already exists.', 'error');
        return;
    }
    const resp = await fetch('/api/shop/create', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ shop_id: shopId })
    });
    const data = await resp.json();
    if (data.ok) {
        hideNewShopModal();
        selectedShopId = shopId;
        showStatus('Shop #' + shopId + ' created.', 'success');
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
        elif path == "/api/shops":
            self._send_shops()
        else:
            self._send_error(404, "Not found")

    def do_POST(self):
        path = urlparse(self.path).path
        if path == "/api/shop/add_item":
            self._handle_add_item()
        elif path == "/api/shop/remove_item":
            self._handle_remove_item()
        elif path == "/api/shop/move_item":
            self._handle_move_item()
        elif path == "/api/shop/add_mapping":
            self._handle_add_mapping()
        elif path == "/api/shop/remove_mapping":
            self._handle_remove_mapping()
        elif path == "/api/shop/create":
            self._handle_create_shop()
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

    def _send_shops(self):
        conn = get_db()

        # Get all items for the catalog/search
        all_items_rows = conn.execute(
            "SELECT item_id, name FROM items ORDER BY name"
        ).fetchall()
        all_items = [{"item_id": r["item_id"], "name": r["name"]} for r in all_items_rows]

        # Get NPC names lookup
        npc_names = {}
        try:
            npc_rows = conn.execute(
                "SELECT npc_id, name FROM npc_configs ORDER BY npc_id"
            ).fetchall()
            for r in npc_rows:
                npc_names[r["npc_id"]] = r["name"]
        except Exception:
            pass

        # Get all shop_items ordered by sort_order
        shop_items_rows = conn.execute(
            "SELECT si.shop_id, si.item_id, i.name "
            "FROM shop_items si "
            "LEFT JOIN items i ON si.item_id = i.item_id "
            "ORDER BY si.shop_id, si.sort_order, si.item_id"
        ).fetchall()

        # Get all NPC mappings
        mapping_rows = conn.execute(
            "SELECT npc_config_id, shop_id, description FROM npc_shop_mapping ORDER BY shop_id, npc_config_id"
        ).fetchall()
        conn.close()

        # Build shop data
        shops_map = {}
        for r in shop_items_rows:
            sid = r["shop_id"]
            if sid not in shops_map:
                shops_map[sid] = {"shop_id": sid, "items": [], "npcs": []}
            shops_map[sid]["items"].append({
                "item_id": r["item_id"],
                "name": r["name"] or f"Unknown({r['item_id']})"
            })

        for r in mapping_rows:
            sid = r["shop_id"]
            if sid not in shops_map:
                shops_map[sid] = {"shop_id": sid, "items": [], "npcs": []}
            npc_config_id = r["npc_config_id"]
            shops_map[sid]["npcs"].append({
                "npc_config_id": npc_config_id,
                "npc_name": npc_names.get(npc_config_id, r["description"] or "")
            })

        shops = sorted(shops_map.values(), key=lambda s: s["shop_id"])
        self._send_json({"shops": shops, "all_items": all_items})

    def _handle_add_item(self):
        try:
            body = json.loads(self._read_body())
            shop_id = int(body["shop_id"])
            item_id = int(body["item_id"])
            before_item_id = body.get("before_item_id")  # optional: insert before this item

            conn = get_db()

            # Check if already in shop
            exists = conn.execute(
                "SELECT 1 FROM shop_items WHERE shop_id = ? AND item_id = ?",
                (shop_id, item_id)
            ).fetchone()
            if exists:
                conn.close()
                self._send_json({"ok": False, "error": "Item already in shop"})
                return

            if before_item_id is not None:
                before_item_id = int(before_item_id)
                # Get current order, insert before target
                rows = conn.execute(
                    "SELECT item_id FROM shop_items WHERE shop_id = ? ORDER BY sort_order, item_id",
                    (shop_id,)
                ).fetchall()
                order = [r["item_id"] for r in rows]
                if before_item_id in order:
                    idx = order.index(before_item_id)
                    order.insert(idx, item_id)
                else:
                    order.append(item_id)
                # Insert the new row
                conn.execute(
                    "INSERT INTO shop_items (shop_id, item_id, sort_order) VALUES (?, ?, ?)",
                    (shop_id, item_id, 0)
                )
                # Re-write all sort_order values
                for i, iid in enumerate(order):
                    conn.execute(
                        "UPDATE shop_items SET sort_order = ? WHERE shop_id = ? AND item_id = ?",
                        (i, shop_id, iid)
                    )
            else:
                # Append to end
                row = conn.execute(
                    "SELECT COALESCE(MAX(sort_order), -1) + 1 AS next_order FROM shop_items WHERE shop_id = ?",
                    (shop_id,)
                ).fetchone()
                conn.execute(
                    "INSERT INTO shop_items (shop_id, item_id, sort_order) VALUES (?, ?, ?)",
                    (shop_id, item_id, row["next_order"])
                )

            conn.commit()
            conn.close()
            self._send_json({"ok": True})
        except Exception as e:
            self._send_json({"ok": False, "error": str(e)})

    def _handle_remove_item(self):
        try:
            body = json.loads(self._read_body())
            shop_id = int(body["shop_id"])
            item_id = int(body["item_id"])
            conn = get_db()
            conn.execute(
                "DELETE FROM shop_items WHERE shop_id = ? AND item_id = ?",
                (shop_id, item_id)
            )
            # Re-normalize sort_order to close gaps
            rows = conn.execute(
                "SELECT item_id FROM shop_items WHERE shop_id = ? ORDER BY sort_order, item_id",
                (shop_id,)
            ).fetchall()
            for idx, r in enumerate(rows):
                conn.execute(
                    "UPDATE shop_items SET sort_order = ? WHERE shop_id = ? AND item_id = ?",
                    (idx, shop_id, r["item_id"])
                )
            conn.commit()
            conn.close()
            self._send_json({"ok": True})
        except Exception as e:
            self._send_json({"ok": False, "error": str(e)})

    def _handle_move_item(self):
        """Handle reordering: either up/down by 1, or drag-drop to before a target item."""
        try:
            body = json.loads(self._read_body())
            shop_id = int(body["shop_id"])
            item_id = int(body["item_id"])
            direction = body.get("direction")         # "up" or "down"
            before_item_id = body.get("before_item_id")  # drag-drop target

            conn = get_db()
            # Get current ordered list
            rows = conn.execute(
                "SELECT item_id FROM shop_items WHERE shop_id = ? ORDER BY sort_order, item_id",
                (shop_id,)
            ).fetchall()
            order = [r["item_id"] for r in rows]

            if item_id not in order:
                conn.close()
                self._send_json({"ok": False, "error": "Item not in shop"})
                return

            idx = order.index(item_id)

            if direction == "up" and idx > 0:
                order[idx], order[idx - 1] = order[idx - 1], order[idx]
            elif direction == "down" and idx < len(order) - 1:
                order[idx], order[idx + 1] = order[idx + 1], order[idx]
            elif before_item_id is not None:
                before_item_id = int(before_item_id)
                if before_item_id in order:
                    order.remove(item_id)
                    target_idx = order.index(before_item_id)
                    order.insert(target_idx, item_id)

            # Write new sort_order
            for i, iid in enumerate(order):
                conn.execute(
                    "UPDATE shop_items SET sort_order = ? WHERE shop_id = ? AND item_id = ?",
                    (i, shop_id, iid)
                )
            conn.commit()
            conn.close()
            self._send_json({"ok": True})
        except Exception as e:
            self._send_json({"ok": False, "error": str(e)})

    def _handle_add_mapping(self):
        try:
            body = json.loads(self._read_body())
            shop_id = int(body["shop_id"])
            npc_config_id = int(body["npc_config_id"])
            conn = get_db()
            existing = conn.execute(
                "SELECT shop_id FROM npc_shop_mapping WHERE npc_config_id = ?",
                (npc_config_id,)
            ).fetchone()
            if existing and existing["shop_id"] != shop_id:
                conn.close()
                self._send_json({
                    "ok": False,
                    "error": f"NPC config {npc_config_id} is already mapped to shop #{existing['shop_id']}"
                })
                return
            conn.execute(
                "INSERT OR REPLACE INTO npc_shop_mapping (npc_config_id, shop_id) VALUES (?, ?)",
                (npc_config_id, shop_id)
            )
            conn.commit()
            conn.close()
            self._send_json({"ok": True})
        except Exception as e:
            self._send_json({"ok": False, "error": str(e)})

    def _handle_remove_mapping(self):
        try:
            body = json.loads(self._read_body())
            npc_config_id = int(body["npc_config_id"])
            conn = get_db()
            conn.execute(
                "DELETE FROM npc_shop_mapping WHERE npc_config_id = ?",
                (npc_config_id,)
            )
            conn.commit()
            conn.close()
            self._send_json({"ok": True})
        except Exception as e:
            self._send_json({"ok": False, "error": str(e)})

    def _handle_create_shop(self):
        try:
            body = json.loads(self._read_body())
            shop_id = int(body["shop_id"])
            conn = get_db()
            existing = conn.execute(
                "SELECT COUNT(*) as cnt FROM shop_items WHERE shop_id = ?",
                (shop_id,)
            ).fetchone()
            if existing and existing["cnt"] > 0:
                conn.close()
                self._send_json({"ok": False, "error": f"Shop #{shop_id} already exists"})
                return
            existing_map = conn.execute(
                "SELECT COUNT(*) as cnt FROM npc_shop_mapping WHERE shop_id = ?",
                (shop_id,)
            ).fetchone()
            if existing_map and existing_map["cnt"] > 0:
                conn.close()
                self._send_json({"ok": False, "error": f"Shop #{shop_id} already exists (has NPC mappings)"})
                return
            conn.close()
            self._send_json({"ok": True})
        except Exception as e:
            self._send_json({"ok": False, "error": str(e)})


def main():
    port = 8081
    print(f"Shop Manager")
    print(f"Database: {os.path.abspath(DB_PATH)}")

    # Ensure sort_order column exists
    ensure_sort_order_column()

    print(f"Serving on http://localhost:{port}")
    server = HTTPServer(("127.0.0.1", port), RequestHandler)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down.")
        server.server_close()

if __name__ == "__main__":
    main()
