"""Launch a debug client, wait for its bridge, stop ONLY that process.

Rules this module enforces:
- The exe runs from its EXISTING path; a copy elsewhere is a new path and a new firewall prompt.
- The working directory is game/client, so assets and settings.json resolve.
- `game/client/Game_x64_win.exe` (Game_x64_linux) is the deployed client; a Release build has NO bridge,
  so the exe is checked for the bridge's log string before launch.
- The PID is recorded and only that PID is ever stopped. Nothing is killed by image name.
- A port some other client already answers on is refused (one bridge connection per client).
"""

import ctypes
import os
import socket
import subprocess
import sys
import time

from . import bridge
from . import common

# Compiled only into DEF_CLAUDE_TESTING builds (agent_bridge.cpp: "agent bridge: listening on ...").
bridge_marker = b"agent bridge: listening"
default_start_screen = None  # this client has no --start-screen flag
bridge_wait_s = 60.0
stop_wait_s = 15.0
port_check_timeout_s = 0.5
# Win32 ShowWindow commands, and CreateProcess's CREATE_BREAKAWAY_FROM_JOB.
sw_restore = 9
sw_minimize = 6
create_breakaway_from_job = 0x01000000
# Images stop_recorded may stop: the Debug client, plus Release for an explicit --exe override.
client_image_names = ("game_x64_win.exe", "game_x64_linux")


def exe_has_bridge(path):
	"""True when the exe carries the agent bridge (a Debug build). Reads the file once."""
	try:
		with open(path, "rb") as handle:
			return bridge_marker in handle.read()
	except OSError:
		return False


def pick_client_exe(preferred=None):
	"""The exe to launch: `preferred` if given, else the deployed game/client exe. Raises when it carries
	no bridge (a Release build)."""
	candidates = [preferred] if preferred else [common.deployed_client_exe, common.debug_client_exe]
	for candidate in candidates:
		if candidate and os.path.isfile(candidate) and exe_has_bridge(candidate):
			return os.path.abspath(candidate)
	raise common.flow_error("no client exe with the agent bridge among {} (the deployed game/client exe must be a "
	                        "Debug build; build and deploy Debug)".format(candidates))


def port_answers(port, host=bridge.default_host):
	"""True when something already accepts connections on the port."""
	try:
		with socket.create_connection((host, port), timeout=port_check_timeout_s):
			return True
	except OSError:
		return False


def _image_name(pid):
	"""The process image for a PID (tasklist), or None when no such process runs."""
	result = subprocess.run(["tasklist", "/FI", "PID eq {}".format(int(pid)), "/FO", "CSV", "/NH"],
	                        capture_output=True, text=True)
	line = result.stdout.strip()
	if not line.startswith('"'):
		return None
	return line.split('","')[0].strip('"')


def _find_window(pid):
	"""The top-level visible window of a PID (Windows only), or None."""
	if sys.platform != "win32":
		return None
	user32 = ctypes.windll.user32
	found = []
	enum_proc = ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.c_void_p, ctypes.c_void_p)

	def visit(hwnd, _):
		owner = ctypes.c_ulong(0)
		user32.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
		if owner.value == pid and user32.IsWindowVisible(hwnd):
			found.append(hwnd)
			return False
		return True

	user32.EnumWindows(enum_proc(visit), 0)
	return found[0] if found else None


class client_process:
	"""One debug client this caller launched. Use as a context manager to guarantee the stop."""

	def __init__(self, port, exe=None, work_dir=None, start_screen=default_start_screen, extra_args=(),
	             pid_file=None):
		self.port = int(port)
		if pid_file:
			# refused up front: record_pid runs AFTER the launch, so a bad path used to fail with a client running
			common.require_scratch_path(pid_file, "pid file")
		self.exe = pick_client_exe(exe)
		self.work_dir = os.path.abspath(work_dir or common.client_dir)
		self.start_screen = start_screen
		self.extra_args = list(extra_args)
		self.pid_file = pid_file
		self.popen = None
		self.pid = None

	def command_line(self):
		args = [self.exe]
		if self.start_screen:
			raise common.flow_error("this client has no --start-screen flag")
		args += ["--agent-bridge-port={}".format(self.port)]
		return args + self.extra_args

	def launch(self, detach=False):
		"""Start the client. detach=True lets it outlive this Python process (the CLI's launch):
		its own process group, out of the caller's job object when the job allows that."""
		if port_answers(self.port):
			raise common.flow_error("port {} already answers - another client holds it; pick a free "
			                        "--agent-bridge-port".format(self.port))
		options = {"cwd": self.work_dir, "stdin": subprocess.DEVNULL, "stdout": subprocess.DEVNULL,
		           "stderr": subprocess.DEVNULL}
		if detach and sys.platform == "win32":
			group = subprocess.CREATE_NEW_PROCESS_GROUP | subprocess.DETACHED_PROCESS
			try:
				self.popen = subprocess.Popen(self.command_line(), creationflags=group | create_breakaway_from_job,
				                              **options)
			except OSError:
				self.popen = subprocess.Popen(self.command_line(), creationflags=group, **options)
		else:
			self.popen = subprocess.Popen(self.command_line(), **options)
		self.pid = self.popen.pid
		if self.pid_file:
			record_pid(self.pid_file, self.pid, self.port)
		return self

	def wait_bridge(self, timeout_s=bridge_wait_s):
		"""A session once the bridge answers; raises if the process died first."""
		deadline = time.monotonic() + timeout_s
		while time.monotonic() < deadline:
			if not self.alive():
				raise common.flow_error("client pid {} exited (code {}) before its bridge answered".format(
					self.pid, self.popen.returncode if self.popen else "?"))
			try:
				return bridge.session.connect(self.port, timeout_s=bridge.connect_retry_s * 4)
			except bridge.bridge_error:
				continue
		raise common.flow_error("bridge on port {} did not answer within {:g}s".format(self.port, timeout_s))

	def alive(self):
		if self.popen is not None:
			return self.popen.poll() is None
		return self.pid is not None and _image_name(self.pid) is not None

	def stop(self, timeout_s=stop_wait_s):
		"""Terminate THIS process only (its own handle, never an image-name kill)."""
		if self.popen is None or self.popen.poll() is not None:
			return
		self.popen.terminate()
		try:
			self.popen.wait(timeout=timeout_s)
		except subprocess.TimeoutExpired:
			raise common.flow_error("client pid {} did not exit within {:g}s of terminate".format(self.pid, timeout_s))

	def wait_exit(self, timeout_s=stop_wait_s):
		"""True when the process exited on its own (a quit flow) within the timeout."""
		if self.popen is None:
			return not self.alive()
		try:
			self.popen.wait(timeout=timeout_s)
			return True
		except subprocess.TimeoutExpired:
			return False

	def window_minimized(self):
		"""True/False for a visible window, None when it cannot be told (no window yet, not Windows)."""
		hwnd = _find_window(self.pid)
		if hwnd is None:
			return None
		return bool(ctypes.windll.user32.IsIconic(hwnd))

	def restore_window(self):
		"""Un-minimize: a minimized client returns stale screenshots and can mis-map probes."""
		hwnd = _find_window(self.pid)
		if hwnd is not None and ctypes.windll.user32.IsIconic(hwnd):
			ctypes.windll.user32.ShowWindow(hwnd, sw_restore)
			return True
		return False

	def minimize_window(self):
		hwnd = _find_window(self.pid)
		if hwnd is not None:
			ctypes.windll.user32.ShowWindow(hwnd, sw_minimize)
			return True
		return False

	def __enter__(self):
		if self.popen is None:
			self.launch()
		return self

	def __exit__(self, *exc):
		self.stop()
		return False


def record_pid(pid_file, pid, port):
	"""Append 'pid port' to a caller's pid file (so a later process can stop only what it launched)."""
	common.require_scratch_path(pid_file, "pid file")
	os.makedirs(os.path.dirname(pid_file), exist_ok=True)
	with open(pid_file, "a", encoding="ascii") as handle:
		handle.write("{} {}\n".format(pid, port))


def recorded_pids(pid_file):
	"""[(pid, port)] from a pid file written by record_pid."""
	if not os.path.isfile(pid_file):
		return []
	rows = []
	with open(pid_file, "r", encoding="ascii") as handle:
		for line in handle:
			parts = line.split()
			if len(parts) == 2 and parts[0].isdigit() and parts[1].isdigit():
				rows.append((int(parts[0]), int(parts[1])))
	return rows


def stop_recorded(pid_file, pid=None, timeout_s=stop_wait_s):
	"""Stop PIDs listed in `pid_file` (one, or all) - never any other process. Returns what it did.

	Checked before each kill: the PID is in the file AND its image is still the game client exe, so a PID the
	OS reused for another program is left alone.
	"""
	report = []
	for listed_pid, port in recorded_pids(pid_file):
		if pid is not None and listed_pid != int(pid):
			continue
		image = _image_name(listed_pid)
		if image is None:
			report.append({"pid": listed_pid, "port": port, "action": "already gone"})
			continue
		if image.lower() not in client_image_names:
			report.append({"pid": listed_pid, "port": port, "action": "left alone (image is {})".format(image)})
			continue
		subprocess.run(["taskkill", "/PID", str(listed_pid)], capture_output=True, text=True)
		gone = common.poll_until(lambda: _image_name(listed_pid) is None, timeout_s, bridge.poll_interval_s * 4)
		if not gone:
			# A GUI app may ignore WM_CLOSE; force THIS pid only.
			subprocess.run(["taskkill", "/F", "/PID", str(listed_pid)], capture_output=True, text=True)
			gone = common.poll_until(lambda: _image_name(listed_pid) is None, timeout_s, bridge.poll_interval_s * 4)
		report.append({"pid": listed_pid, "port": port, "action": "stopped" if gone else "still running"})
	if pid is not None and not report:
		raise common.flow_error("pid {} is not in {} - only PIDs this tooling launched can be stopped".format(
			pid, pid_file))
	return report
