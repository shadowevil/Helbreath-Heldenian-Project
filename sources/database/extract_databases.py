#!/usr/bin/env python
"""Extract the server SQLite databases into the SQL source directory.

Layout written (relative to this script):
    <db>/<table>.sql         CREATE TABLE + its indexes/triggers, verbatim from sqlite_master
    <db>/<table>.data.sql    one INSERT per row, sorted by primary key
    <db>/_sqlite_sequence.data.sql   AUTOINCREMENT counters

Databases are opened read-only and immutable; this script never writes a .db.
Account databases are per-player runtime data: only their structure is
extracted, the data sidecars are written empty.

Usage:
    python extract_databases.py [--db gamedata|mapinfo|account|all] [--dry-run]
"""

import argparse
import glob
import os
import sqlite3
import sys
import urllib.parse

HERE = os.path.dirname(os.path.abspath(__file__))
SERVER_DIR = os.path.normpath(os.path.join(HERE, "..", "..", "game", "server"))

# name -> (path relative to game/server, sidecars left empty)
DB_DEFS = {
	"gamedata": ("gamedata.db", False),
	"mapinfo": ("mapinfo.db", False),
	"account": (os.path.join("accounts", "shadowevil.db"), True),
}

SEQUENCE_TABLE = "sqlite_sequence"
SEQUENCE_FILE = "_sqlite_sequence.data.sql"


def open_ro(path):
	"""Open a database read-only and immutable (never touches the file).

	immutable=1 ignores any write-ahead log, so a -wal file beside the database
	means the main file may be stale; refuse rather than extract old data.
	"""
	if os.path.exists(path + "-wal"):
		sys.exit("ERROR: %s-wal exists; the database has uncommitted WAL data and immutable "
			"mode would read stale rows. Stop the server and checkpoint it first." % path)
	uri = "file:" + urllib.parse.quote(path.replace("\\", "/"), safe="/:") + "?mode=ro&immutable=1"
	return sqlite3.connect(uri, uri=True)


def qident(name):
	return '"' + name.replace('"', '""') + '"'


def read_schema(conn):
	"""Return (tables, views).

	tables: list of dicts {name, sql, extras:[(type, name, sql)]} sorted by name.
	Auto-indexes (sql IS NULL) and sqlite_* internals are skipped.
	"""
	rows = conn.execute(
		"SELECT type, name, tbl_name, sql FROM sqlite_master "
		"WHERE sql IS NOT NULL AND name NOT LIKE 'sqlite\\_%' ESCAPE '\\' "
		"ORDER BY type, name").fetchall()
	tables = {}
	views = {}
	for typ, name, tbl, sql in rows:
		if typ == "table":
			tables[name] = {"name": name, "sql": sql, "extras": []}
		elif typ == "view":
			views[name] = {"name": name, "sql": sql, "extras": []}
	for typ, name, tbl, sql in rows:
		if typ in ("index", "trigger"):
			# A trigger on a view goes into that view's file.
			(tables.get(tbl) or views[tbl])["extras"].append((typ, name, sql))
	return [tables[k] for k in sorted(tables)], [views[k] for k in sorted(views)]


def schema_text(obj):
	parts = [obj["sql"]] + [sql for (_t, _n, sql) in obj["extras"]]
	return "\n\n".join(p + ";" for p in parts) + "\n"


def sql_literal(value):
	if value is None:
		return "NULL"
	if isinstance(value, int):
		return str(value)
	if isinstance(value, float):
		if value != value:
			raise ValueError("NaN cannot be represented")
		if value in (float("inf"), float("-inf")):
			return "9e999" if value > 0 else "-9e999"
		return repr(value)
	if isinstance(value, bytes):
		return "X'" + value.hex().upper() + "'"
	if isinstance(value, str):
		if "\x00" in value:
			raise ValueError("NUL character in text value")
		s = value.replace("'", "''")
		# Keep one row per line: newlines and CRs are rebuilt with char().
		s = s.replace("\r", "'||char(13)||'").replace("\n", "'||char(10)||'")
		return "'" + s + "'"
	raise TypeError("unsupported value type %r" % type(value))


def table_columns(conn, table):
	info = conn.execute("PRAGMA table_info(%s)" % qident(table)).fetchall()
	cols = [r[1] for r in info]
	pk = [r[1] for r in sorted((r for r in info if r[5]), key=lambda r: r[5])]
	return cols, pk


def order_clause(pk):
	if pk:
		return " ORDER BY " + ", ".join(qident(c) for c in pk)
	return " ORDER BY rowid"


def data_text(conn, table, notes):
	cols, pk = table_columns(conn, table)
	collist = ", ".join(qident(c) for c in cols)
	cur = conn.execute("SELECT %s FROM %s%s" % (collist, qident(table), order_clause(pk)))
	prefix = "INSERT INTO %s (%s) VALUES (" % (qident(table), collist)
	lines = []
	blob_cols = set()
	nl_text = False
	for row in cur:
		for i, v in enumerate(row):
			if isinstance(v, bytes):
				blob_cols.add(cols[i])
			elif isinstance(v, str) and ("\n" in v or "\r" in v):
				nl_text = True
		lines.append(prefix + ", ".join(sql_literal(v) for v in row) + ");")
	if blob_cols:
		notes.append("%s: blob columns %s" % (table, ", ".join(sorted(blob_cols))))
	if nl_text:
		notes.append("%s: text values containing CR/LF (written via char())" % table)
	return ("\n".join(lines) + "\n") if lines else "", len(lines)


def sequence_text(conn):
	rows = conn.execute("SELECT name, seq FROM sqlite_sequence ORDER BY name").fetchall()
	# sqlite_sequence has no unique key and loading AUTOINCREMENT rows already
	# creates entries, so the file clears the table first.
	lines = ["DELETE FROM sqlite_sequence;"]
	for name, seq in rows:
		lines.append("INSERT INTO sqlite_sequence (name, seq) VALUES (%s, %s);" % (sql_literal(name), sql_literal(seq)))
	return "\n".join(lines) + "\n", len(rows)


def has_sequence(conn):
	return conn.execute("SELECT 1 FROM sqlite_master WHERE name = ?", (SEQUENCE_TABLE,)).fetchone() is not None


def write_if_changed(path, text, dry_run):
	"""Return 'new', 'changed' or 'same'."""
	status = "new"
	if os.path.exists(path):
		with open(path, "r", encoding="utf-8", newline="") as f:
			status = "same" if f.read() == text else "changed"
	if status != "same" and not dry_run:
		os.makedirs(os.path.dirname(path), exist_ok=True)
		with open(path, "w", encoding="utf-8", newline="\n") as f:
			f.write(text)
	return status


def extract(key, dry_run):
	rel, empty_data = DB_DEFS[key]
	src = os.path.join(SERVER_DIR, rel)
	out_dir = os.path.join(HERE, key)
	if not os.path.isfile(src) or os.path.getsize(src) == 0:
		print("[%s] ERROR: source database missing or empty: %s" % (key, src))
		return False
	conn = open_ro(src)
	try:
		tables, views = read_schema(conn)
		names = [t["name"] for t in tables] + [v["name"] for v in views]
		lower = {}
		for n in names:
			lower.setdefault(n.lower(), []).append(n)
		clash = [v for v in lower.values() if len(v) > 1]
		if clash:
			print("[%s] ERROR: names that collide on a case-insensitive filesystem: %s" % (key, clash))
			return False

		notes = []
		expected = set()
		counts = {}
		stats = {"new": 0, "changed": 0, "same": 0}
		print("[%s] source: %s" % (key, rel.replace("\\", "/")))
		for obj in tables + views:
			name = obj["name"]
			is_view = obj in views
			sql_name = name + ".sql"
			expected.add(sql_name)
			stats[write_if_changed(os.path.join(out_dir, sql_name), schema_text(obj), dry_run)] += 1
			if is_view:
				continue
			cols, pk = table_columns(conn, name)
			if "WITHOUT ROWID" in obj["sql"].upper():
				notes.append("%s: WITHOUT ROWID" % name)
			if not pk:
				notes.append("%s: no primary key (ordered by rowid)" % name)
			data_name = name + ".data.sql"
			expected.add(data_name)
			if empty_data:
				text, n = "", 0
				counts[name] = conn.execute("SELECT COUNT(*) FROM %s" % qident(name)).fetchone()[0]
			else:
				text, n = data_text(conn, name, notes)
				counts[name] = n
			stats[write_if_changed(os.path.join(out_dir, data_name), text, dry_run)] += 1
		if has_sequence(conn):
			expected.add(SEQUENCE_FILE)
			if empty_data:
				text, n = "DELETE FROM sqlite_sequence;\n", 0
			else:
				text, n = sequence_text(conn)
			stats[write_if_changed(os.path.join(out_dir, SEQUENCE_FILE), text, dry_run)] += 1
			counts[SEQUENCE_TABLE] = n

		width = max(len(n) for n in counts)
		for name in sorted(counts):
			tag = " (source rows; sidecar left empty)" if empty_data and name != SEQUENCE_TABLE else ""
			print("  %-*s %6d%s" % (width, name, counts[name], tag))
		idx = sum(len(t["extras"]) for t in tables)
		print("[%s] tables=%d views=%d indexes/triggers=%d files: new=%d changed=%d same=%d%s" % (
			key, len(tables), len(views), idx, stats["new"], stats["changed"], stats["same"],
			" (dry run, nothing written)" if dry_run else ""))
		for n in notes:
			print("  note: " + n)
		if os.path.isdir(out_dir):
			stale = sorted(f for f in os.listdir(out_dir) if f.endswith(".sql") and f not in expected)
			for f in stale:
				print("  WARNING: stale file not produced by this extraction: %s/%s" % (key, f))
		if key == "account":
			ref = [(o["name"], schema_text(o)) for o in tables + views]
			others = sorted(p for p in glob.glob(os.path.join(SERVER_DIR, "accounts", "*.db"))
				if os.path.abspath(p) != os.path.abspath(src))
			for other in others:
				label = "accounts/" + os.path.basename(other)
				if os.path.getsize(other) == 0:
					print("  FAIL account schema cross-check vs %s: file is empty" % label)
					return False
				c2 = open_ro(other)
				try:
					t2, v2 = read_schema(c2)
				finally:
					c2.close()
				if ref != [(o["name"], schema_text(o)) for o in t2 + v2]:
					print("  FAIL account schema cross-check vs %s: schema DIFFERS" % label)
					return False
				print("  account schema cross-check vs %s: identical" % label)
	finally:
		conn.close()
	return True


def main():
	ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
	ap.add_argument("--db", default="all", choices=sorted(DB_DEFS) + ["all"])
	ap.add_argument("--dry-run", action="store_true", help="report only; write nothing")
	args = ap.parse_args()
	keys = sorted(DB_DEFS) if args.db == "all" else [args.db]
	ok = True
	for k in keys:
		ok = extract(k, args.dry_run) and ok
	return 0 if ok else 1


if __name__ == "__main__":
	sys.exit(main())
