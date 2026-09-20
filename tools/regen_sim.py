"""
Regen Algorithm Simulator
Run: python PLANS/regen_sim.py
Opens a browser with interactive line graphs comparing custom regen formulas.
"""

import http.server
import threading
import webbrowser

PORT = 8787

HTML = r"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<title>Regen Algorithm Simulator</title>
<script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.1/dist/chart.umd.min.js"></script>
<style>
* { box-sizing: border-box; margin: 0; padding: 0; }
body { font-family: 'Segoe UI', Tahoma, sans-serif; background: #1a1a2e; color: #e0e0e0; padding: 20px; }
h1 { text-align: center; margin-bottom: 5px; color: #e94560; }
.subtitle { text-align: center; margin-bottom: 20px; color: #888; font-size: 14px; }

.section-label {
    font-size: 11px; text-transform: uppercase; letter-spacing: 2px;
    color: #555; text-align: center; margin: 15px 0 8px 0;
}

.panel {
    background: #16213e; padding: 18px; border-radius: 10px; margin-bottom: 12px;
}

.stats-row {
    display: flex; flex-wrap: wrap; gap: 10px; justify-content: center;
}
.stat-input { display: flex; flex-direction: column; align-items: center; min-width: 80px; }
.stat-input label { font-size: 10px; color: #aaa; text-transform: uppercase; letter-spacing: 1px; margin-bottom: 3px; }
.stat-input input {
    width: 70px; text-align: center; background: #0f1a30; border: 1px solid #2a3a5a;
    color: #e94560; font-size: 18px; font-weight: bold; border-radius: 5px; padding: 4px;
    font-family: 'Consolas', monospace;
}
.stat-input input:focus { border-color: #e94560; outline: none; }

.formula-section {
    background: #1a2744; border-radius: 10px; padding: 18px; margin-bottom: 12px;
}

.formula-row {
    display: flex; gap: 10px; align-items: flex-end; margin-bottom: 10px;
}
.formula-row .formula-input-wrap { flex: 1; }
.formula-row label { font-size: 11px; color: #aaa; text-transform: uppercase; letter-spacing: 1px; display: block; margin-bottom: 4px; }
.formula-row input[type=text] {
    width: 100%; background: #0f1a30; border: 1px solid #2a3a5a;
    color: #e0e0e0; font-size: 15px; border-radius: 5px; padding: 8px 12px;
    font-family: 'Consolas', monospace;
}
.formula-row input[type=text]:focus { border-color: #e94560; outline: none; }

.formula-error { color: #ff4444; font-size: 12px; margin-top: 4px; min-height: 16px; font-family: 'Consolas', monospace; }

.sim-controls {
    display: flex; gap: 10px; align-items: center; justify-content: center; flex-wrap: wrap;
}
.sim-controls label { font-size: 11px; color: #aaa; text-transform: uppercase; letter-spacing: 1px; }
.sim-controls input {
    width: 60px; text-align: center; background: #0f1a30; border: 1px solid #2a3a5a;
    color: #e94560; font-size: 16px; font-weight: bold; border-radius: 5px; padding: 4px;
    font-family: 'Consolas', monospace;
}
.sim-controls input:focus { border-color: #e94560; outline: none; }
.sim-controls .toggle-label {
    font-size: 13px; color: #aaa; cursor: pointer; user-select: none;
}
.sim-controls .toggle-label input { width: auto; margin-right: 4px; accent-color: #e94560; }

button {
    background: #e94560; color: white; border: none; padding: 10px 24px;
    border-radius: 6px; font-size: 14px; cursor: pointer; font-weight: bold;
}
button:hover { background: #c73650; }
button.secondary { background: #2a3a5a; }
button.secondary:hover { background: #3a4a6a; }
button.keep { background: #1a8a4a; }
button.keep:hover { background: #15703d; }

.ceiling-display {
    display: flex; gap: 30px; justify-content: center; margin: 10px 0; font-size: 13px; color: #aaa;
}
.ceiling-display span { font-weight: bold; color: #e94560; }

.charts { display: flex; flex-direction: column; gap: 20px; margin-top: 10px; }
.chart-container {
    background: #16213e; border-radius: 10px; padding: 20px;
    position: relative; height: 380px;
}

.legend-note { text-align: center; color: #666; font-size: 12px; margin-top: 10px; }

.stats-grid {
    display: flex; flex-wrap: wrap; gap: 10px; justify-content: center;
    margin-top: 20px;
}
.stat-card {
    background: #16213e; border-radius: 8px; padding: 15px; text-align: center;
    min-width: 180px; flex: 1; max-width: 250px;
}
.stat-card.current {
    background: #1e2d52;
}
.stat-card h3 {
    font-size: 11px; color: #888; text-transform: uppercase; letter-spacing: 1px;
    margin-bottom: 8px; white-space: nowrap; overflow: hidden; text-overflow: ellipsis;
}
.stat-card .stat-row { display: flex; justify-content: space-between; font-size: 13px; padding: 3px 0; }
.stat-card .stat-label { color: #aaa; }
.stat-card .stat-value { font-weight: bold; }

.kept-list {
    margin-top: 10px;
}
.kept-item {
    display: flex; align-items: center; gap: 8px; padding: 6px 10px;
    background: #0f1a30; border-radius: 5px; margin-bottom: 4px; font-size: 13px;
}
.kept-item .kept-color {
    width: 12px; height: 12px; border-radius: 2px; flex-shrink: 0;
}
.kept-item .kept-formula {
    flex: 1; font-family: 'Consolas', monospace; color: #ccc;
    overflow: hidden; text-overflow: ellipsis; white-space: nowrap;
}
.kept-item button { padding: 3px 10px; font-size: 11px; }

.help-text {
    background: #0f1a30; border-radius: 8px; padding: 14px; margin-top: 20px;
    font-size: 12px; color: #888; line-height: 1.6;
}
.help-text code { color: #e94560; background: #1a2744; padding: 1px 5px; border-radius: 3px; }
.help-text h4 { color: #aaa; margin-bottom: 6px; font-size: 13px; }
</style>
</head>
<body>
<h1>Regen Algorithm Simulator</h1>
<p class="subtitle">Write any formula using character stats and C++ math operations</p>

<!-- Character Stats -->
<div class="section-label">Character Stats</div>
<div class="panel">
    <div class="stats-row">
        <div class="stat-input"><label>STR</label><input type="number" id="stat-str" value="14" min="1" max="999"></div>
        <div class="stat-input"><label>VIT</label><input type="number" id="stat-vit" value="14" min="1" max="999"></div>
        <div class="stat-input"><label>INT</label><input type="number" id="stat-int" value="10" min="1" max="999"></div>
        <div class="stat-input"><label>DEX</label><input type="number" id="stat-dex" value="10" min="1" max="999"></div>
        <div class="stat-input"><label>MAG</label><input type="number" id="stat-mag" value="10" min="1" max="999"></div>
        <div class="stat-input"><label>CHA</label><input type="number" id="stat-cha" value="10" min="1" max="999"></div>
        <div class="stat-input"><label>Level</label><input type="number" id="stat-level" value="1" min="1" max="999"></div>
    </div>
</div>

<!-- Formula -->
<div class="section-label">Formula</div>
<div class="formula-section">
    <div class="formula-row">
        <div class="formula-input-wrap">
            <label>Regen Ceiling Formula</label>
            <input type="text" id="formula" value="vit * 2 + level * 2.5 + str * 0.667" spellcheck="false">
            <div class="formula-error" id="formula-error"></div>
        </div>
    </div>
    <div class="formula-row">
        <div class="formula-input-wrap">
            <label>Regen Floor Formula <span style="color:#555;font-size:10px;text-transform:none">(minimum per tick, 0 = no floor)</span></label>
            <input type="text" id="formula-floor" value="floor(vit / 2)" spellcheck="false">
            <div class="formula-error" id="formula-floor-error"></div>
        </div>
    </div>
    <div class="formula-row">
        <div class="formula-input-wrap">
            <label>Floor Roll Range <span style="color:#555;font-size:10px;text-transform:none">(floor + dice(0, range) — adds variance to the floor each tick)</span></label>
            <input type="text" id="formula-floor-range" value="floor(vit * 0.25)" spellcheck="false">
            <div class="formula-error" id="formula-floor-range-error"></div>
        </div>
    </div>
    <div class="sim-controls">
        <label>Ticks <input type="number" id="num-ticks" value="50" min="5" max="500"></label>
        <label>Rolls <input type="number" id="num-rolls" value="3" min="2" max="20"></label>
        <label class="toggle-label"><input type="checkbox" id="show-raw" checked> Show Raw</label>
        <label class="toggle-label"><input type="checkbox" id="show-nroll" checked> Show N-Roll</label>
        <button onclick="simulate()">Roll</button>
        <button class="keep" onclick="keepCurrent()">Keep</button>
        <button class="secondary" onclick="clearKept()">Clear Kept</button>
        <button class="secondary" onclick="exportData()" style="background:#1a6a8a">Export</button>
        <span id="export-status" style="font-size:12px;color:#4a9;display:none">Saved to regen_data.md</span>
    </div>
</div>

<!-- Kept formulas list -->
<div class="kept-list" id="keptList"></div>

<div class="ceiling-display">
    Current floor: <span id="currentFloor">0</span> &nbsp;|&nbsp;
    Floor range: +<span id="currentFloorRange">0</span> &nbsp;|&nbsp;
    Current ceiling: <span id="currentCeiling">40</span>
</div>

<div class="charts">
    <div class="chart-container"><canvas id="perTickChart"></canvas></div>
    <div class="chart-container"><canvas id="cumulativeChart"></canvas></div>
</div>

<div class="legend-note">Solid thick = current formula | Thin/dashed = kept formulas for comparison</div>

<div class="stats-grid" id="statsGrid"></div>

<!-- Help -->
<div class="help-text">
    <h4>Formula Reference</h4>
    <b>Stats:</b> <code>str</code> <code>vit</code> <code>int</code> <code>dex</code> <code>mag</code> <code>cha</code> <code>level</code><br>
    <b>Operators:</b> <code>+</code> <code>-</code> <code>*</code> <code>/</code> <code>%</code> (modulo) <code>**</code> (power)<br>
    <b>Functions:</b> <code>sqrt(x)</code> <code>floor(x)</code> <code>ceil(x)</code> <code>round(x)</code> <code>abs(x)</code> <code>min(a,b)</code> <code>max(a,b)</code> <code>log(x)</code> <code>log2(x)</code> <code>pow(a,b)</code> <code>clamp(x,lo,hi)</code><br>
    <b>Ternary:</b> <code>(level > 60 ? 0 : level <= 20 ? 15 : 10)</code><br>
    <b>Examples:</b><br>
    <code>vit * 2 + level * 2.5 + str * 0.667</code><br>
    <code>200 * vit / (vit + 100) + level * 1.5</code> (diminishing returns)<br>
    <code>sqrt(vit) * 10 + mag * 0.5</code><br>
    <code>max(vit / 2, 10) + level * 2 + (level <= 20 ? 15 : level <= 60 ? 5 : 0)</code>
</div>

<script>
// --- Safe formula evaluator ---
function buildEvaluator(formulaStr) {
    // Whitelist of allowed tokens
    const allowed = /^[\s\d\.\+\-\*\/\%\(\)\,\?\:\>\<\=\!\&\|a-zA-Z_]+$/;
    if (!allowed.test(formulaStr)) {
        throw new Error('Invalid characters in formula');
    }

    // Replace ** with Math.pow
    let expr = formulaStr.replace(/(\w+|\)|\d)\s*\*\*\s*(\w+|\(|[\d.])/g, 'Math.pow($1,$2)');

    // Replace math functions
    const funcMap = {
        'sqrt': 'Math.sqrt',
        'floor': 'Math.floor',
        'ceil': 'Math.ceil',
        'round': 'Math.round',
        'abs': 'Math.abs',
        'min': 'Math.min',
        'max': 'Math.max',
        'log': 'Math.log',
        'log2': 'Math.log2',
        'log10': 'Math.log10',
        'pow': 'Math.pow',
        'sin': 'Math.sin',
        'cos': 'Math.cos',
        'tan': 'Math.tan',
    };
    for (const [fn, repl] of Object.entries(funcMap)) {
        const re = new RegExp('\\b' + fn + '\\s*\\(', 'g');
        expr = expr.replace(re, repl + '(');
    }

    // Replace clamp(x, lo, hi) with Math.min(Math.max(x, lo), hi)
    expr = expr.replace(/\bclamp\s*\(\s*([^,]+),\s*([^,]+),\s*([^)]+)\)/g,
        'Math.min(Math.max($1,$2),$3)');

    // Stat variable names
    const statNames = ['str','vit','int','dex','mag','cha','level'];

    // Block anything that looks like a function call that isn't Math.*
    const dangerousCall = /\b(?!Math\b)[a-zA-Z_]\w*\s*\(/;
    if (dangerousCall.test(expr)) {
        // Check if it's just a stat name followed by parenthesized expression (multiplication)
        // If not, reject
        const cleaned = expr.replace(/Math\.\w+\s*\(/g, '');
        if (/[a-zA-Z_]\w*\s*\(/.test(cleaned)) {
            throw new Error('Unknown function call in formula');
        }
    }

    // Build the function
    try {
        const fn = new Function(...statNames, `"use strict"; return (${expr});`);
        return fn;
    } catch (e) {
        throw new Error('Syntax error: ' + e.message);
    }
}

function evalFormula(fn, stats) {
    const result = fn(stats.str, stats.vit, stats.int, stats.dex, stats.mag, stats.cha, stats.level);
    if (typeof result !== 'number' || isNaN(result) || !isFinite(result)) return 1;
    return Math.max(1, Math.floor(result));
}

// --- Dice ---
function dice(sides) {
    if (sides <= 0) return 0;
    return Math.floor(Math.random() * sides) + 1;
}

function rollingFloor(baseFloor, floorRange) {
    if (baseFloor <= 0 && floorRange <= 0) return 0;
    return baseFloor + dice(floorRange + 1) - 1;
}

function nRoll(ceiling, n, baseFloor, floorRange) {
    if (ceiling <= 0) return 0;
    baseFloor = baseFloor || 0;
    floorRange = floorRange || 0;
    let lowRolls = [], highRolls = [], avgRolls = [];
    for (let i = 0; i < n; i++) {
        lowRolls.push(Math.max(rollingFloor(baseFloor, floorRange), dice(ceiling)));
        highRolls.push(Math.max(rollingFloor(baseFloor, floorRange), dice(ceiling)));
        avgRolls.push(Math.max(rollingFloor(baseFloor, floorRange), dice(ceiling)));
    }
    const low = Math.min(...lowRolls);
    const high = Math.max(...highRolls);
    const avg = Math.floor(avgRolls.reduce((a, b) => a + b, 0) / n);

    const pick = dice(3);
    if (pick === 1) return low;
    if (pick === 2) return high;
    return avg;
}

// --- State ---
const PALETTE = [
    'rgba(54, 162, 235, 1)',    // blue
    'rgba(255, 159, 64, 1)',    // orange
    'rgba(75, 192, 192, 1)',    // teal
    'rgba(153, 102, 255, 1)',   // purple
    'rgba(255, 205, 86, 1)',    // yellow
    'rgba(255, 99, 132, 1)',    // red
    'rgba(46, 204, 113, 1)',    // green
    'rgba(231, 76, 60, 1)',     // crimson
];

let keptResults = []; // { label, formula, color, rawData, rollData, cumRaw, cumRoll, stats }
let currentResult = null;
let perTickChart = null;
let cumulativeChart = null;
let colorIndex = 0;

function getNextColor() {
    const c = PALETTE[colorIndex % PALETTE.length];
    colorIndex++;
    return c;
}

function getStats() {
    return {
        str: parseInt(document.getElementById('stat-str').value) || 1,
        vit: parseInt(document.getElementById('stat-vit').value) || 1,
        int: parseInt(document.getElementById('stat-int').value) || 1,
        dex: parseInt(document.getElementById('stat-dex').value) || 1,
        mag: parseInt(document.getElementById('stat-mag').value) || 1,
        cha: parseInt(document.getElementById('stat-cha').value) || 1,
        level: parseInt(document.getElementById('stat-level').value) || 1,
    };
}

function calcStats(arr) {
    if (arr.length === 0) return { sum: 0, avg: 0, min: 0, max: 0, median: 0, spread: '0' };
    const sum = arr.reduce((a, b) => a + b, 0);
    const avg = Math.round(sum / arr.length);
    const mn = Math.min(...arr);
    const mx = Math.max(...arr);
    const sorted = [...arr].sort((a, b) => a - b);
    const median = sorted.length % 2 === 0
        ? Math.round((sorted[sorted.length/2 - 1] + sorted[sorted.length/2]) / 2)
        : sorted[Math.floor(sorted.length/2)];
    return { sum, avg, min: mn, max: mx, median, spread: mx === 0 ? '0' : (mx / Math.max(mn, 1)).toFixed(1) };
}

function simulate() {
    const formulaStr = document.getElementById('formula').value.trim();
    const errorEl = document.getElementById('formula-error');
    errorEl.textContent = '';

    let fn;
    try {
        fn = buildEvaluator(formulaStr);
    } catch (e) {
        errorEl.textContent = e.message;
        return;
    }

    const stats = getStats();
    let ceiling;
    try {
        ceiling = evalFormula(fn, stats);
    } catch (e) {
        errorEl.textContent = 'Evaluation error: ' + e.message;
        return;
    }

    document.getElementById('currentCeiling').textContent = ceiling;

    // Evaluate floor formula
    const floorStr = document.getElementById('formula-floor').value.trim();
    let floorVal = 0;
    const floorErrorEl = document.getElementById('formula-floor-error');
    floorErrorEl.textContent = '';
    if (floorStr && floorStr !== '0') {
        try {
            const floorFn = buildEvaluator(floorStr);
            floorVal = evalFormula(floorFn, stats);
        } catch (e) {
            floorErrorEl.textContent = e.message;
            return;
        }
    }
    document.getElementById('currentFloor').textContent = floorVal;

    // Evaluate floor roll range formula
    const floorRangeStr = document.getElementById('formula-floor-range').value.trim();
    let floorRangeVal = 0;
    const floorRangeErrorEl = document.getElementById('formula-floor-range-error');
    floorRangeErrorEl.textContent = '';
    if (floorRangeStr && floorRangeStr !== '0') {
        try {
            const floorRangeFn = buildEvaluator(floorRangeStr);
            floorRangeVal = evalFormula(floorRangeFn, stats);
        } catch (e) {
            floorRangeErrorEl.textContent = e.message;
            return;
        }
    }
    document.getElementById('currentFloorRange').textContent = floorRangeVal;

    const numTicks = parseInt(document.getElementById('num-ticks').value) || 50;
    const numRolls = parseInt(document.getElementById('num-rolls').value) || 3;

    const rawData = [], rollData = [], cumRaw = [], cumRoll = [];
    let cRaw = 0, cRoll = 0;

    for (let i = 0; i < numTicks; i++) {
        const r = Math.max(rollingFloor(floorVal, floorRangeVal), dice(ceiling));
        const t = nRoll(ceiling, numRolls, floorVal, floorRangeVal);
        rawData.push(r);
        rollData.push(t);
        cRaw += r; cRoll += t;
        cumRaw.push(cRaw);
        cumRoll.push(cRoll);
    }

    currentResult = {
        formula: formulaStr,
        floorFormula: floorStr,
        floorRangeFormula: floorRangeStr,
        ceiling,
        floor: floorVal,
        floorRange: floorRangeVal,
        numRolls,
        statValues: { ...stats },
        rawData, rollData, cumRaw, cumRoll,
        rawStats: calcStats(rawData),
        rollStats: calcStats(rollData),
    };

    renderAll();
}

function keepCurrent() {
    if (!currentResult) return;

    const color = getNextColor();
    keptResults.push({
        label: currentResult.formula,
        formula: currentResult.formula,
        floorFormula: currentResult.floorFormula,
        floorRangeFormula: currentResult.floorRangeFormula,
        ceiling: currentResult.ceiling,
        floor: currentResult.floor,
        floorRange: currentResult.floorRange,
        numRolls: currentResult.numRolls,
        statValues: { ...currentResult.statValues },
        color,
        rawData: [...currentResult.rawData],
        rollData: [...currentResult.rollData],
        cumRaw: [...currentResult.cumRaw],
        cumRoll: [...currentResult.cumRoll],
        rawStats: currentResult.rawStats,
        rollStats: currentResult.rollStats,
    });

    renderKeptList();
    renderAll();
}

function removeKept(index) {
    keptResults.splice(index, 1);
    renderKeptList();
    renderAll();
}

function clearKept() {
    keptResults = [];
    colorIndex = 0;
    renderKeptList();
    renderAll();
}

function renderKeptList() {
    const el = document.getElementById('keptList');
    el.innerHTML = '';
    keptResults.forEach((k, i) => {
        const item = document.createElement('div');
        item.className = 'kept-item';
        item.innerHTML = `
            <div class="kept-color" style="background:${k.color}"></div>
            <div class="kept-formula">${k.formula} (ceiling: ${k.ceiling}, floor: ${k.floor}–${k.floor + (k.floorRange || 0)}, ${k.numRolls}-roll)</div>
            <button class="secondary" onclick="removeKept(${i})">Remove</button>
        `;
        el.appendChild(item);
    });
}

function renderAll() {
    const showRaw = document.getElementById('show-raw').checked;
    const showNRoll = document.getElementById('show-nroll').checked;

    const numTicks = currentResult ? currentResult.rawData.length : 50;
    const labels = Array.from({length: numTicks}, (_, i) => i + 1);

    const perTickDatasets = [];
    const cumDatasets = [];

    // Current formula (thick white/bright lines)
    if (currentResult) {
        const nr = currentResult.numRolls;
        if (showNRoll) {
            perTickDatasets.push({
                label: `${nr}-Roll: ${currentResult.formula}`,
                data: currentResult.rollData,
                borderColor: 'rgba(255, 255, 255, 0.95)',
                borderWidth: 2.5, pointRadius: 0, tension: 0.3, fill: false,
            });
            cumDatasets.push({
                label: `${nr}-Roll: ${currentResult.formula}`,
                data: currentResult.cumRoll,
                borderColor: 'rgba(255, 255, 255, 0.95)',
                backgroundColor: 'rgba(255, 255, 255, 0.04)',
                borderWidth: 2.5, pointRadius: 0, tension: 0.3, fill: true,
            });
        }
        if (showRaw) {
            perTickDatasets.push({
                label: `Raw: ${currentResult.formula}`,
                data: currentResult.rawData,
                borderColor: 'rgba(255, 255, 255, 0.4)',
                borderWidth: 1.5, pointRadius: 0, tension: 0.3, fill: false,
                borderDash: [6, 4],
            });
            cumDatasets.push({
                label: `Raw: ${currentResult.formula}`,
                data: currentResult.cumRaw,
                borderColor: 'rgba(255, 255, 255, 0.4)',
                borderWidth: 1.5, pointRadius: 0, tension: 0.3, fill: false,
                borderDash: [6, 4],
            });
        }
    }

    // Kept formulas
    for (const k of keptResults) {
        const alpha = (a) => k.color.replace(/[\d.]+\)$/, a + ')');
        if (showNRoll) {
            perTickDatasets.push({
                label: `${k.numRolls}-Roll: ${k.formula}`,
                data: k.rollData.slice(0, numTicks),
                borderColor: k.color,
                borderWidth: 1.5, pointRadius: 0, tension: 0.3, fill: false,
            });
            cumDatasets.push({
                label: `${k.numRolls}-Roll: ${k.formula}`,
                data: k.cumRoll.slice(0, numTicks),
                borderColor: k.color,
                backgroundColor: alpha(0.05),
                borderWidth: 1.5, pointRadius: 0, tension: 0.3, fill: true,
            });
        }
        if (showRaw) {
            perTickDatasets.push({
                label: `Raw: ${k.formula}`,
                data: k.rawData.slice(0, numTicks),
                borderColor: alpha(0.4),
                borderWidth: 1, pointRadius: 0, tension: 0.3, fill: false,
                borderDash: [4, 4],
            });
            cumDatasets.push({
                label: `Raw: ${k.formula}`,
                data: k.cumRaw.slice(0, numTicks),
                borderColor: alpha(0.4),
                borderWidth: 1, pointRadius: 0, tension: 0.3, fill: false,
                borderDash: [4, 4],
            });
        }
    }

    const chartOpts = (title, yLabel) => ({
        responsive: true, maintainAspectRatio: false,
        plugins: {
            title: { display: true, text: title, color: '#e0e0e0', font: { size: 16 } },
            legend: {
                labels: { color: '#aaa', usePointStyle: true, font: { size: 11 } },
                position: 'bottom',
            }
        },
        scales: {
            x: { title: { display: true, text: 'Tick', color: '#888' }, ticks: { color: '#666' }, grid: { color: '#2a2a4a' } },
            y: { title: { display: true, text: yLabel, color: '#888' }, ticks: { color: '#666' }, grid: { color: '#2a2a4a' }, beginAtZero: true }
        }
    });

    if (perTickChart) perTickChart.destroy();
    perTickChart = new Chart(document.getElementById('perTickChart'), {
        type: 'line',
        data: { labels, datasets: perTickDatasets },
        options: chartOpts('Per-Tick Regen', 'HP Regenerated')
    });

    if (cumulativeChart) cumulativeChart.destroy();
    cumulativeChart = new Chart(document.getElementById('cumulativeChart'), {
        type: 'line',
        data: { labels, datasets: cumDatasets },
        options: chartOpts('Cumulative Regen Over Time', 'Total HP Regenerated')
    });

    // Stats cards
    const grid = document.getElementById('statsGrid');
    grid.innerHTML = '';

    function addCard(info, rawStats, rollStats, borderColor, isCurrent) {
        const card = document.createElement('div');
        card.className = isCurrent ? 'stat-card current' : 'stat-card';
        card.style.borderTop = `2px solid ${borderColor}`;

        let html = '';
        const sv = info.statValues || {};

        // Character stats
        html += `<div style="font-size:11px;color:#aaa;margin-bottom:6px;font-family:Consolas,monospace;">`;
        html += `Lv<span style="color:#e94560">${sv.level||'?'}</span>`;
        html += ` S<span style="color:#e94560">${sv.str||'?'}</span>`;
        html += ` V<span style="color:#e94560">${sv.vit||'?'}</span>`;
        html += ` I<span style="color:#e94560">${sv.int||'?'}</span>`;
        html += ` D<span style="color:#e94560">${sv.dex||'?'}</span>`;
        html += ` M<span style="color:#e94560">${sv.mag||'?'}</span>`;
        html += ` C<span style="color:#e94560">${sv.cha||'?'}</span>`;
        html += `</div>`;

        // Ceiling formula
        html += `<div style="font-size:10px;color:#666;margin-bottom:2px;font-family:Consolas,monospace;">`;
        html += `Ceiling: <span style="color:#aaa">${info.formula}</span> = <span style="color:#e94560">${info.ceiling}</span>`;
        html += `</div>`;

        // Floor formula
        html += `<div style="font-size:10px;color:#666;margin-bottom:2px;font-family:Consolas,monospace;">`;
        html += `Floor: <span style="color:#aaa">${info.floorFormula || '0'}</span> = <span style="color:#e94560">${info.floor}</span>`;
        html += `</div>`;

        // Floor range
        html += `<div style="font-size:10px;color:#666;margin-bottom:4px;font-family:Consolas,monospace;">`;
        html += `Floor range: +<span style="color:#aaa">${info.floorRangeFormula || '0'}</span> = <span style="color:#e94560">${info.floor}–${info.floor + (info.floorRange || 0)}</span>`;
        html += `</div>`;

        // Rolls
        html += `<div style="font-size:10px;color:#666;margin-bottom:6px;font-family:Consolas,monospace;">`;
        html += `Rolls: <span style="color:#e94560">${info.numRolls}</span>`;
        html += `</div>`;

        if (showNRoll) {
            html += `<div style="font-size:10px;color:#666;margin:6px 0 2px;text-transform:uppercase">${info.numRolls}-Roll</div>`;
            html += statRows(rollStats);
        }
        if (showRaw) {
            html += `<div style="font-size:10px;color:#666;margin:6px 0 2px;text-transform:uppercase">Raw</div>`;
            html += statRows(rawStats);
        }

        card.innerHTML = html;
        grid.appendChild(card);
    }

    function statRows(s) {
        return `
            <div class="stat-row"><span class="stat-label">Avg/tick</span><span class="stat-value">${s.avg}</span></div>
            <div class="stat-row"><span class="stat-label">Median</span><span class="stat-value">${s.median}</span></div>
            <div class="stat-row"><span class="stat-label">Low</span><span class="stat-value">${s.min}</span></div>
            <div class="stat-row"><span class="stat-label">High</span><span class="stat-value">${s.max}</span></div>
            <div class="stat-row"><span class="stat-label">Spread</span><span class="stat-value">${s.spread}x</span></div>
            <div class="stat-row"><span class="stat-label">Total</span><span class="stat-value">${s.sum.toLocaleString()}</span></div>
        `;
    }

    for (const k of keptResults) {
        addCard(k, k.rawStats, k.rollStats, k.color, false);
    }
    if (currentResult) {
        addCard(currentResult, currentResult.rawStats, currentResult.rollStats, 'rgba(255,255,255,0.6)', true);
    }
}

// Toggle without re-rolling
document.getElementById('show-raw').addEventListener('change', renderAll);
document.getElementById('show-nroll').addEventListener('change', renderAll);

// Enter key in formula box triggers roll
document.getElementById('formula').addEventListener('keydown', (e) => {
    if (e.key === 'Enter') simulate();
});

// No initial run — start empty, user clicks Roll

function exportData() {
    const lines = [];
    lines.push('# Regen Simulation Data\n');

    function writeEntry(info, rawStats, rollStats, label) {
        const sv = info.statValues || {};
        lines.push(`## ${label}\n`);
        lines.push(`| Stat | Value |`);
        lines.push(`|------|-------|`);
        lines.push(`| Level | ${sv.level || '?'} |`);
        lines.push(`| STR | ${sv.str || '?'} |`);
        lines.push(`| VIT | ${sv.vit || '?'} |`);
        lines.push(`| INT | ${sv.int || '?'} |`);
        lines.push(`| DEX | ${sv.dex || '?'} |`);
        lines.push(`| MAG | ${sv.mag || '?'} |`);
        lines.push(`| CHA | ${sv.cha || '?'} |`);
        lines.push('');
        lines.push(`- **Ceiling formula:** \`${info.formula}\` = ${info.ceiling}`);
        lines.push(`- **Floor formula:** \`${info.floorFormula || '0'}\` = ${info.floor}`);
        lines.push(`- **Floor roll range:** \`${info.floorRangeFormula || '0'}\` = ${info.floorRange || 0} (effective floor: ${info.floor}–${info.floor + (info.floorRange || 0)})`);
        lines.push(`- **Rolls:** ${info.numRolls}`);
        lines.push('');

        lines.push(`### ${info.numRolls}-Roll Stats\n`);
        lines.push(`| Metric | Value |`);
        lines.push(`|--------|-------|`);
        lines.push(`| Avg/tick | ${rollStats.avg} |`);
        lines.push(`| Median | ${rollStats.median} |`);
        lines.push(`| Low | ${rollStats.min} |`);
        lines.push(`| High | ${rollStats.max} |`);
        lines.push(`| Spread | ${rollStats.spread}x |`);
        lines.push(`| Total | ${rollStats.sum.toLocaleString()} |`);
        lines.push('');

        lines.push(`### Raw Stats\n`);
        lines.push(`| Metric | Value |`);
        lines.push(`|--------|-------|`);
        lines.push(`| Avg/tick | ${rawStats.avg} |`);
        lines.push(`| Median | ${rawStats.median} |`);
        lines.push(`| Low | ${rawStats.min} |`);
        lines.push(`| High | ${rawStats.max} |`);
        lines.push(`| Spread | ${rawStats.spread}x |`);
        lines.push(`| Total | ${rawStats.sum.toLocaleString()} |`);
        lines.push('');

        // Per-tick data table
        lines.push(`### Per-Tick Data\n`);
        lines.push(`| Tick | ${info.numRolls}-Roll | Raw |`);
        lines.push(`|------|--------|-----|`);
        const len = info.rawData ? info.rawData.length : 0;
        for (let i = 0; i < len; i++) {
            lines.push(`| ${i + 1} | ${info.rollData[i]} | ${info.rawData[i]} |`);
        }
        lines.push('');
    }

    // Kept formulas first (in order), current last
    keptResults.forEach((k, i) => {
        writeEntry(k, k.rawStats, k.rollStats, `Kept #${i + 1}`);
    });
    if (currentResult) {
        writeEntry(currentResult, currentResult.rawStats, currentResult.rollStats, 'Current');
    }

    const md = lines.join('\n');

    fetch('/export', {
        method: 'POST',
        headers: { 'Content-Type': 'text/plain' },
        body: md,
    }).then(r => {
        if (r.ok) {
            const el = document.getElementById('export-status');
            el.style.display = 'inline';
            setTimeout(() => el.style.display = 'none', 3000);
        }
    });
}
</script>
</body>
</html>
"""

class Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        self.send_response(200)
        self.send_header('Content-Type', 'text/html')
        self.end_headers()
        self.wfile.write(HTML.encode())

    def do_POST(self):
        if self.path == '/export':
            length = int(self.headers.get('Content-Length', 0))
            body = self.rfile.read(length).decode('utf-8')
            import os
            out_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'regen_data.md')
            with open(out_path, 'w', encoding='utf-8') as f:
                f.write(body)
            print(f"Exported to {out_path}")
            self.send_response(200)
            self.end_headers()
        else:
            self.send_response(404)
            self.end_headers()

    def log_message(self, format, *args):
        pass

def main():
    server = http.server.HTTPServer(('127.0.0.1', PORT), Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    print(f"Regen Simulator running at http://127.0.0.1:{PORT}")
    print("Press Ctrl+C to stop")
    webbrowser.open(f'http://127.0.0.1:{PORT}')
    try:
        thread.join()
    except KeyboardInterrupt:
        print("\nStopped.")
        server.shutdown()

if __name__ == '__main__':
    main()
