#!/usr/bin/env python
"""Combine all minimap images from a directory into a single square atlas."""

import sys
import math
from pathlib import Path
from PIL import Image

def main():
    if len(sys.argv) < 2:
        print("Usage: combine_atlas.py <input_dir> [output_file]")
        sys.exit(1)

    input_dir = Path(sys.argv[1])
    output_file = sys.argv[2] if len(sys.argv) > 2 else str(input_dir / "atlas.png")

    images = sorted([p for p in input_dir.glob("*.png") if p.stem != "atlas"],
                    key=lambda p: (int(p.stem) if p.stem.isdigit() else float('inf'), p.stem))
    if not images:
        print(f"No PNG files found in {input_dir}")
        sys.exit(1)

    # All images are 512x512 max, but some may be smaller (non-square maps)
    # Use 512 as the cell size
    cell_size = 512
    count = len(images)
    grid = math.ceil(math.sqrt(count))  # NxN grid

    atlas_size = grid * cell_size
    print(f"{count} images, {grid}x{grid} grid, atlas: {atlas_size}x{atlas_size}")

    atlas = Image.new("RGBA", (atlas_size, atlas_size), (0, 0, 0, 255))

    for i, img_path in enumerate(images):
        col = i % grid
        row = i // grid
        with Image.open(img_path) as img:
            # Center within cell if smaller than 512x512
            x = col * cell_size + (cell_size - img.width) // 2
            y = row * cell_size + (cell_size - img.height) // 2
            atlas.paste(img, (x, y))
            print(f"  [{row},{col}] {img_path.stem} ({img.width}x{img.height})")

    atlas.save(output_file, "PNG")
    print(f"Saved: {output_file} ({atlas_size}x{atlas_size})")

if __name__ == "__main__":
    main()
