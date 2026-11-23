#!/usr/bin/env python3
"""Grep search tool with two output modes.

Brief mode (-b): compact one-line-per-match to stdout.
Detail mode (default): full context blocks to a log file in .claude/scratch/tools/grep/.

Usage:
    python grep.py "pattern" -b                   # brief to stdout
    python grep.py "pattern"                      # detailed to log file
    python grep.py "pattern" --path sources/client --ext .cpp
    python grep.py "CGame::Process" -C 8 -o custom.log
    python grep.py "DEF_MAXCLIENTS" -F            # fixed string, not regex
    python grep.py "todo|fixme|hack" -i -b        # case-insensitive, brief
"""

import argparse
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
SOURCES = REPO / "sources"
OUTPUT_DIR = REPO / ".claude" / "scratch" / "tools" / "grep"
MAX_OUTPUT_DIR_MB = 10


def ensure_output_dir() -> None:
    """Create output dir if needed. Clear it if over size limit."""
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    total = sum(f.stat().st_size for f in OUTPUT_DIR.iterdir() if f.is_file())
    if total > MAX_OUTPUT_DIR_MB * 1024 * 1024:
        cleared = 0
        for f in sorted(OUTPUT_DIR.iterdir(), key=lambda p: p.stat().st_mtime):
            if f.is_file():
                cleared += f.stat().st_size
                f.unlink()
        print(f"[output/] Cleared {cleared // 1024}KB (was over {MAX_OUTPUT_DIR_MB}MB limit)")


def find_scope(lines: list[str], match_idx: int) -> str | None:
    """Scan backwards for the enclosing class/method scope."""
    method_re = re.compile(r"\b([A-Z]\w+)::(\w+)\s*\(")
    class_re = re.compile(r"\b(?:class|struct)\s+([A-Z]\w+)")

    for i in range(match_idx - 1, max(match_idx - 200, -1), -1):
        m = method_re.search(lines[i])
        if m:
            return f"{m.group(1)}::{m.group(2)}"
        m = class_re.search(lines[i])
        if m:
            return m.group(1)
    return None


def search_file(
    filepath: Path, pattern: re.Pattern, context: int
) -> tuple[list[int], list[str]]:
    """Return (match_line_indices_0based, file_lines) for matches."""
    try:
        text = filepath.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return [], []

    lines = text.splitlines()
    matches = [i for i, line in enumerate(lines) if pattern.search(line)]
    return matches, lines


def merge_nearby(matches: list[int], context: int) -> list[list[int]]:
    """Group match indices whose context ranges overlap."""
    if not matches:
        return []

    groups: list[list[int]] = [[matches[0]]]
    for m in matches[1:]:
        if m - context <= groups[-1][-1] + context:
            groups[-1].append(m)
        else:
            groups.append([m])
    return groups


def format_block(
    rel_path: Path,
    lines: list[str],
    match_group: list[int],
    context: int,
) -> list[str]:
    """Format one block of merged matches with context."""
    start = max(0, match_group[0] - context)
    end = min(len(lines), match_group[-1] + context + 1)

    primary = match_group[0]
    scope = find_scope(lines, primary)
    header = f"{rel_path}:{primary + 1}"
    if scope:
        header += f"  ({scope})"

    match_set = set(match_group)
    separator = "-" * 70

    out = [separator, header]
    for i in range(start, end):
        num = i + 1
        marker = ">>>" if i in match_set else "   "
        out.append(f"{marker} {num:>5} | {lines[i]}")

    return out


def main() -> int:
    parser = argparse.ArgumentParser(
        prog="grep.py",
        description="Grep with context -> log file, or brief to stdout",
    )
    parser.add_argument("pattern", help="Search pattern (regex unless -F)")
    parser.add_argument("--path", default=str(SOURCES), help="Directory to search")
    parser.add_argument(
        "--ext", nargs="+", default=[".cpp", ".h"],
        help="File extensions (default: .cpp .h)",
    )
    parser.add_argument(
        "--context", "-C", type=int, default=5, help="Context lines (default 5)",
    )
    parser.add_argument(
        "--output", "-o", default=None,
        help="Output log file (default: .claude/scratch/tools/grep/grep_results.log)",
    )
    parser.add_argument(
        "--fixed", "-F", action="store_true", help="Fixed string match (not regex)",
    )
    parser.add_argument(
        "-i", "--ignore-case", action="store_true", help="Case-insensitive search",
    )
    parser.add_argument(
        "-b", "--brief", action="store_true",
        help="Brief mode: one line per match to stdout (no log file)",
    )

    args = parser.parse_args()

    # Windows console can't print all Unicode from source files
    sys.stdout.reconfigure(errors="replace")

    flags = re.IGNORECASE if args.ignore_case else 0
    raw = re.escape(args.pattern) if args.fixed else args.pattern
    try:
        pattern = re.compile(raw, flags)
    except re.error as e:
        print(f"Bad regex: {e}")
        return 2

    search_path = Path(args.path)
    files: list[Path] = []
    for ext in args.ext:
        ext = ext if ext.startswith(".") else f".{ext}"
        files.extend(search_path.rglob(f"*{ext}"))
    files.sort()

    total_matches = 0
    matched_files = 0

    if args.brief:
        brief_lines: list[str] = []
        for filepath in files:
            matches, lines = search_file(filepath, pattern, args.context)
            if not matches:
                continue
            matched_files += 1
            total_matches += len(matches)
            try:
                rel_path = filepath.relative_to(SOURCES)
            except ValueError:
                rel_path = filepath
            for idx in matches:
                brief_lines.append(
                    f"  {rel_path}:{idx + 1:<6} | {lines[idx].strip()}"
                )

        print(f"# {total_matches} match(es) across {matched_files} file(s)"
              f"  --  pattern: {args.pattern}\n")
        for line in brief_lines:
            print(line)
        return 0 if total_matches > 0 else 1

    # Detail mode: full context blocks to log file
    ensure_output_dir()

    output_lines: list[str] = []
    for filepath in files:
        matches, lines = search_file(filepath, pattern, args.context)
        if not matches:
            continue

        matched_files += 1
        total_matches += len(matches)

        try:
            rel_path = filepath.relative_to(SOURCES)
        except ValueError:
            rel_path = filepath

        groups = merge_nearby(matches, args.context)
        for group in groups:
            block = format_block(rel_path, lines, group, args.context)
            output_lines.extend(block)

    if output_lines:
        output_lines.append("-" * 70)

    summary = f"# {total_matches} match(es) across {matched_files} file(s)  --  pattern: {args.pattern}"
    output_lines.insert(0, summary)
    output_lines.insert(1, "")

    log_path = Path(args.output) if args.output else OUTPUT_DIR / "grep_results.log"
    log_path.write_text("\n".join(output_lines) + "\n", encoding="utf-8")

    print(f"{total_matches} match(es) in {matched_files} file(s). "
          f"Results -> {log_path}")
    return 0 if total_matches > 0 else 1


if __name__ == "__main__":
    sys.exit(main())
