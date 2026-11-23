#!/usr/bin/env python3
"""
Phase 1 Fix: Resolve remaining compile errors after migration.

Fixes:
1. Info(DialogBoxId::X).m_x/m_y/m_size_x/m_size_y/m_is_scroll_selected
   → self-access: m_x, m_y, etc. (when accessing own dialog ID)
   → cross-dialog: get_dialog(DialogBoxId::X)->m_x (when accessing other dialog ID)

2. set_order_at calls → removed (z-layer system replaces them)

3. Redefinition of formal parameter z/lb → remove injected local when already
   a formal parameter

4. DrawMode helpers in ItemUpgrade.h → remove mouse_x/mouse_y from signatures

5. draw_toggle in SysMenu → remove mouse_x/mouse_y from call sites

6. draw_text_aligned in MagicShop → fix m_size_x reference
"""

import re
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).parent
PROJECT_ROOT = SCRIPT_DIR.parent
CLIENT_DIR = PROJECT_ROOT / "Sources" / "Client"

# Map dialog files to their own DialogBoxId
DIALOG_SELF_IDS = {
    "DialogBox_Bank.cpp": "Bank",
    "DialogBox_ChangeStatsMajestic.cpp": "ChangeStatsMajestic",
    "DialogBox_Character.cpp": "CharacterInfo",
    "DialogBox_ChatHistory.cpp": "ChatHistory",
    "DialogBox_CityHallMenu.cpp": "CityHallMenu",
    "DialogBox_Commander.cpp": "Commander",
    "DialogBox_ConfirmExchange.cpp": "ConfirmExchange",
    "DialogBox_Constructor.cpp": "Constructor",
    "DialogBox_CrusadeJob.cpp": "CrusadeJob",
    "DialogBox_Exchange.cpp": "Exchange",
    "DialogBox_Fishing.cpp": "Fishing",
    "DialogBox_GuideMap.cpp": "GuideMap",
    "DialogBox_GuildHallMenu.cpp": "GuildHallMenu",
    "DialogBox_GuildMenu.cpp": "GuildMenu",
    "DialogBox_GuildOperation.cpp": "GuildOperation",
    "DialogBox_Help.cpp": "Help",
    "DialogBox_HudPanel.cpp": "HudPanel",
    "DialogBox_Inventory.cpp": "Inventory",
    "DialogBox_ItemCreator.cpp": "ItemCreator",
    "DialogBox_ItemDrop.cpp": "ItemDrop",
    "DialogBox_ItemDropAmount.cpp": "ItemDropExternal",
    "DialogBox_ItemUpgrade.cpp": "ItemUpgrade",
    "DialogBox_LevelUpSetting.cpp": "LevelUpSetting",
    "DialogBox_Magic.cpp": "Magic",
    "DialogBox_MagicShop.cpp": "MagicShop",
    "DialogBox_Manufacture.cpp": "Manufacture",
    "DialogBox_Map.cpp": "Map",
    "DialogBox_Noticement.cpp": "Noticement",
    "DialogBox_NpcActionQuery.cpp": "NpcActionQuery",
    "DialogBox_NpcTalk.cpp": "NpcTalk",
    "DialogBox_Party.cpp": "Party",
    "DialogBox_Quest.cpp": "Quest",
    "DialogBox_RepairAll.cpp": "RepairAll",
    "DialogBox_Resurrect.cpp": "Resurrect",
    "DialogBox_SellList.cpp": "SellList",
    "DialogBox_SellOrRepair.cpp": "SellOrRepair",
    "DialogBox_Shop.cpp": "SaleMenu",
    "DialogBox_Skill.cpp": "Skill",
    "DialogBox_Slates.cpp": "Slates",
    "DialogBox_Soldier.cpp": "Soldier",
    "DialogBox_SysMenu.cpp": "SysMenu",
    "DialogBox_TesterMenu.cpp": "TesterMenu",
    "DialogBox_Text.cpp": "Text",
    "DialogBox_WarningMsg.cpp": "WarningMsg",
}

# Fields that moved from DialogBoxInfo to IDialogBox
MOVED_FIELDS = {'m_x', 'm_y', 'm_size_x', 'm_size_y', 'm_is_scroll_selected', 'm_can_close_on_right_click'}

changes_log = []


def fix_info_field_access(content, filename):
    """Fix Info(DialogBoxId::X).m_field for fields that moved to IDialogBox."""
    self_id = DIALOG_SELF_IDS.get(filename)

    def replace_match(m):
        prefix = m.group(1)  # e.g. "m_game->m_dialog_box_manager." or "m_game->m_dialog_box_manager."
        dialog_id = m.group(2)  # e.g. "ChatHistory"
        field = m.group(3)  # e.g. "m_x"

        if field not in MOVED_FIELDS:
            return m.group(0)  # Not a moved field, leave as-is

        if self_id and dialog_id == self_id:
            # Self-access: replace with just the field name
            changes_log.append(f"  {filename}: Info(DialogBoxId::{dialog_id}).{field} -> {field} (self)")
            return field
        else:
            # Cross-dialog access: use get_dialog
            changes_log.append(f"  {filename}: Info(DialogBoxId::{dialog_id}).{field} -> get_dialog()->{field}")
            return f"{prefix}get_dialog(DialogBoxId::{dialog_id})->{field}"

    # Pattern: <prefix>Info(DialogBoxId::X).<field>
    # prefix can be "m_game->m_dialog_box_manager." or "m_dialog_box_manager." or empty
    pattern = r'((?:m_game->)?m_dialog_box_manager\.)Info\(DialogBoxId::(\w+)\)\.(\w+)'
    content = re.sub(pattern, replace_match, content)

    return content


def fix_set_order_at(content, filename):
    """Remove set_order_at calls — replaced by z-layer system."""
    lines = content.split('\n')
    new_lines = []
    removed = 0
    i = 0
    while i < len(lines):
        line = lines[i]
        if 'set_order_at' in line:
            # Remove entire line
            removed += 1
            i += 1
            continue
        new_lines.append(line)
        i += 1

    if removed:
        changes_log.append(f"  {filename}: removed {removed} set_order_at lines")

    return '\n'.join(new_lines)


def fix_redefined_params(content, filename):
    """Remove injected local z/lb vars when they're already formal parameters."""
    lines = content.split('\n')
    new_lines = []
    removed = 0

    # Track which methods have z/lb as formal parameters
    # Look for method signatures with (... short z ...) or (... char lb ...)
    i = 0
    while i < len(lines):
        line = lines[i]

        # Check if this is a method signature with z or lb params
        # We need to look at the function signature above and check if z/lb are params
        # Then if the next few lines have the injection, skip them
        # Simpler approach: just look for injected lines and check if the enclosing
        # method has z/lb as params

        # The injected lines look exactly like:
        # \tshort z = static_cast<short>(hb::shared::input::get_mouse_wheel_delta());
        # \tchar lb = hb::shared::input::is_mouse_button_down(...
        stripped = line.strip()

        if stripped == 'short z = static_cast<short>(hb::shared::input::get_mouse_wheel_delta());':
            # Check if z is a formal parameter by looking backwards for the method signature
            if _is_formal_param(lines, i, 'z'):
                removed += 1
                i += 1
                continue

        if stripped.startswith('char lb = hb::shared::input::is_mouse_button_down('):
            if _is_formal_param(lines, i, 'lb'):
                removed += 1
                i += 1
                continue

        new_lines.append(line)
        i += 1

    if removed:
        changes_log.append(f"  {filename}: removed {removed} redundant local var injections (z/lb)")

    return '\n'.join(new_lines)


def _is_formal_param(lines, current_line, param_name):
    """Check if param_name is a formal parameter of the enclosing method."""
    # Walk backwards to find the method signature
    for j in range(current_line - 1, max(current_line - 10, -1), -1):
        line = lines[j].strip()
        # Look for method signature: ReturnType ClassName::MethodName(params)
        m = re.match(r'^(?:void|bool|PressResult|int)\s+\w+::\w+\(([^)]*)\)', line)
        if m:
            params = m.group(1)
            # Check if param_name is in the parameter list
            if re.search(r'\b' + param_name + r'\b', params):
                return True
            return False
        # Opening brace
        if line == '{':
            # Keep looking, could be on line above
            continue
    return False


def fix_item_upgrade_header(dry_run=False):
    """Remove mouse_x/mouse_y from DrawMode helper declarations in ItemUpgrade.h"""
    filepath = CLIENT_DIR / "DialogBox_ItemUpgrade.h"
    content = filepath.read_text(encoding='utf-8', errors='replace')
    original = content

    # Replace each DrawMode declaration that has mouse_x, mouse_y params
    # Pattern: void DrawModeN_Name(int sX, int sY, int mouse_x, int mouse_y);
    content = re.sub(
        r'(void DrawMode\w+\(int sX, int sY), int mouse_x, int mouse_y\)',
        r'\1)',
        content
    )

    if content != original:
        changes_log.append(f"  DialogBox_ItemUpgrade.h: removed mouse params from DrawMode declarations")
        if not dry_run:
            filepath.write_text(content, encoding='utf-8', newline='\n')
        return True
    return False


def fix_item_upgrade_cpp(content, filename):
    """Remove mouse_x/mouse_y from DrawMode definitions in ItemUpgrade.cpp"""
    # Fix definitions: void DialogBox_ItemUpgrade::DrawModeN_Name(int sX, int sY, int mouse_x, int mouse_y)
    new_content = re.sub(
        r'(void DialogBox_ItemUpgrade::DrawMode\w+\(int sX, int sY), int mouse_x, int mouse_y\)',
        r'\1)',
        content
    )
    if new_content != content:
        changes_log.append(f"  {filename}: removed mouse params from DrawMode definitions")
    return new_content


def fix_sysmenu_draw_toggle_calls(content, filename):
    """Remove mouse_x/mouse_y from draw_toggle call sites in SysMenu.cpp"""
    # Call pattern: draw_toggle(x, y, enabled, mouse_x, mouse_y);
    # Should be: draw_toggle(x, y, enabled);
    new_content = re.sub(
        r'draw_toggle\(([^,]+),\s*([^,]+),\s*([^,]+),\s*mouse_x,\s*mouse_y\)',
        r'draw_toggle(\1, \2, \3)',
        content
    )
    if new_content != content:
        count = content.count('mouse_x, mouse_y)') - new_content.count('mouse_x, mouse_y)')
        changes_log.append(f"  {filename}: removed mouse_x/mouse_y from {count} draw_toggle calls")
    return new_content


def fix_auto_ref_info_access(content, filename):
    """Fix auto& ref = Info(DialogBoxId::X) patterns where ref.m_x/m_y is used.

    Pattern: auto& dropInfo = m_game->m_dialog_box_manager.Info(DialogBoxId::ItemDropExternal);
             dropInfo.m_x = ...
             dropInfo.m_y = ...
    Becomes: auto* dropDlg = m_game->m_dialog_box_manager.get_dialog(DialogBoxId::ItemDropExternal);
             auto& dropInfo = dropDlg->Info();
             dropDlg->m_x = ...
             dropDlg->m_y = ...
    """
    self_id = DIALOG_SELF_IDS.get(filename)
    lines = content.split('\n')
    new_lines = []
    # Track variable names bound to Info() refs and their dialog IDs
    info_refs = {}  # var_name -> (dialog_id, is_self)
    dlg_ptr_vars = {}  # var_name -> dlg_ptr_name
    changes_made = False

    for i, line in enumerate(lines):
        # Match: auto& varName = <prefix>Info(DialogBoxId::X);
        m = re.match(
            r'^(\s*)auto&\s+(\w+)\s*=\s*((?:m_game->)?m_dialog_box_manager\.)Info\(DialogBoxId::(\w+)\);',
            line
        )
        if m:
            indent = m.group(1)
            var_name = m.group(2)
            prefix = m.group(3)
            dialog_id = m.group(4)
            is_self = (self_id and dialog_id == self_id)

            # Check if any later line uses var_name.m_x/m_y etc
            uses_moved = False
            for j in range(i + 1, min(i + 30, len(lines))):
                for field in MOVED_FIELDS:
                    if re.search(rf'\b{var_name}\.' + field + r'\b', lines[j]):
                        uses_moved = True
                        break
                if uses_moved:
                    break

            if uses_moved and is_self:
                # Self-access: keep Info() ref for m_v* but replace m_x/m_y with self access
                info_refs[var_name] = (dialog_id, True)
                new_lines.append(line)
                continue
            elif uses_moved:
                # Cross-dialog: need a dialog pointer too
                dlg_ptr_name = var_name.replace('Info', 'Dlg').replace('info', 'Dlg')
                if dlg_ptr_name == var_name:
                    dlg_ptr_name = var_name + 'Dlg'
                info_refs[var_name] = (dialog_id, False)
                dlg_ptr_vars[var_name] = dlg_ptr_name
                # Replace with: auto* dlgPtr = prefix.get_dialog(DialogBoxId::X);
                #               auto& varName = dlgPtr->Info();
                new_lines.append(f'{indent}auto* {dlg_ptr_name} = {prefix}get_dialog(DialogBoxId::{dialog_id});')
                new_lines.append(f'{indent}auto& {var_name} = {dlg_ptr_name}->Info();')
                changes_log.append(f"  {filename}:{i+1}: split auto& {var_name} = Info(...) into dlg ptr + Info ref")
                changes_made = True
                continue

        # Replace var.m_x with self or dlg->m_x
        modified_line = line
        for var_name, (dialog_id, is_self) in info_refs.items():
            for field in MOVED_FIELDS:
                pattern = rf'\b{var_name}\.{field}\b'
                if re.search(pattern, modified_line):
                    if is_self:
                        modified_line = re.sub(pattern, field, modified_line)
                        changes_made = True
                    else:
                        dlg_ptr_name = dlg_ptr_vars[var_name]
                        modified_line = re.sub(pattern, f'{dlg_ptr_name}->{field}', modified_line)
                        changes_made = True

        new_lines.append(modified_line)

    if changes_made:
        return '\n'.join(new_lines)
    return content


def process_file(filepath, dry_run=False):
    """Process a single file with all applicable fixes."""
    content = filepath.read_text(encoding='utf-8', errors='replace')
    original = content
    filename = filepath.name

    # Fix 1: Info(DialogBoxId::X).m_field -> self or cross-dialog access
    content = fix_info_field_access(content, filename)

    # Fix 1b: auto& ref = Info(DialogBoxId::X); ref.m_x pattern
    content = fix_auto_ref_info_access(content, filename)

    # Fix 2: Remove set_order_at
    if 'set_order_at' in content:
        content = fix_set_order_at(content, filename)

    # Fix 3: Remove redefined z/lb params
    content = fix_redefined_params(content, filename)

    # Fix 4: ItemUpgrade DrawMode definitions
    if filename == 'DialogBox_ItemUpgrade.cpp':
        content = fix_item_upgrade_cpp(content, filename)

    # Fix 5: SysMenu draw_toggle calls
    if filename == 'DialogBox_SysMenu.cpp':
        content = fix_sysmenu_draw_toggle_calls(content, filename)

    if content != original:
        if not dry_run:
            filepath.write_text(content, encoding='utf-8', newline='\n')
        return True
    return False


def main():
    dry_run = '--dry-run' in sys.argv

    # All dialog .cpp files + Game.cpp + Game.Hotkeys.cpp + Screen_OnGame.cpp
    files_to_process = sorted(CLIENT_DIR.glob("DialogBox_*.cpp"))
    files_to_process += [
        CLIENT_DIR / "Game.cpp",
        CLIENT_DIR / "Game.Hotkeys.cpp",
        CLIENT_DIR / "Screen_OnGame.cpp",
    ]
    files_to_process = [f for f in files_to_process
                        if 'build' not in str(f) and 'Release_x64' not in str(f) and 'Debug_x64' not in str(f)]

    changed = 0
    for f in files_to_process:
        if f.exists() and process_file(f, dry_run):
            changed += 1

    # Fix ItemUpgrade.h separately
    if fix_item_upgrade_header(dry_run):
        changed += 1

    prefix = 'DRY RUN: ' if dry_run else ''
    print(f"{prefix}{changed} files changed")
    if changes_log:
        print("\nChanges:")
        for line in changes_log:
            print(line)


if __name__ == '__main__':
    main()
