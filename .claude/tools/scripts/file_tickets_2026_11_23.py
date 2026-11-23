"""One-off: file the 2026-11-23 work as tasks and sub-tasks on the HH- board.

A task is a parent ticket; each sub-task blocks its parent, so the parent closes when its
sub-tasks are done. Every ticket goes through author.py draft, then author.py new.
Prints ASCII only.
"""
import json
import subprocess
import sys

REPO = r"C:\Users\ShadowEvil\source\Helbreath-Heldenian-Project"
AUTHOR = [sys.executable, REPO + r"\.claude\tools\tickets\author.py"]
PLAN = ".claude/design/plans/world-screen-split.md"


def run(cmd, data):
	p = subprocess.run(AUTHOR + cmd + ["--from-json", "-", "--json"], input=json.dumps(data),
		capture_output=True, text=True, cwd=REPO)
	if p.returncode != 0:
		print("FAILED", cmd[0], data["summary"])
		print(p.stdout[-800:])
		print(p.stderr[-800:])
		sys.exit(1)
	return json.loads(p.stdout)


def file(data):
	run(["draft"], data)
	out = run(["new"], data)
	tid = out.get("id") or out.get("ticket", {}).get("id")
	print("created", tid, "-", data["summary"])
	return tid


def body(text, done_when, source):
	return text + "\n\n**Done when:** " + done_when + ".\n\nSource: " + source + "."


def ticket(summary, sev, owner, tags, status, text, done_when, source, **links):
	data = {"summary": summary, "severity": sev, "owner": owner, "author": "pm", "status": status,
		"tags": tags, "body": body(text, done_when, source)}
	if links:
		data["links"] = links
	return data


SRC = "user request 2026-11-23; plan " + PLAN

# ---- Already landed today (commit 9cfd510) ----
for s, t, dw in [
	("Speed up screen transitions 50%", "Screen fades run 1.5x faster via `transition_speed` in `sources/client/game_mode_manager.h` (0.15/0.25/0.5 s -> 0.10/0.17/0.33 s, derived).", "fades shorter in game; reviewed, built Debug 0 errors"),
	("Show the splash in Debug and let a click skip it after 1 s", "`sources/client/game.cpp` always starts on `Screen_Splash`; `screen_splash.cpp` skips to loading on a left-click press once 1 s has elapsed (all builds).", "splash shows in Debug and skips on click after 1 s; reviewed, built Debug 0 errors"),
	("Ship only Wine Tahoma and refuse startup without it", "Removed arial, segoeui, MS tahoma and default.ttf from `staged/client/fonts/`; ship `wine_tahoma.ttf` (LGPL 2.1, licence beside it); no system-font lookup (`LoadFontByName` removed); startup refused with \"Unable to load default font\" when it is missing.", "deployed fonts folder holds only Wine Tahoma; reviewed, built Debug 0 errors"),
]:
	data = ticket(s, "low", "client", ["client", "feature"], "done", t, dw, "user request 2026-11-23, commit 9cfd510")
	data["links"] = {"commits": ["9cfd510"]}
	file(data)

# ---- Task: world/screen space split ----
parent = file(ticket(
	"World/screen space split (task)", "high", "client", ["client", "feature"], "in_progress",
	"The world keeps rendering at 800x600 and is upscaled to the window; UI, floating text and every non-game screen render in screen space at window resolution, with a fixed draw order. Today everything draws into one 800x600 back buffer (`sources/sfml_engine/sfml_renderer.cpp`, CreateRenderTextures and end_frame, as of 2026-11-23). Design and the user's decisions: `" + PLAN + "` and d-024 in `.claude/docs/decisions.md`. Closes when its sub-tasks are done.",
	"every sub-task is done and the split is checked in game at 800x600, 1440x1080, fullscreen letterbox and stretch", SRC))

subs = [
	# (summary, status, text, done when)
	("Split 1a: route every draw through a renderer current target", "in_progress",
		"Replace each hard-wired `m_backBuffer` draw (primitives, `SFMLSprite::DrawInternal`, `DrawWidth`, `DrawShifted`, TTF text, bitmap glyphs) with one current target; clip and scissor per target.",
		"grep finds no direct back-buffer draw outside the world target code; Debug build 0 errors; reviewed"),
	("Split 1b: begin_world / end_world composite and UI view", "in_progress",
		"New renderer API: `begin_world`, `end_world` (composites the 800x600 world target in the world rect, stretch or letterbox), UI `sf::View` for 800x600 UI units, `fill_output`, `ui_scale`. In-game calls begin_world before the world and end_world before `DialogBoxManager::draw_all`; other screens draw in screen space only.",
		"in-game and every other screen render correctly through the new path; reviewed"),
	("Split 1c: fades, overlay dim, cursor and performance monitor in screen space", "in_progress",
		"Screen and teleport fades and the overlay dim use `fill_output` (dim rect was one pixel short); the software cursor and performance monitor draw last in screen space; screenshot captures the window.",
		"fades cover the whole window, cursor is sharp at 1440x1080; reviewed"),
	("Split 2: native-resolution TTF text in the screen pass", "open",
		"Screen-pass TTF rasterises at logical size times `ui_scale` and draws at the inverse scale; measuring, wrapping and layout stay in 800x600 units; one shared baseline snapped to window pixels.",
		"text is sharp at 1440x1080 and wraps exactly as at 800x600; reviewed"),
	("Split 3a: queue world-anchored overlays and replay them after the composite", "open",
		"Names (`render_helpers.cpp`), chat bubbles (`CGame::draw_character_body`), damage numbers (`floating_text.draw_all`) and foe icons are queued during the world pass and replayed in screen space above the world, including above the late world effects (user decision).",
		"overlays stay attached to sprites while moving and draw above all world effects; reviewed"),
	("Split 3b: move the item tooltip out of draw_objects", "open",
		"`screen_on_game_draw_objects.cpp` draws an item tooltip inside the world pass; move it to the screen-pass tooltip layer. Crusade markers stay in the world.",
		"tooltip draws in screen space at the cursor; reviewed"),
	("Split 3c: HUD action bar spans the world rect", "open",
		"User decision: the action bar always takes the same share of the screen as at 800x600, so it draws in the world rect even when the rest of the UI is uniform and centred.",
		"in fullscreen stretch on a non-4:3 screen the bar spans the full width; reviewed"),
	("Split 4a: uniform UI rect in stretch mode and mouse mapping", "open",
		"In stretch mode the world fills the window while the UI keeps a uniform, centred scale. Mouse maps window -> UI units through the UI rect, and world picking maps through the world rect; `TransformMouseCoords` and `update_cursor_clip` share one rect helper.",
		"clicks hit dialogs and world targets correctly in letterbox and stretch; reviewed"),
	("Split 5: verify the split on Windows and WSL builds and in game", "open",
		"Build Debug and Release on Windows and the WSL client; manual check at 800x600, 1440x1080, fullscreen letterbox and fullscreen stretch.",
		"all builds pass and the in-game check is recorded on this ticket"),
]
for s, st, t, dw in subs:
	data = ticket(s, "medium", "client", ["client", "feature"], st, t, dw, SRC)
	data["links"] = {}
	tid = file(data)
	subprocess.run(AUTHOR[:1] + [REPO + r"\.claude\tools\tickets\tickets.py", "set", tid, "--block", parent],
		check=True, cwd=REPO, capture_output=True)

# ---- Task: korea-2024 reference art ----
kparent = file(ticket(
	"Extract korea-2024 sprites as PNG reference art (task)", "low", "tooling", ["tooling", "feature"], "in_progress",
	"Download every .pak in Original-Helbreath/History `sprites/korea-2024` (405 files, about 890 MB, measured via the GitHub API 2026-11-23) and convert each sprite sheet to `classic_ui/reference/<pak>/<nnn>.png` with a `frames.json`. Downloads in `build/downloads/korea-2024/`; output gitignored. Closes when its sub-tasks are done.",
	"every parsable pak is converted and spot-checked", "user request 2026-11-23"))
for s, t, dw in [
	("korea-2024 1: download and size-verify the 405 paks", "Resumable download into `build/downloads/korea-2024/`, each size checked against the API.", "405 files present with matching sizes"),
	("korea-2024 2: pak-to-PNG converter", "`.claude/tools/scripts/pak_to_png.py`, built from the client's pak loader and cross-checked with Helbreath-SDL; failures logged and skipped.", "converter runs over all paks; failures listed"),
	("korea-2024 3: spot-check converted art", "View a UI, a character and an effect sheet for colour, transparency and layout.", "three sheets look correct"),
]:
	tid = file(ticket(s, "low", "tooling", ["tooling", "feature"], "in_progress", t, dw, "user request 2026-11-23"))
	subprocess.run(AUTHOR[:1] + [REPO + r"\.claude\tools\tickets\tickets.py", "set", tid, "--block", kparent],
		check=True, cwd=REPO, capture_output=True)

print("done")
