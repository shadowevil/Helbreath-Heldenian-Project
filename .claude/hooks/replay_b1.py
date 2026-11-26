"""Read-only replay of rule B1 over the project's transcripts.

Usage: python -X utf8 replay_b1.py [--show N]
Runs the shipped classify() on every Write/Edit/MultiEdit tool_use that touches a file under
sources/client/. A Write's original text is not in the tool_use (only in its result), so a Write is
classified with old = '' (an upper bound: a rewrite that kept an old violation would count).
Writes nothing.
"""
import glob
import json
import os
import sys
import collections

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from hooklib import paths  # noqa: E402
from rules.write import b1_backend_isolation as b1  # noqa: E402

D = r"C:\Users\ShadowEvil\.claude\projects\C--Users-ShadowEvil-source-Helbreath-Heldenian-Project"
ROOT = "C:/Users/ShadowEvil/source/Helbreath-Heldenian-Project"


def pairs(name, tin):
    if name == "Write":
        return [(tin.get("content") or "", "")]
    edits = tin.get("edits") if name == "MultiEdit" else [tin]
    return [(e.get("new_string") or "", e.get("old_string") or "") for e in edits or [] if isinstance(e, dict)]


def main():
    show = int(sys.argv[sys.argv.index("--show") + 1]) if "--show" in sys.argv else 8
    files = glob.glob(os.path.join(D, "*.jsonl")) + glob.glob(os.path.join(D, "*", "subagents", "*.jsonl"))
    n_files = n_cov = 0
    n_scan = n_sfml_word = 0
    hits = []
    by = collections.Counter()
    for f in files:
        n_files += 1
        for ln in open(f, encoding="utf-8", errors="replace"):
            if '"tool_use"' not in ln:
                continue
            try:
                o = json.loads(ln)
            except ValueError:
                continue
            cont = (o.get("message") or {}).get("content")
            if not isinstance(cont, list):
                continue
            for b in cont:
                if not (isinstance(b, dict) and b.get("type") == "tool_use"
                        and b.get("name") in ("Write", "Edit", "MultiEdit")):
                    continue
                tin = b.get("input") or {}
                rel = paths.rel_to_root(tin.get("file_path") or "", ROOT)
                n_scan += 1
                if not b1.applies(rel):
                    continue
                n_cov += 1
                ps = pairs(b["name"], tin)
                if any("sfml" in (n + o2).lower() or "sf::" in n for n, o2 in ps):
                    n_sfml_word += 1
                hit = b1.classify(rel, ps)
                if hit:
                    by[b["name"]] += 1
                    hits.append((os.path.basename(f), b["name"], hit))
    print("transcripts: %d; Write/Edit/MultiEdit calls: %d; on covered client files: %d" % (n_files, n_scan, n_cov))
    print("covered calls whose text mentions sfml or sf:: at all (incl. comments/strings): %d" % n_sfml_word)
    print("HITS: %d  by tool: %s  distinct transcripts: %d  distinct paths: %d" % (
        len(hits), dict(by), len({h[0] for h in hits}), len({h[2]["path"] for h in hits})))
    for f, tool, h in hits[:show]:
        print("  %s %s %s n=%d kinds=%s samples=%s" % (f[:12], tool, h["path"], h["n_added"], h["kinds"], h["samples"]))


if __name__ == "__main__":
    main()
