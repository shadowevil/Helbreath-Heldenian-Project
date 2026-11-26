"""Build the runtime dialog/button kit the client composes panels from (HH-0054).

Usage:
  python tools/panel_kit/build_panel_kit.py [--out <dir>] [--panel <png>] [--buttons <png>] [--check] [--dry-run]

Reads the user's art sheets (read only):
  working/classic_ui/hd/panel_building.png   corners, bevels, rings, flourishes, wood bars, backgrounds, small buttons
  working/classic_ui/hd/button_panel.png     button frame, faces, arrow caps (the character-select cards are left out:
                                           the Character List plates stay as they are)
Writes into --out (default staged/client/sprites/panelkit/), names lowercase:
  panel_kit.png    one atlas: every piece copied pixel for pixel onto a 2 px padded sheet, plus the baked pieces
                   the runtime cannot make with alpha-only draws (bake(): brightening, per-channel multiply,
                   alpha gain, resampling, white flourish copies)
  panel_kit.json   frames {name: {image, x, y, w, h, group, use, source}} (mapkit style; source = the sheet and
                   tight rect a sliced piece came from, or the piece a baked one was made from, null for solid
                   fills), then the recipes: backgrounds,
                   border_styles, corner_styles, inner (inset/raised/rule/vrule/slot_ring/scrollbar), nine_slice
                   margins, flourishes (runtime tint/blend/opacity), buttons (frame, states, label styles) and
                   text_recipes as draw lists

Composition order (user, 2026-11-24): size -> background inset 1 px -> border -> corners -> inner placements.
tools/panel_kit/compose.py is the reference composer that draws from this kit with the same operations the
client has (region draws with alpha, tiling, 9-slice; tint + additive where the spec adds them).

--check  builds in memory and compares byte for byte with --out; writes nothing. When the PNG bytes differ it
         also compares decoded RGBA pixels (a different zlib/Pillow can re-encode the same image) and says which
         matched. Exit 0 = identical bytes, 1 = differs (pixels-only match included).
--dry-run lists the pieces and the files that would be written; writes nothing.
Writes go to <name>.tmp then rename, PNG first and JSON last, so a failure never leaves a mismatched pair.
--out may not be under working/ (the user's art folder).
Exit status 2 for a missing or unreadable input (the art sheets live in working/classic_ui/hd, gitignored, on the
author's machine) or a build error. Output is deterministic (fixed piece order, no PNG metadata, sorted packing).
Console text is ASCII. Needs Pillow.
"""
import argparse
import hashlib
import io
import json
import os
import sys
from pathlib import Path

from PIL import Image, ImageOps

sys.path.insert(0, str(Path(__file__).resolve().parent))
from magic_circle import DIALOG_OFFSET, make_magic_circle  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
ATLAS_NAME = "panel_kit.png"
JSON_NAME = "panel_kit.json"
ATLAS_WIDTH = 1024
PAD = 2

# ---------------------------------------------------------------- source pieces
# name: (search rect x, y, w, h, group, use). The stored rect is the tight alpha bounding box inside it.
PANEL_PARTS = {
	"corner_stone_tl": (0, 0, 44, 44, "corner_stone", "stone/bronze L corner, top-left (classic big dialogs)"),
	"corner_stone_tr": (55, 0, 44, 44, "corner_stone", "stone/bronze L corner, top-right"),
	"corner_stone_bl": (0, 54, 44, 44, "corner_stone", "stone/bronze L corner, bottom-left"),
	"corner_stone_br": (55, 54, 44, 44, "corner_stone", "stone/bronze L corner, bottom-right"),
	"corner_rivet_tl": (110, 0, 44, 42, "corner_rivet", "gold arm with steel rivet ball, top-left"),
	"corner_rivet_tr": (165, 0, 44, 42, "corner_rivet", "gold arm with steel rivet ball, top-right"),
	"corner_rivet_bl": (110, 54, 44, 42, "corner_rivet", "gold arm with steel rivet ball, bottom-left"),
	"corner_rivet_br": (165, 54, 44, 42, "corner_rivet", "gold arm with steel rivet ball, bottom-right"),
	"corner_gold_tl": (220, 0, 50, 40, "corner_gold", "rounded gold corner with pin, top-left (small gold boxes)"),
	"corner_gold_tr": (298, 0, 42, 40, "corner_gold", "rounded gold corner with pin, top-right"),
	"corner_gold_bl": (220, 54, 50, 42, "corner_gold", "rounded gold corner with pin, bottom-left"),
	"corner_gold_br": (298, 54, 42, 42, "corner_gold", "rounded gold corner with pin, bottom-right"),
	"bevel_box": (344, 0, 36, 36, "bevel", "inset bevel outline 32x32, raw sheet alpha (<= 59); use bevel_box_g3"),
	"ring_bevel": (436, 40, 42, 42, "bevel", "sunken bevelled circle 32x32, raw sheet alpha; use ring_bevel_26"),
	"divider_wave": (384, 0, 60, 30, "flourish", "wave flourish, black mask (System Menu / History titles)"),
	"divider_knot": (444, 2, 60, 26, "flourish", "knotted flourish, black mask (Name & Status heading)"),
	"divider_eye": (504, 0, 56, 20, "flourish", "eye scroll flourish, black mask (Commander / Soldier menus)"),
	"wood_light_top": (355, 42, 21, 9, "wood_light", "light grooved wood bar, top edge, raw 9 px"),
	"wood_light_bottom": (355, 72, 21, 9, "wood_light", "light grooved wood bar, bottom edge, raw 9 px"),
	"wood_light_left": (346, 51, 9, 21, "wood_light", "light grooved wood bar, left edge, raw 9 px"),
	"wood_light_right": (376, 51, 9, 21, "wood_light", "light grooved wood bar, right edge, raw 9 px"),
	"wood_dark_top": (398, 40, 21, 11, "wood_dark", "dark rough wood bar, top edge, raw"),
	"wood_dark_bottom": (398, 72, 21, 11, "wood_dark", "dark rough wood bar, bottom edge, raw"),
	"wood_dark_left": (390, 51, 8, 21, "wood_dark", "dark rough wood bar, left edge, raw"),
	"wood_dark_right": (419, 51, 8, 21, "wood_dark", "dark rough wood bar, right edge, raw"),
	"bg_stone": (2, 110, 256, 256, "background", "grey stone-cloth tile 256x256, raw"),
	"bg_leather": (268, 110, 256, 256, "background", "brown leather tile 256x256, raw"),
	"btn_box_a": (573, 2, 15, 15, "small_button", "framed orange arrow box (scrollbar thumb)"),
	"btn_box_b": (590, 2, 15, 15, "small_button", "framed orange down-arrow box"),
	"btn_round_plus": (575, 20, 12, 12, "small_button", "outline circle plus"),
	"btn_round_minus": (591, 20, 12, 12, "small_button", "outline circle minus"),
	"btn_round_plus_lit": (575, 36, 12, 12, "small_button", "circle with lit plus glyph"),
	"btn_round_minus_lit": (591, 36, 12, 12, "small_button", "circle with lit minus glyph"),
	"btn_sq_up": (575, 52, 12, 12, "small_button", "tan square, light up arrow"),
	"btn_sq_down": (591, 52, 12, 12, "small_button", "tan square, light down arrow"),
	"btn_sq_up_dark": (575, 68, 12, 12, "small_button", "tan square, dark up arrow"),
	"btn_sq_down_dark": (591, 68, 12, 12, "small_button", "tan square, dark down arrow"),
	"btn_sq_plus": (575, 83, 12, 12, "small_button", "tan square, light plus"),
	"btn_sq_minus": (591, 83, 12, 12, "small_button", "tan square, light minus"),
	"btn_sq_plus_dark": (575, 99, 12, 12, "small_button", "tan square, dark plus"),
	"btn_sq_minus_dark": (591, 99, 12, 12, "small_button", "tan square, dark minus"),
	"btn_round_up": (575, 114, 12, 12, "small_button", "circle, lit up arrow"),
	"btn_round_down": (591, 114, 12, 12, "small_button", "circle, lit down arrow"),
	"btn_round_up_dark": (575, 130, 12, 12, "small_button", "circle, dark up arrow"),
	"btn_round_down_dark": (591, 130, 12, 12, "small_button", "circle, dark down arrow"),
	"arrow_up_small": (577, 147, 8, 7, "small_button", "small white up arrow 8x7"),
	"arrow_down_small": (593, 147, 8, 7, "small_button", "small white down arrow 8x7"),
	"ring": (576, 158, 26, 26, "small_button", "26x26 ring: radio / slot circle"),
}
BUTTON_PARTS = {
	"btn_frame_tl": (221, 2, 4, 4, "btn_frame", "button frame corner top-left"),
	"btn_frame_t": (226, 2, 6, 3, "btn_frame", "button frame top edge, tile along x"),
	"btn_frame_tr": (233, 2, 4, 4, "btn_frame", "button frame corner top-right"),
	"btn_frame_l": (221, 7, 4, 4, "btn_frame", "button frame left edge, tile along y"),
	"btn_frame_r": (233, 7, 4, 4, "btn_frame", "button frame right edge, tile along y"),
	"btn_frame_bl": (221, 12, 4, 4, "btn_frame", "button frame corner bottom-left"),
	"btn_frame_b": (226, 13, 6, 3, "btn_frame", "button frame bottom edge, tile along x"),
	"btn_frame_br": (233, 12, 4, 4, "btn_frame", "button frame corner bottom-right"),
	"btn_face_normal": (241, 2, 25, 25, "btn_face", "button face 25x25, normal; tiled, never stretched"),
	"btn_face_hover": (241, 29, 25, 25, "btn_face", "button face 25x25, hover; tiled, never stretched"),
	"btn_cap_left": (223, 25, 5, 7, "btn_cap", "left arrow cap on the button's left middle"),
	"btn_cap_right": (229, 25, 5, 7, "btn_cap", "right arrow cap on the button's right middle"),
}

# ---------------------------------------------------------------- measured look (HH-0054, user-approved)
BEVEL_GAIN = 3.0                       # bevel_box / ring_bevel alpha peaks at 59 on the sheet; x3 reads like the originals
BORDER_THICKNESS = 6                   # classic 5-6 px bronze rim
CLOTH_GAIN = (1.17, 1.14, 1.0)         # outer cloth is warmer than the plain tile (measured)
FIELD = (0.85, 0.88, 0.98)             # insets darken the cloth back to the plain tile
DOLL_FIELD = (0.8, 0.82, 0.9)          # character doll panel
SLOT_RING_SIZE = 26
# Flourish default: black like the originals (user, 2026-11-24: "Default is black"); dark brown, opaque, normal
# blend, fitted to the sprite History / System Menu waves (ink = bg (1 - a) + tint a -> a 1.0, tint ~(40,29,16)).
# Dialogs override per title/heading at runtime (the character dialog keeps its approved warm additive look).
FLOURISH_BLACK = [40, 29, 16]


# Frames the client places at a fixed spot: name -> {dialog, x, y} (top-left of the frame inside that dialog).
PLACEMENTS = {"magic_circle": {"dialog": "magic", "dialog_size": [258, 328], "x": DIALOG_OFFSET[0], "y": DIALOG_OFFSET[1]}}


def tight(im, x, y, w, h):
	box = im.crop((x, y, x + w, y + h)).getchannel("A").getbbox()
	if box is None:
		raise ValueError("empty rect %r" % ((x, y, w, h),))
	return x + box[0], y + box[1], box[2] - box[0], box[3] - box[1]


def scaled_lut(f):
	"""Point table for v -> min(255, int(v * f))."""
	return [min(255, int(v * f)) for v in range(256)]


def alpha_gain(im, g):
	out = im.copy()
	out.putalpha(out.getchannel("A").point(scaled_lut(g)))
	return out


def multiply(im, factor):
	r, g, b, a = im.split()
	r, g, b = (c.point(scaled_lut(f)) for c, f in ((r, factor[0]), (g, factor[1]), (b, factor[2])))
	return Image.merge("RGBA", (r, g, b, a))


def resample_thickness(bar, t, horizontal):
	return bar.resize((bar.width, t) if horizontal else (t, bar.height), Image.LANCZOS)


def mirror_pair(bar, horizontal):
	"""bar + mirrored bar: tiling this pair from offset 0 equals tiling the bar with every other copy mirrored."""
	bw, bh = bar.size
	if horizontal:
		out = Image.new("RGBA", (bw * 2, bh))
		out.paste(bar, (0, 0))
		out.paste(ImageOps.mirror(bar), (bw, 0))
	else:
		out = Image.new("RGBA", (bw, bh * 2))
		out.paste(bar, (0, 0))
		out.paste(ImageOps.flip(bar), (0, bh))
	return out


def solid(w, h, rgba):
	return Image.new("RGBA", (w, h), rgba)


def slice_pieces(panel, buttons):
	pieces = {}  # name -> (image, group, use, source)
	for name, (x, y, w, h, group, use) in PANEL_PARTS.items():
		tx, ty, tw, th = tight(panel, x, y, w, h)
		pieces[name] = (panel.crop((tx, ty, tx + tw, ty + th)), group, use, "panel_building %d,%d %dx%d" % (tx, ty, tw, th))
	for name, (x, y, w, h, group, use) in BUTTON_PARTS.items():
		tx, ty, tw, th = tight(buttons, x, y, w, h)
		pieces[name] = (buttons.crop((tx, ty, tx + tw, ty + th)), group, use, "button_panel %d,%d %dx%d" % (tx, ty, tw, th))
	return pieces


def bake(pieces):
	"""BAKED: pieces the client cannot derive with alpha-only region draws (brightening, per-channel multiply,
	alpha gain, resampling). Each is a pure function of the source pieces."""
	def img(name):
		return pieces[name][0]

	baked = {}
	bevel = alpha_gain(img("bevel_box"), BEVEL_GAIN)
	baked["bevel_box_g3"] = (bevel, "bevel", "inset bevel, alpha x3; 9-slice margin 3 for sunken fields", "bevel_box")
	baked["bevel_box_g3_raised"] = (bevel.rotate(180), "bevel", "raised bevel (inset rotated 180); 9-slice margin 3",
									"bevel_box")
	baked["rule_dark"] = (bevel.crop((4, 0, 28, 1)), "bevel", "rule dark row 24x1; stretch along x", "bevel_box")
	baked["rule_light"] = (bevel.crop((4, bevel.height - 1, 28, bevel.height)), "bevel",
						   "rule light row 24x1; stretch along x", "bevel_box")
	baked["vrule_dark"] = (bevel.crop((0, 4, 1, 28)), "bevel", "vertical rule dark column 1x24; stretch along y",
						   "bevel_box")
	baked["vrule_light"] = (bevel.crop((bevel.width - 1, 4, bevel.width, 28)), "bevel",
							"vertical rule light column 1x24; stretch along y", "bevel_box")
	ring = alpha_gain(img("ring_bevel"), BEVEL_GAIN).resize((SLOT_RING_SIZE, SLOT_RING_SIZE), Image.LANCZOS)
	baked["ring_bevel_26"] = (ring, "bevel", "slot ring 26x26, alpha x3 (character doll slots)", "ring_bevel")
	for style in ("wood_light", "wood_dark"):
		for side in ("top", "bottom", "left", "right"):
			horizontal = side in ("top", "bottom")
			bar = resample_thickness(img("%s_%s" % (style, side)), BORDER_THICKNESS, horizontal)
			baked["%s_%s_t6" % (style, side)] = (mirror_pair(bar, horizontal), "border_" + style,
												 "%s %s edge at 6 px, bar + mirror; tile along the edge" % (style, side),
												 "%s_%s" % (style, side))
	cloth = multiply(img("bg_stone"), CLOTH_GAIN)
	baked["bg_stone_cloth"] = (cloth, "background", "bg_stone x (1.17, 1.14, 1.0): classic outer cloth", "bg_stone")
	baked["bg_stone_field"] = (multiply(cloth, FIELD), "background",
							   "cloth x (0.85, 0.88, 0.98): inset field fill", "bg_stone")
	baked["bg_stone_doll"] = (multiply(cloth, DOLL_FIELD), "background",
							  "cloth x (0.8, 0.82, 0.9): character doll panel fill", "bg_stone")
	for name in ("divider_wave", "divider_knot", "divider_eye"):
		white = solid(img(name).width, img(name).height, (255, 255, 255, 255))
		white.putalpha(img(name).getchannel("A"))
		baked[name + "_white"] = (white, "flourish", "white copy of %s: vertex colour = the tint" % name, name)
	baked["magic_circle"] = (make_magic_circle(), "decor",
							 "magic dialog hexagram-in-ring, engraved bevel, 35%% overall; draw 1:1 at dialog offset "
							 "%d,%d (258x328 dialog)" % DIALOG_OFFSET, "generated by magic_circle.py (HH-0071)")
	# Sideways arrow squares (HH-0115): the sheet has no left/right square arrow, so turn only the 10x10 face of the
	# down arrow; the 1 px bevel keeps its light direction. Collapsed dropdowns point left, open ones point down.
	for dark in ("", "_dark"):
		down = img("btn_sq_down" + dark)
		for side, angle in (("left", -90), ("right", 90)):
			sq = down.copy()
			sq.paste(down.crop((1, 1, down.width - 1, down.height - 1)).rotate(angle), (1, 1))
			baked["btn_sq_%s%s" % (side, dark)] = (sq, "small_button", "tan square, %s %s arrow (btn_sq_down face "
												   "turned, bevel kept)" % ("dark" if dark else "light", side),
												   "btn_sq_down" + dark)
	baked["px_black"] = (solid(4, 4, (0, 0, 0, 255)), "util", "solid black: draw at alpha 1-f for a scalar darken f",
						 None)
	baked["px_white"] = (solid(4, 4, (255, 255, 255, 255)), "util", "solid white: tinted fills once tint exists", None)
	return baked


def pack(items):
	"""Shelf pack, tallest first then by name; returns {name: (x, y)} and the sheet size."""
	def size_key(n):
		return (-items[n].height, -items[n].width, n)

	order = sorted(items, key=size_key)
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


def recipes():
	shadow = [36, 26, 24]
	return {
		"backgrounds": {
			"stone": {"tile": "bg_stone", "use": "plain stone-cloth"},
			"stone_cloth": {"tile": "bg_stone_cloth", "use": "classic dialog outer cloth (default)"},
			"stone_field": {"tile": "bg_stone_field", "use": "inset field fill over stone_cloth"},
			"stone_doll": {"tile": "bg_stone_doll", "use": "character doll panel fill"},
			"leather": {"tile": "bg_leather", "use": "brown leather"},
		},
		"background_rule": "tile into (1, 1, W-2, H-2) with the tile origin at the panel's (1, 1); inset fills tile "
						   "with the same origin so they line up with the cloth",
		"border_styles": {
			style: {"top": style + "_top_t6", "bottom": style + "_bottom_t6", "left": style + "_left_t6",
					"right": style + "_right_t6", "thickness": BORDER_THICKNESS,
					"order": "inner shadow, left, right, top over full width, bottom over full width",
					"inner_shadow": {"piece": "px_black", "rows": 3, "alpha_top_left": [0.45, 0.30, 0.15],
									 "alpha_bottom_right": [0.35, 0.20, 0.05],
									 "rule": "row i sits i px inside the rim, between the side bars"}}
			for style in ("wood_light", "wood_dark")},
		"corner_styles": {s: {k: "corner_%s_%s" % (s, k) for k in ("tl", "tr", "bl", "br")} for s in ("stone", "rivet", "gold")},
		"nine_slice": {"bevel_box_g3": 3, "bevel_box_g3_raised": 3},
		"inner": {
			"inset": {"field": "stone_field", "bevel": "bevel_box_g3", "margin": 3,
					  "rule": "fill (x+1, y+1, w-2, h-2) with the field background (or px_black at 1-f for a scalar "
							  "darken), then 9-slice the bevel over (x, y, w, h); edges stretch (nearest)"},
			"raised": {"bevel": "bevel_box_g3_raised", "margin": 3, "rule": "no fill; 9-slice the raised bevel"},
			"rule": {"dark": "rule_dark", "light": "rule_light", "height": 2,
					 "rule": "top height/2 rows dark, the rest light, each stretched to w"},
			"vrule": {"dark": "vrule_dark", "light": "vrule_light", "width": 2,
					  "rule": "left width/2 columns dark, the rest light, each stretched to h"},
			"slot_ring": {"piece": "ring_bevel_26", "size": SLOT_RING_SIZE, "anchor": "center",
						  "lift": {"rgb_gain": [1.16, 1.12, 1.02], "disc_inset": 2,
								   "note": "optional disc brightening; needs the additive extension or is skipped"}},
			"scrollbar": {"track_w": 6, "groove": "inset", "groove_darken": 0.8, "thumb": "btn_box_a",
						  "rule": "groove = inset centred in the rect, any length, darkened 0.8 (px_black at 0.2); "
								  "thumb centred in x at y + (h - thumb_h) * pos, pos 0 top .. 1 bottom"},
		},
		"flourishes": {
			"note": "black alpha masks; colour is a runtime parameter (user: colourable at runtime). tint = colour "
					"the mask takes; blend normal = alpha, additive = dst + tint * a * opacity; emboss = the mask "
					"drawn first at (+1, +1) in emboss_color",
			"divider_knot": {"scale": 0.66, "tint": FLOURISH_BLACK, "blend": "normal", "opacity": 1.0, "emboss": None},
			"divider_wave": {"scale": 0.46, "tint": FLOURISH_BLACK, "blend": "normal", "opacity": 1.0, "emboss": None,
							 "scale_sys_menu": 0.56},
			"divider_eye": {"scale": 1.0, "tint": FLOURISH_BLACK, "blend": "normal", "opacity": 1.0, "emboss": None,
							"note": "not tuned against an original yet"},
			"style_rule": "tint / blend / opacity / emboss above are the defaults (black, like the originals); a dialog "
						  "may override them per title or heading at runtime",
			"title_rule": "one each side of the title, gap px from the title ink, right one mirrored; vertical centre "
						  "on the title's cap height (cap top to baseline)",
		},
		"buttons": {
			"frame": {"tl": "btn_frame_tl", "t": "btn_frame_t", "tr": "btn_frame_tr", "l": "btn_frame_l",
					  "r": "btn_frame_r", "bl": "btn_frame_bl", "b": "btn_frame_b", "br": "btn_frame_br",
					  "rule": "edges tile (not stretch) between the 4x4 corners; corners last"},
			"face_rule": "face tiles into (x+1, y+1, w-2, h-2) with origin at the button's (x+1, y+1), never stretched",
			"states": {"normal": {"face": "btn_face_normal", "label": "fill"},
					   "hover": {"face": "btn_face_hover", "label": "fill_hover"}},
			"caps": {"left": "btn_cap_left", "right": "btn_cap_right", "inset": 2, "rule": "vertically centred"},
			"label_styles": {"small": "small_button_label", "wide": "wide_button_label"},
			"sizes": {"small": [74, 20], "wide": [176, 24]},
		},
		"text_recipes": {
			"note": "draw lists, bottom to top; each draw = (dx, dy, color, alpha, blend). Faux bold = the same draw "
					"again at dx + bold_dx; the client's int positions round 0.5 up to 1. Anchors: cap = the cap "
					"height is centred on y; centre in x on the ink width",
			"title_red": {"font": "liberation_serif_regular.ttf", "px": 17, "anchor": "centre",
						  "draws": [{"dx": 1, "dy": 1, "color": shadow, "alpha": 0.75, "blend": "normal"},
									{"dx": -1, "dy": -1, "color": [120, 85, 75], "alpha": 0.35, "blend": "additive",
									 "note": "highlight up-left, outside the glyph (fill covers the overlap)"},
									{"dx": 0, "dy": 0, "color": "fill", "alpha": 1.0, "blend": "normal"}],
						  "fills": {"character_info": [140, 8, 8], "system_menu": [148, 24, 16],
									"history": [144, 16, 12]}},
			"heading": {"font": "wine_tahoma.ttf", "px": 13, "bold_dx": 0.5, "bold_passes": "all",
						"draws": [{"dx": 1, "dy": 1, "color": shadow, "alpha": 0.75, "blend": "normal"},
								  {"dx": -1, "dy": -1, "color": [250, 236, 200], "alpha": 0.35, "blend": "normal"},
								  {"dx": 0, "dy": 0, "color": [236, 212, 164], "alpha": 1.0, "blend": "normal"}],
						"draw_count": 6},
			"field_label": {"font": "wine_tahoma.ttf", "px": 12, "bold_dx": 0.5, "bold_passes": "all", "align": "right",
							"gap_to_value_box": 4,
							"draws": [{"dx": 0, "dy": 0, "color": [28, 22, 16], "alpha": 1.0, "blend": "normal"}],
							"draw_count": 2},
			"value": {"font": "default", "px": 0, "note": "as the client draws today: put_aligned_string, UILabel "
														  "(25, 25, 25), no shadow; the name line UIDarkRed (58, 0, 0)",
					  "draws": [{"dx": 0, "dy": 0, "color": [25, 25, 25], "alpha": 1.0, "blend": "normal"}]},
			"small_button_label": {"font": "wine_tahoma.ttf", "px": 11, "tracking": 1, "bold_dx": 0.5,
								   "bold_passes": "all", "anchor": "cap",
								   "draws": [{"dx": 0, "dy": 0, "color": "fill", "alpha": 1.0, "blend": "normal"}],
								   "fill": [239, 222, 206], "fill_hover": [247, 247, 0], "draw_count": 2},
			"wide_button_label": {"font": "liberation_serif_regular.ttf", "px": 18, "tracking": 0.5, "word_gap": 8,
								  "anchor": "cap", "per_word": True,
								  "draws": [{"dx": 1, "dy": 1, "color": [24, 16, 8], "alpha": 0.8, "blend": "normal",
											 "bold": True},
											{"dx": 0, "dy": 0, "color": "fill x 0.65", "alpha": 1.0, "blend": "normal",
											 "bold": True},
											{"dx": 0, "dy": 0, "color": "fill", "alpha": 1.0, "blend": "normal"}],
								  "bold_dx": 0.5, "fill": [250, 240, 218], "fill_hover": [247, 247, 4],
								  "draw_count_per_word": 5,
								  "layout": "words laid out left to right with word_gap between ink boxes, the run "
											"centred in the button"},
		},
	}


def repo_rel(path):
	"""Repo-relative posix path when inside the repo, else the absolute path."""
	p = Path(path).resolve()
	try:
		return p.relative_to(REPO).as_posix()
	except ValueError:
		return p.as_posix()


def under_working(path):
	p = Path(path).resolve()
	w = REPO / "working"
	return p == w or w in p.parents


def pixels_equal(png_a, png_b):
	a = Image.open(io.BytesIO(png_a)).convert("RGBA")
	b = Image.open(io.BytesIO(png_b)).convert("RGBA")
	return a.size == b.size and a.tobytes() == b.tobytes()


def write_pair(out, files):
	"""Each file to <name>.tmp, then rename; PNG first, JSON last, so a reader never sees a new JSON with an old atlas."""
	out.mkdir(parents=True, exist_ok=True)
	for name in (ATLAS_NAME, JSON_NAME):
		data = files[name]
		tmp = out / (name + ".tmp")
		tmp.write_bytes(data)
		os.replace(tmp, out / name)
		print("wrote %s (%d bytes, sha256 %s)" % (out / name, len(data), hashlib.sha256(data).hexdigest()))


SHEETS_NOTE = "art sheets live in working/classic_ui/hd (gitignored, on the author's machine)"


def build(panel_path, buttons_path):
	"""Returns {file name: bytes}."""
	panel = Image.open(panel_path).convert("RGBA")
	buttons = Image.open(buttons_path).convert("RGBA")
	pieces = slice_pieces(panel, buttons)
	pieces.update(bake(pieces))
	pos, size = pack({n: p[0] for n, p in pieces.items()})
	sheet = Image.new("RGBA", size, (0, 0, 0, 0))
	frames = {}
	for name in pieces:  # declaration order in the json, packed position from pack()
		im, group, use, source = pieces[name]
		x, y = pos[name]
		sheet.paste(im, (x, y))
		frames[name] = {"image": ATLAS_NAME, "x": x, "y": y, "w": im.width, "h": im.height, "group": group, "use": use,
						"source": source}
		if name in PLACEMENTS:
			frames[name]["placement"] = PLACEMENTS[name]
	png = io.BytesIO()
	sheet.save(png, "PNG", optimize=False, compress_level=9)
	doc = {
		"kit": "panel_kit",
		"version": 1,
		"ticket": "HH-0054",
		"inputs": {"panel_building": {"path": repo_rel(panel_path),
									  "sha256": hashlib.sha256(Path(panel_path).read_bytes()).hexdigest()},
				   "button_panel": {"path": repo_rel(buttons_path),
									"sha256": hashlib.sha256(Path(buttons_path).read_bytes()).hexdigest()}},
		"compose_order": ["size", "background (1 px inset)", "border", "corners", "inner placements"],
		"frames": frames,
	}
	doc.update(recipes())
	text = json.dumps(doc, indent=1, ensure_ascii=True) + "\n"
	return {ATLAS_NAME: png.getvalue(), JSON_NAME: text.encode("ascii")}


def main(argv=None):
	ap = argparse.ArgumentParser(description="Build the runtime panel/button kit (HH-0054).")
	ap.add_argument("--out", default=str(REPO / "staged" / "client" / "sprites" / "panelkit"))
	ap.add_argument("--panel", default=str(REPO / "working" / "classic_ui" / "hd" / "panel_building.png"))
	ap.add_argument("--buttons", default=str(REPO / "working" / "classic_ui" / "hd" / "button_panel.png"))
	ap.add_argument("--check", action="store_true", help="compare a fresh build with --out byte for byte; write nothing")
	ap.add_argument("--dry-run", action="store_true", help="list pieces and files; write nothing")
	a = ap.parse_args(argv)
	out = Path(a.out)
	if under_working(out):
		print("refused: --out %s is under working/, the user's art folder (read only)" % out, file=sys.stderr)
		return 2
	for p in (a.panel, a.buttons):
		if not os.path.isfile(p):
			print("missing input: %s - %s" % (p, SHEETS_NOTE), file=sys.stderr)
			return 2
	try:
		files = build(a.panel, a.buttons)
	except Exception as e:  # corrupt sheet, bad rect, ...: one line, exit 2
		print("build failed: %s: %s - %s" % (type(e).__name__, e, SHEETS_NOTE), file=sys.stderr)
		return 2
	if not (a.dry_run or a.check):
		write_pair(out, files)
		return 0
	frames = json.loads(files[JSON_NAME])["frames"]
	if a.dry_run:
		for n, f in frames.items():
			print("%-26s %4d,%4d %3dx%3d  %s" % (n, f["x"], f["y"], f["w"], f["h"], f["group"]))
		for name, data in files.items():
			print("would write %s (%d bytes, sha256 %s)" % (out / name, len(data), hashlib.sha256(data).hexdigest()))
		return 0
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
	print("check: %s (%d pieces)" % (verdict, len(frames)))
	return 0 if bytes_same else 1


if __name__ == "__main__":
	sys.exit(main())
