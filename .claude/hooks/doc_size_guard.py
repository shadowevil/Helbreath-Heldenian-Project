"""PostToolUse entry point for Write|Edit|MultiEdit: rule D1, a linked doc over 10 KB (see
rules/write/d1_doc_size.py and RULES.md).

Never blocks (PostToolUse cannot deny) and fails open: any error exits 0 silently.
Modes:
  default         LOG-ONLY: a hit appends one row to <root>/.claude/hooks/log/doc_size_hits.jsonl.
  --flag          also tells the agent (additionalContext), at most once per session and path.
  CLAUDE_DOC_SIZE_GUARD=log   kill switch: log-only whatever the flags say.
  --hits-path=P / CLAUDE_DOC_SIZE_GUARD_HITS_PATH=P   write rows elsewhere (probes, tests).
  --self-test     run every rule's cases plus an end-to-end run; never touches the real log.
"""
import json
import os
import sys

# python -I leaves the script's own directory off sys.path - put it back so hooklib/rules import
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

try:
    from hooklib import output, paths  # noqa: E402
    from rules.write import d1_doc_size as d1  # noqa: E402
except Exception:
    # fail open: a broken install must never break a tool call; --self-test stays loud
    if __name__ == "__main__" and "--self-test" not in sys.argv[1:]:
        sys.exit(0)
    raise

EXEMPT_FILE = os.path.join(HERE, "doc_size_exempt.txt")
HITS_ENV = "CLAUDE_DOC_SIZE_GUARD_HITS_PATH"
KILL_ENV = "CLAUDE_DOC_SIZE_GUARD"
TOOLS = ("Write", "Edit", "MultiEdit")


def load_exempt(path=EXEMPT_FILE):
    """Relative doc paths (one per line, '#' comments) that are documented exceptions."""
    try:
        with open(path, encoding="utf-8") as fh:
            return tuple(ln.strip().replace("\\", "/") for ln in fh
                         if ln.strip() and not ln.lstrip().startswith("#"))
    except OSError:
        return ()


def read_head(path):
    try:
        with open(path, "rb") as fh:
            return fh.read(d1.HEAD_BYTES)
    except OSError:
        return b""


def _crlf_bytes(s, crlf):
    """UTF-8 size of `s` as written to a CRLF file when the string itself carries bare LFs."""
    n = len(s.encode("utf-8"))
    return n + s.count("\n") if crlf and "\r" not in s else n


def prev_size_of(event, size, head_bytes):
    """The doc's size before this write, or None if unknown. Prefers tool_response.originalFile
    (present in Claude Code's Write/Edit results; null for a created file), corrected for CRLF when
    the file on disk uses CRLF but the string does not. Falls back to the Edit/MultiEdit string
    delta from the size on disk (originalFile is null when the result carries
    contentNotInModelContext: 32 of 59 doc Edits in this project's transcripts, 2026-11-23)."""
    crlf = b"\r\n" in head_bytes
    resp = event.get("tool_response")
    if isinstance(resp, dict):
        if resp.get("type") == "create":
            return 0
        orig = resp.get("originalFile")
        if isinstance(orig, str):
            return _crlf_bytes(orig, crlf)
    name = event.get("tool_name")
    tin = event.get("tool_input") or {}
    edits = tin.get("edits") if name == "MultiEdit" else ([tin] if name == "Edit" else None)
    if not edits:
        return None
    delta = 0
    for e in edits:
        old, new = e.get("old_string"), e.get("new_string")
        if not isinstance(old, str) or not isinstance(new, str) or e.get("replace_all"):
            return None  # replace_all: the replacement count is not known
        delta += _crlf_bytes(new, crlf) - _crlf_bytes(old, crlf)
    return max(0, size - delta)


def evaluate(event, root=None, exempt=None):
    """(hit or None, rel path or None) for one PostToolUse event. Pure apart from reading the doc."""
    if (event.get("tool_name") or "") not in TOOLS:
        return None, None
    fp = (event.get("tool_input") or {}).get("file_path") or ""
    rel = paths.rel_to_root(fp, root or paths.project_root(event))
    if not d1.applies(rel):
        return None, rel
    try:
        size = os.path.getsize(paths.normalize(fp))
    except OSError:
        return None, rel
    if size <= d1.LIMIT:
        return None, rel
    head = read_head(paths.normalize(fp))
    hit = d1.classify(rel, size, prev_size_of(event, size, head), head.decode("utf-8", "replace"),
                      load_exempt() if exempt is None else exempt)
    return hit, rel


def already_flagged(hits_path, session, rel, bucket):
    """True if this session was already told about this doc in this 2 KB size bucket."""
    for row in output.recent_rows(hits_path):
        if (row.get("flagged") and row.get("session") == session and row.get("path") == rel
                and row.get("bucket") == bucket):
            return True
    return False


def handle(event, argv, environ, root=None, exempt=None):
    """Run the hook for one event. Returns the stdout text (may be empty). Never raises."""
    try:
        hit, rel = evaluate(event, root, exempt)
        if not hit:
            return ""
        hits_path = None
        for a in argv:
            if a.startswith("--hits-path="):
                hits_path = a.split("=", 1)[1]
        root_dir = root or paths.project_root(event)
        hits_path = hits_path or environ.get(HITS_ENV) or os.path.join(
            root_dir, ".claude", "hooks", "log", "doc_size_hits.jsonl")
        flag_mode = "--flag" in argv and environ.get(KILL_ENV, "").strip().lower() != "log"
        session = event.get("session_id") or ""
        bucket = (hit["size"] - 1) // d1.STEP
        flag = (flag_mode and d1.should_flag(hit)
                and not already_flagged(hits_path, session, hit["path"], bucket))
        row = dict(hit, tool=event.get("tool_name"), session=session, bucket=bucket,
                   agent=event.get("agent_id") or "", ts=_now(), flagged=bool(flag),
                   would_flag=d1.should_flag(hit))
        output.append_row(hits_path, row)
        return output.post_tool_context(d1.message(hit)) if flag else ""
    except Exception:
        return ""


def _now():
    import time
    return time.strftime("%Y-%m-%dT%H:%M:%S")


def self_test():
    import shutil
    results = []
    from rules import all_rule_modules
    for mod in all_rule_modules():
        for name, ok, detail in mod.run_self_test():
            results.append(("%s %s" % (mod.RULE_ID, name), ok, detail))
    # end-to-end: a fake root inside the gitignored log folder, its own hits file
    base = os.path.join(HERE, "log", "selftest_d1")
    shutil.rmtree(base, ignore_errors=True)
    try:
        docs = os.path.join(base, ".claude", "docs")
        os.makedirs(docs)
        big = os.path.join(docs, "big.md")
        with open(big, "wb") as fh:
            fh.write(b"# Big\r\n" + b"x" * 11000 + b"\r\n")
        small = os.path.join(docs, "small.md")
        with open(small, "wb") as fh:
            fh.write(b"# Small\n")
        hits = os.path.join(base, "hits.jsonl")
        env = {}
        orig_lf = "# Big\n" + "x" * 9000 + "\n"
        ev = {"tool_name": "Edit", "session_id": "s1", "cwd": base,
              "tool_input": {"file_path": big.replace("/", "\\")},
              "tool_response": {"originalFile": orig_lf}}
        argv = ["--flag", "--hits-path=" + hits]
        out1 = handle(ev, argv, env, root=base, exempt=())
        out2 = handle(ev, argv, env, root=base, exempt=())
        rows = output.recent_rows(hits)
        results.append(("e2e flag once", '"additionalContext"' in out1 and out2 == "", repr((out1, out2))))
        results.append(("e2e two rows logged", len(rows) == 2 and rows[0]["flagged"] and not rows[1]["flagged"],
                        repr(rows)))
        want_prev = len(orig_lf.encode()) + orig_lf.count("\n")
        results.append(("e2e CRLF-corrected prev_size", rows and rows[0]["prev_size"] == want_prev
                        and rows[0]["crossed"] is True, repr(rows[:1])))
        out3 = handle(dict(ev, session_id="s2"), ["--hits-path=" + hits], env, root=base, exempt=())
        results.append(("e2e log-only prints nothing", out3 == "" and len(output.recent_rows(hits)) == 3, out3))
        out4 = handle(dict(ev, session_id="s3"), argv, {KILL_ENV: "log"}, root=base, exempt=())
        results.append(("e2e kill switch", out4 == "", out4))
        # originalFile null (contentNotInModelContext): prev_size from the Edit string delta
        size_now = os.path.getsize(big)
        ev_delta = dict(ev, session_id="s4", tool_response={"originalFile": None},
                        tool_input={"file_path": big, "old_string": "y", "new_string": "x" * 3000})
        handle(ev_delta, argv, env, root=base, exempt=())
        r4 = output.recent_rows(hits)[-1]
        results.append(("e2e delta prev_size", r4["prev_size"] == size_now - 2999 and r4["crossed"] is True,
                        repr(r4)))
        ev_all = dict(ev_delta, session_id="s5", tool_input=dict(ev_delta["tool_input"], replace_all=True))
        handle(ev_all, argv, env, root=base, exempt=())
        r5 = output.recent_rows(hits)[-1]
        results.append(("e2e replace_all prev unknown", r5["prev_size"] is None and r5["crossed"] is None,
                        repr(r5)))
        n_before_small = len(output.recent_rows(hits))
        ev_small = dict(ev, tool_input={"file_path": small})
        results.append(("e2e small doc silent", handle(ev_small, argv, env, root=base, exempt=()) == ""
                        and len(output.recent_rows(hits)) == n_before_small, ""))
        ev_out = dict(ev, tool_input={"file_path": big}, cwd=docs)
        results.append(("e2e outside docs root silent",
                        handle(ev_out, argv, env, root=docs, exempt=()) == "", ""))
        results.append(("e2e garbage event fails open", handle({"tool_name": "Edit", "tool_input": None},
                                                               argv, env) == "", ""))
        results.append(("exempt file parses", isinstance(load_exempt(), tuple), ""))
    finally:
        shutil.rmtree(base, ignore_errors=True)
    bad = [r for r in results if not r[1]]
    for name, ok, detail in results:
        print(output.ascii_text("%s  %s%s" % ("PASS" if ok else "FAIL", name, ("  " + detail) if detail and not ok else "")))
    print("self-test: %d/%d passed" % (len(results) - len(bad), len(results)))
    return 0 if not bad else 1


def main():
    argv = sys.argv[1:]
    if "--self-test" in argv:
        return self_test()
    try:
        event = json.loads(sys.stdin.buffer.read().decode("utf-8", "replace") or "{}")
    except Exception:
        return 0
    if not isinstance(event, dict):
        return 0
    out = handle(event, argv, os.environ)
    if out:
        sys.stdout.write(out)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except SystemExit:
        raise
    except Exception:
        sys.exit(0)
