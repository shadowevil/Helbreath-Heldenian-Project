#!/usr/bin/env python3
"""
Fix weapon PAK files: blank out incorrect attack pose sprites.

- Melee/wand weapons: blank sprite 7 (male bow pose) and 19 (female bow pose)
- Bow weapons: blank sprite 6 (male melee pose) and 18 (female melee pose)

Operates on both Binaries/Game/sprites/items/ and tools/DisplayIDManager/sprites/items/.
"""

import sys, shutil
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO / 'tools' / 'DisplayIDManager'))
from paklib import PAKFile, Sprite
from PIL import Image


def make_blank_sprite():
    """Create a 1x1 transparent sprite with 0 rects (matches pak_generator)."""
    img = Image.new("RGBA", (1, 1), (0, 0, 0, 0))
    return Sprite.from_image(img, [])


# Melee/wand PAK files: blank sprite 7 (male) and 19 (female)
MELEE_PAKS = [
    "dagger.pak",
    "short_sword.pak",
    "long_sword.pak",
    "sabre.pak",
    "excalibur.pak",
    "scimitar.pak",
    "esterk.pak",
    "kloness_esterk.pak",
    "double_axe.pak",
    "war_axe.pak",
    "light_axe.pak",
    "saxon_axe.pak",
    "golden_axe.pak",
    "pick_axe.pak",
    "hoe.pak",
    "berserk_magic_wand_ms.20.pak",
    "magic_wand_ms30_llf.pak",
    "magic_wand_ms20.pak",
    "dark_mage_templar.pak",
    "resurrection_magic_wand_ms.20.pak",
    "kloness_magic_wand_ms.20.pak",
    "broad_sword.pak",
    "bastad_sword.pak",
    "claymore.pak",
    "great_sword.pak",
    "flameberge.pak",
    "giant_sword.pak",
    "dark_knight_templar.pak",
    "storm_bringer.pak",
    "dark_executor.pak",
    "kloness_blade.pak",
    "the_devastator.pak",
    "battle_axe.pak",
    "kloness_axe.pak",
    "lighting_blade.pak",
    "hammer.pak",
    "battle_hammer.pak",
    "barbarian_hammer.pak",
    "black_shadow_sword.pak",
    "falchion.pak",
]

# Bow PAK files: blank sprite 6 (male) and 18 (female)
BOW_PAKS = [
    "short_bow.pak",
    "long_bow.pak",
    "direction_bow.pak",
    "fire_bow.pak",
]

PAK_DIRS = [
    REPO / "Binaries" / "Game" / "sprites" / "items",
    REPO / "tools" / "DisplayIDManager" / "sprites" / "items",
]


def patch_pak(pak_path, blank_male_idx, blank_female_idx, dry_run=False):
    """Replace specific sprites with blanks in a PAK file."""
    pak = PAKFile.read(pak_path)

    if len(pak.sprites) < 24:
        return f"  SKIP {pak_path.name}: only {len(pak.sprites)} sprites (expected 24)"

    male_was_blank = len(pak.sprites[blank_male_idx].rectangles) == 0
    female_was_blank = len(pak.sprites[blank_female_idx].rectangles) == 0

    if male_was_blank and female_was_blank:
        return f"  SKIP {pak_path.name}: sprites {blank_male_idx}/{blank_female_idx} already blank"

    male_old_rects = len(pak.sprites[blank_male_idx].rectangles)
    female_old_rects = len(pak.sprites[blank_female_idx].rectangles)
    male_old_bytes = len(pak.sprites[blank_male_idx].image_data)
    female_old_bytes = len(pak.sprites[blank_female_idx].image_data)

    blank = make_blank_sprite()
    pak.sprites[blank_male_idx] = blank
    pak.sprites[blank_female_idx] = blank

    if not dry_run:
        pak.write(pak_path)

    action = "WOULD PATCH" if dry_run else "PATCHED"
    return (f"  {action} {pak_path.name}: "
            f"S{blank_male_idx} ({male_old_rects} rects, {male_old_bytes}B -> blank), "
            f"S{blank_female_idx} ({female_old_rects} rects, {female_old_bytes}B -> blank)")


def main():
    dry_run = "--dry-run" in sys.argv

    if dry_run:
        print("=== DRY RUN MODE ===\n")
    else:
        print("=== APPLYING CHANGES ===\n")

    total_patched = 0
    total_skipped = 0

    for pak_dir in PAK_DIRS:
        if not pak_dir.exists():
            print(f"Directory not found: {pak_dir}\n")
            continue

        print(f"Directory: {pak_dir}")
        print(f"{'='*80}")

        # Melee weapons: blank sprite 7 (male) and 19 (female)
        print("\nMelee/Wand weapons (blanking sprites 7 and 19):")
        for pak_name in MELEE_PAKS:
            pak_path = pak_dir / pak_name
            if not pak_path.exists():
                print(f"  MISSING {pak_name}")
                continue
            result = patch_pak(pak_path, 7, 19, dry_run)
            print(result)
            if "PATCHED" in result or "WOULD PATCH" in result:
                total_patched += 1
            else:
                total_skipped += 1

        # Bow weapons: blank sprite 6 (male) and 18 (female)
        print("\nBow weapons (blanking sprites 6 and 18):")
        for pak_name in BOW_PAKS:
            pak_path = pak_dir / pak_name
            if not pak_path.exists():
                print(f"  MISSING {pak_name}")
                continue
            result = patch_pak(pak_path, 6, 18, dry_run)
            print(result)
            if "PATCHED" in result or "WOULD PATCH" in result:
                total_patched += 1
            else:
                total_skipped += 1

        print()

    print(f"Total: {total_patched} patched, {total_skipped} skipped")


if __name__ == "__main__":
    main()
