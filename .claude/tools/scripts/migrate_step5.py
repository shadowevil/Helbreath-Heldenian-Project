"""
Phase 2.1 Step 5: Migrate default CommandCommon send_command calls to caller-side packet construction.

Usage:
    python Scripts/migrate_step5.py --dry-run    # Preview changes
    python Scripts/migrate_step5.py --verify      # Verify no collisions
    python Scripts/migrate_step5.py               # Apply changes

Each send_command(MsgId::CommandCommon, ...) call is replaced with:
- make_common_command() for no-text calls (PacketCommandCommonWithTime)
- make_common_command_str() for text calls (PacketCommandCommonWithString)
"""

import re
import os
import sys
import copy

DRY_RUN = '--dry-run' in sys.argv
VERIFY = '--verify' in sys.argv
ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), '..', 'Sources', 'Client'))
OUTPUT_DIR = os.path.join(os.path.dirname(__file__), 'output')

# ------- Caller context detection -------
# Determines how to access player coordinates and which send function to use.

# Files that are IDialogBox subclasses (use send_command directly, player() accessor)
DIALOG_FILES = {
    'DialogBox_Bank.cpp', 'DialogBox_Character.cpp', 'DialogBox_Commander.cpp',
    'DialogBox_ConfirmExchange.cpp', 'DialogBox_Constructor.cpp',
    'DialogBox_Exchange.cpp', 'DialogBox_GuildHallMenu.cpp',
    'DialogBox_GuildMenu.cpp', 'DialogBox_GuildOperation.cpp',
    'DialogBox_Inventory.cpp', 'DialogBox_ItemCreator.cpp',
    'DialogBox_ItemDrop.cpp', 'DialogBox_NpcActionQuery.cpp',
    'DialogBox_Party.cpp', 'DialogBox_RepairAll.cpp',
    'DialogBox_Skill.cpp', 'DialogBox_Soldier.cpp',
    'DialogBox_TesterMenu.cpp',
}

# Files that use m_game->send_command (IGameScreen subclasses, managers with m_game member)
MGAME_FILES = {
    'DialogBox_CityHallMenu.cpp', 'DialogBox_CrusadeJob.cpp',
    'DialogBox_Fishing.cpp', 'DialogBox_HudPanel.cpp',
    'DialogBox_ItemUpgrade.cpp', 'DialogBox_MagicShop.cpp',
    'DialogBox_NpcTalk.cpp', 'DialogBox_SellOrRepair.cpp',
    'DialogBox_Shop.cpp', 'Screen_OnGame.cpp',
    'InventoryManager.cpp', 'MagicCastingSystem.cpp',
}

# Files that use game->send_command (standalone functions with game* parameter)
GAME_PTR_FILES = {
    'NetworkMessages_Party.cpp',
}

# Game.cpp uses send_command directly (member function of CGame)
SELF_FILES = {
    'Game.cpp', 'Game.Hotkeys.cpp',
}


def get_context(filename):
    """Returns (player_x, player_y, send_fn) based on file type."""
    base = os.path.basename(filename)
    if base in DIALOG_FILES:
        return 'player().m_player_x', 'player().m_player_y', 'send_game_packet'
    elif base in MGAME_FILES:
        return 'm_game->m_player->m_player_x', 'm_game->m_player->m_player_y', 'm_game->send_game_packet'
    elif base in GAME_PTR_FILES:
        return 'game->m_player->m_player_x', 'game->m_player->m_player_y', 'game->send_game_packet'
    elif base in SELF_FILES:
        return 'm_player->m_player_x', 'm_player->m_player_y', 'send_game_packet'
    else:
        raise ValueError(f"Unknown file context: {base}")


def split_args(text):
    """Split a comma-separated argument string respecting parentheses and quotes."""
    args = []
    depth = 0
    current = []
    in_string = False
    escape = False
    for ch in text:
        if escape:
            current.append(ch)
            escape = False
            continue
        if ch == '\\':
            current.append(ch)
            escape = True
            continue
        if ch == '"' and depth == 0:
            in_string = not in_string
            current.append(ch)
            continue
        if in_string:
            current.append(ch)
            continue
        if ch == '(':
            depth += 1
            current.append(ch)
        elif ch == ')':
            depth -= 1
            current.append(ch)
        elif ch == ',' and depth == 0:
            args.append(''.join(current).strip())
            current = []
        else:
            current.append(ch)
    if current:
        args.append(''.join(current).strip())
    return args


def extract_call(lines, start_line_idx):
    """Extract a complete send_command(...) call that may span multiple lines.
    Returns (full_text, end_line_idx, leading_whitespace, prefix_text, suffix_text).
    prefix_text is everything before send_command on the first line.
    suffix_text is everything after the closing ); on the last line.
    """
    # Find the send_command portion
    line = lines[start_line_idx]
    # Match various prefixes: send_command, m_game->send_command, game->send_command
    # Also handle: if (cfg) send_command, else if (cfg) m_game->send_command, etc.
    match = re.search(r'((?:\w+->)?send_command\s*\()', line)
    if not match:
        return None

    call_start = match.start(1)
    prefix_text = line[:call_start]

    # Now extract from the opening paren to the matching close
    text = line[match.start(1):]
    depth = 0
    full = []
    end_idx = start_line_idx
    found_end = False

    for li in range(start_line_idx, min(start_line_idx + 10, len(lines))):
        if li == start_line_idx:
            segment = line[match.start(1):]
        else:
            segment = lines[li]

        for i, ch in enumerate(segment):
            full.append(ch)
            if ch == '(':
                depth += 1
            elif ch == ')':
                depth -= 1
                if depth == 0:
                    end_idx = li
                    # Everything after the closing paren on this line
                    rest = segment[i+1:].strip()
                    if rest.startswith(';'):
                        rest = rest[1:].strip()
                    suffix_text = rest
                    found_end = True
                    break
        if found_end:
            break
        if li > start_line_idx:
            full.append('\n')

    if not found_end:
        return None

    full_text = ''.join(full)
    # Get the leading whitespace (indent) from the first line
    leading_ws = ''
    for ch in line:
        if ch in (' ', '\t'):
            leading_ws += ch
        else:
            break

    return full_text, end_idx, leading_ws, prefix_text.rstrip(), suffix_text


def is_null_text(arg):
    """Check if a text argument represents null/no text."""
    stripped = arg.strip()
    return stripped in ('0', 'nullptr', 'NULL', '""')


def is_zero(arg):
    """Check if an argument is literally zero."""
    return arg.strip() == '0'


def generate_replacement(args, player_x, player_y, send_fn, indent, prefix_text):
    """Generate replacement code for a send_command(MsgId::CommandCommon, ...) call.

    args: [msg_id, command, dir, v1, v2, v3, text, v4?]
    """
    command = args[1].strip()  # e.g. CommonType::Foo
    direction = args[2].strip()
    v1 = args[3].strip()
    v2 = args[4].strip()
    v3 = args[5].strip()
    text_arg = args[6].strip()
    v4 = args[7].strip() if len(args) > 7 else '0'

    has_text = not is_null_text(text_arg)
    has_dir = not is_zero(direction)
    has_v4 = not is_zero(v4)

    lines = []

    # Handle prefix (e.g., "if (cfg) " or "else ")
    # If there's a conditional prefix, we need a block
    needs_block = bool(prefix_text.strip())
    clean_prefix = prefix_text.strip()

    if has_text:
        # PacketCommandCommonWithString
        if has_dir:
            maker = f'hb::net::make_common_command_str({command}, {player_x}, {player_y}, {direction})'
        else:
            maker = f'hb::net::make_common_command_str({command}, {player_x}, {player_y})'

        # Always use braces for multi-statement replacements (avoids switch-case redefinition)
        if needs_block:
            lines.append(f'{indent}{clean_prefix}')
        lines.append(f'{indent}{{')
        inner = indent + '\t'

        lines.append(f'{inner}auto pkt = {maker};')
        if not is_zero(v1):
            lines.append(f'{inner}pkt.v1 = {v1};')
        if not is_zero(v2):
            lines.append(f'{inner}pkt.v2 = {v2};')
        if not is_zero(v3):
            lines.append(f'{inner}pkt.v3 = {v3};')
        lines.append(f'{inner}std::snprintf(pkt.text, sizeof(pkt.text), "%s", {text_arg});')
        if has_v4:
            lines.append(f'{inner}pkt.v4 = {v4};')
        lines.append(f'{inner}{send_fn}(pkt);')
        lines.append(f'{indent}}}')
    else:
        # PacketCommandCommonWithTime
        if has_dir:
            maker = f'hb::net::make_common_command({command}, {player_x}, {player_y}, {direction})'
        else:
            maker = f'hb::net::make_common_command({command}, {player_x}, {player_y})'

        # For simple cases with all zeros, just make + send (single statement, no braces needed)
        all_v_zero = is_zero(v1) and is_zero(v2) and is_zero(v3)

        if needs_block and all_v_zero:
            lines.append(f'{indent}{clean_prefix} {send_fn}(hb::net::make_common_command({command}, {player_x}, {player_y}));')
        elif all_v_zero:
            lines.append(f'{indent}{send_fn}({maker});')
        else:
            # Multi-statement: always use braces
            if needs_block:
                lines.append(f'{indent}{clean_prefix}')
            lines.append(f'{indent}{{')
            inner = indent + '\t'
            lines.append(f'{inner}auto pkt = {maker};')
            if not is_zero(v1):
                lines.append(f'{inner}pkt.v1 = {v1};')
            if not is_zero(v2):
                lines.append(f'{inner}pkt.v2 = {v2};')
            if not is_zero(v3):
                lines.append(f'{inner}pkt.v3 = {v3};')
            lines.append(f'{inner}{send_fn}(pkt);')
            lines.append(f'{indent}}}')


    return lines


def needs_include(lines, include_name):
    """Check if a file already has a specific include."""
    for line in lines:
        if include_name in line:
            return False
    return True


def add_include_after(lines, after_pattern, new_include):
    """Add an #include line after the last line matching a pattern."""
    last_idx = -1
    for i, line in enumerate(lines):
        if after_pattern in line:
            last_idx = i
    if last_idx >= 0:
        lines.insert(last_idx + 1, new_include + '\n')
        return True
    return False


def process_file(filepath):
    """Process a single file, returning (changes_log, new_lines) or None if no changes."""
    filename = os.path.basename(filepath)
    with open(filepath, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    # Strip newlines for processing, we'll add them back
    raw_lines = [l.rstrip('\n').rstrip('\r') for l in lines]

    player_x, player_y, send_fn = get_context(filepath)
    changes = []
    skip_until = -1

    # Find all send_command(MsgId::CommandCommon, ...) calls
    new_lines = list(raw_lines)
    offset = 0  # Track line insertions/deletions

    i = 0
    while i < len(new_lines):
        line = new_lines[i]
        if 'send_command' not in line or 'MsgId::CommandCommon' not in line:
            i += 1
            continue

        # Extract the full call
        result = extract_call(new_lines, i)
        if result is None:
            i += 1
            continue

        full_text, end_idx, leading_ws, prefix_text, suffix_text = result

        # Extract arguments from the call
        # Remove the "send_command(" prefix and trailing ")"
        inner_match = re.search(r'send_command\s*\((.*)\)', full_text, re.DOTALL)
        if not inner_match:
            i += 1
            continue

        args_text = inner_match.group(1)
        args = split_args(args_text)

        if len(args) < 7:
            print(f"  WARNING: {filename}:{i+1}: Only {len(args)} args, skipping: {full_text[:80]}")
            i += 1
            continue

        # Verify first arg is MsgId::CommandCommon
        if 'CommandCommon' not in args[0]:
            i += 1
            continue

        # Generate replacement
        replacement = generate_replacement(args, player_x, player_y, send_fn, leading_ws, prefix_text)

        # Record the change
        old_lines = new_lines[i:end_idx+1]
        changes.append({
            'line': i + 1,
            'old': old_lines,
            'new': replacement,
        })

        # Replace lines
        new_lines[i:end_idx+1] = replacement
        # Adjust i to skip past replacement
        i += len(replacement)

    if not changes:
        return None

    # Add includes if needed
    include_added = False
    if needs_include(new_lines, 'PacketSendHelpers.h'):
        # Add after the last Packet include, or after Game.h / IDialogBox.h
        for pattern in ['Packet/', 'SharedPackets.h', 'NetMessages.h', 'Game.h']:
            if add_include_after(new_lines, pattern, '#include "PacketSendHelpers.h"'):
                include_added = True
                break

    return changes, new_lines, include_added


def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    log_path = os.path.join(OUTPUT_DIR, 'migrate_step5.log')

    all_files = set()
    for s in [DIALOG_FILES, MGAME_FILES, GAME_PTR_FILES, SELF_FILES]:
        all_files.update(s)

    total_changes = 0
    total_files = 0
    log_lines = []

    for filename in sorted(all_files):
        filepath = os.path.join(ROOT, filename)
        if not os.path.exists(filepath):
            log_lines.append(f"SKIP {filename}: file not found")
            continue

        result = process_file(filepath)
        if result is None:
            continue

        changes, new_lines, include_added = result
        total_files += 1
        total_changes += len(changes)

        log_lines.append(f"\n{'='*60}")
        log_lines.append(f"FILE: {filename} ({len(changes)} changes)")
        if include_added:
            log_lines.append(f"  + Added #include \"PacketSendHelpers.h\"")
        log_lines.append(f"{'='*60}")

        for c in changes:
            log_lines.append(f"\n  Line {c['line']}:")
            log_lines.append(f"  OLD:")
            for ol in c['old']:
                log_lines.append(f"    {ol}")
            log_lines.append(f"  NEW:")
            for nl in c['new']:
                log_lines.append(f"    {nl}")

        if not DRY_RUN and not VERIFY:
            with open(filepath, 'w', encoding='utf-8', newline='\n') as f:
                for line in new_lines:
                    f.write(line + '\n')

    # Summary
    summary = f"\nTotal: {total_changes} changes across {total_files} files"
    log_lines.append(summary)
    print(summary)

    with open(log_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(log_lines))
    print(f"Log written to: {log_path}")

    if DRY_RUN:
        print("DRY RUN — no files modified.")
    elif VERIFY:
        print("VERIFY — no files modified.")


if __name__ == '__main__':
    main()
