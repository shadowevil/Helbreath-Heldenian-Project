# Project hook rules

Contents: Layout - D1: linked doc over 10 KB - N1: d-030 (not hookable).

## Layout

- `doc_size_guard.py` - thin PostToolUse entry point for `Write|Edit|MultiEdit`; `--self-test` runs every rule's cases plus an end-to-end run (never touches the real log).
- `hooklib/paths.py` - project root (`CLAUDE_PROJECT_DIR`, else the event `cwd`) and anchored relative-path matching. `hooklib/output.py` - ASCII text, locked log append with a 2 MB cap and one rotation, the PostToolUse context JSON.
- `rules/<action>/<id>_<name>.py` - one module per rule: its check, message and `SELF_TEST` cases. `rules/__init__.py` collects them.
- `replay_d1.py` - read-only replay over the project's transcripts. `doc_size_exempt.txt` - D1's documented exceptions.
- Hits go to `.claude/hooks/log/doc_size_hits.jsonl` (gitignored). Everything is stdlib only, fails open and prints ASCII; nothing blocks.

## D1: linked doc over 10 KB

- **Rule** (user, 2026-11-23, HH-0027 / d-026): "the goal is to keep documentation files under 10kb when linked to the CLAUDE.md - if it gets bigger than that it needs to be audited ...". Recorded in `.claude/docs/documentation.md`. The global docs standard sect. 1 has the same ~10 KB "assess" trigger, so the rule is generic: nothing in the code names this project.
- **Class:** hookable as a flag (PostToolUse cannot deny, and size alone never decides). Id `D1`, module `rules/write/d1_doc_size.py`.
- **Covers:** root `CLAUDE.md` (d-016) and every `.claude/docs/**/*.md`, anchored at the root (copies under `.claude/scratch/` are not docs). The limit is 10,240 bytes. `documentation.md`'s own PowerShell check uses `-gt 10000`, so the two disagree by 240 bytes.
- **Logged per hit:** `size` (from disk), `prev_size` (from `tool_response.originalFile`, CRLF-corrected; else the Edit string delta; unknown for `replace_all`), `crossed`, `step_crossed`, `exempt`, `assessed` (an italic `*Assessed` or bold `**Assessed` line in the first 60 lines), `would_flag`, `flagged`, `bucket`.
- **Flags** (with `--flag` only) when a doc that is not exempt either just crossed 10 KB, is over with no assessment line, or is assessed and just grew past another 2 KB step. Once per session, path and 2 KB bucket.
- **Modes:** log-only by default. `--flag` shows the message to the agent. `CLAUDE_DOC_SIZE_GUARD=log` is a kill switch. `--hits-path=` or `CLAUDE_DOC_SIZE_GUARD_HITS_PATH` redirects the rows.
- **Known gaps:**
  - Writes made through Bash or PowerShell (Mode 2 scripts, `git mv`) are not seen.
  - **Exempt docs never flag or re-flag, however large they grow.** `tickets.md` is the one exempt doc; growth past a 2 KB step is logged (`step_crossed`) but never shown. Removing a line from `doc_size_exempt.txt` is the only way to bring a doc back under the flags.
  - The step re-flag is a staged proposal, not a user ruling. It exists because of the decisions.md finding below.
- **Measured 2026-11-23** (one `replay_d1.py` run: 100 transcripts, 1,259 file writes, 114 to covered docs):
  - **Coverage.** 65 of the 114 were judged: 41 directly from their results, 24 rebuilt by chaining each doc's last full Read or Write. The other 49 are unknown: Edits whose result has a null `originalFile` and no chain. Most are CLAUDE.md (38), which is now 9,921 B. Back-chaining its Edit deltas from that size puts its peak at about 10,067 B: no hit, but 173 B under the limit (estimated). The live hook reads the size from disk and has no such gap.
  - **Hits.** 11, all on `.claude/docs/decisions.md`. One crossing (9,700 -> 11,271), then 10 edits on to 15,101 bytes. The chain's last size matches the 15,101 bytes measured on disk before the split.
  - **Flags.** Without the step rule there is 1 would-flag. The doc's "kept whole, one file" assessment line was already there at 9,700 B, under the limit, and it silenced the other 10. With the step rule there are 3 (11,271, 12,326 and 14,608). The doc was then audited and split (HH-0025, done; now 5,292 B plus six sidecars).
  - **False positives.** 0 of 11 hand-checked: every hit is a doc linked from CLAUDE.md line 19 and really over 10 KB.
  - **Boundary cases.** None: no hit or near miss falls within one byte per line of the limit.
  - **The `assessed` signal.** 13 of the 36 covered docs carry an `*Assessed` line now. So "over with no assessment line" is a real branch, not one that almost never fires.
  - **Over the limit now:** `.claude/docs/tickets.md` (37,899 B, exempt) and `.claude/docs/ccontrols-reference/core.md` (11,042 B). No transcript write touched core.md, so D1 will log it on its next Write or Edit.
- **Cost** (measured): 70-74 ms median per call under `python -I -S`, max 139 ms. A bare interpreter start alone takes 38 ms.

## N1: d-030 (not hookable)

- **Rule** (user, 2026-11-24): "Finish all of hte tickets - if something needs my eye then it should go into user review". Recorded in `.claude/docs/ticket-workflow.md` "Status".
- **Class:** not hookable. Id `N1`; nothing is built.
- **Why:** it governs when a session stops and what it parks. "Finished everything the team can" and "needs the user's eye" are judgments about the work, not properties of a tool call. A Stop hook cannot tell a legitimate stop (waiting on a user decision put in chat) from an early one. The ticket moves go through `author.py` and `tickets.py`, which hold no "should this go to user_review" signal.
- **Possible future check (not built):** a Stop hook that lists tickets still `in_progress` while no agent is running. It would report a symptom of the rule, not the rule itself, and it needs the ticket store's schema, which the ticket system's maintainer owns.
