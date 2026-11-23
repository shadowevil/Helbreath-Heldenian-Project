#!/usr/bin/env python
"""Build minimap.pak from pre-rendered map images.

Sprite 0: atlas image (512px overview, single frame covering full image)
Sprites 1+: 2k map images (one per map index), with 256x256 frame grid

PAK format matches PAKLib (PAK.h):
  file_header (20 bytes) -> sprite_count (u32) -> sprite_entries[] (offset+size, 8 bytes each)
  Per sprite: sprite_header (100 bytes) -> rect_count (u32) -> rects[] (12 bytes each)
              -> padding (u32) -> image_data (raw PNG)
"""

import sys
import struct
import math
from io import BytesIO
from pathlib import Path
from PIL import Image

FILE_HEADER_SIG = b"<Pak file header>"      # 17 bytes
FILE_HEADER_PAD = 3                          # 3 bytes padding = 20 total
SPRITE_HEADER_SIG = b"<Sprite File Header>"  # 20 bytes
SPRITE_HEADER_PAD = 80                       # 80 bytes padding = 100 total

CHUNK_SIZE = 256  # frame grid cell size for 2k sprites


def make_file_header():
    return FILE_HEADER_SIG + b'\x00' * FILE_HEADER_PAD


def make_sprite_header():
    return SPRITE_HEADER_SIG + b'\x00' * SPRITE_HEADER_PAD


def make_rect(x, y, w, h, pivot_x=0, pivot_y=0):
    """Pack a sprite_rect: x, y, width, height (u16), pivotX, pivotY (i16)."""
    return struct.pack('<HHHHhh', x, y, w, h, pivot_x, pivot_y)


def build_sprite_data(png_path, chunk_size=None):
    """Build sprite binary data (header + rects + padding + image).

    If chunk_size is None, creates a single rect covering the full image.
    If chunk_size is set, creates a grid of chunk_size x chunk_size rects.

    Returns (sprite_bytes, rect_count).
    """
    with Image.open(png_path) as img:
        img_w, img_h = img.size

    # Build rectangles
    rects = []
    if chunk_size is None:
        # Single frame covering full image
        rects.append(make_rect(0, 0, img_w, img_h))
    else:
        # Grid of chunk_size x chunk_size frames, left-to-right, top-to-bottom
        cols = math.ceil(img_w / chunk_size)
        rows = math.ceil(img_h / chunk_size)
        for row in range(rows):
            for col in range(cols):
                rx = col * chunk_size
                ry = row * chunk_size
                rw = min(chunk_size, img_w - rx)
                rh = min(chunk_size, img_h - ry)
                rects.append(make_rect(rx, ry, rw, rh))

    # Read raw PNG bytes
    with open(png_path, 'rb') as f:
        image_data = f.read()

    # Assemble: sprite_header + rect_count + rects + padding(4) + image_data
    parts = []
    parts.append(make_sprite_header())
    parts.append(struct.pack('<I', len(rects)))
    for r in rects:
        parts.append(r)
    parts.append(struct.pack('<I', 0))  # padding
    parts.append(image_data)

    sprite_bytes = b''.join(parts)
    return sprite_bytes, len(rects)


def main():
    if len(sys.argv) < 3:
        print("Usage: build_minimap_pak.py <atlas_png> <map_dir_2k> [output_pak]")
        print("  atlas_png:  path to 512px atlas image (sprite 0)")
        print("  map_dir_2k: directory of 2k map images named 0.png..78.png (sprites 1+)")
        print("  output_pak: output path (default: <map_dir_2k>/minimap.pak)")
        sys.exit(1)

    atlas_path = Path(sys.argv[1])
    map_dir = Path(sys.argv[2])
    output_pak = sys.argv[3] if len(sys.argv) > 3 else str(map_dir / "minimap.pak")

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
    print(f"Total sprites in PAK: {total_sprites}")
    print(f"Chunk size: {CHUNK_SIZE}x{CHUNK_SIZE}")
    print()

    # Build all sprite data
    sprite_data_list = []

    # Sprite 0: atlas (512x512 frame grid)
    print(f"  Sprite 0: atlas ({atlas_path.name})")
    data, nrects = build_sprite_data(atlas_path, chunk_size=512)
    sprite_data_list.append(data)
    print(f"    {nrects} frames, {len(data)} bytes")

    # Sprites 1+: map images (single frame covering full image)
    for idx in range(max_index + 1):
        sprite_idx = idx + 1
        if idx in map_images:
            print(f"  Sprite {sprite_idx}: map {idx} ({map_images[idx].name})", end="")
            data, nrects = build_sprite_data(map_images[idx], chunk_size=None)
            sprite_data_list.append(data)
            print(f" -> {nrects} frame, {len(data)} bytes")
        else:
            # Missing map index - create empty sprite with 1 dummy rect
            print(f"  Sprite {sprite_idx}: map {idx} (MISSING - empty placeholder)")
            parts = []
            parts.append(make_sprite_header())
            parts.append(struct.pack('<I', 1))  # 1 rect
            parts.append(make_rect(0, 0, 1, 1))
            parts.append(struct.pack('<I', 0))  # padding
            # 1x1 transparent PNG
            buf = BytesIO()
            Image.new("RGBA", (1, 1), (0, 0, 0, 0)).save(buf, "PNG")
            parts.append(buf.getvalue())
            sprite_data_list.append(b''.join(parts))

    # Calculate offsets
    file_header_bytes = make_file_header()
    sprite_count = len(sprite_data_list)
    header_size = len(file_header_bytes) + 4 + (sprite_count * 8)  # file_header + count + entries

    offsets = []
    current_offset = header_size
    for sd in sprite_data_list:
        offsets.append(current_offset)
        current_offset += len(sd)

    # Write PAK
    with open(output_pak, 'wb') as f:
        f.write(file_header_bytes)
        f.write(struct.pack('<I', sprite_count))
        for i, sd in enumerate(sprite_data_list):
            f.write(struct.pack('<II', offsets[i], len(sd)))
        for sd in sprite_data_list:
            f.write(sd)

    total_size = current_offset
    print(f"\nSaved: {output_pak} ({total_size} bytes, {total_size / 1024 / 1024:.1f} MB)")
    print(f"  {sprite_count} sprites total")


if __name__ == "__main__":
    main()
