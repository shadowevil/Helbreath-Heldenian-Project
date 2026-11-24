# decisions: Naming and the Phase R rename

*Sidecar of `.claude/docs/decisions.md` (index, scope rule and entry format live there). Each entry is a settled user ruling with the user's exact quote.*

## d-009 Files and folders are snake_case

- Date: 2026-11-23.
- Quote: "We also want to convert everything to snake casing files and folders".
- Decided: project source files and folders are renamed to snake_case; the project rule was then replaced by the global naming rule, and the user kept the rename.
- Recorded in: CLAUDE.md "Coding standards" (naming bullet); `~/.claude/CLAUDE.md` "File and folder names".

## d-010 Phase R runs through a dry-run and a review

- Date: 2026-11-23.
- Quote: "Do whatever you need to do. Scripts and changes should be ran through a dry-run and reviewed before properly executing."
- Decided: the rename script is dry-run and reviewed before it is applied.
- Recorded in: CLAUDE.md "Workflow" (Mode 2); `.claude/docs/workflow.md`.

## d-013 W7 is a global hook

- Date: 2026-11-23.
- Quote: "W7 - this should be a global hook".
- Decided: the file-and-folder naming check is a global hook (W7), following the global naming rule.
- Recorded in: `~/.claude/CLAUDE.md` "File and folder names"; the global hook.

## d-021 Phase R naming choices

- Date: 2026-11-23.
- Quotes (selected answers): "Join acronym", "Drop the I", "Defer data", "Project files and CHANGELOG".
- Decided: acronyms stay one word (`sfml_engine`, `ccontrols`); interface headers drop the `I` (`renderer.h`); `staged/` data files are not renamed in Phase R; `Helbreath.sln`, the `.vcxproj`/`.filters` files and `CHANGELOG.md` keep their names.
- Recorded in: `.claude/tools/scripts/rename_to_naming_rule.decisions.json`.
