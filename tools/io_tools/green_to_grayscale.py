"""
Convert green-channel hair sprites to grayscale.

For each PNG in the input folder:
- Fully opaque pixels: use the green channel value as brightness for all RGB channels
  (0,255,0) -> (255,255,255), (0,128,0) -> (128,128,128)
- Transparent/semi-transparent pixels: left untouched

Usage: python green_to_grayscale.py <folder_path>
"""

import sys
import os
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("Pillow is required: pip install Pillow")
    sys.exit(1)


def convert_image(path: Path) -> bool:
    img = Image.open(path).convert("RGBA")
    pixels = img.load()
    w, h = img.size
    changed = False

    for y in range(h):
        for x in range(w):
            r, g, b, a = pixels[x, y]
            if a == 255:
                # Use green channel as brightness for all channels
                pixels[x, y] = (g, g, g, 255)
                if r != g or b != g:
                    changed = True

    if changed:
        img.save(path)
    return changed


def main():
    if len(sys.argv) < 2:
        print(f"Usage: python {sys.argv[0]} <folder_path>")
        sys.exit(1)

    folder = Path(sys.argv[1])
    if not folder.is_dir():
        print(f"Error: '{folder}' is not a directory")
        sys.exit(1)

    pngs = sorted(folder.glob("*.png"))
    if not pngs:
        print(f"No PNG files found in '{folder}'")
        sys.exit(1)

    print(f"Processing {len(pngs)} PNG files in '{folder}'...")
    converted = 0
    for p in pngs:
        if convert_image(p):
            converted += 1
            print(f"  Converted: {p.name}")
        else:
            print(f"  Skipped (no change): {p.name}")

    print(f"\nDone. {converted}/{len(pngs)} files modified.")


if __name__ == "__main__":
    main()
