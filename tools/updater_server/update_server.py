#!/usr/bin/env python3
"""Simple HTTP file server for the Helbreath auto-updater.

Serves a directory over HTTP with CORS headers.
Settings are saved to update_server.json — delete it to reconfigure.
"""

import json
import os
import sys
import threading
import time
from collections import defaultdict
from functools import partial
from http.server import SimpleHTTPRequestHandler
from socketserver import ThreadingMixIn, TCPServer


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
CONFIG_PATH = os.path.join(SCRIPT_DIR, "update_server.json")

DEFAULTS = {
    "directory": "Binaries/Game",
    "host": "0.0.0.0",
    "port": 8080,
    "rate_limit_mbps": 10,
    "max_requests_per_minute": 300,
    "connection_timeout": 30,
}


class ConnectionTracker:
    """Per-IP request rate limiting.

    Tracks timestamps of requests per IP. Once an IP exceeds
    max_per_minute within a rolling 60-second window, further
    requests are silently dropped.
    """

    def __init__(self, max_per_minute):
        self.max_per_minute = max_per_minute
        self._hits = defaultdict(list)
        self._lock = threading.Lock()
        self._last_cleanup = time.monotonic()

    def allow(self, ip):
        if self.max_per_minute <= 0:
            return True
        now = time.monotonic()
        with self._lock:
            # Purge stale IPs every 60 seconds to prevent memory growth
            if now - self._last_cleanup > 60:
                self._cleanup(now)
                self._last_cleanup = now
            timestamps = self._hits[ip]
            cutoff = now - 60
            timestamps[:] = [t for t in timestamps if t > cutoff]
            if len(timestamps) >= self.max_per_minute:
                return False
            timestamps.append(now)
            return True

    def _cleanup(self, now):
        cutoff = now - 60
        stale = [ip for ip, ts in self._hits.items()
                 if not ts or ts[-1] <= cutoff]
        for ip in stale:
            del self._hits[ip]


class ThreadingHTTPServer(ThreadingMixIn, TCPServer):
    """Handle each connection in a new daemon thread."""

    daemon_threads = True
    allow_reuse_address = True

    def server_bind(self):
        self.server_name = self.server_address[0]
        self.server_port = self.server_address[1]
        self.socket.bind(self.server_address)


class UpdateRequestHandler(SimpleHTTPRequestHandler):
    """HTTP handler with CORS support, rate limiting, and cleaner logging."""

    rate_limit_bps = 0
    connection_timeout = 30

    def setup(self):
        super().setup()
        self.request.settimeout(self.connection_timeout)

    def handle_one_request(self):
        """Override to silently reject non-updater traffic."""
        try:
            # Per-IP rate limiting
            ip = self.client_address[0]
            tracker = getattr(self.server, 'connection_tracker', None)
            if tracker is not None and not tracker.allow(ip):
                self.close_connection = True
                return

            self.raw_requestline = self.rfile.readline(65537)
            if len(self.raw_requestline) > 65536:
                self.close_connection = True
                return
            if not self.raw_requestline:
                self.close_connection = True
                return
            # TLS ClientHello: 0x16 (handshake) followed by 0x03 (TLS version)
            if (len(self.raw_requestline) >= 2
                    and self.raw_requestline[0] == 0x16
                    and self.raw_requestline[1] == 0x03):
                self.close_connection = True
                return
            if not self.parse_request():
                return
            # Only allow GET and HEAD — silently drop everything else
            if self.command not in ('GET', 'HEAD'):
                self.close_connection = True
                return
            mname = 'do_' + self.command
            method = getattr(self, mname)
            method()
            self.wfile.flush()
        except (TimeoutError, ConnectionError, BrokenPipeError):
            self.close_connection = True

    def copyfile(self, source, outputfile):
        """Rate-limited file copy."""
        if self.rate_limit_bps <= 0:
            return super().copyfile(source, outputfile)

        chunk_size = max(4096, self.rate_limit_bps // 10)  # ~100ms chunks
        interval = chunk_size / self.rate_limit_bps

        while True:
            start = time.monotonic()
            buf = source.read(chunk_size)
            if not buf:
                break
            outputfile.write(buf)
            elapsed = time.monotonic() - start
            sleep_time = interval - elapsed
            if sleep_time > 0:
                time.sleep(sleep_time)

    def send_error(self, code, message=None, explain=None):
        """Silently close instead of sending HTTP error responses."""
        self.close_connection = True

    def do_GET(self):
        """Only serve files that exist. Silently drop everything else."""
        path = self.translate_path(self.path)
        if not os.path.isfile(path):
            self.close_connection = True
            return
        super().do_GET()

    def do_HEAD(self):
        """Only serve files that exist. Silently drop everything else."""
        path = self.translate_path(self.path)
        if not os.path.isfile(path):
            self.close_connection = True
            return
        super().do_HEAD()

    def end_headers(self):
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, HEAD")
        self.send_header("Access-Control-Allow-Headers", "Range")
        super().end_headers()

    def log_message(self, format, *args):
        if not args:
            return
        msg = format % args
        print(f"[{self.log_date_time_string()}] {msg}")


def resolve_path(path: str) -> str:
    """Resolve a path relative to the script's directory, not CWD."""
    path = path.strip("\"'")
    if os.path.isabs(path):
        return os.path.normpath(path)
    return os.path.normpath(os.path.join(SCRIPT_DIR, path))


def prompt(text: str, default: str = "") -> str:
    if default:
        result = input(f"{text} [{default}]: ").strip()
        return result if result else default
    return input(f"{text}: ").strip()


def load_config() -> dict | None:
    if not os.path.isfile(CONFIG_PATH):
        return None
    try:
        with open(CONFIG_PATH) as f:
            return json.load(f)
    except (json.JSONDecodeError, OSError):
        return None


def save_config(cfg: dict):
    with open(CONFIG_PATH, "w", newline="\n") as f:
        json.dump(cfg, f, indent=4)
    print(f"  Config saved to {CONFIG_PATH}\n")


def run_setup() -> dict:
    """Interactive first-time setup. Returns config dict."""
    print("  No config found — running first-time setup.")
    print(f"  (Delete {CONFIG_PATH} to reconfigure)\n")

    # Directory
    directory = prompt("Directory to serve", DEFAULTS["directory"])
    while True:
        resolved = resolve_path(directory)
        if not os.path.isdir(resolved):
            print(f"  '{resolved}' is not a valid directory.")
            directory = prompt("Directory to serve")
            continue

        print(f"  Resolved: {resolved}")
        ok = prompt("  Correct? (y/n)", "y")
        if ok.lower() in ("y", "yes"):
            directory = resolved
            break
        directory = prompt("Directory to serve")

    # Rate limit
    rate_str = prompt("Rate limit MB/s (0 = unlimited)", str(DEFAULTS["rate_limit_mbps"]))
    while True:
        try:
            rate_mbps = float(rate_str)
            if rate_mbps >= 0:
                break
        except ValueError:
            pass
        print("  Invalid number.")
        rate_str = prompt("Rate limit MB/s (0 = unlimited)", str(DEFAULTS["rate_limit_mbps"]))

    # Host / Port
    host = prompt("Host", DEFAULTS["host"])
    port_str = prompt("Port", str(DEFAULTS["port"]))
    while not port_str.isdigit() or not (1 <= int(port_str) <= 65535):
        print("  Invalid port number.")
        port_str = prompt("Port", str(DEFAULTS["port"]))

    cfg = {
        "directory": directory,
        "host": host,
        "port": int(port_str),
        "rate_limit_mbps": rate_mbps,
    }
    save_config(cfg)
    return cfg


def main():
    print("=== Helbreath Update Server ===")
    print(f"  Working from: {SCRIPT_DIR}\n")

    cfg = load_config()
    if cfg is None:
        cfg = run_setup()
    else:
        print(f"  Loaded config from {CONFIG_PATH}")

    directory = cfg.get("directory", DEFAULTS["directory"])
    host = cfg.get("host", DEFAULTS["host"])
    port = cfg.get("port", DEFAULTS["port"])
    rate_mbps = cfg.get("rate_limit_mbps", DEFAULTS["rate_limit_mbps"])
    max_rpm = cfg.get("max_requests_per_minute", DEFAULTS["max_requests_per_minute"])
    conn_timeout = cfg.get("connection_timeout", DEFAULTS["connection_timeout"])

    # Resolve directory
    directory = resolve_path(directory)
    if not os.path.isdir(directory):
        print(f"  Error: '{directory}' is not a valid directory.")
        print(f"  Delete {CONFIG_PATH} to reconfigure.")
        return 1

    # Check for manifest
    manifest_path = os.path.join(directory, "update.manifest.json")
    if not os.path.isfile(manifest_path):
        print(f"\n  Warning: No update.manifest.json found in {directory}")
        print("  Run gen_update_manifest.py first to generate one.")

    # Apply settings to handler class
    if rate_mbps > 0:
        UpdateRequestHandler.rate_limit_bps = int(rate_mbps * 1024 * 1024)
    else:
        UpdateRequestHandler.rate_limit_bps = 0
    UpdateRequestHandler.connection_timeout = conn_timeout

    # Start
    abs_dir = os.path.abspath(directory)
    rate_display = f"{rate_mbps} MB/s" if rate_mbps > 0 else "unlimited"
    rpm_display = f"{max_rpm}/min per IP" if max_rpm > 0 else "unlimited"
    print(f"\n  Directory:  {abs_dir}")
    print(f"  Rate limit: {rate_display}")
    print(f"  Requests:   {rpm_display}")
    print(f"  Timeout:    {conn_timeout}s")
    print(f"  Listening:  http://{host}:{port}")
    print("  Press Ctrl+C to stop\n")

    handler = partial(UpdateRequestHandler, directory=directory)
    server = ThreadingHTTPServer((host, port), handler)
    server.connection_tracker = ConnectionTracker(max_rpm)

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down")
        server.shutdown()

    return 0


if __name__ == "__main__":
    sys.exit(main())
