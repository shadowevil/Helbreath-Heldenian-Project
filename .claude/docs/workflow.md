# Workflow Details

*Assessed 2026-11-23 (docs standard sect. 1): kept whole, about 6 KB. The Mode 1 cycle, the Mode 2 script pattern and its regex rules are read together when planning an edit; the sections cross-refer. Mode 1 and Mode 2 step detail moved here from CLAUDE.md 2026-11-23, which keeps the binding rules. The full `--dry-run`/`--verify` spec is `.claude/docs/bulk-scripts-dry-run.md`.*

Contents: Session context file - Mode 1 - Mode 2 - Bulk script verification - Regex safety - Include ordering.

## Session context file

For complex or multi-step tasks, maintain a session context file (`session_context.md` in the memory directory) with: current task, key decisions, files being worked on, and constraints. It survives context compaction. Delete it when the task is complete.

## Mode 1: Direct Edit (DEFAULT, 1-9 files)

```
bak.py guard <files>  ->  Read/Edit tools  ->  Build  ->  Review  ->  bak.py commit (or revert)
```

1. **Guard**: `python .claude/tools/bak.py guard <file1> [file2 ...]` creates a versioned checkpoint (`.bak_<guid>`). Only files under `sources/`; back up anything else (e.g. `tools/`) to `.claude/scratch/` first.
2. **Edit**: Read and Edit tools.
3. **Simplify**: run `/simplify` (a built-in skill that reviews changed code for reuse, quality and efficiency, then fixes what it finds).
4. **Verify**: for refactors, run `git diff` on the changed files to confirm no logic was lost or altered.
5. **Build**: `powershell -ExecutionPolicy Bypass -File sources/build.ps1 -Target All -Config Debug -NoDeploy` (compile check only; `-NoDeploy` keeps verification builds from writing into `game/`).
6. **Review**: a separate read-only review agent (`ADVISOR: FORBIDDEN`) gets the diff and the goal, not the author's reasoning (global rule). Fix what it finds and rebuild.
7. **If build and review pass**: `python .claude/tools/bak.py commit <id>` (one per checkpoint, or `--force` for all) deletes the `.bak` files and accepts the changes.
8. **If the build fails**, choose:
   - `guard` again to checkpoint, then fix and rebuild (layer the fix).
   - `revert <id>` to undo a specific checkpoint, retry from the previous one.
   - `revert <id> <file1> [file2 ...]` to revert specific files from a checkpoint.

Rules:
- Specific edits to specific lines are Mode 1.
- One logical change per guard-edit-build-commit cycle.
- Each `guard` creates a new checkpoint: layer fixes and peel back selectively.
- `python .claude/tools/bak.py --help` lists guard, status, revert and commit.
- After every `bak.py commit`, update `CHANGELOG.md` (formats: `.claude/docs/changelog-format.md`).

## Mode 2: Python Script (BULK — 10+ files)

Use only for large-scale mechanical transforms (e.g., "replace X with Y in every file containing X").

Must explicitly justify: "This touches N files with pattern X, a script is appropriate because Y."

1. Write a script in `.claude/tools/scripts/` that calls `.claude/tools/bak.py guard` internally.
2. Run `--dry-run` — preview changes, full log to `.claude/scratch/tools/bulk/<script_name>_dry_run.log`.
3. Run `--verify` — scan for collisions (Shared enums, SFMLEngine interfaces, C++ keywords, duplicate targets). Log to `.claude/scratch/tools/bulk/<script_name>_verify.log`.
4. Review both logs. Fix any flagged collisions.
5. Run without flags to apply.
6. If errors: fix the script itself, re-run. **Never write a second "fix" script.**
7. Once 0 errors: `python .claude/tools/bak.py commit <id>` to accept.

## Bulk Script Verification Standards

Every Mode 2 script supports `--dry-run`, `--verify` and `--skip-verify` and must pass `--verify` before it applies. What each phase reports, the argparse template and the log formats are in `.claude/docs/bulk-scripts-dry-run.md`; read it when writing a script.

## Regex Safety Rules (for Mode 2 scripts)

- **NEVER** use `::TypeName` as regex — matches inside prefixed names (`sf::Color`). Use `(?<!\w)::TypeName\b`.
- **NEVER** replace inside `#define` macro names — `#define DEF_X` cannot become `#define hb::shared::X`.
- **ALWAYS** use `\b` word boundaries — `PlayerStatus` without `\b` matches inside `player_status_data.h`.
- **ALWAYS** use `(?<!\w)` lookbehind for `::Name` — `::GetPoint2` matches inside `CMisc::GetPoint2`.
- **PREFER** `content.replace("exact_old", "exact_new")` over regex for known patterns.
- **ALWAYS** order replacements longest-first — `DEF_OBJECTMOVE_CONFIRM` before `DEF_OBJECTMOVE`.
- **USE** placeholder approach for substring collisions (`SFMLInput::` contains `Input::`).
- **TEST** on 2-3 files first when dealing with regex patterns.

## Include Ordering Pitfall

`global_def.h` and `render_constants.h` both define `LOGICAL_WIDTH()`, `LOGICAL_HEIGHT()`, etc.
`render_constants.h` guards behind `#ifndef GLOBALDEF_H_RESOLUTION_FUNCTIONS`.

**Rule**: `global_def.h` must be included BEFORE `renderer.h` (which pulls in `render_constants.h`).
- In .cpp files: include `map_data.h` or `game.h` (transitively includes `global_def.h`) before render headers.
- In .h files: use forward declarations for `IRenderer`, `SpriteCollection`, etc.
