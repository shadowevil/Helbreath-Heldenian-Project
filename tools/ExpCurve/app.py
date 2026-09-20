"""
Exp Curve — Level XP Curve Visualizer & Formula Editor

Reads the level_exp formula from gamedata.db and max_level from
server_config.json, then serves a browser-based chart tool that lets
you visualize and experiment with XP curve formulas.

Usage: python Tools/ExpCurve/app.py
Then open http://localhost:8082 in your browser.
"""

import json
import sqlite3
from http.server import HTTPServer, BaseHTTPRequestHandler
from pathlib import Path

PORT = 8082
BINARIES = Path(__file__).resolve().parent / "../../Binaries/Server"
DB_PATH = BINARIES / "gamedata.db"
CONFIG_PATH = BINARIES / "server_config.json"


def get_config():
    """Read formula from DB and max_level from config."""
    formula = ""
    max_level = 180

    try:
        conn = sqlite3.connect(str(DB_PATH))
        cur = conn.cursor()
        cur.execute("SELECT expression FROM formulas WHERE formula_id = 'level_exp'")
        row = cur.fetchone()
        if row:
            formula = row[0]
        conn.close()
    except Exception as e:
        print(f"[expcurve] DB error: {e}")

    try:
        with open(str(CONFIG_PATH), "r") as f:
            cfg = json.load(f)
        max_level = cfg.get("character", {}).get("max_level", 180)
    except Exception as e:
        print(f"[expcurve] Config error: {e}")

    return {"formula": formula, "max_level": max_level}


def get_npcs():
    """Read all monsters with exp > 0 from DB, sorted by avg XP ascending."""
    npcs = []
    try:
        conn = sqlite3.connect(str(DB_PATH))
        cur = conn.cursor()
        cur.execute(
            "SELECT name, exp_min, exp_max, hp_min, hp_max FROM npc_configs "
            "WHERE exp_min > 0 ORDER BY (exp_min + exp_max) / 2"
        )
        for row in cur.fetchall():
            name, exp_min, exp_max, hp_min, hp_max = row
            npcs.append({
                "name": name,
                "exp_avg": (exp_min + exp_max) / 2,
                "exp_min": exp_min,
                "exp_max": exp_max,
                "hp_min": hp_min,
                "hp_max": hp_max,
            })
        conn.close()
    except Exception as e:
        print(f"[expcurve] NPC DB error: {e}")
    return npcs


PAGE_HTML = r"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Exp Curve</title>
<script src="https://cdn.jsdelivr.net/npm/chart.js@4"></script>
<script src="https://cdn.jsdelivr.net/npm/chartjs-plugin-zoom@2"></script>
<style>
* { box-sizing: border-box; margin: 0; padding: 0; }
html, body { height: 100%; }
body {
    font-family: 'Segoe UI', Tahoma, sans-serif;
    background: #181620;
    color: #ddd8d0;
}

/* Header */
.header {
    position: sticky;
    top: 0;
    z-index: 100;
    background: #22202c;
    border-bottom: 2px solid #3a3248;
    padding: 10px 20px;
    display: flex;
    align-items: center;
    gap: 16px;
}
.header h1 {
    font-size: 18px;
    font-weight: 700;
    color: #c8a850;
    white-space: nowrap;
}

/* Status bar */
.status-bar {
    position: sticky;
    top: 48px;
    z-index: 99;
    background: #22202c;
    border-bottom: 1px solid #3a3248;
    padding: 6px 20px;
    font-size: 13px;
    color: #887868;
    min-height: 30px;
    display: flex;
    align-items: center;
}
.status-bar.error {
    color: #e05050;
    background: #2a1820;
}

/* Formula bar */
.formula-bar {
    display: flex;
    gap: 10px;
    padding: 14px 20px;
    background: #22202c;
    border-bottom: 1px solid #3a3248;
    align-items: center;
    flex-wrap: wrap;
}
.formula-bar label {
    font-size: 13px;
    color: #887868;
    font-weight: 600;
    white-space: nowrap;
}
.formula-bar input {
    flex: 1;
    min-width: 300px;
    background: #181620;
    border: 1px solid #332e3a;
    color: #ddd8d0;
    padding: 8px 12px;
    font-size: 14px;
    font-family: 'Consolas', 'Courier New', monospace;
    border-radius: 4px;
    outline: none;
}
.formula-bar input:focus {
    border-color: #c8a850;
}
.btn-apply {
    background: #6e5530;
    color: #ddd8d0;
    border: none;
    padding: 8px 20px;
    font-size: 14px;
    font-weight: 600;
    border-radius: 4px;
    cursor: pointer;
    white-space: nowrap;
}
.btn-apply:hover {
    background: #c8a850;
    color: #181620;
}

/* Stats bar */
.stats-bar {
    display: flex;
    gap: 24px;
    padding: 10px 20px;
    background: #1e1c28;
    border-bottom: 1px solid #3a3248;
    flex-wrap: wrap;
}
.stat-item {
    display: flex;
    gap: 6px;
    align-items: baseline;
    font-size: 13px;
}
.stat-label {
    color: #887868;
}
.stat-value {
    color: #c8a850;
    font-weight: 600;
    font-family: 'Consolas', 'Courier New', monospace;
}

/* Monster selector bar */
.monster-bar {
    display: flex;
    gap: 12px;
    padding: 10px 20px;
    background: #1e1c28;
    border-bottom: 1px solid #3a3248;
    align-items: center;
    flex-wrap: wrap;
}
.monster-bar label {
    font-size: 13px;
    color: #887868;
    font-weight: 600;
    white-space: nowrap;
}
.monster-bar select {
    background: #181620;
    border: 1px solid #332e3a;
    color: #ddd8d0;
    padding: 6px 10px;
    font-size: 13px;
    border-radius: 4px;
    outline: none;
    min-width: 180px;
}
.monster-bar select:focus {
    border-color: #c8a850;
}
.monster-info {
    font-size: 13px;
    color: #887868;
    font-family: 'Consolas', 'Courier New', monospace;
}
.monster-info .val {
    color: #5a8ac8;
    font-weight: 600;
}

/* Charts */
.charts {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 16px;
    padding: 16px 20px;
}
@media (max-width: 1000px) {
    .charts { grid-template-columns: 1fr; }
}
.chart-panel {
    background: #22202c;
    border: 1px solid #3a3248;
    border-radius: 6px;
    padding: 14px;
}
.chart-panel h2 {
    font-size: 14px;
    color: #887868;
    margin-bottom: 10px;
    font-weight: 600;
}
.chart-panel canvas {
    width: 100% !important;
    max-height: 400px;
}
.chart-hint {
    font-size: 11px;
    color: #555;
    margin-top: 6px;
    text-align: center;
}

/* Zoom reset */
.zoom-controls {
    display: flex;
    justify-content: flex-end;
    padding: 0 20px 4px;
    gap: 8px;
}
.btn-small {
    background: #2a2838;
    color: #887868;
    border: 1px solid #3a3248;
    padding: 3px 12px;
    font-size: 12px;
    border-radius: 3px;
    cursor: pointer;
}
.btn-small:hover {
    background: #3a3248;
    color: #ddd8d0;
}

/* Drill-down panel */
.drill-panel {
    display: none;
    margin: 0 20px 20px;
    background: #22202c;
    border: 1px solid #3a3248;
    border-radius: 6px;
    padding: 16px;
}
.drill-panel.visible {
    display: block;
}
.drill-header {
    display: flex;
    align-items: center;
    gap: 12px;
    margin-bottom: 14px;
    flex-wrap: wrap;
}
.drill-header h2 {
    font-size: 15px;
    color: #c8a850;
    font-weight: 600;
}
.drill-header .drill-sub {
    font-size: 13px;
    color: #887868;
}
.drill-controls {
    display: flex;
    gap: 6px;
    margin-left: auto;
    align-items: center;
}
.drill-controls .range-label {
    font-size: 12px;
    color: #887868;
    margin-right: 4px;
}
.drill-chart-wrap {
    position: relative;
    width: 100%;
    overflow-x: auto;
}
.drill-chart-wrap canvas {
    width: 100% !important;
}
</style>
</head>
<body>

<div class="header">
    <h1>Exp Curve</h1>
</div>
<div class="status-bar" id="statusBar">Ready</div>

<div class="formula-bar">
    <label>FORMULA</label>
    <input type="text" id="formulaInput" spellcheck="false" placeholder="Loading...">
    <button class="btn-apply" id="btnApply">Apply</button>
</div>

<div class="stats-bar" id="statsBar">
    <div class="stat-item"><span class="stat-label">Max Level:</span><span class="stat-value" id="statMaxLevel">—</span></div>
    <div class="stat-item"><span class="stat-label">XP @ 50:</span><span class="stat-value" id="statXp50">—</span></div>
    <div class="stat-item"><span class="stat-label">XP @ 100:</span><span class="stat-value" id="statXp100">—</span></div>
    <div class="stat-item"><span class="stat-label">XP @ Max:</span><span class="stat-value" id="statXpMax">—</span></div>
    <div class="stat-item" id="statKills50Wrap" style="display:none"><span class="stat-label">Kills @ 50:</span><span class="stat-value" id="statKills50">—</span></div>
    <div class="stat-item" id="statKills100Wrap" style="display:none"><span class="stat-label">Kills @ 100:</span><span class="stat-value" id="statKills100">—</span></div>
    <div class="stat-item" id="statKillsMaxWrap" style="display:none"><span class="stat-label">Kills @ Max:</span><span class="stat-value" id="statKillsMax">—</span></div>
</div>

<div class="monster-bar">
    <label>MONSTER</label>
    <select id="npcSelect"><option value="">Loading...</option></select>
    <span class="monster-info" id="npcInfo"></span>
</div>

<div class="zoom-controls">
    <button class="btn-small" id="btnResetZoom" style="display:none">Reset Zoom</button>
</div>

<div class="charts" style="grid-template-columns: 1fr">
    <div class="chart-panel">
        <h2>XP To Next Level</h2>
        <canvas id="chartDelta"></canvas>
        <div class="chart-hint">Click a point to drill down — drag to zoom</div>
    </div>
</div>

<div class="drill-panel" id="drillPanel">
    <div class="drill-header">
        <button class="btn-small" id="btnDrillBack">Back</button>
        <h2 id="drillTitle">Level X → X+1</h2>
        <span class="drill-sub" id="drillSub"></span>
        <div class="drill-controls">
            <span class="range-label">Range:</span>
            <button class="btn-small" id="btnRangeMinus">-</button>
            <span id="drillRangeLabel" style="font-size:12px;color:#887868">1 level</span>
            <button class="btn-small" id="btnRangePlus">+</button>
        </div>
    </div>
    <div class="drill-chart-wrap">
        <canvas id="chartDrill"></canvas>
    </div>
</div>

<script>
// =========================================================================
// Formula engine — JS port of FormulaEngine.cpp
// =========================================================================

const TOKEN = {
    NUMBER: 'number', IDENT: 'identifier',
    PLUS: '+', MINUS: '-', STAR: '*', SLASH: '/',
    LPAREN: '(', RPAREN: ')', COMMA: ',', END: 'end'
};

function tokenize(expr) {
    const tokens = [];
    let i = 0;
    while (i < expr.length) {
        const c = expr[i];
        if (/\s/.test(c)) { i++; continue; }

        if (/\d/.test(c) || (c === '.' && i + 1 < expr.length && /\d/.test(expr[i + 1]))) {
            let start = i;
            while (i < expr.length && (/\d/.test(expr[i]) || expr[i] === '.')) i++;
            const text = expr.slice(start, i);
            tokens.push({ type: TOKEN.NUMBER, text, value: parseFloat(text) });
            continue;
        }

        if (/[a-zA-Z_]/.test(c)) {
            let start = i;
            while (i < expr.length && /[a-zA-Z0-9_]/.test(expr[i])) i++;
            tokens.push({ type: TOKEN.IDENT, text: expr.slice(start, i) });
            continue;
        }

        const ops = { '+': TOKEN.PLUS, '-': TOKEN.MINUS, '*': TOKEN.STAR, '/': TOKEN.SLASH,
                       '(': TOKEN.LPAREN, ')': TOKEN.RPAREN, ',': TOKEN.COMMA };
        if (ops[c]) {
            tokens.push({ type: ops[c], text: c });
            i++;
            continue;
        }

        throw new Error("Unexpected character '" + c + "'");
    }
    tokens.push({ type: TOKEN.END, text: '' });
    return tokens;
}

function parse(tokens) {
    let pos = 0;
    const cur = () => tokens[pos];
    const adv = () => tokens[pos++];

    function parseAdditive() {
        let left = parseMultiplicative();
        while (cur().type === TOKEN.PLUS || cur().type === TOKEN.MINUS) {
            const op = adv().type;
            const right = parseMultiplicative();
            left = { type: 'binop', op, left, right };
        }
        return left;
    }

    function parseMultiplicative() {
        let left = parseUnary();
        while (cur().type === TOKEN.STAR || cur().type === TOKEN.SLASH) {
            const op = adv().type;
            const right = parseUnary();
            left = { type: 'binop', op, left, right };
        }
        return left;
    }

    function parseUnary() {
        if (cur().type === TOKEN.MINUS) {
            adv();
            const operand = parseUnary();
            return { type: 'neg', child: operand };
        }
        return parsePrimary();
    }

    function parsePrimary() {
        if (cur().type === TOKEN.NUMBER) {
            const val = cur().value;
            adv();
            return { type: 'num', value: val };
        }

        if (cur().type === TOKEN.IDENT) {
            const name = cur().text;
            adv();

            if (cur().type === TOKEN.LPAREN) {
                adv(); // consume '('
                const args = [];
                if (cur().type !== TOKEN.RPAREN) {
                    args.push(parseAdditive());
                    while (cur().type === TOKEN.COMMA) {
                        adv();
                        args.push(parseAdditive());
                    }
                }
                if (cur().type !== TOKEN.RPAREN)
                    throw new Error("Expected ')' after function arguments");
                adv(); // consume ')'
                return { type: 'call', name, args };
            }

            return { type: 'var', name };
        }

        if (cur().type === TOKEN.LPAREN) {
            adv();
            const node = parseAdditive();
            if (cur().type !== TOKEN.RPAREN)
                throw new Error("Expected ')'");
            adv();
            return node;
        }

        throw new Error("Unexpected token: '" + cur().text + "'");
    }

    const ast = parseAdditive();
    if (cur().type !== TOKEN.END)
        throw new Error("Unexpected token: '" + cur().text + "'");
    return ast;
}

// Built-in 1-arg functions
const FUNC1 = {
    trunc: v => Math.trunc(v),
    floor: v => Math.floor(v),
    ceil:  v => Math.ceil(v),
    round: v => Math.round(v),
    abs:   v => Math.abs(v),
    sqrt:  v => Math.sqrt(v),
    log:   v => Math.log(v),
    log2:  v => Math.log2(v),
    log10: v => Math.log10(v),
};

// Built-in 2-arg functions
const FUNC2 = {
    pow: (a, b) => Math.pow(a, b),
    min: (a, b) => Math.min(a, b),
    max: (a, b) => Math.max(a, b),
};

function evaluate(node, vars) {
    switch (node.type) {
        case 'num': return node.value;
        case 'var': return vars[node.name] !== undefined ? vars[node.name] : 0;
        case 'neg': return -evaluate(node.child, vars);
        case 'binop': {
            const l = evaluate(node.left, vars);
            const r = evaluate(node.right, vars);
            if (node.op === '+') return l + r;
            if (node.op === '-') return l - r;
            if (node.op === '*') return l * r;
            if (node.op === '/') return r !== 0 ? l / r : 0;
            return 0;
        }
        case 'call': {
            const name = node.name;
            const args = node.args;

            // 1-arg builtins
            if (FUNC1[name]) {
                if (args.length !== 1) throw new Error(name + "() requires 1 argument");
                return FUNC1[name](evaluate(args[0], vars));
            }

            // 2-arg builtins
            if (FUNC2[name]) {
                if (args.length !== 2) throw new Error(name + "() requires 2 arguments");
                return FUNC2[name](evaluate(args[0], vars), evaluate(args[1], vars));
            }

            // clamp(value, lo, hi)
            if (name === 'clamp') {
                if (args.length !== 3) throw new Error("clamp() requires 3 arguments");
                const val = evaluate(args[0], vars);
                const lo = evaluate(args[1], vars);
                const hi = evaluate(args[2], vars);
                return Math.min(Math.max(val, lo), hi);
            }

            // sum(start, end, body) — loop var 'i'
            if (name === 'sum') {
                if (args.length !== 3) throw new Error("sum() requires 3 arguments");
                const start = Math.trunc(evaluate(args[0], vars));
                const end = Math.trunc(evaluate(args[1], vars));
                let total = 0;
                const localVars = Object.assign({}, vars);
                for (let i = start; i <= end; i++) {
                    localVars.i = i;
                    total += evaluate(args[2], localVars);
                }
                return total;
            }

            throw new Error("Unknown function: " + name);
        }
    }
    return 0;
}

function evalFormula(expression, level, maxLevel) {
    const tokens = tokenize(expression);
    const ast = parse(tokens);
    const vars = { level, max_level: maxLevel, i: 0 };
    return Math.trunc(evaluate(ast, vars));
}

// =========================================================================
// Chart setup
// =========================================================================

const chartColors = {
    line: '#c8a850',
    line2: '#5a8ac8',
    grid: '#2a2838',
    text: '#887868',
};

function killsNeeded(xpDelta, npc) {
    if (!npc || npc.exp_avg <= 0) return 0;
    return Math.ceil(xpDelta / npc.exp_avg);
}

// Shared zoom/pan plugin config
const zoomPluginOpts = {
    zoom: {
        drag: {
            enabled: true,
            backgroundColor: 'rgba(200,168,80,0.12)',
            borderColor: 'rgba(200,168,80,0.4)',
            borderWidth: 1,
        },
        mode: 'x',
        onZoomComplete: () => {
            document.getElementById('btnResetZoom').style.display = '';
        },
    },
};

function makeTooltipCallbacks() {
    return {
        title: (items) => {
            if (!items.length) return '';
            return 'Level ' + items[0].label;
        },
        label: (ctx) => {
            const val = ctx.parsed.y;
            const lines = ['XP needed: ' + val.toLocaleString()];
            if (selectedNpc) {
                const k = killsNeeded(val, selectedNpc);
                lines.push(selectedNpc.name + ' kills: ' + k.toLocaleString());
                lines.push('(' + selectedNpc.name + ' avg: ' + Math.round(selectedNpc.exp_avg).toLocaleString() + ' XP)');
            }
            return lines;
        },
        afterLabel: () => '',
    };
}

const chartDelta = new Chart(document.getElementById('chartDelta'), {
    type: 'line',
    data: {
        labels: [],
        datasets: [{
            label: 'Level Cost',
            data: [],
            borderColor: chartColors.line2,
            backgroundColor: 'rgba(90,138,200,0.08)',
            fill: true,
            pointRadius: 2,
            pointHoverRadius: 6,
            pointHitRadius: 10,
            borderWidth: 2,
            tension: 0.1,
        }]
    },
    options: {
        responsive: true,
        maintainAspectRatio: false,
        onClick: (evt, elems) => {
            if (elems.length > 0) {
                const idx = elems[0].index;
                const level = chartDelta.data.labels[idx];
                showDrillDown(level);
            }
        },
        plugins: {
            legend: { display: false },
            tooltip: {
                callbacks: makeTooltipCallbacks(),
                bodyFont: { family: "'Consolas', 'Courier New', monospace", size: 12 },
                titleFont: { size: 13, weight: 'bold' },
                backgroundColor: '#22202c',
                borderColor: '#3a3248',
                borderWidth: 1,
                padding: 10,
            },
            zoom: zoomPluginOpts,
        },
        scales: {
            x: {
                title: { display: true, text: 'Level', color: chartColors.text },
                ticks: { color: chartColors.text, maxTicksLimit: 20 },
                grid: { color: chartColors.grid },
            },
            y: {
                title: { display: true, text: 'XP', color: chartColors.text },
                ticks: {
                    color: chartColors.text,
                    callback: v => v >= 1e6 ? (v / 1e6).toFixed(1) + 'M' : v >= 1e3 ? (v / 1e3).toFixed(0) + 'K' : v,
                },
                grid: { color: chartColors.grid },
            },
        },
    }
});

// =========================================================================
// App logic
// =========================================================================

let maxLevel = 180;
let currentFormula = '';
let statusTimer = null;
let npcs = [];
let selectedNpc = null;
let deltaData = [];

// Drill-down state
let drillLevel = null;
let drillRange = 1;
let drillChart = null;

function setStatus(msg, isError) {
    const bar = document.getElementById('statusBar');
    bar.textContent = msg;
    bar.classList.toggle('error', !!isError);
    if (statusTimer) clearTimeout(statusTimer);
    if (!isError) {
        statusTimer = setTimeout(() => {
            bar.textContent = 'Ready';
            bar.classList.remove('error');
        }, 4000);
    }
}

function formatXp(n) {
    if (n >= 1e9) return (n / 1e9).toFixed(2) + 'B';
    if (n >= 1e6) return (n / 1e6).toFixed(2) + 'M';
    if (n >= 1e3) return (n / 1e3).toFixed(1) + 'K';
    return String(n);
}

function formatKills(n) {
    if (n >= 1e6) return (n / 1e6).toFixed(1) + 'M';
    if (n >= 1e3) return (n / 1e3).toFixed(1) + 'K';
    return String(n);
}

function updateKillStats() {
    const hasNpc = !!selectedNpc;
    document.getElementById('statKills50Wrap').style.display = hasNpc ? '' : 'none';
    document.getElementById('statKills100Wrap').style.display = hasNpc ? '' : 'none';
    document.getElementById('statKillsMaxWrap').style.display = hasNpc ? '' : 'none';

    if (!hasNpc || deltaData.length === 0) return;

    document.getElementById('statKills50').textContent =
        maxLevel >= 50 ? formatKills(killsNeeded(deltaData[49], selectedNpc)) : '—';
    document.getElementById('statKills100').textContent =
        maxLevel >= 100 ? formatKills(killsNeeded(deltaData[99], selectedNpc)) : '—';
    document.getElementById('statKillsMax').textContent =
        formatKills(killsNeeded(deltaData[deltaData.length - 1], selectedNpc));
}

function updateNpcInfo() {
    const info = document.getElementById('npcInfo');
    if (!selectedNpc) {
        info.textContent = '';
        return;
    }
    info.innerHTML =
        'Avg XP: <span class="val">' + Math.round(selectedNpc.exp_avg).toLocaleString() + '</span>' +
        '  (Min: ' + selectedNpc.exp_min.toLocaleString() +
        ' / Max: ' + selectedNpc.exp_max.toLocaleString() + ')' +
        '  HP: <span class="val">' + selectedNpc.hp_min + '-' + selectedNpc.hp_max + '</span>';
}

function updateCharts(formula) {
    const labels = [];
    deltaData = [];

    for (let lvl = 1; lvl <= maxLevel; lvl++) {
        labels.push(lvl);
        deltaData.push(evalFormula(formula, lvl, maxLevel));
    }

    chartDelta.data.labels = labels;
    chartDelta.data.datasets[0].data = deltaData;
    chartDelta.update();

    // Stats
    document.getElementById('statMaxLevel').textContent = maxLevel;
    document.getElementById('statXp50').textContent = maxLevel >= 50 ? formatXp(deltaData[49]) : '—';
    document.getElementById('statXp100').textContent = maxLevel >= 100 ? formatXp(deltaData[99]) : '—';
    document.getElementById('statXpMax').textContent = formatXp(deltaData[deltaData.length - 1]);

    updateKillStats();

    currentFormula = formula;
    setStatus('Formula applied — ' + maxLevel + ' levels computed');

    // Refresh drill-down if visible
    if (drillLevel !== null) {
        renderDrillDown();
    }
}

function applyFormula() {
    const formula = document.getElementById('formulaInput').value.trim();
    if (!formula) {
        setStatus('Formula is empty', true);
        return;
    }
    try {
        updateCharts(formula);
    } catch (e) {
        setStatus('Error: ' + e.message, true);
    }
}

// =========================================================================
// Monster selector
// =========================================================================

function populateNpcDropdown() {
    const sel = document.getElementById('npcSelect');
    sel.innerHTML = '';
    const opt0 = document.createElement('option');
    opt0.value = '';
    opt0.textContent = '— None —';
    sel.appendChild(opt0);

    npcs.forEach((npc, idx) => {
        const opt = document.createElement('option');
        opt.value = idx;
        opt.textContent = npc.name + ' (' + Math.round(npc.exp_avg) + ' avg)';
        sel.appendChild(opt);
    });

    // Default: pick a mid-tier monster (~index at 40% through the list)
    if (npcs.length > 0) {
        const midIdx = Math.floor(npcs.length * 0.4);
        sel.value = midIdx;
        selectedNpc = npcs[midIdx];
        updateNpcInfo();
        updateKillStats();
    }
}

document.getElementById('npcSelect').addEventListener('change', function() {
    const idx = this.value;
    if (idx === '') {
        selectedNpc = null;
    } else {
        selectedNpc = npcs[parseInt(idx)];
    }
    updateNpcInfo();
    updateKillStats();
    // Refresh chart to update tooltips
    chartDelta.update();
    // Refresh drill-down if visible
    if (drillLevel !== null) {
        renderDrillDown();
    }
});

// =========================================================================
// Zoom reset
// =========================================================================

document.getElementById('btnResetZoom').addEventListener('click', function() {
    chartDelta.resetZoom();
    this.style.display = 'none';
});

// =========================================================================
// Drill-down panel
// =========================================================================

function getDeltaForRange(levelStart, levelEnd) {
    // Average delta across the range [levelStart, levelEnd] inclusive
    // Levels are 1-based, deltaData is 0-indexed
    let sum = 0;
    let count = 0;
    for (let lvl = levelStart; lvl <= levelEnd && lvl <= maxLevel; lvl++) {
        const idx = lvl - 1;
        if (idx >= 0 && idx < deltaData.length) {
            sum += deltaData[idx];
            count++;
        }
    }
    return count > 0 ? sum / count : 0;
}

function getBarColor(kills, maxKills) {
    if (maxKills <= 0) return '#555';
    const ratio = kills / maxKills;
    if (ratio <= 0.1) return '#c8a850';   // gold — few kills
    if (ratio <= 0.3) return '#7a9a4a';   // green
    if (ratio <= 0.6) return '#5a8ac8';   // blue — moderate
    return '#554a60';                      // muted — many
}

function showDrillDown(level) {
    drillLevel = level;
    drillRange = 1;
    renderDrillDown();
    document.getElementById('drillPanel').classList.add('visible');
    document.getElementById('drillPanel').scrollIntoView({ behavior: 'smooth', block: 'nearest' });
}

function hideDrillDown() {
    drillLevel = null;
    document.getElementById('drillPanel').classList.remove('visible');
    if (drillChart) {
        drillChart.destroy();
        drillChart = null;
    }
}

function renderDrillDown() {
    if (drillLevel === null || deltaData.length === 0) return;

    const rangeStart = Math.max(1, drillLevel - Math.floor((drillRange - 1) / 2));
    const rangeEnd = Math.min(maxLevel, rangeStart + drillRange - 1);
    const avgDelta = getDeltaForRange(rangeStart, rangeEnd);

    // Title
    if (drillRange === 1) {
        document.getElementById('drillTitle').textContent =
            'Level ' + drillLevel + ' \u2192 ' + (drillLevel + 1);
        document.getElementById('drillSub').textContent =
            Math.round(avgDelta).toLocaleString() + ' XP needed';
    } else {
        document.getElementById('drillTitle').textContent =
            'Levels ' + rangeStart + ' \u2192 ' + rangeEnd;
        document.getElementById('drillSub').textContent =
            'Avg ' + Math.round(avgDelta).toLocaleString() + ' XP per level';
    }

    document.getElementById('drillRangeLabel').textContent =
        drillRange === 1 ? '1 level' : drillRange + ' levels';

    // Calculate kills for each NPC
    const entries = npcs.map(npc => ({
        name: npc.name,
        kills: killsNeeded(avgDelta, npc),
        exp_avg: npc.exp_avg,
        hp_min: npc.hp_min,
        hp_max: npc.hp_max,
    }));
    entries.sort((a, b) => a.kills - b.kills);

    const maxKills = entries.length > 0 ? entries[entries.length - 1].kills : 1;

    const labels = entries.map(e => e.name);
    const data = entries.map(e => e.kills);
    const bgColors = entries.map(e => getBarColor(e.kills, maxKills));

    // Dynamic height based on number of bars
    const barHeight = 22;
    const chartHeight = Math.max(300, entries.length * barHeight + 60);
    const canvas = document.getElementById('chartDrill');
    canvas.style.height = chartHeight + 'px';
    canvas.parentElement.style.height = (chartHeight + 20) + 'px';

    if (drillChart) {
        drillChart.destroy();
    }

    drillChart = new Chart(canvas, {
        type: 'bar',
        data: {
            labels: labels,
            datasets: [{
                label: 'Kills needed',
                data: data,
                backgroundColor: bgColors,
                borderWidth: 0,
                barPercentage: 0.8,
                categoryPercentage: 0.9,
            }]
        },
        options: {
            indexAxis: 'y',
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
                legend: { display: false },
                tooltip: {
                    callbacks: {
                        label: function(ctx) {
                            const e = entries[ctx.dataIndex];
                            return [
                                'Kills: ' + e.kills.toLocaleString(),
                                'Avg XP: ' + Math.round(e.exp_avg).toLocaleString(),
                                'HP: ' + e.hp_min + '-' + e.hp_max,
                            ];
                        }
                    },
                    bodyFont: { family: "'Consolas', 'Courier New', monospace", size: 12 },
                    backgroundColor: '#22202c',
                    borderColor: '#3a3248',
                    borderWidth: 1,
                    padding: 10,
                },
            },
            scales: {
                x: {
                    title: { display: true, text: 'Kills Needed', color: chartColors.text },
                    ticks: {
                        color: chartColors.text,
                        callback: v => v >= 1e6 ? (v / 1e6).toFixed(1) + 'M' : v >= 1e3 ? (v / 1e3).toFixed(0) + 'K' : v,
                    },
                    grid: { color: chartColors.grid },
                },
                y: {
                    ticks: {
                        color: '#ddd8d0',
                        font: { size: 11, family: "'Segoe UI', Tahoma, sans-serif" },
                    },
                    grid: { display: false },
                },
            },
        },
    });
}

document.getElementById('btnDrillBack').addEventListener('click', hideDrillDown);

document.getElementById('btnRangeMinus').addEventListener('click', function() {
    if (drillRange > 1) {
        if (drillRange <= 5) drillRange = 1;
        else if (drillRange <= 10) drillRange = 5;
        else drillRange = 10;
        renderDrillDown();
    }
});

document.getElementById('btnRangePlus').addEventListener('click', function() {
    if (drillRange < 10) {
        if (drillRange === 1) drillRange = 5;
        else drillRange = 10;
    } else if (drillRange < 20) {
        drillRange = 20;
    } else if (drillRange < 50) {
        drillRange = 50;
    }
    renderDrillDown();
});

// =========================================================================
// Event listeners
// =========================================================================

document.getElementById('btnApply').addEventListener('click', applyFormula);
document.getElementById('formulaInput').addEventListener('keydown', e => {
    if (e.key === 'Enter') applyFormula();
});

// =========================================================================
// Initial load
// =========================================================================

Promise.all([
    fetch('/api/config').then(r => r.json()),
    fetch('/api/npcs').then(r => r.json()),
]).then(([configData, npcData]) => {
    // Config
    maxLevel = configData.max_level || 180;
    const formula = configData.formula || '';
    document.getElementById('formulaInput').value = formula;

    // NPCs
    npcs = npcData;
    populateNpcDropdown();

    // Render charts
    if (formula) {
        try {
            updateCharts(formula);
        } catch (e) {
            setStatus('Error evaluating DB formula: ' + e.message, true);
        }
    } else {
        setStatus('No level_exp formula found in database', true);
    }
}).catch(e => {
    setStatus('Failed to load: ' + e.message, true);
});
</script>
</body>
</html>"""


class RequestHandler(BaseHTTPRequestHandler):

    def log_message(self, fmt, *args):
        print(f"[expcurve] {fmt % args}")

    def do_GET(self):
        if self.path == "/" or self.path.startswith("/?"):
            self._serve_html()
        elif self.path == "/api/config":
            self._serve_config()
        elif self.path == "/api/npcs":
            self._serve_npcs()
        else:
            self.send_error(404)

    def _serve_html(self):
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.end_headers()
        self.wfile.write(PAGE_HTML.encode())

    def _serve_config(self):
        data = get_config()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.end_headers()
        self.wfile.write(json.dumps(data).encode())

    def _serve_npcs(self):
        data = get_npcs()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.end_headers()
        self.wfile.write(json.dumps(data).encode())


def main():
    HTTPServer.allow_reuse_address = True
    server = HTTPServer(("127.0.0.1", PORT), RequestHandler)
    print(f"Exp Curve running on http://localhost:{PORT}")
    server.serve_forever()


if __name__ == "__main__":
    main()
