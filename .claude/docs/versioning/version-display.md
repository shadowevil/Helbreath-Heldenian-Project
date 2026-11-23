# Version display

Verified against the call sites named, 2026-11-23. Not observed running: **screen output is unverified**.

Every displayed version names ONE component. There is no project version, so a surface must say which one it
shows. The Build half is shared, so `Build 41279+37580` needs no component label.

## Canonical forms (from `version.cpp`)

```text
DEV   0.2.52-alpha:DEV.41279+37580
TEST  0.2.52-alpha:TEST.41279+37580
PROD  0.2.52-alpha:41279+37580           (PROD omits the label)
Player lines:  Version 0.2.52-alpha   /   Build 41279+37580
```

API: `version_string(component)`, `release_version(component)`, `player_version_line(component)`,
`player_build_line()`, `build_id()`, `commit_sha()`, `source_clean()`. Every release accessor takes the
component it means; there is no no-argument form, deliberately.

DEV and TEST builds must expose their environment so they cannot be mistaken for production.

## Surfaces

| Surface | Shows | Where |
|---|---|---|
| client window title | `Helbreath <client version_string>` | `client/game.cpp` (`set_title`) |
| client stamp (menu/overlays) | `client <client version_string>` | `client/game.cpp` (stamp text, `G_cTxt`) |
| client log, first line | `client <version_string>` | `client/game.cpp` after `hb::logger::initialize` |
| server console banner | Version: server string; Build: `H+L (clean|dirty, 8-char commit)`; SQL: release | `server/wmain.cpp` |
| server log, first line | `Server <server version> (sql <sql release>, clean|dirty tree, commit <8>)` | `server/wmain.cpp` |
| in-game `/version` | `Helbreath server <server string>, sql <sql release>` | `server/game.cpp` `show_version` |
| exe file properties | client / server release | `gen_version_info.py` -> `version_info.rc` |
| version-mismatch overlay | client version string | `Overlay_VersionNotMatch` (**unverified** it was updated) |

Shared must not log (CLAUDE.md Logging), so version log lines live in Client and Server only.

When two components appear together, label them: `client 0.2.52-alpha:DEV.41279+37580` and
`sql 0.0.1-alpha`.

The `draw_version` stamp: the plan called for one fixed stamp at 80% opacity on every overlay. Whether the six
overlays now share one draw is **unverified**; check `client/game.cpp` (stamp draw near `G_cTxt`) before
claiming it.
