"""
migrate_step6.py -- Replace send_command calls for motion and chat with direct packet construction.

Motion calls in Game.cpp -> send_game_packet(hb::net::make_motion*(...)) + increment_command_count()
Chat calls in Game.Hotkeys.cpp and Screen_OnGame.cpp -> send_chat_message(...)

Usage:
    python Scripts/migrate_step6.py --dry-run    # Preview changes
    python Scripts/migrate_step6.py --verify     # Verify no issues
    python Scripts/migrate_step6.py              # Apply changes
"""

import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUTPUT_DIR = ROOT / "Scripts" / "output"

GAME_CPP = ROOT / "Sources" / "Client" / "Game.cpp"
HOTKEYS_CPP = ROOT / "Sources" / "Client" / "Game.Hotkeys.cpp"
SCREEN_ONGAME_CPP = ROOT / "Sources" / "Client" / "Screen_OnGame.cpp"

# Game.cpp motion replacements
# Each entry: (line_number, old_text, new_text)
GAME_REPLACEMENTS = [
    (
        5244,
        "\t\t\t\tsend_command(MsgId::CommandMotion, Type::stop, move_dir, 0, 0, 0, 0);",
        "\t\t\t\tsend_game_packet(hb::net::make_motion(Type::stop, m_player->m_player_x, m_player->m_player_y, move_dir));\n"
        "\t\t\t\tm_player->m_Controller.increment_command_count();",
    ),
    (
        6174,
        "\t\t\tsend_command(MsgId::CommandMotion, Type::stop, m_player->m_player_dir, 0, 0, 0, 0);",
        "\t\t\tsend_game_packet(hb::net::make_motion(Type::stop, m_player->m_player_x, m_player->m_player_y, m_player->m_player_dir));\n"
        "\t\t\tm_player->m_Controller.increment_command_count();",
    ),
    (
        6275,
        "\t\t\t\t\tsend_command(MsgId::CommandMotion, Type::stop, pending_dir, 0, 0, 0, 0);",
        "\t\t\t\t\tsend_game_packet(hb::net::make_motion(Type::stop, m_player->m_player_x, m_player->m_player_y, pending_dir));\n"
        "\t\t\t\t\tm_player->m_Controller.increment_command_count();",
    ),
    (
        6316,
        "\t\t\t\t\tsend_command(MsgId::CommandMotion, m_player->m_Controller.get_command(), move_dir, 0, 0, 0, 0);",
        "\t\t\t\t\tsend_game_packet(hb::net::make_motion(m_player->m_Controller.get_command(), m_player->m_player_x, m_player->m_player_y, move_dir));\n"
        "\t\t\t\t\tm_player->m_Controller.increment_command_count();",
    ),
    (
        6371,
        "\t\t\t\tsend_command(MsgId::CommandMotion, Type::Attack, move_dir, m_player->m_Controller.get_destination_x(), m_player->m_Controller.get_destination_y(), action_type, 0, m_comm_object_id);",
        "\t\t\t\tsend_game_packet(hb::net::make_motion_attack(Type::Attack, m_player->m_player_x, m_player->m_player_y, move_dir, static_cast<int16_t>(m_player->m_Controller.get_destination_x()), static_cast<int16_t>(m_player->m_Controller.get_destination_y()), static_cast<int16_t>(action_type), static_cast<uint16_t>(m_comm_object_id)));\n"
        "\t\t\t\tm_player->m_Controller.increment_command_count();",
    ),
    (
        6421,
        "\t\t\t\t\tsend_command(MsgId::CommandMotion, Type::AttackMove, move_dir, m_player->m_Controller.get_destination_x(), m_player->m_Controller.get_destination_y(), action_type, 0, m_comm_object_id);",
        "\t\t\t\t\tsend_game_packet(hb::net::make_motion_attack(Type::AttackMove, m_player->m_player_x, m_player->m_player_y, move_dir, static_cast<int16_t>(m_player->m_Controller.get_destination_x()), static_cast<int16_t>(m_player->m_Controller.get_destination_y()), static_cast<int16_t>(action_type), static_cast<uint16_t>(m_comm_object_id)));\n"
        "\t\t\t\t\tm_player->m_Controller.increment_command_count();",
    ),
    (
        6465,
        "\t\t\tsend_command(MsgId::CommandMotion, Type::GetItem, m_player->m_player_dir, 0, 0, 0, 0);",
        "\t\t\tsend_game_packet(hb::net::make_motion(Type::GetItem, m_player->m_player_x, m_player->m_player_y, m_player->m_player_dir));\n"
        "\t\t\tm_player->m_Controller.increment_command_count();",
    ),
    (
        6482,
        "\t\t\tsend_command(MsgId::CommandMotion, Type::Magic, m_player->m_player_dir, m_casting_magic_type, 0, 0, 0);",
        "\t\t\tsend_game_packet(hb::net::make_motion(Type::Magic, m_player->m_player_x, m_player->m_player_y, m_player->m_player_dir, static_cast<int16_t>(m_casting_magic_type)));\n"
        "\t\t\tm_player->m_Controller.increment_command_count();",
    ),
]

# Game.Hotkeys.cpp chat replacements
HOTKEYS_REPLACEMENTS = [
    (
        219,
        "\t\tsend_command(MsgId::CommandChatMsg, 0, 0, 0, 0, 0, tempid.c_str());",
        "\t\tsend_chat_message(tempid.c_str());",
    ),
    (
        225,
        "\t\tsend_command(MsgId::CommandChatMsg, 0, 0, 0, 0, 0, tempid.c_str());",
        "\t\tsend_chat_message(tempid.c_str());",
    ),
]

# Screen_OnGame.cpp chat replacement (uses spaces, not tabs)
SCREEN_ONGAME_REPLACEMENTS = [
    (
        389,
        "                            m_game->send_command(MsgId::CommandChatMsg, 0, 0, 0, 0, 0, G_cTxt.c_str());",
        "                            m_game->send_chat_message(G_cTxt.c_str());",
    ),
]


def apply_replacements(filepath, replacements, dry_run):
    """Apply line-based replacements to a file. Returns list of change descriptions."""
    lines = filepath.read_text(encoding="utf-8", newline="").split("\n")
    changes = []
    # Track line offset caused by replacements that expand 1 line into 2+ lines
    offset = 0

    for orig_lineno, old_text, new_text in replacements:
        idx = orig_lineno - 1 + offset
        actual_line = lines[idx] if idx < len(lines) else None

        if actual_line is None:
            changes.append(f"  ERROR: Line {orig_lineno} (adjusted {idx+1}) out of range (file has {len(lines)} lines)")
            continue

        if actual_line != old_text:
            changes.append(f"  ERROR: Line {orig_lineno} (adjusted {idx+1}) does not match expected text.")
            changes.append(f"    Expected: {repr(old_text)}")
            changes.append(f"    Actual:   {repr(actual_line)}")
            continue

        new_lines = new_text.split("\n")
        lines[idx:idx+1] = new_lines
        added = len(new_lines) - 1
        offset += added

        changes.append(f"  Line {orig_lineno} (adjusted {idx+1}): replaced 1 line with {len(new_lines)} line(s)")
        changes.append(f"    - {old_text.strip()}")
        for nl in new_lines:
            changes.append(f"    + {nl.strip()}")

    if not dry_run and changes and not any("ERROR" in c for c in changes):
        filepath.write_text("\n".join(lines), encoding="utf-8", newline="")

    return changes


def verify_no_remaining(dry_run):
    """Verify that no send_command(MsgId::CommandMotion/ChatMsg) calls remain in target files."""
    issues = []

    if not dry_run:
        game_text = GAME_CPP.read_text(encoding="utf-8")
        for line_idx, line in enumerate(game_text.split("\n"), 1):
            if "send_command(MsgId::CommandMotion" in line:
                issues.append(f"  WARN: Game.cpp:{line_idx} still has CommandMotion send_command: {line.strip()}")

        hotkeys_text = HOTKEYS_CPP.read_text(encoding="utf-8")
        for line_idx, line in enumerate(hotkeys_text.split("\n"), 1):
            if "send_command(MsgId::CommandChatMsg" in line:
                issues.append(f"  WARN: Game.Hotkeys.cpp:{line_idx} still has CommandChatMsg send_command: {line.strip()}")

        screen_text = SCREEN_ONGAME_CPP.read_text(encoding="utf-8")
        for line_idx, line in enumerate(screen_text.split("\n"), 1):
            if "send_command(MsgId::CommandChatMsg" in line:
                issues.append(f"  WARN: Screen_OnGame.cpp:{line_idx} still has CommandChatMsg send_command: {line.strip()}")

    return issues


def main():
    parser = argparse.ArgumentParser(description="Step 6: Replace send_command motion/chat calls with direct packet construction")
    parser.add_argument("--dry-run", action="store_true", help="Preview changes without modifying files")
    parser.add_argument("--verify", action="store_true", help="Verify no leftover send_command calls after applying")
    args = parser.parse_args()

    dry_run = args.dry_run
    mode = "DRY RUN" if dry_run else ("VERIFY" if args.verify else "APPLY")
    print(f"=== migrate_step6.py [{mode}] ===\n")

    # Validate files exist
    for f in [GAME_CPP, HOTKEYS_CPP, SCREEN_ONGAME_CPP]:
        if not f.exists():
            print(f"FATAL: File not found: {f}")
            sys.exit(1)

    all_ok = True
    log_lines = [f"=== migrate_step6.py [{mode}] ===", ""]

    # --- Game.cpp motion replacements ---
    print(f"Game.cpp - {len(GAME_REPLACEMENTS)} motion replacements:")
    log_lines.append(f"Game.cpp - {len(GAME_REPLACEMENTS)} motion replacements:")
    changes = apply_replacements(GAME_CPP, GAME_REPLACEMENTS, dry_run)
    for c in changes:
        print(c)
        log_lines.append(c)
        if "ERROR" in c:
            all_ok = False
    print()
    log_lines.append("")

    # --- Game.Hotkeys.cpp chat replacements ---
    print(f"Game.Hotkeys.cpp - {len(HOTKEYS_REPLACEMENTS)} chat replacements:")
    log_lines.append(f"Game.Hotkeys.cpp - {len(HOTKEYS_REPLACEMENTS)} chat replacements:")
    changes = apply_replacements(HOTKEYS_CPP, HOTKEYS_REPLACEMENTS, dry_run)
    for c in changes:
        print(c)
        log_lines.append(c)
        if "ERROR" in c:
            all_ok = False
    print()
    log_lines.append("")

    # --- Screen_OnGame.cpp chat replacement ---
    print(f"Screen_OnGame.cpp - {len(SCREEN_ONGAME_REPLACEMENTS)} chat replacement:")
    log_lines.append(f"Screen_OnGame.cpp - {len(SCREEN_ONGAME_REPLACEMENTS)} chat replacement:")
    changes = apply_replacements(SCREEN_ONGAME_CPP, SCREEN_ONGAME_REPLACEMENTS, dry_run)
    for c in changes:
        print(c)
        log_lines.append(c)
        if "ERROR" in c:
            all_ok = False
    print()
    log_lines.append("")

    # --- Verify (only when --verify or after apply) ---
    if args.verify or (not dry_run and not args.verify):
        issues = verify_no_remaining(dry_run)
        if issues:
            print("Verification issues:")
            log_lines.append("Verification issues:")
            for issue in issues:
                print(issue)
                log_lines.append(issue)
            all_ok = False
        else:
            msg = "Verification: OK (no leftover send_command calls in target files)" if not dry_run else "Verification: skipped in dry-run"
            print(msg)
            log_lines.append(msg)
    print()
    log_lines.append("")

    # Summary
    total = len(GAME_REPLACEMENTS) + len(HOTKEYS_REPLACEMENTS) + len(SCREEN_ONGAME_REPLACEMENTS)
    summary = f"Total: {total} replacements across 3 files."
    status = "All replacements matched." if all_ok else "ERRORS detected - review above."
    print(summary)
    print(status)
    log_lines.extend([summary, status])

    # Write log
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    log_path = OUTPUT_DIR / "migrate_step6.log"
    log_path.write_text("\n".join(log_lines), encoding="utf-8")
    print(f"\nLog written to: {log_path}")

    sys.exit(0 if all_ok else 1)


if __name__ == "__main__":
    main()
