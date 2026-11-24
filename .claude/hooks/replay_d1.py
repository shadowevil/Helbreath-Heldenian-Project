"""Replay rule D1 over a project's Claude Code transcripts (read-only; writes nothing).

usage: python replay_d1.py [--root=<project root>] [--transcripts=<dir>] [--sample=N] [--csv=<out>]

Pairs every Write/Edit/MultiEdit tool_use with its toolUseResult, rebuilds the file content after
the write from toolUseResult.originalFile (+ the edit), and calls the shipped d1.classify(). Sizes
are UTF-8 bytes of the transcript strings, which may lack the file's CR bytes: a hit within one byte
per line of the limit is marked `boundary` (could be a false positive or a missed hit on disk).
"""
import glob
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

from hooklib import output, paths  # noqa: E402
from rules.write import d1_doc_size as d1  # noqa: E402
import doc_size_guard  # noqa: E402

DEFAULT_TRANSCRIPTS = os.path.join(os.environ.get("USERPROFILE") or os.environ.get("HOME") or "",
                                   ".claude", "projects")


def transcript_dir_for(root):
    """Claude Code's folder name for a project: the path with ':', '\\', '/' and '.' as '-'."""
    name = paths.normalize(root)
    for ch in ":\\/._":
        name = name.replace(ch, "-")
    return os.path.join(DEFAULT_TRANSCRIPTS, name)


def after_content(name, tin, res):
    """The file text after the write, or None when the transcript cannot tell."""
    if name == "Write":
        c = tin.get("content")
        return c if isinstance(c, str) else None
    orig = res.get("originalFile")
    if not isinstance(orig, str):
        return None
    edits = tin.get("edits") if name == "MultiEdit" else [tin]
    text = orig
    for e in edits or []:
        old, new = e.get("old_string"), e.get("new_string")
        if not isinstance(old, str) or not isinstance(new, str):
            return None
        text = text.replace(old, new) if e.get("replace_all") else text.replace(old, new, 1)
    return text


def iter_pairs(tdir):
    """(file, tool_use_id, name, input, result, timestamp) for each paired file-write call."""
    for f in sorted(glob.glob(os.path.join(tdir, "**", "*.jsonl"), recursive=True)):
        uses = {}
        try:
            fh = open(f, encoding="utf-8", errors="replace")
        except OSError:
            continue
        with fh:
            for line in fh:
                if '"tool_use' not in line and '"toolUseResult"' not in line:
                    continue
                try:
                    d = json.loads(line)
                except ValueError:
                    continue
                content = (d.get("message") or {}).get("content")
                if not isinstance(content, list):
                    continue
                for blk in content:
                    if not isinstance(blk, dict):
                        continue
                    if blk.get("type") == "tool_use" and blk.get("name") in doc_size_guard.TOOLS + ("Read",):
                        uses[blk.get("id")] = (blk.get("name"), blk.get("input") or {})
                    elif blk.get("type") == "tool_result" and blk.get("tool_use_id") in uses:
                        res = d.get("toolUseResult")
                        name, tin = uses.pop(blk.get("tool_use_id"))
                        yield (f, blk.get("tool_use_id"), name, tin, res, d.get("timestamp") or "",
                               bool(blk.get("is_error")))


def main(argv):
    opts = dict(a[2:].split("=", 1) for a in argv if a.startswith("--") and "=" in a)
    root = opts.get("root") or os.environ.get("CLAUDE_PROJECT_DIR") or os.getcwd()
    tdir = opts.get("transcripts") or transcript_dir_for(root)
    sample = int(opts.get("sample", "15"))
    exempt = doc_size_guard.load_exempt()
    seen = set()
    n = {"file_writes": 0, "doc_writes": 0, "unknown_size": 0, "failed_calls": 0, "dupes": 0,
         "no_result": 0}
    hits = []
    events = []
    for ev in iter_pairs(tdir):
        if ev[1] in seen:
            n["dupes"] += 1
            continue
        seen.add(ev[1])
        events.append(ev)
    events.sort(key=lambda ev: ev[5])  # ISO timestamps: chain each doc's content in time order
    known = {}  # rel -> last known full text (from a full Read, a Write, or a judged Edit)
    n["reconstructed"] = 0
    for f, tid, name, tin, res, ts, is_error in events:
        rel = paths.rel_to_root(tin.get("file_path") or "", root)
        if name == "Read":
            fl = res.get("file") if isinstance(res, dict) else None
            if (d1.applies(rel) and isinstance(fl, dict) and isinstance(fl.get("content"), str)
                    and fl.get("startLine", 1) == 1 and fl.get("numLines", -1) >= fl.get("totalLines", 0)):
                known[rel.lower()] = fl["content"]
            continue
        n["file_writes"] += 1
        if not d1.applies(rel):
            continue
        n["doc_writes"] += 1
        if is_error:
            n["failed_calls"] += 1
            continue
        if not isinstance(res, dict):
            n["no_result"] += 1
            res = {}  # a Write's content is in its input; an Edit may be rebuilt from the chain
        orig = res.get("originalFile")
        reconstructed = False
        if name != "Write" and not isinstance(orig, str):
            base = known.get(rel.lower())
            edits = tin.get("edits") if name == "MultiEdit" else [tin]
            if base is not None and all(isinstance(e.get("old_string"), str) and e["old_string"] in base
                                        for e in edits or []):
                orig, reconstructed = base, True
                res = dict(res, originalFile=base)
        text = after_content(name, tin, res)
        if text is None:
            n["unknown_size"] += 1
            known.pop(rel.lower(), None)  # the chain is broken until the next full Read or Write
            continue
        known[rel.lower()] = text
        n["reconstructed"] += reconstructed
        size = len(text.encode("utf-8"))
        if res.get("type") == "create":
            prev = 0
        elif isinstance(orig, str):
            prev = len(orig.encode("utf-8"))
        else:
            prev = None
        hit = d1.classify(rel, size, prev, text[:d1.HEAD_BYTES], exempt)
        lines = text.count("\n")
        near = abs(size - d1.LIMIT) <= lines and "\r" not in text
        if hit is None:
            n.setdefault("near_miss_boundary", 0)
            n["near_miss_boundary"] += 1 if near and size <= d1.LIMIT else 0
            continue
        hit.update(boundary=near, ts=ts, reconstructed=reconstructed,
                   assessment=d1.assessment_line(text[:d1.HEAD_BYTES])[:140], tool=name, transcript=os.path.relpath(f, tdir),
                   would_flag=d1.should_flag(hit))
        hits.append(hit)
    by_path = {}
    for h in hits:
        by_path.setdefault(h["path"], []).append(h)
    flags = [h for h in hits if h["would_flag"]]
    print("transcripts: %s" % output.ascii_text(tdir))
    print("counts: " + ", ".join("%s=%d" % kv for kv in sorted(n.items())))
    print("D1 hits (doc writes leaving the doc > %d bytes): %d" % (d1.LIMIT, len(hits)))
    print("  crossed=%d  step_crossed=%d  already_over=%d  prev_unknown=%d  exempt=%d  assessed=%d  boundary=%d" % (
        sum(1 for h in hits if h["crossed"]), sum(1 for h in hits if h["step_crossed"] and not h["crossed"]),
        sum(1 for h in hits if h["crossed"] is False),
        sum(1 for h in hits if h["crossed"] is None), sum(1 for h in hits if h["exempt"]),
        sum(1 for h in hits if h["assessed"]), sum(1 for h in hits if h["boundary"])))
    print("  would flag in --flag mode: %d; after the (session, path, 2 KB bucket) dedupe: %d" % (
        len(flags), len({(h["transcript"].split(os.sep)[0].split(".")[0], h["path"], (h["size"] - 1) // d1.STEP)
                         for h in flags})))
    print("per path (hits / would_flag / max bytes):")
    for p, hs in sorted(by_path.items(), key=lambda kv: -len(kv[1])):
        print("  %-55s %4d %4d %7d" % (output.ascii_text(p), len(hs), sum(1 for h in hs if h["would_flag"]),
                                       max(h["size"] for h in hs)))
    step = max(1, len(hits) // sample) if hits else 1
    print("sample (every %dth hit):" % step)
    for h in hits[::step][:sample]:
        print("  %s %-9s %-36s %6s -> %6d crossed=%s exempt=%s assessed=%s boundary=%s recon=%s" % (
            h["ts"][:19], h["tool"], output.ascii_text(h["path"]), h["prev_size"], h["size"], h["crossed"],
            h["exempt"], h["assessed"], h["boundary"], h["reconstructed"]))
    if opts.get("csv"):
        with open(paths.normalize(opts["csv"]), "w", encoding="utf-8") as fh:
            json.dump(hits, fh, indent=1)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
