#!/usr/bin/env python3
"""
Pure Python library for reading and writing Helbreath PAK files.
Based on PAKLib format specification.
"""

import struct
from dataclasses import dataclass
from pathlib import Path
from typing import List, BinaryIO
from PIL import Image
import io


@dataclass
class SpriteRectangle:
    """Sprite rectangle metadata (12 bytes)."""
    x: int  # Int16
    y: int  # Int16
    width: int  # Int16
    height: int  # Int16
    pivot_x: int  # Int16
    pivot_y: int  # Int16
    
    @classmethod
    def from_bytes(cls, data: bytes) -> 'SpriteRectangle':
        """Parse rectangle from 12 bytes."""
        x, y, w, h, px, py = struct.unpack('<6h', data)
        return cls(x, y, w, h, px, py)
    
    def to_bytes(self) -> bytes:
        """Serialize rectangle to 12 bytes."""
        return struct.pack('<6h', self.x, self.y, self.width, self.height, self.pivot_x, self.pivot_y)


@dataclass
class Sprite:
    """A single sprite with image data and rectangles."""
    image_data: bytes  # PNG or BMP data
    rectangles: List[SpriteRectangle]
    
    def get_image(self) -> Image.Image:
        """Get PIL Image from sprite data."""
        return Image.open(io.BytesIO(self.image_data))
    
    @classmethod
    def from_image(cls, image: Image.Image, rectangles: List[SpriteRectangle]) -> 'Sprite':
        """Create sprite from PIL Image."""
        buffer = io.BytesIO()
        image.save(buffer, format='PNG')
        return cls(buffer.getvalue(), rectangles)


class PAKFile:
    """Helbreath PAK file reader/writer."""
    
    # Format constants
    FILE_HEADER_MAGIC = b'<Pak file header>\x00\x00\x00'
    SPRITE_HEADER_MAGIC = b'<Sprite File Header>'
    SPRITE_HEADER_PADDING = 80
    
    def __init__(self):
        self.sprites: List[Sprite] = []
    
    @classmethod
    def read(cls, path: Path) -> 'PAKFile':
        """Read PAK file from disk."""
        pak = cls()
        
        with open(path, 'rb') as f:
            # 1. Read file header (20 bytes)
            header = f.read(20)
            if header != cls.FILE_HEADER_MAGIC:
                raise ValueError(f"Invalid PAK file header: {header[:17]}")
            
            # 2. Read sprite count (4 bytes)
            sprite_count = struct.unpack('<I', f.read(4))[0]
            
            # 3. Read sprite table (8 bytes per sprite)
            sprite_table = []
            for _ in range(sprite_count):
                offset, length = struct.unpack('<II', f.read(8))
                sprite_table.append((offset, length))
            
            # 4. Read each sprite entry
            for offset, length in sprite_table:
                f.seek(offset)
                pak.sprites.append(pak._read_sprite_entry(f, length))
        
        return pak
    
    def _read_sprite_entry(self, f: BinaryIO, length: int) -> Sprite:
        """Read a single sprite entry."""
        start_pos = f.tell()
        
        # 4.1 Sprite header (100 bytes)
        sprite_header = f.read(20)
        if sprite_header != self.SPRITE_HEADER_MAGIC:
            raise ValueError(f"Invalid sprite header: {sprite_header}")
        
        f.read(self.SPRITE_HEADER_PADDING)  # Skip padding (80 bytes)
        
        # 4.2 Rectangle data
        rect_count = struct.unpack('<I', f.read(4))[0]
        rectangles = []
        for _ in range(rect_count):
            rect_data = f.read(12)
            rectangles.append(SpriteRectangle.from_bytes(rect_data))
        
        # 4.3 Entry padding (4 bytes)
        f.read(4)
        
        # 4.4 Image data (remaining bytes)
        bytes_read = f.tell() - start_pos
        image_size = length - bytes_read
        image_data = f.read(image_size)
        
        return Sprite(image_data, rectangles)
    
    def write(self, path: Path):
        """Write PAK file to disk."""
        with open(path, 'wb') as f:
            # 1. Write file header (20 bytes)
            f.write(self.FILE_HEADER_MAGIC)
            
            # 2. Write sprite count (4 bytes)
            f.write(struct.pack('<I', len(self.sprites)))
            
            # 3. Calculate sprite table (we'll write it after calculating offsets)
            table_start = f.tell()
            table_size = len(self.sprites) * 8
            f.write(b'\x00' * table_size)  # Reserve space for table
            
            # 4. Write sprite entries and build table
            sprite_table = []
            for sprite in self.sprites:
                offset = f.tell()
                length = self._write_sprite_entry(f, sprite)
                sprite_table.append((offset, length))
            
            # Go back and write the sprite table
            f.seek(table_start)
            for offset, length in sprite_table:
                f.write(struct.pack('<II', offset, length))
    
    def _write_sprite_entry(self, f: BinaryIO, sprite: Sprite) -> int:
        """Write a single sprite entry, returns total bytes written."""
        start_pos = f.tell()
        
        # 4.1 Sprite header (100 bytes)
        f.write(self.SPRITE_HEADER_MAGIC)
        f.write(b'\x00' * self.SPRITE_HEADER_PADDING)
        
        # 4.2 Rectangle data
        f.write(struct.pack('<I', len(sprite.rectangles)))
        for rect in sprite.rectangles:
            f.write(rect.to_bytes())
        
        # 4.3 Entry padding (4 bytes)
        f.write(b'\x00\x00\x00\x00')
        
        # 4.4 Image data
        f.write(sprite.image_data)
        
        return f.tell() - start_pos
    
    def add_sprite(self, sprite: Sprite):
        """Add a sprite to the PAK."""
        self.sprites.append(sprite)
    
    def extract_all(self, output_dir: Path, prefix: str = "sprite"):
        """Extract all sprites to directory as PNG + JSON."""
        output_dir.mkdir(parents=True, exist_ok=True)
        
        import json
        
        for i, sprite in enumerate(self.sprites):
            # Save image
            img = sprite.get_image()
            img_path = output_dir / f"{prefix}_{i}.png"
            img.save(img_path, 'PNG')
            
            # Save rectangles as JSON
            json_path = output_dir / f"{prefix}_rectangles_{i}.json"
            rects = [
                {
                    'x': r.x, 'y': r.y,
                    'width': r.width, 'height': r.height,
                    'pivotX': r.pivot_x, 'pivotY': r.pivot_y
                }
                for r in sprite.rectangles
            ]
            with open(json_path, 'w') as f:
                json.dump(rects, f, indent=2)
    
    @classmethod
    def from_directory(cls, directory: Path, prefix: str = None) -> 'PAKFile':
        """Load sprites from directory (PNG + JSON files)."""
        import json
        import re
        
        pak = cls()
        
        # Find all PNG files that match the pattern *_N.png
        all_pngs = list(directory.glob("*.png"))
        sprite_files = []
        
        for png_path in all_pngs:
            # Match files ending with _N.png where N is a number
            match = re.match(r'(.+)_(\d+)\.png$', png_path.name)
            if match:
                sprite_files.append((int(match.group(2)), png_path, match.group(1)))
        
        # Sort by sprite index
        sprite_files.sort(key=lambda x: x[0])
        
        for index, img_path, base_prefix in sprite_files:
            json_path = directory / f"{base_prefix}_rectangles_{index}.json"
            
            if not json_path.exists():
                print(f"Warning: Missing rectangles JSON for {img_path.name}, skipping")
                continue
            
            # Load image
            img = Image.open(img_path)
            
            # Load rectangles
            with open(json_path, 'r') as f:
                rect_data = json.load(f)
            
            rectangles = [
                SpriteRectangle(
                    x=r['x'], y=r['y'],
                    width=r['width'], height=r['height'],
                    pivot_x=r['pivotX'], pivot_y=r['pivotY']
                )
                for r in rect_data
            ]
            
            pak.add_sprite(Sprite.from_image(img, rectangles))
        
        return pak


# Convenience functions
def extract_pak(pak_path: Path, output_dir: Path, prefix: str = None):
    """Extract a PAK file to a directory."""
    if prefix is None:
        prefix = pak_path.stem
    
    pak = PAKFile.read(pak_path)
    pak.extract_all(output_dir, prefix)
    return len(pak.sprites)


def combine_directories(dir1: Path, dir2: Path, output_pak: Path, prefix1: str = None, prefix2: str = None):
    """Combine two directories of sprites into a single PAK."""
    if prefix1 is None:
        prefix1 = "sprite"
    if prefix2 is None:
        prefix2 = "sprite"
    
    # Load both directories
    pak1 = PAKFile.from_directory(dir1, prefix1)
    pak2 = PAKFile.from_directory(dir2, prefix2)
    
    # Combine
    combined = PAKFile()
    combined.sprites = pak1.sprites + pak2.sprites
    
    # Write
    output_pak.parent.mkdir(parents=True, exist_ok=True)
    combined.write(output_pak)
    
    return len(combined.sprites)


if __name__ == "__main__":
    # Example usage
    import sys
    
    if len(sys.argv) < 2:
        print("Usage:")
        print("  python paklib.py extract <pak_file> <output_dir>")
        print("  python paklib.py combine <dir1> <dir2> <output_pak>")
        print("  python paklib.py info <pak_file>")
        sys.exit(1)
    
    command = sys.argv[1]
    
    if command == "extract":
        pak_path = Path(sys.argv[2])
        output_dir = Path(sys.argv[3])
        count = extract_pak(pak_path, output_dir)
        print(f"Extracted {count} sprites to {output_dir}")
    
    elif command == "combine":
        dir1 = Path(sys.argv[2])
        dir2 = Path(sys.argv[3])
        output_pak = Path(sys.argv[4])
        count = combine_directories(dir1, dir2, output_pak)
        print(f"Combined {count} sprites into {output_pak}")
    
    elif command == "info":
        pak_path = Path(sys.argv[2])
        pak = PAKFile.read(pak_path)
        print(f"PAK File: {pak_path}")
        print(f"Sprite Count: {len(pak.sprites)}")
        for i, sprite in enumerate(pak.sprites):
            print(f"\nSprite {i}:")
            print(f"  Image Size: {len(sprite.image_data)} bytes")
            print(f"  Rectangles: {len(sprite.rectangles)}")
    
    else:
        print(f"Unknown command: {command}")
        sys.exit(1)
