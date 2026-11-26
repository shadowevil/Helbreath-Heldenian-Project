"""PostToolUse entry point for Write|Edit|MultiEdit: rule B1, client code does not call a graphics
backend's own API (see rules/write/b1_backend_isolation.py and RULES.md).

Never blocks (PostToolUse cannot deny) and fails open: any error exits 0 silently.
  default         LOG-ONLY: a hit appends one row to <root>/.claude/hooks/log/backend_hits.jsonl.
  --flag          also tells the agent (additionalContext), once per session and path.
  CLAUDE_BACKEND_GUARD=log   kill switch: log-only whatever the flags say.
  --hits-path=P / CLAUDE_BACKEND_GUARD_HITS_PATH=P   write rows elsewhere (probes, tests).
  --self-test     run every rule's cases plus an end-to-end run; never touches the real log.
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

try:
    from hooklib import output, paths  # noqa: E402
    from rules.write import b1_backend_isolation as b1  # noqa: E402
except Exception:
    if __name__ == "__main__" and "--self-test" not in sys.argv[1:]:
        sys.exit(0)
    raise

HITS_ENV = "CLAUDE_BACKEND_GUARD_HITS_PATH"
KILL_ENV = "CLAUDE_BACKEND_GUARD"
TOOLS = ("Write", "Edit", "MultiEdit")


def pairs_of(event):
    """[(new_text, old_text)] for a Write/Edit/MultiEdit event. A Write's old text is the result's
    originalFile when present (so a rewrite that keeps an old violation is not a new one)."""
    name = event.get("tool_name")
    tin = event.get("tool_input") or {}
    if name == "Write":
        resp = event.get("tool_response")
        orig = resp.get("originalFile") if isinstance(resp, dict) else None
        return [(tin.get("content") or "", orig if isinstance(orig, str) else "")]
    edits = tin.get("edits") if name == "MultiEdit" else [tin]
    out = []
    for e in edits or []:
        if isinstance(e, dict):
            out.append((e.get("new_string") or "", e.get("old_string") or ""))
    return out


def evaluate(event, root=None):
    """(hit or None) for one PostToolUse event. Pure: reads nothing from disk."""
    if (event.get("tool_name") or "") not in TOOLS:
        return None
    fp = (event.get("tool_input") or {}).get("file_path") or ""
    rel = paths.rel_to_root(fp, root or paths.project_root(event))
    if not b1.applies(rel):
        return None
    return b1.classify(rel, pairs_of(event))


def already_flagged(hits_path, session, rel):
    for row in output.recent_rows(hits_path):
        if row.get("flagged") and row.get("session") == session and row.get("path") == rel:
            return True
    return False


def handle(event, argv, environ, root=None):
    """Run the hook for one event. Returns the stdout text (may be empty). Never raises."""
    try:
        hit = evaluate(event, root)
        if not hit:
            return ""
        hits_path = None
        for a in argv:
            if a.startswith("--hits-path="):
                hits_path = a.split("=", 1)[1]
        root_dir = root or paths.project_root(event)
        hits_path = hits_path or environ.get(HITS_ENV) or os.path.join(
            root_dir, ".claude", "hooks", "log", "backend_hits.jsonl")
        session = event.get("session_id") or ""
        flag_mode = "--flag" in argv and environ.get(KILL_ENV, "").strip().lower() != "log"
        flag = flag_mode and not already_flagged(hits_path, session, hit["path"])
        row = dict(hit, tool=event.get("tool_name"), session=session,
                   agent=event.get("agent_id") or "", ts=_now(), flagged=bool(flag))
        output.append_row(hits_path, row)
        return output.post_tool_context(b1.message(hit)) if flag else ""
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
        if mod.RULE_ID != b1.RULE_ID:
            continue  # the other rules' entry points run their own cases
        for name, ok, detail in mod.run_self_test():
            results.append(("%s %s" % (mod.RULE_ID, name), ok, detail))
    base = os.path.join(HERE, "log", "selftest_b1")
    shutil.rmtree(base, ignore_errors=True)
    try:
        os.makedirs(base)
        hits = os.path.join(base, "hits.jsonl")
        f = base.replace("\\", "/") + "/sources/client/game.cpp"
        ev = {"tool_name": "Edit", "session_id": "s1", "cwd": base,
              "tool_input": {"file_path": f, "old_string": "int a;", "new_string": "sf::Clock a;"}}
        argv = ["--flag", "--hits-path=" + hits]
        out1 = handle(ev, argv, {}, root=base)
        out2 = handle(ev, argv, {}, root=base)
        rows = output.recent_rows(hits)
        results.append(("e2e flag once", '"additionalContext"' in out1 and out2 == "", repr((out1, out2))))
        results.append(("e2e two rows, first flagged", len(rows) == 2 and rows[0]["flagged"]
                        and not rows[1]["flagged"] and rows[0]["rule"] == "B1", repr(rows)))
        out3 = handle(dict(ev, session_id="s2"), ["--hits-path=" + hits], {}, root=base)
        results.append(("e2e log-only prints nothing", out3 == "" and len(output.recent_rows(hits)) == 3, out3))
        out4 = handle(dict(ev, session_id="s3"), argv, {KILL_ENV: "log"}, root=base)
        results.append(("e2e kill switch", out4 == "", out4))
        n = len(output.recent_rows(hits))
        clean = dict(ev, tool_input={"file_path": f, "old_string": "a", "new_string": "b"})
        results.append(("e2e clean edit silent", handle(clean, argv, {}, root=base) == ""
                        and len(output.recent_rows(hits)) == n, ""))
        mv = {"tool_name": "MultiEdit", "session_id": "s9", "cwd": base,
              "tool_input": {"file_path": f, "edits": [
                  {"old_string": "x", "new_string": "y"},
                  {"old_string": "p", "new_string": '#include <SFML/Window.hpp>'}]}}
        handle(mv, argv, {}, root=base)
        results.append(("e2e multiedit hit", output.recent_rows(hits)[-1]["kinds"] == ["include"], ""))
        wr = {"tool_name": "Write", "session_id": "s8", "cwd": base,
              "tool_input": {"file_path": f, "content": "sf::Clock a;\nint b;\n"},
              "tool_response": {"originalFile": "sf::Clock a;\nint c;\n"}}
        n = len(output.recent_rows(hits))
        handle(wr, argv, {}, root=base)
        results.append(("e2e write keeping old violation silent", len(output.recent_rows(hits)) == n, ""))
        wr2 = dict(wr, session_id="s7", tool_response={"type": "create", "originalFile": None})
        handle(wr2, argv, {}, root=base)
        results.append(("e2e new write hits", len(output.recent_rows(hits)) == n + 1, ""))
        other = dict(ev, tool_input={"file_path": base.replace("\\", "/") + "/sources/sfml_engine/a.cpp",
                                     "old_string": "x", "new_string": "sf::Clock c;"})
        n = len(output.recent_rows(hits))
        results.append(("e2e backend folder silent", handle(other, argv, {}, root=base) == ""
                        and len(output.recent_rows(hits)) == n, ""))
        results.append(("e2e garbage event fails open",
                        handle({"tool_name": "Edit", "tool_input": None}, argv, {}) == ""
                        and handle({"tool_name": "Write", "tool_input": {"file_path": f, "content": 5}},
                                   argv, {}, root=base) == "", ""))
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
