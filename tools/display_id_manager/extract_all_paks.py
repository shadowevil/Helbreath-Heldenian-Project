#!/usr/bin/env python3
"""
Extract all Helbreath PAK files into categorized folders.
Walks the sprites directory, finds all .pak files, and extracts
each into a mirrored folder structure.

Usage:
    python extract_all_paks.py [--sprites-dir PATH] [--output-dir PATH]
"""

import argparse
import sys
from pathlib import Path
from paklib import PAKFile

def extract_all(sprites_dir: Path, output_dir: Path):
    """Extract all PAK files from sprites directory tree."""
    pak_files = sorted(sprites_dir.rglob("*.pak"))
    
    if not pak_files:
        print(f"No .pak files found in {sprites_dir}")
        sys.exit(1)
    
    print(f"Found {len(pak_files)} PAK files\n")
    
    total_sprites = 0
    errors = []
    
    for pak_path in pak_files:
        # Mirror the folder structure: sprites/items/sword.pak -> output/items/sword/
        relative = pak_path.relative_to(sprites_dir)
        category = relative.parent  # e.g. "items", "npcs/bosses"
        pak_name = pak_path.stem     # e.g. "sword"
        
        out_dir = output_dir / category / pak_name
        
        try:
            pak = PAKFile.read(pak_path)
            sprite_count = len(pak.sprites)
            
            if sprite_count == 0:
                print(f"  SKIP  {relative} (empty)")
                continue
            
            pak.extract_all(out_dir, prefix=pak_name)
            total_sprites += sprite_count
            print(f"  OK    {relative} -> {sprite_count} sprites")
            
        except Exception as e:
            errors.append((pak_path, str(e)))
            print(f"  FAIL  {relative} -> {e}")
    
    print(f"\n{'='*60}")
    print(f"Total PAK files: {len(pak_files)}")
    print(f"Total sprites extracted: {total_sprites}")
    print(f"Output directory: {output_dir}")
    
    if errors:
        print(f"\nErrors ({len(errors)}):")
        for path, err in errors:
            print(f"  {path}: {err}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Extract all Helbreath PAK files")
    parser.add_argument(
        "--sprites-dir",
        type=Path,
        default=Path(r"C:\Users\ShadowEvil\source\Repos3\Helbreath-3.82\Binaries\Game\sprites"),
        help="Path to sprites directory containing PAK files"
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path(r"C:\AI\helbreath-lora\dataset\raw"),
        help="Output directory for extracted sprites"
    )
    args = parser.parse_args()
    
    if not args.sprites_dir.exists():
        print(f"Sprites directory not found: {args.sprites_dir}")
        sys.exit(1)
    
    extract_all(args.sprites_dir, args.output_dir)