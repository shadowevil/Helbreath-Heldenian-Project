"""
Normalize brightness of grayscale hair sprites.

For each PNG in the input folder:
- Finds the brightest opaque pixel value
- Scales all opaque pixels so the brightest becomes 255
- Preserves the gradient (dark pixels stay proportionally dark)
- Transparent pixels are left untouched

Usage: python normalize_brightness.py <folder_path>
"""

import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("Pillow is required: pip install Pillow")
    sys.exit(1)


def normalize_image(path: Path) -> bool:
    img = Image.open(path).convert("RGBA")
    pixels = img.load()
    w, h = img.size

    # Find the max brightness across all opaque pixels
    max_val = 0
    for y in range(h):
        for x in range(w):
            r, g, b, a = pixels[x, y]
            if a == 255:
                max_val = max(max_val, r, g, b)

    if max_val == 0 or max_val == 255:
        print(f"  Skipped: {path.name} (max={max_val})")
        return False

    print(f"  {path.name}: max brightness {max_val} -> 255 (scale {255/max_val:.2f}x)")

    # Scale all opaque pixels
    scale = 255.0 / max_val
    for y in range(h):
        for x in range(w):
            r, g, b, a = pixels[x, y]
            if a == 255:
                nr = min(255, int(r * scale + 0.5))
                ng = min(255, int(g * scale + 0.5))
                nb = min(255, int(b * scale + 0.5))
                pixels[x, y] = (nr, ng, nb, 255)

    img.save(path)
    return True


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
    normalized = 0
    for p in pngs:
        if normalize_image(p):
            normalized += 1

    print(f"\nDone. {normalized}/{len(pngs)} files normalized.")


if __name__ == "__main__":
    main()
