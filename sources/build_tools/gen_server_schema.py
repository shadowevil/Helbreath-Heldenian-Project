#!/usr/bin/env python
"""Write the server's generated schema headers before every server compile (Phase F).

    --schema-out FILE   schema_sql_generated.h: gamedata/mapinfo schema text + fingerprints, written by
                        sources/database/build_databases.py's own functions, so it is byte-identical to the
                        header build_databases.py writes. No database is built or touched.
    --account-out FILE  account_schema_generated.h: the frozen v6 account baseline schema
                        (sources/database/account/*.sql) and every account migration
                        (sources/database/account/migrations/NNNN_<name>.sql) with its release and sha256.

Each header is rewritten only when its content changes, so an unchanged source does not recompile.

Contract with build_databases.py (reviewed separately). This script uses exactly these of its names:
  functions  schema_parts, schema_text_of, fingerprint, check_schema_size, header_text, cpp_literal_lines,
             write_if_changed, normalize, read_text, read_sql_release
  constants  BUILD_KEYS, CHANNELS, SCHEMA_LIMIT
  exception  BuildError
Renaming or changing the signature of any of them breaks the server build.

Account rules (checked here, enforced again by the server at runtime):
  - sources/database/account/*.sql is the FROZEN v6 baseline: the structure every account DB had once the
    pre-Phase-F server had opened it. Its fingerprint is pinned below; a change to it fails the build.
    Structure changes go into a new migration instead. hb_migrations.sql (which a re-extract adds, since
    the server creates that table itself) is not part of the baseline.
  - Migrations are append-only: NNNN_<lowercase_name>.sql, numbered 0001, 0002, ... without gaps. The
    first line is `-- release: M.m.p[-alpha|-beta]`, at most the current sql_release. A migration runs
    inside the server's transaction, so it must not contain BEGIN/COMMIT, and PRAGMA foreign_keys has no
    effect there. Never edit a migration once it may have run anywhere: the server refuses a DB whose
    recorded hash differs from the file.

Usage:
    python gen_server_schema.py --environment DEV|TEST|PROD --schema-out FILE --account-out FILE
Exit status: 0 ok, 1 on any error.
"""

import argparse
import hashlib
import os
import re
import sys

sys.dont_write_bytecode = True   # IDE and CMake builds do not set PYTHONDONTWRITEBYTECODE

HERE = os.path.dirname(os.path.abspath(__file__))
DATABASE_DIR = os.path.normpath(os.path.join(HERE, "..", "database"))
sys.path.insert(0, DATABASE_DIR)

import build_databases as bd  # noqa: E402  (needs the path above)

ACCOUNT_KEY = "account"
MIGRATIONS_DIR = os.path.join(DATABASE_DIR, ACCOUNT_KEY, "migrations")
NOT_IN_BASELINE = {"hb_migrations.sql"}
# sha256 of sources/database/account/*.sql (names + LF-normalized text, build_databases.fingerprint)
# as extracted on 2026-11-23 from the DBs the pre-Phase-F server maintained: the v6 baseline.
V6_BASELINE_FINGERPRINT = "79e2de1f20e804ece2d322f9b78d000cd338aa163484fd69aa42010e85baf2b6"

MIGRATION_NAME = re.compile(r"(\d{4})_[a-z0-9_]+\.sql")
RELEASE_LINE = re.compile(r"-- release: (\d+)\.(\d+)\.(\d+)(-alpha|-beta)?")
MATURITY_RANK = {"-alpha": 0, "-beta": 1, None: 2}
# Statements that would end the server's transaction early. A trigger body's BEGIN ... END stays allowed;
# the server also checks after each migration that its transaction is still open.
TRANSACTION_CONTROL = re.compile(r"(?im)^\s*(BEGIN\s*(DEFERRED|IMMEDIATE|EXCLUSIVE)?\s*(TRANSACTION)?|"
	r"(COMMIT|ROLLBACK)(\s+TRANSACTION)?|END\s+TRANSACTION)\s*;")


class GenError(Exception):
	pass


def release_key(text):
	match = re.fullmatch(r"(\d+)\.(\d+)\.(\d+)(-alpha|-beta)?", text)
	if not match:
		raise GenError("not a release: %r" % text)
	return (int(match.group(1)), int(match.group(2)), int(match.group(3)), MATURITY_RANK[match.group(4)])


def literal_block(text, indent):
	if len(text.encode("utf-8")) > bd.SCHEMA_LIMIT:
		raise GenError("text of %d bytes is over the %d-byte string literal limit" % (len(text.encode("utf-8")),
			bd.SCHEMA_LIMIT))
	return "\n".join(indent + line for line in bd.cpp_literal_lines(text))


def baseline_parts():
	parts = [p for p in bd.schema_parts(ACCOUNT_KEY) if p[0] not in NOT_IN_BASELINE]
	found = bd.fingerprint(parts)
	if found != V6_BASELINE_FINGERPRINT:
		raise GenError("sources/database/account/*.sql changed (fingerprint %s, the frozen v6 baseline is %s). "
			"The account structure files are the frozen baseline every existing account DB is checked "
			"against; restore them and put the change in a new account/migrations/NNNN_<name>.sql instead."
			% (found, V6_BASELINE_FINGERPRINT))
	return parts


def read_migrations(sql_release):
	"""[(id, release, sha256, normalized text)] in order."""
	if not os.path.isdir(MIGRATIONS_DIR):
		raise GenError("migrations folder missing: " + MIGRATIONS_DIR)
	names = sorted(f for f in os.listdir(MIGRATIONS_DIR) if f.endswith(".sql"))
	out = []
	for expected, name in enumerate(names, start=1):
		match = MIGRATION_NAME.fullmatch(name)
		if not match:
			raise GenError("%s: a migration is named NNNN_<lowercase_name>.sql" % name)
		if int(match.group(1)) != expected:
			raise GenError("%s: migrations are numbered 0001, 0002, ... without gaps or repeats; expected %04d"
				% (name, expected))
		text = bd.normalize(bd.read_text(os.path.join(MIGRATIONS_DIR, name)))
		first = text.split("\n", 1)[0].rstrip()
		release_match = RELEASE_LINE.fullmatch(first)
		if not release_match:
			raise GenError("%s: the first line must be '-- release: M.m.p[-alpha|-beta]', not %r" % (name, first))
		release = first[len("-- release: "):]
		if release_key(release) > release_key(sql_release):
			raise GenError("%s: release %s is newer than the current sql_release %s (shared/version.h)"
				% (name, release, sql_release))
		if TRANSACTION_CONTROL.search(text):
			raise GenError("%s: a migration runs inside the server's transaction; remove its BEGIN/COMMIT/"
				"ROLLBACK/END TRANSACTION statement" % name)
		digest = hashlib.sha256(text.encode("utf-8")).hexdigest()
		out.append((name[:-len(".sql")], release, digest, text))
	return out


ACCOUNT_TEMPLATE = """// GENERATED FILE - do not edit and do not commit.
//
// Written by sources/build_tools/gen_server_schema.py. baseline_schema is sources/database/account/*.sql (the
// frozen v6 account baseline) in ordinal order; migrations are sources/database/account/migrations/*.sql in
// order, each with the release from its header line and the sha256 of its LF-normalized text.
#pragma once

#include <array>
#include <string_view>

namespace hb::server::account_schema
{{
	inline constexpr std::string_view baseline_schema =
{baseline};

	inline constexpr std::string_view baseline_fingerprint = "{fingerprint}";

	struct migration
	{{
		std::string_view id;           // file name without .sql, e.g. "0001_v6_baseline"
		std::string_view release;      // from the "-- release:" header line
		std::string_view content_hash; // sha256 hex of the LF-normalized file text
		std::string_view sql;          // that text
	}};

	inline constexpr std::array<migration, {count}> migrations{{{{
{entries}	}}}};
}} // namespace hb::server::account_schema
"""


def account_header(parts, migrations):
	entries = []
	for mid, release, digest, text in migrations:
		entries.append('\t\t{"%s", "%s", "%s",\n%s},\n' % (mid, release, digest, literal_block(text, "\t\t\t")))
	return ACCOUNT_TEMPLATE.format(baseline=literal_block(bd.schema_text_of({"parts": parts}), "\t\t"),
		fingerprint=bd.fingerprint(parts), count=len(migrations), entries="".join(entries))


def schema_header(channel):
	infos = {}
	for key in bd.BUILD_KEYS:
		parts = bd.schema_parts(key)
		infos[key] = {"key": key, "parts": parts, "fingerprint": bd.fingerprint(parts)}
		bd.check_schema_size(key, infos[key])
	return bd.header_text(infos, bd.read_sql_release(), channel)


def main():
	ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
	ap.add_argument("--environment", required=True, choices=bd.CHANNELS)
	ap.add_argument("--schema-out", required=True)
	ap.add_argument("--account-out", required=True)
	args = ap.parse_args()
	try:
		sql_release = bd.read_sql_release()
		schema_text = schema_header(args.environment)
		account_text = account_header(baseline_parts(), read_migrations(sql_release))
		for path, text in ((args.schema_out, schema_text), (args.account_out, account_text)):
			status = bd.write_if_changed(os.path.abspath(path), text)
			if status != "same":
				print("gen_server_schema: %s %s" % (status, path))
	except (GenError, bd.BuildError, OSError, UnicodeError) as error:
		print("gen_server_schema: ERROR: %s" % error, file=sys.stderr)
		return 1
	return 0


if __name__ == "__main__":
	sys.exit(main())
