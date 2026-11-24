# Ticket workflow (this project's practice)

*Assessed 2026-11-23 (docs standard sect. 1): kept whole, about 3 KB; one procedure read together. This is the project's own practice on top of the ticket system's rules doc, `.claude/docs/tickets.md` (release text, merged on upgrade: never put project rules there). The procedure for the CLI is the `tickets` skill. Settled rulings are d-022 to d-025 in `.claude/docs/decisions.md`.*

Contents: Always - Filing - Status - Comments - With agents - Closing - Viewer.

## Always

- **Tickets are how work is tracked** (user, 2026-11-23: "Make sure we are filing tickets as we work, as the tickets are how we keep track of our work"). Every piece of work gets a ticket when it starts, not after.
- The CLI is the only writer (`python .claude/tools/tickets/tickets.py`, `author.py` for new tickets). Never hand-edit a ticket's JSON.
- Run `tickets.py validate` after each batch of edits and before any commit that touches `.claude/tickets/`.

## Filing

- **Small tickets** (user: "Tickets should be small and multiple tickets for larger work"). One ticket is one reviewable change or one finding.
- **Larger work is a task with sub-tasks** (user: "Broken into tasks/sub-tasks"):
  - a task ticket, summary ending `(task)`, holding the goal, the plan link and the decisions;
  - one sub-task ticket per piece, summary prefixed with the task's short name and step (`Split 1a: ...`);
  - each sub-task blocks its task: `tickets.py set <sub-task> --block <task>`. The task closes only when every sub-task is done.
- Every ticket carries one `kind` tag (bug, feature, cleanup) and at least one `area` tag (client, server, shared, database, tooling, docs).
- The body follows the skill's shape: the finding, measured facts (each number marked measured / derived / unverified), **Done when**, Source.
- Work already landed with no ticket gets one filed `done` with its commit link.

## Status

- **`in_progress` only while an agent is actively working that ticket** (user, 2026-11-23: "Only put tickets in progress if an agent is actively working on them. A master ticket can remain in in-progress as long as agents are actively working on sub tickets. But only put sub tickets in progress if an agent is working on them").
  - A sub-task nobody is working stays `open`, even if its task is in progress.
  - A task is `in_progress` while any agent works any of its sub-tasks; otherwise `open`.
- `in_review` when the work is handed to the review agent; back to `in_progress` if the review sends it back.
- `verified` once review passes and the build is green, so only the commit is left.
- `user_review` when the user must look or decide, with a line saying what. **The board is view-only for the user** (user, 2026-11-23: "I also cannot interact with the ticket board, only view. So all questions or issues should come to me"): every question, decision or issue that needs the user ("Well issues that request me") is put to them in chat (a question with buttons where possible); issues the team can handle are just tracked on the board. Never wait on the user to act on the board; record their chat answer on the ticket as a comment. **Work through to the end; what needs the user's eye goes to `user_review`** (user, 2026-11-24: "Finish all of hte tickets - if something needs my eye then it should go into user review"): finish everything the team can, and park only the part that needs the user's look or decision in `user_review`, with a comment saying exactly what to look at (d-030).
- `blocked` with a line saying what it waits on.

## Comments

Use comments wherever the board would otherwise leave the reader guessing (user, 2026-11-23: "use comments in the tickets where deemed necessary such as work pausing or holding"). Add one with `author.py comment <id> --author <you> --body-file <file|->`:
- **Paused or holding:** why it stopped, what it waits on (a user decision, another ticket, a running agent) and what resumes it. Every move out of `in_progress` that is not to `in_review`, `verified` or `done` gets one.
- **Resumed:** what changed that let it continue.
- **Handed over or re-scoped:** who picks it up, or what moved to another ticket (mention that ticket's id).
- **Short answers or decisions** from the user or a reviewer that affect the work.

Findings and results that belong to the ticket itself are appended to its body (`set <id> --append-body-file`), not commented. A comment is never edited: correct it with a later one.

## With agents (Mode 1 and agent teams)

| Step | Ticket action (Coordinator) |
|---|---|
| Dispatch an editor or worker | set its tickets `in_progress`; name the ticket ids in the brief |
| Editor reports done, build passes | set `in_review`; dispatch the read-only reviewer (`ADVISOR: FORBIDDEN`) |
| Review passes | set `verified`; `bak.py commit`; CHANGELOG |
| Git commit | set `done --link-commit <hash>` with a line saying what verified it; commit the ticket change with the next change set |
| Review finds a new issue out of scope | file it as its own small ticket, `open` |

Agents name every ticket id they touched in their report. Reviewers edit no tickets; the Coordinator files their findings.

## Closing

- `done` needs the commit hash, so closing is a second small edit after the commit.
- A task closes after its last sub-task, with the in-game or build check that covers the whole task.
- `wont_do` with a line saying why. Tickets are never deleted.

## Viewer

- Keep the viewer running and open in the in-app browser at all times (user, 2026-11-23: "keep it launched and in the in-app browser at all times").
- Check `http://127.0.0.1:18550/api/health` answers `heldenian-tickets`; if not, start `python .claude/tools/tickets/serve.py` (localhost only) and open `http://127.0.0.1:18550/` in the browser pane.
