# Agent Teams Workflow

*Assessed 2026-11-23 (docs standard sect. 1): kept whole, about 9 KB, under the 10 KB review line. Structure, roles, communication and phases presuppose each other and are read together before a team run; consulted rarely. Per-role briefs are in `.claude/agents/`.*

Contents: Team Structure - Roles - Communication Rules - Workflow Phases - Error Recovery - Hard Constraints.

Team structure, communication rules, and coordination patterns for parallel agent work. Use teams when a task has independent workstreams (client + server, research + implementation, multi-subsystem refactors).

## Team Structure

```
                         ┌──────────────┐
                         │  Coordinator  │
                         │    (Lead)     │
                         └───────┬───────┘
                    ┌────────────┼────────────┐
                    ▼            ▼             ▼
             ┌────────────┐  shared/   ┌────────────┐
             │ Server Wing│  (owned    │ Client Wing│
             └─────┬──────┘  by one)   └─────┬──────┘
              ┌────┴────┐                ┌────┴────┐
              ▼         ▼                ▼         ▼
         ┌─────────┐ ┌────────┐    ┌─────────┐ ┌────────┐
         │Research  │ │ Editor │    │Research  │ │ Editor │
         │(2-3 max)│ │  (1)   │    │(2-3 max)│ │  (1)   │
         └─────────┘ └────────┘    └─────────┘ └────────┘
```

Max 5-8 agents total. More than that thrashes context with no benefit.

## Roles

### Coordinator (exactly 1) - always the main session

The single authority. Decomposes the task, assigns all work, and owns the build/commit cycle.
The Coordinator is always the main session, never a spawned agent (user ruling, 2026-11-23), because
the global rule is that only the main session dispatches and agents never spawn agents. A teammate that
needs another agent asks the Coordinator.

Dispatch rules (global, `~/.claude/CLAUDE.md` "When dispatching agents"; restated here because they
bind every brief the Coordinator writes):
- The first line of every brief is an advisor line: `ADVISOR: ALLOWED`, `ADVISOR: FORBIDDEN` or `ADVISOR: ASK PM`.
  Review agents always get `ADVISOR: FORBIDDEN`, and the Coordinator never calls the advisor itself.
- Every brief states the model and why, using the cheapest model that does the job well. Haiku for fully
  specified edits and lookups, Sonnet by default, Opus for design, hard debugging and security-sensitive code.
- Briefs are self-contained: intent, goal, where (verified / located / guess), constraints, done-when,
  report shape.

Exclusive responsibilities (no other agent may do these):
- Run builds (`build.ps1` / `build_linux.sh`)
- Run `bak.py commit`
- Write to `CHANGELOG.md`
- Maintain `session_context.md` (task summary, decisions, constraints)
- Shut down agents when work is complete

Workflow responsibilities:
- Decompose task into sub-tasks (TaskCreate)
- Assign work to editors and research agents
- Build only when **all agents are idle/stopped**
- Diagnose build failures and re-assign fixes
- Manage task list (TaskCreate, TaskUpdate, TaskList)

### Research Agents (2-3 per wing)

Read-only investigators. Use **Explore subagent type** — they cannot edit files.

- Investigate subsystems, trace packet chains, read handlers, search patterns
- Report findings to **Coordinator** as structured summaries
- Can exchange information with their wing's **Editor** (bidirectional)
- Never communicate across wings directly
- Named: `server-research-1`, `server-research-2`, `client-research-1`, etc.

### Editor Agents (1 per wing)

The only agents that modify code. Each wing has exactly one editor to prevent file conflicts.

- **Takes orders from Coordinator only** — never self-directs work
- Guards files with `bak.py guard` before any edit
- Makes code changes within their wing's directory
- Can request information from their wing's Research Agents
- Reports completion to Coordinator with: files changed, summary of changes
- Never runs builds, never commits, never writes changelog
- Named: `server-editor`, `client-editor`

### shared/ Ownership

`sources/dependencies/shared/` affects both client and server. Special rules:
- Coordinator designates **exactly one agent** to edit Shared (typically one of the editors, or coordinator directly)
- Never have both editors touching Shared simultaneously
- The designated agent must coordinate with both wings since changes propagate to both

## Communication Rules

### Allowed

```
Coordinator  ──→  Any agent          (directives, assignments)
Any agent    ──→  Coordinator        (findings, completion reports, questions)
Research     ←→   Editor             (same wing only — info exchange)
```

### Forbidden

```
Editor       ←→   Editor             (no cross-wing editor communication)
Research     ←→   Research           (no cross-wing research communication)
Editor       ──→  self               (no self-directed work)
```

Editors that need information from the other wing route through the Coordinator.

### Message Content

- **Research → Coordinator**: Structured findings (what, where, implications, risks)
- **Editor → Coordinator**: Completion report (files changed, what was done, any concerns)
- **Coordinator → Editor**: Clear directives (which files, what changes, acceptance criteria)
- **Research ↔ Editor**: Targeted Q&A (specific function behavior, data flow, handler side effects)

## Workflow Phases

### Phase 1: Research

1. Coordinator analyzes the task and identifies what needs investigation
2. Coordinator spawns Research Agents with specific search focuses
3. Research agents investigate in parallel, report findings to Coordinator
4. Coordinator synthesizes findings and creates the task list

### Phase 2: Implementation

1. Coordinator assigns specific file edits to Editor Agents
2. Each Editor runs `bak.py guard` on their files
3. Editors make changes, consulting Research Agents as needed for clarification
4. Each Editor runs `/simplify` on their changed code to check for reuse, quality, and efficiency issues
5. For refactors, each Editor runs `git diff` on changed files to verify no logic was lost or altered
6. Editors report completion to Coordinator with change summaries
7. Research Agents remain available for follow-up questions

### Phase 3: Build & Verify

1. Coordinator confirms **all agents are idle**
2. Coordinator runs build (`-Target All` for both, or `-Target Game`/`Server` individually)
3. **If build fails**: Coordinator diagnoses the error, assigns targeted fixes to the appropriate Editor
4. **If build succeeds**: Coordinator dispatches a separate read-only review agent (`ADVISOR: FORBIDDEN`)
   with the diff and the goal, not the editors' reasoning. Fix what it finds, then rebuild.
5. On a passing review: Coordinator runs `bak.py commit`
6. Coordinator updates `CHANGELOG.md`

### Phase 4: Cleanup

1. Coordinator shuts down all agents (SendMessage with `shutdown_request`)
2. Coordinator deletes the team (TeamDelete)

## Error Recovery

- **Build failure**: Coordinator reads the build log, identifies which wing caused it, assigns the fix to that wing's Editor. Never have an Editor self-diagnose a build failure.
- **Research dead-end**: Research Agent reports to Coordinator what was tried and what wasn't found. Coordinator redirects or spawns additional investigation.
- **Editor blocked**: Editor reports the blocker to Coordinator. Coordinator may spawn a Research Agent to investigate, or reassign the task.
- **File conflict detected**: Coordinator immediately stops both Editors, resolves ownership, then resumes.

## Hard Constraints

| Rule | Reason |
|------|--------|
| One build at a time | MSBuild is single-process |
| One agent per file | Prevents edit conflicts |
| Git per the global rules | Local commits and working-tree git are allowed; push/PR only on the user's ask; never pull, reset --hard, force or rewrite history (user ruling, 2026-11-23) |
| Guard before every edit | `bak.py guard` required for rollback safety |
| shared/ is single-owner | Crosses both wings, must be coordinated |
| Research agents are read-only | Explore subagent type only |
| Coordinator builds when all idle | Ensures no partial changes |
| Coordinator is the main session; only it spawns | Global rule: agents never spawn agents |
| Review before `bak.py commit` | Global rule: code changes get a separate read-only review |
| Coordinator-only commit | Single point of truth for accepting changes |
| Coordinator-only changelog | Consistent, accurate summaries |
