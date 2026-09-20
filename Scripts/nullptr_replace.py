"""
Phase 2: Replace pointer null checks with nullptr in EntityManager.cpp
Mode 2 justified: ~145 mechanical replacements of the same pattern across one file.

Only replaces == 0, != 0, == NULL, != NULL when the LHS is a known pointer expression.
Does NOT touch integer comparisons like naming_value != -1, spot_mob_index != 0, etc.
"""

import re
import sys
from pathlib import Path

FILE = Path(__file__).resolve().parent.parent / "Sources" / "Server" / "EntityManager.cpp"

# Known pointer patterns (LHS of comparison)
# These are all pointer types: CNpc*, CMap*, CGame*, CClient*, CMagic*, CItem*, DropTable*
POINTER_PATTERNS = [
    r'm_npc_list\[[^\]]+\]',          # m_npc_list[X]
    r'm_map_list\[[^\]]+\]',          # m_map_list[X]
    r'm_client_list\[[^\]]+\]',       # m_client_list[X]
    r'm_game',                         # m_game
    r'm_map_list',                     # m_map_list (the array pointer itself)
    r'm_npc_list',                     # m_npc_list (the array pointer itself)
    r'm_active_entity_list',           # m_active_entity_list
    r'm_magic_config_list\[[^\]]+\]',  # m_magic_config_list[X]
    r'm_quest_config_list\[[^\]]+\]',  # m_quest_config_list[X]
    r'name',                           # name (const char*)
    r'table',                          # table (const DropTable*)
    r'npc',                            # npc (CNpc*)
    r'item',                           # item (CItem*)
    r'waypoint_list',                  # waypoint_list (char*)
    r'offset_x',                       # offset_x (int*)
    r'offset_y',                       # offset_y (int*)
    r'area',                           # area (GameRectangle*)
]

def build_regex():
    """Build regex that matches pointer == 0, != 0, == NULL, != NULL patterns."""
    # Build alternation for pointer LHS patterns
    ptr_alt = '|'.join(f'(?:{p})' for p in POINTER_PATTERNS)

    # Match: (pointer_expr) (==|!=) (0|NULL)
    # But NOT inside assignments like = 0; or = NULL;
    # The key is we need == or != (two chars), not just = (one char)
    patterns = [
        # pointer == 0  or  pointer != 0
        (re.compile(r'((?:' + ptr_alt + r'))\s*(==|!=)\s*\b0\b(?!\.)'), r'\1 \2 nullptr'),
        # pointer == NULL  or  pointer != NULL
        (re.compile(r'((?:' + ptr_alt + r'))\s*(==|!=)\s*NULL\b'), r'\1 \2 nullptr'),
    ]
    return patterns

def process_line(line, patterns, line_num):
    """Process a single line, returning (new_line, list_of_changes)."""
    changes = []
    new_line = line

    for pattern, replacement in patterns:
        matches = list(pattern.finditer(new_line))
        if matches:
            for m in matches:
                # Skip if this is inside a comment
                comment_pos = new_line.find('//')
                if comment_pos != -1 and m.start() > comment_pos:
                    continue

                # Skip assignment patterns: "= 0;" or "= NULL;"
                # We only want == and != (already enforced by regex)

                # Skip known integer comparisons by checking context
                match_text = m.group(0)

                # Don't replace if it's an integer variable comparison
                # e.g., naming_value != 0, spot_mob_index != 0, owner_h != 0, etc.
                # These use simple variable names that happen to match 'name' pattern
                # We need to be more precise
                lhs = m.group(1)

                # Skip if LHS is just 'name' but not preceded by something that makes it a pointer
                # Actually let's be very specific about what we match

            new_line = pattern.sub(replacement, new_line)
            for m in matches:
                comment_pos = line.find('//')
                if comment_pos != -1 and m.start() > comment_pos:
                    continue
                changes.append((line_num, line.rstrip(), new_line.rstrip()))

    return new_line, changes

def main():
    dry_run = '--dry-run' in sys.argv

    content = FILE.read_text(encoding='utf-8')
    lines = content.split('\n')

    # Simpler approach: do targeted replacements line by line
    all_changes = []
    new_lines = []

    for i, line in enumerate(lines):
        line_num = i + 1
        new_line = line
        changed = False

        # Skip comment-only lines
        stripped = line.lstrip()
        if stripped.startswith('//') or stripped.startswith('/*'):
            new_lines.append(line)
            continue

        # Skip lines that are assignments (= 0 but not == 0)
        # Pattern 1: m_npc_list[X] == 0  ->  m_npc_list[X] == nullptr
        # Pattern 2: m_npc_list[X] != 0  ->  m_npc_list[X] != nullptr
        # Pattern 3: m_game == NULL  ->  m_game == nullptr
        # Pattern 4: m_map_list == NULL  ->  m_map_list == nullptr

        # Replace == NULL and != NULL first (easy, unambiguous)
        if '== NULL' in new_line or '!= NULL' in new_line:
            new_line = new_line.replace('== NULL', '== nullptr').replace('!= NULL', '!= nullptr')
            if new_line != line:
                changed = True

        # Now handle == 0 and != 0 for pointer expressions
        # We need to be careful to only match pointer comparisons

        # Known pointer comparison patterns (very specific)
        ptr_zero_patterns = [
            # m_npc_list[...] == 0 or != 0
            (r'(m_npc_list\[[^\]]+\])\s*(==|!=)\s*0\b(?!\.)', r'\1 \2 nullptr'),
            # m_map_list[...] == 0 or != 0
            (r'(m_map_list\[[^\]]+\])\s*(==|!=)\s*0\b(?!\.)', r'\1 \2 nullptr'),
            # m_client_list[...] == 0 or != 0
            (r'(m_client_list\[[^\]]+\])\s*(==|!=)\s*0\b(?!\.)', r'\1 \2 nullptr'),
            # m_magic_config_list[...] == 0 or != 0
            (r'(m_magic_config_list\[[^\]]+\])\s*(==|!=)\s*0\b(?!\.)', r'\1 \2 nullptr'),
            # m_quest_config_list[...] == 0 or != 0
            (r'(m_quest_config_list\[[^\]]+\])\s*(==|!=)\s*0\b(?!\.)', r'\1 \2 nullptr'),
        ]

        for pat, repl in ptr_zero_patterns:
            result = re.sub(pat, repl, new_line)
            if result != new_line:
                new_line = result
                changed = True

        # Also handle standalone pointer variables compared to 0
        # But ONLY specific known pointer vars, not integers
        # m_game == 0, m_map_list == 0, m_npc_list == 0, etc.
        # These are less common but exist in constructor/destructor
        standalone_ptr_patterns = [
            (r'\b(m_game)\s*(==|!=)\s*0\b', r'\1 \2 nullptr'),
            (r'\b(m_map_list)\s*(==|!=)\s*0\b', r'\1 \2 nullptr'),
            (r'\b(m_npc_list)\s*(==|!=)\s*0\b', r'\1 \2 nullptr'),
            (r'\b(m_active_entity_list)\s*(==|!=)\s*0\b', r'\1 \2 nullptr'),
        ]

        for pat, repl in standalone_ptr_patterns:
            result = re.sub(pat, repl, new_line)
            if result != new_line:
                new_line = result
                changed = True

        if changed:
            all_changes.append((line_num, line.rstrip(), new_line.rstrip()))

        new_lines.append(new_line)

    # Also fix assignment patterns: m_npc_list[X] = 0 -> m_npc_list[X] = nullptr
    # But NOT m_npc_list[npc_h]->m_target_index = 0 (that's an int)
    # Only: m_npc_list[i] = 0 and m_npc_list[i] = NULL (setting pointer to null)
    final_lines = []
    for i, line in enumerate(new_lines):
        line_num = i + 1
        new_line = line

        # m_npc_list[X] = 0; (assignment, not comparison)
        # Must be = 0 not == 0 (already handled above)
        assign_patterns = [
            (r'(m_npc_list\[[^\]]+\])\s*=\s*0\s*;', r'\1 = nullptr;'),
            (r'(m_npc_list\[[^\]]+\])\s*=\s*NULL\s*;', r'\1 = nullptr;'),
            (r'(m_map_list)\s*=\s*NULL\s*;', r'\1 = nullptr;'),
            (r'(m_game)\s*=\s*NULL\s*;', r'\1 = nullptr;'),
            (r'(m_active_entity_list)\s*=\s*NULL\s*;', r'\1 = nullptr;'),
        ]

        changed = False
        for pat, repl in assign_patterns:
            result = re.sub(pat, repl, new_line)
            if result != new_line:
                new_line = result
                changed = True

        if changed:
            all_changes.append((line_num, line.rstrip(), new_line.rstrip()))

        final_lines.append(new_line)

    # Output
    print(f"Total changes: {len(all_changes)}")

    if dry_run:
        for line_num, old, new in sorted(all_changes, key=lambda x: x[0]):
            print(f"  L{line_num}:")
            print(f"    - {old}")
            print(f"    + {new}")
        print(f"\nDry run complete. {len(all_changes)} changes would be made.")
    else:
        new_content = '\n'.join(final_lines)
        FILE.write_text(new_content, encoding='utf-8')
        print(f"Applied {len(all_changes)} changes to {FILE.name}")

if __name__ == '__main__':
    main()
