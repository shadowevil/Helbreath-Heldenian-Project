"""PostToolUse entry point for Bash|PowerShell|Write|Edit|MultiEdit: rule A1, archive before replace
(see rules/execute/a1_archive_before_replace.py, hooklib/gitstate.py and RULES.md).

Never blocks (PostToolUse cannot deny) and fails open: any error exits 0 silently.
  default         LOG-ONLY: a hit appends one row to <root>/.claude/hooks/log/archive_hits.jsonl.
  --flag          also tells the agent (additionalContext), once per session and file set.
  CLAUDE_ARCHIVE_GUARD=log   kill switch: log-only whatever the flags say.
  --hits-path=P / CLAUDE_ARCHIVE_GUARD_HITS_PATH=P   write rows elsewhere (probes, tests).
  --self-test     run every rule's cases plus an end-to-end run in a throwaway git repo.
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

try:
    from hooklib import gitstate, output, paths  # noqa: E402
    from rules.execute import a1_archive_before_replace as a1  # noqa: E402
except Exception:
    if __name__ == "__main__" and "--self-test" not in sys.argv[1:]:
        sys.exit(0)
    raise

HITS_ENV = "CLAUDE_ARCHIVE_GUARD_HITS_PATH"
KILL_ENV = "CLAUDE_ARCHIVE_GUARD"


def evaluate(event, root=None):
    """The hit for one PostToolUse event, or None. Reads the disk and git; never writes."""
    tool = event.get("tool_name") or ""
    if tool not in a1.EDIT_TOOLS + a1.SHELL_TOOLS:
        return None
    root = root or paths.project_root(event)
    cands = a1.extract(tool, event.get("tool_input"), event.get("cwd") or root, root)
    if not cands:
        return None
    ents, truncated = gitstate.inspect(root, sorted({c["rel"] for c in cands}))
    hit = a1.judge(ents, src_hd=any(c["src_hd"] for c in cands))
    if hit:
        hit["verbs"] = sorted({c["verb"] for c in cands})
        hit["truncated"] = bool(truncated)  # the file list was cut at gitstate.MAX_ENTRIES
    return hit


def already_flagged(hits_path, session, key):
    for row in output.recent_rows(hits_path):
        if row.get("flagged") and row.get("session") == session and row.get("key") == key:
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
            root_dir, ".claude", "hooks", "log", "archive_hits.jsonl")
        session = event.get("session_id") or ""
        key = "|".join(hit["files"][:3])
        flag_mode = "--flag" in argv and environ.get(KILL_ENV, "").strip().lower() != "log"
        flag = flag_mode and not already_flagged(hits_path, session, key)
        row = dict(hit, tool=event.get("tool_name"), session=session, key=key,
                   agent=event.get("agent_id") or "", ts=_now(), flagged=bool(flag))
        output.append_row(hits_path, row)
        return output.post_tool_context(a1.message(hit)) if flag else ""
    except Exception:
        return ""


def _now():
    import time
    return time.strftime("%Y-%m-%dT%H:%M:%S")


def self_test():
    from hooklib import a1_selftest
    from rules import all_rule_modules
    results = []
    for mod in all_rule_modules():
        for name, ok, detail in mod.run_self_test():
            results.append(("%s %s" % (mod.RULE_ID, name), ok, detail))
    results += a1_selftest.run(HERE, handle, KILL_ENV)
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
