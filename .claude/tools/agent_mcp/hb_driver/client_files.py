"""Byte-exact save and restore of the files a client rewrites in game/client.

A driven client persists every settings change (muting included) to settings.json at once, and the
login screen writes or deletes remembered_account.txt when the account field changes. Both are the
USER's files. client_files_guard copies them before anything runs and puts them back byte-identical
afterwards, proven by SHA-256 - including deleting one that did not exist before.

Restore AFTER the client has stopped: a running client rewrites client.json on its next settings
change. launch_client (session.py) orders it that way.
"""

import hashlib
import json
import os

from . import common


def sha256_of(data):
	return hashlib.sha256(data).hexdigest() if data is not None else None


def read_bytes(path):
	"""The file's bytes, or None when it does not exist."""
	try:
		with open(path, "rb") as handle:
			return handle.read()
	except FileNotFoundError:
		return None


def default_client_files(work_dir=None):
	work_dir = work_dir or common.client_dir
	return [os.path.join(work_dir, common.client_json_name),
	        os.path.join(work_dir, common.remembered_account_name)]


class client_files_guard:
	"""Context manager: save on enter, restore + verify on exit (also on an exception).

	`backup_dir` (optional, under .claude/scratch/) also writes the copies to disk, so a crashed driver can
	be recovered with restore_from_backup().
	"""

	def __init__(self, paths=None, backup_dir=None):
		self.paths = [os.path.abspath(path) for path in (paths or default_client_files())]
		self.backup_dir = backup_dir
		self.saved = {}
		self.report = None

	def save(self):
		for path in self.paths:
			self.saved[path] = read_bytes(path)
		if self.backup_dir:
			write_backup(self.backup_dir, self.saved)
		return {path: sha256_of(data) for path, data in self.saved.items()}

	def restore(self):
		"""Put every file back; returns [{path, before_restore_sha256, restored_sha256, ok}]."""
		self.report = restore_files(self.saved)
		failed = [row for row in self.report if not row["ok"]]
		if failed:
			raise common.flow_error("client file restore did not verify: {}".format(failed))
		return self.report

	def __enter__(self):
		self.save()
		return self

	def __exit__(self, *exc):
		self.restore()
		return False


def restore_files(saved):
	report = []
	for path, data in saved.items():
		current = read_bytes(path)
		if data is None:
			if current is not None:
				os.remove(path)  # the run created it; absence is the original state
		elif current != data:
			with open(path, "wb") as handle:
				handle.write(data)
		after = read_bytes(path)
		report.append({"path": path, "changed_during_run": current != data,
		               "restored_sha256": sha256_of(after), "expected_sha256": sha256_of(data),
		               "ok": after == data})
	return report


def premute(client_json):
	"""Write audio.master_muted = true into client.json BEFORE a launch, so the client is silent from
	its first frame. Only inside a client_files_guard: the guard puts the original bytes back."""
	data = common.read_json_file(client_json) if os.path.isfile(client_json) else {}
	data.setdefault("audio", {})["master_muted"] = True
	with open(client_json, "w", encoding="utf-8", newline="\n") as handle:
		json.dump(data, handle, indent="\t")
		handle.write("\n")


# --- on-disk copies, so a crashed run can still be put back --------------------------------------

manifest_name = "client_files_manifest.json"


def write_backup(backup_dir, saved):
	common.require_scratch_path(backup_dir, "client file backup dir")
	os.makedirs(backup_dir, exist_ok=True)
	manifest = []
	for index, (path, data) in enumerate(saved.items()):
		copy_name = None
		if data is not None:
			copy_name = "file{}.bin".format(index)
			with open(os.path.join(backup_dir, copy_name), "wb") as handle:
				handle.write(data)
		manifest.append({"path": path, "copy": copy_name, "sha256": sha256_of(data)})
	common.write_json_file(os.path.join(backup_dir, manifest_name), manifest)


def load_backup(backup_dir):
	manifest = common.read_json_file(os.path.join(backup_dir, manifest_name))
	saved = {}
	for row in manifest:
		data = None
		if row.get("copy"):
			data = read_bytes(os.path.join(backup_dir, row["copy"]))
			if sha256_of(data) != row.get("sha256"):
				raise common.flow_error("backup copy of {} does not match its recorded hash".format(row["path"]))
		saved[row["path"]] = data
	return saved


def restore_from_backup(backup_dir):
	"""Restore from a write_backup() folder (the CLI's `restore-client-files`)."""
	report = restore_files(load_backup(backup_dir))
	failed = [row for row in report if not row["ok"]]
	if failed:
		raise common.flow_error("client file restore did not verify: {}".format(failed))
	return report
