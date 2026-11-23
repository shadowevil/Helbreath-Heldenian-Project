# Bulk Script Dry Run & Verification Standards

*Assessed 2026-11-23 (docs standard sect. 1): kept whole, about 6 KB. One continuous spec, read once when writing a Mode 2 script. This is the single home of the `--dry-run`/`--verify` detail; `.claude/docs/workflow.md` points here. Originally written as a plan, now the adopted standard.*

Contents: Motivation - Current limitations - Two-phase verification - Script template - Collision scanner - Output format - Files changed - Retrofitting.

## Motivation

Mode 2 bulk rename scripts (e.g., `client_snake_case.py`, `client_methods_snake_case.py`)
have caused issues — Shared enum collisions, SFMLEngine method collisions, regex
false positives. The existing `--dry-run` shows basic line counts but doesn't catch
these problems before they happen. Scripts need a standardized verification step
that identifies potential issues before any files are modified.

## Current Dry Run Limitations

The existing `--dry-run`:
- Shows per-file replacement counts and first 20 line details
- Does NOT flag potential collisions (e.g., renaming a name that exists in Shared)
- Does NOT verify the rename won't break `#include` paths
- Does NOT check if a rename target is a C++ keyword
- Does NOT show context around matches (hard to judge if a match is correct)
- Truncates detail output at 20 lines per file

## Proposed Standard: Two-Phase Verification

All Mode 2 bulk scripts should support two verification phases before applying:

### Phase 1: `--dry-run` (Quick Preview)

What it does now, plus:

- **Full detail output** — no truncation, write all details to a log file
  (`.claude/scratch/tools/bulk/<script_name>_dry_run.log`)
- **Summary by rename entry** — report which rename entries had matches and how
  many, so unused entries are visible
- **Keyword check** — warn if any rename target is a C++ reserved keyword
  (`register`, `default`, `class`, `new`, `delete`, etc.)

### Phase 2: `--verify` (Collision Detection)

New flag. Scans for potential problems without modifying files:

- **Shared collision scan** — for each rename entry, grep
  `sources/dependencies/shared/` for the old name in enum/struct/class contexts.
  Report matches as potential collisions.
- **SFMLEngine collision scan** — for each rename entry, grep
  `sources/sfml_engine/` for the old name in class method declarations.
  Report matches as potential collisions.
- **Cross-scope check** — for each rename entry, check if the old name appears
  in files OUTSIDE the script's target directory (e.g., a client script matching
  in Shared code). Report as out-of-scope matches.
- **Duplicate target check** — warn if two different old names map to the same
  new name (would create ambiguity).
- **Context preview** — for each flagged collision, show the line with surrounding
  context so the user can judge if it's a real problem.

Output goes to `.claude/scratch/tools/bulk/<script_name>_verify.log`.

### Phase 3: Apply (no flag)

Run without `--dry-run` or `--verify` to apply. Should refuse to run if
`--verify` has never been run (check for existence of verify log), or allow
`--skip-verify` to bypass.

## Script Template

New scripts should follow this pattern:

```python
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--dry-run", action="store_true",
                        help="Preview changes without modifying files")
    parser.add_argument("--verify", action="store_true",
                        help="Scan for collisions in Shared/SFMLEngine")
    parser.add_argument("--skip-verify", action="store_true",
                        help="Apply without requiring prior --verify")
    args = parser.parse_args()

    if args.verify:
        return run_verify(renames)
    elif args.dry_run:
        return run_dry_run(renames)
    else:
        return run_apply(renames, skip_verify=args.skip_verify)
```

## Collision Scanner Details

The `--verify` scanner for each rename entry (old_name → new_name):

1. **Shared enum scan**: Search `sources/dependencies/shared/` for `\bold_name\b`
   in lines containing `enum`, `=`, or appearing inside enum blocks. Flag as
   "Shared enum collision."

2. **Shared struct/class scan**: Search `sources/dependencies/shared/` for
   `\bold_name\b` in lines containing `struct`, `class`, `::`, or method
   declarations. Flag as "Shared type collision."

3. **SFMLEngine interface scan**: Search `sources/sfml_engine/` for `\bold_name\b`
   in lines containing `virtual`, `override`, or method declarations. Flag as
   "SFMLEngine interface collision."

4. **C++ keyword check**: Check if `new_name` is in the set of C++ reserved words.

5. **Duplicate target check**: Check if `new_name` appears as a value for any
   other entry in the RENAMES dict.

## Output Format

**Dry run log** (`.claude/scratch/tools/bulk/<name>_dry_run.log`):
```
=== Dry Run: client_methods_snake_case.py ===
Total renames: 335
Files scanned: 273

--- Per-file details ---
sources/client/game.cpp: 1023 replacements
  L142: DrawObjects -> draw_objects (1x)
  L145: CommandProcessor -> command_processor (1x)
  ...

--- Unused entries (0 matches) ---
  "SomeMethod" -> "some_method"  (no matches found)

--- Summary ---
Would change: 5510 replacements across 202 files
```

**Verify log** (`.claude/scratch/tools/bulk/<name>_verify.log`):
```
=== Verify: client_methods_snake_case.py ===

--- Collisions Found ---
[SHARED ENUM] "GrandMagicResult" -> "grand_magic_result"
  sources/dependencies/shared/net/net_messages.h:288
    GrandMagicResult = 0x0B9D,

[SHARED ENUM] "Stop" -> "stop"
  sources/dependencies/shared/entity/action_id.h:12
    Stop = 0,

[C++ KEYWORD] "Register" -> "register"
  `register` is a C++ reserved keyword

--- Clean (no collisions) ---
335 entries checked, 3 collisions found, 332 clean
```

## Files Changed

| File | Change |
|------|--------|
| Future Mode 2 scripts | Follow the two-phase template |
| `CLAUDE.md` | Update Mode 2 section to require `--verify` before apply |
| `.claude/docs/workflow.md` | Add verification standards to script pattern docs |

## Retrofitting Existing Scripts

Existing scripts (`client_snake_case.py`, `client_methods_snake_case.py`) are
already committed and applied. No need to retrofit them. The standard applies
to new scripts going forward.
