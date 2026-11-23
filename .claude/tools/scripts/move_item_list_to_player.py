"""
Move m_item_list from CGame to CPlayer.

Touches 22 client files with pattern:
  - bare m_item_list[ → m_player->m_item_list[  (CGame member functions)
  - m_game->m_item_list → m_game->m_player->m_item_list  (dialog/manager files)
  - game->m_item_list → game->m_player->m_item_list  (NetworkMessage handlers)

Also handles .begin()/.end() and other dot-access patterns.

Excludes:
  - ShopManager.h / ShopManager.cpp (has its OWN m_item_list member)
  - Server/ files (different codebase)
  - Game.h (declaration removal done separately)
  - Player.h (declaration addition done separately)

Usage:
  python Scripts/move_item_list_to_player.py --dry-run
  python Scripts/move_item_list_to_player.py --verify
  python Scripts/move_item_list_to_player.py
"""

import os
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CLIENT_DIR = ROOT / "Sources" / "Client"
OUTPUT_DIR = ROOT / "Scripts" / "output"

EXCLUDE_FILES = {
    "ShopManager.h",
    "ShopManager.cpp",
    "Game.h",
    "Player.h",
}

# All client files known to contain m_item_list
TARGET_FILES = [
    "Game.cpp",
    "Game.Hotkeys.cpp",
    "InventoryManager.cpp",
    "DialogBox_Bank.cpp",
    "Screen_OnGame.cpp",
    "NetworkMessages_Items.cpp",
    "DialogBox_SellList.cpp",
    "DialogBox_SellOrRepair.cpp",
    "DialogBox_NpcActionQuery.cpp",
    "DialogBox_Character.cpp",
    "DialogBox_Slates.cpp",
    "DialogBox_Manufacture.cpp",
    "DialogBox_RepairAll.cpp",
    "DialogBox_ItemUpgrade.cpp",
    "DialogBox_ItemDropAmount.cpp",
    "DialogBox_ItemDrop.cpp",
    "DialogBox_Inventory.cpp",
    "DialogBox_Exchange.cpp",
    "DialogBox_Magic.cpp",
    "BuildItemManager.cpp",
    "MagicCastingSystem.cpp",
]


def transform(content: str) -> str:
    """Apply the three replacement passes in order."""
    # Pass 1: m_game->m_item_list → m_game->m_player->m_item_list
    content = content.replace("m_game->m_item_list", "m_game->m_player->m_item_list")

    # Pass 2: game->m_item_list → game->m_player->m_item_list
    # Must run AFTER pass 1 so it doesn't match inside m_game-> patterns
    # Use regex with negative lookbehind to avoid matching m_game-> (already handled)
    content = re.sub(r"(?<!m_)game->m_item_list", "game->m_player->m_item_list", content)

    # Pass 3: bare m_item_list → m_player->m_item_list
    # Only matches m_item_list NOT preceded by -> (bare member access in CGame methods)
    content = re.sub(r"(?<!->)m_item_list", "m_player->m_item_list", content)

    return content


def dry_run():
    """Preview all changes without modifying files."""
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    log_path = OUTPUT_DIR / "move_item_list_dry_run.log"
    total_changes = 0

    with open(log_path, "w", encoding="utf-8") as log:
        for fname in TARGET_FILES:
            fpath = CLIENT_DIR / fname
            if not fpath.exists():
                log.write(f"WARNING: {fname} not found\n")
                print(f"  WARNING: {fname} not found")
                continue

            original = fpath.read_text(encoding="utf-8")
            transformed = transform(original)

            if original == transformed:
                log.write(f"NO CHANGES: {fname}\n")
                print(f"  NO CHANGES: {fname}")
                continue

            # Count changes per line
            orig_lines = original.splitlines()
            new_lines = transformed.splitlines()
            file_changes = 0

            log.write(f"\n{'='*80}\n")
            log.write(f"FILE: {fname}\n")
            log.write(f"{'='*80}\n")

            for i, (ol, nl) in enumerate(zip(orig_lines, new_lines), 1):
                if ol != nl:
                    file_changes += 1
                    log.write(f"  Line {i}:\n")
                    log.write(f"    - {ol.strip()}\n")
                    log.write(f"    + {nl.strip()}\n")

            total_changes += file_changes
            print(f"  {fname}: {file_changes} line(s) changed")
            log.write(f"\nTotal lines changed in {fname}: {file_changes}\n")

        log.write(f"\n{'='*80}\n")
        log.write(f"TOTAL: {total_changes} line(s) across {len(TARGET_FILES)} files\n")

    print(f"\nTotal: {total_changes} line(s) changed")
    print(f"Full log: {log_path}")


def verify():
    """Check for potential issues."""
    issues = []

    # 1. Check that excluded files are not accidentally targeted
    for fname in TARGET_FILES:
        if fname in EXCLUDE_FILES:
            issues.append(f"COLLISION: {fname} is in both TARGET and EXCLUDE lists")

    # 2. Check that all target files exist
    for fname in TARGET_FILES:
        fpath = CLIENT_DIR / fname
        if not fpath.exists():
            issues.append(f"MISSING: {fname}")

    # 3. Check ShopManager files are NOT in target list
    for excluded in EXCLUDE_FILES:
        if excluded in TARGET_FILES:
            issues.append(f"SAFETY: {excluded} should not be in TARGET_FILES")

    # 4. After transformation, check no double-replacement
    for fname in TARGET_FILES:
        fpath = CLIENT_DIR / fname
        if not fpath.exists():
            continue
        content = fpath.read_text(encoding="utf-8")
        transformed = transform(content)
        if "m_player->m_player->m_item_list" in transformed:
            issues.append(f"DOUBLE REPLACE: {fname} has m_player->m_player->m_item_list")

    # 5. Check that ShopManager files still have their own m_item_list untouched
    for fname in ["ShopManager.h", "ShopManager.cpp"]:
        fpath = CLIENT_DIR / fname
        if fpath.exists():
            content = fpath.read_text(encoding="utf-8")
            if "m_item_list" not in content:
                issues.append(f"UNEXPECTED: {fname} has no m_item_list (expected its own)")

    # 6. Scan for any Client files with m_item_list that are NOT in our target list
    for fpath in CLIENT_DIR.glob("*.cpp"):
        if fpath.name in EXCLUDE_FILES or fpath.name in TARGET_FILES:
            continue
        content = fpath.read_text(encoding="utf-8")
        # Check for CGame's m_item_list access (not ShopManager's)
        if re.search(r"(m_game->m_item_list|game->m_item_list|\bm_item_list\[)", content):
            issues.append(f"UNCOVERED: {fpath.name} has m_item_list but is not in TARGET_FILES")
    for fpath in CLIENT_DIR.glob("*.h"):
        if fpath.name in EXCLUDE_FILES or fpath.name in TARGET_FILES:
            continue
        content = fpath.read_text(encoding="utf-8")
        if re.search(r"(m_game->m_item_list|game->m_item_list)", content):
            issues.append(f"UNCOVERED: {fpath.name} has m_item_list but is not in TARGET_FILES")

    if issues:
        print("VERIFICATION FAILED:")
        for issue in issues:
            print(f"  {issue}")
        return False
    else:
        print("VERIFICATION PASSED: No issues found")
        return True


def apply():
    """Apply changes to all files."""
    total_changes = 0
    for fname in TARGET_FILES:
        fpath = CLIENT_DIR / fname
        if not fpath.exists():
            print(f"  WARNING: {fname} not found, skipping")
            continue

        original = fpath.read_text(encoding="utf-8")
        transformed = transform(original)

        if original == transformed:
            print(f"  NO CHANGES: {fname}")
            continue

        file_changes = sum(1 for a, b in zip(original.splitlines(), transformed.splitlines()) if a != b)
        fpath.write_text(transformed, encoding="utf-8")
        total_changes += file_changes
        print(f"  APPLIED: {fname} ({file_changes} lines)")

    print(f"\nTotal: {total_changes} line(s) changed across files")


def main():
    if "--dry-run" in sys.argv:
        print("DRY RUN — previewing changes:")
        dry_run()
    elif "--verify" in sys.argv:
        print("VERIFYING — checking for issues:")
        ok = verify()
        sys.exit(0 if ok else 1)
    else:
        print("APPLYING changes:")
        apply()


if __name__ == "__main__":
    main()
