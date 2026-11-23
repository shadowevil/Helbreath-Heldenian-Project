#!/usr/bin/env python
"""Rebuild each database in memory from the SQL directory and compare it to the original.

Per table: the sqlite_master sql text (table + indexes/triggers), the row count
and a SHA-256 over the rows ordered by primary key. The originals are opened
read-only and immutable.

Account databases are compared on structure only (their sidecars are empty by
design); the sidecars are additionally required to be empty.

Usage:
    python verify_roundtrip.py [--db gamedata|mapinfo|account|all]
Exit status is non-zero when any line is FAIL.
"""

import argparse
import hashlib
import os
import sqlite3
import sys

import extract_databases as ex


def load_directory(key):
	"""Build an in-memory DB from <key>/. Returns (conn, data_files_nonempty)."""
	d = os.path.join(ex.HERE, key)
	files = sorted(os.listdir(d))
	conn = sqlite3.connect(":memory:")
	nonempty_data = []
	for f in files:
		if f.endswith(".sql") and not f.endswith(".data.sql"):
			with open(os.path.join(d, f), "r", encoding="utf-8", newline="") as fh:
				conn.executescript(fh.read())
	for f in files:
		if f.endswith(".data.sql") and f != ex.SEQUENCE_FILE:
			with open(os.path.join(d, f), "r", encoding="utf-8", newline="") as fh:
				text = fh.read()
			if text:
				nonempty_data.append(f)
			conn.executescript(text)
	seq = os.path.join(d, ex.SEQUENCE_FILE)
	if os.path.isfile(seq):
		with open(seq, "r", encoding="utf-8", newline="") as fh:
			conn.executescript(fh.read())
	conn.commit()
	return conn, nonempty_data


def master_by_table(conn):
	"""{tbl_name: {(type, name): sql}} for objects that have sql, excluding sqlite_sequence."""
	out = {}
	for typ, name, tbl, sql in conn.execute(
			"SELECT type, name, tbl_name, sql FROM sqlite_master WHERE sql IS NOT NULL"):
		if tbl == ex.SEQUENCE_TABLE:
			continue
		out.setdefault(tbl, {})[(typ, name)] = sql
	return out


def content_hash(conn, table):
	cols, pk = ex.table_columns(conn, table)
	collist = ", ".join(ex.qident(c) for c in cols)
	cur = conn.execute("SELECT %s FROM %s%s" % (collist, ex.qident(table), ex.order_clause(pk)))
	h = hashlib.sha256()
	n = 0
	for row in cur:
		h.update(repr(row).encode("utf-8", "surrogatepass"))
		h.update(b"\n")
		n += 1
	return n, h.hexdigest()


def verify(key):
	rel, schema_only = ex.DB_DEFS[key]
	src = os.path.join(ex.SERVER_DIR, rel)
	print("[%s] %s%s" % (key, rel.replace("\\", "/"), " (schema only)" if schema_only else ""))
	orig = ex.open_ro(src)
	try:
		built, nonempty = load_directory(key)
		try:
			m_orig = master_by_table(orig)
			m_built = master_by_table(built)
			fails = 0
			rows = 0
			for name in sorted(set(m_orig) | set(m_built)):
				problems = []
				if name not in m_built:
					problems.append("missing from directory")
				elif name not in m_orig:
					problems.append("not in original")
				else:
					if m_orig[name] != m_built[name]:
						problems.append("sqlite_master sql differs")
					if schema_only:
						if nonempty and (name + ".data.sql") in nonempty:
							problems.append("sidecar is not empty")
						n_b = built.execute("SELECT COUNT(*) FROM %s" % ex.qident(name)).fetchone()[0]
						if n_b:
							problems.append("rebuilt table has %d rows" % n_b)
						detail = "schema only"
					else:
						n_o, h_o = content_hash(orig, name)
						n_b, h_b = content_hash(built, name)
						if n_o != n_b:
							problems.append("row count %d != %d" % (n_o, n_b))
						if h_o != h_b:
							problems.append("content hash differs")
						rows += n_o
						detail = "rows=%d sha256=%s" % (n_o, h_o[:16])
				if problems:
					fails += 1
					print("  FAIL %s: %s" % (name, "; ".join(problems)))
				else:
					print("  PASS %s %s" % (name, detail))
			# sqlite_sequence
			if schema_only:
				ok = built.execute("SELECT COUNT(*) FROM sqlite_sequence").fetchone()[0] == 0 \
					if ex.has_sequence(built) else True
				print("  %s %s schema only (rebuilt counters empty)" % ("PASS" if ok else "FAIL", ex.SEQUENCE_TABLE))
				fails += 0 if ok else 1
			elif ex.has_sequence(orig) or ex.has_sequence(built):
				a = orig.execute("SELECT name, seq FROM sqlite_sequence ORDER BY name").fetchall() if ex.has_sequence(orig) else None
				b = built.execute("SELECT name, seq FROM sqlite_sequence ORDER BY name").fetchall() if ex.has_sequence(built) else None
				ok = a == b
				print("  %s %s rows=%s" % ("PASS" if ok else "FAIL", ex.SEQUENCE_TABLE, len(a) if a is not None else "none"))
				fails += 0 if ok else 1
			print("[%s] tables=%d failures=%d" % (key, len(set(m_orig) | set(m_built)), fails))
			return fails
		finally:
			built.close()
	finally:
		orig.close()


def main():
	ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
	ap.add_argument("--db", default="all", choices=sorted(ex.DB_DEFS) + ["all"])
	args = ap.parse_args()
	keys = sorted(ex.DB_DEFS) if args.db == "all" else [args.db]
	total = sum(verify(k) for k in keys)
	print("RESULT: %s (%d failing)" % ("PASS" if total == 0 else "FAIL", total))
	return 0 if total == 0 else 1


if __name__ == "__main__":
	sys.exit(main())
