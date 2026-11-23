#!/usr/bin/env python3
"""
Item Type System Redesign — Mechanical Renames (Mode 2)

Renames CItem member variables and PacketItemConfigEntry fields across the codebase.
Only does safe, unambiguous whole-word replacements.

Usage:
    python Scripts/item_type_renames.py --dry-run    # Preview changes
    python Scripts/item_type_renames.py              # Apply changes
"""

import os
import re
import sys
import argparse
from pathlib import Path

# Scope: only these directories
SEARCH_DIRS = [
    "Sources/Dependencies/Shared",
    "Sources/Server",
    "Sources/Client",
    "Sources/SFMLEngine",
    "Sources/CControls",
]

# File extensions to process
EXTENSIONS = {".h", ".cpp", ".hpp", ".inl"}

# Replacements: (regex_pattern, replacement_string, description)
# All patterns use word boundaries for safety
REPLACEMENTS = [
    # CItem member renames
    (r"\bm_max_life_span\b", "m_durability", "CItem: m_max_life_span -> m_durability"),
    (r"\bm_cur_life_span\b", "m_cur_durability", "CItem: m_cur_life_span -> m_cur_durability"),
    (r"\bm_level_limit\b", "m_level_requirement", "CItem: m_level_limit -> m_level_requirement"),
    (r"\bm_gender_limit\b", "m_gender_requirement", "CItem: m_gender_limit -> m_gender_requirement"),
    (r"\bm_price\b", "m_sell_price", "CItem: m_price -> m_sell_price"),
    (r"\bm_speed\b", "m_swing_speed", "CItem: m_speed -> m_swing_speed"),

    # PacketItemConfigEntry member renames (camelCase, unique names)
    (r"\bmaxLifeSpan\b", "durability", "Packet: maxLifeSpan -> durability"),
    (r"\blevelLimit\b", "levelRequirement", "Packet: levelLimit -> levelRequirement"),
    (r"\bgenderLimit\b", "genderRequirement", "Packet: genderLimit -> genderRequirement"),
]


def find_source_files(base_dir):
    """Find all source files in scope."""
    files = []
    for search_dir in SEARCH_DIRS:
        full_dir = os.path.join(base_dir, search_dir)
        if not os.path.isdir(full_dir):
            continue
        for root, dirs, filenames in os.walk(full_dir):
            # Skip build directories
            dirs[:] = [d for d in dirs if d not in {"build", "Debug_x64", "Release_x64", "x64", ".vs"}]
            for fn in filenames:
                ext = os.path.splitext(fn)[1].lower()
                if ext in EXTENSIONS:
                    files.append(os.path.join(root, fn))
    return sorted(files)


def process_file(filepath, dry_run=False):
    """Process a single file, applying all replacements. Returns (changed, details)."""
    try:
        with open(filepath, "r", encoding="utf-8", errors="replace") as f:
            original = f.read()
    except Exception as e:
        return False, [f"  ERROR reading: {e}"]

    content = original
    details = []

    for pattern, replacement, desc in REPLACEMENTS:
        matches = list(re.finditer(pattern, content))
        if matches:
            count = len(matches)
            # Show line numbers of matches
            lines_with_matches = set()
            for m in matches:
                line_num = content[:m.start()].count("\n") + 1
                lines_with_matches.add(line_num)
            line_str = ", ".join(str(ln) for ln in sorted(lines_with_matches))
            details.append(f"  {desc}: {count} match(es) on line(s) {line_str}")
            content = re.sub(pattern, replacement, content)

    if content != original:
        if not dry_run:
            with open(filepath, "w", encoding="utf-8", newline="\n") as f:
                f.write(content)
        return True, details
    return False, details


def main():
    parser = argparse.ArgumentParser(description="Item type system mechanical renames")
    parser.add_argument("--dry-run", action="store_true", help="Preview changes without applying")
    parser.add_argument("--verify", action="store_true", help="Check for potential collisions")
    args = parser.parse_args()

    base_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

    if args.verify:
        print("=== VERIFICATION MODE ===")
        print("Checking for potential naming collisions...")
        files = find_source_files(base_dir)

        # Check if any target names already exist (would cause collision)
        collisions = []
        for _, replacement, desc in REPLACEMENTS:
            pattern = r"\b" + re.escape(replacement) + r"\b"
            for f in files:
                try:
                    with open(f, "r", encoding="utf-8", errors="replace") as fh:
                        content = fh.read()
                    matches = list(re.finditer(pattern, content))
                    if matches:
                        rel = os.path.relpath(f, base_dir)
                        for m in matches:
                            line_num = content[:m.start()].count("\n") + 1
                            collisions.append(f"  COLLISION: '{replacement}' already exists in {rel}:{line_num}")
                except Exception:
                    pass

        if collisions:
            print(f"\nFound {len(collisions)} potential collisions:")
            for c in collisions:
                print(c)
            print("\nThese may be false positives if the replacement is in a different context.")
        else:
            print("No collisions found.")
        return

    mode = "DRY RUN" if args.dry_run else "APPLYING"
    print(f"=== {mode} ===")

    files = find_source_files(base_dir)
    print(f"Scanning {len(files)} source files...")

    changed_count = 0
    total_changes = 0

    # Output log
    log_dir = os.path.join(base_dir, "Scripts", "output")
    os.makedirs(log_dir, exist_ok=True)
    log_path = os.path.join(log_dir, "item_type_renames.log")

    with open(log_path, "w", encoding="utf-8") as log:
        for filepath in files:
            changed, details = process_file(filepath, dry_run=args.dry_run)
            if changed:
                rel = os.path.relpath(filepath, base_dir)
                changed_count += 1
                total_changes += len(details)
                header = f"\n{rel}:"
                print(header)
                log.write(header + "\n")
                for d in details:
                    print(d)
                    log.write(d + "\n")

    print(f"\n{'Would modify' if args.dry_run else 'Modified'} {changed_count} file(s) with {total_changes} replacement(s).")
    print(f"Full log: {log_path}")


if __name__ == "__main__":
    main()
