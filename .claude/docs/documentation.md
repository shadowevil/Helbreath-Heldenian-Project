# Documentation (this project's additions)

*Assessed 2026-11-23 (docs standard sect. 1): kept whole, about 2 KB; one rule and its procedure. The full standard is global: `C:\Users\ShadowEvil\.claude\docs\documentation_standards.md` (read it before adding or restructuring a doc). This file holds only what this project adds.*

Contents: The 10 KB goal - When a doc goes over - Exceptions - Checking sizes.

## The 10 KB goal

- **Every doc linked from CLAUDE.md stays under 10 KB** (user, 2026-11-23: "the goal is to keep documentation files under 10kb when linked to the CLAUDE.md - if it gets bigger than that it needs to be audited and checked to see if it needs to have an index system to keep it lean").
- This covers each file CLAUDE.md's Docs index names, and every index those files point to. A sidecar file reached through an index should also stay under 10 KB, so a lookup is one small index read plus one small targeted read.
- CLAUDE.md itself stays under 10 KB (d-016).

## When a doc goes over

1. File a small ticket (`docs`, `cleanup`) for the audit; see `.claude/docs/ticket-workflow.md`.
2. Audit it against the global standard:
   - **Several standalone topics:** split it into a lean index (`topic.md`, one line per entry: the path, what it covers, WHEN to read it) and a sidecar folder (`topic/<entry>.md`). Keep the index's path, so links into it still work; keep cited section numbers resolvable.
   - **One cohesive topic:** keep it whole, add or update its Contents block and its italic assessment line saying why it stays over 10 KB.
   - **Stale or duplicated content:** remove it first; the doc may drop under the limit without a split.
3. Grep the repo for every moved path and heading and repair each reference; whitelist new files in `.gitignore` (it ignores `*.md` by default) and confirm with `git check-ignore -v`.
4. A read-only review checks that no content was lost, then commit.

## Exceptions

- `.claude/docs/tickets.md` (about 38 KB) is the ticket system's release text, merged on every upgrade; splitting it would break the upgrade merge. It stays whole; changes go to the ticket system's maintainer.
- A doc kept whole after an audit says so in its assessment line, with the date.

## Checking sizes

```powershell
Get-ChildItem C:\Users\ShadowEvil\source\Helbreath-Heldenian-Project\.claude\docs -Recurse -Filter *.md | Where-Object Length -gt 10240 | Select-Object FullName, Length
```

Run it after any doc change; anything listed that is not an exception above gets an audit ticket.
