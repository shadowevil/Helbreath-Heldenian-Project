"""Print the lines of covered client writes that mention sfml / sf:: (hand-check of B1 near misses)."""
import glob
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from hooklib import paths  # noqa: E402
from rules.write import b1_backend_isolation as b1  # noqa: E402

D = r"C:\Users\ShadowEvil\.claude\projects\C--Users-ShadowEvil-source-Helbreath-Heldenian-Project"
ROOT = "C:/Users/ShadowEvil/source/Helbreath-Heldenian-Project"
for f in glob.glob(os.path.join(D, "*.jsonl")) + glob.glob(os.path.join(D, "*", "subagents", "*.jsonl")):
    for ln in open(f, encoding="utf-8", errors="replace"):
        if '"tool_use"' not in ln or not re.search(r"sfml|sf::", ln, re.I):
            continue
        try:
            o = json.loads(ln)
        except ValueError:
            continue
        for b in (o.get("message") or {}).get("content") or []:
            if not (isinstance(b, dict) and b.get("type") == "tool_use" and b.get("name") in ("Write", "Edit", "MultiEdit")):
                continue
            tin = b.get("input") or {}
            rel = paths.rel_to_root(tin.get("file_path") or "", ROOT)
            if not b1.applies(rel):
                continue
            texts = [tin.get("content") or "", tin.get("new_string") or "", tin.get("old_string") or ""]
            for e in tin.get("edits") or []:
                texts += [e.get("new_string") or "", e.get("old_string") or ""]
            for t in texts:
                for l in t.split("\n"):
                    if re.search(r"sfml|sf::", l, re.I):
                        print(os.path.basename(f)[:8], rel, "|", l.strip()[:110].encode("ascii", "replace").decode())
