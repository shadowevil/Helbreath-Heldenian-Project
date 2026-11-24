"""launch_client: the guaranteed-cleanup wrapper around one driven client.

	with launch_client(port=8892) as run:        # files saved, launched, bridge up
		flows.log_in(run.session, ident)
		...
	# on ANY exit: session closed, THIS pid stopped, client.json + remembered_account.txt restored
	# byte-identical and verified by hash - in that order, so the stopped client cannot re-write them.
"""

import contextlib
import os

from . import client_files
from . import client_process
from . import common
from . import flows

restore_window_wait_s = 10.0
restore_window_poll_s = 0.25
# How the client is muted: "config" writes audio.master_muted into client.json before launch (silent
# from the first frame); "bridge" clicks Settings > Master after launch; None leaves sound alone.
mute_modes = ("config", "bridge", None)


class running_client:
	"""What launch_client yields: the process, its session, the file guard, and the mute result."""

	def __init__(self, process, session, files):
		self.process = process
		self.session = session
		self.files = files
		self.mute = None
		self.window_minimized_at_launch = None

	def client_json(self):
		return os.path.join(self.process.work_dir, common.client_json_name)


@contextlib.contextmanager
def launch_client(port, exe=None, work_dir=None, start_screen=client_process.default_start_screen, extra_args=(),
                  mute=None, pid_file=None, backup_dir=None, restore_window=True):
	"""Launch a debug client on `port` and yield a running_client; always clean up.

	mute: see mute_modes. With "config" the Settings tick is READ back as proof on the main menu; on a
	logged-in start screen (charselect, charcreate) the proof is client.json's audio.master_muted
	instead (flows.prove_launch_mute, HB-0217). restore_window: un-minimize it (a minimized client returns stale
	screenshots and can mis-map probes). backup_dir (under .claude/scratch/) also writes the file copies.
	"""
	if mute not in mute_modes:
		raise common.flow_error("mute must be one of {}".format(mute_modes))
	process = client_process.client_process(port, exe=exe, work_dir=work_dir, start_screen=start_screen,
	                                        extra_args=extra_args, pid_file=pid_file)
	guard = client_files.client_files_guard(client_files.default_client_files(process.work_dir), backup_dir)
	guard.save()
	run = None
	try:
		if mute == "config":
			client_files.premute(os.path.join(process.work_dir, common.client_json_name))
		process.launch()
		run = running_client(process, process.wait_bridge(), guard)
		if restore_window:
			seen = common.poll_until(lambda: process.window_minimized() is not None, restore_window_wait_s,
			                         restore_window_poll_s)
			run.window_minimized_at_launch = process.window_minimized() if seen else None
			process.restore_window()
		if mute:
			# "config": already muted, so this only proves it (no click); "bridge": it clicks
			run.mute = flows.prove_launch_mute(run.session, mute, run.client_json())
		yield run
	finally:
		try:
			if run is not None:
				run.session.close()
		finally:
			try:
				process.stop()
			finally:
				guard.restore()
