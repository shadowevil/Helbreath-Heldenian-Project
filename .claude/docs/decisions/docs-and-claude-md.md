# decisions: Docs standard and CLAUDE.md size

*Sidecar of `.claude/docs/decisions.md` (index, scope rule and entry format live there). Each entry is a settled user ruling with the user's exact quote.*

## d-011 Project docs follow the global docs standard

- Date: 2026-11-23.
- Quote: "follow the global docs"; and for this cleanup, "documentation clean up and standardization, following the global requirements of index files and reference files including a decisions file".
- Decided: Markdown docs and their sidecar folders are lowercase-hyphenated; docs are lean indexes plus sidecars; this file exists.
- Recorded in: CLAUDE.md "Docs (index)"; `~/.claude/docs/documentation_standards.md`.

## d-012 The tools location is each project's call

- Date: 2026-11-23.
- Quote: "W6 shouldn't be a hook - it should be up to the project to define it - but global should have a recommendation".
- Decided: no hook enforces where workflow tools live; this project uses `.claude/tools/`, with one-off scripts in `.claude/tools/scripts/`.
- Recorded in: CLAUDE.md "Workflow".

## d-016 CLAUDE.md stays lean

- Date: 2026-11-23.
- Quote: "CLAUDE.md should not be 20kb".
- Decided: CLAUDE.md keeps each binding rule as a short statement plus a pointer; detail lives in `.claude/docs/`.
- Recorded in: CLAUDE.md "Docs (index)".

## d-026 Docs linked from CLAUDE.md stay under 10 KB

- Date: 2026-11-23.
- Quote: "the goal is to keep documentation files under 10kb when linked to the CLAUDE.md - if it gets bigger than that it needs to be audited and checked to see if it needs to have an index system to keep it lean".
- Decided: a linked doc over 10 KB gets an audit ticket and is split into an index plus sidecars, or kept whole with a dated reason.
- Recorded in: `.claude/docs/documentation.md`; CLAUDE.md "Docs (index)".
- Recorded in: CLAUDE.md "Workflow"; `.claude/docs/ticket-workflow.md`; `.claude/docs/workflow.md`; `.claude/docs/agents.md`.
