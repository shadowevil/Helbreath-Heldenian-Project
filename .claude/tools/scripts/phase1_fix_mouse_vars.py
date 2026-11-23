#!/usr/bin/env python3
"""
Phase 1 Fix: Inject missing mouse local variables into helper method bodies.

The main migration script correctly injects local vars into virtual methods
(on_draw, on_click, etc.) but misses private helper methods that still have
other params and use mouse_x/mouse_y/z/lb from the former parent scope.

This script scans all DialogBox_*.cpp files for method bodies that use
mouse_x, mouse_y, z, or lb without declaring them.
"""

import re
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).parent
PROJECT_ROOT = SCRIPT_DIR.parent
CLIENT_DIR = PROJECT_ROOT / "Sources" / "Client"

SKIP_FILES = {
    "DialogBox_HudPanel.cpp",
    "IDialogBox.cpp",
    "DialogBoxManager.cpp",
}


def find_method_bodies(content):
    """Find all C++ method definitions and their body ranges.

    Returns list of (method_name, sig_line_idx, body_start_line_idx, body_end_line_idx, param_str)
    """
    lines = content.split('\n')
    methods = []

    i = 0
    while i < len(lines):
        line = lines[i].strip()

        # Match: ReturnType ClassName::MethodName(params)
        m = re.match(r'^(?:void|bool|PressResult|int)\s+\w+::(\w+)\(([^)]*)\)\s*$', line)
        if not m:
            # Also try multi-line: signature on this line, { on next
            m = re.match(r'^(?:void|bool|PressResult|int)\s+\w+::(\w+)\(([^)]*)\)$', line)

        if m:
            method_name = m.group(1)
            params = m.group(2).strip()
            sig_line = i

            # Find the opening brace
            brace_line = -1
            for j in range(i, min(i + 5, len(lines))):
                if '{' in lines[j]:
                    brace_line = j
                    break

            if brace_line >= 0:
                # Find matching closing brace
                depth = 0
                end_line = brace_line
                for j in range(brace_line, len(lines)):
                    for ch in lines[j]:
                        if ch == '{':
                            depth += 1
                        elif ch == '}':
                            depth -= 1
                            if depth == 0:
                                end_line = j
                                break
                    if depth == 0:
                        break

                methods.append((method_name, sig_line, brace_line, end_line, params))
                i = end_line + 1
                continue

        i += 1

    return methods


def process_file(filepath, dry_run=False):
    """Process a single .cpp file."""
    with open(filepath, 'r', encoding='utf-8', errors='replace') as f:
        content = f.read()

    original = content
    lines = content.split('\n')
    methods = find_method_bodies(content)

    # Track insertions (reverse order to not shift indices)
    insertions = []  # (line_idx, vars_to_inject)

    for method_name, sig_line, brace_line, end_line, params in methods:
        # Get body text (lines between opening and closing brace)
        body_lines = lines[brace_line + 1:end_line]
        body = '\n'.join(body_lines)

        # Check what mouse vars are used in body
        needs_mx = bool(re.search(r'\bmouse_x\b', body))
        needs_my = bool(re.search(r'\bmouse_y\b', body))
        needs_z = False
        needs_lb = False

        # For z: only if used as standalone variable (not in words like "size", "z_layer")
        # Check for z in typical scroll-related patterns
        if re.search(r'(?<!\w)z(?!\w)', body):
            # Further filter: z must appear in comparison/assignment context
            # like "z != 0", "z > 0", "z < 0", "if (z"
            if re.search(r'\bz\s*[!=<>]|\bz\s*\)', body) or re.search(r'[\(,]\s*z\s*[,\)]', body):
                needs_z = True

        # For lb: left button variable
        if re.search(r'\blb\b', body):
            if re.search(r'\blb\s*[!=<>]|\blb\s*\)', body) or re.search(r'[\(,]\s*lb\s*[,\)]', body):
                needs_lb = True

        # Check if already declared in params
        if 'mouse_x' in params:
            needs_mx = False
        if 'mouse_y' in params:
            needs_my = False

        # Check if already declared as local vars in body
        if needs_mx and re.search(r'short\s+mouse_x\s*=', body):
            needs_mx = False
        if needs_my and re.search(r'short\s+mouse_y\s*=', body):
            needs_my = False
        if needs_z and re.search(r'short\s+z\s*=', body):
            needs_z = False
        if needs_lb and re.search(r'char\s+lb\s*=', body):
            needs_lb = False

        if needs_mx or needs_my or needs_z or needs_lb:
            inject = []
            if needs_mx:
                inject.append('\tshort mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());')
            if needs_my:
                inject.append('\tshort mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());')
            if needs_z:
                inject.append('\tshort z = static_cast<short>(hb::shared::input::get_mouse_wheel_delta());')
            if needs_lb:
                inject.append('\tchar lb = hb::shared::input::is_mouse_button_down(hb::shared::input::MouseButton::Left) ? 1 : 0;')

            insertions.append((brace_line + 1, inject, method_name))

    if not insertions:
        return False

    # Apply insertions in reverse order
    insertions.sort(key=lambda x: x[0], reverse=True)
    for line_idx, inject_lines, method_name in insertions:
        if dry_run:
            print(f"  {filepath.name}:{line_idx} in {method_name}: would inject {len(inject_lines)} local var(s)")
        for idx, line in enumerate(reversed(inject_lines)):
            lines.insert(line_idx, line)

    new_content = '\n'.join(lines)
    if new_content != original:
        if not dry_run:
            with open(filepath, 'w', encoding='utf-8', newline='\n') as f:
                f.write(new_content)
        return True
    return False


def main():
    dry_run = '--dry-run' in sys.argv

    cpp_files = sorted(CLIENT_DIR.glob("DialogBox_*.cpp"))
    cpp_files = [f for f in cpp_files if f.name not in SKIP_FILES
                 and 'build' not in str(f) and 'Release_x64' not in str(f) and 'Debug_x64' not in str(f)]

    changed = 0
    for f in cpp_files:
        if process_file(f, dry_run):
            changed += 1

    print(f"{'DRY RUN: ' if dry_run else ''}{changed} files needed fixes")


if __name__ == '__main__':
    main()
