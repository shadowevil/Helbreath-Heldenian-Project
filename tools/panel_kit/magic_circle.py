"""Magic circle (hexagram in a ring) as a transparent engraved bevel, traced from the old magic dialog art (HH-0071).

make_magic_circle() returns a 200x200 RGBA PIL image, transparent except the bevel. Deterministic: pure numpy /
scipy / Pillow maths, no randomness. Geometry is in 1x dialog pixels (the old magic dialog frame is 258x328).
Needs Pillow, numpy and scipy.
"""
import math

import numpy as np
from PIL import Image, ImageDraw
from scipy.ndimage import gaussian_filter, sobel

# measured from the old art (game_dialogs sprite 0 frame 1)
CX, CY = 128.5, 162.5     # circle centre in the 258x328 dialog (midway between the star's two horizontal bands)
R_OUT, RING = 90.0, 7.5   # ring outer radius and band width
BAND = 9.0                # star band width
R_STAR = 85.0             # star tips reach into the ring, as on the original
OPACITY = 0.5             # user, 2026-11-24: "I think it had a 50% transparency overall"
SS = 8                    # supersampling

W, H = 200, 200           # circle canvas, centred on (CX, CY)
# top-left of the canvas in the dialog: centre minus half the canvas (128.5 - 100, 162.5 - 100)
DIALOG_OFFSET = (round(CX - W / 2), round(CY - H / 2))


def _polygon(cx, cy, r, rot):
	return [(cx + r * math.cos(math.radians(rot + k * 120)), cy + r * math.sin(math.radians(rot + k * 120)))
			for k in range(3)]


def union_mask():
	"""Ring plus two triangle bands (hexagram), supersampled then box-filtered to W x H; float 0..1."""
	big = Image.new("L", (W * SS, H * SS), 0)
	d = ImageDraw.Draw(big)
	cx, cy = W / 2 * SS, H / 2 * SS
	ro, ri = R_OUT * SS, (R_OUT - RING) * SS
	d.ellipse((cx - ro, cy - ro, cx + ro, cy + ro), fill=255)
	d.ellipse((cx - ri, cy - ri, cx + ri, cy + ri), fill=0)
	rv = R_STAR * SS
	r2 = rv - BAND * SS * 2  # inradius shrink by BAND -> circumradius shrink by 2 * BAND
	for rot in (-90, 90):
		band = Image.new("L", big.size, 0)
		bd = ImageDraw.Draw(band)
		bd.polygon(_polygon(cx, cy, rv, rot), fill=255)
		bd.polygon(_polygon(cx, cy, r2, rot), fill=0)
		big = Image.fromarray(np.maximum(np.asarray(big), np.asarray(band)))
	return np.asarray(big.resize((W, H), Image.BOX), dtype=np.float32) / 255.0


def bevel(m):
	"""Engraved look, lit from the top-left: top-left groove walls in shadow, bottom-right walls catch the light,
	the groove floor slightly darker. Returns the RGBA image."""
	h = gaussian_filter(m, 0.3)
	gx, gy = sobel(h, axis=1), sobel(h, axis=0)
	lx, ly = -1 / math.sqrt(2), -1 / math.sqrt(2)
	shade = gx * lx + gy * ly
	shade /= max(1e-6, np.abs(shade).max())
	rgba = np.zeros((H, W, 4), np.float32)
	hi = np.clip(shade, 0, 1) ** 1.4
	lo = np.clip(-shade, 0, 1) ** 1.1
	floor = m * 0.7
	a_lo, a_hi = lo * 0.95, hi * 0.8
	col = np.zeros((H, W, 3), np.float32)
	a = np.zeros((H, W), np.float32)
	for layer_col, la in (((122, 82, 76), floor), ((0, 0, 0), a_lo), ((255, 244, 220), a_hi)):
		col = col * (1 - la[..., None]) + np.array(layer_col, np.float32) * la[..., None]
		a = a * (1 - la) + la
	rgba[..., :3] = np.where(a[..., None] > 0, col / np.maximum(a[..., None], 1e-6), 0)
	rgba[..., 3] = a * 255 * OPACITY
	return Image.fromarray(np.clip(rgba, 0, 255).astype(np.uint8), "RGBA")


def make_magic_circle():
	return bevel(union_mask())
