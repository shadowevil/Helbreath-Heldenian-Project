"""Import Helbreath-SDL's minimap pyramid levels 01-03 as JPG q92 into staged/ (HH-0045, d-032).

Reads (read-only)  Helbreath-SDL/Helbreath/working/minimaps/<map>/01.png .. 03.png
Writes             staged/client/sprites/minimaps/<map>/01.jpg .. 03.jpg

Level 00 (2096 px, SDL's big-map level) is never read. Only maps whose .amd exists in
staged/shared/mapdata/ are imported. --dry-run lists the plan and writes nothing.
"""
import argparse
import sys
from pathlib import Path

from PIL import Image

REPO = Path(__file__).resolve().parents[3]
SOURCE = Path(r"C:\Users\ShadowEvil\source\Helbreath-SDL\Helbreath\working\minimaps")
DEST = REPO / "staged" / "client" / "sprites" / "minimaps"
MAPDATA = REPO / "staged" / "shared" / "mapdata"
LEVELS = ("01", "02", "03")
QUALITY = 92


def main() -> int:
	ap = argparse.ArgumentParser()
	ap.add_argument("--dry-run", action="store_true")
	args = ap.parse_args()

	maps = sorted(p.name for p in SOURCE.iterdir() if p.is_dir())
	planned, skipped = [], []
	for name in maps:
		if not (MAPDATA / f"{name}.amd").is_file():
			skipped.append(name)
			continue
		for level in LEVELS:
			src = SOURCE / name / f"{level}.png"
			if not src.is_file():
				print(f"FAIL missing {src}")
				return 1
			planned.append((src, DEST / name.lower() / f"{level}.jpg"))

	print(f"maps: {len(maps)}, imported: {len(maps) - len(skipped)}, skipped (no .amd): {skipped}")
	if args.dry_run:
		for src, dst in planned[:6]:
			print(f"plan {src} -> {dst}")
		print(f"plan total: {len(planned)} files (dry run, nothing written)")
		return 0

	total = 0
	for src, dst in planned:
		dst.parent.mkdir(parents=True, exist_ok=True)
		with Image.open(src) as img:
			img.convert("RGB").save(dst, "JPEG", quality=QUALITY, subsampling=0, optimize=True)
		total += dst.stat().st_size
	print(f"wrote {len(planned)} files, {total / 1e6:.1f} MB")
	return 0


if __name__ == "__main__":
	sys.exit(main())
