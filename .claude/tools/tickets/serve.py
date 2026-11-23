"""Ticket viewer service: serves the read-only ticket site from the repo.

    python <CLI_DIR>/serve.py               ensure it is running (idempotent), bound to 127.0.0.1 and ::1, print the URLs
    python <CLI_DIR>/serve.py --all-interfaces  same, bound to 0.0.0.0 and ::: the LAN (a phone) can reach it;
                                            Windows may show a firewall prompt
    (--local-only, the opt-out before 1.4.0, is still accepted and changes nothing: localhost is the default)
    python <CLI_DIR>/serve.py --stop        stop the recorded server (its PID only)
    python <CLI_DIR>/serve.py --foreground  run in this console (debugging)

<CLI_DIR> is this file's folder (src/tools/tickets or tools/tickets). The port, the /api/health
marker, the names on the page and the other per-project values come from .claude/tickets/config.json
(viewer_port, viewer_service, store_name, ...; see tickets.load_config); --port overrides viewer_port.

A plain start answers at once when our server already holds the port; otherwise it starts itself
DETACHED (no console window, survives the calling shell). The server records its PID in
build/tickets/serve.pid once it holds the port. VIEW ONLY: GET on a fixed route table, nothing else;
tickets are read fresh from .claude/tickets on every /api/tickets request, and /api/events pushes a
version to open pages whenever a ticket file changes. Rules: .claude/docs/tickets.md.

Two listeners (B67, 1.5.3): the IPv4 one above, whose bind is the one recorded and reported, and an IPv6 one beside
it in its own thread (::1, or :: with --all-interfaces), because many browsers resolve localhost to ::1 first. Where
IPv6 or ::1 is unavailable it is skipped with a log line. The health check, --stop and printed URLs use 127.0.0.1.

The one exception to GET only is the one-click update (B65, updater.py): GET /api/update and /api/update/status
report it, and POST /api/update/apply starts it, only from this PC with the right Host, Origin and the page's token
(apply_update). On start, an update whose helper died part-way is undone first (recover_interrupted_update).
"""

import argparse
import hashlib
import hmac
import html
import http.server
import ipaddress
import json
import os
import re
import secrets
import signal
import socket
import subprocess
import sys
import threading
import time
import urllib.request
from datetime import datetime, timezone

viewer_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, viewer_dir)


def stop_requested(argv):
	"""True when argv asks for --stop, read by argparse's own rules (no abbreviations, as in main)."""
	class quiet_parser(argparse.ArgumentParser):
		def error(self, message):
			raise ValueError(message)
	probe = quiet_parser(add_help=False, allow_abbrev=False)
	probe.add_argument("--stop", action="store_true")
	try:
		return probe.parse_known_args(argv)[0].stop
	except ValueError:
		return False


def stop_repo_root():
	"""The repository --stop works on without tickets.py: the nearest folder above this one that holds
	.claude/tickets/config.json (as tickets.py finds it), else the nearest that holds build/tickets/serve.pid, so a
	config.json that has gone missing does not leave a running viewer unreachable. None when neither exists."""
	for marker in (os.path.join(".claude", "tickets", "config.json"), os.path.join("build", "tickets", "serve.pid")):
		folder = viewer_dir
		while True:
			if os.path.isfile(os.path.join(folder, marker)):
				return folder
			parent = os.path.dirname(folder)
			if parent == folder:
				break
			folder = parent
	return None


def stop_config(root):
	"""config.json read leniently ({} when missing, unreadable or not an object); --stop needs only the health marker."""
	try:
		with open(os.path.join(root, ".claude", "tickets", "config.json"), "r", encoding="utf-8-sig") as handle:
			value = json.load(handle)
	except (OSError, ValueError, RecursionError):
		return {}
	return value if isinstance(value, dict) else {}


def stop_marker(settings):
	"""viewer_service as tickets.load_config would default it, or None when config.json gives no usable one."""
	marker = settings.get("viewer_service")
	if isinstance(marker, str) and marker.strip():
		return marker
	name = settings.get("store_name")
	if "viewer_service" not in settings and isinstance(name, str) and name.strip():
		slug = re.sub(r"[^a-z0-9]+", "-", name.lower()).strip("-")
		return slug + "-tickets" if slug else "tickets"
	return None


def is_integer(value):
	return isinstance(value, int) and not isinstance(value, bool)


def process_created(pid):
	"""The process's creation time in Windows FILETIME ticks, or None when it is gone, unreadable or not on Windows."""
	if os.name != "nt" or not is_integer(pid) or not 0 < pid <= 0xFFFFFFFF:
		return None
	import ctypes
	from ctypes import wintypes
	kernel32 = ctypes.windll.kernel32
	kernel32.OpenProcess.restype = wintypes.HANDLE
	kernel32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
	kernel32.GetProcessTimes.argtypes = [wintypes.HANDLE] + [ctypes.POINTER(wintypes.FILETIME)] * 4
	kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
	# PROCESS_QUERY_LIMITED_INFORMATION
	handle = kernel32.OpenProcess(0x1000, False, pid)
	if not handle:
		return None
	try:
		times = [wintypes.FILETIME() for _ in range(4)]
		if not kernel32.GetProcessTimes(handle, *[ctypes.byref(value) for value in times]):
			return None
		return (times[0].dwHighDateTime << 32) | times[0].dwLowDateTime
	finally:
		kernel32.CloseHandle(handle)


def process_running(pid, created):
	"""True while pid runs and, where its creation time was recorded (Windows), is still that process."""
	if not is_integer(pid) or pid <= 0:
		return False
	if os.name != "nt":
		try:
			os.kill(pid, 0)
		except OSError:
			return False
		return True
	current = process_created(pid)
	if current is None or (is_integer(created) and current != created):
		return False
	import ctypes
	from ctypes import wintypes
	kernel32 = ctypes.windll.kernel32
	handle = kernel32.OpenProcess(0x1000, False, pid)
	if not handle:
		return False
	try:
		code = wintypes.DWORD()
		# STILL_ACTIVE (259) while it runs; a process that has exited but whose handle is held still has its times.
		return not kernel32.GetExitCodeProcess(handle, ctypes.byref(code)) or code.value == 259
	finally:
		kernel32.CloseHandle(handle)


def path_inside(path, folder):
	"""True when the absolute path lies inside folder (case-insensitively on Windows); no symlink is followed."""
	if not isinstance(path, str) or not os.path.isabs(path):
		return False
	path = os.path.normcase(os.path.normpath(path))
	folder = os.path.normcase(os.path.normpath(folder))
	return path.startswith(folder.rstrip("\\/") + os.sep)


def lenient_json(path):
	try:
		with open(path, "r", encoding="utf-8") as handle:
			return json.load(handle)
	except (OSError, ValueError, RecursionError):
		return None


def recover_interrupted_update(root):
	"""Undo a one-click update (B65) whose helper died part-way, before anything is imported from the tool folder,
	which may be half swapped: build/tickets/update.lock names a process that is gone, and update/status.json holds no
	final state. Every file plan.json lists as backed up is put back, and every file it added is removed; targets
	outside this folder (and the project schema) and backups outside the update's work area are refused. Self-contained
	(it uses no updater.py, which may be one of the swapped files). True when it restored something."""
	build = os.path.join(root, "build", "tickets")
	lock_path = os.path.join(build, "update.lock")
	if not os.path.isfile(lock_path):
		return False
	lock = lenient_json(lock_path)
	if not isinstance(lock, dict):
		try:
			# Being written by the apply that has just taken it.
			if time.time() - os.path.getmtime(lock_path) < 30:
				return False
		except OSError:
			return False
		lock = {}
	if process_running(lock.get("pid"), lock.get("created")):
		return False
	update_dir = os.path.join(build, "update")
	status_path = os.path.join(update_dir, "status.json")
	status = lenient_json(status_path)
	status = status if isinstance(status, dict) else {}
	if status.get("state") in ("done", "rolled_back", "failed"):
		try:
			os.remove(lock_path)
		except OSError:
			pass
		return False
	work = lock.get("work")
	plan = lenient_json(os.path.join(work, "plan.json")) if path_inside(work, update_dir) else None
	problems = []
	restored = 0
	schema = os.path.join(root, ".claude", "tickets", "ticket.schema.json")
	if isinstance(plan, dict):
		def allowed(target):
			return path_inside(target, viewer_dir) or (isinstance(target, str) and
				os.path.normcase(os.path.normpath(target)) == os.path.normcase(schema))
		for item in plan.get("backed_up") if isinstance(plan.get("backed_up"), list) else []:
			target = item.get("target") if isinstance(item, dict) else None
			backup = item.get("backup") if isinstance(item, dict) else None
			if not allowed(target) or not path_inside(backup, work):
				problems.append("refused a plan entry outside the tool folder: {!r}".format(target))
				continue
			try:
				with open(backup, "rb") as source:
					saved = source.read()
				try:
					with open(target, "rb") as current:
						if current.read() == saved:
							# Never replaced (the swap stopped before it), or already put back. Raw bytes on purpose: the
							# backup is a byte copy of the target, so a CRLF checkout compares equal until the swap.
							continue
				except OSError:
					pass
				with open(target + ".restore", "wb") as copy:
					copy.write(saved)
				for attempt in range(40):
					try:
						os.replace(target + ".restore", target)
						break
					except PermissionError:
						if attempt == 39:
							raise
						time.sleep(0.25)
				restored += 1
			except OSError as error:
				problems.append("cannot restore {}: {}".format(target, error))
				try:
					os.remove(target + ".restore")
				except OSError:
					pass
		for target in plan.get("added") if isinstance(plan.get("added"), list) else []:
			if not allowed(target):
				problems.append("refused a plan entry outside the tool folder: {!r}".format(target))
				continue
			try:
				os.remove(target)
				restored += 1
			except FileNotFoundError:
				pass
			except OSError as error:
				problems.append("cannot remove {}: {}".format(target, error))
		for item in plan.get("copies") if isinstance(plan.get("copies"), list) else []:
			target = item.get("target") if isinstance(item, dict) else None
			if allowed(target) and os.path.isfile(target + ".new"):
				try:
					os.remove(target + ".new")
				except OSError:
					pass
		cache = os.path.join(viewer_dir, "__pycache__")
		if os.path.isdir(cache):
			import shutil
			shutil.rmtree(cache, ignore_errors=True)
	status.update({"state": "rolled_back", "step": "recovery", "detail": "the update stopped part-way (its helper is "
		"gone); serve.py restored the backup on its next start ({} file(s))".format(restored) if plan else
		"the update stopped before it changed any file", "problems": [str(problem).encode("ascii", "replace").decode(
		"ascii") for problem in problems], "changed": []})
	try:
		os.makedirs(update_dir, exist_ok=True)
		with open(status_path, "w", encoding="utf-8", newline="\n") as handle:
			json.dump(status, handle, indent=2)
			handle.write("\n")
		os.remove(lock_path)
	except OSError:
		pass
	print("note: an interrupted update was undone ({} file(s) restored{}); see build/tickets/update/status.json".format(
		restored, ", {} problem(s)".format(len(problems)) if problems else ""))
	return bool(plan)


# --stop needs only the pid file, the repo root and the health marker. tickets.py exits at import when config.json
# is missing or invalid, so a stop is handled without it: a viewer must stay stoppable while the config is broken.
stop_only = __name__ == "__main__" and stop_requested(sys.argv[1:])
if __name__ == "__main__" and not stop_only and stop_repo_root():
	if recover_interrupted_update(stop_repo_root()):
		# This process may have loaded a replaced serve.py; run the restored one instead.
		sys.exit(subprocess.call([sys.executable] + sys.argv))
if stop_only:
	ticket_store = None
	repo_root = stop_repo_root()
	config = stop_config(repo_root) if repo_root else {}
	default_tickets_dir = None
else:
	import tickets as ticket_store  # noqa: E402 - the sibling module, found through the path above
	repo_root = ticket_store.repo_root
	config = ticket_store.config
	default_tickets_dir = ticket_store.tickets_dir
state_dir = os.path.join(repo_root or viewer_dir, "build", "tickets")
pid_path = os.path.join(state_dir, "serve.pid")
log_path = os.path.join(state_dir, "serve.log")

lan_bind = "0.0.0.0"
local_bind = "127.0.0.1"
# The IPv6 listener's address for each bind (B67): localhost may resolve to ::1 before 127.0.0.1.
ipv6_listen = {local_bind: "::1", lan_bind: "::"}
version_pattern = re.compile(r"^[0-9]+\.[0-9]+\.[0-9]+$")
health_path = "/api/health"
health_timeout_seconds = 1.5
# A connect to a localhost port that holds a listener completes at once, but one to a free port is refused only after
# Windows has retried the SYN (about 2 s). So a connect is given this long, and no answer in that time counts as a
# free port.
connect_probe_seconds = 0.3
events_path = "/api/events"
# How often a live page's stream checks the ticket folder for edits.
watch_poll_seconds = 1.0
# A comment line this often keeps an idle stream open and finds pages that have gone away.
heartbeat_seconds = 15.0
# The browser waits this long before reconnecting a dropped stream.
reconnect_ms = 3000
# A detached start takes about a second (the interpreter, the imports, the bind); wait up to this before giving up,
# which leaves room for a slow or busy machine.
start_wait_seconds = 15.0
start_poll_seconds = 0.2
# The log is a debugging aid only; start it fresh when it grows past this.
log_cap_bytes = 512 * 1024
# Windows process creation flags (winbase.h): no console, own process group.
detached_process = 0x00000008
create_new_process_group = 0x00000200
create_no_window = 0x08000000
# An HTTP header token: visible ASCII, no spaces (checked with fullmatch: "$" would let a final newline through).
header_token_pattern = re.compile(r"^[!-~]+$")


def fail_config(text):
	print("ERROR .claude/tickets/config.json: " + text.encode("ascii", "replace").decode("ascii"))
	sys.exit(1)


def text_setting(key):
	value = config.get(key)
	if not isinstance(value, str) or not value.strip():
		fail_config("{} must be a non-empty string".format(key))
	return value


# Returned by /api/health so a start or a stop can tell our server from anything else on the port. Read as they
# stand here, checked by check_settings() for everything but --stop: that needs only the pid file, so a config
# that is broken mid-upgrade must not stop it.
service_marker = stop_marker(config) if stop_only else config.get("viewer_service")
store_name = config.get("store_name")
short_name = config.get("short_name")
theme_key = config.get("theme_key")
server_header = config.get("server_version")


def check_settings():
	"""Fail with a clean ERROR unless every setting the viewer serves with is usable; then use them."""
	global service_marker, store_name, short_name, theme_key, server_header
	service_marker = text_setting("viewer_service")
	store_name = text_setting("store_name")
	short_name = text_setting("short_name")
	theme_key = text_setting("theme_key")
	server_header = text_setting("server_version")
	if not header_token_pattern.fullmatch(server_header):
		fail_config("server_version must be visible ASCII without spaces (it is the HTTP Server header)")
	page_title = config.get("page_title", "")
	if not isinstance(page_title, str):
		fail_config("page_title must be a string")
	# The one-click update's source (B65): absent or empty, updates are off.
	if "update_source" in config and not isinstance(config["update_source"], str):
		fail_config("update_source must be a string: the absolute path of this machine's ticket-system folder (leave it "
			"out to turn updates off)")
	# Absent or empty, the title is "<store_name> Tickets", on the page and in the manifest alike.
	page_values.update({"store_name": store_name, "short_name": short_name, "theme_key": theme_key,
		"page_title": page_title or store_name + " Tickets", "id_max_digits": str(ticket_store.id_max_digits)
		if ticket_store else "", "version": installed_version()})
	viewer_handler.server_version = server_header


content_types = {
	".html": "text/html; charset=utf-8",
	".js": "text/javascript; charset=utf-8",
	".css": "text/css; charset=utf-8",
	".webmanifest": "application/manifest+json; charset=utf-8",
	".png": "image/png",
}

lib_dir = os.path.join(viewer_dir, "lib")
# 1.0.0 kept the libraries in <repo>/external/; served from there when <CLI_DIR>/lib/ lacks the file.
legacy_lib_dir = os.path.join(repo_root or viewer_dir, "external")
index_path = os.path.join(viewer_dir, "index.html")
manifest_path = os.path.join(viewer_dir, "manifest.webmanifest")

# The whole site: each route lists the files it may come from, the first that exists wins. Anything
# not listed is a 404: no directory listing and nothing a crafted path (../, %2e%2e, backslashes) can
# reach, because a request is matched exactly, never joined to a path.
routes = {
	"/": [index_path],
	"/index.html": [index_path],
	"/app.js": [os.path.join(viewer_dir, "app.js")],
	"/style.css": [os.path.join(viewer_dir, "style.css")],
	"/manifest.webmanifest": [manifest_path],
	"/icons/icon-192.png": [os.path.join(viewer_dir, "icons", "icon-192.png")],
	"/icons/icon-512.png": [os.path.join(viewer_dir, "icons", "icon-512.png")],
	"/icons/apple-touch-icon.png": [os.path.join(viewer_dir, "icons", "apple-touch-icon.png")],
	"/lib/marked.min.js": [os.path.join(lib_dir, "marked", "marked.umd.min.js"),
		os.path.join(legacy_lib_dir, "marked", "marked.umd.min.js")],
	"/lib/purify.min.js": [os.path.join(lib_dir, "dompurify", "purify.min.js"),
		os.path.join(legacy_lib_dir, "dompurify", "purify.min.js")],
}

# Scripts only from this origin (so a <script> that slipped past DOMPurify still cannot run);
# markdown images may come from anywhere.
content_security_policy = ("default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; "
	"img-src 'self' data: http: https:; connect-src 'self'; object-src 'none'; base-uri 'none'; "
	"frame-ancestors 'none'; form-action 'none'")

# index.html's {{name}} placeholders; each value is HTML-escaped (quotes too) when the page is served.
page_values = {
	"store_name": "",
	"short_name": "",
	"id_prefix": ticket_store.id_prefix if ticket_store else "",
	"theme_key": "",
	"page_title": "",
	"id_max_digits": "",
	"version": "",
}


def installed_version():
	"""<CLI_DIR>/VERSION (x.y.z), shown in the page's status bar to every client; '' when missing or not x.y.z. Read with
	strip(), so a CRLF checkout reads the same."""
	try:
		with open(os.path.join(viewer_dir, "VERSION"), "r", encoding="utf-8-sig") as handle:
			value = handle.read(64).strip()
	except (OSError, ValueError):
		return ""
	return value if version_pattern.match(value) else ""
placeholder_pattern = re.compile(r"\{\{([a-z_]+)\}\}")

# The one-click update (B65). The token is made at start, kept in memory only and put in the page (a <meta>) for a
# browser on this PC; POST /api/update/apply must send it back in X-Tickets-Token. A custom header forces a CORS
# preflight, which this server never grants, so another site's page cannot send it, and cannot read the page for it.
update_token = secrets.token_urlsafe(32)
update_token_header = "X-Tickets-Token"
update_apply_path = "/api/update/apply"
update_body_max = 4096
loopback_clients = ("127.0.0.1", "::1", "::ffff:127.0.0.1")
updater_module = None


def update_tools():
	"""The sibling updater.py, imported on first use (a viewer that never updates never loads it)."""
	global updater_module
	if updater_module is None:
		import updater as module
		updater_module = module
	return updater_module


def render_page(body, extra=None):
	"""index.html with this project's names filled in, in one pass (a value is never re-scanned)."""
	values = dict(page_values, **(extra or {}))

	def value_of(match):
		name = match.group(1)
		if name not in values:
			return match.group(0)
		return html.escape(values[name], quote=True)
	return placeholder_pattern.sub(value_of, body.decode("utf-8")).encode("utf-8")


def render_manifest(body):
	"""manifest.webmanifest with this project's names in place of the file's generic ones."""
	manifest = json.loads(body.decode("utf-8"))
	manifest["name"] = page_values["page_title"] or store_name + " Tickets"
	manifest["short_name"] = short_name
	manifest["description"] = "Read-only view of the {} project tickets.".format(store_name)
	return (json.dumps(manifest, indent=2, ensure_ascii=False) + "\n").encode("utf-8")


renderers = {
	"/": render_page,
	"/index.html": render_page,
	"/manifest.webmanifest": render_manifest,
}


def utc_now():
	return datetime.now(timezone.utc).replace(microsecond=0).strftime("%Y-%m-%dT%H:%M:%SZ")


def log_line(text):
	os.makedirs(state_dir, exist_ok=True)
	line = "{} {}".format(utc_now(), text)
	with open(log_path, "a", encoding="utf-8", newline="\n") as handle:
		handle.write(line + "\n")
	# Echoed too, so a foreground run (a Claude background task) shows its traffic live.
	if sys.stdout is not None:
		try:
			print(line.encode("ascii", "replace").decode("ascii"), flush=True)
		except (OSError, ValueError):
			pass


def configured_tag_groups():
	"""config.json's tag_groups as written (tag names, min, max), or [] when it has none; the entries that are objects
	only, since the page reads them as such. tickets.py checks them; the viewer only passes them on."""
	groups = config.get("tag_groups")
	if not isinstance(groups, list):
		return []
	return [group for group in groups if isinstance(group, dict)]


def tickets_payload():
	"""Every ticket, the tag store's map (id string -> name, color, description; an old-format tags.json is converted
	in memory, numbered in name order, and reported), its format, the configured tag groups, and one problem line per
	thing that went wrong, among them every tags.json entry that does not read (it is left out of the map, so its chips
	are grey). Problems are plain strings; the page shows them as text. The page resolves a ticket's tag ids only when
	tag_format is 2: an old-format store's ids are provisional and name no tag."""
	# The reads' retry budget (tickets.when_not_busy) is per command; for the long-running viewer, per request. It is
	# kept per thread, so concurrent requests (ThreadingHTTPServer) neither share nor reset each other's.
	ticket_store.reset_busy_read_budget()
	tickets, problems = ticket_store.load_all()
	tags = {}
	tag_format = None
	# One read of tags.json: the store, the format it is written in, and every problem (the whole reason when it does
	# not read at all, else one per entry left out).
	store, version, store_problems = ticket_store.tag_store_state()
	problems += store_problems
	if store is not None:
		tag_format = version
		tags = store["tags"]
		if tag_format == 1:
			problems.append("tags.json is in the old format: run author.py tags --probe")
	return {"generated": utc_now(), "tags": tags, "tag_format": tag_format, "tag_groups": configured_tag_groups(),
		"tickets": tickets, "problems": problems}


def tickets_version():
	"""A digest of every file's name, size and mtime in the ticket folder: it changes whenever a ticket does. The tag store
	lock (tags.json.lock) and the writers' temporary files (*.tmp: a ticket or tags.json before it is moved into place,
	an import's journal) are left out: they change nothing the page shows."""
	entries = []
	try:
		with os.scandir(ticket_store.tickets_dir) as scan:
			for entry in scan:
				if entry.is_file() and entry.name != "tags.json.lock" and not entry.name.lower().endswith(".tmp"):
					stat = entry.stat()
					entries.append("{}|{}|{}".format(entry.name, stat.st_size, stat.st_mtime_ns))
	except OSError:
		return "unreadable"
	entries.sort()
	return hashlib.sha1("\n".join(entries).encode("utf-8")).hexdigest()


class viewer_handler(http.server.BaseHTTPRequestHandler):
	server_version = "ticket-viewer"  # replaced by check_settings() with config's server_version
	sys_version = ""

	def version_string(self):
		# The base class joins server_version and sys_version with a space, which leaves one trailing.
		return self.server_version

	def send_body(self, status, body, content_type, cache):
		self.send_response(status)
		self.send_header("Content-Type", content_type)
		self.send_header("Content-Length", str(len(body)))
		self.send_header("Cache-Control", cache)
		self.send_header("X-Content-Type-Options", "nosniff")
		self.send_header("Referrer-Policy", "no-referrer")
		self.send_header("Content-Security-Policy", content_security_policy)
		self.end_headers()
		self.wfile.write(body)

	def send_json(self, value, status=200):
		body = json.dumps(value, ensure_ascii=False).encode("utf-8")
		self.send_body(status, body, "application/json; charset=utf-8", "no-store")

	def client_is_local(self):
		return self.client_address[0] in loopback_clients

	def local_hosts(self):
		port = self.server.server_address[1]
		return tuple(name.format(port) for name in ("127.0.0.1:{}", "localhost:{}", "[::1]:{}"))

	def local_request(self):
		"""A client on this PC that named this server by a loopback Host (a DNS-rebound name such as evil.example is not)."""
		return self.client_is_local() and (self.one_header("Host") or "").lower() in self.local_hosts()

	def update_info(self):
		"""GET /api/update: the installed and latest release, the state, the release notes between them, the last apply's
		status, and whether this client may apply (a browser on this PC, with nothing running)."""
		tools = update_tools()
		info = tools.discover(config, viewer_dir)
		paths = tools.state_paths(repo_root)
		info["status"] = tools.read_status(paths)
		info["busy"] = os.path.isfile(paths["lock"])
		info["can_apply"] = self.local_request() and info["state"] == "available" and not info["busy"]
		return info

	def refuse_update(self, status, reason):
		log_line("update refused ({}): {} from {}".format(status, reason, self.address_string()))
		self.send_json({"error": reason}, status)

	def one_header(self, name):
		"""The header's value when it is sent exactly once, else None."""
		values = self.headers.get_all(name) or []
		return values[0].strip() if len(values) == 1 else None

	def apply_update(self):
		"""POST /api/update/apply: start the update the page confirmed. Every check must pass, else 403: the client is
		this PC; Host is 127.0.0.1:<port>, localhost:<port> or [::1]:<port>; Origin is http:// plus one of those;
		X-Tickets-Token is this server's token. Then the body must be JSON naming the version GET /api/update offers, and
		no apply may be running (update.lock). No CORS header is ever sent."""
		if update_tools().source_folder(config)[0] is None:
			# No (usable) update_source: updates are off, and nothing is accepted however it is asked.
			self.refuse_update(403, "updates are off (no update_source in config.json)")
			return
		port = self.server.server_address[1]
		hosts = self.local_hosts()
		host = (self.one_header("Host") or "").lower()
		origin = (self.one_header("Origin") or "").lower()
		token = self.one_header(update_token_header) or ""
		token_ok = hmac.compare_digest(token.encode("utf-8", "replace"), update_token.encode("ascii"))
		if not (self.client_is_local() and host in hosts and origin in ["http://" + item for item in hosts] and token_ok):
			self.refuse_update(403, "forbidden")
			return
		if (self.one_header("Content-Type") or "").split(";", 1)[0].strip().lower() != "application/json":
			self.refuse_update(415, "the body must be application/json")
			return
		try:
			length = int(self.one_header("Content-Length") or "")
		except ValueError:
			length = -1
		if not 0 < length <= update_body_max:
			self.refuse_update(400, "a JSON body of at most {} bytes is required".format(update_body_max))
			return
		try:
			request = json.loads(self.rfile.read(length).decode("utf-8"))
		except (ValueError, UnicodeDecodeError, RecursionError):
			request = None
		if not isinstance(request, dict) or not isinstance(request.get("version"), str):
			self.refuse_update(400, "the body must be {\"version\": \"x.y.z\"}")
			return
		tools = update_tools()
		info = tools.discover(config, viewer_dir)
		if info["state"] != "available" or request["version"] != info["latest"]:
			self.refuse_update(409, "{} is not the update on offer ({})".format(ticket_store.ascii_text(
				request["version"][:20]), info["latest"] if info["state"] == "available" else info["state"]))
			return
		paths = tools.state_paths(repo_root)
		version = info["latest"]
		work = os.path.join(paths["update"], version)
		if not tools.take_lock(paths, version, work):
			self.refuse_update(409, "an update is already running")
			return
		viewer = {"pid": os.getpid(), "created": process_created(os.getpid()), "port": port,
			"bind": self.server.bind_address, "service": service_marker,
			"data_dir": ticket_store.tickets_dir if ticket_store.tickets_dir != default_tickets_dir else None}
		log_line("update {} -> {} accepted from {}".format(info["installed"], version, self.address_string()))
		threading.Thread(target=tools.prepare, args=(repo_root, viewer_dir, config, version, viewer),
			daemon=True).start()
		self.send_json({"state": "running", "version": version}, 202)

	def do_POST(self):
		if self.path.split("?", 1)[0] == update_apply_path:
			self.apply_update()
			return
		self.refuse()

	def stream_events(self):
		"""Server-sent events: a 'tickets' event carrying the folder version on connect and on every change."""
		self.send_response(200)
		self.send_header("Content-Type", "text/event-stream; charset=utf-8")
		self.send_header("Cache-Control", "no-store")
		self.send_header("X-Content-Type-Options", "nosniff")
		self.send_header("Content-Security-Policy", content_security_policy)
		self.end_headers()
		last_version = None
		quiet_seconds = 0.0
		try:
			self.wfile.write("retry: {}\n\n".format(reconnect_ms).encode("ascii"))
			while True:
				version = tickets_version()
				if version != last_version:
					self.wfile.write("event: tickets\ndata: {}\n\n".format(version).encode("ascii"))
					last_version = version
					quiet_seconds = 0.0
				elif quiet_seconds >= heartbeat_seconds:
					self.wfile.write(b": keep-alive\n\n")
					quiet_seconds = 0.0
				self.wfile.flush()
				time.sleep(watch_poll_seconds)
				quiet_seconds += watch_poll_seconds
		except OSError:
			# The page closed or the network dropped; the browser reconnects on its own.
			return

	def do_GET(self):
		path = self.path.split("?", 1)[0].split("#", 1)[0]
		if path == "/api/tickets":
			self.send_json(tickets_payload())
			return
		if path == events_path:
			self.stream_events()
			return
		if path == health_path:
			self.send_json({"service": service_marker, "pid": os.getpid(), "bind": self.server.bind_address})
			return
		if path in ("/api/update", "/api/update/status") and not self.local_request():
			# They hold this machine's paths (the source, the work area); a LAN or DNS-rebound client gets nothing.
			self.send_json({"error": "the update routes answer this PC only"}, 403)
			return
		if path == "/api/update":
			self.send_json(self.update_info())
			return
		if path == "/api/update/status":
			tools = update_tools()
			paths = tools.state_paths(repo_root)
			self.send_json({"status": tools.read_status(paths), "busy": os.path.isfile(paths["lock"])})
			return
		target = None
		for candidate in routes.get(path, []):
			if os.path.isfile(candidate):
				target = candidate
				break
		if target is None:
			self.send_body(404, b"not found\n", "text/plain; charset=utf-8", "no-store")
			return
		with open(target, "rb") as handle:
			body = handle.read()
		if path in ("/", "/index.html"):
			# The update token only for a browser on this PC that named a loopback Host: a LAN client, or a page reached
			# through a DNS-rebound name, never gets it.
			body = render_page(body, {"update_token": update_token if self.local_request() else ""})
		elif path in renderers:
			body = renderers[path](body)
		content_type = content_types.get(os.path.splitext(target)[1], "application/octet-stream")
		# no-cache = revalidate every time, so an edited viewer shows on refresh.
		self.send_body(200, body, content_type, "no-cache")

	def refuse(self):
		self.send_response(405)
		self.send_header("Allow", "GET")
		self.send_header("Content-Length", "0")
		self.end_headers()

	do_HEAD = refuse
	# do_POST (above) answers POST /api/update/apply only; every other POST is refused like this.
	do_PUT = refuse
	do_PATCH = refuse
	do_DELETE = refuse
	do_OPTIONS = refuse
	do_TRACE = refuse
	do_CONNECT = refuse

	def log_message(self, format, *args):
		log_line("{} {}".format(self.address_string(), format % args))


class viewer_server(http.server.ThreadingHTTPServer):
	daemon_threads = True
	# Never share the port with a second server (Windows allows a silent double bind otherwise).
	allow_reuse_address = False

	def __init__(self, bind_address, port, listen=None):
		"""bind_address is the viewer's bind (127.0.0.1 or 0.0.0.0), the one /api/health reports and an update restarts
		with; listen is the address this socket takes, when it differs (the IPv6 listener's ::1 or ::)."""
		self.bind_address = bind_address
		listen = listen or bind_address
		if ":" in listen:
			self.address_family = socket.AF_INET6
		super().__init__((listen, port), viewer_handler)

	def server_bind(self):
		# Windows also lets a specific address bind beside a wildcard one; exclusive use refuses that too.
		if hasattr(socket, "SO_EXCLUSIVEADDRUSE"):
			self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
		if self.address_family == socket.AF_INET6 and hasattr(socket, "IPV6_V6ONLY"):
			# IPv6 only: the IPv4 side is the other listener's.
			self.socket.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, 1)
		super().server_bind()


def start_ipv6_listener(port, bind_address):
	"""The IPv6 listener beside the IPv4 one (B67), serving in a daemon thread: ::1 for the default bind, :: for
	--all-interfaces. None, with a log line, where IPv6 or that address is unavailable (the IPv4 URLs still work)."""
	listen = ipv6_listen.get(bind_address, "::1")
	if not socket.has_ipv6:
		log_line("IPv6 listener skipped: this Python has no IPv6 support; use http://127.0.0.1:{}/".format(port))
		return None
	try:
		server = viewer_server(bind_address, port, listen)
	except (OSError, ValueError) as error:
		log_line("IPv6 listener [{}]:{} skipped ({}); use http://127.0.0.1:{}/".format(listen, port, error, port))
		return None
	threading.Thread(target=server.serve_forever, name="ipv6-listener", daemon=True).start()
	log_line("serving on [{}]:{} too (pid {})".format(listen, port, os.getpid()))
	return server


def lan_address():
	"""The private IPv4 address of the default-route adapter, or None.

	Only that adapter is presented as reachable from the LAN: host-only adapters (WSL, Hyper-V's
	Default Switch, VPNs) carry private addresses too, but a phone on the LAN cannot reach them.
	"""
	probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
	try:
		# connect() on UDP only picks the outgoing interface; nothing is sent.
		probe.connect(("192.0.2.1", 9))
		text = probe.getsockname()[0]
	except OSError:
		return None
	finally:
		probe.close()
	try:
		address = ipaddress.ip_address(text)
	except ValueError:
		return None
	if address.is_private and not address.is_loopback and not address.is_link_local:
		return text
	return None


def print_urls(port, bind_address):
	print("ticket viewer: http://127.0.0.1:{}/".format(port))
	if bind_address == lan_bind:
		address = lan_address()
		if address:
			print("on the LAN:    http://{}:{}/".format(address, port))
		else:
			print("on the LAN:    no private IPv4 address on the default-route adapter of this PC")
	else:
		print("LAN access:    off (bound to 127.0.0.1; start with --all-interfaces for the phone)")


def health(port, marker=None):
	"""The /api/health answer when it carries our marker (config's, or the one given), {} when something else
	answers, None when nothing does."""
	marker = marker if marker is not None else service_marker
	if not port_open(port):
		return None
	try:
		with urllib.request.urlopen("http://127.0.0.1:{}{}".format(port, health_path),
				timeout=health_timeout_seconds) as response:
			value = json.loads(response.read().decode("utf-8"))
	except (OSError, ValueError, RecursionError):
		return None
	if isinstance(value, dict) and marker is not None and value.get("service") == marker:
		return value
	return {}


def valid_port(value):
	"""True for an int (not a bool) in 1-65535. A pid file is data from disk, so its port is checked before use."""
	return isinstance(value, int) and not isinstance(value, bool) and 0 < value <= 65535


def port_open(port):
	"""True when something accepts a connection on 127.0.0.1:port. Asked with a short timeout: a free port is refused
	slowly on Windows, and a listener on localhost never needs longer. False for anything that is not a port."""
	if not valid_port(port):
		return False
	probe = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	probe.settimeout(connect_probe_seconds)
	try:
		return probe.connect_ex(("127.0.0.1", port)) == 0
	finally:
		probe.close()


def read_pid_file():
	try:
		with open(pid_path, "r", encoding="utf-8") as handle:
			record = json.load(handle)
	except (OSError, ValueError, RecursionError):
		return None
	return record if isinstance(record, dict) else None


def write_pid_file(pid, port, bind_address):
	"""Record this server. Besides where it serves: its health marker, so --stop can confirm it while config.json
	is broken; its working folder, against which a relative script path on its command line is read; and its
	creation time, so a later process that reuses the pid is never taken for it."""
	os.makedirs(state_dir, exist_ok=True)
	record = {"pid": pid, "port": port, "bind": bind_address, "script": os.path.abspath(__file__),
		"service": service_marker, "cwd": os.getcwd(), "created": process_created(pid), "started": utc_now()}
	with open(pid_path, "w", encoding="utf-8", newline="\n") as handle:
		json.dump(record, handle, indent=2)
		handle.write("\n")


def recorded_text(record, key):
	"""A non-empty string the record holds under key, else None (a pid file from 1.0.0 lacks the newer keys)."""
	value = record.get(key)
	return value if isinstance(value, str) and value.strip() else None


def recorded_instance(record, pid):
	"""True when pid is still the process that wrote record (the same creation time), False when the pid now
	belongs to a later process, None when that cannot be told (a record without 'created', or no answer)."""
	created = record.get("created")
	if not is_integer(created):
		return None
	current = process_created(pid)
	if current is None:
		return None
	return current == created


def live_recorded_server(own_port):
	"""The pid file's record when it names a running server of this script other than this process, else None.

	Live means its /api/health answers with that pid, or its command line holds this script's path."""
	record = read_pid_file()
	if not record:
		return None
	try:
		pid = int(record.get("pid", 0))
	except (TypeError, ValueError):
		return None
	if pid <= 0 or pid == os.getpid():
		return None
	port = record.get("port")
	if valid_port(port) and port == own_port:
		# This process holds that port exclusively, so the server that recorded it has let it go: a stale record.
		# (Probing it would only reach this process's own socket, which answers nothing until it serves.)
		return None
	same = recorded_instance(record, pid)
	if same is False:
		return None
	if valid_port(port):
		status = health(port)
		if status and status.get("pid") == pid:
			return record
	if is_our_command_line(command_line_of(pid), recorded_text(record, "cwd") if same else None):
		return record
	return None


def remove_pid_file(pid):
	"""Delete serve.pid only while it still names pid, so nobody deletes another server's record."""
	record = read_pid_file()
	if record is None or record.get("pid") != pid:
		return
	try:
		os.remove(pid_path)
	except OSError:
		pass


def command_line_of(pid):
	"""The process's command line (Windows, through CIM), or '' when it is gone or unreadable."""
	if os.name != "nt":
		try:
			with open("/proc/{}/cmdline".format(int(pid)), "rb") as handle:
				return handle.read().replace(b"\0", b" ").decode("utf-8", "replace")
		except OSError:
			return ""
	# PowerShell writes a pipe in the OEM codepage unless told otherwise, which mangles a non-ASCII path.
	query = ("[Console]::OutputEncoding=[Text.Encoding]::UTF8;"
		"(Get-CimInstance Win32_Process -Filter 'ProcessId={}').CommandLine".format(int(pid)))
	try:
		result = subprocess.run(["powershell", "-NoProfile", "-NonInteractive", "-Command", query],
			capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=20,
			creationflags=create_no_window)
	except (OSError, subprocess.SubprocessError):
		return ""
	return result.stdout.strip()


def long_path_name(path):
	"""path with every 8.3 short component expanded (Windows, the file must exist), else path unchanged."""
	if os.name != "nt":
		return path
	import ctypes
	function = ctypes.windll.kernel32.GetLongPathNameW
	function.restype = ctypes.c_uint32
	size = 1024
	for _ in range(2):
		buffer = ctypes.create_unicode_buffer(size)
		length = function(path, buffer, size)
		if length == 0:
			return path
		if length < size:
			return buffer.value
		size = length + 1
	return path


def same_file_path(left, right):
	"""True when two absolute paths name the same file whatever their case, slashes, 8.3 short components or
	junctions and symlinks in the folder path."""
	def canonical(path, resolve):
		path = os.path.abspath(path)
		if resolve:
			path = os.path.realpath(path)
		return os.path.normcase(long_path_name(path))
	# realpath resolves junctions and symlinks. No samefile() fallback: a file system reporting file id 0 would make
	# any file on the volume "the same".
	try:
		return canonical(left, False) == canonical(right, False) or canonical(left, True) == canonical(right, True)
	except (OSError, ValueError):
		return False


def split_command_line(command_line):
	"""The arguments of a process command line, split the way its own runtime did."""
	if os.name == "nt":
		import ctypes
		from ctypes import wintypes
		shell32 = ctypes.windll.shell32
		shell32.CommandLineToArgvW.restype = ctypes.POINTER(wintypes.LPWSTR)
		shell32.CommandLineToArgvW.argtypes = [wintypes.LPCWSTR, ctypes.POINTER(ctypes.c_int)]
		ctypes.windll.kernel32.LocalFree.argtypes = [ctypes.c_void_p]
		count = ctypes.c_int(0)
		parts = shell32.CommandLineToArgvW(command_line, ctypes.byref(count))
		if parts:
			try:
				return [parts[index] for index in range(count.value)]
			finally:
				ctypes.windll.kernel32.LocalFree(ctypes.cast(parts, ctypes.c_void_p))
		return []
	return command_line.split()


def is_our_command_line(command_line, cwd=None):
	"""True when an argument of the command line is this very serve.py, however that path is spelled (long, 8.3
	short, any case). An absolute argument always counts; a relative one (python -u tools/tickets/serve.py) only
	when cwd, the process's own working folder as it recorded it, is given, and then read against it. A longer
	name such as serve.py.bak, another script, or another folder's serve.py never counts."""
	script = os.path.abspath(__file__)
	if os.name != "nt":
		# /proc/<pid>/cmdline was joined with spaces, which cannot be split back reliably.
		return script in command_line
	try:
		arguments = split_command_line(command_line)
	except (OSError, AttributeError, ValueError):
		return False
	if cwd is not None and not os.path.isabs(cwd):
		cwd = None
	for argument in arguments:
		if not argument:
			continue
		if os.path.isabs(argument):
			path = argument
		elif cwd is not None:
			path = os.path.join(cwd, argument)
		else:
			continue
		if same_file_path(path, script):
			return True
	return False


def report_serving(status, port, bind_address):
	print("already serving (pid {}, bound to {})".format(status.get("pid"), status.get("bind")))
	if status.get("bind") != bind_address:
		print("note: it is bound to {}; run --stop, then start again to change that".format(status.get("bind")))
	print_urls(port, status.get("bind"))


def ensure_running(port, bind_address):
	# One quick connect tells a free port from a held one; only a held port is asked who holds it.
	if port_open(port):
		status = health(port)
		if status:
			report_serving(status, port, bind_address)
			return 0
		print("ERROR port {} is held by something that is not the ticket viewer; not starting".format(port))
		return 1
	os.makedirs(state_dir, exist_ok=True)
	try:
		if os.path.isfile(log_path) and os.path.getsize(log_path) > log_cap_bytes:
			os.remove(log_path)
	except OSError:
		# Another start is trimming or writing it; the log is only a debugging aid.
		pass
	arguments = [sys.executable, os.path.abspath(__file__), "--foreground", "--port", str(port)]
	if bind_address == lan_bind:
		arguments.append("--all-interfaces")
	if ticket_store.tickets_dir != default_tickets_dir:
		arguments += ["--data-dir", ticket_store.tickets_dir]
	options = {"stdin": subprocess.DEVNULL, "stdout": subprocess.DEVNULL, "stderr": subprocess.DEVNULL,
		"cwd": state_dir, "close_fds": True}
	if os.name == "nt":
		options["creationflags"] = detached_process | create_new_process_group | create_no_window
	else:
		options["start_new_session"] = True
	# The child records serve.pid itself once it holds the port; this process never writes or deletes it,
	# so a start that loses a race for the port cannot clobber the winner's record.
	child = subprocess.Popen(arguments, **options)
	deadline = time.monotonic() + start_wait_seconds
	while time.monotonic() < deadline:
		status = health(port)
		if status and status.get("pid") == child.pid:
			print("started (pid {}, bound to {})".format(child.pid, bind_address))
			print_urls(port, bind_address)
			return 0
		if child.poll() is not None:
			# Our server has exited. When another start won the race for the port, that one serves it;
			# keep asking while the port is held, since the winner may still be coming up.
			if status:
				report_serving(status, port, bind_address)
				return 0
			if not port_open(port):
				break
		time.sleep(start_poll_seconds)
	print("ERROR the server did not come up; see {}".format(os.path.relpath(log_path, repo_root)))
	return 1


def stop():
	record = read_pid_file()
	if not record:
		print("nothing recorded in {}; nothing stopped".format(
			os.path.relpath(pid_path, repo_root) if repo_root else "build/tickets/serve.pid"))
		return 0
	try:
		pid = int(record.get("pid", 0))
	except (TypeError, ValueError):
		pid = 0
	port = record.get("port")
	# A pid a later process has taken is never confirmed, whatever answers on the port.
	same = recorded_instance(record, pid) if pid > 0 else None
	if same is False:
		remove_pid_file(record.get("pid"))
		print("recorded pid {} is not running (a later process has that pid); cleared the pid file".format(pid))
		return 0
	# The marker config.json gives, then the one the server recorded (a broken or missing config.json gives none; a
	# config.json changed while the viewer ran gives one the server does not answer with).
	markers = []
	for marker in (service_marker, recorded_text(record, "service")):
		if marker is not None and marker not in markers:
			markers.append(marker)
	confirmed = False
	if valid_port(port):
		for marker in markers:
			status = health(port, marker)
			if status and status.get("pid") == pid:
				confirmed = True
				break
	if not confirmed:
		command_line = command_line_of(pid) if pid > 0 else ""
		if not command_line:
			remove_pid_file(record.get("pid"))
			print("recorded pid {} is not running; cleared the pid file".format(pid))
			return 0
		# A detached start passes this script's absolute path, long or 8.3 short, so a match is our server and
		# nothing else. A foreground start may pass a relative one: that is read against the folder the server
		# recorded, and only while the pid is still the process that recorded it.
		confirmed = is_our_command_line(command_line, recorded_text(record, "cwd") if same else None)
	if not confirmed:
		print("ERROR recorded pid {} is not this ticket server; not stopping it".format(pid))
		return 1
	os.kill(pid, signal.SIGTERM)
	remove_pid_file(record.get("pid"))
	print("stopped the ticket viewer (pid {})".format(pid))
	return 0


def log_update_check():
	"""The start's update check (B65): one log line when a release is on offer or the source cannot be read."""
	try:
		info = update_tools().discover(config, viewer_dir)
	except Exception as error:  # noqa: BLE001 - the check never stops the viewer from serving
		log_line("update check failed: {}: {}".format(type(error).__name__, error))
		return
	if info["state"] == "available":
		log_line("update: {} is available (installed {}); the page shows it".format(info["latest"], info["installed"]))
	elif info["state"] in ("unavailable", "unknown"):
		log_line("update check: {}".format(info["reason"]))


def run_foreground(port, bind_address):
	try:
		server = viewer_server(bind_address, port)
	except OSError as error:
		# Nothing is recorded or removed: serve.pid belongs to whichever server holds the port.
		log_line("cannot bind {}:{} (pid {}): {}".format(bind_address, port, os.getpid(), error))
		status = health(port)
		if status:
			print("ERROR port {} is already served by the ticket viewer (pid {}); run --stop first".format(
				port, status.get("pid")))
		else:
			print("ERROR cannot bind {}:{} ({})".format(bind_address, port, error))
		return 1
	# Written only now that this process holds the port, so --stop and the idempotent start find it, and
	# never over the record of a live server of this script: that one keeps the record, and this one (a
	# second port, a test) is reached from its own console only.
	other = live_recorded_server(port)
	if other is None:
		write_pid_file(os.getpid(), port, bind_address)
	else:
		note = ("serve.pid names the running viewer (pid {}, port {}); this one (pid {}) is not recorded, so "
			"--stop will not reach it".format(other.get("pid"), other.get("port"), os.getpid()))
		log_line("note: " + note)  # echoed to the console too
	log_line("serving on {}:{} (pid {})".format(bind_address, port, os.getpid()))
	server6 = start_ipv6_listener(port, bind_address)
	log_update_check()
	if sys.stdout is not None:
		try:
			print_urls(port, bind_address)
			print("Ctrl+C to stop")
		except (OSError, ValueError):
			pass
	try:
		server.serve_forever()
	except KeyboardInterrupt:
		pass
	finally:
		if server6 is not None:
			server6.shutdown()
			server6.server_close()
		server.server_close()
		log_line("stopped (pid {})".format(os.getpid()))
		remove_pid_file(os.getpid())
	return 0


def use_data_dir(path):
	ticket_store.tickets_dir = path
	ticket_store.tags_path = os.path.join(path, "tags.json")
	ticket_store.schema_path = os.path.join(path, "ticket.schema.json")


def configured_port():
	"""viewer_port from config.json; a clean ERROR naming the key when it is missing or not a port."""
	if "viewer_port" not in config:
		fail_config("missing 'viewer_port', the port this viewer serves on (any free port no other viewer on this "
			"machine uses; 18000-19000 is the suggested range); add it, or pass --port")
	port = config["viewer_port"]
	if not isinstance(port, int) or isinstance(port, bool) or not 1 <= port <= 65535:
		fail_config("viewer_port must be a port number (1-65535), not {}".format(json.dumps(port)))
	return port


def main():
	shown_name = ticket_store.ascii_text(store_name) if ticket_store else "the"
	parser = argparse.ArgumentParser(description="{} ticket viewer (read-only).".format(shown_name), allow_abbrev=False)
	parser.add_argument("--port", type=int, help="serve on this port instead of config.json's viewer_port")
	bind_choice = parser.add_mutually_exclusive_group()
	bind_choice.add_argument("--all-interfaces", action="store_true", help="bind 0.0.0.0 and ::, reachable from the "
		"LAN (the default is 127.0.0.1 and ::1, this PC only)")
	# Before 1.4.0 the default was the LAN and this flag opted out; it is kept so existing start lines still work.
	bind_choice.add_argument("--local-only", action="store_true", help="bind 127.0.0.1 (the default; kept for older "
		"start lines)")
	parser.add_argument("--foreground", action="store_true", help="run in this console")
	parser.add_argument("--stop", action="store_true", help="stop the recorded server")
	parser.add_argument("--data-dir", help="serve tickets from this directory instead (a test fixture)")
	args = parser.parse_args()
	if args.stop:
		return stop()
	if args.data_dir:
		use_data_dir(os.path.abspath(args.data_dir))
	bind_address = lan_bind if args.all_interfaces else local_bind
	check_settings()
	if args.port is not None and not 1 <= args.port <= 65535:
		print("ERROR --port must be a port number (1-65535), not {}".format(args.port))
		return 1
	port = args.port if args.port is not None else configured_port()
	if args.foreground:
		return run_foreground(port, bind_address)
	return ensure_running(port, bind_address)


if __name__ == "__main__":
	sys.exit(main())
