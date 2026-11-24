#!/usr/bin/env python
"""Build the minimap sprite folder from pre-rendered map images.

Sprite 0: atlas image (512px overview, 512x512 frame grid)
Sprites 1+: 2k map images (one per map index), single frame covering the full image

Output is a sprite folder (manifest.json + one image per sprite, the pak_extract manifest format)
written by tools/sprite_folder/sprite_folder.py; add it to a sprite tree's index.json with
sprite_folder.update_index when it lives under staged/client/sprites.
"""

import sys
import math
from io import BytesIO
from pathlib import Path
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "sprite_folder"))
import sprite_folder
from sprite_folder import Rect, Sprite

CHUNK_SIZE = 256  # frame grid cell size for 2k sprites


def build_sprite(png_path, chunk_size=None):
	"""Build one Sprite from a PNG file.

	If chunk_size is None, creates a single rect covering the full image.
	If chunk_size is set, creates a grid of chunk_size x chunk_size rects.
	"""
	with Image.open(png_path) as img:
		img_w, img_h = img.size

	rects = []
	if chunk_size is None:
		rects.append(Rect(0, 0, img_w, img_h))
	else:
		# Grid of chunk_size x chunk_size frames, left-to-right, top-to-bottom
		cols = math.ceil(img_w / chunk_size)
		rows = math.ceil(img_h / chunk_size)
		for row in range(rows):
			for col in range(cols):
				rx = col * chunk_size
				ry = row * chunk_size
				rects.append(Rect(rx, ry, min(chunk_size, img_w - rx), min(chunk_size, img_h - ry)))

	with open(png_path, 'rb') as f:
		image_data = f.read()
	return Sprite(rects, image_data)


def main():
	if len(sys.argv) < 3:
		print("Usage: build_minimap_sprites.py <atlas_png> <map_dir_2k> [output_folder]")
		print("  atlas_png:     path to 512px atlas image (sprite 0)")
		print("  map_dir_2k:    directory of 2k map images named 0.png..78.png (sprites 1+)")
		print("  output_folder: sprite folder to write (default: <map_dir_2k>/minimap)")
		sys.exit(1)

	atlas_path = Path(sys.argv[1])
	map_dir = Path(sys.argv[2])
	output = Path(sys.argv[3]) if len(sys.argv) > 3 else map_dir / "minimap"

	if not atlas_path.exists():
		print(f"Atlas not found: {atlas_path}")
		sys.exit(1)
	if not map_dir.is_dir():
		print(f"Map directory not found: {map_dir}")
		sys.exit(1)

	# Find all numbered map images
	map_images = {}
	for p in map_dir.glob("*.png"):
		if p.stem.isdigit():
			map_images[int(p.stem)] = p

	if not map_images:
		print(f"No numbered PNG files found in {map_dir}")
		sys.exit(1)

	max_index = max(map_images.keys())
	total_sprites = 1 + max_index + 1  # sprite 0 = atlas, sprites 1..max_index+1

	print(f"Atlas: {atlas_path}")
	print(f"Maps: {len(map_images)} images (indices 0-{max_index})")
	print(f"Total sprites: {total_sprites}")
	print(f"Chunk size: {CHUNK_SIZE}x{CHUNK_SIZE}")
	print()

	sprites = []

	# Sprite 0: atlas (512x512 frame grid)
	print(f"  Sprite 0: atlas ({atlas_path.name})")
	sp = build_sprite(atlas_path, chunk_size=512)
	sprites.append(sp)
	print(f"    {len(sp.rectangles)} frames, {len(sp.image_data)} bytes")

	# Sprites 1+: map images (single frame covering full image)
	for idx in range(max_index + 1):
		sprite_idx = idx + 1
		if idx in map_images:
			print(f"  Sprite {sprite_idx}: map {idx} ({map_images[idx].name})", end="")
			sp = build_sprite(map_images[idx], chunk_size=None)
			sprites.append(sp)
			print(f" -> {len(sp.rectangles)} frame, {len(sp.image_data)} bytes")
		else:
			# Missing map index - placeholder with 1 dummy rect and a 1x1 transparent PNG
			print(f"  Sprite {sprite_idx}: map {idx} (MISSING - empty placeholder)")
			buf = BytesIO()
			Image.new("RGBA", (1, 1), (0, 0, 0, 0)).save(buf, "PNG")
			sprites.append(Sprite([Rect(0, 0, 1, 1)], buf.getvalue()))

	_manifest, stale = sprite_folder.write_folder(output, sprites)
	if stale:
		print(f"stale, not removed: {', '.join(stale)} (in {output})")
	size = sum(len(s.image_data) for s in sprites)
	print(f"\nSaved: {output} ({size} image bytes, {size / 1024 / 1024:.1f} MB)")
	print(f"  {len(sprites)} sprites total")


if __name__ == "__main__":
	main()
