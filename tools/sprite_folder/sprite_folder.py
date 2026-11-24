"""Read and write Helbreath sprite folders (the extracted form of the old .pak files).

A sprite folder is a directory holding manifest.json plus one image file per non-empty sprite
(000.png, 001.bmp, ...). The manifest schema is the one .claude/tools/scripts/pak_extract.py wrote:
  sprites[i] = {"index", "rect_count", "frames": [[x, y, w, h, pivot_x, pivot_y], ...],
                "color_key_u32", "image": {"file", "format", "width", "height", ...}}
  an empty sprite has "empty": true and no frames or image (the client skips it).
A sprite tree has an index.json: {"assets": {"npcs/ogre": {"dir", "sprite_count", "manifest"}, ...}}.

Two trees exist: STAGED_TREE (staged/client/sprites, what the client ships) and TOOLS_TREE
(tools/display_id_manager/sprites, the per-item equipment sprites the display-id tools edit).

Reading (stdlib + Pillow; numpy only for Sprite.rgba):
  sf = open_asset("npcs/ogre")                    # by index name, in STAGED_TREE
  sf = open_asset("items/dagger", TOOLS_TREE)
  sf = open_folder(path)                          # any folder with a manifest.json
  sf.sprites[i].rectangles                        # [Rect(x, y, width, height, pivot_x, pivot_y)]
  sf.sprites[i].get_image()                       # PIL image, bytes exactly as stored
  sf.sprites[i].rgba()                            # RGBA with the colour key turned transparent
The sprites list, Sprite.rectangles, Sprite.image_data, Sprite.get_image() and Sprite.from_image()
deliberately mirror the retired paklib API, so ported tools change little.

Writing: write_folder(out_dir, sprites) then update_index(tree, name). write_folder stamps the manifest
with tool "pak_extract" (the format id sources/build_tools/gen_sprite_index.py accepts), reads every image
before writing anything (so a folder may be rewritten from its own sprites) and deletes nothing: it
returns the NNN.* image/.bin files in the folder that the new manifest does not name, for the caller to report.
Names taken from data (asset names, index dirs, image files) must be plain relative paths: no "..", no
leading "/" or "\\", no backslash, no ":".
ASCII-only output; every file is read and written with explicit encodings.
"""

import io
import json
import os
import re
import struct
from dataclasses import dataclass
from pathlib import Path

from PIL import Image

REPO = Path(__file__).resolve().parent.parent.parent
STAGED_TREE = REPO / "staged" / "client" / "sprites"
TOOLS_TREE = REPO / "tools" / "display_id_manager" / "sprites"

TOOL_ID = "pak_extract"  # the "tool" value every sprite manifest and index.json carries (the format id, kept as is)
FORMAT_VERSION = 1
_SIDE_FILE = re.compile(r"\d{3,}\.(bmp|png|jpg|tail\.bin|raw\.bin|bin)")
FRAME_FIELDS = ["x", "y", "w", "h", "pivot_x", "pivot_y"]


@dataclass
class Rect:
	"""One frame: a rectangle inside the sprite's image plus its pivot."""
	x: int
	y: int
	width: int
	height: int
	pivot_x: int = 0
	pivot_y: int = 0

	@property
	def w(self):
		return self.width

	@property
	def h(self):
		return self.height

	def as_list(self):
		return [self.x, self.y, self.width, self.height, self.pivot_x, self.pivot_y]


SpriteRectangle = Rect  # name the retired paklib used


class Sprite:
	"""One sprite: frames plus an image held in memory or read lazily from the folder."""

	def __init__(self, rectangles=None, image_data=None, color_key=0, path=None, index=0):
		self.rectangles = list(rectangles or [])
		self.color_key = color_key
		self.index = index
		self._data = image_data
		self._path = path

	@property
	def empty(self):
		return not self.rectangles

	@property
	def image_data(self):
		"""The image bytes exactly as stored (b'' for an empty sprite)."""
		if self._data is None:
			self._data = Path(self._path).read_bytes() if self._path else b""
		return self._data

	def get_image(self):
		"""PIL image decoded from the stored bytes; None for an empty sprite."""
		data = self.image_data
		if not data:
			return None
		im = Image.open(io.BytesIO(data))
		im.load()
		return im

	def rgba(self):
		"""RGBA image with the colour key transparent (BMP sheets; PNG sheets are taken as they are).
		Key rule (PAKLib README): a nonzero key field is the key colour (bytes 00 R G B in file order),
		otherwise the top-left pixel is the key, as the game client does. None for an empty sprite."""
		data = self.image_data
		if not data:
			return None
		if data[:8] == b"\x89PNG\r\n\x1a\n":
			return Image.open(io.BytesIO(data)).convert("RGBA")
		import numpy
		fixed = _repair_bmp(data)
		try:
			im = Image.open(io.BytesIO(fixed))
			im.load()
		except Exception:
			fixed = _repad_24bpp(fixed)
			im = Image.open(io.BytesIO(fixed))
			im.load()
		rgb = im.convert("RGB")
		arr = numpy.array(rgb.convert("RGBA"))

		def key_mask(c):
			return (arr[:, :, 0] == c[0]) & (arr[:, :, 1] == c[1]) & (arr[:, :, 2] == c[2])

		mask = None
		key = self.color_key
		if key:
			m = key_mask(((key >> 8) & 255, (key >> 16) & 255, (key >> 24) & 255))
			if m.any():
				mask = m
		if mask is None:
			mask = key_mask(rgb.getpixel((0, 0)))
		arr[mask] = (0, 0, 0, 0)
		return Image.fromarray(arr, "RGBA")

	@classmethod
	def from_image(cls, image, rectangles):
		"""Sprite holding image as PNG bytes."""
		buf = io.BytesIO()
		image.save(buf, format="PNG")
		return cls(rectangles, buf.getvalue())


def _repair_bmp(img):
	"""Restore the BMP header fields the Korea 2024 encryption garbled (bytes 12-17, 'BM')."""
	d = bytearray(img)
	if len(d) < 54:
		return bytes(d)
	if d[0:2] != b"BM":
		d[0:2] = b"BM"
	d[12:14] = b"\x00\x00"
	d[14:18] = struct.pack("<I", 40)
	if len(d) % 4 == 0:
		d[-2:] = b"\x00\x00"
	return bytes(d)


def _repad_24bpp(d):
	"""Some 24bpp bitmaps store rows unpadded; pad rows to 4 bytes."""
	w, h = struct.unpack_from("<ii", d, 18)
	bpp = struct.unpack_from("<H", d, 28)[0]
	off = struct.unpack_from("<I", d, 10)[0]
	if bpp != 24:
		raise ValueError("cannot repair non-24bpp bitmap")
	h = abs(h)
	row = w * 3
	if len(d) - off != row * h:
		raise ValueError("size does not match unpadded rows")
	pad = b"\x00" * ((4 - row % 4) % 4)
	body = b"".join(d[off + y * row:off + (y + 1) * row] + pad for y in range(h))
	return bytes(d[:off]) + body


def safe_rel(name):
	"""name when it is a plain relative path taken from data, else ValueError (no '..', no leading '/' or
	'\\', no backslash, no ':')."""
	if (not isinstance(name, str) or not name or ".." in name or name[0] in "/\\" or "\\" in name or ":" in name):
		raise ValueError("unsafe path in sprite data: %r" % (name,))
	return name


class SpriteFolder:
	"""An opened sprite folder: .path, .name, .manifest, .sprites (list of Sprite)."""

	def __init__(self, path, name=None):
		self.path = Path(path)
		self.name = name or self.path.name
		with open(self.path / "manifest.json", "r", encoding="utf-8") as f:
			self.manifest = json.load(f)
		self.sprites = []
		for i, e in enumerate(self.manifest["sprites"]):
			frames = e.get("frames") or []
			img = e.get("image") or {}
			spath = self.path / safe_rel(img["file"]) if img.get("file") and frames else None
			self.sprites.append(Sprite([Rect(*fr) for fr in frames], None, e.get("color_key_u32", 0), spath, e.get("index", i)))

	def __len__(self):
		return len(self.sprites)


def open_folder(path, name=None):
	"""Open the sprite folder at path (it must hold manifest.json)."""
	return SpriteFolder(path, name)


def read_index(tree=STAGED_TREE):
	"""The tree's index.json 'assets' mapping, {} when the tree has none."""
	p = Path(tree) / "index.json"
	if not p.is_file():
		return {}
	with open(p, "r", encoding="utf-8") as f:
		return json.load(f).get("assets", {})


def asset_names(tree=STAGED_TREE):
	"""Every asset name in the tree's index, sorted."""
	return sorted(read_index(tree))


def open_asset(name, tree=STAGED_TREE):
	"""Open an asset by its index name ('npcs/ogre'); falls back to <tree>/<name> when the index lacks it.
	Raises FileNotFoundError when neither names a folder with a manifest."""
	tree = Path(tree)
	entry = read_index(tree).get(safe_rel(name))
	folder = tree / safe_rel(entry["dir"] if entry else name)
	if not (folder / "manifest.json").is_file():
		raise FileNotFoundError("no sprite folder %r in %s" % (name, tree))
	return SpriteFolder(folder, name)


def has_asset(name, tree=STAGED_TREE):
	"""True when open_asset(name, tree) would succeed."""
	tree = Path(tree)
	entry = read_index(tree).get(safe_rel(name))
	return (tree / safe_rel(entry["dir"] if entry else name) / "manifest.json").is_file()


# ---------------------------------------------------------------- writing
def _image_info(data):
	"""(extension, manifest image dict) for stored image bytes, sniffed from the bytes."""
	info = {"size": len(data)}
	if data[:2] == b"BM":
		ext = "bmp"
	elif data[:8] == b"\x89PNG\r\n\x1a\n":
		ext = "png"
	elif data[:2] == b"\xff\xd8":
		ext = "jpg"
	else:
		raise ValueError("image bytes are neither BMP, PNG nor JPEG")
	info["format"] = ext
	with Image.open(io.BytesIO(data)) as im:
		info["width"], info["height"] = im.size
	return ext, info


def write_folder(out_dir, sprites):
	"""Write sprites (a list of Sprite) as a sprite folder at out_dir, in the pak_extract manifest format.
	A sprite without rectangles is written as an empty entry (no image). The folder is created; an
	existing manifest.json or image file of the same name is replaced. Nothing is ever deleted: any other
	NNN.<ext> image or .bin file in the folder that the new manifest does not name is only reported. Every
	image is read before the first write, so the sprites may come from the very folder being written.
	Returns (manifest dict, sorted names of the stale files left in the folder)."""
	out = Path(out_dir)
	blobs = [(sp.image_data if sp.rectangles else None) for sp in sprites]  # read everything up front
	out.mkdir(parents=True, exist_ok=True)
	entries = []
	named = set()
	for i, sp in enumerate(sprites):
		if not sp.rectangles:
			entries.append({"index": i, "rect_count": 0, "empty": True})
			continue
		ext, info = _image_info(blobs[i])
		info["file"] = "%03d.%s" % (i, ext)
		named.add(info["file"])
		(out / info["file"]).write_bytes(blobs[i])
		k = sp.color_key
		entries.append({
			"index": i, "rect_count": len(sp.rectangles), "frames": [r.as_list() for r in sp.rectangles],
			"color_key_u32": k, "color_key_hex": "%08x" % k,
			"color_key_rgb": [(k >> 8) & 255, (k >> 16) & 255, (k >> 24) & 255],
			"image": info,
		})
	manifest = {"tool": TOOL_ID, "format_version": FORMAT_VERSION, "sprite_count": len(entries),
	            "frame_fields": FRAME_FIELDS, "sprites": entries}
	with open(out / "manifest.json", "w", encoding="utf-8", newline="\n") as f:
		json.dump(manifest, f, indent=1)
		f.write("\n")
	stale = [e for e in sorted(os.listdir(out)) if _SIDE_FILE.fullmatch(e) and e not in named and (out / e).is_file()]
	return manifest, stale


def update_index(tree, name, sprite_count):
	"""Add or refresh the asset 'name' (the folder's path relative to tree, '/'-separated) in the tree's index.json."""
	tree = Path(tree)
	p = tree / "index.json"
	data = {"assets": {}}
	if p.is_file():
		with open(p, "r", encoding="utf-8") as f:
			data = json.load(f)
	data.setdefault("assets", {})[name] = {"dir": name, "manifest": name + "/manifest.json", "sprite_count": sprite_count}
	data["assets"] = dict(sorted(data["assets"].items()))
	with open(p, "w", encoding="utf-8", newline="\n") as f:
		json.dump(data, f, indent=1)
		f.write("\n")
