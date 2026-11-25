"""Build the runtime HUD (action bar) kit the client draws the bottom panel from (HH-0068).

Usage:
  python tools/panel_kit/build_hud_kit.py [--out <dir>] [--sheet <png>] [--check] [--dry-run]

Reads the user's art sheet (read only):
  working/sprite_work/GameDialog/gamedialog_sprite_6.png   bar, EXP fill, gauge troughs and fills, message frame,
                                                           crusade icons, combat orbs, hover ring, socket, menu buttons
Writes into --out (default staged/client/sprites/hudkit/), names lowercase:
  hud_kit.png    one atlas: every part copied pixel for pixel (tight alpha box) onto a 2 px padded sheet
  hud_kit.json   frames {name: {image, x, y, w, h, use, source}}, layout (where each part sits on the 800x53 bar,
                 bar-relative: screen x, HUD top + y) and the gauge rule's offsets (fill inside trough, cap widths)

Gauge rule (spec .claude/design/plans/hud-kit-runtime.md, user 2026-11-24: "A, hard cut"):
  trough -> fill clipped to value / max with a straight edge -> trough caps over the fill ends.
  HP, MP, SP and EXP are anchored left and deplete to the left; hunger is anchored at the bottom.
The sheet's dark third menu row is the disabled state, which the game does not use (user): not shipped.

--check, --dry-run, the .tmp-then-rename writes and the exit codes are those of build_panel_kit.py (whose helpers
this reuses): --check builds in memory and compares byte for byte with --out (exit 0 identical, 1 differs);
exit 2 for a missing input or a build error. Output is deterministic. Console text is ASCII. Needs Pillow.
"""
import argparse
import hashlib
import io
import json
import os
import sys
from pathlib import Path

from PIL import Image

from build_panel_kit import REPO, pack, pixels_equal, repo_rel, tight, under_working

ATLAS_NAME = "hud_kit.png"
JSON_NAME = "hud_kit.json"
SHEET_NOTE = "the art sheet lives in working/sprite_work/GameDialog (gitignored, on the author's machine)"

# name: (search rect x, y, w, h on the sheet, use). The stored rect is the tight alpha box inside it.
PARTS = {
	"bar": (0, 135, 800, 53, "800x53 leather action bar, plain; its top 4 rows are the EXP trough trim"),
	"exp_fill": (0, 130, 800, 4, "green EXP fill 800x4, drawn over the bar's top trim, clipped from the left"),
	"mode_eye_lit": (4, 4, 34, 42, "crusade duty icon, Elvine eye, hover"),
	"mode_eye": (40, 4, 34, 42, "crusade duty icon, Elvine eye"),
	"mode_cross_lit": (76, 4, 34, 42, "crusade duty icon, Aresden cross, hover"),
	"mode_cross": (112, 4, 34, 42, "crusade duty icon, Aresden cross"),
	"tube_trough": (2, 54, 128, 20, "HP/MP glass trough: gold caps x 0-12 and 115-127, glass x 13-114"),
	"tube_fill_hp": (13, 74, 106, 15, "HP fill, red glass; at trough + fill_offset, clipped from the left"),
	"tube_fill_mp": (13, 89, 106, 15, "MP fill, blue glass; at trough + fill_offset, clipped from the left"),
	"hunger_fill": (123, 77, 7, 35, "hunger fill, orange, vertical; clipped from the top (anchored at the bottom)"),
	"hunger_trough": (131, 73, 7, 43, "hunger trough, white glass with 4 px gold caps top and bottom"),
	"sp_fill": (144, 54, 168, 9, "SP fill, green; at trough + fill_offset, clipped from the left"),
	"sp_trough": (142, 65, 172, 13, "SP trough, grey"),
	"msg_frame": (141, 79, 174, 31, "map-name / coordinates box, open centre"),
	"orb_peace": (540, 2, 36, 35, "combat orb, dark grey sword: peace mode"),
	"orb_peace_hover": (578, 2, 36, 35, "combat orb, light grey sword: peace mode, hovered"),
	"orb_attack": (616, 2, 36, 35, "combat orb, red sword: attack mode"),
	"orb_safe": (654, 2, 36, 35, "combat orb, gold sword: safe attack mode"),
	"orb_super": (692, 2, 36, 36, "purple lightning orb: super attack ready while ALT is held, additive 0.7"),
	"ring_hover": (539, 38, 39, 38, "white ring, open centre: combat button hover"),
	"socket": (582, 38, 43, 42, "brown square socket behind the combat orb"),
}
MENU = ["character", "inventory", "magic", "skill", "chat_log", "system"]
for _i, _n in enumerate(MENU):
	PARTS["menu_%s_normal" % _n] = (320 + 37 * _i, 1, 33, 41, "%s menu button, normal" % _n)
	PARTS["menu_%s_hover" % _n] = (320 + 37 * _i, 44, 33, 41, "%s menu button, hover" % _n)

# Bar-relative positions (x from the bar's left, y from its top), matched to today's baked art at 800x600
# (.claude/scratch/panel_kit/hud/placements.json, approved with the mockup). Orbs, the ring and orb_super centre
# on the socket.
LAYOUT = {
	"note": "bar-relative: screen (x, hud_top + y); the bar itself at (0, 0)",
	"bar": [0, 0],
	"exp_fill": [0, 0],
	"msg_frame": [227, 20],
	"tube_hp": [90, 8],
	"tube_mp": [90, 30],
	"sp_trough": [229, 7],
	"hunger_trough": [219, 7],
	"socket": [442, 7],
	"crusade": [405, 8],
	"menu": [493, 8],
	"menu_step": 37,
	"menu_order": MENU,
}
GAUGES = {
	"rule": "trough, then the fill clipped to value/max with a hard cut, then the trough caps over the fill ends",
	"tube": {"fill_offset": [11, 2], "cap_left": 13, "cap_right": 13},
	"sp": {"fill_offset": [2, 2]},
	"hunger": {"fill_offset": [0, 4], "cap": 4},
}


def build(sheet_path):
	"""Returns {file name: bytes}."""
	sheet = Image.open(sheet_path).convert("RGBA")
	pieces = {}
	sources = {}
	for name, (x, y, w, h, use) in PARTS.items():
		tx, ty, tw, th = tight(sheet, x, y, w, h)
		pieces[name] = (sheet.crop((tx, ty, tx + tw, ty + th)), use)
		sources[name] = {"sheet": "gamedialog_sprite_6", "rect": [tx, ty, tw, th]}
	pos, size = pack({n: p[0] for n, p in pieces.items()})
	atlas = Image.new("RGBA", size, (0, 0, 0, 0))
	frames = {}
	for name, (im, use) in pieces.items():
		x, y = pos[name]
		atlas.paste(im, (x, y))
		frames[name] = {"image": ATLAS_NAME, "x": x, "y": y, "w": im.width, "h": im.height, "use": use,
						"source": sources[name]}
	png = io.BytesIO()
	atlas.save(png, "PNG", optimize=False, compress_level=9)
	doc = {
		"kit": "hud_kit",
		"version": 1,
		"ticket": "HH-0068",
		"inputs": {"gamedialog_sprite_6": {"path": repo_rel(sheet_path),
										   "sha256": hashlib.sha256(Path(sheet_path).read_bytes()).hexdigest()}},
		"frames": frames,
		"layout": LAYOUT,
		"gauges": GAUGES,
	}
	text = json.dumps(doc, indent=1, ensure_ascii=True) + "\n"
	return {ATLAS_NAME: png.getvalue(), JSON_NAME: text.encode("ascii")}


def write_pair(out, files):
	"""Each file to <name>.tmp, then rename; PNG first, JSON last, so a reader never sees a new JSON with an old atlas."""
	out.mkdir(parents=True, exist_ok=True)
	for name in (ATLAS_NAME, JSON_NAME):
		data = files[name]
		tmp = out / (name + ".tmp")
		tmp.write_bytes(data)
		os.replace(tmp, out / name)
		print("wrote %s (%d bytes, sha256 %s)" % (out / name, len(data), hashlib.sha256(data).hexdigest()))


def check(out, files):
	"""Byte-for-byte compare with the staged pair; 0 identical, 1 differs."""
	bytes_same = pixels_same = True
	for name, data in files.items():
		target = out / name
		staged = target.read_bytes() if target.is_file() else None
		new = hashlib.sha256(data).hexdigest()
		old = hashlib.sha256(staged).hexdigest() if staged is not None else "missing"
		if old == new:
			print("same %s built %s staged %s" % (name, new, old))
			continue
		bytes_same = False
		if name == ATLAS_NAME and staged is not None and pixels_equal(data, staged):
			print("DIFF %s bytes (built %s staged %s) but decoded RGBA pixels match" % (name, new, old))
		else:
			pixels_same = False
			print("DIFF %s built %s staged %s" % (name, new, old))
	if bytes_same:
		verdict = "identical"
	elif pixels_same:
		verdict = "pixels and json match, png bytes differ (re-encoded); rebuild to restage"
	else:
		verdict = "differs"
	print("check: %s (%d parts)" % (verdict, len(PARTS)))
	return 0 if bytes_same else 1


def main(argv=None):
	ap = argparse.ArgumentParser(description="Build the runtime HUD action-bar kit (HH-0068).")
	ap.add_argument("--out", default=str(REPO / "staged" / "client" / "sprites" / "hudkit"))
	ap.add_argument("--sheet", default=str(REPO / "working" / "sprite_work" / "GameDialog" / "gamedialog_sprite_6.png"))
	ap.add_argument("--check", action="store_true", help="compare a fresh build with --out byte for byte; write nothing")
	ap.add_argument("--dry-run", action="store_true", help="list parts and files; write nothing")
	a = ap.parse_args(argv)
	out = Path(a.out)
	if under_working(out):
		print("refused: --out %s is under working/, the user's art folder (read only)" % out, file=sys.stderr)
		return 2
	if not os.path.isfile(a.sheet):
		print("missing input: %s - %s" % (a.sheet, SHEET_NOTE), file=sys.stderr)
		return 2
	try:
		files = build(a.sheet)
	except Exception as e:  # corrupt sheet, bad rect, ...: one line, exit 2
		print("build failed: %s: %s - %s" % (type(e).__name__, e, SHEET_NOTE), file=sys.stderr)
		return 2
	if a.check:
		return check(out, files)
	if a.dry_run:
		for n, f in json.loads(files[JSON_NAME])["frames"].items():
			print("%-24s %4d,%4d %3dx%3d" % (n, f["x"], f["y"], f["w"], f["h"]))
		for name, data in files.items():
			print("would write %s (%d bytes, sha256 %s)" % (out / name, len(data), hashlib.sha256(data).hexdigest()))
		return 0
	write_pair(out, files)
	return 0


if __name__ == "__main__":
	sys.exit(main())
