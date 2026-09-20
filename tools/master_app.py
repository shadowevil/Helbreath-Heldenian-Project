"""
Master Tool App — Unified Navigation Shell for Helbreath Tools

Launches all tool servers as background threads and serves a navigation
shell with hamburger side menu on port 8000.

Usage: python tools/master_app.py
Then open http://localhost:8000 in your browser.
"""

import sys
import os
import signal
import threading
import time
from http.server import HTTPServer, BaseHTTPRequestHandler

# ---------------------------------------------------------------------------
# Tool registry — add new tools here
# ---------------------------------------------------------------------------
TOOL_REGISTRY = [
    {
        "name": "Item Rename",
        "category": "Database",
        "port": 8080,
        "module": "ItemRenameTool.app",
        "icon": "rename",
    },
    {
        "name": "Shop Manager",
        "category": "Database",
        "port": 8081,
        "module": "ShopManager.app",
        "icon": "shop",
    },
    {
        "name": "Drop Tables",
        "category": "Database",
        "port": 8888,
        "module": "Drops.drop_manager_server",
        "icon": "drops",
    },
    {
        "name": "Exp Curve",
        "category": "Database",
        "port": 8082,
        "module": "ExpCurve.app",
        "icon": "expcurve",
    },
    {
        "name": "NPC Editor",
        "category": "Database",
        "port": 8083,
        "module": "NpcEditor.app",
        "icon": "npc",
    },
    {
        "name": "Creation Items",
        "category": "Database",
        "port": 8084,
        "module": "CreationItemManager.app",
        "icon": "creation",
    },
]

MASTER_PORT = 8000

# ---------------------------------------------------------------------------
# Launch each tool's main() in a daemon thread
# ---------------------------------------------------------------------------

def _launch_tool(tool):
    """Import a tool module and call its main() (blocking — run in a thread)."""
    try:
        mod = __import__(tool["module"], fromlist=["main"])
        print(f"  Starting {tool['name']} on port {tool['port']}...")
        mod.main()
    except Exception as exc:
        print(f"  [ERROR] {tool['name']} failed: {exc}")


def start_tools():
    """Start all registered tools as daemon threads."""
    # Ensure the tools directory is on sys.path so sub-packages resolve
    tools_dir = os.path.dirname(os.path.abspath(__file__))
    if tools_dir not in sys.path:
        sys.path.insert(0, tools_dir)

    for tool in TOOL_REGISTRY:
        t = threading.Thread(target=_launch_tool, args=(tool,), daemon=True)
        t.start()

    # Brief pause so tool servers can bind before we report ready
    time.sleep(0.5)

# ---------------------------------------------------------------------------
# Build the navigation shell HTML
# ---------------------------------------------------------------------------

def _build_nav_items():
    """Group tools by category and return HTML list items."""
    cats = {}
    for tool in TOOL_REGISTRY:
        cats.setdefault(tool["category"], []).append(tool)

    parts = []
    for cat, tools in cats.items():
        parts.append(f'<div class="nav-category">{cat.upper()} TOOLS</div>')
        for t in tools:
            parts.append(
                f'<a class="nav-item" href="#" data-port="{t["port"]}" '
                f'data-name="{t["name"]}">{t["name"]}</a>'
            )
    return "\n                ".join(parts)


SHELL_HTML = """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Helbreath Tools</title>
<style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    html, body { height: 100%; overflow: hidden; }
    body {
        font-family: 'Segoe UI', Tahoma, sans-serif;
        background: #181620;
        color: #ddd8d0;
    }

    /* ---------- Top bar ---------- */
    .topbar {
        height: 48px;
        background: #22202c;
        border-bottom: 2px solid #3a3248;
        display: flex;
        align-items: center;
        padding: 0 16px;
        gap: 14px;
        z-index: 200;
        position: relative;
    }
    .hamburger {
        background: none;
        border: none;
        color: #ddd8d0;
        font-size: 22px;
        cursor: pointer;
        padding: 4px 6px;
        border-radius: 4px;
        line-height: 1;
    }
    .hamburger:hover { background: #3a3248; }
    .topbar-title {
        font-size: 16px;
        font-weight: 600;
        color: #c8a850;
        user-select: none;
    }
    .topbar-tool {
        font-size: 14px;
        color: #887868;
        margin-left: auto;
    }

    /* ---------- Sidebar ---------- */
    .sidebar {
        position: fixed;
        top: 48px;
        left: 0;
        bottom: 0;
        width: 260px;
        background: #22202c;
        border-right: 1px solid #3a3248;
        z-index: 150;
        display: flex;
        flex-direction: column;
        overflow-y: auto;
        padding: 12px 0;
    }

    .nav-category {
        padding: 16px 20px 6px;
        font-size: 11px;
        font-weight: 700;
        color: #686058;
        letter-spacing: 0.5px;
    }
    .nav-item {
        display: block;
        padding: 10px 20px 10px 24px;
        color: #ccc0b4;
        text-decoration: none;
        font-size: 14px;
        border-left: 3px solid transparent;
        transition: background 0.15s, border-color 0.15s;
    }
    .nav-item:hover {
        background: #181620;
        color: #fff;
    }
    .nav-item.active {
        border-left-color: #c8a850;
        background: #181620;
        color: #c8a850;
        font-weight: 600;
    }

    /* ---------- Overlay (mobile) ---------- */
    .overlay {
        display: none;
        position: fixed;
        top: 48px;
        left: 0;
        right: 0;
        bottom: 0;
        background: rgba(0,0,0,0.4);
        z-index: 140;
    }
    .overlay.visible { display: block; }

    /* ---------- Content area ---------- */
    .content {
        position: absolute;
        top: 48px;
        right: 0;
        bottom: 0;
    }

    .content iframe {
        width: 100%;
        height: 100%;
        border: none;
    }

    .welcome {
        display: flex;
        flex-direction: column;
        align-items: center;
        justify-content: center;
        height: 100%;
        color: #585048;
        gap: 12px;
    }
    .welcome h2 { color: #c8a850; font-size: 22px; }
    .welcome p { font-size: 15px; }

    /* ---------- Desktop: sidebar always visible ---------- */
    @media (min-width: 800px) {
        .sidebar { transform: translateX(0); }
        .content { left: 260px; }
        .hamburger { display: none; }
    }

    /* ---------- Narrow: sidebar slides in/out ---------- */
    @media (max-width: 799px) {
        .sidebar {
            transform: translateX(-260px);
            transition: transform 0.3s ease;
        }
        .sidebar.open { transform: translateX(0); }
        .content {
            left: 0;
            transition: left 0.3s ease;
        }
    }
</style>
</head>
<body>

<div class="topbar">
    <button class="hamburger" id="btnHamburger" title="Toggle menu">&#9776;</button>
    <span class="topbar-title">Helbreath Tools</span>
    <span class="topbar-tool" id="activeToolLabel"></span>
</div>

<div class="overlay" id="overlay"></div>

<nav class="sidebar" id="sidebar">
    """ + _build_nav_items() + """
</nav>

<div class="content" id="content">
    <div class="welcome" id="welcomeScreen">
        <h2>Helbreath Tools</h2>
        <p>Select a tool to get started.</p>
    </div>
    <iframe id="toolFrame" style="display:none" sandbox="allow-same-origin allow-scripts allow-forms allow-popups"></iframe>
</div>

<script>
const sidebar   = document.getElementById('sidebar');
const overlay   = document.getElementById('overlay');
const content   = document.getElementById('content');
const frame     = document.getElementById('toolFrame');
const welcome   = document.getElementById('welcomeScreen');
const toolLabel = document.getElementById('activeToolLabel');
const burger    = document.getElementById('btnHamburger');

let sidebarOpen = false;

function toggleSidebar() {
    sidebarOpen = !sidebarOpen;
    sidebar.classList.toggle('open', sidebarOpen);
    overlay.classList.toggle('visible', sidebarOpen);
    content.classList.toggle('shifted', sidebarOpen);
}

burger.addEventListener('click', toggleSidebar);
overlay.addEventListener('click', toggleSidebar);

// Nav item clicks
document.querySelectorAll('.nav-item').forEach(link => {
    link.addEventListener('click', (e) => {
        e.preventDefault();
        const port = link.dataset.port;
        const name = link.dataset.name;

        // Highlight active
        document.querySelectorAll('.nav-item').forEach(l => l.classList.remove('active'));
        link.classList.add('active');

        // Load tool in iframe
        welcome.style.display = 'none';
        frame.style.display = 'block';
        frame.src = 'http://localhost:' + port + '?_t=' + Date.now();
        toolLabel.textContent = name;

        // Auto-close sidebar on mobile
        if (window.innerWidth < 800 && sidebarOpen) {
            toggleSidebar();
        }
    });
});
</script>
</body>
</html>"""

# ---------------------------------------------------------------------------
# Master app HTTP handler
# ---------------------------------------------------------------------------

class MasterHandler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        print(f"[master] {fmt % args}")

    def do_GET(self):
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.end_headers()
        self.wfile.write(SHELL_HTML.encode())


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main():
    print("=" * 50)
    print("  Helbreath Master Tool App")
    print("=" * 50)

    print("\nLaunching tool servers...")
    start_tools()

    print(f"\nMaster shell on http://localhost:{MASTER_PORT}")
    print("Press Ctrl+C to stop all.\n")

    HTTPServer.allow_reuse_address = True
    server = HTTPServer(("127.0.0.1", MASTER_PORT), MasterHandler)

    def _shutdown(sig, frame):
        print("\nShutting down master app.")
        threading.Thread(target=server.shutdown, daemon=True).start()

    signal.signal(signal.SIGINT, _shutdown)
    signal.signal(signal.SIGTERM, _shutdown)
    server.serve_forever()


if __name__ == "__main__":
    main()
