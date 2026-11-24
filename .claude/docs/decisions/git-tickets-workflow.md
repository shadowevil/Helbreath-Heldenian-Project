# decisions: Git, tickets and how agents work

*Sidecar of `.claude/docs/decisions.md` (index, scope rule and entry format live there). Each entry is a settled user ruling with the user's exact quote.*

## d-014 Git follows the global rules

- Date: 2026-11-23.
- Quote: "Remove the git restriction from the project and fall back to the global rules".
- Decided: the project's "agents never run git" rule is gone; the global git rules apply.
- Recorded in: CLAUDE.md "Workflow"; `~/.claude/CLAUDE.md` "Safety".

## d-015 Original-Helbreath is the Helbreath source of truth

- Date: 2026-11-23.
- Quote: "Everything in there can answer just about anything regarding Helbreath".
- Decided: the Original-Helbreath GitHub organization is the source of truth for original game behaviour, protocol, data and formulas.
- Recorded in: CLAUDE.md "Helbreath source of truth"; `sources/README.md`.

## d-022 Commit to Development only; no push until the user says so

- Date: 2026-11-23.
- Quote: "From now on only push to Development - don't push to origin until I say so".
- Decided: work is committed locally on the `Development` branch only; nothing is pushed to `origin` until the user explicitly says so.
- Recorded in: CLAUDE.md "Workflow".

## d-023 Adopt the ticket system

- Date: 2026-11-23.
- Quote: "I want you to also adopt the ticketing system from @[ticket-master]"; selected answers "HH- / Heldenian", ".claude/tools/tickets", areas "client, server, shared, database, tooling, docs".
- Decided: ticket system 1.5.4 with prefix `HH-`, board "Heldenian Tickets" on port 18550, CLI in `.claude/tools/tickets`.
- Recorded in: `.claude/tickets/config.json`; `.claude/docs/tickets.md`; `.claude/skills/tickets/SKILL.md`.

## d-025 Work is tracked in small tickets, as tasks and sub-tasks

- Date: 2026-11-23.
- Quotes: "Make sure we are filing tickets as we work, as the tickets are how we keep track of our work. Tickets should be small and multiple tickets for larger work"; "Broken into tasks/sub-tasks".
- Decided: every piece of work gets a ticket when it starts; larger work is a task ticket whose small sub-task tickets block it.
- Added the same day: "Only put tickets in progress if an agent is actively working on them. A master ticket can remain in in-progress as long as agents are actively working on sub tickets. But only put sub tickets in progress if an agent is working on them". A sub-task is `in_progress` only while an agent works it; otherwise it stays `open`.
- Added the same day: "use comments in the tickets where deemed necessary such as work pausing or holding". Pauses, holds, resumes and hand-overs get a ticket comment.
- Added the same day: "I also cannot interact with the ticket board, only view. So all questions or issues should come to me". Clarified: "Well issues that request me". Questions and issues that need the user go to them in chat; their answers are recorded on the ticket. Other issues are only tracked on the board.

## d-030 Finish the tickets; park what needs the user in user_review

- Date: 2026-11-24.
- Quote: "Finish all of hte tickets - if something needs my eye then it should go into user review".
- Decided: agents finish every ticket they can without the user. A ticket, or the part of one, that needs the user's look or decision moves to `user_review`, with a comment saying what to look at. The work does not stop to wait for the user.
- Recorded in: `.claude/docs/ticket-workflow.md` "Status".
