"""Read-only replay of rule A1's static layer (extract) over the project's transcripts.

Usage: python -X utf8 replay_a1.py [--sample N] [--misses]
The disk/git layer cannot be replayed (the old state is gone), so this measures how many tool calls
the rule would examine, by verb, and lets a human hand-check the parse. Writes nothing.
"""
import glob
import json
import os
import random
import re
import sys
import collections

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from rules.execute import a1_archive_before_replace as a1  # noqa: E402

D = r"C:\Users\ShadowEvil\.claude\projects\C--Users-ShadowEvil-source-Helbreath-Heldenian-Project"
ROOT = "C:/Users/ShadowEvil/source/Helbreath-Heldenian-Project"
SPR = re.compile(r"staged[/\\]+client[/\\]+sprites", re.I)
DESTR = re.compile(r"\b(rm|del|erase|Remove-Item|unlink|mv|Move-Item|git mv|git rm|cp|Copy-Item|copy)\b|>")


def calls():
    files = glob.glob(os.path.join(D, "*.jsonl")) + glob.glob(os.path.join(D, "*", "subagents", "*.jsonl"))
    for f in files:
        for ln in open(f, encoding="utf-8", errors="replace"):
            if "sprites" not in ln:
                continue
            try:
                o = json.loads(ln)
            except ValueError:
                continue
            cont = (o.get("message") or {}).get("content")
            if not isinstance(cont, list):
                continue
            for b in cont:
                if isinstance(b, dict) and b.get("type") == "tool_use":
                    yield os.path.basename(f), o.get("cwd") or ROOT, b["name"], b.get("input") or {}


def main():
    n = 10
    if "--sample" in sys.argv:
        n = int(sys.argv[sys.argv.index("--sample") + 1])
    total = hit_calls = 0
    by = collections.Counter()
    rows, misses, sess = [], [], set()
    for f, cwd, tool, tin in calls():
        total += 1
        c = a1.extract(tool, tin, cwd, ROOT)
        s = tin.get("command") or tin.get("file_path") or ""
        if c:
            hit_calls += 1
            sess.add(f)
            for x in c:
                by[(tool, x["verb"], x["src_hd"])] += 1
            rows.append((f, tool, s, c))
        elif tool in a1.SHELL_TOOLS and SPR.search(s) and DESTR.search(s):
            misses.append(s)
    arch_same = sum(1 for r in rows if "working/archive" in r[2].replace("\\", "/"))
    print("calls mentioning 'sprites': %d; extracted (would examine): %d in %d transcripts; "
          "same command also names working/archive: %d" % (total, hit_calls, len(sess), arch_same))
    for k, v in sorted(by.items(), key=lambda kv: -kv[1]):
        print("  %-10s %-9s src_hd=%-5s %d" % (k[0], k[1], k[2], v))
    random.seed(35)
    pick = random.sample(rows, min(n, len(rows)))
    print("--- sample of %d extracted calls ---" % len(pick))
    for f, tool, s, c in pick:
        print("* [%s] %s" % (tool, re.sub(r"\s+", " ", s)[:170]))
        print("    -> %s" % [(x["rel"][-48:], x["verb"], x["src_hd"]) for x in c][:4])
    if "--misses" in sys.argv:
        print("--- %d sprite-mentioning shell calls with a verb-ish word but nothing extracted ---" % len(misses))
        for s in random.sample(misses, min(n, len(misses))):
            print("- " + re.sub(r"\s+", " ", s)[:170])
    else:
        print("(unextracted shell calls with a verb word: %d; --misses lists a sample)" % len(misses))


if __name__ == "__main__":
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
    main()
