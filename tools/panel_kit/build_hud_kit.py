"""Build the runtime HUD (action bar) kit the client draws the bottom panel from (HH-0068, HD since HH-0100).

Usage:
  python tools/panel_kit/build_hud_kit.py [--out <dir>] [--sheet <png>] [--check] [--dry-run]

Reads the user's HD art sheet (read only):
  working/classic_ui/hd/gamedialog_sprite_6.png   1920x451, a 2.4x upscale of the original 800x188 sheet: bar, EXP
                                                  fill, gauge troughs and fills, message frame, crusade icons, combat
                                                  orbs, hover ring, socket, menu buttons
Writes into --out (default staged/client/sprites/hudkit/), names lowercase:
  hud_kit.png    one atlas: every part copied from the HD sheet onto an 8 px padded sheet (alpha and visible pixels
                 unchanged; transparent pixels near colour take that colour, so no white halo), each part's edge
                 pixels extruded 4 px into its padding (the client samples it smooth with mipmaps)
  hud_kit.json   frames {name: {image, x, y, w, h (atlas px), size [w, h] (game px), use, source}}, layout (where
                 each part sits on the 800x53 bar, bar-relative in game px: screen x, HUD top + y) and the gauge
                 rule's offsets (fill inside trough, cap widths, game px)

The HUD covers the same 800x53 game pixels at every screen size (user, 2026-11-25: the world is 800x600 and "should
cover the same amount whether at 4k or 800x600"); the HD atlas only adds resolution. The client draws each frame into
its game-px size, so the HD frames replace the old native atlas at every scale, 1x included (user, 2026-11-25: "We
should also be replacing lower quality versions if we have a hd counterpart"). The native 800x188 kit is archived in
working/archive/hudkit/ (the only copy of the original sheet's pixels).

Cutting: PARTS holds each part's rect on the original 800x188 sheet (game px, the part's size in the HUD). Its HD cut
is that rect times 2.4, then each edge snaps to the HD sheet's alpha edge when one lies within 3 px (the upscale moved
some edges by a pixel or two; parts that touch a neighbour keep the scaled edge).

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

from build_panel_kit import REPO, pixels_equal, repo_rel, under_working

ATLAS_NAME = "hud_kit.png"
JSON_NAME = "hud_kit.json"
SHEET_NOTE = "the HD art sheet lives in working/classic_ui/hd (gitignored, on the author's machine)"
SCALE = 2.4      # HD sheet px per original-sheet (game) px
SNAP = 3         # an HD edge snaps to an alpha edge at most this many px from the scaled edge
EDGE_ALPHA = 32  # a line with no alpha above this is empty (the upscale left faint rows between parts)
ATLAS_WIDTH = 2048
PAD = 8          # gap between parts on the atlas
EXTRUDE = 4      # edge pixels repeated into the gap, so smooth sampling at a part's edge never reads its neighbour
BLEED = 8        # transparent pixels within this many px of colour take that colour (see bleed)

# name: (rect x, y, w, h on the original 800x188 sheet = the part's game-px size, use)
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

# Bar-relative positions (x from the bar's left, y from its top) in game px. The spacing inside each group is the
# baked art's at 800x600 (.claude/scratch/panel_kit/hud/placements.json, approved with the mockup); the groups sit
# flush to the bar's ends, MARGIN px in to clear the corner rivets (outer 8 px), free space in the middle (user,
# 2026-11-25: "split right after the stamina and hud info display"). DialogBox_HudPanel's hit rects and text use
# the same shifts (left_group_dx, right_group_dx). Orbs, the ring and orb_super centre on the socket.
MARGIN = 12
LEFT_DX = MARGIN - 90             # left group: tubes, hunger, SP, message frame; leftmost = the tubes at x 90
RIGHT_DX = (800 - MARGIN) - 711   # right group: crusade icon, combat socket, menu; rightmost = the menu's end, x 711
INFO_DX = 8                       # SP trough and message frame, a further shift right to give the hunger tube room
                                  # (user, 2026-11-25, HH-0102); 8, not the sketch's ~7, leaves a
                                  # 17 px gap in which the 7 px hunger tube centres exactly
# The hunger tube centred between the HP/MP tubes' right end and the shifted message frame's left edge (user,
# 2026-11-25: "make sure the hunger bar is horizontally centered"): (90 + 128 .. 227 + INFO_DX) + LEFT_DX, width 7
HUNGER_X = (90 + 128 + 227 + INFO_DX - 7) // 2 + LEFT_DX
LAYOUT = {
	"note": "bar-relative: screen (x, hud_top + y); the bar itself at (0, 0)",
	"bar": [0, 0],
	"exp_fill": [0, 0],
	"msg_frame": [227 + LEFT_DX + INFO_DX, 20],
	"tube_hp": [90 + LEFT_DX, 8],
	"tube_mp": [90 + LEFT_DX, 30],
	"sp_trough": [229 + LEFT_DX + INFO_DX, 7],
	"hunger_trough": [HUNGER_X, 7],
	"socket": [442 + RIGHT_DX, 7],
	"crusade": [405 + RIGHT_DX, 8],
	"menu": [493 + RIGHT_DX, 8],
	"menu_step": 37,
	"menu_order": MENU,
}
GAUGES = {
	"rule": "trough, then the fill clipped to value/max with a hard cut, then the trough caps over the fill ends",
	"tube": {"fill_offset": [11, 2], "cap_left": 13, "cap_right": 13},
	"sp": {"fill_offset": [2, 2]},
	"hunger": {"fill_offset": [0, 4], "cap": 4},
}


def snap_edges(alpha, x0, y0, x1, y1):
	"""Moves each edge of the HD box [x0, x1) x [y0, y1) to the nearest alpha edge within SNAP px, if any."""
	w, h = alpha.size
	px = alpha.load()

	def col_has(x, ya, yb):
		return 0 <= x < w and any(px[x, y] > EDGE_ALPHA for y in range(max(ya, 0), min(yb, h)))

	def row_has(y, xa, xb):
		return 0 <= y < h and any(px[x, y] > EDGE_ALPHA for x in range(max(xa, 0), min(xb, w)))

	def nearest(edge, filled, start):
		# start edge: first filled line after an empty one; end edge: first empty line after a filled one
		best = None
		for e in range(edge - SNAP, edge + SNAP + 1):
			inside, outside = (e, e - 1) if start else (e - 1, e)
			if filled(inside) and not filled(outside) and (best is None or abs(e - edge) < abs(best - edge)):
				best = e
		return edge if best is None else best

	# Test lines over the box's interior, 2 px in from the other axis' edges, so a neighbour's corner is not read
	col = lambda x: col_has(x, y0 + 2, y1 - 2)
	row = lambda y: row_has(y, x0 + 2, x1 - 2)
	return nearest(x0, col, True), nearest(y0, row, True), nearest(x1, col, False), nearest(y1, row, False)


def bleed(im):
	"""Gives near-transparent pixels the average colour of their coloured neighbours, BLEED px deep (alpha kept).

	The sheet's transparent pixels are white; smooth, mipmapped sampling would mix that white into every alpha
	edge as a light halo. Returns a new image."""
	w, h = im.size
	px = list(im.get_flattened_data() if hasattr(im, "get_flattened_data") else im.getdata())
	known = [p[3] > EDGE_ALPHA for p in px]   # the upscale's faint white specks (alpha 1-3) must not seed
	for _ in range(BLEED):
		grown = []
		for i, p in enumerate(px):
			if known[i]:
				continue
			x, y = i % w, i // w
			r = g = b = n = 0
			for nx, ny in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1), (x - 1, y - 1), (x + 1, y - 1),
						   (x - 1, y + 1), (x + 1, y + 1)):
				if 0 <= nx < w and 0 <= ny < h and known[ny * w + nx]:
					q = px[ny * w + nx]
					r, g, b, n = r + q[0], g + q[1], b + q[2], n + 1
			if n:
				grown.append((i, (r // n, g // n, b // n, p[3])))
		if not grown:
			break
		for i, q in grown:
			px[i] = q
			known[i] = True
	out = Image.new("RGBA", im.size)
	out.putdata(px)
	return out


def extrude(atlas, im, x, y):
	"""Pastes im at (x, y) and repeats its edge rows, columns and corners EXTRUDE px outward."""
	w, h = im.size
	e = EXTRUDE
	atlas.paste(im, (x, y))
	atlas.paste(im.crop((0, 0, w, 1)).resize((w, e)), (x, y - e))
	atlas.paste(im.crop((0, h - 1, w, h)).resize((w, e)), (x, y + h))
	atlas.paste(im.crop((0, 0, 1, h)).resize((e, h)), (x - e, y))
	atlas.paste(im.crop((w - 1, 0, w, h)).resize((e, h)), (x + w, y))
	for cx, cy, dx, dy in ((0, 0, -e, -e), (w - 1, 0, w, -e), (0, h - 1, -e, h), (w - 1, h - 1, w, h)):
		atlas.paste(im.crop((cx, cy, cx + 1, cy + 1)).resize((e, e)), (x + dx, y + dy))


def pack(items):
	"""Shelf pack, tallest first then by name; returns {name: (x, y)} and the sheet size."""
	order = sorted(items, key=lambda n: (-items[n].height, -items[n].width, n))
	pos = {}
	x = y = PAD
	shelf_h = 0
	for n in order:
		w, h = items[n].size
		if x + w + PAD > ATLAS_WIDTH:
			x, y = PAD, y + shelf_h + PAD
			shelf_h = 0
		pos[n] = (x, y)
		x += w + PAD
		shelf_h = max(shelf_h, h)
	return pos, (ATLAS_WIDTH, y + shelf_h + PAD)


def build(sheet_path):
	"""Returns {file name: bytes}."""
	sheet = Image.open(sheet_path).convert("RGBA")
	alpha = sheet.getchannel("A")
	pieces = {}
	sources = {}
	for name, (x, y, w, h, use) in PARTS.items():
		bx0, by0, bx1, by1 = snap_edges(alpha, round(x * SCALE), round(y * SCALE),
										round((x + w) * SCALE), round((y + h) * SCALE))
		if bx0 < 0 or by0 < 0 or bx1 > sheet.width or by1 > sheet.height or bx1 <= bx0 or by1 <= by0:
			raise ValueError("%s: HD cut %r outside the %dx%d sheet" % (name, (bx0, by0, bx1, by1), *sheet.size))
		pieces[name] = (bleed(sheet.crop((bx0, by0, bx1, by1))), (w, h), use)
		sources[name] = {"sheet": "gamedialog_sprite_6", "rect": [bx0, by0, bx1 - bx0, by1 - by0], "game_rect": [x, y, w, h]}
	pos, size = pack({n: p[0] for n, p in pieces.items()})
	atlas = Image.new("RGBA", size, (0, 0, 0, 0))
	frames = {}
	for name, (im, game_size, use) in pieces.items():
		x, y = pos[name]
		extrude(atlas, im, x, y)
		frames[name] = {"image": ATLAS_NAME, "x": x, "y": y, "w": im.width, "h": im.height, "size": list(game_size),
						"use": use, "source": sources[name]}
	png = io.BytesIO()
	atlas.save(png, "PNG", optimize=False, compress_level=9)
	doc = {
		"kit": "hud_kit",
		"version": 2,
		"ticket": "HH-0100",
		"note": "frames: x, y, w, h in atlas px (HD, about 2.4 per game px); size, layout and gauges in game px",
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
	ap = argparse.ArgumentParser(description="Build the runtime HUD action-bar kit from the HD sheet (HH-0068, HH-0100).")
	ap.add_argument("--out", default=str(REPO / "staged" / "client" / "sprites" / "hudkit"))
	ap.add_argument("--sheet", default=str(REPO / "working" / "classic_ui" / "hd" / "gamedialog_sprite_6.png"))
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
			r, g = f["source"]["rect"], f["source"]["game_rect"]
			print("%-24s atlas %4d,%4d %4dx%3d  hd %4d,%3d  scaled %4d,%3d %4dx%3d" % (
				n, f["x"], f["y"], f["w"], f["h"], r[0], r[1],
				round(g[0] * SCALE), round(g[1] * SCALE),
				round((g[0] + g[2]) * SCALE) - round(g[0] * SCALE), round((g[1] + g[3]) * SCALE) - round(g[1] * SCALE)))
		for name, data in files.items():
			print("would write %s (%d bytes, sha256 %s)" % (out / name, len(data), hashlib.sha256(data).hexdigest()))
		return 0
	write_pair(out, files)
	return 0


if __name__ == "__main__":
	sys.exit(main())
