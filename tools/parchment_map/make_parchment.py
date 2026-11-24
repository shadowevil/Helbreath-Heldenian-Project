"""Make a Helbreath parchment map (the 300x300 RGBA picture the Map item opens) from a map's .amd file.

Usage:
  python tools/parchment_map/make_parchment.py <map.amd> -o <out.png> [--scale N] [--paper hq|patches|<image>]
         [--mapinfo <map name | mapinfo dir>] [--icons-json <out.json>] [--icons] [--ink-layer <ink.png>]
         [--minimap <dir>] [--seed N] [--sheet <compare.png>] [--reference <png>] [-v]

  --scale N  output is 300*N px square (1 = the client's size; 4 = 1200, the project's HQ art size). The ink
             is computed at the output resolution: the same strokes, drawn crisp at N > 1.
  --paper    hq (default): the shipped high-quality parchment (data/paper_hq.png, torn edge in its alpha);
             patches: the old paper quilted from the originals; or any image path (RGBA with its own torn
             edge, or opaque: then an original's edge is used).
  --icons-json  write the icon placements (exits, portals, buildings, entrances, monster areas) as JSON in
             output pixels; no icons are drawn by default (an icon kit will be placed from this file).
  --icons    also draw the old cut-out icons (off by default).
  --ink-layer  also write the ink alone as a transparent PNG at the output size: drawn over the same paper
             (source-over) it gives the written map, less the hatching's emboss highlight (a runtime kit).

Works from the .amd alone. Optional inputs refine it:
  --minimap  a folder of minimap art (01.jpg/02.jpg/03.jpg); used instead of the shipped tile palette as the
             per-tile colour (colour only decides which neighbouring tile sheets count as one terrain).
             A missing folder or images: a one-line note, then the palette is used.
  --mapinfo  map database rows (teleports, spawn generators, map names). If a directory exists at that
             path, rows for the .amd's file name are read from its *.data.sql files; otherwise the value is
             a map name and the rows for THAT name are read from sources/database/mapinfo.
             Without it, exits and doors are guessed from teleport tiles and no monster areas are drawn.
  --reference  a 300x300 original: the ink NCC against it is printed (with or without --sheet).
  --sheet    also write a review sheet at 2x: [reference | generated | ink overlay (red original,
             green generated, yellow both)] with --reference, else the generated map alone.

Exit status 0 on success; 2 with one 'error: ...' line on stderr for a missing, truncated or non-.amd
input (map sizes must be 1..4096) or an unreadable reference.
Exit status 2 also for --scale outside 1..8 or a missing --paper image.
Output is deterministic for a given seed. Console text is ASCII apart from echoed file paths. Needs numpy and Pillow.
"""
import argparse
import json
import re
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter

HERE = Path(__file__).resolve().parent


def configure(scale):
	"""Set the output scale: SIZE = 300*scale; every pixel constant below is multiplied by PX."""
	global PX, SIZE, BOX, BOX_DY
	if not 1 <= scale <= 8:
		raise ValueError(f"--scale {scale} is outside 1..8")
	PX, SIZE, BOX, BOX_DY = scale, BASE * scale, BASE_BOX * scale, BASE_BOX_DY * scale
DATA = HERE / "data"
REPO = HERE.parents[1]
BASE = 300           # the client's parchment size; --scale N renders BASE*N
BASE_BOX = 245       # the whole map is scaled into a BOX x BOX square, centred, as the originals do
BASE_BOX_DY = -1     # (fitted on 7 originals: map edge at x 28-32, y 25-29, 243-246 px wide)
PX = 1               # output pixels per original pixel (--scale); set by configure()
SIZE, BOX, BOX_DY = BASE, BASE_BOX, BASE_BOX_DY
HQ_PAPER = "paper_hq.png"   # default paper (the user's high-quality parchment, torn edge in its alpha)
INK_RGB = np.array([52, 36, 22], np.float32)   # dark-brown ink the strokes lean towards on the HQ paper
PAPER_CONTRAST = 1.35
INK_FLOOR = 0.38     # darkest ink multiplies the paper by this (measured on the originals: 0.37-0.40)


# ---------------------------------------------------------------- map data

MAX_MAP_SIZE = 4096


def header_size(hdr, key, path):
	"""MAPSIZEX / MAPSIZEY from the split 256-byte .amd header, checked to be 1..MAX_MAP_SIZE."""
	try:
		i = hdr.index(key)
		if hdr[i + 1] != "=":
			raise ValueError
		v = int(hdr[i + 2])
	except (ValueError, IndexError):
		raise ValueError(f"{path}: not a .amd file (no '{key} = <n>' in the 256-byte header)") from None
	if not 1 <= v <= MAX_MAP_SIZE:
		raise ValueError(f"{path}: {key} = {v} is outside 1..{MAX_MAP_SIZE}")
	return v


class MapData:
	def __init__(self, path):
		data = Path(path).read_bytes()
		hdr = data[:256].replace(b"\0", b" ").decode("ascii", "replace").replace("=", " = ").split()
		self.sx = header_size(hdr, "MAPSIZEX", path)
		self.sy = header_size(hdr, "MAPSIZEY", path)
		need = 256 + self.sx * self.sy * 10
		if len(data) < need:
			raise ValueError(f"{path}: {len(data)} bytes, header says {self.sx}x{self.sy} needs {need}")
		t = np.frombuffer(data[256:need], np.uint8).reshape(self.sy, self.sx, 10)
		v = t[:, :, :8].copy().view("<i2").reshape(self.sy, self.sx, 4)
		self.tile_spr, self.tile_frm, self.obj_spr, self.obj_frm = (v[:, :, i].astype(np.int32) for i in range(4))
		flags = t[:, :, 8]
		self.blocked = (flags & 0x80) != 0
		self.teleport = (flags & 0x40) != 0
		self.name = Path(path).stem.lower()

	def upsampled(self, k):
		"""The same map with every tile split into k x k sub-tiles (for drawing zoomed-in interiors)."""
		u = object.__new__(MapData)
		rep = lambda a: np.repeat(np.repeat(a, k, 0), k, 1)
		u.sx, u.sy, u.name = self.sx * k, self.sy * k, self.name
		for f in ("tile_spr", "tile_frm", "obj_spr", "obj_frm", "blocked", "teleport"):
			setattr(u, f, rep(getattr(self, f)))
		return u


# ---------------------------------------------------------------- colour map

def load_palette():
	z = np.load(DATA / "tile_palette.npz")
	return z["offsets"], z["counts"], z["rgba"]


def colour_map(m, palette):
	"""Per-tile mean colour of the tile sprite frame (and object, weighted by its coverage).
	Returns (rgb float32 [sy,sx,3], known fraction)."""
	offsets, counts, rgba = palette
	spr = np.clip(m.tile_spr, 0, len(offsets) - 1)
	frm = m.tile_frm
	ok = (m.tile_spr >= 0) & (m.tile_spr < len(offsets)) & (offsets[spr] >= 0) & (frm >= 0) & (frm < counts[spr])
	idx = np.where(ok, offsets[spr] + np.clip(frm, 0, None), 0)
	col = rgba[idx][:, :, :3].astype(np.float32)
	ok &= rgba[idx][:, :, 3] > 0
	# unknown tiles: a stable pseudo colour per sprite id, so region borders still show
	if not ok.all():
		h = (m.tile_spr.astype(np.int64) * 2654435761) & 0xFFFFFF
		pseudo = np.dstack([(h >> 16) & 255, (h >> 8) & 255, h & 255]).astype(np.float32) * 0.5 + 64
		col = np.where(ok[:, :, None], col, pseudo)
	ospr = np.clip(m.obj_spr, 0, len(offsets) - 1)
	ook = (m.obj_spr > 0) & (m.obj_spr < len(offsets)) & (offsets[ospr] >= 0) & (m.obj_frm >= 0) & (m.obj_frm < counts[ospr])
	oidx = np.where(ook, offsets[ospr] + np.clip(m.obj_frm, 0, None), 0)
	ocol = rgba[oidx].astype(np.float32)
	ow = np.where(ook, ocol[:, :, 3] / 255.0, 0.0) * 0.5
	col = col * (1 - ow[:, :, None]) + ocol[:, :, :3] * ow[:, :, None]
	return col, float(ok.mean())


def minimap_colour(m, mdir):
	"""Colour map from minimap art: the largest available image, resampled to one pixel per tile."""
	for n in ("01.jpg", "02.jpg", "03.jpg"):
		p = Path(mdir) / n
		if p.is_file():
			img = Image.open(p).convert("RGB").resize((m.sx, m.sy), Image.BOX)
			return np.asarray(img, np.float32)
	raise FileNotFoundError(f"no 01.jpg/02.jpg/03.jpg in {mdir}")


# ---------------------------------------------------------------- image helpers

def gblur(a, r):
	"""Separable Gaussian blur of a 2-D float array (edge padded)."""
	if r <= 0:
		return a
	k = int(r * 3) + 1
	x = np.arange(-k, k + 1, dtype=np.float32)
	g = np.exp(-x * x / (2 * r * r))
	g /= g.sum()
	p = np.pad(a, ((k, k), (0, 0)), mode="edge")
	a = sum(g[i] * p[i:i + a.shape[0]] for i in range(2 * k + 1))
	p = np.pad(a, ((0, 0), (k, k)), mode="edge")
	return sum(g[i] * p[:, i:i + a.shape[1]] for i in range(2 * k + 1))


def value_noise(rng, h, w, cells):
	g = rng.random((cells + 1, cells + 1)).astype(np.float32)
	return np.asarray(Image.fromarray((g * 255).astype(np.uint8)).resize((w, h), Image.BICUBIC), np.float32) / 255.0


def fractal(rng, h, w, octaves=5, start=4, persistence=0.6):
	out = np.zeros((h, w), np.float32)
	amp = tot = 0.0
	amp = 1.0
	for o in range(octaves):
		out += amp * value_noise(rng, h, w, start * 2 ** o)
		tot += amp
		amp *= persistence
	return out / tot


# ---------------------------------------------------------------- framing

ROOM_SHARE = 0.5      # a main walkable area smaller than this share of the map is framed on its own
ROOM_MARGIN = 1.4     # framed area = room box x this
ROOM_MIN_TILES = 28   # never zoom in closer than this many tiles across


def main_area(m):
	"""Bounding box (x0, y0, x1, y1) of the walkable component holding the most teleport tiles (else the largest)."""
	walk = ~m.blocked
	lab = np.zeros(walk.shape, np.int32)
	n = 0
	best, best_key = None, None
	for y0, x0 in zip(*np.nonzero(walk)):
		if lab[y0, x0]:
			continue
		n += 1
		stack = [(y0, x0)]
		lab[y0, x0] = n
		ys, xs, tel = [], [], 0
		while stack:
			y, x = stack.pop()
			ys.append(y)
			xs.append(x)
			tel += bool(m.teleport[y, x])
			for ny, nx in ((y - 1, x), (y + 1, x), (y, x - 1), (y, x + 1)):
				if 0 <= ny < m.sy and 0 <= nx < m.sx and walk[ny, nx] and not lab[ny, nx]:
					lab[ny, nx] = n
					stack.append((ny, nx))
		# teleports sit on walkable tiles next to doors; also count those just outside the component
		key = (tel, len(ys))
		if best_key is None or key > best_key:
			best_key, best = key, (min(xs), min(ys), max(xs) + 1, max(ys) + 1)
	return best


def frame_rect(m):
	"""(scale px/tile, ox, oy) with px = ox + (tile + 0.5) * scale. The originals draw the whole map in a
	BOX-wide square; a map whose main walkable area is small (an interior, an arena) is framed on that area."""
	area = main_area(m)
	if area is not None:
		x0, y0, x1, y1 = area
		side = max(x1 - x0, y1 - y0)
		if side < ROOM_SHARE * max(m.sx, m.sy):
			side = min(max(side * ROOM_MARGIN, ROOM_MIN_TILES), max(m.sx, m.sy))
			s = BOX / side
			cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
			return s, SIZE / 2 - cx * s, SIZE / 2 - cy * s + BOX_DY
	s = BOX / max(m.sx, m.sy)
	w, h = m.sx * s, m.sy * s
	return s, (SIZE - w) / 2, (SIZE - h) / 2 + BOX_DY


def to_px(rect, tx, ty):
	s, ox, oy = rect
	return ox + (tx + 0.5) * s, oy + (ty + 0.5) * s


# ---------------------------------------------------------------- ink

FAMILY_COLOUR = 24.0     # sheets this close in mean colour ...
FAMILY_BLOCKED = 0.35    # ... and in blocked share (object tiles excluded) are one terrain
FAMILY_TOUCH = 8         # ... if they touch along at least this many tile edges
STROKE_PX_PER_TILE = 1.45 # the beginner-zone original's scale; finer scales draw on sub-tiles
INK_DEPTH = 2            # strokes are this many tiles deep, inside the smaller terrain
STROKE_MIN_PX = 2.2      # on coarse maps (big cities, middleland) strokes are deepened to stay this wide
INK_MIN_TILES = 5        # smaller ink specks are dropped


def neighbours8(a, pad_mode="edge"):
	"""The 8 shifted copies of a 2-D array (edge padded), as (dy, dx, view)."""
	h, w = a.shape
	p = np.pad(a, 1, mode=pad_mode)
	return [(dy, dx, p[1 + dy:1 + dy + h, 1 + dx:1 + dx + w]) for dy in (-1, 0, 1) for dx in (-1, 0, 1) if dx or dy]


def clean_labels(lab, passes=2):
	"""A tile whose label appears in none of its 8 neighbours takes their most common label
	(lone decoration tiles: flowers, stones, single variant tiles)."""
	for _ in range(passes):
		nb = np.stack([v for _, _, v in neighbours8(lab)])
		lone = (nb != lab[None]).all(0)
		if not lone.any():
			break
		ys, xs = np.nonzero(lone)
		vals = nb[:, ys, xs]
		for i in range(len(ys)):
			u, c = np.unique(vals[:, i], return_counts=True)
			lab[ys[i], xs[i]] = u[np.argmax(c)]
	return lab


def terrain_families(m, col, lab):
	"""Union adjacent sheets that read as one terrain (similar colour and blockedness)."""
	ids = np.unique(lab)
	free = m.obj_spr <= 0
	stats = {}
	for i in ids:
		k = lab == i
		kf = k & free
		stats[int(i)] = (col[k].mean(0), m.blocked[kf].mean() if kf.any() else m.blocked[k].mean(), int(k.sum()))
	parent = {int(i): int(i) for i in ids}

	def find(a):
		while parent[a] != a:
			parent[a] = parent[parent[a]]
			a = parent[a]
		return a
	pairs = {}
	for a, b in ((lab[:, 1:], lab[:, :-1]), (lab[1:, :], lab[:-1, :])):
		diff = a != b
		lo = np.minimum(a[diff], b[diff]).astype(np.int64)
		hi = np.maximum(a[diff], b[diff]).astype(np.int64)
		key, cnt = np.unique(lo * 100000 + hi, return_counts=True)
		for k, c in zip(key, cnt):
			pairs[int(k)] = pairs.get(int(k), 0) + int(c)
	for k, c in pairs.items():
		a, b = k // 100000, k % 100000
		if c < FAMILY_TOUCH:
			continue
		ca, ba, _ = stats[a]
		cb, bb, _ = stats[b]
		if np.sqrt(((ca - cb) ** 2).sum()) < FAMILY_COLOUR and abs(ba - bb) < FAMILY_BLOCKED:
			parent[find(a)] = find(b)
	fam = np.vectorize(lambda v: find(int(v)))(lab) if len(ids) < 4096 else lab
	return fam


def ink_tiles(m, col, depth=INK_DEPTH):
	"""Ink per tile in {0,1}, the way the originals trace a map (calibrated on 7 originals):
	  1. the outline of the walkable land, drawn INK_DEPTH tiles into the blocked side (coasts, cliff
	     and rock edges, the foot of walls, fences, small blocked islands as little rings);
	  2. borders between two terrains that are both blocked (the top of a wall against the roof), drawn
	     inside the smaller terrain, so a wall face shows as a double line;
	borders between two walkable terrains (grass and dirt, plazas) are left bare."""
	b = m.blocked
	lab = clean_labels(m.tile_spr.copy())
	fam = terrain_families(m, col, lab)
	fams, inv, area = np.unique(fam, return_inverse=True, return_counts=True)
	fam_area = area[inv.reshape(fam.shape)]
	outline = np.zeros(b.shape, bool)
	for _, _, nb in neighbours8(b):
		outline |= b & ~nb
	inner = np.zeros(b.shape, bool)
	for (_, _, nf), (_, _, na), (_, _, nb) in zip(neighbours8(fam), neighbours8(fam_area), neighbours8(b)):
		inner |= b & nb & (nf != fam) & ((na > fam_area) | ((na == fam_area) & (nf > fam)))
	outline = deepen(outline, b, depth)
	inner = deepen(inner, b & (fam >= 0), depth, fam)
	ink = outline | inner
	return drop_small(ink, INK_MIN_TILES).astype(np.float32)


def deepen(seed, allowed, depth, same=None):
	"""Grow a tile mask depth-1 steps into allowed tiles (and, with same, only within one label)."""
	ink = seed.copy()
	for _ in range(depth - 1):
		grow = ink.copy()
		if same is None:
			for _, _, ni in neighbours8(ink):
				grow |= ni
		else:
			for (_, _, ni), (_, _, ns) in zip(neighbours8(ink), neighbours8(same)):
				grow |= ni & (ns == same)
		ink = grow & allowed | seed
	return ink


def drop_small(b, min_tiles):
	"""Remove 8-connected components of fewer than min_tiles tiles."""
	h, w = b.shape
	seen = np.zeros_like(b)
	out = b.copy()
	ys, xs = np.nonzero(b)
	for y0, x0 in zip(ys, xs):
		if seen[y0, x0]:
			continue
		stack = [(y0, x0)]
		seen[y0, x0] = True
		comp = []
		while stack:
			y, x = stack.pop()
			comp.append((y, x))
			for ny in (y - 1, y, y + 1):
				for nx in (x - 1, x, x + 1):
					if 0 <= ny < h and 0 <= nx < w and b[ny, nx] and not seen[ny, nx]:
						seen[ny, nx] = True
						stack.append((ny, nx))
		if len(comp) < min_tiles:
			for y, x in comp:
				out[y, x] = False
	return out


def place(m, tiles, rect, blur=0.5):
	"""Resample a tile-resolution layer onto the 300x300 frame (bilinear: blocky-soft, as the originals)."""
	s, ox, oy = rect
	w, h = max(1, int(round(m.sx * s))), max(1, int(round(m.sy * s)))
	img = Image.fromarray((np.clip(tiles, 0, 1) * 255).astype(np.uint8)).resize((w, h), Image.BILINEAR)
	a = gblur(np.asarray(img, np.float32) / 255.0, blur)
	full = np.zeros((SIZE, SIZE), np.float32)
	x0, y0 = int(round(ox)), int(round(oy))
	xs0, ys0 = max(0, -x0), max(0, -y0)
	xe, ye = min(w, SIZE - x0), min(h, SIZE - y0)
	if xe > xs0 and ye > ys0:
		full[y0 + ys0:y0 + ye, x0 + xs0:x0 + xe] = a[ys0:ye, xs0:xe]
	return full


def ink_pixels(m, tiles, rect, rng):
	"""Strokes on the frame: random half-tone blocks along the stroke edges (the originals' strokes swell
	and thin), then mottled like brushed ink. At scale 1 the strokes are the originals' blocky-soft ones;
	at higher scales the same tile shapes are drawn crisp: an anti-aliased edge on a slightly rough
	threshold, with the tone and grain of brushed ink."""
	halo = np.zeros_like(tiles)
	for _, _, n in neighbours8(tiles):
		halo = np.maximum(halo, n)
	halo = (halo - tiles) * (rng.random(tiles.shape) < 0.35) * rng.uniform(0.3, 0.7, tiles.shape)
	core = tiles * rng.uniform(0.8, 1.0, tiles.shape)
	mott = 0.62 + 0.38 * fractal(rng, SIZE, SIZE, octaves=4, start=12, persistence=0.75)
	if PX == 1:
		return np.clip(place(m, core + halo, rect) * mott * 1.1, 0, 1)
	field = place(m, core + halo, rect, blur=0.3 * PX)
	rough = fractal(rng, SIZE, SIZE, octaves=3, start=BASE, persistence=0.55)
	t = 0.45 + 0.10 * (rough - 0.5)
	shape = np.clip((field - t) / 0.16 + 0.5, 0, 1)              # about 2 output px of anti-aliased edge per PX 4
	tone = np.clip(place(m, core + halo, rect, blur=1.5 * PX) * 1.2, 0, 1)
	# dry-brush grain: fine noise stretched along a random slant, so the fill shows bristle streaks
	g = fractal(rng, SIZE // 2, SIZE * 2, octaves=3, start=BASE // 2, persistence=0.7)
	g = np.asarray(Image.fromarray((g * 255).astype(np.uint8)).resize((SIZE * 2, SIZE * 2), Image.BILINEAR).rotate(35, Image.BILINEAR), np.float32)[SIZE // 2:SIZE // 2 + SIZE, SIZE // 2:SIZE // 2 + SIZE] / 255.0
	grain = 0.70 + 0.30 * g
	wick = 0.24 * gblur(shape, 2.2 * PX)          # ink soaked into the paper beside the stroke
	ink = 0.95 * shape * (0.55 + 0.45 * tone) * grain * (0.75 + 0.25 * mott)
	return np.clip(np.maximum(ink, wick * mott), 0, 1)


# hatched terrain: tile sprite sheet -> hatch strength. Measured on the originals as the ink over blocked
# tiles of each sheet away from the traced strokes (rock and cliff sheets are hatched; grass, sand,
# roofs and water are not). Sheets not listed are not hatched.
HATCH_SHEETS = {28: 1.0, 29: 0.9, 14: 0.7, 340: 0.65, 25: 0.6, 18: 0.5, 26: 0.35}
HATCH_REACH = 12     # only blocked tiles this close (in tiles) to walkable land are hatched
HATCH_DARK = 0.85    # hatch dashes are this dark relative to a full stroke
HATCH_TONE = 0.30    # and the hatched ground between them this dark


def hatch_tiles(m):
	"""Hatch strength per tile: hatched sheets, blocked, near the walkable land."""
	w = np.zeros(m.blocked.shape, np.float32)
	for sid, v in HATCH_SHEETS.items():
		w[m.tile_spr == sid] = v
	near = ~m.blocked
	for _ in range(HATCH_REACH):
		g = near.copy()
		for _, _, n in neighbours8(near):
			g |= n
		near = g
	return w * (m.blocked & near)


def hatch_texture(rng):
	"""Embossed relief of staggered short dashes (the originals' rock hatching): returns (dark, light)."""
	dark = np.zeros((SIZE, SIZE), np.float32)
	period = 3 * PX
	for y in range(0, SIZE, period):
		x = -int(rng.integers(0, 6 * PX))
		while x < SIZE:
			length = int(rng.integers(3 * PX, 7 * PX))
			gap = int(rng.integers(1 * PX, 4 * PX))
			yy = y + (PX if rng.random() < 0.25 else 0)
			if 0 <= yy < SIZE:
				dark[yy:yy + max(1, (2 * PX) // 3), max(0, x):max(0, min(SIZE, x + length))] = 0.6 + 0.4 * rng.random()
			x += length + gap
	dark = gblur(dark, 0.55 * PX)
	light = np.zeros_like(dark)
	light[:-PX] = dark[PX:]                 # highlight just above each dash: the emboss
	light = np.clip(light - dark, 0, 1)
	return np.clip(dark * 1.6, 0, 1), light


# ---------------------------------------------------------------- paper

def patch_paper(rng):
	"""The old paper: quilted from bare-paper patches of the originals, built at 300 px. (rgb, alpha)."""
	global SIZE
	size_out = SIZE
	SIZE = BASE
	try:
		rgb = paper(rng)
		alpha = edge_alpha(rng)
		rgb = rgb * rim_shade(alpha)[:, :, None]
	finally:
		SIZE = size_out
	if SIZE != BASE:
		rgb = np.dstack([np.asarray(Image.fromarray(np.clip(rgb[:, :, c], 0, 255).astype(np.uint8)).resize((SIZE, SIZE), Image.LANCZOS), np.float32) for c in range(3)])
		alpha = np.asarray(Image.fromarray(alpha).resize((SIZE, SIZE), Image.LANCZOS), np.uint8)
	return rgb, alpha


EDGE_TINT = np.array([96, 58, 30], np.float32)   # burnt-brown the HQ paper's orange-red edge fringe is pulled to
EDGE_TAME = 0.75                                  # how far (0 keeps the fringe as drawn, 1 replaces it)


def image_paper(path, rng):
	"""A paper texture image: resampled premultiplied (no colour bleeds from transparent pixels), interior
	alpha made fully opaque, and its translucent torn-edge fringe tamed towards burnt brown. (rgb, alpha)."""
	img = Image.open(path).convert("RGBA")
	src = np.asarray(img, np.float32)
	a = src[:, :, 3]
	if a.min() >= 250:                        # no torn edge of its own: use one of the originals'
		a = np.asarray(Image.fromarray(edge_alpha(rng)).resize(img.size, Image.LANCZOS), np.float32)
	pre = np.dstack([src[:, :, :3] * (a[:, :, None] / 255.0), a])
	res = np.dstack([np.asarray(Image.fromarray(np.clip(pre[:, :, c], 0, 255).astype(np.uint8)).resize((SIZE, SIZE), Image.LANCZOS), np.float32) for c in range(4)])
	alpha = res[:, :, 3]
	rgb = res[:, :, :3] / np.maximum(alpha[:, :, None] / 255.0, 1e-3)
	# interior alpha: the texture's opaque level (252 in the shipped paper) becomes 255
	solid = np.percentile(alpha[alpha > 0], 90)
	alpha = np.clip(np.rint(alpha * (255.0 / max(solid, 1.0))), 0, 255)
	alpha[alpha >= 248] = 255                 # resampling leaves 250-254 inside: the paper is solid there
	# fringe: the translucent rim carries saturated orange-red; over a dark background it reads as a glow
	w = EDGE_TAME * np.clip(1.0 - alpha / 255.0, 0, 1) ** 0.5
	w = np.maximum(w, EDGE_TAME * (alpha < 128))
	rgb = rgb * (1 - w[:, :, None]) + EDGE_TINT * w[:, :, None]
	return np.clip(rgb, 0, 255), alpha.astype(np.uint8)


def paper(rng):
	meta = json.loads((DATA / "paper_patches.json").read_text())
	P, n, cols = meta["patch"], meta["count"], meta["cols"]
	atlas = np.asarray(Image.open(DATA / "paper_patches.png").convert("RGB"), np.float32)
	patches = [atlas[(i // cols) * P:(i // cols + 1) * P, (i % cols) * P:(i % cols + 1) * P] for i in range(n)]
	overlap = P // 3
	step = P - overlap
	acc = np.zeros((SIZE + 2 * P, SIZE + 2 * P, 3), np.float32)
	wsum = np.zeros((SIZE + 2 * P, SIZE + 2 * P, 1), np.float32)
	ramp = np.minimum(1.0, np.minimum(np.arange(P) + 1, P - np.arange(P)) / overlap).astype(np.float32)
	weight = (ramp[:, None] * ramp[None, :])[:, :, None]
	for y in range(-overlap, SIZE, step):
		for x in range(-overlap, SIZE, step):
			p = np.rot90(patches[rng.integers(n)], rng.integers(4))
			if rng.random() < 0.5:
				p = p[:, ::-1]
			yy, xx = y + overlap, x + overlap
			acc[yy:yy + P, xx:xx + P] += p * weight
			wsum[yy:yy + P, xx:xx + P] += weight
	rgb = (acc / np.maximum(wsum, 1e-6))[overlap:overlap + SIZE, overlap:overlap + SIZE]
	# large, soft stains
	stain = fractal(rng, SIZE, SIZE, octaves=3, start=3, persistence=0.5)
	rgb *= (0.93 + 0.12 * stain)[:, :, None]
	rgb *= facets(rng)[:, :, None]
	# blending overlapping patches flattens the crumple a little: restore its contrast
	mean = gblur(rgb.mean(2), 6.0)[:, :, None]
	rgb = mean + (rgb - mean) * PAPER_CONTRAST
	if "mean_rgb" in meta:
		rgb *= (np.array(meta["mean_rgb"], np.float32) / rgb.reshape(-1, 3).mean(0))[None, None, :]
	return rgb


def facets(rng, cells=110, amp=0.07):
	"""Large crumple facets: flat-lit Voronoi cells, each tilted its own way (the patches carry the fine creases)."""
	pts = rng.random((cells, 2)) * SIZE
	tilt = rng.normal(0.0, 1.0, cells).astype(np.float32)
	yy, xx = np.mgrid[0:SIZE, 0:SIZE].astype(np.float32)
	best = np.full((SIZE, SIZE), 1e9, np.float32)
	owner = np.zeros((SIZE, SIZE), np.int32)
	for i, (px, py) in enumerate(pts):
		d = (xx - px) ** 2 + (yy - py) ** 2
		closer = d < best
		owner[closer] = i
		best[closer] = d[closer]
	return 1.0 + amp * gblur(np.clip(tilt[owner], -2, 2) / 2, 0.8)


def edge_alpha(rng):
	files = sorted(DATA.glob("edge_alpha_*.png"))
	return np.asarray(Image.open(files[rng.integers(len(files))]).convert("L"), np.uint8)


def rim_shade(alpha):
	prof = json.loads((DATA / "rim_profile.json").read_text())["lum_ratio_by_distance"][:16]
	prof = np.array(prof + [1.0], np.float32)
	prof = np.minimum(prof / max(prof[8:16].max(), 1e-6), 1.0)
	mask = alpha > 0
	dist = np.zeros(mask.shape, np.int32)
	cur = mask.copy()
	for d in range(len(prof) - 1):
		dist[cur] = d
		er = cur.copy()
		er[1:, :] &= cur[:-1, :]; er[:-1, :] &= cur[1:, :]
		er[:, 1:] &= cur[:, :-1]; er[:, :-1] &= cur[:, 1:]
		cur = er
	dist[cur] = len(prof) - 1
	return prof[dist]


# ---------------------------------------------------------------- map database (optional)

def sql_rows(path, table):
	"""Rows of `INSERT INTO "table" (cols) VALUES (...)` lines as dicts (ints where they parse)."""
	rows = []
	pat = re.compile(r'INSERT INTO "?%s"? \(([^)]*)\) VALUES \((.*)\);\s*$' % re.escape(table))
	if not Path(path).exists():
		return rows
	for line in Path(path).read_text(encoding="utf-8", errors="replace").splitlines():
		mt = pat.match(line)
		if not mt:
			continue
		cols = [c.strip().strip('"') for c in mt.group(1).split(",")]
		vals, cur, q = [], "", False
		for ch in mt.group(2):
			if ch == "'" and not q:
				q = True
			elif ch == "'" and q:
				q = False
			elif ch == "," and not q:
				vals.append(cur.strip())
				cur = ""
				continue
			cur += ch
		vals.append(cur.strip())
		row = {}
		for c, v in zip(cols, vals):
			v = v.strip("'") if v.startswith("'") else v
			try:
				row[c] = int(v)
			except ValueError:
				row[c] = v
		rows.append(row)
	return rows


class MapInfo:
	"""Teleport destinations, spawn generators and map names from sources/database/mapinfo (all optional)."""

	def __init__(self, spec, map_name):
		self.name = map_name
		d = Path(spec)
		if not d.is_dir():                      # a bare map name: rows for that name from the repo database
			self.name = spec.lower()
			d = REPO / "sources" / "database" / "mapinfo"
		self.teleports = [r for r in sql_rows(d / "map_teleport_locations.data.sql", "map_teleport_locations") if r.get("map_name") == self.name]
		self.spawns = [r for r in sql_rows(d / "map_spot_mob_generators.data.sql", "map_spot_mob_generators") if r.get("map_name") == self.name]
		self.maps = {r["map_name"]: r for r in sql_rows(d / "maps.data.sql", "maps")}


# ---------------------------------------------------------------- icons

ICON_DIR = DATA / "icons"
# building behind a door, by destination map name (checked against the SDL building art over each door);
# the name prefix decides, the database display name only for a destination no prefix matches
BUILDING_PREFIXES = [("bsmith", "blacksmith"), ("gshop", "shop"), ("wrhus", "warehouse"), ("arewrhus", "warehouse"),
                     ("elvwrhus", "warehouse"), ("cityhall", "city_hall"), ("cath", "cathedral"), ("gldhall", "guild_hall"),
                     ("cmdhall", "command_hall"), ("arebrk", "barrack"), ("elvbrk", "barrack"), ("wzdtwr", "wizard_tower"),
                     ("arejail", "jail"), ("elvjail", "jail")]
BUILDING_WORDS = [("blacksmith", "blacksmith"), ("shop", "shop"), ("warehouse", "warehouse"), ("cityhall", "city_hall"),
                  ("city hall", "city_hall"), ("cathedral", "cathedral"), ("church", "cathedral"), ("guildhall", "guild_hall"),
                  ("guild hall", "guild_hall"), ("command hall", "command_hall"), ("barrack", "barrack"),
                  ("magic tower", "wizard_tower"), ("wizard tower", "wizard_tower"), ("jail", "jail")]
# the old cut-out drawn for each building with --icons (there are only six)
OLD_BUILDING_ICON = {"blacksmith": "bld_blacksmith", "shop": "bld_shop", "warehouse": "bld_warehouse",
                     "city_hall": "bld_city_hall", "cathedral": "bld_city_hall", "guild_hall": "bld_guild_hall",
                     "command_hall": "bld_guild_hall", "barrack": "bld_guild_hall", "wizard_tower": "bld_wizard_tower",
                     "jail": "bld_warehouse"}
DUNGEON_WORDS = ("dungeon", "mine", "tower of hell")
DUNGEON_NAME = re.compile(r"(d1[nx]?$|^dglv|^toh\d|dungeon)")
SWORD_MERGE_PX = 25      # spawn areas closer than this on the frame share one sword
SWORD_MAX = 6
EDGE_BAND = 0.08         # teleports this close to the edge of the land (share of its size) are exits


def icon(name):
	return Image.open(ICON_DIR / f"{name}.png").convert("RGBA")


def arrow(direction):
	"""Red exit arrow pointing in one of 8 directions (0 = north, clockwise)."""
	if direction % 2 == 0:
		base, turns = icon("arrow_n"), direction // 2
	else:
		base, turns = icon("arrow_nw"), ((direction + 1) // 2) % 4
	return base.rotate(-90 * turns, expand=True) if turns else base


def teleport_clusters(m):
	"""Groups of teleport tiles (gaps up to 2 tiles join a group): [(tiles list)]."""
	t = m.teleport
	seen = np.zeros_like(t)
	out = []
	for y0, x0 in zip(*np.nonzero(t)):
		if seen[y0, x0]:
			continue
		stack, comp = [(y0, x0)], []
		seen[y0, x0] = True
		while stack:
			y, x = stack.pop()
			comp.append((y, x))
			for ny in range(y - 2, y + 3):
				for nx in range(x - 2, x + 3):
					if 0 <= ny < m.sy and 0 <= nx < m.sx and t[ny, nx] and not seen[ny, nx]:
						seen[ny, nx] = True
						stack.append((ny, nx))
		out.append(comp)
	return out


def classify_destination(dest, info):
	"""(kind, detail) for a teleport destination map name: ('dungeon', 'dungeon'), ('building', <building>)
	or ('outdoor', None). The name prefix decides the building; the database display name is the fallback."""
	row = info.maps.get(dest, {}) if info else {}
	label = str(row.get("display_name", "")).lower()
	if any(w in label for w in DUNGEON_WORDS) or DUNGEON_NAME.search(dest):
		return "dungeon", "dungeon"
	for p, building in BUILDING_PREFIXES:
		if dest.startswith(p):
			return "building", building
	for w, building in BUILDING_WORDS:
		if w in label:
			return "building", building
	return "outdoor", None


def is_barrack(name):
	return name.startswith(("arebrk", "elvbrk"))


def place_icons(m, rect, info):
	"""Decide the icon placements: list of dicts {kind, icon, x, y, anchor ('center'|'bottom'), tile, dest,
	...} in output pixels, and readable notes. kind: exit | portal | building | stairs (a barrack floor
	change) | entrance | monster_area."""
	ys, xs = np.nonzero(~m.blocked)
	if len(xs) == 0:
		return [], []
	wx0, wx1, wy0, wy1 = xs.min(), xs.max(), ys.min(), ys.max()
	cx, cy = (wx0 + wx1) / 2, (wy0 + wy1) / 2
	edge_band = max(6, EDGE_BAND * max(wx1 - wx0, wy1 - wy0))
	dest_at = {}
	if info:
		for r in info.teleports:
			dest_at[(r["src_y"], r["src_x"])] = str(r["dest_map_name"]).lower()
	placed, notes = [], []
	for comp in teleport_clusters(m):
		ty = float(np.mean([p[0] for p in comp]))
		tx = float(np.mean([p[1] for p in comp]))
		dests = [dest_at[p] for p in comp if p in dest_at]
		dest = max(sorted(set(dests)), key=dests.count) if dests else None
		near_edge = min(tx - wx0, wx1 - tx, ty - wy0, wy1 - ty) < edge_band
		if dest:
			kind, ic = classify_destination(dest, info)
			faction = str(info.maps.get(dest, {}).get("location_name", dest)).lower() if info else dest
		else:
			# no database: an exit on the edge of the land is an arrow; a pad in open ground is a portal; a
			# doorway hemmed in by blocked tiles gets the neutral entrance marker (the originals use it for
			# dungeon mouths and some portals), since without the destination it may be a shop or a stair
			y0, y1 = int(max(0, ty - 3)), int(min(m.sy, ty + 4))
			x0, x1 = int(max(0, tx - 3)), int(min(m.sx, tx + 4))
			openness = 1 - m.blocked[y0:y1, x0:x1].mean()
			kind = "outdoor" if near_edge or openness > 0.7 else "dungeon"
			ic, faction = None, ""
		px, py = to_px(rect, tx, ty)
		base = {"tile": [round(tx, 1), round(ty, 1)], "tiles": len(comp), "dest": dest}
		if kind == "outdoor" and near_edge:
			ang = np.degrees(np.arctan2(tx - cx, -(ty - cy))) % 360
			d = int(round(ang / 45)) % 8
			# pull the arrow a little inside the land, as the originals draw it
			dx, dy = np.sin(np.radians(d * 45)), -np.cos(np.radians(d * 45))
			placed.append(dict(base, kind="exit", icon=f"arrow_{d * 45}", direction_deg=d * 45,
			                   x=px - dx * 6 * PX, y=py - dy * 6 * PX, anchor="center"))
			notes.append(f"exit arrow dir {d * 45} at tile ({tx:.0f},{ty:.0f})" + (f" -> {dest}" if dest else ""))
		elif kind == "outdoor":
			side = "elvine" if faction.startswith("elv") else ("aresden" if faction.startswith("are") else "")
			placed.append(dict(base, kind="portal", icon="portal_blue" if side == "elvine" else "portal_red",
			                   faction=side or None, x=px, y=py + 2 * PX, anchor="bottom"))
			notes.append(f"portal at tile ({tx:.0f},{ty:.0f})" + (f" -> {dest}" if dest else ""))
		elif kind == "building" and ic == "barrack" and is_barrack(m.name):
			# a floor change inside the barracks (one barrack map to another): stairs, not a building
			placed.append(dict(base, kind="stairs", icon="dungeon", x=px, y=py, anchor="center"))
			notes.append(f"stairs at tile ({tx:.0f},{ty:.0f}) -> {dest}")
		elif kind == "dungeon":
			placed.append(dict(base, kind="entrance", icon="dungeon", x=px, y=py, anchor="center"))
			notes.append(f"dungeon at tile ({tx:.0f},{ty:.0f})" + (f" -> {dest}" if dest else ""))
		else:
			placed.append(dict(base, kind="building", icon=OLD_BUILDING_ICON[ic], building=ic, x=px, y=py + 2 * PX, anchor="bottom"))
			notes.append(f"building {ic} at tile ({tx:.0f},{ty:.0f})" + (f" -> {dest}" if dest else ""))
	# swords: spawn areas (not single-tile spot spawns), merged when close on the frame
	if info and info.spawns:
		pts = []
		for r in info.spawns:
			w, h = r.get("tile_w", 0), r.get("tile_h", 0)
			# single-tile spots and guard posts are not monster areas; nor is a generator covering the whole map
			if w * h <= 1 or r.get("max_mobs", 0) <= 2 or w * h > 0.25 * m.sx * m.sy:
				continue
			pts.append([*to_px(rect, r["tile_x"] + w / 2 - 0.5, r["tile_y"] + h / 2 - 0.5), w * h, r])
		groups = []
		for p in sorted(pts, key=lambda p: -p[2]):
			for g in groups:
				if np.hypot(g[0] - p[0], g[1] - p[1]) < SWORD_MERGE_PX * PX:
					tot = g[2] + p[2]
					g[0], g[1], g[2] = (g[0] * g[2] + p[0] * p[2]) / tot, (g[1] * g[2] + p[1] * p[2]) / tot, tot
					g[3].append(p[3])
					break
			else:
				groups.append([p[0], p[1], p[2], [p[3]]])
		for g in sorted(groups, key=lambda g: -g[2])[:SWORD_MAX]:
			gens = [{"generator": r.get("generator_index"), "npc_config_id": r.get("npc_config_id"),
			         "rect": [r["tile_x"], r["tile_y"], r["tile_w"], r["tile_h"]]} for r in g[3]]
			placed.append({"kind": "monster_area", "icon": "sword", "x": g[0], "y": g[1], "anchor": "center",
			               "area_tiles": g[2], "generators": gens})
			notes.append(f"monster area at px ({g[0]:.0f},{g[1]:.0f})")
	return placed, notes


def icon_image(p):
	"""The old cut-out icon for a placement, at the output scale (LANCZOS-resampled above scale 1)."""
	if p["kind"] == "exit":
		im = arrow(p["direction_deg"] // 45)
	else:
		im = icon(p["icon"])
	return im.resize((im.width * PX, im.height * PX), Image.LANCZOS) if PX > 1 else im


def draw_icons(img, placed):
	"""Paste the old cut-out icons (only with --icons), skipping one that would mostly cover an icon already drawn."""
	boxes = []
	for p in placed:
		im, x, y = icon_image(p), p["x"], p["y"]
		left = int(round(x - im.width / 2))
		top = int(round(y - im.height / 2 if p["anchor"] == "center" else y - im.height))
		left = min(max(left, 14 * PX), SIZE - 14 * PX - im.width)
		top = min(max(top, 14 * PX), SIZE - 14 * PX - im.height)
		box = (left, top, left + im.width, top + im.height)
		clash = False
		for b in boxes:
			ix = max(0, min(b[2], box[2]) - max(b[0], box[0]))
			iy = max(0, min(b[3], box[3]) - max(b[1], box[1]))
			if ix * iy > 0.5 * im.width * im.height:
				clash = True
				break
		if clash:
			continue
		boxes.append(box)
		img.alpha_composite(im, (left, top))
	return len(boxes)


# ---------------------------------------------------------------- compose

def compose(m, args):
	rng = np.random.default_rng(args.seed)
	rect = frame_rect(m)
	col = None
	if args.minimap:
		try:
			col, known = minimap_colour(m, args.minimap), 1.0
			source = "minimap"
		except FileNotFoundError as e:
			print(f"note: {e}; using the tile palette instead")
	if col is None:
		col, known = colour_map(m, load_palette())
		source = "palette"
	# strokes are about INK_DEPTH tiles at the originals' 1.0-1.45 px/tile; when zoomed in further (interiors),
	# draw on sub-tiles so strokes keep that width in pixels. The stroke shapes are decided at scale-1 pixel
	# sizes (rect[0] / PX), so a --scale 4 map has the same strokes as scale 1, only drawn finer.
	k = max(1, int(round(rect[0] / PX / STROKE_PX_PER_TILE)))
	mk, colk, rectk = (m.upsampled(k), np.repeat(np.repeat(col, k, 0), k, 1), (rect[0] / k, rect[1], rect[2])) if k > 1 else (m, col, rect)
	depth = max(INK_DEPTH, int(round(STROKE_MIN_PX * PX / rectk[0])))
	tiles = ink_tiles(mk, colk, depth)
	stroke = ink_pixels(mk, tiles, rectk, rng)
	hmask = place(mk, hatch_tiles(mk), rectk, blur=1.0 * PX)
	hdark, hlight = hatch_texture(rng)
	ink = np.maximum(stroke, hmask * (HATCH_TONE + (HATCH_DARK - HATCH_TONE) * hdark))
	# a zoomed-in interior must not run ink into the margin: fade it out at the BOX square
	lo, hi = (SIZE - BOX) / 2, (SIZE + BOX) / 2
	r = np.arange(SIZE, dtype=np.float32)
	ramp = np.clip(np.minimum(r - lo, hi - r) / (6.0 * PX) + 0.5, 0, 1)
	boxmask = np.minimum(ramp[:, None], ramp[None, :])
	ink *= boxmask
	hmask *= boxmask
	stroke *= boxmask
	paper_choice = getattr(args, "paper", None) or "hq"
	if paper_choice == "patches":
		rgb, alpha = patch_paper(rng)
	else:
		rgb, alpha = image_paper(DATA / HQ_PAPER if paper_choice == "hq" else Path(paper_choice), rng)
	layer = ink_layer(ink, alpha, paper_choice != "patches") if getattr(args, "ink_layer", None) else None
	k_ink = (1.0 - (1.0 - INK_FLOOR) * ink)[:, :, None]
	if paper_choice == "patches":
		rgb = rgb * k_ink
	else:
		# the HQ paper is lighter and warmer than the originals: lean the ink towards dark brown as well
		rgb = rgb * k_ink * (1 - 0.3 * ink[:, :, None]) + INK_RGB * 0.3 * ink[:, :, None]
	rgb = rgb * (1.0 + 0.18 * (hmask * hlight * (1 - stroke)))[:, :, None]
	img = Image.fromarray(np.dstack([np.clip(rgb, 0, 255).astype(np.uint8), alpha]), "RGBA")
	minfo = MapInfo(args.mapinfo, m.name) if args.mapinfo else None
	placed, notes = place_icons(m, rect, minfo)
	drawn = 0
	if getattr(args, "icons", False):
		drawn = draw_icons(img, placed)
		# icons never reach past the torn edge
		a = np.asarray(img, np.uint8).copy()
		a[:, :, 3] = alpha
		img = Image.fromarray(a, "RGBA")
	info = {"source": source, "known": known, "rect": rect, "ink": ink, "icons": drawn, "notes": notes,
	        "placements": placed, "paper": paper_choice, "ink_layer": layer}
	return img, info


def ink_layer(ink, paper_alpha, lean):
	"""The ink alone as straight-alpha RGBA, so drawing it over the paper (source-over) gives compose()'s
	multiply: paper * f + K = paper * (1 - a) + C * a with a = 1 - f and C = K / a. lean is the HQ paper's
	lean towards INK_RGB (C is then dark brown); without it (patches) the ink is a black multiply.
	Not carried: the hatching's emboss highlight, which lightens the paper. Coverage is cut by the
	paper's alpha so nothing shows past the torn edge."""
	d = 1.0 - INK_FLOOR
	f = 1.0 - d * ink
	if lean:
		f = f * (1 - 0.3 * ink)
	cut = paper_alpha.astype(np.float32) / 255.0
	a8 = np.rint(np.clip(1.0 - f, 0, 1) * cut * 255.0)
	if not lean:
		return Image.fromarray(np.dstack([np.zeros(ink.shape + (3,), np.uint8), a8.astype(np.uint8)]), "RGBA")
	# C from the stored alpha alone (ink solved back from a = (1.3 - F) i - 0.3 (1 - F) i^2), so RGB is a
	# function of alpha: the same look as C from the float ink, and a much smaller PNG
	a = np.where(cut > 0, a8 / 255.0 / np.maximum(cut, 1e-6), 0.0)
	b, q = 1.3 - INK_FLOOR, 0.3 * d
	i = (b - np.sqrt(np.maximum(b * b - 4 * q * a, 0))) / (2 * q)
	# where the coverage is nil: the limit of K / a as the ink goes to 0, so filtering shows no fringe
	rgb = np.where(a[:, :, None] > 0, INK_RGB * 0.3 * i[:, :, None] / np.maximum(a, 1e-6)[:, :, None], INK_RGB * 0.3 / b)
	return Image.fromarray(np.dstack([np.clip(np.rint(rgb), 0, 255).astype(np.uint8), a8.astype(np.uint8)]), "RGBA")


def placements_json(m, info, args):
	"""The icon placements for a later icon kit: positions in output pixels of the written PNG."""
	def clean(v):
		if isinstance(v, (np.floating, float)):
			return round(float(v), 1)
		if isinstance(v, (np.integer,)):
			return int(v)
		if isinstance(v, dict):
			return {k: clean(x) for k, x in v.items()}
		if isinstance(v, (list, tuple)):
			return [clean(x) for x in v]
		return v
	return {
		"map": m.name, "image": Path(args.out).name, "size": SIZE, "scale": PX,
		"tiles": [m.sx, m.sy], "px_per_tile": round(float(info["rect"][0]), 4),
		"tile_to_px": "x = %.3f + (tile_x + 0.5) * %.4f, y = %.3f + (tile_y + 0.5) * %.4f" % (info["rect"][1], info["rect"][0], info["rect"][2], info["rect"][0]),
		"origin_x": round(float(info["rect"][1]), 4), "origin_y": round(float(info["rect"][2]), 4),
		"anchor_note": "center: icon centred on (x, y); bottom: icon's bottom edge centred on (x, y)",
		"mapinfo": bool(args.mapinfo),
		"placements": clean(info["placements"]),
	}


# ---------------------------------------------------------------- review sheet

def reference_ink(ref):
	"""Ink estimate of an original frame: darker than the local paper, wider than a crease; labels/icons masked."""
	rgb = np.asarray(ref.convert("RGBA"), np.float32)
	lum0 = rgb[:, :, :3].mean(2)
	sat = rgb[:, :, :3].max(2) - rgb[:, :, :3].min(2)
	lum = np.asarray(Image.fromarray(lum0.astype(np.uint8)).filter(ImageFilter.MedianFilter(3)), np.float32)
	base = np.asarray(Image.fromarray(lum.astype(np.uint8)).filter(ImageFilter.GaussianBlur(10)), np.float32)
	ink = np.clip(base - lum, 0, 255) / np.maximum(base, 1)
	bad = (lum0 > 205) | (sat > 110) | (rgb[:, :, 2] > rgb[:, :, 0] + 10) | (rgb[:, :, 3] < 255)
	bad = np.asarray(Image.fromarray((bad * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(9)), np.float32) > 0
	return np.clip(ink / (1 - INK_FLOOR), 0, 1), bad


def ncc(a, b, mask):
	x = a[mask] - a[mask].mean()
	y = b[mask] - b[mask].mean()
	return float((x * y).sum() / (np.sqrt((x * x).sum() * (y * y).sum()) + 1e-9))


def reference_score(info, ref_path):
	"""(reference image, its ink, ink NCC of the generated map against it)."""
	ref = Image.open(ref_path).convert("RGBA")
	if ref.size != (BASE, BASE):
		raise ValueError(f"{ref_path}: reference must be {BASE}x{BASE}, got {ref.size[0]}x{ref.size[1]}")
	rink, bad = reference_ink(ref)
	gink = info["ink"]
	if gink.shape[0] != BASE:                 # score a scaled render on the original's 300 px grid
		gink = np.asarray(Image.fromarray(gink.astype(np.float32), "F").resize((BASE, BASE), Image.BOX), np.float32)
	return ref, rink, ncc(gaussian(gink, 1.5), gaussian(rink, 1.5), ~bad)


def write_sheet(img, info, ref_path, out):
	panels = [img]
	score = None
	if ref_path:
		ref, rink, score = reference_score(info, ref_path)
		gink = info["ink"]
		if gink.shape[0] != BASE:
			gink = np.asarray(Image.fromarray(gink.astype(np.float32), "F").resize((BASE, BASE), Image.BOX), np.float32)
		ov = np.zeros((BASE, BASE, 4), np.uint8)
		ov[:, :, 3] = 255
		ov[:, :, 0] = np.clip(gaussian(rink, 1.0) * 400, 0, 255)          # red: original ink
		ov[:, :, 1] = np.clip(gaussian(gink, 1.0) * 400, 0, 255)          # green: generated ink
		up = lambda im: im.resize((SIZE, SIZE), Image.NEAREST) if SIZE != BASE else im
		panels = [up(ref), img, up(Image.fromarray(ov, "RGBA"))]
	gap = 8 * PX
	sheet = Image.new("RGBA", (len(panels) * (SIZE + gap) - gap, SIZE), (40, 40, 40, 255))
	for i, p in enumerate(panels):
		sheet.alpha_composite(p, (i * (SIZE + gap), 0))
	if PX == 1:
		sheet = sheet.resize((sheet.width * 2, sheet.height * 2), Image.NEAREST)
	Path(out).parent.mkdir(parents=True, exist_ok=True)
	sheet.save(out)
	return score


def gaussian(a, r):
	return gblur(a.astype(np.float32), r)


def main(argv=None):
	ap = argparse.ArgumentParser(description="Make a parchment map from a .amd file.")
	ap.add_argument("amd")
	ap.add_argument("-o", "--out", required=True)
	ap.add_argument("--minimap")
	ap.add_argument("--mapinfo")
	ap.add_argument("--seed", type=int, default=1)
	ap.add_argument("--sheet")
	ap.add_argument("--reference")
	ap.add_argument("--scale", type=int, default=1, help="output is 300*N px square (1 = the client's size, 4 = 1200)")
	ap.add_argument("--paper", default="hq", help="'hq' (default, the shipped high-quality parchment), 'patches' (the old quilted paper) or an image path")
	ap.add_argument("--icons-json", help="write the icon placements (exits, portals, buildings, entrances, monster areas) to this JSON file")
	ap.add_argument("--icons", action="store_true", help="also draw the old cut-out icons (off by default; an icon kit will replace them)")
	ap.add_argument("--ink-layer", help="also write the ink alone as a transparent PNG (drawn over the paper it gives the map, less the hatch emboss)")
	ap.add_argument("-v", "--verbose", action="store_true", help="list every icon placement and why")
	args = ap.parse_args(argv)
	try:
		configure(args.scale)
		if args.paper not in ("hq", "patches") and not Path(args.paper).is_file():
			raise ValueError(f"--paper {args.paper}: no such image (use hq, patches or an image path)")
		m = MapData(args.amd)
		img, info = compose(m, args)
		Path(args.out).parent.mkdir(parents=True, exist_ok=True)
		img.save(args.out)
		msg = f"{m.name}: {m.sx}x{m.sy} tiles, colour {info['source']} ({info['known'] * 100:.0f}% known), {SIZE}px, {info['rect'][0]:.3f} px/tile, paper {info['paper']} -> {args.out}"
		msg += f", placements {len(info['placements'])}" + (f", icons drawn {info['icons']}" if args.icons else "")
		if args.icons_json:
			Path(args.icons_json).parent.mkdir(parents=True, exist_ok=True)
			Path(args.icons_json).write_text(json.dumps(placements_json(m, info, args), indent=1), encoding="utf-8")
		if args.ink_layer:
			Path(args.ink_layer).parent.mkdir(parents=True, exist_ok=True)
			info["ink_layer"].save(args.ink_layer, optimize=True)
			msg += f", ink layer -> {args.ink_layer}"
		for n in info["notes"] if args.verbose else []:
			print("   " + n)
		score = None
		if args.sheet:
			score = write_sheet(img, info, args.reference, args.sheet)
		elif args.reference:
			score = reference_score(info, args.reference)[2]
		if score is not None:
			msg += f", ink ncc vs reference {score:.3f}"
	except (OSError, ValueError, ZeroDivisionError) as e:
		print(f"error: {e}", file=sys.stderr)
		return 2
	print(msg)
	return 0


if __name__ == "__main__":
	sys.exit(main())
