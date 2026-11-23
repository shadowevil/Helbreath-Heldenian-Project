#!/usr/bin/env python3
"""
Mode 2 bulk script: Replace ItemType:: references in client code
with new item_type/is_stackable() equivalents.

Handles mechanical replacements only. Semantic changes (use-item dispatch,
CombatSystem, packet parsing, m_appearance_value) are done manually.

Usage:
  python Scripts/item_type_client_fixes.py --dry-run
  python Scripts/item_type_client_fixes.py
"""

import sys
import os
import re
import glob

DRY_RUN = "--dry-run" in sys.argv

# All client .cpp files
CLIENT_DIR = os.path.join("Sources", "Client")

def apply_fixes(filepath):
    with open(filepath, "r", encoding="utf-8") as f:
        content = f.read()

    original = content
    changes = []

    # Pattern 1: Two-line Consume || Arrow stacking check
    # (X->get_item_type() == ItemType::Consume) ||\n\t(X->get_item_type() == ItemType::Arrow)
    # Replace with: X->is_stackable()
    pattern1 = re.compile(
        r'\(\s*'
        r'((?:m_game->)?[a-zA-Z_>0-9\[\]\(\)\-\.]+?)->get_item_type\(\)\s*==\s*ItemType::Consume\s*\)'
        r'\s*\|\|\s*\n\s*'
        r'\(\s*\1->get_item_type\(\)\s*==\s*ItemType::Arrow\s*\)',
        re.MULTILINE
    )

    def replace_with_stackable(m):
        accessor = m.group(1)
        changes.append(f"  Consume||Arrow -> is_stackable(): {accessor}")
        return f'{accessor}->is_stackable()'

    content = pattern1.sub(replace_with_stackable, content)

    # Pattern 2: Single-line Consume || Arrow
    pattern2 = re.compile(
        r'\(\s*'
        r'((?:m_game->)?[a-zA-Z_>0-9\[\]\(\)\-\.]+?)->get_item_type\(\)\s*==\s*ItemType::Consume\s*\)'
        r'\s*\|\|\s*'
        r'\(\s*\1->get_item_type\(\)\s*==\s*ItemType::Arrow\s*\)',
    )

    content = pattern2.sub(replace_with_stackable, content)

    # Pattern 2b: Inverted form - != Consume && != Arrow (means NOT stackable)
    # (X->get_item_type() != ItemType::Consume) &&\n\t(X->get_item_type() != ItemType::Arrow)
    # Replace with: !X->is_stackable()
    pattern2b = re.compile(
        r'\(\s*'
        r'((?:m_game->)?[a-zA-Z_>0-9\[\]\(\)\-\.]+?)->get_item_type\(\)\s*!=\s*ItemType::Consume\s*\)'
        r'\s*&&\s*\n?\s*'
        r'\(\s*\1->get_item_type\(\)\s*!=\s*ItemType::Arrow\s*\)',
    )

    def replace_with_not_stackable(m):
        accessor = m.group(1)
        changes.append(f"  !Consume && !Arrow -> !is_stackable(): {accessor}")
        return f'!{accessor}->is_stackable()'

    content = pattern2b.sub(replace_with_not_stackable, content)

    # Pattern 3: Bare variable form (item_type == ItemType::Consume) || (item_type == ItemType::Arrow)
    # Used in NetworkMessages_Items.cpp where item_type is a local variable
    pattern3 = re.compile(
        r'\(\s*item_type\s*==\s*ItemType::Consume\s*\)'
        r'\s*\|\|\s*'
        r'\(\s*item_type\s*==\s*ItemType::Arrow\s*\)',
    )

    def replace_var_stackable(m):
        changes.append("  item_type Consume||Arrow -> item_stackable (manual check needed)")
        return 'item_stackable'

    content = pattern3.sub(replace_var_stackable, content)

    # Pattern 4: Standalone ItemType::Equip -> item_type::equipment
    equip_pattern = re.compile(r'ItemType::Equip\b')
    equip_count = len(equip_pattern.findall(content))
    if equip_count > 0:
        content = equip_pattern.sub('hb::shared::item::item_type::equipment', content)
        changes.append(f"  ItemType::Equip -> item_type::equipment: {equip_count} replacements")

    # Pattern 5: Standalone ItemType::Material -> item_type::material
    material_pattern = re.compile(r'ItemType::Material\b')
    mat_count = len(material_pattern.findall(content))
    if mat_count > 0:
        content = material_pattern.sub('hb::shared::item::item_type::material', content)
        changes.append(f"  ItemType::Material -> item_type::material: {mat_count} replacements")

    # Pattern 6: Standalone ItemType::Arrow (not paired with Consume)
    # These are arrow-specific checks (ammo), not stacking checks
    arrow_pattern = re.compile(r'get_item_type\(\)\s*==\s*ItemType::Arrow\b')
    arrow_count = len(arrow_pattern.findall(content))
    if arrow_count > 0:
        content = arrow_pattern.sub('get_item_sub_type() == hb::shared::item::item_sub_type::ammo', content)
        changes.append(f"  ItemType::Arrow -> item_sub_type::ammo: {arrow_count} replacements")

    # Pattern 6b: check_item_by_type(ItemType::Arrow) - function call with Arrow
    arrow_func_pattern = re.compile(r'ItemType::Arrow\b')
    arrow_func_count = len(arrow_func_pattern.findall(content))
    if arrow_func_count > 0:
        content = arrow_func_pattern.sub('hb::shared::item::item_sub_type::ammo', content)
        changes.append(f"  Remaining ItemType::Arrow -> item_sub_type::ammo: {arrow_func_count} replacements")

    # Pattern 7: is_true_stack_type(X) -> X->is_stackable() or similar
    stack_pattern = re.compile(r'is_true_stack_type\(\s*([^)]+?)->get_item_type\(\)\s*\)')
    stack_count = len(stack_pattern.findall(content))
    if stack_count > 0:
        content = stack_pattern.sub(r'\1->is_stackable()', content)
        changes.append(f"  is_true_stack_type(X->get_item_type()) -> X->is_stackable(): {stack_count} replacements")

    # Pattern 7b: is_true_stack_type(variable)
    stack_pattern2 = re.compile(r'is_true_stack_type\(\s*(\w+)\s*\)')
    if stack_pattern2.search(content):
        changes.append("  WARNING: is_true_stack_type with bare variable (needs manual fix)")

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
            print(f"\n  Remaining ItemType:: ({len(remaining)}):")
            for r in set(remaining):
                count = remaining.count(r)
                print(f"    {r}: {count}")
        print()
    else:
        pass  # No output for unchanged files

    return content != original

if __name__ == "__main__":
    os.chdir(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

    if not os.path.isdir(CLIENT_DIR):
        print(f"ERROR: {CLIENT_DIR} not found")
        sys.exit(1)

    print(f"{'DRY RUN - ' if DRY_RUN else ''}Processing client files...")
    total = 0
    for cpp_file in sorted(glob.glob(os.path.join(CLIENT_DIR, "*.cpp"))):
        if apply_fixes(cpp_file):
            total += 1

    print(f"\n{'[DRY RUN] ' if DRY_RUN else ''}Total files modified: {total}")
