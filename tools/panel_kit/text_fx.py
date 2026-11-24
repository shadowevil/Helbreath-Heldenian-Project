"""Multi-pass text for the panel kit's text recipes (reference for the client's draw lists).

Every pass is the SAME glyph run drawn again, offset and recoloured, so the client reproduces it with one
draw_text call per pass (colour + alpha), no blur and no shader:

    render(canvas, xy, text, style, anchor)   draws the passes back to front: shadow, then fill
    glyph_mask(text, style)                    the glyph run as an L mask (used to measure ink widths)

style keys:
  face             serif | tahoma (staged/client/fonts)
  px               font size in px
  fill             (r, g, b); alpha = fill alpha 0..1 (default 1)
  tracking         extra px between letters (default 0)
  embolden         faux bold: the run drawn again +dx in x (True = 1 px, or a float such as 0.5); every pass
                   then costs 2 draws
  embolden_passes  "all" (default) or "fill": faux-bold only the fill, shadow uses the plain run
  shadow           {"dx", "dy", "color", "alpha"}
anchors: la (top-left of the ascent box), mm (ink centre), cap (x = ink centre, y = cap-height centre),
rcap / lcap (x = right / left ink edge, y = cap-height centre), ink (top-left of the ink box).
"""
import os

import numpy as np
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
FONTS = os.path.join(ROOT, "staged", "client", "fonts")
FACES = {"serif": "liberation_serif_regular.ttf", "tahoma": "wine_tahoma.ttf"}
PAD = 6


def font(face, px):
	return ImageFont.truetype(os.path.join(FONTS, FACES[face]), px)


def glyph_mask(text, style):
	"""Render the glyph run once as an L mask (PAD px margin). Returns mask, pad, ascent."""
	f = font(style.get("face", "serif"), style["px"])
	tracking = style.get("tracking", 0)
	if tracking:
		w = int(sum(f.getlength(c) + tracking for c in text)) + 2 * PAD
	else:
		w = int(f.getlength(text)) + 2 * PAD
	asc, desc = f.getmetrics()
	m = Image.new("L", (w, asc + desc + 2 * PAD), 0)
	d = ImageDraw.Draw(m)
	if tracking:
		x = PAD
		for c in text:
			d.text((x, PAD), c, font=f, fill=255)
			x += f.getlength(c) + tracking
	else:
		d.text((PAD, PAD), text, font=f, fill=255)
	if style.get("embolden"):
		dx = 1.0 if style["embolden"] is True else float(style["embolden"])
		a = np.asarray(m)
		if dx == int(dx):
			k = int(dx)
			b = a.copy()
			b[:, k:] = np.maximum(a[:, k:], a[:, :-k])
		else:
			s = np.asarray(Image.fromarray(a, "L").transform(m.size, Image.AFFINE, (1, 0, -dx, 0, 1, 0),
															 resample=Image.BILINEAR))
			b = np.maximum(a, s)
		m = Image.fromarray(b.astype(np.uint8), "L")
	return m, PAD, asc


def passes(style):
	"""Ordered (dx, dy, rgb, alpha, kind) draws, back to front."""
	out = []
	s = style.get("shadow")
	if s:
		out.append((s["dx"], s["dy"], tuple(s["color"]), s.get("alpha", 1.0), "shadow"))
	out.append((0, 0, tuple(style.get("fill", (255, 255, 255))), style.get("alpha", 1.0), "fill"))
	return out


def render(canvas, xy, text, style, anchor="la"):
	"""Draw text with its passes onto canvas (RGBA). Returns the ink bbox of the fill pass on the canvas."""
	m, pad, _ = glyph_mask(text, style)
	m_effects = m
	if style.get("embolden") and style.get("embolden_passes", "all") == "fill":
		m_effects, _, _ = glyph_mask(text, dict(style, embolden=False))
	mw, mh = m.size
	ink = m.getbbox()
	x, y = xy
	if anchor == "mm":
		x, y = x - (ink[0] + ink[2]) // 2, y - (ink[1] + ink[3]) // 2
	elif anchor in ("cap", "rcap", "lcap"):
		hb = font(style.get("face", "serif"), style["px"]).getbbox("H")
		if anchor == "cap":
			x = x - (ink[0] + ink[2]) // 2
		elif anchor == "rcap":
			x = x - ink[2]
		else:
			x = x - ink[0]
		y = y - (2 * pad + hb[1] + hb[3]) // 2
	elif anchor == "ink":
		x, y = x - ink[0], y - ink[1]
	else:
		x, y = x - pad, y - pad
	alpha_fill = np.asarray(m).astype(np.float32) / 255.0
	alpha_fx = np.asarray(m_effects).astype(np.float32) / 255.0
	for dx, dy, color, a, kind in passes(style):
		malpha = alpha_fill if kind == "fill" else alpha_fx
		rgb = np.broadcast_to(np.array(color, np.float32), (mh, mw, 3))
		layer = np.dstack([rgb, malpha * a * 255]).astype(np.uint8)
		canvas.alpha_composite(Image.fromarray(layer, "RGBA"), (x + dx, y + dy))
	return (x + ink[0], y + ink[1], x + ink[2], y + ink[3])
