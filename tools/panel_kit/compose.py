"""Reference composer for the staged panel kit (HH-0054): what the client's panel_kit class must draw.

    compose(size, background, border_style, corner_style, inner) -> RGBA image

Reads only staged/client/sprites/panelkit/panel_kit.{png,json} and uses only the operations the client
renderer has or the runtime spec adds: region draws with alpha, tiling, 9-slice (nearest), stretch, and for
flourishes a tint + normal/additive blend. Order (user, 2026-11-24): size -> background inset 1 px -> border ->
corners -> inner placements.

Usage:
  python tools/panel_kit/compose.py [--kit <dir>] [--out <png>]
writes a preview (a character-style panel and the two button sizes in both states) to --out
(default build/panel_kit_preview/preview.png). Console text is ASCII. Needs Pillow and numpy.
"""
import argparse
import json
import math
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageOps

sys.path.insert(0, str(Path(__file__).resolve().parent))  # text_fx from any cwd
import text_fx  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
DEFAULT_KIT = REPO / "staged" / "client" / "sprites" / "panelkit"


class Kit:
	def __init__(self, kit_dir=DEFAULT_KIT):
		kit_dir = Path(kit_dir)
		self.doc = json.loads((kit_dir / "panel_kit.json").read_text(encoding="ascii"))
		self.sheet = Image.open(kit_dir / "panel_kit.png").convert("RGBA")
		self._cache = {}

	def piece(self, name):
		if name not in self._cache:
			f = self.doc["frames"][name]
			self._cache[name] = self.sheet.crop((f["x"], f["y"], f["x"] + f["w"], f["y"] + f["h"]))
		return self._cache[name]


# ---------------------------------------------------------------- primitive draws (client equivalents)

def draw(canvas, im, x, y, alpha=1.0):
	"""draw_texture_region(src, dst same size, alpha)."""
	if alpha != 1.0:
		im = im.copy()
		def scale(v):
			return int(v * alpha)
		im.putalpha(im.getchannel("A").point(scale))
	canvas.alpha_composite(im, (x, y))


def tile(canvas, im, rect, origin):
	"""Repeat region draws over rect; the tile grid starts at origin (clipped to rect)."""
	x, y, w, h = rect
	if w <= 0 or h <= 0:
		return
	layer = Image.new("RGBA", (w, h))
	tw, th = im.size
	ox, oy = (x - origin[0]) % tw, (y - origin[1]) % th
	for yy in range(-oy, h, th):
		for xx in range(-ox, w, tw):
			layer.paste(im, (xx, yy))
	canvas.alpha_composite(layer, (x, y))


def stretch(canvas, im, rect, alpha=1.0):
	x, y, w, h = rect
	if w > 0 and h > 0:
		draw(canvas, im.resize((w, h), Image.NEAREST), x, y, alpha)


def nine_slice(canvas, im, rect, m):
	x, y, w, h = rect
	sw, sh = im.size
	xs = [(0, m, 0, m), (m, sw - m, m, w - m), (sw - m, sw, w - m, w)]
	ys = [(0, m, 0, m), (m, sh - m, m, h - m), (sh - m, sh, h - m, h)]
	for sy0, sy1, dy0, dy1 in ys:
		for sx0, sx1, dx0, dx1 in xs:
			if dx1 > dx0 and dy1 > dy0:
				stretch(canvas, im.crop((sx0, sy0, sx1, sy1)), (x + dx0, y + dy0, dx1 - dx0, dy1 - dy0))


def tinted_draw(canvas, mask, x, y, tint, blend="normal", opacity=1.0):
	"""Spec extension: draw a mask in a colour, normal or additive."""
	if blend == "additive":
		region = np.asarray(canvas.crop((x, y, x + mask.width, y + mask.height))).astype(np.float32)
		k = np.asarray(mask.getchannel("A")).astype(np.float32)[:, :, None] / 255.0 * opacity
		region[:, :, :3] = np.clip(region[:, :, :3] + np.array(tint, np.float32) * k, 0, 255)
		canvas.paste(Image.fromarray(region.astype(np.uint8), "RGBA"), (x, y))
		return
	solid = Image.new("RGBA", mask.size, tuple(tint) + (255,))
	solid.putalpha(mask.getchannel("A"))
	draw(canvas, solid, x, y, opacity)


# ---------------------------------------------------------------- the five steps

def compose(kit, size, background="stone_cloth", border_style="wood_light", corner_style="stone", inner=()):
	W, H = size
	c = Image.new("RGBA", (W, H))                                                  # 1 size
	if background:                                                                 # 2 background, 1 px inset
		tile(c, kit.piece(kit.doc["backgrounds"][background]["tile"]), (1, 1, W - 2, H - 2), (1, 1))
	if border_style:                                                               # 3 border
		b = kit.doc["border_styles"][border_style]
		t = b["thickness"]
		sh = b["inner_shadow"]
		px = kit.piece(sh["piece"])
		for i in range(sh["rows"]):
			a1, a2 = sh["alpha_top_left"][i], sh["alpha_bottom_right"][i]
			stretch(c, px, (t, t + i, W - 2 * t, 1), a1)
			stretch(c, px, (t, H - t - i - 1, W - 2 * t, 1), a2)
			stretch(c, px, (t + i, t, 1, H - 2 * t), a1)
			stretch(c, px, (W - t - i - 1, t, 1, H - 2 * t), a2)
		tile(c, kit.piece(b["left"]), (0, 0, t, H), (0, 0))
		tile(c, kit.piece(b["right"]), (W - t, 0, t, H), (W - t, 0))
		tile(c, kit.piece(b["top"]), (0, 0, W, t), (0, 0))
		tile(c, kit.piece(b["bottom"]), (0, H - t, W, t), (0, H - t))
	if corner_style:                                                               # 4 corners
		cs = kit.doc["corner_styles"][corner_style]
		tl, tr, bl, br = (kit.piece(cs[k]) for k in ("tl", "tr", "bl", "br"))
		draw(c, tl, 0, 0)
		draw(c, tr, W - tr.width, 0)
		draw(c, bl, 0, H - bl.height)
		draw(c, br, W - br.width, H - br.height)
	for p in inner:                                                                # 5 inner placements
		INNER[p["type"]](kit, c, p)
	return c


def _inset(kit, c, p):
	x, y, w, h = p["rect"]
	spec = kit.doc["inner"]["inset"]
	field = p.get("field", spec["field"])
	if field:
		tile(c, kit.piece(kit.doc["backgrounds"][field]["tile"]), (x + 1, y + 1, w - 2, h - 2), (1, 1))
	if p.get("darken"):
		stretch(c, kit.piece("px_black"), (x + 1, y + 1, w - 2, h - 2), 1.0 - p["darken"])
	nine_slice(c, kit.piece(spec["bevel"]), (x, y, w, h), spec["margin"])


def _raised(kit, c, p):
	spec = kit.doc["inner"]["raised"]
	nine_slice(c, kit.piece(spec["bevel"]), p["rect"], spec["margin"])


def _rule(kit, c, p):
	spec = kit.doc["inner"]["rule"]
	h = p.get("height", spec["height"])
	for i in range(h):
		stretch(c, kit.piece(spec["dark"] if i < h // 2 else spec["light"]), (p["x"], p["y"] + i, p["w"], 1))


def _vrule(kit, c, p):
	spec = kit.doc["inner"]["vrule"]
	wd = p.get("width", spec["width"])
	for i in range(wd):
		stretch(c, kit.piece(spec["dark"] if i < wd // 2 else spec["light"]), (p["x"] + i, p["y"], 1, p["h"]))


def _slot_ring(kit, c, p):
	ring = kit.piece(kit.doc["inner"]["slot_ring"]["piece"])
	draw(c, ring, p["x"] - ring.width // 2, p["y"] - ring.height // 2)


def _scrollbar(kit, c, p):
	x, y, w, h = p["rect"]
	spec = kit.doc["inner"]["scrollbar"]
	tw = spec["track_w"]
	_inset(kit, c, {"rect": (x + (w - tw) // 2, y, tw, h), "field": None, "darken": spec["groove_darken"]})
	thumb = kit.piece(spec["thumb"])
	draw(c, thumb, x + (w - thumb.width) // 2, y + round((h - thumb.height) * p.get("pos", 1.0)))


def _flourish(kit, c, p):
	spec = kit.doc["flourishes"][p["name"]]
	mask = kit.piece(p["name"])
	sc = p.get("scale", spec["scale"])
	mask = mask.resize((round(mask.width * sc), round(mask.height * sc)), Image.LANCZOS)
	if p.get("mirror"):
		mask = ImageOps.mirror(mask)
	x, y = p["x"] - mask.width // 2, p["y"] - mask.height // 2
	# p["style"] = {"tint", "blend", "opacity"} overrides the JSON default (black) for this flourish, as the
	# client's flourish_style does; an override draws no emboss
	style = p.get("style")
	if style:
		tinted_draw(c, mask, x, y, style["tint"], style.get("blend", "normal"), style.get("opacity", 1.0))
		return
	if spec.get("emboss") and spec["blend"] == "normal":
		tinted_draw(c, mask, x + 1, y + 1, spec["emboss"])
	tinted_draw(c, mask, x, y, spec["tint"], spec["blend"], spec["opacity"])


def _bold_dx(r):
	"""A recipe's faux-bold offset; "subpixel": false rounds it half up to a whole pixel, as the client does."""
	dx = r.get("bold_dx", 0)
	return math.floor(dx + 0.5) if r.get("subpixel", True) is False else dx


def _button(kit, c, p):
	x, y, w, h = p["rect"]
	b = kit.doc["buttons"]
	state = b["states"][p.get("state", "normal")]
	tile(c, kit.piece(state["face"]), (x + 1, y + 1, w - 2, h - 2), (x + 1, y + 1))
	f = {k: kit.piece(v) for k, v in b["frame"].items() if k != "rule"}
	tile(c, f["t"], (x + 4, y, w - 8, f["t"].height), (x + 4, y))
	tile(c, f["b"], (x + 4, y + h - f["b"].height, w - 8, f["b"].height), (x + 4, y + h - f["b"].height))
	tile(c, f["l"], (x, y + 4, f["l"].width, h - 8), (x, y + 4))
	tile(c, f["r"], (x + w - f["r"].width, y + 4, f["r"].width, h - 8), (x + w - f["r"].width, y + 4))
	draw(c, f["tl"], x, y)
	draw(c, f["tr"], x + w - 4, y)
	draw(c, f["bl"], x, y + h - 4)
	draw(c, f["br"], x + w - 4, y + h - 4)
	cl, cr = kit.piece(b["caps"]["left"]), kit.piece(b["caps"]["right"])
	ins = b["caps"]["inset"]
	draw(c, cl, x + ins, y + (h - cl.height) // 2)
	draw(c, cr, x + w - ins - cr.width, y + (h - cr.height) // 2)
	if p.get("label"):
		r = kit.doc["text_recipes"][b["label_styles"][p.get("size", "small")]]
		fill = r["fill_hover" if p.get("state") == "hover" else "fill"]
		face = "serif" if "serif" in r["font"] else "tahoma"
		base = {"face": face, "px": r["px"], "tracking": r.get("tracking", 0),
				"embolden_passes": r.get("bold_passes", "all")}
		if not r.get("per_word"):
			text_fx.render(c, (x + w // 2, y + h // 2), p["label"], dict(base, embolden=_bold_dx(r), fill=tuple(fill)),
						   anchor="cap")
			return
		sh = r["draws"][0]
		under = dict(base, embolden=_bold_dx(r), fill=tuple(int(v * 0.65) for v in fill),
					 shadow={"dx": sh["dx"], "dy": sh["dy"], "color": sh["color"], "alpha": sh["alpha"]})
		top = dict(base, fill=tuple(fill))
		words = p["label"].split()
		inks = []
		for wd in words:
			bb = text_fx.glyph_mask(wd, under)[0].getbbox()
			inks.append(bb[2] - bb[0])
		cx = x + (w - (sum(inks) + r["word_gap"] * (len(words) - 1))) // 2
		for wd, iw in zip(words, inks):
			text_fx.render(c, (cx, y + h // 2), wd, under, anchor="lcap")
			text_fx.render(c, (cx, y + h // 2), wd, top, anchor="lcap")
			cx += iw + r["word_gap"]


def _heading(kit, c, p):
	"""A text recipe (default "heading") with its ink centred on (x, y), x defaulting to the panel centre, and
	optionally a flourish each side, gap px from the ink, the right one mirrored (kit_dialog::heading / title).
	The flourishes centre on the text's cap height (cap top to baseline), not its ink box, so descenders do not
	pull them down (user, 2026-11-24). Normal-blend draws only."""
	r = kit.doc["text_recipes"][p.get("recipe", "heading")]
	if any(d.get("blend", "normal") != "normal" for d in r["draws"]):
		raise ValueError("heading: recipe %r has a non-normal blend draw; text_fx draws normal only"
						 % p.get("recipe", "heading"))
	face = "serif" if "serif" in r["font"] else "tahoma"
	base = {"face": face, "px": r["px"], "tracking": r.get("tracking", 0), "embolden": _bold_dx(r),
			"embolden_passes": r.get("bold_passes", "all")}
	cx, cy = p.get("x", c.width // 2), p["y"]
	for d in r["draws"]:
		fill = p.get("fill", r.get("fill")) if d["color"] == "fill" else d["color"]
		text_fx.render(c, (cx + d["dx"], cy + d["dy"]), p["text"], dict(base, fill=tuple(fill), alpha=d["alpha"]),
					   anchor="mm")
	if not p.get("flourish"):
		return
	m, pad, _ = text_fx.glyph_mask(p["text"], base)
	ink = m.getbbox()
	hb = text_fx.font(face, r["px"]).getbbox("H")
	fy = cy - (ink[1] + ink[3]) // 2 + (2 * pad + hb[1] + hb[3]) // 2
	spec = kit.doc["flourishes"][p["flourish"]]
	fw = round(kit.piece(p["flourish"]).width * spec["scale"])
	reach = (ink[2] - ink[0]) // 2 + p.get("gap", 6) + fw // 2
	style = p.get("flourish_style")
	_flourish(kit, c, {"name": p["flourish"], "x": cx - reach, "y": fy, "style": style})
	_flourish(kit, c, {"name": p["flourish"], "x": cx + reach, "y": fy, "mirror": True, "style": style})


INNER = {"inset": _inset, "raised": _raised, "rule": _rule, "vrule": _vrule, "slot_ring": _slot_ring,
		 "scrollbar": _scrollbar, "flourish": _flourish, "button": _button, "heading": _heading}


def preview(kit):
	panel = compose(kit, (270, 376), inner=[
		# the character dialog overrides the default black knots with its approved warm additive look
		{"type": "heading", "text": "Name & Status", "y": 40, "flourish": "divider_knot",
		 "flourish_style": {"tint": (196, 176, 126), "blend": "additive", "opacity": 0.5}},
		{"type": "heading", "text": "Equipment & Characteristics", "y": 91},
		{"type": "inset", "rect": (14, 49, 242, 34)},
		{"type": "inset", "rect": (14, 100, 242, 178), "field": None},
		{"type": "inset", "rect": (18, 104, 102, 169), "field": "stone_doll"},
		{"type": "slot_ring", "x": 37, "y": 123}, {"type": "slot_ring", "x": 101, "y": 183},
		{"type": "rule", "x": 20, "y": 290, "w": 230}, {"type": "scrollbar", "rect": (240, 104, 15, 169), "pos": 0.3},
		{"type": "button", "rect": (98, 340, 74, 20), "label": "Party"},
		{"type": "button", "rect": (180, 340, 74, 20), "label": "Level Set.", "state": "hover"},
	])
	out = Image.new("RGBA", (270 + 196, 376), (24, 22, 20, 255))
	out.alpha_composite(panel, (0, 0))
	for i, st in enumerate(("normal", "hover")):
		b = compose(kit, (176, 24), None, None, None,
					[{"type": "button", "rect": (0, 0, 176, 24), "label": "Create New Character", "size": "wide", "state": st}])
		out.alpha_composite(b, (280, 10 + i * 34))
	return out


def main(argv=None):
	ap = argparse.ArgumentParser(description="Preview the staged panel kit with the reference composer.")
	ap.add_argument("--kit", default=str(DEFAULT_KIT))
	ap.add_argument("--out", default=str(REPO / "build" / "panel_kit_preview" / "preview.png"))
	a = ap.parse_args(argv)
	out = Path(a.out).resolve()
	if out == REPO / "working" or (REPO / "working") in out.parents:
		print("refused: --out is under working/, the user's art folder (read only)", file=sys.stderr)
		return 2
	out.parent.mkdir(parents=True, exist_ok=True)
	preview(Kit(a.kit)).save(out)
	print("wrote %s" % out)
	return 0


if __name__ == "__main__":
	sys.exit(main())
