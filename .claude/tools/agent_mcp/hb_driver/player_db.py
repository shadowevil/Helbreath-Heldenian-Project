"""Read a character's position from hb_player, and write it under the test.md guard.

Access follows environment.md sect. 5.1 and db_schemas.py exactly: mysql.exe, credentials from the
`database` block of bin/server/worldserver.json (HB_DB_USER / HB_DB_PASSWORD override), and the
password handed to the CHILD process through MYSQL_PWD - never on a command line, never printed.

The one write is test.md's exception, and write_position() refuses unless EVERY condition holds:
1. the character was created by this tooling on this identity's account (identity file + a DB
   join on accounts.name), and it is not the user's account;
2. it is logged out: a logout_proof from server_logs.wait_logout_saved (save queued -> session
   closed -> persistence batch), no `player entered:` for it since, and the DB row equal to that
   logged save;
3. a db_snapshot.py set of hb_player exists first - taken here from `snapshot_label`, or named by
   `snapshot_set` and checked on disk;
4. the row is re-read after the UPDATE and must equal the target.
Only character_position (map_id, x, y, direction), one row, UPDATE only.
"""

import os
import re
import subprocess
import sys

from . import common
from . import server_logs

mysql_bin_dir = r"C:\Program Files\MySQL\MySQL Server 8.4\bin"
default_host = "127.0.0.1"
default_port = 3306
player_schema = "hb_player"
world_schema = "hb_world"
snapshot_timeout_s = 300.0
query_timeout_s = 60.0
set_name_pattern = re.compile(r"saved set: (\S+)")
label_pattern = re.compile(r"^[A-Za-z0-9_.-]+$")
# character_position.direction: the 8 facings, 1..8 (the world writes 1..8; 4 = south-east default).
direction_min = 1
direction_max = 8


def mysql_exe():
	path = os.path.join(mysql_bin_dir, "mysql.exe")
	if os.path.isfile(path):
		return path
	for folder in os.environ.get("PATH", "").split(os.pathsep):
		candidate = os.path.join(folder, "mysql.exe")
		if os.path.isfile(candidate):
			return candidate
	raise common.flow_error("mysql.exe not found in {} or on PATH".format(mysql_bin_dir))


class db_connection:
	"""Credentials resolved like db_schemas.get_db_connection. The password is held, never printed."""

	def __init__(self, config_path=None):
		config_path = config_path or common.world_config_path
		block = {}
		if os.path.isfile(config_path):
			block = common.read_server_config(config_path).get("database") or {}
		self.host = str(block.get("host") or default_host)
		self.port = int(block.get("port") or default_port)
		self.user = os.environ.get("HB_DB_USER") or block.get("user")
		self._password = os.environ.get("HB_DB_PASSWORD")
		if self._password is None and block.get("password") is not None:
			self._password = str(block["password"])
		if not self.user or self._password is None:
			raise common.flow_error("no MySQL credentials: set HB_DB_USER/HB_DB_PASSWORD or add a database block "
			                        "to {}".format(config_path))

	def query(self, sql, schema):
		"""Rows as lists of strings (mysql -N -B). Raises on a non-zero exit."""
		env = dict(os.environ, MYSQL_PWD=self._password)
		result = subprocess.run(
			[mysql_exe(), "--default-character-set=utf8mb4", "-h", self.host, "-P", str(self.port), "-u", self.user,
			 "-N", "-B", "--database=" + schema, "-e", sql],
			env=env, capture_output=True, text=True, timeout=query_timeout_s)
		if result.returncode != 0:
			raise common.flow_error("mysql failed on {}: {}".format(schema, result.stderr.strip()))
		return [line.split("\t") for line in result.stdout.splitlines() if line]


def _checked_name(name):
	if not re.fullmatch(r"[A-Za-z0-9]{1,10}", name or ""):
		raise common.flow_error("not a valid character name: {!r}".format(name))
	return name


def entered_world_ever(name):
	"""True when the world log holds a 'player entered' record for the character (the whole log, from its start)."""
	whole_log = server_logs.log_watch(common.world_log_path)
	whole_log.offset = 0
	return bool(server_logs.entered_since(whole_log, name))


def logout_proof_around(action, proof_file, name, timeout_s=server_logs.logout_save_wait_s, db_config=None):
	"""Run `action()` (a logout, switch or quit); with `proof_file`, wait for `name`'s logout save in the world
	log and write the proof there (under .claude/scratch/). The log watch is marked BEFORE the action, and the file
	and name are checked before it too, so a bad argument never costs a logout. Returns the action's result
	dict with `logout_proof` added (and `logout_proof_file`)."""
	watch = None
	if proof_file:
		if not name:
			raise common.flow_error("proof_file needs name (the character logging out)")
		common.require_scratch_path(proof_file, "proof file")
		watch = server_logs.log_watch(common.world_log_path)
	result = action()
	if proof_file:
		row = character_row(db_connection(db_config), name)
		if row is None:
			raise common.flow_error("no character {!r} in hb_player".format(name))
		if not server_logs.logout_save(watch.records(), row["id"]) and not entered_world_ever(name):
			# a character that never entered the world has no logout save to wait for (and no position to protect)
			result["logout_proof"] = None
			result["logout_proof_skipped"] = "{!r} never entered the world (no 'player entered' in the world log), so " \
			                                 "no logout save exists".format(name)
			return result
		proof = server_logs.wait_logout_saved(watch, row["id"], timeout_s)
		common.write_json_file(proof_file, proof)
		result["logout_proof"] = proof
		result["logout_proof_file"] = proof_file
	return result


def character_row(db, name):
	"""{id, account_id, account} for a character name, or None."""
	rows = db.query("SELECT c.id, c.account_id, a.name FROM characters c JOIN accounts a ON a.id = c.account_id "
	                "WHERE c.name = '{}'".format(_checked_name(name)), player_schema)
	if not rows:
		return None
	return {"id": int(rows[0][0]), "account_id": int(rows[0][1]), "account": rows[0][2]}


def map_id(db, map_ref):
	"""An hb_world map id from an id or an internal name."""
	if isinstance(map_ref, int) or str(map_ref).isdigit():
		rows = db.query("SELECT id, internal_name FROM maps WHERE id = {}".format(int(map_ref)), world_schema)
	else:
		if not re.fullmatch(r"[A-Za-z0-9_]{1,31}", str(map_ref)):
			raise common.flow_error("not a valid map name: {!r}".format(map_ref))
		rows = db.query("SELECT id, internal_name FROM maps WHERE internal_name = '{}'".format(map_ref), world_schema)
	if not rows:
		raise common.flow_error("no map {!r} in {}.maps".format(map_ref, world_schema))
	return int(rows[0][0]), rows[0][1]


def read_position(db, name):
	"""{character_id, map_id, map, x, y, direction} from hb_player.character_position."""
	row = character_row(db, name)
	if row is None:
		raise common.flow_error("no character named {!r} in {}".format(name, player_schema))
	rows = db.query("SELECT map_id, x, y, direction FROM character_position WHERE character_id = {}".format(row["id"]),
	                player_schema)
	if not rows:
		raise common.flow_error("character {} has no character_position row".format(row["id"]))
	map_number = int(rows[0][0])
	_, map_name = map_id(db, map_number)
	return {"character_id": row["id"], "account": row["account"], "map_id": map_number, "map": map_name,
	        "x": int(rows[0][1]), "y": int(rows[0][2]), "direction": int(rows[0][3])}


def take_snapshot(label, schemas=(player_schema,)):
	"""db_snapshot.py save. Returns the set name."""
	if not label_pattern.match(label):
		raise common.flow_error("snapshot label must be [A-Za-z0-9_.-]: {!r}".format(label))
	command = [sys.executable, common.db_snapshot_script, "save", "--label", label, "--schemas"] + list(schemas)
	result = subprocess.run(command, capture_output=True, text=True, timeout=snapshot_timeout_s)
	match = set_name_pattern.search(result.stdout or "")
	if result.returncode != 0 or not match:
		raise common.flow_error("db_snapshot.py save failed: {}".format((result.stderr or result.stdout).strip()))
	set_name = match.group(1)
	check_snapshot(set_name)
	return set_name


def check_snapshot(set_name):
	"""Raise unless .claude/.backups/<set>/mysql/hb_player.sql exists and is non-empty."""
	if not re.fullmatch(r"[A-Za-z0-9_.-]+", set_name or ""):
		raise common.flow_error("not a snapshot set name: {!r}".format(set_name))
	dump = os.path.join(common.backups_dir, set_name, "mysql", player_schema + ".sql")
	if not os.path.isfile(dump) or os.path.getsize(dump) == 0:
		raise common.flow_error("snapshot set {} has no {} dump at {}".format(set_name, player_schema, dump))
	return dump


def write_position(db, ident, name, map_ref, x, y, direction=None, logout_proof=None, snapshot_label=None,
                   snapshot_set=None, world_watch=None):
	"""Move a logged-out throwaway character. See the module docstring for every guard.

	`world_watch` (a server_logs.log_watch) defaults to one reopened at the proof's offset, so an entry
	after the logout is caught however much later the write happens.
	"""
	# 1. ours, and not the user's
	if not ident.created_character(name):
		raise common.flow_error("refused: {!r} was not created by this tooling on identity {}".format(name, ident.path))
	row = character_row(db, name)
	if row is None:
		raise common.flow_error("refused: no character {!r} in {}".format(name, player_schema))
	if row["account"] != ident.account or row["account"].lower() in ("shadowevil",):
		raise common.flow_error("refused: {!r} belongs to account {!r}, not the identity's {!r}".format(
			name, row["account"], ident.account))
	# 2. logged out, and the logout save is what the row holds
	if not logout_proof or logout_proof.get("character_id") != row["id"]:
		raise common.flow_error("refused: no logout_proof for character {} (server_logs.wait_logout_saved after "
		                        "the logout)".format(row["id"]))
	watch = world_watch
	if watch is None:
		watch = server_logs.log_watch(logout_proof["watch_path"])
		watch.offset = logout_proof["watch_offset"]
	reentered = [record for record, _ in server_logs.entered_since(watch, name)
	             if record["wall_ms"] > logout_proof["commit_wall_ms"]]
	if reentered:
		raise common.flow_error("refused: {!r} entered the world after its logout save: {}".format(
			name, reentered[-1]["line"]))
	current = read_position(db, name)
	# The world log names the map by id ('1'), the row holds an id too, and a caller's proof may carry the name
	# ('default'): both sides are resolved to the hb_world map id before they are compared.
	logged_map_id, _ = map_id(db, logout_proof["map"])
	logged = (logged_map_id, logout_proof["x"], logout_proof["y"], logout_proof["direction"])
	if (current["map_id"], current["x"], current["y"], current["direction"]) != logged:
		raise common.flow_error("refused: the row {} does not equal the logged logout save {}".format(
			(current["map"], current["x"], current["y"], current["direction"]),
			(logout_proof["map"], logout_proof["x"], logout_proof["y"], logout_proof["direction"])))
	# 3. a snapshot first
	if snapshot_set:
		check_snapshot(snapshot_set)
	elif snapshot_label:
		snapshot_set = take_snapshot(snapshot_label)
	else:
		raise common.flow_error("refused: pass snapshot_label (a db_snapshot.py set is taken) or snapshot_set")
	# the write
	target_map, target_name = map_id(db, map_ref)
	target_direction = current["direction"] if direction is None else int(direction)
	if not direction_min <= target_direction <= direction_max:
		raise common.flow_error("direction must be {}..{}".format(direction_min, direction_max))
	db.query("UPDATE character_position SET map_id = {}, x = {}, y = {}, direction = {} WHERE character_id = {}"
	         .format(target_map, int(x), int(y), target_direction, row["id"]), player_schema)
	# 4. re-read
	after = read_position(db, name)
	if (after["map_id"], after["x"], after["y"], after["direction"]) != (target_map, int(x), int(y), target_direction):
		raise common.flow_error("write did not land: re-read {} after writing {}".format(
			after, (target_name, x, y, target_direction)))
	return {"before": current, "after": after, "snapshot_set": snapshot_set, "logout_proof": logout_proof["lines"]}
