#!/usr/bin/env python3
"""
Mode 2 bulk migration script: Remove CGame::m_dialog_box_manager raw pointer shortcut.

Replaces all occurrences of:
    m_game->m_dialog_box_manager->
with:
    m_game->get_dialog_box_manager().

This is a mechanical replacement — the accessor returns a reference, so -> becomes .

Scope: Sources/Client/ only (no Shared, no SFMLEngine, no Server).
"""

import argparse
import os
import re
import subprocess
import sys

REPO_ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), ".."))
TARGET_DIR = os.path.join(REPO_ROOT, "Sources", "Client")
OUTPUT_DIR = os.path.join(REPO_ROOT, "Scripts", "output")
SCRIPT_NAME = "migrate_dialog_box_manager"

OLD_PATTERN = "m_game->m_dialog_box_manager->"
NEW_PATTERN = "m_game->get_dialog_box_manager()."

# Shared/SFMLEngine directories for collision scanning
SHARED_DIR = os.path.join(REPO_ROOT, "Sources", "Dependencies", "Shared")
SFML_DIR = os.path.join(REPO_ROOT, "Sources", "SFMLEngine")


def find_cpp_files(directory):
    """Find all .cpp and .h files in directory."""
    results = []
    for root, dirs, files in os.walk(directory):
        for f in files:
            if f.endswith((".cpp", ".h")):
                results.append(os.path.join(root, f))
    return sorted(results)


def run_dry_run():
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    log_path = os.path.join(OUTPUT_DIR, f"{SCRIPT_NAME}_dry_run.log")

    files = find_cpp_files(TARGET_DIR)
    total_replacements = 0
    files_changed = 0
    per_file = []

    for fpath in files:
        with open(fpath, "r", encoding="utf-8", errors="replace") as f:
            lines = f.readlines()

        file_matches = []
        for i, line in enumerate(lines, 1):
            count = line.count(OLD_PATTERN)
            if count > 0:
                file_matches.append((i, line.rstrip(), count))

        if file_matches:
            file_total = sum(m[2] for m in file_matches)
            total_replacements += file_total
            files_changed += 1
            rel = os.path.relpath(fpath, REPO_ROOT)
            per_file.append((rel, file_total, file_matches))

    with open(log_path, "w", encoding="utf-8") as log:
        log.write(f"=== Dry Run: {SCRIPT_NAME}.py ===\n")
        log.write(f"Total files scanned: {len(files)}\n")
        log.write(f"Files with matches: {files_changed}\n")
        log.write(f"Total replacements: {total_replacements}\n\n")
        log.write(f"Pattern: '{OLD_PATTERN}' -> '{NEW_PATTERN}'\n\n")
        log.write("--- Per-file details ---\n\n")

        for rel, count, matches in per_file:
            log.write(f"{rel}: {count} replacement(s)\n")
            for line_num, line_text, cnt in matches:
                log.write(f"  L{line_num}: {line_text.strip()}\n")
            log.write("\n")

        log.write(f"--- Summary ---\n")
        log.write(f"Would change: {total_replacements} replacements across {files_changed} files\n")

    print(f"Dry run complete. {total_replacements} replacements across {files_changed} files.")
    print(f"Full log: {log_path}")
    return 0


def run_verify():
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    log_path = os.path.join(OUTPUT_DIR, f"{SCRIPT_NAME}_verify.log")
    collisions = []

    # Check Shared for old pattern
    shared_files = find_cpp_files(SHARED_DIR) if os.path.isdir(SHARED_DIR) else []
    for fpath in shared_files:
        with open(fpath, "r", encoding="utf-8", errors="replace") as f:
            for i, line in enumerate(f, 1):
                if "m_dialog_box_manager" in line:
                    rel = os.path.relpath(fpath, REPO_ROOT)
                    collisions.append(("SHARED", rel, i, line.rstrip()))

    # Check SFMLEngine for old pattern
    sfml_files = find_cpp_files(SFML_DIR) if os.path.isdir(SFML_DIR) else []
    for fpath in sfml_files:
        with open(fpath, "r", encoding="utf-8", errors="replace") as f:
            for i, line in enumerate(f, 1):
                if "m_dialog_box_manager" in line:
                    rel = os.path.relpath(fpath, REPO_ROOT)
                    collisions.append(("SFMLENGINE", rel, i, line.rstrip()))

    # Check for out-of-scope matches (Server, CControls, etc.)
    for check_dir_name in ["Server", "CControls"]:
        check_dir = os.path.join(REPO_ROOT, "Sources", check_dir_name)
        if os.path.isdir(check_dir):
            for fpath in find_cpp_files(check_dir):
                with open(fpath, "r", encoding="utf-8", errors="replace") as f:
                    for i, line in enumerate(f, 1):
                        if "m_dialog_box_manager" in line:
                            rel = os.path.relpath(fpath, REPO_ROOT)
                            collisions.append(("OUT_OF_SCOPE", rel, i, line.rstrip()))

    # Check that the new accessor name doesn't collide with anything existing
    target_files = find_cpp_files(TARGET_DIR)
    existing_uses = []
    for fpath in target_files:
        with open(fpath, "r", encoding="utf-8", errors="replace") as f:
            for i, line in enumerate(f, 1):
                if "get_dialog_box_manager" in line:
                    rel = os.path.relpath(fpath, REPO_ROOT)
                    existing_uses.append((rel, i, line.rstrip()))

    with open(log_path, "w", encoding="utf-8") as log:
        log.write(f"=== Verify: {SCRIPT_NAME}.py ===\n\n")

        if collisions:
            log.write(f"--- Collisions Found ({len(collisions)}) ---\n\n")
            for kind, rel, line_num, line_text in collisions:
                log.write(f"[{kind}] {rel}:{line_num}\n")
                log.write(f"  {line_text}\n\n")
        else:
            log.write("--- No collisions in Shared/SFMLEngine/Server/CControls ---\n\n")

        if existing_uses:
            log.write(f"--- Existing 'get_dialog_box_manager' references ({len(existing_uses)}) ---\n")
            log.write("(These are expected — the new accessor declarations)\n\n")
            for rel, line_num, line_text in existing_uses:
                log.write(f"  {rel}:{line_num}: {line_text}\n")
            log.write("\n")

        log.write(f"--- Result ---\n")
        log.write(f"Collisions: {len(collisions)}\n")
        if not collisions:
            log.write("Safe to apply.\n")
        else:
            log.write("Review collisions before applying.\n")

    print(f"Verify complete. {len(collisions)} collision(s) found.")
    print(f"Full log: {log_path}")
    return 1 if collisions else 0


def run_apply(skip_verify=False):
    # Check for prior verify unless skipped
    verify_log = os.path.join(OUTPUT_DIR, f"{SCRIPT_NAME}_verify.log")
    if not skip_verify and not os.path.exists(verify_log):
        print("ERROR: Run --verify first, or use --skip-verify to bypass.")
        return 1

    # Guard all files that will be changed
    files = find_cpp_files(TARGET_DIR)
    files_to_guard = []
    for fpath in files:
        with open(fpath, "r", encoding="utf-8", errors="replace") as f:
            content = f.read()
        if OLD_PATTERN in content:
            files_to_guard.append(fpath)

    if not files_to_guard:
        print("No files contain the old pattern. Nothing to do.")
        return 0

    # Guard files via bak.py
    bak_py = os.path.join(REPO_ROOT, "bak.py")
    cmd = [sys.executable, bak_py, "guard"] + files_to_guard
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"bak.py guard failed:\n{result.stderr}")
        # If files already have checkpoints, that's OK for our purposes
        if "already" not in result.stderr.lower():
            return 1

    # Apply replacements
    total = 0
    changed = 0
    for fpath in files_to_guard:
        with open(fpath, "r", encoding="utf-8", errors="replace") as f:
            content = f.read()

        count = content.count(OLD_PATTERN)
        if count > 0:
            new_content = content.replace(OLD_PATTERN, NEW_PATTERN)
            with open(fpath, "w", encoding="utf-8") as f:
                f.write(new_content)
            total += count
            changed += 1
            rel = os.path.relpath(fpath, REPO_ROOT)
            print(f"  {rel}: {count} replacement(s)")

    print(f"\nApplied: {total} replacements across {changed} files.")
    return 0


def main():
    parser = argparse.ArgumentParser(
        description="Migrate m_game->m_dialog_box_manager-> to m_game->get_dialog_box_manager()."
    )
    parser.add_argument("--dry-run", action="store_true",
                        help="Preview changes without modifying files")
    parser.add_argument("--verify", action="store_true",
                        help="Scan for collisions in Shared/SFMLEngine")
    parser.add_argument("--skip-verify", action="store_true",
                        help="Apply without requiring prior --verify")
    args = parser.parse_args()

    if args.verify:
        return run_verify()
    elif args.dry_run:
        return run_dry_run()
    else:
        return run_apply(skip_verify=args.skip_verify)


if __name__ == "__main__":
    sys.exit(main())
