#!/usr/bin/env python3
"""Step 3: Migrate unique game server sends from send_command to caller-side packet construction.

Each migration replaces a send_command(MsgId, ...) call with direct packet construction
and send_game_packet() call.

Usage:
    python Scripts/migrate_step3.py --dry-run   # Preview changes
    python Scripts/migrate_step3.py             # Apply changes
"""

import re
import sys
import os
from pathlib import Path

DRY_RUN = "--dry-run" in sys.argv
ROOT = Path("Z:/Helbreath-3.82/Sources/Client")
OUTPUT_DIR = Path("Z:/Helbreath-3.82/Scripts/output")
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

changes = []  # list of (file, line_num, old_text, new_text) for reporting

def replace_in_file(filepath, old, new, description=""):
    """Replace exact text in file. Returns True if replacement was made."""
    content = filepath.read_text(encoding='utf-8')
    if old not in content:
        print(f"  WARNING: Pattern not found in {filepath.name}: {description}")
        return False
    count = content.count(old)
    if count > 1:
        print(f"  WARNING: Pattern found {count} times in {filepath.name}: {description}")
        return False
    if DRY_RUN:
        line_num = content[:content.index(old)].count('\n') + 1
        changes.append((filepath.name, line_num, old.strip()[:80], new.strip()[:80]))
        print(f"  [DRY-RUN] {filepath.name}:{line_num} — {description}")
        return True
    new_content = content.replace(old, new, 1)
    filepath.write_text(new_content, encoding='utf-8')
    line_num = content[:content.index(old)].count('\n') + 1
    changes.append((filepath.name, line_num, old.strip()[:80], new.strip()[:80]))
    print(f"  Applied: {filepath.name}:{line_num} — {description}")
    return True

def ensure_include(filepath, include_line):
    """Ensure a file has the given #include line. Adds after last existing #include if missing."""
    content = filepath.read_text(encoding='utf-8')
    if include_line in content:
        return  # already present
    # Find last #include line
    lines = content.split('\n')
    last_include_idx = -1
    for i, line in enumerate(lines):
        if line.strip().startswith('#include'):
            last_include_idx = i
    if last_include_idx >= 0:
        lines.insert(last_include_idx + 1, include_line)
        if DRY_RUN:
            print(f"  [DRY-RUN] Would add '{include_line}' to {filepath.name}")
        else:
            filepath.write_text('\n'.join(lines), encoding='utf-8')
            print(f"  Added include: {include_line} to {filepath.name}")


# ============================================================================
# DialogBox_Resurrect.cpp — RequestResurrectYes/No → PacketRequestHeaderOnly
# ============================================================================
f = ROOT / "DialogBox_Resurrect.cpp"
ensure_include(f, '#include "Packet/SharedPackets.h"')

replace_in_file(f,
    'send_command(MsgId::RequestResurrectYes, 0, 0, 0, 0, 0, nullptr, 0);',
    '{\n\t\t\thb::net::PacketRequestHeaderOnly req{};\n\t\t\treq.header.msg_id = MsgId::RequestResurrectYes;\n\t\t\treq.header.msg_type = 0;\n\t\t\tsend_game_packet(req);\n\t\t}',
    "RequestResurrectYes")

replace_in_file(f,
    'send_command(MsgId::RequestResurrectNo, 0, 0, 0, 0, 0, nullptr, 0);',
    '{\n\t\t\thb::net::PacketRequestHeaderOnly req{};\n\t\t\treq.header.msg_id = MsgId::RequestResurrectNo;\n\t\t\treq.header.msg_type = 0;\n\t\t\tsend_game_packet(req);\n\t\t}',
    "RequestResurrectNo")


# ============================================================================
# DialogBox_LevelUpSetting.cpp — LevelUpSettings → PacketRequestLevelUpSettings
# ============================================================================
f = ROOT / "DialogBox_LevelUpSetting.cpp"
ensure_include(f, '#include "Packet/SharedPackets.h"')

replace_in_file(f,
    'send_command(MsgId::LevelUpSettings, 0, 0, 0, 0, 0, 0);',
    '{\n\t\thb::net::PacketRequestLevelUpSettings req{};\n\t\treq.header.msg_id = MsgId::LevelUpSettings;\n\t\treq.header.msg_type = 0;\n\t\treq.str = player().m_lu_str;\n\t\treq.vit = player().m_lu_vit;\n\t\treq.dex = player().m_lu_dex;\n\t\treq.intel = player().m_lu_int;\n\t\treq.mag = player().m_lu_mag;\n\t\treq.chr = player().m_lu_char;\n\t\tsend_game_packet(req);\n\t}',
    "LevelUpSettings")


# ============================================================================
# DialogBox_Bank.cpp — RequestRetrieveItem → PacketRequestRetrieveItem
# ============================================================================
f = ROOT / "DialogBox_Bank.cpp"
ensure_include(f, '#include "Packet/SharedPackets.h"')

replace_in_file(f,
    'send_command(MsgId::RequestRetrieveItem, 0, 0, itemIndex, 0, 0, 0);',
    '{\n\t\t\thb::net::PacketRequestRetrieveItem req{};\n\t\t\treq.header.msg_id = MsgId::RequestRetrieveItem;\n\t\t\treq.header.msg_type = MsgType::Confirm;\n\t\t\treq.item_slot = static_cast<uint8_t>(itemIndex);\n\t\t\tsend_game_packet(req);\n\t\t}',
    "RequestRetrieveItem")


# ============================================================================
# DialogBox_ChangeStatsMajestic.cpp — StateChangePoint → PacketRequestStateChange
# ============================================================================
f = ROOT / "DialogBox_ChangeStatsMajestic.cpp"
ensure_include(f, '#include "Packet/SharedPackets.h"')

replace_in_file(f,
    'send_command(MsgId::StateChangePoint, 0, 0, 0, 0, 0, 0);',
    '{\n\t\thb::net::PacketRequestStateChange req{};\n\t\treq.header.msg_id = MsgId::StateChangePoint;\n\t\treq.header.msg_type = 0;\n\t\treq.str = static_cast<int16_t>(-player().m_lu_str);\n\t\treq.vit = static_cast<int16_t>(-player().m_lu_vit);\n\t\treq.dex = static_cast<int16_t>(-player().m_lu_dex);\n\t\treq.intel = static_cast<int16_t>(-player().m_lu_int);\n\t\treq.mag = static_cast<int16_t>(-player().m_lu_mag);\n\t\treq.chr = static_cast<int16_t>(-player().m_lu_char);\n\t\tsend_game_packet(req);\n\t}',
    "StateChangePoint")


# ============================================================================
# Screen_OnGame.cpp — RequestRestart → PacketRequestHeaderOnly
# ============================================================================
f = ROOT / "Screen_OnGame.cpp"
ensure_include(f, '#include "Packet/SharedPackets.h"')

replace_in_file(f,
    "m_game->send_command(MsgId::RequestRestart, 0, 0, 0, 0, 0, 0);",
    '{\n\t\t\thb::net::PacketRequestHeaderOnly req{};\n\t\t\treq.header.msg_id = MsgId::RequestRestart;\n\t\t\treq.header.msg_type = 0;\n\t\t\tm_game->send_game_packet(req);\n\t\t}',
    "RequestRestart")


# ============================================================================
# Screen_OnGame.cpp — request_create_new_guild
# ============================================================================
replace_in_file(f,
    "m_game->send_command(MsgId::request_create_new_guild, MsgType::Confirm, 0, 0, 0, 0, 0);",
    '{\n\t\t\thb::net::PacketRequestGuildAction req{};\n\t\t\treq.header.msg_id = MsgId::request_create_new_guild;\n\t\t\treq.header.msg_type = MsgType::Confirm;\n\t\t\tstd::snprintf(req.player, sizeof(req.player), "%s", m_game->m_player->m_player_name.c_str());\n\t\t\tstd::snprintf(req.account, sizeof(req.account), "%s", m_game->m_account_name.c_str());\n\t\t\tstd::snprintf(req.password, sizeof(req.password), "%s", m_game->m_account_password.c_str());\n\t\t\tstd::snprintf(req.guild, sizeof(req.guild), "%s", m_game->m_player->m_guild_name.c_str());\n\t\t\tCMisc::replace_string(req.guild, \' \', \'_\');\n\t\t\tm_game->send_game_packet(req);\n\t\t}',
    "request_create_new_guild (Screen_OnGame)")


# ============================================================================
# DialogBox_GuildMenu.cpp — request_create_new_guild, request_disband_guild, RequestFightZoneReserve
# ============================================================================
f = ROOT / "DialogBox_GuildMenu.cpp"
ensure_include(f, '#include "Packet/SharedPackets.h"')

replace_in_file(f,
    "send_command(MsgId::request_create_new_guild, MsgType::Confirm, 0, 0, 0, 0, 0);",
    '{\n\t\t\thb::net::PacketRequestGuildAction req{};\n\t\t\treq.header.msg_id = MsgId::request_create_new_guild;\n\t\t\treq.header.msg_type = MsgType::Confirm;\n\t\t\tstd::snprintf(req.player, sizeof(req.player), "%s", player().m_player_name.c_str());\n\t\t\tstd::snprintf(req.account, sizeof(req.account), "%s", m_game->m_account_name.c_str());\n\t\t\tstd::snprintf(req.password, sizeof(req.password), "%s", m_game->m_account_password.c_str());\n\t\t\tstd::snprintf(req.guild, sizeof(req.guild), "%s", player().m_guild_name.c_str());\n\t\t\tCMisc::replace_string(req.guild, \' \', \'_\');\n\t\t\tsend_game_packet(req);\n\t\t}',
    "request_create_new_guild (GuildMenu)")

replace_in_file(f,
    "send_command(MsgId::request_disband_guild, MsgType::Confirm, 0, 0, 0, 0, 0);",
    '{\n\t\t\thb::net::PacketRequestGuildAction req{};\n\t\t\treq.header.msg_id = MsgId::request_disband_guild;\n\t\t\treq.header.msg_type = MsgType::Confirm;\n\t\t\tstd::snprintf(req.player, sizeof(req.player), "%s", player().m_player_name.c_str());\n\t\t\tstd::snprintf(req.account, sizeof(req.account), "%s", m_game->m_account_name.c_str());\n\t\t\tstd::snprintf(req.password, sizeof(req.password), "%s", m_game->m_account_password.c_str());\n\t\t\tstd::snprintf(req.guild, sizeof(req.guild), "%s", player().m_guild_name.c_str());\n\t\t\tCMisc::replace_string(req.guild, \' \', \'_\');\n\t\t\tsend_game_packet(req);\n\t\t}',
    "request_disband_guild (GuildMenu)")


# ============================================================================
# DialogBox_GuildMenu.cpp — RequestFightZoneReserve (8 calls with different zone IDs)
# ============================================================================
for zone_id in range(1, 9):
    replace_in_file(f,
        f"send_command(MsgId::RequestFightZoneReserve, 0, 0, {zone_id}, 0, 0, 0);",
        f'{{\n\t\t\t\thb::net::PacketRequestFightzoneReserve req{{}};\n\t\t\t\treq.header.msg_id = MsgId::RequestFightZoneReserve;\n\t\t\t\treq.header.msg_type = 0;\n\t\t\t\treq.fightzone = {zone_id};\n\t\t\t\tsend_game_packet(req);\n\t\t\t}}',
        f"RequestFightZoneReserve zone={zone_id}")


# ============================================================================
# TeleportManager.cpp — RequestTeleport, RequestTeleportAuth
# ============================================================================
f = ROOT / "TeleportManager.cpp"
ensure_include(f, '#include "Packet/SharedPackets.h"')

replace_in_file(f,
    "m_game->send_command(MsgId::RequestTeleportAuth, 0, 0, 0, 0, 0, 0);",
    '{\n\t\thb::net::PacketRequestHeaderOnly req{};\n\t\treq.header.msg_id = MsgId::RequestTeleportAuth;\n\t\treq.header.msg_type = 0;\n\t\tm_game->send_game_packet(req);\n\t}',
    "RequestTeleportAuth")

replace_in_file(f,
    "m_game->send_command(MsgId::RequestTeleport, 0, 0, 0, 0, 0, 0);",
    '{\n\t\thb::net::PacketRequestHeaderOnly req{};\n\t\treq.header.msg_id = MsgId::RequestTeleport;\n\t\treq.header.msg_type = MsgType::Confirm;\n\t\tm_game->send_game_packet(req, false);\n\t}\n\tteleport_manager::get().set_requested(true);',
    "RequestTeleport")


# ============================================================================
# DialogBox_CityHallMenu.cpp — RequestTeleportList, RequestChargedTeleport, RequestCivilRight
# ============================================================================
f = ROOT / "DialogBox_CityHallMenu.cpp"
ensure_include(f, '#include "Packet/SharedPackets.h"')

replace_in_file(f,
    'm_game->send_command(ClientMsgId::RequestTeleportList, 0, 0, 0, 0, 0, 0);',
    '{\n\t\t\thb::net::PacketRequestName20 req{};\n\t\t\treq.header.msg_id = ClientMsgId::RequestTeleportList;\n\t\t\treq.header.msg_type = 0;\n\t\t\tstd::snprintf(req.name, sizeof(req.name), "%s", "William");\n\t\t\tm_game->send_game_packet(req);\n\t\t}',
    "RequestTeleportList")

replace_in_file(f,
    "m_game->send_command(ClientMsgId::RequestChargedTeleport, 0, 0, teleport_manager::get().get_list()[i].index, 0, 0, 0);",
    '{\n\t\t\t\thb::net::PacketRequestTeleportId req{};\n\t\t\t\treq.header.msg_id = ClientMsgId::RequestChargedTeleport;\n\t\t\t\treq.header.msg_type = 0;\n\t\t\t\treq.teleport_id = teleport_manager::get().get_list()[i].index;\n\t\t\t\tm_game->send_game_packet(req);\n\t\t\t}',
    "RequestChargedTeleport")

replace_in_file(f,
    "m_game->send_command(MsgId::RequestCivilRight, MsgType::Confirm, 0, 0, 0, 0, 0);",
    '{\n\t\t\thb::net::PacketRequestHeaderOnly req{};\n\t\t\treq.header.msg_id = MsgId::RequestCivilRight;\n\t\t\treq.header.msg_type = MsgType::Confirm;\n\t\t\tm_game->send_game_packet(req);\n\t\t}',
    "RequestCivilRight")


# ============================================================================
# DialogBox_SellList.cpp — RequestSellItemList
# ============================================================================
f = ROOT / "DialogBox_SellList.cpp"
ensure_include(f, '#include "Packet/SharedPackets.h"')

replace_in_file(f,
    'm_game->send_command(MsgId::RequestSellItemList, 0, 0, 0, 0, 0, 0);',
    '{\n\t\thb::net::PacketRequestSellItemList req{};\n\t\treq.header.msg_id = MsgId::RequestSellItemList;\n\t\treq.header.msg_type = 0;\n\t\tfor (int i = 0; i < game_limits::max_sell_list; i++) {\n\t\t\treq.entries[i].index = static_cast<uint8_t>(get_entry(i).index);\n\t\t\treq.entries[i].amount = get_entry(i).amount;\n\t\t}\n\t\tm_game->send_game_packet(req);\n\t}',
    "RequestSellItemList")


# ============================================================================
# DialogBox_GuildHallMenu.cpp — RequestAngel (4 calls), RequestHeldenianScroll (6 calls),
#                                RequestHeldenianTpList, RequestHeldenianTp
# ============================================================================
f = ROOT / "DialogBox_GuildHallMenu.cpp"
ensure_include(f, '#include "Packet/SharedPackets.h"')

# RequestHeldenianTpList
replace_in_file(f,
    'send_command(ClientMsgId::RequestHeldenianTpList, 0, 0, 0, 0, 0, 0);',
    '{\n\t\t\thb::net::PacketRequestName20 req{};\n\t\t\treq.header.msg_id = ClientMsgId::RequestHeldenianTpList;\n\t\t\treq.header.msg_type = 0;\n\t\t\tstd::snprintf(req.name, sizeof(req.name), "%s", "Gail");\n\t\t\tsend_game_packet(req);\n\t\t}',
    "RequestHeldenianTpList")

# RequestHeldenianTp
replace_in_file(f,
    'send_command(ClientMsgId::RequestHeldenianTp, 0, 0, teleport_manager::get().get_list()[i].index, 0, 0, 0);',
    '{\n\t\t\t\thb::net::PacketRequestTeleportId req{};\n\t\t\t\treq.header.msg_id = ClientMsgId::RequestHeldenianTp;\n\t\t\t\treq.header.msg_type = 0;\n\t\t\t\treq.teleport_id = teleport_manager::get().get_list()[i].index;\n\t\t\t\tsend_game_packet(req);\n\t\t\t}',
    "RequestHeldenianTp")

# RequestAngel — 4 calls with angel_id 1-4
for angel_id in range(1, 5):
    replace_in_file(f,
        f'send_command(MsgId::RequestAngel, 0, 0, {angel_id}, 0, 0, "Gail", 0);',
        f'{{\n\t\t\t\thb::net::PacketRequestAngel req{{}};\n\t\t\t\treq.header.msg_id = MsgId::RequestAngel;\n\t\t\t\treq.header.msg_type = 0;\n\t\t\t\tstd::snprintf(req.name, sizeof(req.name), "%s", "Gail");\n\t\t\t\treq.angel_id = {angel_id};\n\t\t\t\tsend_game_packet(req);\n\t\t\t}}',
        f"RequestAngel angel_id={angel_id}")

# RequestHeldenianScroll — 6 calls with item_ids 875-880
scroll_ids = [(875, 'send_command(MsgId::RequestHeldenianScroll, 875, 1, 2, 3, 4, "Gail", 5);'),
              (876, 'send_command(MsgId::RequestHeldenianScroll, 876, 0, 0, 0, 0, "Gail", 0);'),
              (877, 'send_command(MsgId::RequestHeldenianScroll, 877, 0, 0, 0, 0, "Gail", 0);'),
              (878, 'send_command(MsgId::RequestHeldenianScroll, 878, 0, 0, 0, 0, "Gail", 0);'),
              (879, 'send_command(MsgId::RequestHeldenianScroll, 879, 0, 0, 0, 0, "Gail", 0);'),
              (880, 'send_command(MsgId::RequestHeldenianScroll, 880, 0, 0, 0, 0, "Gail", 0);')]

for item_id, old_call in scroll_ids:
    replace_in_file(f,
        old_call,
        f'{{\n\t\t\t\thb::net::PacketRequestHeldenianScroll req{{}};\n\t\t\t\treq.header.msg_id = MsgId::RequestHeldenianScroll;\n\t\t\t\treq.header.msg_type = 0;\n\t\t\t\tstd::snprintf(req.name, sizeof(req.name), "%s", "Gail");\n\t\t\t\treq.item_id = {item_id};\n\t\t\t\tsend_game_packet(req);\n\t\t\t}}',
        f"RequestHeldenianScroll item_id={item_id}")


# ============================================================================
# DialogBox_Inventory.cpp — RequestSetItemPos (2 calls)
# ============================================================================
f = ROOT / "DialogBox_Inventory.cpp"
ensure_include(f, '#include "Packet/SharedPackets.h"')

replace_in_file(f,
    'send_command(MsgId::RequestSetItemPos, 0, item_id, dX, dY, 0, 0);',
    '{\n\t\t\t\thb::net::PacketRequestSetItemPos req{};\n\t\t\t\treq.header.msg_id = MsgId::RequestSetItemPos;\n\t\t\t\treq.header.msg_type = 0;\n\t\t\t\treq.dir = static_cast<uint8_t>(item_id);\n\t\t\t\treq.x = static_cast<int16_t>(dX);\n\t\t\t\treq.y = static_cast<int16_t>(dY);\n\t\t\t\tsend_game_packet(req, false);\n\t\t\t}',
    "RequestSetItemPos (item_id)")

replace_in_file(f,
    'send_command(MsgId::RequestSetItemPos, 0, selected_id, dX, dY, 0, 0);',
    '{\n\t\t\t\thb::net::PacketRequestSetItemPos req{};\n\t\t\t\treq.header.msg_id = MsgId::RequestSetItemPos;\n\t\t\t\treq.header.msg_type = 0;\n\t\t\t\treq.dir = static_cast<uint8_t>(selected_id);\n\t\t\t\treq.x = static_cast<int16_t>(dX);\n\t\t\t\treq.y = static_cast<int16_t>(dY);\n\t\t\t\tsend_game_packet(req, false);\n\t\t\t}',
    "RequestSetItemPos (selected_id)")


# ============================================================================
# Game.cpp — RequestSetItemPos
# ============================================================================
f = ROOT / "Game.cpp"

replace_in_file(f,
    'send_command(MsgId::RequestSetItemPos, 0, item_index, nX, nY, 0, 0);',
    '{\n\t\t\thb::net::PacketRequestSetItemPos req{};\n\t\t\treq.header.msg_id = MsgId::RequestSetItemPos;\n\t\t\treq.header.msg_type = 0;\n\t\t\treq.dir = static_cast<uint8_t>(item_index);\n\t\t\treq.x = static_cast<int16_t>(nX);\n\t\t\treq.y = static_cast<int16_t>(nY);\n\t\t\tsend_game_packet(req, false);\n\t\t}',
    "RequestSetItemPos (Game.cpp)")


# ============================================================================
# NetworkMessages_Items.cpp — RequestSetItemPos (3 calls with i, nX, nY)
# All 3 calls are identical: game->send_command(MsgId::RequestSetItemPos, 0, i, nX, nY, 0, 0);
# ============================================================================
f = ROOT / "NetworkMessages_Items.cpp"
ensure_include(f, '#include "Packet/SharedPackets.h"')

# There are 3 identical calls. Use replace_all style — replace each occurrence
content = f.read_text(encoding='utf-8')
old_call = 'game->send_command(MsgId::RequestSetItemPos, 0, i, nX, nY, 0, 0);'
new_call = '{\n\t\t\thb::net::PacketRequestSetItemPos req{};\n\t\t\treq.header.msg_id = MsgId::RequestSetItemPos;\n\t\t\treq.header.msg_type = 0;\n\t\t\treq.dir = static_cast<uint8_t>(i);\n\t\t\treq.x = static_cast<int16_t>(nX);\n\t\t\treq.y = static_cast<int16_t>(nY);\n\t\t\tgame->send_game_packet(req, false);\n\t\t}'
count = content.count(old_call)
if count == 3:
    if DRY_RUN:
        print(f"  [DRY-RUN] NetworkMessages_Items.cpp — 3x RequestSetItemPos")
    else:
        new_content = content.replace(old_call, new_call)
        f.write_text(new_content, encoding='utf-8')
        print(f"  Applied: NetworkMessages_Items.cpp — 3x RequestSetItemPos")
    changes.append(("NetworkMessages_Items.cpp", 0, old_call[:80], new_call[:80]))
else:
    print(f"  WARNING: Expected 3 occurrences in NetworkMessages_Items.cpp, found {count}")


# ============================================================================
# Report
# ============================================================================
print(f"\n{'='*60}")
print(f"Total changes: {len(changes)}")
if DRY_RUN:
    print("DRY RUN — no files were modified")
    log_path = OUTPUT_DIR / "step3_dry_run.log"
    with open(log_path, 'w') as f:
        for fname, line, old, new in changes:
            f.write(f"{fname}:{line}\n  OLD: {old}\n  NEW: {new}\n\n")
    print(f"Details written to: {log_path}")
else:
    print("All changes applied successfully")
