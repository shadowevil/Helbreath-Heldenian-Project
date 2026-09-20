# Changelog Format Guide

Two changelog formats are used: an internal dev log (`CHANGELOG.md`) and a Discord post for players.

---

## Internal Changelog (`CHANGELOG.md`)

After every `bak.py commit`, append a short summary to the bottom of `CHANGELOG.md`. One or two sentences describing what was done -- no category headers, no bullet lists, no code references. Write it like a brief commit message.

The `#` header at the top of `CHANGELOG.md` should be a short summarization of all the entries -- updated as needed to reflect the current contents.

### Example

```markdown
# Dialog system fixes and dead code cleanup

Fixed HudPanel right-click disabling bug and migrated right-click-close settings from dead init_defaults() into individual dialog constructors.
```

### Rules

- Short and sweet -- a general summarization, not a detailed breakdown
- No file paths, struct names, or code references
- Append new entries to the bottom (oldest first, newest last)
- The `#` header summarizes the overall changelog contents
- If the log is empty, treat it as only the most recent thing done
- Before writing, present an interactive multi-select prompt listing generalized items from the session. Include an "All" option. Only include items the user selects.

---

## Discord Changelog (`CHANGELOG.md` -- when user requests a Discord post)

Written for end-users, not developers. Non-technical, player-facing language. Written directly into `CHANGELOG.md`, replacing the internal format.

### Format

```
## ========================== Helbreath The Heldenian Project vX.Y.Z - Update (Mon DD) ==========================
Update required (or omit if not required)

### Category Name
- Change description in plain, player-friendly language
- Another change

### Another Category
- Change description
```

### Rules

- **ASCII only** -- use `-` (hyphen-minus, U+002D) for dashes, never Unicode em/en dashes
- **No emojis**
- **Title** uses `## ` (h2) with `=` padding on both sides, version, and date
- **Categories** use `### ` (h3) headings (e.g. `### Server`, `### Bug Fixes`, `### Visuals`)
- **"Update required"** line goes immediately after the title when clients need to update; omit otherwise
- **No code references** -- no file paths, function names, struct names, config keys
- Each item is a `- ` bullet, one line, written from the player's perspective
- Sub-items use `  - ` (two-space indent) for grouped details under a parent bullet
- Keep descriptions concise but informative -- explain what changed, not how
- Before writing, present an interactive multi-select prompt listing generalized items from the session. Include an "All" option. Only include items the user selects.

### Example

```
## ========================== Helbreath The Heldenian Project v0.2.11 - Update (Feb 24) ==========================
Update required

### Server
- Introduced a new expression-based formula engine - game formulas like weapon attack speed are now configurable in the database instead of being hardcoded
- Formulas are human-readable strings that can reference each other, and the server validates all of them on startup before accepting players
- Redesigned the server console with color-coded log output, grouped startup summaries, and a refreshed ASCII banner

### Bug Fixes
- Fixed armor break not properly reducing endurance for other players
- Restored the missing black fog visual effect when armor breaks
- Fixed an issue where arrows could be sold when they shouldn't be
- Fixed stats dialog alignment being off
- Fixed create food crashing the client
- Fixed player corpses disappearing before players could revive or restart
- Dead player corpses now display hover names when moused over
- Enabled the Pretend-Corpse skill to be activated directly from the skill dialog
- Improved click and drag detection so actions feel more responsive and less prone to misfires
- Widened the skill list click area and centered the skill-use status text on screen
- Fixed 27 guild-related bugs:
  - Guild name display and caching issues
  - Kennedy NPC interaction problems
  - Item transfer edge cases
  - Sell dialog showing double count prefixes
  - Unified maximum guild members to 128

### Visuals
- Added a Christmas holiday tile variant that swaps in automatically when the server enables seasonal weather
- Fixed gold not appearing immediately when dropped - it no longer plays invisible animation frames first
- Added a new animated ping-pong cursor effect when targeting items
- Added scrolling support to the graphics settings tab in the System Menu, with a custom scrollbar and mouse wheel support

### Performance
- Sprites now load their textures on demand when first drawn rather than all at once
- Unused sprite textures automatically release from memory after 30 seconds of inactivity, reducing overall memory footprint

### Platform
- Fixed Linux build compatibility issues with the server and client
```
