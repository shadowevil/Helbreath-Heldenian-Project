#!/usr/bin/env python3
"""
Helbreath Map Renderer — Generates high-resolution map images from .amd files.

Reads .amd map data and renders tiles + objects using PAK sprite sheets.
Each tile is 32x32 pixels. Objects are overlaid on top of tiles.

Usage:
    python map_renderer.py <map_file.amd>                 # Render one map
    python map_renderer.py --all                           # Render all maps
    python map_renderer.py <map_file.amd> --scale 0.5     # Half resolution
    python map_renderer.py <map_file.amd> --tiles-only     # Skip objects
    python map_renderer.py --all --scale 0.25              # Quarter-res all maps
"""

import struct
import sys
import argparse
from pathlib import Path
from PIL import Image

# paklib is a sibling file
sys.path.insert(0, str(Path(__file__).parent))
from paklib import PAKFile, SpriteRectangle

TILE_SIZE = 32

# ---------------------------------------------------------------------------
# Sprite ID → PAK file mapping (mirrors Screen_Loading.cpp)
# ---------------------------------------------------------------------------
# Format: (pak_relative_path, start_id, count)
# "tiles/maptiles1" → sprite IDs 0..31 loaded into m_tile_spr[]

TILE_PAK_MAP = [
    # Stage 4: Tiles1
    ("tiles/maptiles1",       0,   32),
    ("tiles/sinside1",        70,  27),
    ("objects/trees1",        100, 46),
    ("objects/treeshadows",   150, 46),
    ("objects/objects1",      200, 10),
    ("objects/objects2",      211,  5),
    ("objects/objects3",      216,  4),
    ("objects/objects4",      220,  2),
    # Stage 8: Tiles2
    ("tiles/tile223-225",     223,  3),
    ("tiles/tile226-229",     226,  4),
    ("objects/objects5",      230,  9),
    ("objects/objects6",      238,  4),
    ("objects/objects7",      242,  7),
    ("tiles/maptiles2",       300, 15),
    ("tiles/maptiles4",       320, 10),
    ("tiles/maptiles5",       330, 19),
    ("tiles/maptiles6",       349,  4),
    ("tiles/maptiles353-361", 353,  9),
    ("tiles/tile363-366",     363,  4),
    ("tiles/tile367-367",     367,  1),
    ("tiles/tile370-381",     370, 12),
    ("tiles/tile382-387",     382,  6),
    ("tiles/tile388-402",     388, 15),
    # Stage 12: Tiles3
    ("tiles/tile403-405",     403,  3),
    ("tiles/tile406-421",     406, 16),
    ("tiles/tile422-429",     422,  8),
    ("tiles/tile430-443",     430, 14),
    ("tiles/tile444-444",     444,  1),
    ("tiles/tile445-461",     445, 17),
    ("tiles/tile462-473",     462, 12),
    ("tiles/tile474-478",     474,  5),
    ("tiles/tile479-488",     479, 10),
    ("tiles/tile489-522",     489, 34),
    ("tiles/tile523-530",     523,  8),
    ("tiles/tile531-540",     531, 10),
    ("tiles/tile541-545",     541,  5),
]

# structures1 has special entries: sprite 1 → ID 51, sprite 5 → ID 55
STRUCTURES1_MAP = {51: 1, 55: 5}


class SpriteCache:
    """Loads and caches PAK sprite sheets + their frame rectangles."""

    def __init__(self, sprites_dir: Path):
        self.sprites_dir = sprites_dir
        # sprite_id → (PIL.Image sheet, [SpriteRectangle])
        self._cache: dict[int, tuple[Image.Image, list[SpriteRectangle]]] = {}
        self._pak_cache: dict[str, PAKFile] = {}
        self._missing_paks: set[str] = set()
        self._load_all()

    def _load_pak(self, rel_path: str) -> PAKFile | None:
        if rel_path in self._pak_cache:
            return self._pak_cache[rel_path]
        if rel_path in self._missing_paks:
            return None

        pak_path = self.sprites_dir / (rel_path + ".pak")
        if not pak_path.exists():
            print(f"  Warning: PAK not found: {pak_path}")
            self._missing_paks.add(rel_path)
            return None

        pak = PAKFile.read(pak_path)
        self._pak_cache[rel_path] = pak
        return pak

    def _load_all(self):
        """Pre-load all tile/object PAK files into the cache."""
        print("Loading sprite PAKs...")

        for rel_path, start_id, count in TILE_PAK_MAP:
            pak = self._load_pak(rel_path)
            if pak is None:
                continue
            for i in range(min(count, len(pak.sprites))):
                sprite = pak.sprites[i]
                try:
                    img = sprite.get_image().convert("RGBA")
                    self._cache[start_id + i] = (img, sprite.rectangles)
                except Exception as e:
                    print(f"  Warning: Failed to load sprite {start_id + i} from {rel_path}: {e}")

        # structures1 special entries
        pak = self._load_pak("objects/structures1")
        if pak:
            for sprite_id, pak_index in STRUCTURES1_MAP.items():
                if pak_index < len(pak.sprites):
                    sprite = pak.sprites[pak_index]
                    try:
                        img = sprite.get_image().convert("RGBA")
                        self._cache[sprite_id] = (img, sprite.rectangles)
                    except Exception:
                        pass

        print(f"  Loaded {len(self._cache)} sprite sheets")

    def get_frame(self, sprite_id: int, frame: int) -> Image.Image | None:
        """Extract a single frame from a sprite sheet using its rectangle."""
        entry = self._cache.get(sprite_id)
        if entry is None:
            return None

        img, rects = entry
        if frame < 0 or frame >= len(rects):
            # If frame 0 requested but no rects, return whole image
            if frame == 0 and len(rects) == 0:
                return img
            return None

        r = rects[frame]
        # Rectangle defines a crop region within the sprite sheet
        # x, y is top-left; width, height is size
        left = r.x
        top = r.y
        right = r.x + r.width
        bottom = r.y + r.height

        # Clamp to image bounds
        left = max(0, left)
        top = max(0, top)
        right = min(img.width, right)
        bottom = min(img.height, bottom)

        if right <= left or bottom <= top:
            return None

        return img.crop((left, top, right, bottom))

    def get_frame_with_pivot(self, sprite_id: int, frame: int) -> tuple[Image.Image, int, int] | None:
        """
        Get frame image plus its pivot offset.

        The engine draws at: drawX = x + pivotX, drawY = y + pivotY
        So pivotX/pivotY are offsets applied to the draw position to get
        the actual top-left of the rendered frame on screen.
        """
        entry = self._cache.get(sprite_id)
        if entry is None:
            return None

        img, rects = entry
        if frame < 0 or frame >= len(rects):
            if frame == 0 and len(rects) == 0:
                return (img, 0, 0)
            return None

        r = rects[frame]
        left = max(0, r.x)
        top = max(0, r.y)
        right = min(img.width, r.x + r.width)
        bottom = min(img.height, r.y + r.height)

        if right <= left or bottom <= top:
            return None

        cropped = img.crop((left, top, right, bottom))
        # Pivot is the offset from the draw call position to actual top-left
        # Engine: drawX = x + pivotX, drawY = y + pivotY
        return (cropped, r.pivot_x, r.pivot_y)


def read_amd(path: Path) -> tuple[int, int, list]:
    """
    Read an .amd map file.
    Returns (width, height, tiles) where tiles is a flat list of
    (tile_sprite, tile_frame, obj_sprite, obj_frame, flags) tuples,
    stored row-major (y outer, x inner).
    """
    with open(path, "rb") as f:
        header_raw = f.read(256)

        # Parse header — replace nulls with spaces, then extract key=value
        header = bytearray(header_raw)
        for i in range(256):
            if header[i] == 0:
                header[i] = ord(' ')
        header_str = header.decode('ascii', errors='replace')

        map_w = 0
        map_h = 0
        tokens = header_str.replace('=', ' ').replace(',', ' ').split()
        for i, tok in enumerate(tokens):
            if tok == "MAPSIZEX" and i + 1 < len(tokens):
                try:
                    map_w = int(tokens[i + 1])
                except ValueError:
                    pass
            elif tok == "MAPSIZEY" and i + 1 < len(tokens):
                try:
                    map_h = int(tokens[i + 1])
                except ValueError:
                    pass

        if map_w <= 0 or map_h <= 0:
            raise ValueError(f"Invalid map dimensions: {map_w}x{map_h}")

        # Read tile data: 10 bytes per tile, row-major (y outer, x inner)
        tile_data = f.read(map_w * map_h * 10)
        if len(tile_data) < map_w * map_h * 10:
            raise ValueError(f"Truncated map data: expected {map_w * map_h * 10} bytes, got {len(tile_data)}")

    tiles = []
    offset = 0
    for y in range(map_h):
        for x in range(map_w):
            tile_spr, tile_frame, obj_spr, obj_frame, flags = struct.unpack_from('<5h', tile_data, offset)
            offset += 10
            tiles.append((tile_spr, tile_frame, obj_spr, obj_frame, flags))

    return map_w, map_h, tiles


def _safe_paste(canvas: Image.Image, src: Image.Image, x: int, y: int):
    """Alpha-composite src onto canvas at (x, y), handling negative coords and overflow."""
    cw, ch = canvas.size
    sw, sh = src.size

    # Source crop region
    sx1 = max(0, -x)
    sy1 = max(0, -y)
    sx2 = min(sw, cw - x)
    sy2 = min(sh, ch - y)

    if sx1 >= sx2 or sy1 >= sy2:
        return  # Fully out of bounds

    dx = max(0, x)
    dy = max(0, y)

    if sx1 != 0 or sy1 != 0 or sx2 != sw or sy2 != sh:
        src = src.crop((sx1, sy1, sx2, sy2))

    # alpha_composite blends properly (unlike paste which just replaces)
    canvas.alpha_composite(src, (dx, dy))


def render_map(amd_path: Path, sprites: SpriteCache, scale: float = 1.0,
               render_objects: bool = True, render_shadows: bool = True) -> Image.Image:
    """Render a map to a PIL Image."""
    print(f"Reading map: {amd_path.name}")
    map_w, map_h, tiles = read_amd(amd_path)
    print(f"  Map size: {map_w}x{map_h} tiles ({map_w * TILE_SIZE}x{map_h * TILE_SIZE} pixels)")

    img_w = map_w * TILE_SIZE
    img_h = map_h * TILE_SIZE

    # Create output image
    canvas = Image.new("RGBA", (img_w, img_h), (0, 0, 0, 255))

    # Pass 1: Ground tiles
    # Engine: m_tile_spr[tile_spr]->draw(ix, iy, tile_frame)
    # draw() applies pivot: drawX = ix + pivotX, drawY = iy + pivotY
    print("  Rendering ground tiles...")
    rendered_tiles = 0
    missing_tiles = set()

    for y in range(map_h):
        for x in range(map_w):
            idx = y * map_w + x
            tile_spr, tile_frame, obj_spr, obj_frame, flags = tiles[idx]

            if tile_spr == 0 and tile_frame == 0:
                continue

            result = sprites.get_frame_with_pivot(tile_spr, tile_frame)
            if result is not None:
                frame_img, pv_x, pv_y = result
                px = x * TILE_SIZE + pv_x
                py = y * TILE_SIZE + pv_y
                _safe_paste(canvas, frame_img, px, py)
                rendered_tiles += 1
            else:
                missing_tiles.add(tile_spr)

    print(f"  Ground tiles rendered: {rendered_tiles}")
    if missing_tiles:
        print(f"  Missing tile sprite IDs: {sorted(missing_tiles)[:20]}{'...' if len(missing_tiles) > 20 else ''}")

    # Pass 2: Object shadows (trees: sprite 100-199 get shadow from sprite+50)
    # Engine: tree shadows drawn at draw(ix, iy, frame) → pos = ix + pivotX, iy + pivotY
    # Engine: structure shadows drawn at draw(ix - 16, iy - 16, frame) → pos = ix-16+pivotX, iy-16+pivotY
    if render_objects and render_shadows:
        print("  Rendering object shadows...")
        for y in range(map_h):
            for x in range(map_w):
                idx = y * map_w + x
                _, _, obj_spr, obj_frame, _ = tiles[idx]
                if obj_spr == 0:
                    continue

                ix = x * TILE_SIZE
                iy = y * TILE_SIZE

                if 100 <= obj_spr < 200:
                    # Tree shadow: draw(ix, iy, frame)
                    shadow_id = obj_spr + 50
                    result = sprites.get_frame_with_pivot(shadow_id, obj_frame)
                    if result:
                        shadow_img, pv_x, pv_y = result
                        shadow_img = shadow_img.copy()
                        alpha = shadow_img.split()[3]
                        alpha = alpha.point(lambda a: a // 3)
                        shadow_img.putalpha(alpha)
                        _safe_paste(canvas, shadow_img, ix + pv_x, iy + pv_y)
                elif obj_spr in (200, 223):
                    # Structure shadows: draw(ix - 16, iy - 16, frame)
                    result = sprites.get_frame_with_pivot(obj_spr, obj_frame)
                    if result:
                        shadow_img, pv_x, pv_y = result
                        shadow_img = shadow_img.copy()
                        alpha = shadow_img.split()[3]
                        alpha = alpha.point(lambda a: a // 3)
                        shadow_img.putalpha(alpha)
                        _safe_paste(canvas, shadow_img, ix - 16 + pv_x, iy - 16 + pv_y)

    # Pass 3: Objects
    # Engine: trees draw(ix - 16, iy - 16, frame) → pos = ix-16+pivotX, iy-16+pivotY
    # Engine: other objects draw(ix - 16, iy - 16, frame) → same
    if render_objects:
        print("  Rendering objects...")
        rendered_objects = 0
        missing_objects = set()

        for y in range(map_h):
            for x in range(map_w):
                idx = y * map_w + x
                _, _, obj_spr, obj_frame, _ = tiles[idx]
                if obj_spr == 0:
                    continue

                ix = x * TILE_SIZE
                iy = y * TILE_SIZE

                if 100 <= obj_spr < 200:
                    # Trees: draw(ix - 16, iy - 16, frame)
                    result = sprites.get_frame_with_pivot(obj_spr, obj_frame)
                    if result:
                        obj_img, pv_x, pv_y = result
                        _safe_paste(canvas, obj_img, ix - 16 + pv_x, iy - 16 + pv_y)
                        rendered_objects += 1
                    else:
                        missing_objects.add(obj_spr)
                else:
                    # All other objects: draw(ix - 16, iy - 16, frame)
                    result = sprites.get_frame_with_pivot(obj_spr, obj_frame)
                    if result:
                        obj_img, pv_x, pv_y = result
                        _safe_paste(canvas, obj_img, ix - 16 + pv_x, iy - 16 + pv_y)
                        rendered_objects += 1
                    else:
                        missing_objects.add(obj_spr)

        print(f"  Objects rendered: {rendered_objects}")
        if missing_objects:
            print(f"  Missing object sprite IDs: {sorted(missing_objects)[:20]}{'...' if len(missing_objects) > 20 else ''}")

    # Scale if requested
    if scale != 1.0:
        new_w = max(1, int(img_w * scale))
        new_h = max(1, int(img_h * scale))
        print(f"  Scaling to {new_w}x{new_h}...")
        canvas = canvas.resize((new_w, new_h), Image.LANCZOS)

    return canvas


def main():
    parser = argparse.ArgumentParser(description="Render Helbreath .amd maps to PNG images")
    parser.add_argument("map_file", nargs="?", help="Path to .amd map file")
    parser.add_argument("--all", action="store_true", help="Render all maps in mapdata/")
    parser.add_argument("--scale", type=float, default=1.0, help="Output scale (0.25 = quarter res)")
    parser.add_argument("--tiles-only", action="store_true", help="Skip object rendering")
    parser.add_argument("--no-shadows", action="store_true", help="Skip shadow rendering")
    parser.add_argument("--output", "-o", type=str, help="Output directory (default: Tools/MapRenderer/output)")
    parser.add_argument("--sprites-dir", type=str, help="Sprites directory (default: auto-detect)")
    parser.add_argument("--mapdata-dir", type=str, help="Map data directory (default: auto-detect)")
    args = parser.parse_args()

    if not args.map_file and not args.all:
        parser.print_help()
        sys.exit(1)

    # Locate directories
    tool_dir = Path(__file__).parent
    repo_root = tool_dir.parent.parent

    sprites_dir = Path(args.sprites_dir) if args.sprites_dir else repo_root / "Binaries" / "Game" / "sprites"
    mapdata_dir = Path(args.mapdata_dir) if args.mapdata_dir else repo_root / "Binaries" / "Game" / "mapdata"
    output_dir = Path(args.output) if args.output else tool_dir / "output"

    if not sprites_dir.exists():
        print(f"Error: Sprites directory not found: {sprites_dir}")
        sys.exit(1)
    if not mapdata_dir.exists():
        print(f"Error: Map data directory not found: {mapdata_dir}")
        sys.exit(1)

    output_dir.mkdir(parents=True, exist_ok=True)

    # Load all sprite PAKs once
    sprite_cache = SpriteCache(sprites_dir)

    # Determine which maps to render
    if args.all:
        amd_files = sorted(mapdata_dir.glob("*.amd"))
        print(f"\nFound {len(amd_files)} maps to render\n")
    else:
        map_path = Path(args.map_file)
        if not map_path.exists():
            # Try relative to mapdata dir
            map_path = mapdata_dir / args.map_file
        if not map_path.exists():
            print(f"Error: Map file not found: {args.map_file}")
            sys.exit(1)
        amd_files = [map_path]

    # Render each map
    for amd_path in amd_files:
        try:
            img = render_map(
                amd_path,
                sprite_cache,
                scale=args.scale,
                render_objects=not args.tiles_only,
                render_shadows=not args.no_shadows,
            )

            out_path = output_dir / f"{amd_path.stem}.png"
            img.save(out_path, "PNG")
            print(f"  Saved: {out_path}\n")

        except Exception as e:
            print(f"  Error rendering {amd_path.name}: {e}\n")

    print("Done!")


if __name__ == "__main__":
    main()
