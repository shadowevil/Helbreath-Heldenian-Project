"""
Mode 2 Script: CItem composition refactor — bulk field renames.

Touches ~45 files with ~1000+ mechanical field renames across three groups:
  A) CItem instance field -> m_instance.field (16 fields)
  B) lifespan -> durability renames (fields, enums, structs, handlers, SQL, strings)
  C) Type compatibility fixes (bool->uint8_t, enum->uint8_t)

Justification: Purely mechanical field name replacements across a large surface area.
Script appropriate because all transforms are find/replace with no logic changes.

Usage:
  python Scripts/citem_composition_rename.py --dry-run
  python Scripts/citem_composition_rename.py --verify
  python Scripts/citem_composition_rename.py
"""

import re
import sys
import os
import subprocess
import argparse
from pathlib import Path
from collections import defaultdict

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.join(SCRIPT_DIR, "..")
OUTPUT_DIR = os.path.join(SCRIPT_DIR, "output")
SCRIPT_NAME = "citem_composition_rename"

log_lines = []

def log(msg):
    print(msg)
    log_lines.append(msg)


def read_file(path):
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        return f.read()


def write_file(path, content):
    with open(path, "w", encoding="utf-8") as f:
        f.write(content)


def write_log(filename):
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    path = os.path.join(OUTPUT_DIR, filename)
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(log_lines))
    print(f"\nLog written to: {path}")


# ============================================================
# Files to EXCLUDE (manually updated in Phase 1)
# ============================================================
EXCLUDED_FILES = {
    os.path.normpath("Sources/Dependencies/Shared/Item/ItemInstanceData.h"),
    os.path.normpath("Sources/Dependencies/Shared/Item/Item.h"),
    os.path.normpath("Sources/Client/Tile.h"),
    os.path.normpath("Sources/Client/MapData.h"),
    os.path.normpath("Sources/Client/MapData.cpp"),
    os.path.normpath("Sources/Client/ItemNameFormatter.h"),
    os.path.normpath("Sources/Client/ItemNameFormatter.cpp"),
    os.path.normpath("Sources/Client/DialogBox_Exchange.h"),
    os.path.normpath("Sources/Dependencies/Shared/Packet/PacketEvent.h"),
    os.path.normpath("Sources/Dependencies/Shared/Packet/PacketNotify.h"),
    os.path.normpath("Sources/Dependencies/Shared/Packet/PacketResponse.h"),
    os.path.normpath("Sources/Server/AccountSqliteStore.h"),
    os.path.normpath("Sources/Server/GameConfigSqliteStore.h"),
    os.path.normpath("Sources/Dependencies/Shared/Net/NetMessages.h"),
    os.path.normpath("Sources/Client/NetworkMessageManager.cpp"),
    os.path.normpath("Sources/Client/LAN_ENG.H"),
}

# ============================================================
# Target directories
# ============================================================
TARGET_DIRS = [
    os.path.join(ROOT_DIR, "Sources", "Server"),
    os.path.join(ROOT_DIR, "Sources", "Client"),
    os.path.join(ROOT_DIR, "Sources", "Dependencies", "Shared"),
]

CPP_EXTENSIONS = {".h", ".cpp", ".hpp"}

# ============================================================
# Group A: CItem instance field -> m_instance.field
# Ordered LONGEST-FIRST to avoid substring collisions
# ============================================================
GROUP_A_RENAMES = [
    # Longest names first
    ("m_item_special_effect_value1", "m_instance.special_effect_value1"),
    ("m_item_special_effect_value2", "m_instance.special_effect_value2"),
    ("m_item_special_effect_value3", "m_instance.special_effect_value3"),
    ("m_touch_effect_value1", "m_instance.touch_effect_value1"),
    ("m_touch_effect_value2", "m_instance.touch_effect_value2"),
    ("m_touch_effect_value3", "m_instance.touch_effect_value3"),
    ("m_touch_effect_type", "m_instance.touch_effect_type"),
    ("m_secondary_value", "m_instance.secondary_value"),
    ("m_secondary_type", "m_instance.secondary_type"),
    ("m_enchant_bonus", "m_instance.enchant_bonus"),
    ("m_prefix_value", "m_instance.prefix_value"),
    ("m_cur_durability", "m_instance.cur_durability"),
    ("m_custom_made", "m_instance.custom_made"),
    ("m_prefix_type", "m_instance.prefix_type"),
    ("m_item_color", "m_instance.item_color"),
    ("m_count", "m_instance.count"),
]

# ============================================================
# Group B: Lifespan -> Durability renames
# Ordered LONGEST-FIRST to avoid substring collisions
# ============================================================

# Plain string replacements (no regex needed)
GROUP_B_STRING_RENAMES = [
    # Packet struct names (longest first)
    ("PacketNotifyCurLifeSpan", "PacketNotifyCurDurability"),
    ("PacketNotifyItemLifeSpanEnd", "PacketNotifyItemDurabilityEnd"),
    # Handler function names
    ("HandleCurLifeSpan", "HandleCurDurability"),
    ("HandleItemLifeSpanEnd", "HandleItemDurabilityEnd"),
    # Enum values
    ("ItemLifeSpanEnd", "ItemDurabilityEnd"),
    ("CurLifeSpan", "CurDurability"),
    # String constant
    ("NOTIFYMSG_ITEMLIFE_SPANEND1", "NOTIFYMSG_ITEM_DURABILITY_END1"),
    # Server function name
    ("calculate_endurance_decrement", "calculate_durability_decrement"),
    # Field names (longest first to avoid substring collisions)
    ("cur_life_span", "cur_durability"),
    ("max_life_span", "max_durability"),
    ("cur_lifespan", "cur_durability"),
    ("max_lifespan", "max_durability"),
]

# Regex replacements (word-boundary needed)
GROUP_B_REGEX_RENAMES = [
    # cur_life / max_life with word boundaries (avoid matching cur_lifespan)
    (r"\bcur_life\b", "cur_durability"),
    (r"\bmax_life\b", "max_durability"),
]

# SQL string literals (inside quoted strings)
GROUP_B_SQL_RENAMES = [
    # Column names in SQL strings — these are inside quotes in .cpp files
    # Already handled by the plain string renames above (cur_lifespan -> cur_durability, etc.)
    # But we need the config struct field:
    ("entry.lifespan", "entry.durability"),
]

# Display text renames (LAN_ENG.H) — handled in Phase 1, excluded from script

# Comment renames
GROUP_B_COMMENT_RENAMES = [
    ("// Items - LifeSpan/Released", "// Items - Durability/Released"),
]

# ============================================================
# Group C: Type compatibility fixes
# Applied AFTER Groups A and B
# ============================================================
GROUP_C_FIXES = [
    # Bool -> uint8_t assignments
    ("m_instance.custom_made = false", "m_instance.custom_made = 0"),
    ("m_instance.custom_made = true", "m_instance.custom_made = 1"),
    # Enum -> uint8_t assignments
    ("m_instance.prefix_type = AttributePrefixType::None", "m_instance.prefix_type = 0"),
    ("m_instance.prefix_type = hb::shared::item::AttributePrefixType::None", "m_instance.prefix_type = 0"),
    ("m_instance.secondary_type = SecondaryEffectType::None", "m_instance.secondary_type = 0"),
    ("m_instance.secondary_type = hb::shared::item::SecondaryEffectType::None", "m_instance.secondary_type = 0"),
]

# Regex-based Group C: Enum comparisons
# These need to handle both prefixed and unprefixed enum access
GROUP_C_REGEX_FIXES = [
    # prefix_type == AttributePrefixType::X -> prefix_type == static_cast<uint8_t>(AttributePrefixType::X)
    (r"m_instance\.prefix_type\s*==\s*((?:hb::shared::item::)?AttributePrefixType::\w+)",
     lambda m: f"m_instance.prefix_type == static_cast<uint8_t>({m.group(1)})"),
    (r"m_instance\.prefix_type\s*!=\s*((?:hb::shared::item::)?AttributePrefixType::\w+)",
     lambda m: f"m_instance.prefix_type != static_cast<uint8_t>({m.group(1)})"),
    # secondary_type == SecondaryEffectType::X
    (r"m_instance\.secondary_type\s*==\s*((?:hb::shared::item::)?SecondaryEffectType::\w+)",
     lambda m: f"m_instance.secondary_type == static_cast<uint8_t>({m.group(1)})"),
    (r"m_instance\.secondary_type\s*!=\s*((?:hb::shared::item::)?SecondaryEffectType::\w+)",
     lambda m: f"m_instance.secondary_type != static_cast<uint8_t>({m.group(1)})"),
]

# SQL column renames in string literals
# Pattern: find "cur_lifespan" (already in GROUP_B), "lifespan" -> "durability" in SQL
GROUP_B_SQL_COLUMN_RENAMES = [
    # In GameConfigSqliteStore.cpp SQL strings
    # "max_lifespan" already handled by GROUP_B_STRING
    # But "lifespan" alone (not cur_/max_ prefixed) needs word-boundary replacement in SQL
]


def collect_files():
    """Collect all .cpp/.h files in target directories, excluding Phase 1 files."""
    files = []
    for target_dir in TARGET_DIRS:
        for root, dirs, filenames in os.walk(target_dir):
            for fn in filenames:
                ext = os.path.splitext(fn)[1].lower()
                if ext not in CPP_EXTENSIONS:
                    continue
                full_path = os.path.join(root, fn)
                rel_path = os.path.relpath(full_path, ROOT_DIR)
                if os.path.normpath(rel_path) in EXCLUDED_FILES:
                    continue
                files.append(full_path)
    return sorted(files)


def apply_renames(content, dry_run=False):
    """Apply all rename groups to content. Returns (new_content, changes_list)."""
    changes = []
    original = content

    # Group A: CItem instance fields -> m_instance.field
    for old, new in GROUP_A_RENAMES:
        # Use word boundary regex to avoid partial matches
        # But m_count, m_prefix_type etc. are unique enough for plain replace
        # However, we need to be careful: m_count shouldn't match dm_count or similar
        pattern = r'(?<![a-zA-Z_])' + re.escape(old) + r'(?![a-zA-Z_0-9])'
        matches = list(re.finditer(pattern, content))
        if matches:
            changes.append((old, new, len(matches)))
            content = re.sub(pattern, new, content)

    # Group B: String renames (lifespan -> durability)
    for old, new in GROUP_B_STRING_RENAMES:
        count = content.count(old)
        if count > 0:
            changes.append((old, new, count))
            content = content.replace(old, new)

    # Group B: Regex renames (cur_life/max_life with word boundaries)
    for pattern, new in GROUP_B_REGEX_RENAMES:
        matches = list(re.finditer(pattern, content))
        if matches:
            changes.append((pattern, new, len(matches)))
            content = re.sub(pattern, new, content)

    # Group B: SQL column renames
    for old, new in GROUP_B_SQL_RENAMES:
        count = content.count(old)
        if count > 0:
            changes.append((old, new, count))
            content = content.replace(old, new)

    # Group B: Comment renames
    for old, new in GROUP_B_COMMENT_RENAMES:
        count = content.count(old)
        if count > 0:
            changes.append((old, new, count))
            content = content.replace(old, new)

    # Also handle standalone "lifespan" in SQL strings in GameConfigSqliteStore.cpp
    # We need to rename the SQL column "lifespan" -> "durability" but only inside SQL string literals
    # This is tricky — let's use a targeted approach: replace '"lifespan"' with '"durability"'
    # and '\"lifespan\"' patterns
    for sql_old, sql_new in [
        ('"lifespan"', '"durability"'),
        ("'lifespan'", "'durability'"),
    ]:
        count = content.count(sql_old)
        if count > 0:
            changes.append((sql_old, sql_new, count))
            content = content.replace(sql_old, sql_new)

    # Group C: Type compatibility fixes (plain string)
    for old, new in GROUP_C_FIXES:
        count = content.count(old)
        if count > 0:
            changes.append((old, new, count))
            content = content.replace(old, new)

    # Group C: Regex fixes (enum comparisons)
    for pattern, replacement in GROUP_C_REGEX_FIXES:
        matches = list(re.finditer(pattern, content))
        if matches:
            changes.append((pattern if isinstance(pattern, str) else str(pattern),
                          "static_cast<uint8_t>(...)", len(matches)))
            content = re.sub(pattern, replacement, content)

    return content, changes


def run_dry_run(files):
    """Preview all changes without modifying files."""
    log(f"=== DRY RUN: {SCRIPT_NAME} ===")
    log(f"Files to scan: {len(files)}")
    log("")

    total_changes = 0
    files_changed = 0
    entry_counts = defaultdict(int)  # old_name -> total matches

    for filepath in files:
        content = read_file(filepath)
        new_content, changes = apply_renames(content)

        if changes:
            files_changed += 1
            rel = os.path.relpath(filepath, ROOT_DIR)
            log(f"\n--- {rel} ---")
            for old, new, count in changes:
                log(f"  {old} -> {new} ({count}x)")
                entry_counts[old] += count
                total_changes += count

    log(f"\n=== SUMMARY ===")
    log(f"Files scanned: {len(files)}")
    log(f"Files changed: {files_changed}")
    log(f"Total replacements: {total_changes}")

    # Report unused entries
    log(f"\n=== UNUSED ENTRIES ===")
    all_entries = (
        [(old, new) for old, new in GROUP_A_RENAMES] +
        [(old, new) for old, new in GROUP_B_STRING_RENAMES] +
        [(old, new) for old, new in GROUP_C_FIXES]
    )
    unused = [old for old, new in all_entries if entry_counts.get(old, 0) == 0]
    if unused:
        for u in unused:
            log(f"  WARNING: No matches for: {u}")
    else:
        log("  All entries matched at least once.")

    # C++ keyword check
    log(f"\n=== C++ KEYWORD CHECK ===")
    cpp_keywords = {"auto", "break", "case", "class", "const", "continue", "default",
                    "delete", "do", "double", "else", "enum", "extern", "false", "float",
                    "for", "goto", "if", "inline", "int", "long", "namespace", "new",
                    "nullptr", "operator", "private", "protected", "public", "return",
                    "short", "signed", "sizeof", "static", "struct", "switch", "template",
                    "this", "throw", "true", "try", "typedef", "unsigned", "using",
                    "virtual", "void", "volatile", "while", "bool", "char", "uint8_t",
                    "uint16_t", "uint32_t", "uint64_t", "int8_t", "int16_t", "int32_t",
                    "int64_t", "size_t", "constexpr", "override", "final", "noexcept"}
    new_names = [new for _, new in GROUP_A_RENAMES] + [new for _, new in GROUP_B_STRING_RENAMES]
    conflicts = [n for n in new_names if n in cpp_keywords]
    if conflicts:
        for c in conflicts:
            log(f"  ERROR: Rename target is C++ keyword: {c}")
    else:
        log("  No C++ keyword conflicts.")

    write_log(f"{SCRIPT_NAME}_dry_run.log")


def run_verify(files):
    """Scan for collisions in Shared/SFMLEngine."""
    log(f"=== VERIFY: {SCRIPT_NAME} ===")

    scan_dirs = [
        os.path.join(ROOT_DIR, "Sources", "Dependencies", "Shared"),
        os.path.join(ROOT_DIR, "Sources", "SFMLEngine"),
    ]

    # Check for old names still present in excluded files or other locations
    all_old_names = (
        [old for old, _ in GROUP_A_RENAMES] +
        [old for old, _ in GROUP_B_STRING_RENAMES]
    )

    log(f"\n=== SHARED/SFMLENGINE COLLISION SCAN ===")
    collision_count = 0
    for scan_dir in scan_dirs:
        if not os.path.isdir(scan_dir):
            continue
        for root, dirs, filenames in os.walk(scan_dir):
            for fn in filenames:
                ext = os.path.splitext(fn)[1].lower()
                if ext not in CPP_EXTENSIONS:
                    continue
                full_path = os.path.join(root, fn)
                rel_path = os.path.relpath(full_path, ROOT_DIR)
                if os.path.normpath(rel_path) in EXCLUDED_FILES:
                    continue
                content = read_file(full_path)
                for old_name in all_old_names:
                    if old_name in content:
                        lines = content.split('\n')
                        for i, line in enumerate(lines, 1):
                            if old_name in line:
                                log(f"  COLLISION: {rel_path}:{i}: {old_name}")
                                log(f"    {line.strip()}")
                                collision_count += 1

    if collision_count == 0:
        log("  No collisions found in Shared/SFMLEngine (excluding Phase 1 files).")
    else:
        log(f"  Found {collision_count} collisions — these will be renamed by the script.")

    # Duplicate target check
    log(f"\n=== DUPLICATE TARGET CHECK ===")
    new_names = defaultdict(list)
    for old, new in GROUP_A_RENAMES:
        new_names[new].append(old)
    for old, new in GROUP_B_STRING_RENAMES:
        new_names[new].append(old)
    dupes = {new: olds for new, olds in new_names.items() if len(olds) > 1}
    if dupes:
        for new, olds in dupes.items():
            log(f"  WARNING: Multiple old names map to '{new}': {olds}")
    else:
        log("  No duplicate targets.")

    write_log(f"{SCRIPT_NAME}_verify.log")


def run_apply(files):
    """Apply all renames."""
    log(f"=== APPLY: {SCRIPT_NAME} ===")

    # Guard all files that will be modified
    files_to_modify = []
    for filepath in files:
        content = read_file(filepath)
        new_content, changes = apply_renames(content)
        if changes:
            files_to_modify.append(filepath)

    if not files_to_modify:
        log("No files need modification.")
        return

    log(f"Guarding {len(files_to_modify)} files...")
    # Convert to relative paths for bak.py
    rel_files = [os.path.relpath(f, ROOT_DIR) for f in files_to_modify]
    # Batch in groups of 10 to avoid command-line length limits on Windows
    batch_size = 10
    for i in range(0, len(rel_files), batch_size):
        batch = rel_files[i:i+batch_size]
        guard_cmd = [sys.executable, os.path.join(ROOT_DIR, "bak.py"), "guard"] + batch
        result = subprocess.run(guard_cmd, capture_output=True, text=True, cwd=ROOT_DIR)
        if result.returncode != 0:
            log(f"ERROR: bak.py guard failed (batch {i//batch_size}): {result.stderr}")
            return
        if result.stdout.strip():
            log(result.stdout.strip())

    # Apply changes
    total_changes = 0
    files_changed = 0
    for filepath in files_to_modify:
        content = read_file(filepath)
        new_content, changes = apply_renames(content)
        if changes:
            write_file(filepath, new_content)
            files_changed += 1
            rel = os.path.relpath(filepath, ROOT_DIR)
            change_count = sum(c for _, _, c in changes)
            total_changes += change_count
            log(f"  {rel}: {change_count} replacements")

    log(f"\n=== COMPLETE ===")
    log(f"Files modified: {files_changed}")
    log(f"Total replacements: {total_changes}")


def main():
    parser = argparse.ArgumentParser(description="CItem composition refactor — bulk field renames")
    parser.add_argument("--dry-run", action="store_true", help="Preview changes without modifying files")
    parser.add_argument("--verify", action="store_true", help="Scan for collisions in Shared/SFMLEngine")
    parser.add_argument("--skip-verify", action="store_true", help="Apply without requiring prior --verify")
    args = parser.parse_args()

    files = collect_files()

    if args.dry_run:
        run_dry_run(files)
    elif args.verify:
        run_verify(files)
    else:
        run_apply(files)


if __name__ == "__main__":
    main()
