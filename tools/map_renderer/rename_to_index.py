#!/usr/bin/env python
"""Rename minimap images from map names to their server map_list index.

The server loads maps from the mapinfo.db 'maps' table ordered alphabetically
by map_name. The index in m_map_list[] is simply the alphabetical position.

This script reads the database to build the name->index mapping, then renames
all matching PNG files in the target directory from {name}.png to {index}.png.
"""

import sys
import sqlite3
import shutil
from contextlib import closing
from pathlib import Path

# Repo root is two levels up (tools/map_renderer/); file name is case-sensitive on Linux
DEFAULT_DB = Path(__file__).resolve().parents[2] / "Binaries" / "Server" / "MapInfo.db"

def get_map_indices(db_path):
    """Read map names from database, return {lowercase_name: index} dict."""
    with closing(sqlite3.connect(db_path)) as conn:
        rows = conn.execute("SELECT map_name FROM maps ORDER BY map_name;").fetchall()
    return {row[0].lower(): i for i, row in enumerate(rows)}

def main():
    if len(sys.argv) < 2:
        print("Usage: rename_to_index.py <directory> [--db path/to/mapinfo.db] [--dry-run]")
        sys.exit(1)

    input_dir = Path(sys.argv[1])
    dry_run = "--dry-run" in sys.argv

    # Optional --db flag
    db_path = DEFAULT_DB
    if "--db" in sys.argv:
        db_idx = sys.argv.index("--db")
        if db_idx + 1 < len(sys.argv):
            db_path = sys.argv[db_idx + 1]

    if not input_dir.is_dir():
        print(f"Not a directory: {input_dir}")
        sys.exit(1)

    map_indices = get_map_indices(db_path)
    print(f"Loaded {len(map_indices)} map names from {db_path}")

    images = sorted(input_dir.glob("*.png"))
    if not images:
        print(f"No PNG files found in {input_dir}")
        sys.exit(1)

    renamed = 0
    skipped = 0
    for img_path in images:
        stem = img_path.stem.lower()
        if stem in map_indices:
            idx = map_indices[stem]
            new_name = f"{idx}.png"
            new_path = img_path.parent / new_name
            if new_path.exists() and new_path != img_path:
                print(f"  {img_path.name} -> {new_name} (target exists, skipped)")
                skipped += 1
                continue
            if dry_run:
                print(f"  [DRY RUN] {img_path.name} -> {new_name}")
            else:
                shutil.move(str(img_path), str(new_path))
                print(f"  {img_path.name} -> {new_name}")
            renamed += 1
        else:
            print(f"  {img_path.name} (not in database, skipped)")
            skipped += 1

    print(f"\n{renamed} renamed, {skipped} skipped" + (" [DRY RUN]" if dry_run else ""))

if __name__ == "__main__":
    main()
