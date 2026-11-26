"""Derive the create-new-character sheet with the value-box recesses baked into the HD panels (HH-0122).

Reads the user's art (working/classic_ui/hd/character_creation_screen.png, read only) and writes the derived sheet
to staged/client/sprites/interface/front_end/character_create.png, the tracked output (same size and layout: the
1920x1080 backdrop at sheet (0,0), the eight 48x33 arrow and +/- buttons in the strip to its right). The recesses
are the select screen's (make_select_backdrop.py recess(), same numbers): the panel's own texture darkened, a dark
groove on the top/left inner edge and a light ridge on the bottom/right. Nothing is upscaled (d-035).

Layout: the original screen's elements keep their places, each old panel face mapped onto its HD face (old faces
UI px x 138.1-383.8 / 451.9-666.3, y 141.3-503.8 / 141.3-310.0; HD faces art px x 403-929 / 1071-1523, y 173-940 /
173-531). The boxes below are that mapping, rounded; screen_create_new_character.cpp draws its text and controls
from the same numbers and must agree with them: re-measure both after the user moves a panel.

Usage: python tools/front_end/make_create_backdrop.py [--dry-run]
"""
import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "working" / "classic_ui" / "hd" / "character_creation_screen.png"
DST = ROOT / "staged" / "client" / "sprites" / "interface" / "front_end" / "character_create.png"

# (name, x, y, w, h) in sheet px (= backdrop art px)
NAME_BOX = ("name", 690, 227, 231, 41)
STAT_BOX_X, STAT_BOX_W = 691, 78
STAT_BOX_Y = (586, 620, 654, 687, 721, 755)    # str, vit, dex, int, mag, chr; 30 px tall, centred on the rows
VITAL_BOX_X, VITAL_BOX_W = 1423, 79
VITAL_BOX_Y = (409, 443, 477)                   # hp, mp, sp
BOX_H = 30


def boxes():
	out = [NAME_BOX]
	out += [("stat%d" % i, STAT_BOX_X, y, STAT_BOX_W, BOX_H) for i, y in enumerate(STAT_BOX_Y)]
	out += [("vital%d" % i, VITAL_BOX_X, y, VITAL_BOX_W, BOX_H) for i, y in enumerate(VITAL_BOX_Y)]
	return out


def recess(img, x0, y0, x1, y1, face=0.84, groove=(0.70, 0.45, 0.25, 0.12), ridge=(0.50, 0.30, 0.12)):
	px = img.load()

	def blend(x, y, rgb, a):
		r, g, b, al = px[x, y]
		px[x, y] = (round(r + (rgb[0] - r) * a), round(g + (rgb[1] - g) * a), round(b + (rgb[2] - b) * a), al)

	for y in range(y0, y1):
		for x in range(x0, x1):
			r, g, b, a = px[x, y]
			px[x, y] = (round(r * face), round(g * face), round(b * face), a)
	dark, light = (20, 12, 4), (255, 244, 220)
	for i, a in enumerate(groove):   # top and left: the shadow cast into the recess
		for x in range(x0 + i, x1):
			blend(x, y0 + i, dark, a)
		for y in range(y0 + i + 1, y1):
			blend(x0 + i, y, dark, a)
	for i, a in enumerate(ridge):    # bottom and right: the lit far wall
		for x in range(x0 + i, x1 - i):
			blend(x, y1 - 1 - i, light, a)
		for y in range(y0 + i, y1 - i - 1):
			blend(x1 - 1 - i, y, light, a)


def main():
	if not SRC.is_file():
		print(f"error: source art not found: {SRC}")
		return 1
	img = Image.open(SRC).convert("RGBA")
	for name, x, y, w, h in boxes():
		recess(img, x, y, x + w, y + h)
		print(f"{name}: sheet px x {x}-{x + w} y {y}-{y + h} ({w}x{h})")
	if "--dry-run" in sys.argv:
		print("dry run: nothing written")
		return 0
	img.save(DST, optimize=True)
	print(f"wrote {DST}")
	return 0


if __name__ == "__main__":
	sys.exit(main())
