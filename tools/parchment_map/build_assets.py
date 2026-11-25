"""Build the data files make_parchment.py ships with (run once; outputs are committed with the tool).

Usage:
  python tools/parchment_map/build_assets.py palette             tile colour palette from the tile/object sprite folders
  python tools/parchment_map/build_assets.py paper [newmaps]      paper patches, edge alphas, rim profile
  python tools/parchment_map/build_assets.py icons [newmaps]      icon cut-outs (rects listed in ICONS below)

[newmaps] is the interface/newmaps sprite folder: a folder path holding manifest.json, or an asset name
in staged/client/sprites (default: the archived copy in working/archive/interface/newmaps, since HH-0105);
read through tools/sprite_folder/sprite_folder.py.

Outputs go to tools/parchment_map/data/. ASCII console output only.
"""
import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
sys.path.insert(0, str(REPO / "tools" / "sprite_folder"))
import sprite_folder  # noqa: E402

# The newmaps sheets left the game in HH-0105; the paper and icon sources now live in the archive
NEWMAPS_DIR = REPO / "working" / "archive" / "interface" / "newmaps"
DATA = HERE / "data"
PATCH = 24


def build_palette():
	"""Mean colour and coverage of every (sprite id, frame) the map renderer knows, as a compact npz."""
	sys.path.insert(0, str(REPO / "tools" / "map_renderer"))
	from map_renderer import SpriteCache  # noqa: E402
	cache = SpriteCache(REPO / "staged" / "client" / "sprites")
	ids = sorted(cache._cache)
	offsets = np.full(1024, -1, np.int32)
	counts = np.zeros(1024, np.int32)
	rows = []
	for sid in ids:
		_, rects = cache._cache[sid]
		offsets[sid] = len(rows)
		counts[sid] = len(rects)
		for fi in range(len(rects)):
			f = cache.get_frame(sid, fi)
			row = [0, 0, 0, 0]
			if f is not None:
				a = np.asarray(f.convert("RGBA"), np.float32)
				m = a[:, :, 3] > 0
				if m.any():
					c = a[:, :, :3][m].mean(0)
					row = [int(round(c[0])), int(round(c[1])), int(round(c[2])), int(round(255 * m.mean()))]
			rows.append(row)
	DATA.mkdir(exist_ok=True)
	np.savez_compressed(DATA / "tile_palette.npz", offsets=offsets, counts=counts, rgba=np.array(rows, np.uint8))
	print(f"palette: {len(ids)} sprites, {len(rows)} frames -> data/tile_palette.npz")


def open_newmaps(arg=None):
	"""The newmaps sprite folder: arg is a folder path, an asset name, or None for the archived copy."""
	if arg and (Path(arg) / "manifest.json").is_file():
		return sprite_folder.open_folder(Path(arg))
	if arg:
		return sprite_folder.open_asset(arg)
	return sprite_folder.open_folder(NEWMAPS_DIR)


def frames(newmaps):
	"""Every frame of every newmaps sheet (colour key transparent), in sprite then frame order."""
	if not isinstance(newmaps, sprite_folder.SpriteFolder):
		newmaps = open_newmaps(newmaps)
	out = []
	for sprite in newmaps.sprites:
		sheet = sprite.rgba()
		for r in sprite.rectangles:
			out.append(sheet.crop((r.x, r.y, r.x + r.width, r.y + r.height)))
	return out


def ink_free_mask(f):
	"""True where a frame shows bare paper: no ink, labels or icons, fully opaque."""
	rgb = np.asarray(f, np.float32)[:, :, :3]
	a = np.asarray(f)[:, :, 3]
	lum = rgb.mean(2)
	sat = rgb.max(2) - rgb.min(2)
	med = np.asarray(Image.fromarray(lum.astype(np.uint8)).filter(ImageFilter.MedianFilter(3)), np.float32)
	base = np.asarray(Image.fromarray(med.astype(np.uint8)).filter(ImageFilter.GaussianBlur(10)), np.float32)
	# ink strokes are 3-5 px wide; paper creases are 1 px: a wide blur keeps the strokes and dilutes the creases
	ink = np.asarray(Image.fromarray(np.clip(base - med, 0, 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(3)), np.float32)
	bad = (ink > 9) | (lum < 70) | (lum > 236) | (sat > 120) | (a < 255)
	bad = np.asarray(Image.fromarray((bad * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(3)), np.float32) > 0
	return ~bad


def build_paper(newmaps):
	fs = frames(newmaps)
	patches = []
	for f in fs:
		rgb = np.asarray(f, np.uint8)[:, :, :3]
		ok = ink_free_mask(f)
		# interior only (no rim): rim shading is applied separately from the measured profile
		for y in range(16, 300 - 16 - PATCH, 6):
			for x in range(16, 300 - 16 - PATCH, 6):
				if ok[y:y + PATCH, x:x + PATCH].mean() > 0.85:
					p = rgb[y:y + PATCH, x:x + PATCH].astype(np.float32)
					patches.append(p)
	# every patch to the same mean colour, so patches from the darker rim band tile seamlessly with interior ones
	target = np.median(np.array([p.reshape(-1, 3).mean(0) for p in patches]), axis=0)
	patches = [np.clip(p * (target / p.reshape(-1, 3).mean(0)), 0, 255).astype(np.uint8) for p in patches]
	n = len(patches)
	cols = 16
	rows = (n + cols - 1) // cols
	atlas = np.zeros((rows * PATCH, cols * PATCH, 3), np.uint8)
	for i, p in enumerate(patches):
		atlas[(i // cols) * PATCH:(i // cols + 1) * PATCH, (i % cols) * PATCH:(i % cols + 1) * PATCH] = p
	DATA.mkdir(exist_ok=True)
	Image.fromarray(atlas).save(DATA / "paper_patches.png")
	# mean colour of bare paper over all originals (the generated paper is matched to it)
	bare = np.concatenate([np.asarray(f, np.float32)[:, :, :3][ink_free_mask(f)] for f in fs])
	mean_rgb = [round(float(v), 1) for v in bare.mean(0)]
	(DATA / "paper_patches.json").write_text(json.dumps({"patch": PATCH, "count": n, "cols": cols, "mean_rgb": mean_rgb}))
	# torn-edge alphas, one per original frame
	for i, f in enumerate(fs):
		Image.fromarray(np.asarray(f)[:, :, 3]).save(DATA / f"edge_alpha_{i:02d}.png")
	# rim profile: paper luminance (relative to the interior) by distance from the torn edge, over bare paper
	prof_sum = np.zeros(60)
	prof_n = np.zeros(60)
	for f in fs:
		a = np.asarray(f)[:, :, 3] > 0
		lum = np.asarray(f, np.float32)[:, :, :3].mean(2)
		ok = ink_free_mask(f) | (a & (lum >= 90))
		dist = distance_inside(a)
		interior = lum[(dist > 40) & ok].mean()
		for d in range(60):
			m = (dist == d) & a & (lum > 40)
			if m.any():
				prof_sum[d] += (lum[m] / interior).mean()
				prof_n[d] += 1
	prof = (prof_sum / np.maximum(prof_n, 1)).round(4).tolist()
	(DATA / "rim_profile.json").write_text(json.dumps({"lum_ratio_by_distance": prof}))
	print(f"paper: {n} patches, {len(fs)} edge alphas, rim profile {prof[:8]} ... {prof[30]}")


def distance_inside(mask):
	"""Chebyshev-ish distance (in px) from each opaque pixel to the nearest transparent one, capped at 60."""
	dist = np.zeros(mask.shape, np.int32)
	cur = mask.copy()
	for d in range(60):
		dist[cur] = d
		er = cur.copy()
		er[1:, :] &= cur[:-1, :]; er[:-1, :] &= cur[1:, :]
		er[:, 1:] &= cur[:, :-1]; er[:, :-1] &= cur[:, 1:]
		er[0, :] = er[-1, :] = er[:, 0] = er[:, -1] = False
		cur = er
	dist[cur] = 60
	return dist


# icon cut-outs: name -> (frame index in frames() order, x, y, w, h, kind); kind picks the matte rule.
# frames() order: 0 aresden, 1 elvine, 2 middleland, 3 default (beginner zone), 4 huntzone2, 5 huntzone1,
# 6 huntzone4, 7 huntzone3, 8 arefarm, 9 elvfarm. Boxes stop short of the baked-in labels.
ICONS = {
	"arrow_n": (4, 109, 41, 12, 16, "red"),
	"arrow_e": (4, 233, 144, 20, 12, "red"),
	"arrow_nw": (0, 44, 35, 16, 16, "red"),
	"portal_red": (3, 132, 114, 22, 22, "red"),
	"portal_blue": (3, 199, 112, 24, 24, "blue"),
	"sword": (3, 117, 152, 18, 19, "sword"),
	"dungeon": (4, 155, 153, 9, 9, "generic"),
	"bld_wizard_tower": (0, 50, 102, 20, 20, "building"),
	"bld_warehouse": (0, 188, 107, 22, 22, "building"),
	"bld_shop": (0, 129, 139, 21, 20, "building"),
	"bld_blacksmith": (0, 152, 157, 22, 23, "building"),
	"bld_city_hall": (0, 138, 107, 23, 21, "building"),
	"bld_guild_hall": (0, 121, 86, 22, 21, "building"),
}


def build_icons(newmaps):
	fs = frames(newmaps)
	out = DATA / "icons"
	out.mkdir(parents=True, exist_ok=True)
	for name, (fi, x, y, w, h, kind) in ICONS.items():
		f = fs[fi]
		rgba = np.asarray(f, np.float32)
		crop = rgba[y:y + h, x:x + w, :3]
		alpha = icon_matte(f, x, y, w, h, kind)
		img = np.dstack([crop, alpha]).clip(0, 255).astype(np.uint8)
		Image.fromarray(img, "RGBA").save(out / f"{name}.png")
		print(f"icon {name}: {w}x{h}, opaque {int((alpha > 128).sum())} px")


def icon_matte(f, x, y, w, h, kind):
	"""Alpha for an icon painted over paper: distance of each pixel from the local bare-paper colour."""
	rgb = np.asarray(f, np.float32)[:, :, :3]
	pad = 12
	x0, y0 = max(0, x - pad), max(0, y - pad)
	reg = rgb[y0:y + h + pad, x0:x + w + pad]
	# paper estimate: median of the ring around the box
	ring = np.ones(reg.shape[:2], bool)
	ring[y - y0:y - y0 + h, x - x0:x - x0 + w] = False
	paper = np.median(reg[ring], axis=0)
	crop = rgb[y:y + h, x:x + w]
	diff = np.abs(crop - paper)
	lum = crop.mean(2)
	sat = crop.max(2) - crop.min(2)
	if kind == "red":
		score = np.clip((crop[:, :, 0] - crop[:, :, 1] - 45) * 6, 0, 255)
	elif kind == "blue":
		score = np.clip((crop[:, :, 2] - crop[:, :, 0] + 5) * 8, 0, 255)
	elif kind == "sword":  # white blade, dark and blue-grey outline: far from paper, and not paper-hued
		score = np.clip((diff.sum(2) - 100) * 3, 0, 255) * ((sat < 70) | (lum < 80))
	else:  # generic: anything far from the paper colour (dark outlines, white blades, roofs)
		score = np.clip((diff.sum(2) - 80) * 3, 0, 255)
	core = score > 128
	if kind == "red":
		# the arrow's dark outline: dark pixels touching the red body
		near = np.asarray(Image.fromarray((core * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(3)), np.uint8) > 0
		core |= near & (lum < 85)
	if kind == "building":
		# roofs are close to the paper colour: close the outline before filling it
		img = Image.fromarray((core * 255).astype(np.uint8))
		core = np.asarray(img.filter(ImageFilter.MaxFilter(5)).filter(ImageFilter.MinFilter(5)), np.uint8) > 0
	core = largest_component(core)
	core = fill_holes(core)
	a = np.asarray(Image.fromarray((core * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(0.35)), np.float32)
	return np.where(core, 255.0, a)


def largest_component(mask):
	"""The largest 8-connected component of a boolean mask, plus components within 1 px of it."""
	h, w = mask.shape
	lab = np.zeros((h, w), np.int32)
	n = 0
	sizes = [0]
	for y0, x0 in zip(*np.nonzero(mask)):
		if lab[y0, x0]:
			continue
		n += 1
		stack = [(y0, x0)]
		lab[y0, x0] = n
		size = 0
		while stack:
			y, x = stack.pop()
			size += 1
			for ny in (y - 1, y, y + 1):
				for nx in (x - 1, x, x + 1):
					if 0 <= ny < h and 0 <= nx < w and mask[ny, nx] and not lab[ny, nx]:
						lab[ny, nx] = n
						stack.append((ny, nx))
		sizes.append(size)
	if n == 0:
		return mask
	keep = lab == int(np.argmax(sizes))
	grown = np.asarray(Image.fromarray((keep * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(5)), np.uint8) > 0
	ids = set(np.unique(lab[grown & mask]).tolist()) - {0}
	return np.isin(lab, list(ids))


def fill_holes(mask):
	"""Fill regions of a mask not reachable from the border through unmasked pixels."""
	h, w = mask.shape
	outside = np.zeros((h, w), bool)
	stack = [(y, x) for y in range(h) for x in (0, w - 1) if not mask[y, x]]
	stack += [(y, x) for x in range(w) for y in (0, h - 1) if not mask[y, x]]
	for y, x in stack:
		outside[y, x] = True
	while stack:
		y, x = stack.pop()
		for ny, nx in ((y - 1, x), (y + 1, x), (y, x - 1), (y, x + 1)):
			if 0 <= ny < h and 0 <= nx < w and not mask[ny, nx] and not outside[ny, nx]:
				outside[ny, nx] = True
				stack.append((ny, nx))
	return ~outside


if __name__ == "__main__":
	if len(sys.argv) < 2:
		print(__doc__)
		sys.exit(1)
	cmd = sys.argv[1]
	if cmd == "palette":
		build_palette()
	elif cmd == "paper":
		build_paper(sys.argv[2] if len(sys.argv) > 2 else None)
	elif cmd == "icons":
		build_icons(sys.argv[2] if len(sys.argv) > 2 else None)
	else:
		print(__doc__)
		sys.exit(1)
