"""D1: a linked doc over 10 KB gets an audit (user, 2026-11-23, HH-0027 / d-026: "the goal is to keep
documentation files under 10kb when linked to the CLAUDE.md - if it gets bigger than that it needs
to be audited ..."). Recorded in .claude/docs/documentation.md; the global docs standard sect. 1
has the same ~10 KB "assess" trigger, so the rule is generic and nothing here names a project.

Covered: <root>/CLAUDE.md (d-016) and every <root>/.claude/docs/**/*.md (the docs CLAUDE.md links,
the indexes they point to and their sidecars). Anchored at the root: a copy under
.claude/scratch/docs-audit/ is not a doc. Size alone never decides (global standard sect. 1), so a
hit only asks for an audit; it never blocks.

classify() is pure (relative path, sizes, head text, exempt set in; hit dict or None out). The live
hook and the replay both call it, so the replay measures the shipped rule.
"""
import re

RULE_ID = "D1"
LIMIT = 10240          # 10 KiB, per the brief; documentation.md's own check uses -gt 10000
HEAD_BYTES = 16384     # read cap for the head; the assessment search looks at its first HEAD_LINES
HEAD_LINES = 60        # an assessment line sits under the title, after any Contents block
STEP = 2048            # an assessed doc is re-flagged each time it grows past another 2 KB step
# Why STEP (a staged proposal, not a user ruling): replay 2026-11-23 - decisions.md carried a "kept
# whole, one file" assessment line written while it was under 10 KB (9,700 B), then grew 11.3 -> 15.1
# KB over 10 edits with the line unchanged and nothing flagged, until it was audited and split
# (HH-0025). An assessment line holds for the size it was written at, not forever.

# italic or bold: *Assessed, _Assessed, **Assessed, __Assessed
_ASSESS = re.compile(r"^[ \t]*(?:\*\*|__|\*|_)assessed\b", re.IGNORECASE | re.MULTILINE)

MSG_CROSSED = ("D1: %s just went over 10 KB (%d -> %d bytes). Docs linked from CLAUDE.md stay under "
               "10 KB (d-026): file a small docs/cleanup audit ticket and audit it per "
               ".claude/docs/documentation.md (split into an index + sidecar, or keep whole with a "
               "dated assessment line).")
MSG_OVER = ("D1: %s is %d bytes, over the 10 KB goal (d-026), and has no assessment line. Audit it "
            "per .claude/docs/documentation.md, or record why it stays whole in an italic "
            "'*Assessed <date>: ...*' line under the title.")
MSG_GREW = ("D1: %s grew past another 2 KB step (%d -> %d bytes) since its assessment line. Re-check "
            "that the line still holds per .claude/docs/documentation.md (split, or re-date it).")


def applies(rel):
    """True for root CLAUDE.md and any .md under .claude/docs/ (relative path, forward slashes)."""
    if not rel:
        return False
    r = rel.replace("\\", "/").lower()
    if r == "claude.md":
        return True
    return r.startswith(".claude/docs/") and r.endswith(".md")


def assessment_line(head_text):
    """The italic or bold '*Assessed ...' / '**Assessed ...**' line in the doc's first HEAD_LINES
    lines, or ''."""
    t = "\n".join((head_text or "").split("\n")[:HEAD_LINES])
    m = _ASSESS.search(t)
    if not m:
        return ""
    end = t.find("\n", m.end())
    return t[m.start():end if end >= 0 else len(t)].strip()


def has_assessment(head_text):
    return bool(assessment_line(head_text))


def classify(rel, size, prev_size=None, head_text="", exempt=()):
    """The hit for one write, or None. `prev_size` None means unknown; 0 means a new file."""
    if not applies(rel) or size is None or size <= LIMIT:
        return None
    norm = rel.replace("\\", "/")
    return {
        "rule": RULE_ID,
        "path": norm,
        "size": int(size),
        "prev_size": None if prev_size is None else int(prev_size),
        "crossed": None if prev_size is None else prev_size <= LIMIT,
        "step_crossed": None if prev_size is None else (prev_size - 1) // STEP < (size - 1) // STEP,
        "exempt": norm.lower() in {e.lower() for e in exempt},
        "assessed": has_assessment(head_text),
    }


def should_flag(hit):
    """In flag mode: tell the agent when a doc just crossed 10 KB, is over with no assessment line,
    or (assessed) just grew past another STEP. Exempt docs are logged but never flagged."""
    if not hit or hit["exempt"]:
        return False
    return bool(hit["crossed"]) or bool(hit["step_crossed"]) or not hit["assessed"]


def message(hit):
    if hit["crossed"]:
        return MSG_CROSSED % (hit["path"], hit["prev_size"], hit["size"])
    if hit["assessed"] and hit["step_crossed"]:
        return MSG_GREW % (hit["path"], hit["prev_size"], hit["size"])
    return MSG_OVER % (hit["path"], hit["size"])


# (name, classify kwargs, expected subset of the hit or None, expected should_flag)
SELF_TEST = [
    ("under limit", dict(rel=".claude/docs/build.md", size=5000), None, False),
    ("exactly limit", dict(rel=".claude/docs/build.md", size=10240), None, False),
    ("one over, crossed", dict(rel=".claude/docs/build.md", size=10241, prev_size=9000),
     {"crossed": True, "exempt": False}, True),
    ("over, already over, no assessment", dict(rel=".claude/docs/decisions.md", size=15101, prev_size=15000),
     {"crossed": False, "assessed": False}, True),
    ("over, already over, assessed", dict(rel=".claude/docs/agents.md", size=12000, prev_size=11900,
                                         head_text="# Agents\n\n*Assessed 2026-11-23: kept whole.*\n"),
     {"crossed": False, "assessed": True}, False),
    ("assessed, grew past 12 KB step, flags", dict(rel=".claude/docs/decisions.md", size=12326, prev_size=12158,
                                                   head_text="*Assessed 2026-11-23: kept whole.*"),
     {"crossed": False, "step_crossed": True, "assessed": True}, True),
    ("assessed, growth inside a step, silent", dict(rel=".claude/docs/decisions.md", size=12158, prev_size=12069,
                                                   head_text="*Assessed 2026-11-23: kept whole.*"),
     {"step_crossed": False}, False),
    ("exactly on a step boundary is not past it", dict(rel=".claude/docs/a.md", size=12288, prev_size=12000,
                                                       head_text="*Assessed x*"),
     {"step_crossed": False}, False),
    ("assessed but just crossed still flags", dict(rel=".claude/docs/agents.md", size=12000, prev_size=9000,
                                                   head_text="_Assessed 2026-11-23_"),
     {"crossed": True, "assessed": True}, True),
    ("exempt never flags", dict(rel=".claude/docs/tickets.md", size=37899, prev_size=1000,
                               exempt=(".claude/docs/tickets.md",)),
     {"exempt": True, "crossed": True}, False),
    ("assessment mid-sentence does not count", dict(rel=".claude/docs/a.md", size=12000, prev_size=11000,
                                                    head_text="# A\n\nThis was *assessed* later.\n"),
     {"assessed": False}, True),
    ("indented assessment counts", dict(rel=".claude/docs/a.md", size=12000, prev_size=11000,
                                        head_text="# A\n\n  *Assessed 2026-11-23: kept whole.*\n"),
     {"assessed": True}, False),
    ("bold assessment counts", dict(rel=".claude/docs/a.md", size=12000, prev_size=11000,
                                    head_text="# A\n\n**Assessed 2026-11-23: kept whole.**\n"),
     {"assessed": True}, False),
    ("assessment on line 50 counts", dict(rel=".claude/docs/a.md", size=12000, prev_size=11000,
                                          head_text="# A\n" + "- x\n" * 48 + "*Assessed x*\n"),
     {"assessed": True}, False),
    ("assessment past line 60 does not", dict(rel=".claude/docs/a.md", size=12000, prev_size=11000,
                                              head_text="# A\n" + "- x\n" * 70 + "*Assessed x*\n"),
     {"assessed": False}, True),
    ("exempt grown far never re-flags", dict(rel=".claude/docs/tickets.md", size=80000, prev_size=37899,
                                             exempt=(".claude/docs/tickets.md",)),
     {"exempt": True, "step_crossed": True}, False),
    ("sidecar covered", dict(rel=".claude/docs/versioning/build-id.md", size=11000),
     {"crossed": None}, True),
    ("root CLAUDE.md covered", dict(rel="CLAUDE.md", size=10300, prev_size=10100), {"crossed": True}, True),
    ("nested CLAUDE.md not covered", dict(rel="sources/CLAUDE.md", size=20000), None, False),
    ("scratch copy not covered", dict(rel=".claude/scratch/docs-audit/decisions.md", size=20000), None, False),
    ("non-md under docs", dict(rel=".claude/docs/sizes.txt", size=20000), None, False),
    ("case-insensitive match", dict(rel=".CLAUDE/Docs/Build.MD", size=20000), {"path": ".CLAUDE/Docs/Build.MD"}, True),
    ("new file over", dict(rel=".claude/docs/new.md", size=11000, prev_size=0), {"crossed": True}, True),
    ("unknown size", dict(rel=".claude/docs/build.md", size=None), None, False),
]


def run_self_test():
    """[(name, ok, detail)] for every case."""
    out = []
    for name, kw, want, want_flag in SELF_TEST:
        hit = classify(**kw)
        if want is None:
            ok = hit is None
        else:
            ok = hit is not None and all(hit.get(k) == v for k, v in want.items())
        ok = ok and should_flag(hit) == want_flag
        if ok and hit is not None:
            message(hit)  # must format without raising
        out.append((name, ok, "" if ok else "got %r flag=%r" % (hit, should_flag(hit))))
    return out
