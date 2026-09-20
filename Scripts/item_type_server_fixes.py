#!/usr/bin/env python3
"""
Mode 2 bulk script: Replace ItemType:: references in server ItemManager.cpp
with new item_type/is_stackable() equivalents.

Only handles the mechanical Consume||Arrow -> is_stackable() pattern.
Semantic changes (category, appearance_value, is_for_sale, use-item dispatch)
are done manually.

Usage:
  python Scripts/item_type_server_fixes.py --dry-run
  python Scripts/item_type_server_fixes.py
"""

import sys
import os
import re

DRY_RUN = "--dry-run" in sys.argv

# Target file
TARGET = os.path.join("Sources", "Server", "ItemManager.cpp")

def apply_fixes(filepath):
    with open(filepath, "r", encoding="utf-8") as f:
        content = f.read()

    original = content
    changes = []

    # Pattern 1: Two-line Consume || Arrow stacking check
    # Matches: (X->get_item_type() == ItemType::Consume) ||\n\t\t(X->get_item_type() == ItemType::Arrow)
    # Replace with: X->is_stackable()

    # This regex handles the most common form with varying whitespace
    pattern1 = re.compile(
        r'\(\s*'
        r'((?:m_game->)?[a-zA-Z_>\[\]\(\)0-9\-\.]+?)->get_item_type\(\)\s*==\s*ItemType::Consume\s*\)'
        r'\s*\|\|\s*\n\s*'
        r'\(\s*\1->get_item_type\(\)\s*==\s*ItemType::Arrow\s*\)',
        re.MULTILINE
    )

    def replace_with_stackable(m):
        accessor = m.group(1)
        changes.append(f"  Consume||Arrow -> is_stackable(): {accessor}")
        return f'{accessor}->is_stackable()'

    content = pattern1.sub(replace_with_stackable, content)

    # Pattern 2: Single-line form (some sites)
    pattern2 = re.compile(
        r'\(\s*'
        r'((?:m_game->)?[a-zA-Z_>\[\]\(\)0-9\-\.]+?)->get_item_type\(\)\s*==\s*ItemType::Consume\s*\)'
        r'\s*\|\|\s*'
        r'\(\s*\1->get_item_type\(\)\s*==\s*ItemType::Arrow\s*\)',
    )

    content = pattern2.sub(replace_with_stackable, content)

    # Pattern 3: Standalone ItemType::Equip -> item_type::equipment
    equip_pattern = re.compile(r'ItemType::Equip\b')
    equip_count = len(equip_pattern.findall(content))
    if equip_count > 0:
        content = equip_pattern.sub('hb::shared::item::item_type::equipment', content)
        changes.append(f"  ItemType::Equip -> item_type::equipment: {equip_count} replacements")

    # Pattern 4: Standalone ItemType::Material -> item_type::material
    material_pattern = re.compile(r'ItemType::Material\b')
    mat_count = len(material_pattern.findall(content))
    if mat_count > 0:
        content = material_pattern.sub('hb::shared::item::item_type::material', content)
        changes.append(f"  ItemType::Material -> item_type::material: {mat_count} replacements")

    # Pattern 5: Standalone ItemType::Arrow (not paired with Consume)
    # These are arrow-specific checks (ammo), not stacking checks
    arrow_pattern = re.compile(r'get_item_type\(\)\s*==\s*ItemType::Arrow\b')
    arrow_count = len(arrow_pattern.findall(content))
    if arrow_count > 0:
        content = arrow_pattern.sub('get_item_sub_type() == hb::shared::item::item_sub_type::ammo', content)
        changes.append(f"  ItemType::Arrow -> item_sub_type::ammo: {arrow_count} replacements")

    if content != original:
        if not DRY_RUN:
            with open(filepath, "w", encoding="utf-8") as f:
                f.write(content)
        print(f"{'[DRY RUN] ' if DRY_RUN else ''}Modified: {filepath}")
        for c in changes:
            print(c)

        # Count remaining ItemType:: references
        remaining = re.findall(r'ItemType::\w+', content)
        if remaining:
            print(f"\n  WARNING: {len(remaining)} remaining ItemType:: references:")
            for r in set(remaining):
                count = remaining.count(r)
                print(f"    {r}: {count}")
    else:
        print(f"No changes needed: {filepath}")

    return content != original

if __name__ == "__main__":
    os.chdir(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

    if not os.path.exists(TARGET):
        print(f"ERROR: {TARGET} not found")
        sys.exit(1)

    print(f"{'DRY RUN - ' if DRY_RUN else ''}Processing {TARGET}...")
    apply_fixes(TARGET)
