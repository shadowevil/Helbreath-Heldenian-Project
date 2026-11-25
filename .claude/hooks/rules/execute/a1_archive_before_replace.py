"""A1: archive before replace (user, 2026-11-25, d-035: "We should also be replacing lower quality
versions if we have a hd counterpart"; "Archiving the old versions"). Where an HD asset exists the
game draws it at every scale and the low-res version is archived byte-identical under
working/archive/, never deleted.

What is checkable: a tool call that deletes, moves away or overwrites a tracked art file under
staged/client/sprites/ while the file's last committed bytes (git HEAD) are in no file under
working/archive/. Matching is by content hash anywhere under working/archive/, so the archive layout
is free. What is not checkable: whether an HD counterpart really exists and is wired in (that is the
other half of d-035). Writes made inside a script (python, build tools) are not seen.

extract() and judge() are pure (the live hook and the replay both call them). The disk and git
look-ups live in the entry point. LOG-ONLY: nothing here blocks.
"""
import re

RULE_ID = "A1"
WATCH = "staged/client/sprites/"
HD_MARK = "classic_ui/hd/"
ART_EXT = (".png", ".jpg", ".jpeg", ".bmp", ".gif", ".webp", ".tga", ".psd")
EDIT_TOOLS = ("Write", "Edit", "MultiEdit")
SHELL_TOOLS = ("Bash", "PowerShell")

_SPLIT = re.compile(r"&&|\|\||;|\n|\|")
_TOKEN = re.compile(r"\"[^\"]*\"|'[^']*'|\S+")
_REDIR = re.compile(r"(?<![0-9&<])>>?\s*(\"[^\"]*\"|'[^']*'|[^\s;|&]+)")
_LEAD = ("do", "then", "else", "{", "&", "if")
DELETE = {"rm", "del", "erase", "remove-item", "ri", "unlink"}
MOVE = {"mv", "move", "move-item", "mi"}
COPY = {"cp", "copy", "copy-item", "cpi"}

MSG = ("A1: %d art file(s) under staged/client/sprites/ were deleted or overwritten with their "
       "committed bytes in no file under working/archive/ (d-035: HD replaces the low-res version, "
       "the old one is archived byte-identical, never deleted). First: %s. Archive the old bytes "
       "(git show HEAD:<path> > working/archive/...) and keep the replacement.")


def _unq(tok):
    return tok[1:-1] if len(tok) >= 2 and tok[0] == tok[-1] and tok[0] in "\"'" else tok


def _norm(p):
    return p.replace("\\", "/")


def _rel(tok, cwd, root):
    """Root-relative forward-slash path of a command token, or None (outside root, unresolvable)."""
    t = _norm(_unq(tok)).strip()
    if not t or "$" in t or "`" in t or "%" in t:
        return None
    m = re.match(r"^/([A-Za-z])/(.*)$", t)
    if m:
        t = m.group(1).upper() + ":/" + m.group(2)
    if not re.match(r"^[A-Za-z]:/", t):
        t = _norm(cwd).rstrip("/") + "/" + t if cwd else t
    r = _norm(root).rstrip("/")
    tl, rl = t.lower(), r.lower()
    if not tl.startswith(rl + "/"):
        return None
    out = []
    for seg in t[len(r) + 1:].split("/"):
        if seg == "..":
            if out:
                out.pop()
        elif seg and seg != ".":
            out.append(seg)
    return "/".join(out)


def _watched(rel):
    return bool(rel) and (rel.lower() + "/").startswith(WATCH)


def extract(tool, tin, cwd, root):
    """[{rel, verb, src_hd}] for the watched paths one tool call deletes, moves away or overwrites.
    verb is 'delete' or 'overwrite'. Pure: no disk access."""
    out = []
    tin = tin or {}
    if tool in EDIT_TOOLS:
        r = _rel(tin.get("file_path") or "", cwd, root)
        if _watched(r):
            out.append({"rel": r, "verb": "overwrite", "src_hd": False})
        return out
    if tool not in SHELL_TOOLS:
        return out
    cmd = tin.get("command") or ""
    for seg in _SPLIT.split(cmd):
        toks = _TOKEN.findall(seg)
        while toks and toks[0].lower() in _LEAD:
            toks = toks[1:]
        if not toks:
            continue
        for m in _REDIR.finditer(seg):
            r = _rel(m.group(1), cwd, root)
            if _watched(r):
                out.append({"rel": r, "verb": "overwrite", "src_hd": False})
        head = _unq(toks[0]).lower()
        if head == "git" and len(toks) > 1:
            head = "git " + toks[1].lower()
            toks = toks[1:]
        pos = [t for t in toks[1:] if not _unq(t).startswith("-") and not _unq(t).startswith(">")]
        if head in DELETE or head == "git rm":
            for t in pos:
                r = _rel(t, cwd, root)
                if _watched(r):
                    out.append({"rel": r, "verb": "delete", "src_hd": False})
        elif head in MOVE or head == "git mv":
            for i, t in enumerate(pos):
                r = _rel(t, cwd, root)
                if _watched(r):
                    out.append({"rel": r, "verb": "overwrite" if i == len(pos) - 1 else "delete",
                                "src_hd": False})
        elif head in COPY and len(pos) >= 2:
            r = _rel(pos[-1], cwd, root)
            if _watched(r):
                hd = any(HD_MARK in _norm(_unq(t)).lower() for t in pos[:-1])
                out.append({"rel": r, "verb": "overwrite", "src_hd": hd})
    return out


def judge(entries, src_hd=False):
    """entries: [{path, state ('deleted'|'changed'|'unchanged'), archived (bool)}] for the tracked
    files the call touched. Returns the hit dict or None. Only art files count."""
    arts = [e for e in entries if e["path"].lower().endswith(ART_EXT)]
    moved = [e for e in arts if e["state"] in ("deleted", "changed")]
    bad = [e for e in moved if not e["archived"]]
    if not bad:
        return None
    return {"rule": RULE_ID, "n_touched": len(moved), "n_unarchived": len(bad),
            "files": [e["path"] for e in bad][:10], "src_hd": bool(src_hd),
            "states": sorted({e["state"] for e in bad})}


def message(hit):
    return MSG % (hit["n_unarchived"], hit["files"][0])


R = "C:/proj"
SELF_TEST = [
    # (name, tool, tool_input, expected [(rel, verb, src_hd)])
    ("rm one", "Bash", {"command": 'rm "C:/proj/staged/client/sprites/interface/a.png"'},
     [("staged/client/sprites/interface/a.png", "delete", False)]),
    ("rm git-bash path", "Bash", {"command": "rm /c/proj/staged/client/sprites/a.png /c/proj/staged/client/sprites/b.png"},
     [("staged/client/sprites/a.png", "delete", False), ("staged/client/sprites/b.png", "delete", False)]),
    ("git rm flag skipped", "Bash", {"command": "git rm -q staged/client/sprites/a.png"},
     [("staged/client/sprites/a.png", "delete", False)]),
    ("Remove-Item LiteralPath", "PowerShell",
     {"command": 'Remove-Item -LiteralPath "C:\\proj\\staged\\client\\sprites\\game_dialogs\\001.png"; Test-Path x'},
     [("staged/client/sprites/game_dialogs/001.png", "delete", False)]),
    ("cp from hd overwrites", "Bash",
     {"command": "cp working/classic_ui/hd/main_menu.png staged/client/sprites/interface/front_end/main_menu.png && cmp a b"},
     [("staged/client/sprites/interface/front_end/main_menu.png", "overwrite", True)]),
    ("cp from sprites is a source, not a hit", "Bash",
     {"command": "cp staged/client/sprites/npcs/ogre/manifest.json .claude/scratch/x/"}, []),
    ("cp copy of a sprite into hd is not a hit", "Bash",
     {"command": "cp staged/client/sprites/interface/sprite_fonts/000.png working/classic_ui/hd/sf-000.png"}, []),
    ("for-loop do cp", "Bash",
     {"command": "for n in 000 001; do cp x.png staged/client/sprites/interface/sprite_fonts/$n.png; done"}, []),
    ("mv source deletes, dest overwrites", "Bash",
     {"command": "mv staged/client/sprites/a.png working/archive/a.png"},
     [("staged/client/sprites/a.png", "delete", False)]),
    ("mv into sprites", "Bash", {"command": "mv .claude/scratch/n.png staged/client/sprites/a.png"},
     [("staged/client/sprites/a.png", "overwrite", False)]),
    ("redirect overwrite", "Bash", {"command": "python make.py > staged/client/sprites/a.png"},
     [("staged/client/sprites/a.png", "overwrite", False)]),
    ("stderr redirect not a hit", "Bash", {"command": "git status staged/client/sprites 2>/dev/null"}, []),
    ("read-only ls", "Bash", {"command": "ls staged/client/sprites/interface | head"}, []),
    ("Write tool", "Write", {"file_path": "C:\\proj\\staged\\client\\sprites\\a.png"},
     [("staged/client/sprites/a.png", "overwrite", False)]),
    ("Write elsewhere", "Write", {"file_path": "C:\\proj\\working\\classic_ui\\hd\\a.png"}, []),
    ("dotdot resolved", "Bash", {"command": "rm staged/client/sprites/../../x.png"}, []),
    ("prefix is not a segment", "Bash", {"command": "rm staged/client/sprites_old/a.png"}, []),
    ("outside root", "Bash", {"command": "rm D:/other/staged/client/sprites/a.png"}, []),
    ("variable path unresolved", "PowerShell", {"command": "Remove-Item $dst\\staged\\client\\sprites\\a.png"}, []),
    ("quoted path with space", "Bash", {"command": 'rm "staged/client/sprites/my dir/a.png"'},
     [("staged/client/sprites/my dir/a.png", "delete", False)]),
    ("chained rm after cd", "Bash", {"command": "cd /c/proj && rm staged/client/sprites/a.png"},
     [("staged/client/sprites/a.png", "delete", False)]),
    ("case-insensitive dir", "Bash", {"command": "rm Staged/Client/Sprites/A.PNG"},
     [("Staged/Client/Sprites/A.PNG", "delete", False)]),
]

JUDGE_TEST = [
    ("unarchived delete flags", [dict(path="staged/client/sprites/a.png", state="deleted", archived=False)], True),
    ("archived delete silent", [dict(path="staged/client/sprites/a.png", state="deleted", archived=True)], False),
    ("unchanged silent", [dict(path="staged/client/sprites/a.png", state="unchanged", archived=False)], False),
    ("changed unarchived flags", [dict(path="staged/client/sprites/a.png", state="changed", archived=False)], True),
    ("json data file ignored", [dict(path="staged/client/sprites/a/manifest.json", state="changed", archived=False)], False),
    ("mixed: one unarchived flags", [dict(path="staged/client/sprites/a.png", state="deleted", archived=True),
                                     dict(path="staged/client/sprites/b.png", state="deleted", archived=False)], True),
    ("empty silent", [], False),
]


def run_self_test():
    out = []
    for name, tool, tin, want in SELF_TEST:
        got = [(c["rel"], c["verb"], c["src_hd"]) for c in extract(tool, tin, "C:/proj", R)]
        ok = got == want
        out.append(("extract: " + name, ok, "" if ok else "got %r want %r" % (got, want)))
    for name, ent, want in JUDGE_TEST:
        hit = judge(ent)
        ok = bool(hit) == want
        if ok and hit:
            message(hit)
        out.append(("judge: " + name, ok, "" if ok else "got %r" % (hit,)))
    return out
