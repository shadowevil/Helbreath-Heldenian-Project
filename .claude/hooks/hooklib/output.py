"""ASCII-safe text, the hit log (locked append, size cap with one rotation) and the PostToolUse
context message. Every function fails open: a logging problem never breaks the tool call."""
import json
import os
import time

LOG_MAX_BYTES = 2 * 1024 * 1024
LOCK_TRIES = 50
LOCK_SLEEP_S = 0.02


def ascii_text(s):
    """The console is cp1252: anything printed must be ASCII."""
    return str(s).encode("ascii", "replace").decode("ascii")


def _lock(fh):
    """Exclusive lock on byte 0 of a side lock file; returns an unlock callable, or None."""
    try:
        if os.name == "nt":
            import msvcrt
            for _ in range(LOCK_TRIES):
                try:
                    fh.seek(0)
                    msvcrt.locking(fh.fileno(), msvcrt.LK_NBLCK, 1)
                    return lambda: (fh.seek(0), msvcrt.locking(fh.fileno(), msvcrt.LK_UNLCK, 1))
                except OSError:
                    time.sleep(LOCK_SLEEP_S)
            return None
        import fcntl
        fcntl.flock(fh.fileno(), fcntl.LOCK_EX)
        return lambda: fcntl.flock(fh.fileno(), fcntl.LOCK_UN)
    except Exception:
        return None


def append_row(path, row):
    """Append one JSON line to `path`. Rotates to `<path>.1` past LOG_MAX_BYTES. Never raises."""
    if not path:
        return False
    try:
        os.makedirs(os.path.dirname(path), exist_ok=True)
        line = json.dumps(row, ensure_ascii=True, sort_keys=True) + "\n"
        with open(path + ".lock", "a+b") as lk:
            unlock = _lock(lk)
            try:
                if os.path.exists(path) and os.path.getsize(path) > LOG_MAX_BYTES:
                    os.replace(path, path + ".1")
                with open(path, "a", encoding="ascii", newline="\n") as fh:
                    fh.write(line)
            finally:
                if unlock:
                    try:
                        unlock()
                    except Exception:
                        pass
        return True
    except Exception:
        return False


def recent_rows(path, tail_bytes=65536):
    """The parsed rows in the last `tail_bytes` of the log (for per-session dedupe). Never raises."""
    rows = []
    try:
        with open(path, "rb") as fh:
            fh.seek(0, 2)
            size = fh.tell()
            fh.seek(max(0, size - tail_bytes))
            chunk = fh.read().decode("ascii", "replace")
        for ln in chunk.splitlines():
            try:
                rows.append(json.loads(ln))
            except ValueError:
                pass
    except Exception:
        pass
    return rows


def post_tool_context(message):
    """The PostToolUse JSON that shows `message` to the agent. It informs; it cannot block."""
    return json.dumps({"hookSpecificOutput": {"hookEventName": "PostToolUse",
                                              "additionalContext": ascii_text(message)}})
