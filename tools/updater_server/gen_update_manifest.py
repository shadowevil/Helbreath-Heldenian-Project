#!/usr/bin/env python3
"""Generate update.manifest.json for the Helbreath auto-updater.

Scans a directory recursively, computes SHA256 for each file,
writes update.manifest.json.

Skips: settings.json, cache/, logs/, save/, updates/, *.old, *.update,
       update.manifest.json

Settings are saved to manifest_config.json — delete it to reconfigure.
"""

import hashlib
import json
import os
import sys


SKIP_DIRS = {"cache", "logs", "save", "updates", ".git", "__pycache__"}
SKIP_FILES = {"settings.json", "update.manifest.json", "gen_update_manifest.py"}
SKIP_EXTENSIONS = {".old", ".update", ".log", ".bak"}

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
CONFIG_PATH = os.path.join(SCRIPT_DIR, "manifest_config.json")


def sha256_file(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while True:
            chunk = f.read(1 << 20)  # 1MB chunks
            if not chunk:
                break
            h.update(chunk)
    return h.hexdigest()


def is_executable(rel_path: str) -> bool:
    _, ext = os.path.splitext(rel_path)
    if ext.lower() in (".exe", ".dll"):
        return True
    # Extensionless files on Linux are likely executables
    if not ext:
        return True
    return False


def detect_platform(rel_path: str) -> str:
    _, ext = os.path.splitext(rel_path)
    ext = ext.lower()
    if ext in (".exe", ".dll"):
        return "windows"
    if ext == ".so":
        return "linux"
    # Extensionless files are linux binaries (Game_x64_linux etc.)
    if not ext:
        return "linux"
    return "any"


def format_size(size_bytes: int) -> str:
    if size_bytes >= 1 << 30:
        return f"{size_bytes / (1 << 30):.1f} GB"
    if size_bytes >= 1 << 20:
        return f"{size_bytes / (1 << 20):.1f} MB"
    if size_bytes >= 1 << 10:
        return f"{size_bytes / (1 << 10):.1f} KB"
    return f"{size_bytes} B"


def collect_files(root_dir: str) -> list[str]:
    """First pass: collect all file paths (fast, no hashing)."""
    files = []
    root_dir = os.path.normpath(root_dir)

    for dirpath, dirnames, filenames in os.walk(root_dir):
        dirnames[:] = [
            d for d in dirnames
            if d.lower() not in SKIP_DIRS and not d.startswith(".")
        ]

        for filename in sorted(filenames):
            if filename in SKIP_FILES:
                continue
            _, ext = os.path.splitext(filename)
            if ext.lower() in SKIP_EXTENSIONS:
                continue
            if ".bak_" in filename:
                continue
            files.append(os.path.join(dirpath, filename))

    return files


def scan_directory(root_dir: str) -> list[dict]:
    root_dir = os.path.normpath(root_dir)
    files = collect_files(root_dir)
    total = len(files)

    if total == 0:
        return []

    total_bytes = sum(os.path.getsize(f) for f in files)
    print(f"  {total} files, {format_size(total_bytes)} total")

    entries = []
    hashed_bytes = 0

    for i, full_path in enumerate(files):
        rel_path = os.path.relpath(full_path, root_dir).replace("\\", "/")
        file_size = os.path.getsize(full_path)

        pct = (hashed_bytes / total_bytes * 100) if total_bytes else 0
        print(f"\r  [{pct:5.1f}%] ({i + 1}/{total}) {rel_path[:60]:<60}", end="", flush=True)

        file_hash = sha256_file(full_path)
        hashed_bytes += file_size

        entries.append({
            "path": rel_path,
            "sha256": file_hash,
            "size": file_size,
            "executable": is_executable(rel_path),
            "platform": detect_platform(rel_path),
        })

    print(f"\r  [100.0%] Done.{' ' * 60}")
    return entries


def prompt(text: str, default: str = "") -> str:
    if default:
        result = input(f"{text} [{default}]: ").strip()
        return result if result else default
    return input(f"{text}: ").strip()


def resolve_path(path: str) -> str:
    """Resolve a path relative to the script's directory, not CWD."""
    path = path.strip("\"'")
    if os.path.isabs(path):
        return os.path.normpath(path)
    return os.path.normpath(os.path.join(SCRIPT_DIR, path))


def make_relative(path: str) -> str:
    """Convert an absolute path to relative (from SCRIPT_DIR) if possible."""
    try:
        rel = os.path.relpath(path, SCRIPT_DIR)
        if os.path.isabs(rel):
            return path
        return rel
    except ValueError:
        return path


def load_config() -> dict:
    if os.path.isfile(CONFIG_PATH):
        try:
            with open(CONFIG_PATH, "r") as f:
                return json.load(f)
        except (json.JSONDecodeError, OSError):
            pass
    return {}


def save_config(cfg: dict) -> None:
    with open(CONFIG_PATH, "w", newline="\n") as f:
        json.dump(cfg, f, indent=4)


def main():
    print("=== Helbreath Update Manifest Generator ===")
    print(f"  Working from: {SCRIPT_DIR}\n")

    cfg = load_config()

    # Directory
    default_dir = cfg.get("directory", "../../Binaries/Game")
    directory = prompt("Game directory to scan", default_dir)
    while True:
        directory = resolve_path(directory)

        if not os.path.isdir(directory):
            print(f"  '{directory}' is not a valid directory.")
            directory = prompt("Game directory to scan")
            continue

        print(f"  Resolved: {directory}")
        ok = prompt("  Correct? (y/n)", "y")
        if ok.lower() in ("y", "yes"):
            break
        directory = prompt("Game directory to scan")

    # Version
    default_version = cfg.get("version", "0.1.0")
    version_str = prompt("Version (MAJOR.MINOR.PATCH)", default_version)
    while True:
        parts = version_str.split(".")
        if len(parts) == 3 and all(p.isdigit() for p in parts):
            break
        print("  Invalid format. Use MAJOR.MINOR.PATCH (e.g. 0.2.0)")
        version_str = prompt("Version (MAJOR.MINOR.PATCH)", default_version)

    # Save config for next run
    save_config({
        "directory": make_relative(directory),
        "version": version_str,
    })

    # Scan
    print(f"\nScanning {directory}...")
    entries = scan_directory(directory)

    exe_count = sum(1 for e in entries if e["executable"])
    data_count = len(entries) - exe_count
    win_count = sum(1 for e in entries if e["platform"] == "windows")
    linux_count = sum(1 for e in entries if e["platform"] == "linux")
    any_count = sum(1 for e in entries if e["platform"] == "any")
    print(f"  {exe_count} executable(s), {data_count} data file(s)")
    print(f"  Platform: {win_count} windows, {linux_count} linux, {any_count} cross-platform")

    # Write manifest directly into the scanned directory
    output_path = os.path.join(directory, "update.manifest.json")
    version_parts = version_str.split(".")
    manifest = {
        "version": {
            "major": int(version_parts[0]),
            "minor": int(version_parts[1]),
            "patch": int(version_parts[2]),
        },
        "files": entries,
    }

    with open(output_path, "w", newline="\n") as f:
        json.dump(manifest, f, indent=4)

    print(f"\nWrote manifest to {output_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
