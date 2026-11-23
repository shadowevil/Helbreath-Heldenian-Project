"""The detached half of the viewer's one-click update (B65); the viewer starts the NEW release's copy of this file.

    python -B <export>/payload/src/tools/tickets/updater_helper.py <repo>/build/tickets/update/<x.y.z>/plan.json

The viewer has exported and verified the release, merged the schema, backed up every file it replaces and written
plan.json (updater.prepare). This process then:
  1. stops the viewer that asked (its pid, confirmed by /api/health and its creation time);
  2. installs each file as <name>.new, then os.replace, retrying a held file for up to 10 s, and deletes the files the
     release dropped and <CLI_DIR>/__pycache__;
  3. runs `tickets.py validate`;
  4. starts the viewer again with the same port, bind and data folder, and waits for /api/health with its marker;
  5. on success stages the skill, the hooks and the rules doc and rewrites .claude/tickets/README.md; on a failure in
     2-4 it restores the backup, starts the old viewer and records rolled_back with the failing step's output.
The restarted viewer is detached: a viewer a Claude session ran in the foreground (a background task) is not
reattached, so that task ends; start it again from the session (serve.py --stop, then serve.py --foreground).
It never runs git in the adopter's repository. Its output goes to helper.log beside plan.json; its progress to build/tickets/update/status.json.
Stdlib only; ASCII output.
"""

import json
import os
import signal
import sys
import time
import urllib.request

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import updater  # noqa: E402 - the release's own module, beside this file

stop_wait_seconds = 10.0
health_wait_seconds = 20.0
lock_wait_seconds = 10.0


def log(text):
	print("{} {}".format(updater.utc_now(), updater.ascii_text(text)), flush=True)


class helper(object):
	def __init__(self, plan):
		self.plan = plan
		self.paths = updater.state_paths(plan["repo"])
		self.viewer = plan.get("viewer") or {}
		self.started_new = False

	def status(self, state, **fields):
		log("status {} {}".format(state, fields.get("step", "")))
		updater.write_status(self.paths, state, **fields)

	# --- the viewer ----------------------------------------------------------------------------------------------

	def health(self):
		port = self.viewer.get("port")
		try:
			with urllib.request.urlopen("http://127.0.0.1:{}/api/health".format(port), timeout=1.5) as response:
				value = json.loads(response.read().decode("utf-8"))
		except (OSError, ValueError):
			return None
		if isinstance(value, dict) and value.get("service") == self.viewer.get("service"):
			return value
		return {}

	def stop_viewer(self):
		"""Stop the viewer that asked for the update; a problem line or None."""
		pid = self.viewer.get("pid")
		status = self.health()
		if status and status.get("pid") == pid and updater.process_alive(pid, self.viewer.get("created")):
			try:
				os.kill(pid, signal.SIGTERM)
			except OSError as error:
				return "cannot stop the viewer (pid {}): {}".format(pid, error)
		deadline = time.monotonic() + stop_wait_seconds
		while time.monotonic() < deadline:
			if self.health() is None:
				self.clear_pid_file(pid)
				return None
			time.sleep(0.2)
		return "the viewer on port {} did not stop".format(self.viewer.get("port"))

	def clear_pid_file(self, pid):
		path = os.path.join(self.paths["build"], "serve.pid")
		record = updater.read_json_file(path, 64 * 1024)
		if isinstance(record, dict) and record.get("pid") == pid:
			try:
				os.remove(path)
			except OSError:
				pass

	def start_viewer(self):
		"""Start the viewer in the tool folder with the old flags; (ok, output)."""
		arguments = [self.plan["python"], os.path.join(self.plan["cli_dir"], "serve.py"), "--port",
			str(self.viewer.get("port"))]
		# The viewer's IPv4 bind; serve.py opens the IPv6 listener beside it on its own (1.5.3, B67).
		if self.viewer.get("bind") in ("0.0.0.0", "::"):
			arguments.append("--all-interfaces")
		if self.viewer.get("data_dir"):
			arguments += ["--data-dir", self.viewer["data_dir"]]
		code, output = updater.run(arguments, cwd=self.plan["repo"], timeout=60, env=self.quiet_env())
		log("serve.py exit {}: {}".format(code, output))
		deadline = time.monotonic() + health_wait_seconds
		while time.monotonic() < deadline:
			if self.health():
				return True, output
			time.sleep(0.3)
		return False, output

	def quiet_env(self):
		env = dict(os.environ)
		env[updater.quiet_env] = "1"
		return env

	# --- the steps -----------------------------------------------------------------------------------------------

	def wait_for_lock(self):
		"""The viewer names this process in update.lock right after starting it; wait for that, then hold it."""
		deadline = time.monotonic() + lock_wait_seconds
		while time.monotonic() < deadline:
			record = updater.read_lock(self.paths)
			if record and record.get("pid") == os.getpid():
				return
			time.sleep(0.1)
		updater.hand_lock(self.paths, os.getpid(), self.plan["version"], self.plan["work"])

	def swap(self):
		copies = self.plan.get("copies", [])
		for index, item in enumerate(copies):
			self.status("running", step="replacing files ({}/{})".format(index + 1, len(copies)))
			updater.install_file(item["source"], item["target"])
		for item in self.plan.get("deletions", []):
			updater.remove_with_retry(item["target"])
		updater.clear_pycache(self.plan["cli_dir"])

	def validate(self):
		code, output = updater.run([self.plan["python"], os.path.join(self.plan["cli_dir"], "tickets.py"), "validate"],
			cwd=self.plan["repo"], timeout=300, env=self.quiet_env())
		log("validate exit {}:\n{}".format(code, output))
		return code == 0, output

	def rollback(self, step, detail):
		log("rolling back after {}: {}".format(step, detail))
		self.status("running", step="rolling back")
		if self.started_new and self.health():
			self.stop_viewer_by_record()
		problems = updater.restore(self.plan)
		updater.clear_pycache(self.plan["cli_dir"])
		restarted, output = self.start_viewer()
		if not restarted:
			problems.append("the old viewer did not come back: " + updater.ascii_text(updater.last_lines(output, 3)))
		self.status("rolled_back", step=step, detail=updater.ascii_text(updater.last_lines(detail, 15)),
			problems=problems, changed=[], finished=updater.utc_now())

	def stop_viewer_by_record(self):
		"""Stop a viewer this helper started (the new one, before a rollback), through serve.py --stop."""
		code, output = updater.run([self.plan["python"], os.path.join(self.plan["cli_dir"], "serve.py"), "--stop"],
			cwd=self.plan["repo"], timeout=60, env=self.quiet_env())
		log("serve.py --stop exit {}: {}".format(code, output))
		deadline = time.monotonic() + stop_wait_seconds
		while time.monotonic() < deadline and self.health() is not None:
			time.sleep(0.2)

	def run(self):
		self.wait_for_lock()
		try:
			self.status("running", step="stopping the viewer")
			problem = self.stop_viewer()
			if problem:
				# Nothing has changed yet; the old viewer may still be up.
				if self.health() is None:
					self.start_viewer()
				self.status("failed", step="stopping the viewer", detail=problem, changed=[], finished=updater.utc_now())
				return 1
			try:
				self.swap()
			except (OSError, KeyError, TypeError) as error:
				self.rollback("replacing files", "{}: {}".format(type(error).__name__, error))
				return 1
			self.status("running", step="validating")
			ok, output = self.validate()
			if not ok:
				self.rollback("validate", output)
				return 1
			self.status("running", step="restarting the viewer")
			self.started_new = True
			ok, output = self.start_viewer()
			if not ok:
				self.rollback("restarting the viewer", output)
				return 1
			self.status("running", step="staging")
			changed = [item["shown"] for item in self.plan.get("copies", [])] + ["{} (deleted)".format(item["shown"])
				for item in self.plan.get("deletions", [])] + [updater.readme_shown]
			try:
				actions = updater.stage(self.plan, log)
			except Exception as error:  # noqa: BLE001 - the update itself succeeded; staging is reported, not undone
				actions = ["staging failed ({}: {}); take the skill, hooks and rules doc from the release by hand".format(
					type(error).__name__, updater.ascii_text(error))]
			try:
				updater.update_readme(self.plan, actions, changed)
			except Exception as error:  # noqa: BLE001
				actions.append("writing .claude/tickets/README.md failed ({})".format(updater.ascii_text(error)))
			self.status("done", step="done", changed=changed, actions=actions, finished=updater.utc_now())
			return 0
		except Exception as error:  # noqa: BLE001 - never leave the lock held with no final state
			log("unexpected {}: {}".format(type(error).__name__, error))
			try:
				self.rollback("the helper", "{}: {}".format(type(error).__name__, error))
			except Exception as inner:  # noqa: BLE001
				self.status("failed", detail="the helper failed and could not roll back: {}".format(
					updater.ascii_text(inner)), finished=updater.utc_now())
			return 1
		finally:
			updater.release_lock(self.paths)


def main():
	if len(sys.argv) != 2:
		print("usage: updater_helper.py <plan.json>")
		return 2
	plan = updater.read_json_file(sys.argv[1])
	if not isinstance(plan, dict) or plan.get("format") != updater.plan_format:
		print("ERROR {} is not a plan this helper reads (format {})".format(updater.ascii_text(sys.argv[1]),
			updater.plan_format))
		return 2
	log("update {} -> {} (helper pid {})".format(plan.get("installed"), plan.get("version"), os.getpid()))
	return helper(plan).run()


if __name__ == "__main__":
	sys.exit(main())
