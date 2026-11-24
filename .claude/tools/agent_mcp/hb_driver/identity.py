"""Throwaway test identities: an account, its password and the characters made on it.

The identity lives in a caller-given JSON file under .claude/scratch/ (gitignored), because a password is
needed again at every login and must never reach a tracked path or the console. `public()` is the
only form any flow or the CLI prints. `created` flags record what THIS tooling made, which is the
proof write_position() demands before touching a row.

Name rules are the login server's (accounts.cpp is_valid_name / is_valid_password, protocol.h):
account and character names 3..10 of [A-Za-z0-9], passwords 4..10 printable ASCII.
"""

import os
import secrets
import string
import time

from . import common

name_min = 3
name_max = 10
password_length = 10
default_account_prefix = "hbd"
default_character_prefix = "Hbd"
name_alphabet = string.ascii_lowercase + string.digits
# Letters and digits only: printable, and nothing a shell or a log could mangle.
password_alphabet = string.ascii_letters + string.digits
# Never generated, never touched (the user's own account).
reserved_names = {"shadowevil"}


name_characters = set(string.ascii_letters + string.digits)


def valid_name(name):
	return (name_min <= len(name) <= name_max and set(name) <= name_characters
	        and name.lower() not in reserved_names)


def random_name(prefix, length=name_max):
	if not set(prefix) <= name_characters:
		raise common.flow_error("name prefix '{}' is not [A-Za-z0-9]".format(prefix))
	if len(prefix) >= length:
		raise common.flow_error("name prefix '{}' leaves no room in {} characters".format(prefix, length))
	return prefix + "".join(secrets.choice(name_alphabet) for _ in range(length - len(prefix)))


def random_password():
	return "".join(secrets.choice(password_alphabet) for _ in range(password_length))


class identity:
	"""An account + password + characters, backed by one JSON file."""

	def __init__(self, path, data):
		self.path = path
		self.data = data

	@property
	def account(self):
		return self.data["account"]

	@property
	def password(self):
		return self.data["password"]

	@property
	def email(self):
		return self.data.get("email", "")

	@property
	def characters(self):
		return self.data.setdefault("characters", [])

	def character(self, name):
		for entry in self.characters:
			if entry["name"] == name:
				return entry
		return None

	def add_character(self, name=None, prefix=default_character_prefix):
		"""Reserve a character name in the file (created=False until create_character succeeds)."""
		name = name or random_name(prefix)
		if not valid_name(name):
			raise common.flow_error("character name '{}' must be {}..{} of [A-Za-z0-9]".format(name, name_min, name_max))
		if self.character(name) is None:
			self.characters.append({"name": name, "created": False})
			self.save()
		return self.character(name)

	def mark(self, **fields):
		self.data.update(fields)
		self.save()

	def mark_character(self, name, **fields):
		entry = self.character(name) or self.add_character(name)
		entry.update(fields)
		self.save()
		return entry

	def created_character(self, name):
		"""True only when THIS tooling created the character on THIS identity's account."""
		entry = self.character(name)
		return bool(self.data.get("account_created") and entry and entry.get("created"))

	def public(self):
		"""Everything but the password - the only form that is ever printed."""
		shown = {key: value for key, value in self.data.items() if key != "password"}
		shown["password"] = "*" * len(self.password)
		shown["path"] = self.path
		return shown

	def save(self):
		common.write_json_file(self.path, self.data)


def new_identity(path, account_prefix=default_account_prefix, character_prefix=default_character_prefix,
                 characters=1, email="", overwrite=False):
	"""Generate an account, password and `characters` character names; write them to `path`."""
	common.require_scratch_path(path, "identity file")
	if os.path.exists(path) and not overwrite:
		raise common.flow_error("identity file already exists (load it, or pass overwrite): {}".format(path))
	data = {"account": random_name(account_prefix), "password": random_password(), "email": email,
	        "account_created": False, "characters": [], "generated_at": time.strftime("%Y-%m-%dT%H:%M:%S")}
	made = identity(path, data)
	for _ in range(characters):
		made.add_character(prefix=character_prefix)
	made.save()
	return made


def load_identity(path):
	"""`path` may be relative: it resolves against the repo root (HB-0625), as the CLI's --identity does."""
	path = str(path)
	if not os.path.isabs(path):
		path = os.path.normpath(os.path.join(common.repo_root, path))
	common.require_scratch_path(path, "identity file")
	if not os.path.isfile(path):
		raise common.flow_error("no identity file at {}".format(path))
	data = common.read_json_file(path)
	for key in ("account", "password"):
		if key not in data:
			raise common.flow_error("identity file {} has no '{}'".format(path, key))
	return identity(path, data)


def delete_identity(path):
	"""Remove the identity file (the password with it). The account row stays in hb_player: there is
	no account-delete path in the client or the login server."""
	common.require_scratch_path(path, "identity file")
	if os.path.isfile(path):
		os.remove(path)
		return True
	return False
