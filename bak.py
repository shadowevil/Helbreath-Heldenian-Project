#!/usr/bin/env python3
"""Centralized .bak file manager with versioned checkpoints.

Uses .bak_<guid> files to track layered changes. Each `guard` creates
a new checkpoint; `revert <id>` restores from a specific checkpoint;
`commit` accepts all changes.

Commands:
    guard <files...>            Create a new versioned checkpoint
    status                      List all checkpoints with dirty/clean status
    revert <id>                 Revert all files from checkpoint <id>
    revert <id> <files...>      Revert specific files from checkpoint <id>
    commit                      Accept all changes (refuses if multiple checkpoints; use --force)
    commit <id>                 Accept changes for a specific checkpoint only
    commit --force              Accept all changes across all checkpoints

Global flags:
    --dry-run                   Preview what would happen without modifying files
"""

import argparse
import filecmp
import re
import shutil
import sys
import uuid
from collections import defaultdict
from datetime import datetime
from pathlib import Path

SOURCES = Path(r"Z:\Helbreath-3.82\Sources")
BAK_RE = re.compile(r"^(.+)\.bak_([a-f0-9]{8})$")


def find_all_baks() -> list[Path]:
    """Find all .bak_<guid> files under Sources/."""
    results = []
    for p in SOURCES.rglob("*.bak_*"):
        if p.is_file() and BAK_RE.match(p.name):
            results.append(p)
    return sorted(results)


def find_legacy_baks() -> list[Path]:
    """Find old-style .bak files (no guid) under Sources/."""
    results = []
    for p in SOURCES.rglob("*.bak"):
        if p.is_file() and p.suffix == ".bak" and not BAK_RE.match(p.name):
            results.append(p)
    return sorted(results)


def parse_bak(path: Path) -> tuple[Path, str] | None:
    """Parse .bak_<guid> path into (original_path, guid)."""
    m = BAK_RE.match(path.name)
    if m:
        return path.parent / m.group(1), m.group(2)
    return None


def get_checkpoints() -> list[tuple[str, float, list[Path]]]:
    """Return checkpoints as (guid, mtime, [bak_paths]) ordered oldest-first."""
    baks = find_all_baks()
    guid_files: dict[str, list[Path]] = defaultdict(list)
    guid_mtime: dict[str, float] = {}

    for bak in baks:
        parsed = parse_bak(bak)
        if parsed:
            _, guid = parsed
            guid_files[guid].append(bak)
            mt = bak.stat().st_mtime
            if guid not in guid_mtime or mt > guid_mtime[guid]:
                guid_mtime[guid] = mt

    return [
        (g, guid_mtime[g], guid_files[g])
        for g in sorted(guid_files, key=lambda g: guid_mtime[g])
    ]


def rel(path: Path) -> str:
    """Display path relative to Sources/."""
    try:
        return str(path.relative_to(SOURCES))
    except ValueError:
        return str(path)


def resolve_path(raw: str) -> Path:
    """Resolve user-provided path to absolute."""
    p = Path(raw)
    if p.is_absolute():
        return p
    cwd_path = Path.cwd() / p
    if cwd_path.exists():
        return cwd_path.resolve()
    src_path = SOURCES / p
    if src_path.exists():
        return src_path.resolve()
    return cwd_path.resolve()


def file_status(bak: Path, orig: Path) -> str:
    """Return dirty/clean/MISSING status for a bak-original pair."""
    if not orig.exists():
        return "MISSING"
    elif filecmp.cmp(str(bak), str(orig), shallow=False):
        return "clean"
    else:
        return "dirty"


def get_other_checkpoints_for_file(orig: Path, exclude_guid: str) -> list[str]:
    """Find other checkpoint GUIDs that have a .bak for the same original file."""
    other_guids = []
    for p in orig.parent.iterdir():
        if p.is_file():
            parsed = parse_bak(p)
            if parsed and parsed[0] == orig and parsed[1] != exclude_guid:
                other_guids.append(parsed[1])
    return sorted(other_guids)


def get_existing_checkpoints_for_file(filepath: Path) -> list[str]:
    """Find all checkpoint GUIDs that have a .bak for the given file."""
    guids = []
    for p in filepath.parent.iterdir():
        if p.is_file():
            parsed = parse_bak(p)
            if parsed and parsed[0] == filepath:
                guids.append(parsed[1])
    return sorted(guids)


def generate_unique_guid() -> str:
    """Generate a GUID that doesn't collide with existing checkpoints."""
    existing_guids = {guid for guid, _, _ in get_checkpoints()}
    for _ in range(100):
        candidate = uuid.uuid4().hex[:8]
        if candidate not in existing_guids:
            return candidate
    # Practically impossible, but be defensive
    raise RuntimeError("Failed to generate unique GUID after 100 attempts")


def find_checkpoint(checkpoint_id: str) -> tuple[str, float, list[Path]] | None:
    """Find a checkpoint by ID (exact or prefix match)."""
    checkpoints = get_checkpoints()
    # Exact match first
    for guid, mtime, bak_paths in checkpoints:
        if guid == checkpoint_id:
            return guid, mtime, bak_paths
    # Prefix match
    matches = [(g, m, b) for g, m, b in checkpoints if g.startswith(checkpoint_id)]
    if len(matches) == 1:
        return matches[0]
    if len(matches) > 1:
        print(f"ERROR: Ambiguous checkpoint ID '{checkpoint_id}'. Matches:")
        for g, _, _ in matches:
            print(f"  [{g}]")
        return None
    return None


def print_available_checkpoints():
    """Print list of available checkpoint IDs for the user."""
    checkpoints = get_checkpoints()
    if not checkpoints:
        print("No checkpoints found.")
    else:
        print("Available checkpoints:")
        for guid, mtime, bak_paths in checkpoints:
            ts = datetime.fromtimestamp(mtime).strftime("%Y-%m-%d %H:%M:%S")
            print(f"  [{guid}]  {ts}  ({len(bak_paths)} file(s))")


# ── guard ──────────────────────────────────────────────────────────

def cmd_guard(args: argparse.Namespace) -> int:
    """Create a new versioned checkpoint for the listed files."""
    dry_run = args.dry_run
    force = args.force
    files = [resolve_path(f) for f in args.files]

    for f in files:
        if not f.exists():
            print(f"ERROR: {f} does not exist.")
            return 1
        try:
            f.resolve().relative_to(SOURCES.resolve())
        except ValueError:
            print(f"ERROR: {f} is outside Sources/. bak.py only guards files within Sources/.")
            return 1

    # Check for files with existing checkpoints
    warnings = []
    for f in files:
        existing = get_existing_checkpoints_for_file(f)
        if existing:
            for g in existing:
                warnings.append((f, g))

    if warnings and not force and not dry_run:
        print("WARNING: The following files already have checkpoints:")
        for f, g in warnings:
            print(f"  {rel(f)} — already in checkpoint [{g}]")
        print("\nGuarding will create a new layer. The file may be in a modified state.")
        print("Use --force to proceed anyway.")
        return 1

    guid = generate_unique_guid()

    if dry_run:
        print(f"[DRY RUN] Would create checkpoint [{guid}] with {len(files)} file(s):")
        for f in files:
            existing = get_existing_checkpoints_for_file(f)
            suffix = ""
            if existing:
                suffix = f" — WARNING: already has checkpoint [{', '.join(existing)}]"
            print(f"  {rel(f)}{suffix}")
        return 0

    if warnings and force:
        print("WARNING: Creating new layer on files with existing checkpoints (--force).")

    for f in files:
        bak = f.parent / f"{f.name}.bak_{guid}"
        shutil.copy2(str(f), str(bak))
        print(f"  Guarded: {rel(f)}")

    print(f"\nCheckpoint [{guid}] created ({len(files)} file(s)).")
    return 0


# ── status ─────────────────────────────────────────────────────────

def cmd_status(args: argparse.Namespace) -> int:
    """List all checkpoints with dirty/clean status. Exit 1 if any exist."""
    checkpoints = get_checkpoints()
    legacy = find_legacy_baks()

    if not checkpoints and not legacy:
        print("No checkpoints in Sources/")
        return 0

    if checkpoints:
        total = sum(len(files) for _, _, files in checkpoints)
        print(f"{len(checkpoints)} checkpoint(s), {total} file(s) in Sources/:\n")

        for i, (guid, mtime, bak_paths) in enumerate(checkpoints):
            ts = datetime.fromtimestamp(mtime).strftime("%Y-%m-%d %H:%M:%S")
            label = ""
            if len(checkpoints) > 1:
                if i == 0:
                    label = "  <- original"
                elif i == len(checkpoints) - 1:
                    label = "  <- most recent"
            print(f"  [{guid}]  {ts}  ({len(bak_paths)} file(s)){label}")

            for bak in sorted(bak_paths):
                parsed = parse_bak(bak)
                if parsed:
                    orig, _ = parsed
                    status = file_status(bak, orig)
                    print(f"    [{status:^8s}]  {rel(orig)}")
            print()

    if legacy:
        print(f"WARNING: {len(legacy)} legacy .bak file(s) (no guid):")
        for p in legacy:
            print(f"  {rel(p)}")
        print("Run 'bak.py commit' to clean these up.\n")

    return 1  # .bak files exist


# ── revert ─────────────────────────────────────────────────────────

def cmd_revert(args: argparse.Namespace) -> int:
    """Revert files from a specific checkpoint."""
    dry_run = args.dry_run
    force = args.force

    if not args.checkpoint_id:
        print("ERROR: revert requires a checkpoint ID.\n")
        print_available_checkpoints()
        return 1

    checkpoint = find_checkpoint(args.checkpoint_id)
    if not checkpoint:
        if not get_checkpoints():
            print("Nothing to revert — no checkpoints found.")
        else:
            print(f"ERROR: Checkpoint '{args.checkpoint_id}' not found.\n")
            print_available_checkpoints()
        return 1

    guid, _, bak_paths = checkpoint

    # If specific files were requested, filter to those
    if args.files:
        requested = [resolve_path(f) for f in args.files]
        # Build map of orig -> bak for this checkpoint
        orig_to_bak = {}
        for bak in bak_paths:
            parsed = parse_bak(bak)
            if parsed:
                orig_to_bak[parsed[0]] = bak

        filtered_baks = []
        for req in requested:
            if req not in orig_to_bak:
                print(f"ERROR: {rel(req)} is not part of checkpoint [{guid}].")
                print(f"Files in checkpoint [{guid}]:")
                for orig in sorted(orig_to_bak, key=lambda p: rel(p)):
                    print(f"  {rel(orig)}")
                return 1
            filtered_baks.append(orig_to_bak[req])
        bak_paths = filtered_baks

    # Check for multi-layer warnings
    multi_layer_warnings = []
    for bak in bak_paths:
        parsed = parse_bak(bak)
        if parsed:
            orig, _ = parsed
            other_guids = get_other_checkpoints_for_file(orig, guid)
            if other_guids:
                multi_layer_warnings.append((orig, other_guids))

    if multi_layer_warnings and not force and not dry_run:
        print("WARNING: The following files have other checkpoint layers:")
        for orig, other_guids in multi_layer_warnings:
            guids_str = ", ".join(f"[{g}]" for g in other_guids)
            print(f"  {rel(orig)} — also in checkpoint {guids_str}")
        print("\nReverting may conflict with those checkpoints. Use --force to proceed.")
        return 1

    if dry_run:
        file_label = f" ({len(args.files)} specific file(s))" if args.files else ""
        print(f"[DRY RUN] Would revert checkpoint [{guid}]{file_label}:")
        for bak in sorted(bak_paths):
            parsed = parse_bak(bak)
            if parsed:
                orig, _ = parsed
                status = file_status(bak, orig)
                other_guids = get_other_checkpoints_for_file(orig, guid)
                suffix = ""
                if other_guids:
                    guids_str = ", ".join(f"[{g}]" for g in other_guids)
                    suffix = f" — WARNING: also in checkpoint {guids_str}"
                print(f"  {rel(orig)}   [{status}]{suffix}")
        return 0

    if multi_layer_warnings and force:
        print("WARNING: Reverting files with other checkpoint layers (--force).")

    print(f"Reverting checkpoint [{guid}]...")

    for bak in sorted(bak_paths):
        parsed = parse_bak(bak)
        if parsed:
            orig, _ = parsed
            shutil.copy2(str(bak), str(orig))
            bak.unlink()
            print(f"  Restored: {rel(orig)}")

    # Check remaining checkpoints
    remaining = len(get_checkpoints())
    print(f"\n{len(bak_paths)} file(s) restored. {remaining} checkpoint(s) remaining.")
    return 0


# ── commit ─────────────────────────────────────────────────────────

def cmd_commit(args: argparse.Namespace) -> int:
    """Delete .bak* files (accept current state).

    If multiple checkpoint IDs exist and no specific ID is given,
    refuses to proceed and reports all checkpoints so the caller
    can decide which to commit.
    """
    dry_run = args.dry_run
    force = args.force
    specific_id = args.checkpoint_id
    baks = find_all_baks()
    legacy = find_legacy_baks()

    if not baks and not legacy:
        print("Nothing to commit — no .bak files found.")
        return 0

    checkpoints = get_checkpoints()

    # If a specific checkpoint ID was given, scope to just that checkpoint
    if specific_id:
        checkpoint = find_checkpoint(specific_id)
        if not checkpoint:
            print(f"ERROR: Checkpoint '{specific_id}' not found.\n")
            print_available_checkpoints()
            return 1
        guid, mtime, bak_paths = checkpoint
        all_files = bak_paths

        if dry_run:
            ts = datetime.fromtimestamp(mtime).strftime("%Y-%m-%d %H:%M:%S")
            print(f"[DRY RUN] Would commit checkpoint [{guid}]  {ts}  ({len(bak_paths)} file(s)):")
            for bak in sorted(bak_paths):
                parsed = parse_bak(bak)
                if parsed:
                    orig, _ = parsed
                    status = file_status(bak, orig)
                    print(f"    {rel(orig)}   [{status}]")
            return 0

        for f in all_files:
            f.unlink()
        print(f"Committed checkpoint [{guid}] — deleted {len(all_files)} file(s). Changes accepted.")
        remaining = get_checkpoints()
        if remaining:
            print(f"\n{len(remaining)} other checkpoint(s) still active:")
            for g, mt, bp in remaining:
                ts = datetime.fromtimestamp(mt).strftime("%Y-%m-%d %H:%M:%S")
                print(f"  [{g}]  {ts}  ({len(bp)} file(s))")
        return 0

    # No specific ID — if multiple checkpoints exist, refuse unless --force
    if len(checkpoints) > 1 and not force:
        print(f"ERROR: {len(checkpoints)} checkpoints exist. Specify which to commit, or use --force to commit all.\n")
        for guid, mtime, bak_paths in checkpoints:
            ts = datetime.fromtimestamp(mtime).strftime("%Y-%m-%d %H:%M:%S")
            print(f"  [{guid}]  {ts}  ({len(bak_paths)} file(s))")
            for bak in sorted(bak_paths):
                parsed = parse_bak(bak)
                if parsed:
                    orig, _ = parsed
                    print(f"    {rel(orig)}")
        print(f"\nUsage:  bak.py commit <id>        Commit a specific checkpoint")
        print(f"        bak.py commit --force     Commit all checkpoints")
        return 1

    # Single checkpoint or --force: commit everything
    all_files = baks + legacy

    if dry_run:
        print(f"[DRY RUN] Would delete {len(all_files)} checkpoint file(s) "
              f"across {len(checkpoints)} checkpoint(s):")
        for guid, mtime, bak_paths in checkpoints:
            ts = datetime.fromtimestamp(mtime).strftime("%Y-%m-%d %H:%M:%S")
            print(f"\n  [{guid}]  {ts}  ({len(bak_paths)} file(s))")
            for bak in sorted(bak_paths):
                parsed = parse_bak(bak)
                if parsed:
                    orig, _ = parsed
                    status = file_status(bak, orig)
                    print(f"    {rel(orig)}   [{status}]")
        if legacy:
            print(f"\n  {len(legacy)} legacy .bak file(s):")
            for p in legacy:
                print(f"    {rel(p)}")
        return 0

    for f in all_files:
        f.unlink()

    parts = []
    if baks:
        parts.append(f"{len(baks)} checkpoint file(s) across "
                      f"{len(checkpoints)} checkpoint(s)")
    if legacy:
        parts.append(f"{len(legacy)} legacy .bak file(s)")
    print(f"Deleted {', '.join(parts)}. Changes accepted.")
    return 0


# ── main ───────────────────────────────────────────────────────────

def main() -> int:
    parser = argparse.ArgumentParser(
        prog="bak.py",
        description="Versioned .bak file manager for Sources/",
    )
    parser.add_argument("--dry-run", action="store_true",
                        help="Preview what would happen without modifying files")

    sub = parser.add_subparsers(dest="command", required=True)

    p_guard = sub.add_parser("guard", help="Create a new versioned checkpoint")
    p_guard.add_argument("files", nargs="+", help="Files to checkpoint")
    p_guard.add_argument("--force", action="store_true",
                         help="Proceed even if files already have checkpoints")

    sub.add_parser("status", help="List all checkpoints with status")

    p_revert = sub.add_parser("revert", help="Revert files from a specific checkpoint")
    p_revert.add_argument("checkpoint_id", nargs="?", default=None,
                          help="Checkpoint ID to revert")
    p_revert.add_argument("files", nargs="*", help="Specific files to revert (optional)")
    p_revert.add_argument("--force", action="store_true",
                          help="Proceed even if files have other checkpoint layers")

    p_commit = sub.add_parser("commit", help="Delete .bak* files (accept changes)")
    p_commit.add_argument("checkpoint_id", nargs="?", default=None,
                          help="Specific checkpoint ID to commit (optional)")
    p_commit.add_argument("--force", action="store_true",
                          help="Commit all checkpoints even when multiple exist")

    args = parser.parse_args()

    handlers = {
        "guard": cmd_guard,
        "status": cmd_status,
        "revert": cmd_revert,
        "commit": cmd_commit,
    }
    return handlers[args.command](args)


if __name__ == "__main__":
    sys.exit(main())
