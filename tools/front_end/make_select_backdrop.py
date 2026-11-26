"""Derive the character-select sheet with the original's recessed boxes baked into the HD plank (HH-0108).

Reads the user's art (working/classic_ui/hd/character_selection_screen.png, read only) and writes the derived sheet
to staged/client/sprites/interface/front_end/character_select.png, the tracked output (same size and layout: the
backdrop at sheet (2,2), the cards in the right strip). The boxes are made from the plank's own wood: a darkened
face, a dark groove on the top/left inner edge and a light ridge on the bottom/right, like the original insets.
Nothing is upscaled from the low-res sheet (d-035).

Old geometry (the retired intermediate_screens 005.png, UI px, measured): plank inset face x 173-619, y 338-505;
help box x 179-438, y 343-441; account box x 179-438, y 449-501; footer strip x 209-401, y 512-531 on the plank's
bottom border. New plank (backdrop art px, measured on the 2026-11-25 19:26 art): inset face x 488-1432, y 578-927,
its bottom ridge at y 930, the plank's bottom edge at y 994. UI = (art - 240) / 1.8 across, art / 1.8 down. The
layout is in boxes() below; screen_select_character.cpp computes the same boxes for its text and buttons and must
agree with it: re-measure both after the user moves the plank.

Usage: python tools/front_end/make_select_backdrop.py [--dry-run]
"""
import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "working" / "classic_ui" / "hd" / "character_selection_screen.png"
DST = ROOT / "staged" / "client" / "sprites" / "interface" / "front_end" / "character_select.png"


def ui_x(art):
	return (art - 240.0) / 1.8


def ui_y(art):
	return art / 1.8


OLD_IN = (173.0, 338.0, 619.0, 505.0)
NEW_IN = (ui_x(488), ui_y(578), ui_x(1433), ui_y(928))   # inset face, right and bottom exclusive
KX = (NEW_IN[2] - NEW_IN[0]) / (OLD_IN[2] - OLD_IN[0])
KY = (NEW_IN[3] - NEW_IN[1]) / (OLD_IN[3] - OLD_IN[1])
BOTTOM_RIDGE_UI = ui_y(930)    # the inset's bottom ridge
PLANK_BOTTOM_UI = ui_y(994)    # the plank's bottom edge


def sheet_x(ui):
	return round(240 + ui * 1.8) + 2


def sheet_y(ui):
	return round(ui * 1.8) + 2


MARGIN = 7.0   # UI px, all round the inset, between the boxes and between the boxes and the buttons


def boxes():
	"""The same layout as screen_select_character.cpp: one margin all round the inset, the button column (the
	original 176 px face at the inset's scale) against its right edge, the help and account boxes stacked in the
	original's 99:53 height ratio, and the footer strip on the bottom border sharing their edges."""
	button_w = int(176.0 * KX + 0.5)
	box_left = NEW_IN[0] + MARGIN
	box_right = NEW_IN[2] - MARGIN - button_w - MARGIN
	box_top = NEW_IN[1] + MARGIN
	box_bottom = NEW_IN[3] - MARGIN
	help_bottom = box_top + (box_bottom - box_top - MARGIN) * 99.0 / (99.0 + 53.0)
	strip_h = (531 - 512 + 1) * KY
	border_mid = (BOTTOM_RIDGE_UI + PLANK_BOTTOM_UI) / 2.0
	return {
		"help": (box_left, box_top, box_right, help_bottom),
		"account": (box_left, help_bottom + MARGIN, box_right, box_bottom),
		"footer": (box_left, border_mid - strip_h / 2.0, box_right, border_mid + strip_h / 2.0),
	}


def recess(img, ui_box, face=0.84, groove=(0.70, 0.45, 0.25, 0.12), ridge=(0.50, 0.30, 0.12)):
	x0, y0, x1, y1 = sheet_x(ui_box[0]), sheet_y(ui_box[1]), sheet_x(ui_box[2]), sheet_y(ui_box[3])
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
	return (x0, y0, x1, y1)


def main():
	if not SRC.is_file():
		print(f"error: source art not found: {SRC}")
		return 1
	img = Image.open(SRC).convert("RGBA")
	for name, b in boxes().items():
		s = recess(img, b)
		print(f"{name}: UI x {b[0]:.2f}-{b[2]:.2f} y {b[1]:.2f}-{b[3]:.2f}; sheet px {s}")
	if "--dry-run" in sys.argv:
		print("dry run: nothing written")
		return 0
	img.save(DST, optimize=True)
	print(f"wrote {DST}")
	return 0


if __name__ == "__main__":
	sys.exit(main())
