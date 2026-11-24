"""Project root and path matching. Nothing here names a project: the root is CLAUDE_PROJECT_DIR,
else the event's `cwd`, so the same file works as a project hook or a global one."""
import os
import re

_MSYS_DRIVE = re.compile(r"^/([A-Za-z])(/.*)?$")


def normalize(path):
    """Absolute, forward-slash form; a Git Bash `/c/...` path becomes `C:/...`. Empty in, empty out."""
    if not path:
        return ""
    p = str(path).strip().strip('"')
    m = _MSYS_DRIVE.match(p)
    if m and os.name == "nt":
        p = m.group(1).upper() + ":" + (m.group(2) or "/")
    p = os.path.abspath(p)
    return p.replace("\\", "/")


def project_root(event):
    """CLAUDE_PROJECT_DIR if the host set it, else the event's cwd, else the process cwd."""
    return normalize(os.environ.get("CLAUDE_PROJECT_DIR") or (event or {}).get("cwd") or os.getcwd())


def rel_to_root(path, root):
    """`path` relative to `root`, forward slashes, or None when it is outside the root.
    Case-insensitive on Windows. Anchored: a prefix that is not a whole segment does not match."""
    p = normalize(path)
    r = normalize(root).rstrip("/")
    if not p or not r:
        return None
    pc, rc = (p.lower(), r.lower()) if os.name == "nt" else (p, r)
    if not pc.startswith(rc + "/"):
        return None
    return p[len(r) + 1:]
