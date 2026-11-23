#!/usr/bin/env python3
"""
Phase 1 Step 2: Mechanical signature migration for all dialog box files.

Transforms:
1. Virtual method signatures: remove mouse params
2. Method body replacements: mouse_x/y/z/lb → hb::shared::input calls
3. Info().m_x/y/size_x/size_y → m_x/m_y/m_size_x/m_size_y (moved to base)
4. Info().m_is_scroll_selected → m_is_scroll_selected
5. Info().m_can_close_on_right_click → m_can_close_on_right_click
6. set_can_close_on_right_click(...) → m_can_close_on_right_click = ...
7. get_top_dialog_box_index() → get_top_id()
8. Private helper method signatures: remove mouse params where they were threaded
9. Add #include "IInput.h" to .cpp files that use input functions

Usage:
  python phase1_dialog_signature_migration.py --dry-run
  python phase1_dialog_signature_migration.py
"""

import os
import re
import sys
import glob
import argparse
from pathlib import Path

SCRIPT_DIR = Path(__file__).parent
PROJECT_ROOT = SCRIPT_DIR.parent
CLIENT_DIR = PROJECT_ROOT / "Sources" / "Client"
OUTPUT_DIR = SCRIPT_DIR / "output"

# Files to skip (already manually updated)
SKIP_FILES = {
    "DialogBox_HudPanel.h",
    "DialogBox_HudPanel.cpp",
    "IDialogBox.h",
    "IDialogBox.cpp",
    "DialogBoxManager.h",
    "DialogBoxManager.cpp",
    "DialogBoxInfo.h",
}

# Non-dialog files that also need get_top_dialog_box_index → get_top_id
EXTRA_FILES = [
    CLIENT_DIR / "Screen_OnGame.cpp",
    CLIENT_DIR / "Game.Hotkeys.cpp",
]

def get_dialog_files():
    """Find all DialogBox_*.h and DialogBox_*.cpp files."""
    files = []
    for pattern in ["DialogBox_*.h", "DialogBox_*.cpp"]:
        for f in CLIENT_DIR.glob(pattern):
            if f.name not in SKIP_FILES:
                # Skip build artifacts
                if "build" in str(f) or "Release_x64" in str(f) or "Debug_x64" in str(f):
                    continue
                files.append(f)
    return sorted(files)


class DialogMigrator:
    def __init__(self, dry_run=False):
        self.dry_run = dry_run
        self.changes = []  # (file, description) tuples
        self.log_lines = []

    def log(self, msg):
        self.log_lines.append(msg)

    def process_all(self):
        dialog_files = get_dialog_files()
        self.log(f"Found {len(dialog_files)} dialog files to process")

        for f in dialog_files:
            self.process_file(f)

        # Also process extra files
        for f in EXTRA_FILES:
            if f.exists():
                self.process_extra_file(f)

        return self.changes

    def process_file(self, filepath):
        """Process a single dialog file."""
        with open(filepath, 'r', encoding='utf-8', errors='replace') as fh:
            content = fh.read()

        original = content
        is_header = filepath.suffix == '.h'
        is_cpp = filepath.suffix == '.cpp'

        # 1. Signature changes in headers
        if is_header:
            content = self.transform_header_signatures(content, filepath.name)

        # 2. Signature changes in .cpp (definitions)
        if is_cpp:
            content = self.transform_cpp_signatures(content, filepath.name)
            # Add IInput.h include if needed and not already present
            if 'hb::shared::input::' in content and '#include "IInput.h"' not in content:
                content = self.add_iinput_include(content)

        # 3. Info() field migrations (both .h and .cpp)
        content = self.migrate_info_fields(content, filepath.name)

        # 4. get_top_dialog_box_index → get_top_id
        content = self.replace_get_top(content, filepath.name)

        # 5. set_can_close_on_right_click
        content = self.replace_set_can_close(content, filepath.name)

        if content != original:
            self.changes.append((filepath, "transformed"))
            if not self.dry_run:
                with open(filepath, 'w', encoding='utf-8', newline='\n') as fh:
                    fh.write(content)

    def process_extra_file(self, filepath):
        """Process non-dialog files that need get_top_dialog_box_index updates."""
        with open(filepath, 'r', encoding='utf-8', errors='replace') as fh:
            content = fh.read()
        original = content
        content = self.replace_get_top(content, filepath.name)
        if content != original:
            self.changes.append((filepath, "get_top_id update"))
            if not self.dry_run:
                with open(filepath, 'w', encoding='utf-8', newline='\n') as fh:
                    fh.write(content)

    def transform_header_signatures(self, content, filename):
        """Transform virtual method declarations in headers."""
        # on_draw(short mouse_x, short mouse_y, short z, char lb) → on_draw()
        content = re.sub(
            r'void on_draw\(short mouse_x, short mouse_y, short z, char lb\)',
            'void on_draw()',
            content
        )
        # on_click(short mouse_x, short mouse_y) → on_click()
        content = re.sub(
            r'bool on_click\(short mouse_x, short mouse_y\)',
            'bool on_click()',
            content
        )
        # on_double_click(short mouse_x, short mouse_y) → on_double_click()
        content = re.sub(
            r'bool on_double_click\(short mouse_x, short mouse_y\)',
            'bool on_double_click()',
            content
        )
        # on_press(short mouse_x, short mouse_y) → on_press()
        content = re.sub(
            r'PressResult on_press\(short mouse_x, short mouse_y\)',
            'PressResult on_press()',
            content
        )
        # on_item_drop(short mouse_x, short mouse_y) → on_item_drop()
        content = re.sub(
            r'bool on_item_drop\(short mouse_x, short mouse_y\)',
            'bool on_item_drop()',
            content
        )
        # on_enable(int type, int v1, int v2, char* string) → on_enable(int type, int64_t v1, int v2, const char* string)
        content = re.sub(
            r'void on_enable\(int type, int v1, int v2, char\* string\)',
            'void on_enable(int type, int64_t v1, int v2, const char* string)',
            content
        )

        # --- Private helper methods that thread mouse params ---
        # Common pattern: draw_something(..., short mouse_x, short mouse_y, short z, char lb)
        # We remove the mouse params from these too
        content = self.transform_helper_signatures_header(content, filename)

        return content

    def transform_helper_signatures_header(self, content, filename):
        """Transform private helper method declarations that take mouse params."""
        # Pattern: method(..., short mouse_x, short mouse_y, short z, char lb)
        # Remove the trailing mouse params
        content = re.sub(
            r'(void \w+\([^)]*?),\s*short mouse_x,\s*short mouse_y,\s*short z,\s*char lb\)',
            r'\1)',
            content
        )
        # Pattern: method(..., short mouse_x, short mouse_y)
        content = re.sub(
            r'(void \w+\([^)]*?),\s*short mouse_x,\s*short mouse_y\)',
            r'\1)',
            content
        )
        # Pattern: method(short mouse_x, short mouse_y, short z, char lb) — no other params
        content = re.sub(
            r'(void \w+)\(short mouse_x,\s*short mouse_y,\s*short z,\s*char lb\)',
            r'\1()',
            content
        )
        # Pattern: method(short mouse_x, short mouse_y) — no other params
        content = re.sub(
            r'(void \w+)\(short mouse_x,\s*short mouse_y\)',
            r'\1()',
            content
        )
        # bool variants
        content = re.sub(
            r'(bool \w+\([^)]*?),\s*short mouse_x,\s*short mouse_y\)',
            r'\1)',
            content
        )
        content = re.sub(
            r'(bool \w+)\(short mouse_x,\s*short mouse_y\)',
            r'\1()',
            content
        )
        return content

    def transform_cpp_signatures(self, content, filename):
        """Transform method definitions in .cpp files."""
        # Get class name from filename: DialogBox_Bank.cpp → DialogBox_Bank
        class_name = filename.replace('.cpp', '')

        # on_draw definition
        content = re.sub(
            rf'void {re.escape(class_name)}::on_draw\(short mouse_x, short mouse_y, short z, char lb\)',
            f'void {class_name}::on_draw()',
            content
        )
        # on_click definition
        content = re.sub(
            rf'bool {re.escape(class_name)}::on_click\(short mouse_x, short mouse_y\)',
            f'bool {class_name}::on_click()',
            content
        )
        # on_double_click definition
        content = re.sub(
            rf'bool {re.escape(class_name)}::on_double_click\(short mouse_x, short mouse_y\)',
            f'bool {class_name}::on_double_click()',
            content
        )
        # on_press definition
        content = re.sub(
            rf'PressResult {re.escape(class_name)}::on_press\(short mouse_x, short mouse_y\)',
            f'PressResult {class_name}::on_press()',
            content
        )
        # on_item_drop definition
        content = re.sub(
            rf'bool {re.escape(class_name)}::on_item_drop\(short mouse_x, short mouse_y\)',
            f'bool {class_name}::on_item_drop()',
            content
        )
        # on_enable definition
        content = re.sub(
            rf'void {re.escape(class_name)}::on_enable\(int type, int v1, int v2, char\* string\)',
            f'void {class_name}::on_enable(int type, int64_t v1, int v2, const char* string)',
            content
        )

        # Private helper method definitions
        content = self.transform_helper_signatures_cpp(content, class_name)

        # Now replace mouse variable usage in method bodies
        content = self.replace_mouse_vars_in_bodies(content, filename)

        return content

    def transform_helper_signatures_cpp(self, content, class_name):
        """Transform private helper method definitions in .cpp files."""
        escaped = re.escape(class_name)
        # Pattern: ClassName::method(..., short mouse_x, short mouse_y, short z, char lb)
        content = re.sub(
            rf'((?:void|bool) {escaped}::\w+\([^)]*?),\s*short mouse_x,\s*short mouse_y,\s*short z,\s*char lb\)',
            r'\1)',
            content
        )
        # Pattern: ClassName::method(..., short mouse_x, short mouse_y)
        content = re.sub(
            rf'((?:void|bool) {escaped}::\w+\([^)]*?),\s*short mouse_x,\s*short mouse_y\)',
            r'\1)',
            content
        )
        # Pattern: ClassName::method(short mouse_x, short mouse_y, short z, char lb)
        content = re.sub(
            rf'((?:void|bool) {escaped}::\w+)\(short mouse_x,\s*short mouse_y,\s*short z,\s*char lb\)',
            r'\1()',
            content
        )
        # Pattern: ClassName::method(short mouse_x, short mouse_y)
        content = re.sub(
            rf'((?:void|bool) {escaped}::\w+)\(short mouse_x,\s*short mouse_y\)',
            r'\1()',
            content
        )
        return content

    def replace_mouse_vars_in_bodies(self, content, filename):
        """Replace mouse variable usage with hb::shared::input calls.

        We need to be careful:
        - mouse_x → hb::shared::input::get_mouse_x()
        - mouse_y → hb::shared::input::get_mouse_y()
        - z (scroll delta) → hb::shared::input::get_mouse_wheel_delta()
        - lb (left button) → hb::shared::input::is_mouse_button_down(hb::shared::input::MouseButton::Left)

        But we need to avoid replacing inside: function signatures (already done),
        string literals, comments, etc.

        Strategy: Replace whole-word matches of these variable names.
        For 'z' and 'lb', only replace when they appear in specific patterns
        to avoid false positives (z is too short for blind replacement).
        """
        lines = content.split('\n')
        new_lines = []
        in_method_body = False
        brace_depth = 0
        method_has_mouse_params = False
        needs_mouse_locals = set()  # Track which methods need local vars

        # First pass: identify which methods used mouse params and add local variable declarations
        # We'll use a simpler approach: add local variable extraction at the top of each method body
        # that used to receive mouse params

        # Instead of complex tracking, we inject local variables at method body start
        i = 0
        while i < len(lines):
            line = lines[i]

            # Detect the start of a method that was transformed (no mouse params anymore)
            # Look for pattern: "void ClassName::on_draw()" or "bool ClassName::on_click()" etc.
            # followed by "{" on same or next line
            method_match = re.match(
                r'^(void|bool|PressResult)\s+\w+::(on_draw|on_click|on_double_click|on_press|on_item_drop)\(\)',
                line.strip()
            )
            if method_match:
                method_name = method_match.group(2)
                # Determine which local vars this method needs
                remaining_content = '\n'.join(lines[i:])

                # Find the method body (up to matching closing brace)
                body_start = remaining_content.find('{')
                if body_start >= 0:
                    # Scan ahead to find end of method (matching braces)
                    depth = 0
                    body_end = body_start
                    for j in range(body_start, len(remaining_content)):
                        if remaining_content[j] == '{':
                            depth += 1
                        elif remaining_content[j] == '}':
                            depth -= 1
                            if depth == 0:
                                body_end = j
                                break

                    body = remaining_content[body_start:body_end+1]

                    # Check what mouse vars are used in this body
                    has_mouse_x = bool(re.search(r'\bmouse_x\b', body))
                    has_mouse_y = bool(re.search(r'\bmouse_y\b', body))
                    has_z = bool(re.search(r'\bz\b', body))
                    has_lb = bool(re.search(r'\blb\b', body))

                    # For z and lb, be more careful - only count if in specific contexts
                    # z is commonly used as a scroll variable, lb as left button
                    if has_z:
                        # Check if z is used as a parameter name vs standalone variable
                        # Look for patterns like "z != 0", "z > 0", "z < 0", scroll-related
                        z_usage = re.findall(r'(?<!\w)z(?!\w)', body)
                        if not z_usage:
                            has_z = False

                    if has_lb:
                        lb_usage = re.findall(r'(?<!\w)lb(?!\w)', body)
                        if not lb_usage:
                            has_lb = False

                    # Inject local variable declarations after the opening brace
                    if has_mouse_x or has_mouse_y or has_z or has_lb:
                        # Find the line with the opening brace
                        for k in range(i, min(i + 5, len(lines))):
                            if '{' in lines[k]:
                                inject_lines = []
                                if has_mouse_x:
                                    inject_lines.append('\tshort mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());')
                                if has_mouse_y:
                                    inject_lines.append('\tshort mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());')
                                if has_z:
                                    inject_lines.append('\tshort z = static_cast<short>(hb::shared::input::get_mouse_wheel_delta());')
                                if has_lb:
                                    inject_lines.append('\tchar lb = hb::shared::input::is_mouse_button_down(hb::shared::input::MouseButton::Left) ? 1 : 0;')

                                # Insert after the brace line
                                for idx, inject in enumerate(inject_lines):
                                    lines.insert(k + 1 + idx, inject)
                                break

            # Also handle private helper methods that had mouse params stripped
            helper_match = re.match(
                r'^(void|bool)\s+\w+::(\w+)\(\s*\)',
                line.strip()
            )
            if helper_match and not method_match:
                method_name = helper_match.group(2)
                # Skip if it's a known no-param method
                if method_name not in ('on_update', 'on_disable'):
                    # Check if this was a method that originally had mouse params
                    # by looking if mouse_x/y/z/lb are used in its body
                    remaining_content = '\n'.join(lines[i:])
                    body_start = remaining_content.find('{')
                    if body_start >= 0:
                        depth = 0
                        body_end = body_start
                        for j in range(body_start, len(remaining_content)):
                            if remaining_content[j] == '{':
                                depth += 1
                            elif remaining_content[j] == '}':
                                depth -= 1
                                if depth == 0:
                                    body_end = j
                                    break
                        body = remaining_content[body_start:body_end+1]
                        has_mouse_x = bool(re.search(r'\bmouse_x\b', body))
                        has_mouse_y = bool(re.search(r'\bmouse_y\b', body))
                        has_z = bool(re.search(r'\bz\b', body))
                        has_lb = bool(re.search(r'\blb\b', body))
                        if has_mouse_x or has_mouse_y or has_z or has_lb:
                            for k in range(i, min(i + 5, len(lines))):
                                if '{' in lines[k]:
                                    inject_lines = []
                                    if has_mouse_x:
                                        inject_lines.append('\tshort mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());')
                                    if has_mouse_y:
                                        inject_lines.append('\tshort mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());')
                                    if has_z:
                                        inject_lines.append('\tshort z = static_cast<short>(hb::shared::input::get_mouse_wheel_delta());')
                                    if has_lb:
                                        inject_lines.append('\tchar lb = hb::shared::input::is_mouse_button_down(hb::shared::input::MouseButton::Left) ? 1 : 0;')
                                    for idx, inject in enumerate(inject_lines):
                                        lines.insert(k + 1 + idx, inject)
                                    break

            i += 1

        # Also handle helper methods that still have some params but had mouse params removed
        # e.g. draw_item_list(short sX, short sY, short size_x) — mouse_x/y/z/lb were removed
        # These need local var injection too
        content = '\n'.join(lines)

        # Now handle helper call sites - remove mouse args from calls to helpers
        # e.g. draw_scrollbar(sX, sY, total, mouse_x, mouse_y, z, lb) → draw_scrollbar(sX, sY, total)
        # This is tricky because we need to know which args are the mouse ones.
        # The safest approach: the signature transform already removed the params,
        # so calls with extra args will cause compile errors that we fix in the build step.
        # For now, let's handle the common patterns.
        content = self.remove_mouse_args_from_calls(content, filename)

        return content

    def remove_mouse_args_from_calls(self, content, filename):
        """Remove mouse_x, mouse_y, z, lb arguments from internal method calls."""
        # Pattern: method_call(..., mouse_x, mouse_y, z, lb)
        content = re.sub(
            r'(\b\w+\([^)]*?),\s*mouse_x,\s*mouse_y,\s*z,\s*lb\)',
            r'\1)',
            content
        )
        # Pattern: method_call(mouse_x, mouse_y, z, lb) — only args
        content = re.sub(
            r'(\b\w+)\(mouse_x,\s*mouse_y,\s*z,\s*lb\)',
            r'\1()',
            content
        )
        # Pattern: method_call(..., mouse_x, mouse_y)
        content = re.sub(
            r'(\b\w+\([^)]*?),\s*mouse_x,\s*mouse_y\)',
            r'\1)',
            content
        )
        # Pattern: method_call(mouse_x, mouse_y) — only args
        content = re.sub(
            r'(\b\w+)\(mouse_x,\s*mouse_y\)',
            r'\1()',
            content
        )
        return content

    def migrate_info_fields(self, content, filename):
        """Replace Info().m_x → m_x etc. for fields that moved to IDialogBox base."""
        # Info().m_x → m_x
        content = content.replace('Info().m_x', 'm_x')
        content = content.replace('Info().m_y', 'm_y')
        content = content.replace('Info().m_size_x', 'm_size_x')
        content = content.replace('Info().m_size_y', 'm_size_y')
        content = content.replace('Info().m_is_scroll_selected', 'm_is_scroll_selected')
        content = content.replace('Info().m_can_close_on_right_click', 'm_can_close_on_right_click')
        return content

    def replace_get_top(self, content, filename):
        """Replace get_top_dialog_box_index() → get_top_id()."""
        content = content.replace('get_top_dialog_box_index()', 'get_top_id()')
        return content

    def replace_set_can_close(self, content, filename):
        """Replace set_can_close_on_right_click(X) → m_can_close_on_right_click = X."""
        content = re.sub(
            r'set_can_close_on_right_click\((\w+)\)',
            r'm_can_close_on_right_click = \1',
            content
        )
        return content

    def add_iinput_include(self, content):
        """Add #include "IInput.h" after other includes."""
        # Find the last #include line and add after it
        lines = content.split('\n')
        last_include_idx = -1
        for i, line in enumerate(lines):
            if line.strip().startswith('#include'):
                last_include_idx = i
        if last_include_idx >= 0:
            lines.insert(last_include_idx + 1, '#include "IInput.h"')
        return '\n'.join(lines)


def main():
    parser = argparse.ArgumentParser(description='Phase 1 dialog signature migration')
    parser.add_argument('--dry-run', action='store_true', help='Preview changes without modifying files')
    parser.add_argument('--verify', action='store_true', help='Verify no remaining old signatures')
    args = parser.parse_args()

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    if args.verify:
        print("=== Verification Mode ===")
        dialog_files = get_dialog_files()
        issues = []
        for f in dialog_files:
            with open(f, 'r', encoding='utf-8', errors='replace') as fh:
                content = fh.read()
            # Check for old signatures
            if re.search(r'on_draw\(short mouse_x', content):
                issues.append(f"{f.name}: still has old on_draw signature")
            if re.search(r'on_click\(short mouse_x', content):
                issues.append(f"{f.name}: still has old on_click signature")
            if 'Info().m_x' in content or 'Info().m_y' in content:
                issues.append(f"{f.name}: still has Info().m_x/m_y")
            if 'get_top_dialog_box_index' in content:
                issues.append(f"{f.name}: still has get_top_dialog_box_index")
        if issues:
            print(f"Found {len(issues)} issues:")
            for issue in issues:
                print(f"  - {issue}")
        else:
            print("All files clean!")
        return

    migrator = DialogMigrator(dry_run=args.dry_run)
    changes = migrator.process_all()

    # Write log
    log_path = OUTPUT_DIR / "phase1_migration_log.txt"
    with open(log_path, 'w') as f:
        f.write(f"Phase 1 Dialog Signature Migration\n")
        f.write(f"{'DRY RUN' if args.dry_run else 'APPLIED'}\n")
        f.write(f"{'=' * 60}\n\n")
        f.write(f"Files changed: {len(changes)}\n\n")
        for filepath, desc in changes:
            f.write(f"  {filepath.name}: {desc}\n")
        f.write(f"\n{'=' * 60}\n")
        for line in migrator.log_lines:
            f.write(f"{line}\n")

    print(f"{'DRY RUN: ' if args.dry_run else ''}Processed {len(changes)} files")
    print(f"Log written to: {log_path}")

    if args.dry_run:
        for filepath, desc in changes:
            print(f"  Would change: {filepath.name} ({desc})")


if __name__ == '__main__':
    main()
