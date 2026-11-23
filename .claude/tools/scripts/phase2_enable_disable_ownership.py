#!/usr/bin/env python3
"""
Phase 2: Enable/Disable Ownership
Moves enable/disable case blocks from DialogBoxManager's centralized switches
into each dialog's on_enable()/on_disable() override.

Mode 2 justified: touches ~30 dialog .h/.cpp files with mechanical pattern.

Usage:
    python Scripts/phase2_enable_disable_ownership.py --dry-run
    python Scripts/phase2_enable_disable_ownership.py
"""

import os
import sys
import re
import argparse
from pathlib import Path
from datetime import datetime

REPO_ROOT = Path(__file__).resolve().parent.parent
CLIENT_DIR = REPO_ROOT / "Sources" / "Client"
OUTPUT_DIR = REPO_ROOT / "Scripts" / "output"

# ============================================================
# Data: on_enable definitions per dialog
# ============================================================
# Each entry: (dialog_name, class_name, needs_first_enable_check, code_lines, extra_includes)
# code_lines are the body of on_enable(int type, int64_t v1, int v2, const char* string)
# References are already translated from dlg-> to direct member access

ENABLE_DEFS = [
    # RepairAll: no first-enable check, always sets m_mode
    ("RepairAll", "DialogBox_RepairAll", False, [
        "\tInfo().m_mode = type;",
    ], []),

    # SaleMenu → Shop file. Only on first enable.
    ("Shop", "DialogBox_Shop", True, [
        "\tswitch (type) {",
        "\tcase 0:",
        "\t\tbreak;",
        "\tdefault:",
        "\t\tif (shop_manager::get().has_items()) {",
        "\t\t\tInfo().m_v1 = type;",
        "\t\t\tInfo().m_mode = 0;",
        "\t\t\tInfo().m_view = 0;",
        "\t\t\tInfo().m_flag = true;",
        "\t\t\tInfo().m_v3 = 1;",
        "\t\t} else {",
        "\t\t\tshop_manager::get().set_pending_npc_config_id(type);",
        "\t\t\tshop_manager::get().request_shop_menu(type);",
        "\t\t}",
        "\t\tbreak;",
        "\t}",
    ], ["ShopManager.h"]),

    # LevelUpSetting
    ("LevelUpSetting", "DialogBox_LevelUpSetting", True, [
        "\tauto* charDlg = get_dialog_box(DialogBoxId::CharacterInfo);",
        "\tif (charDlg) { m_x = charDlg->m_x + 20; m_y = charDlg->m_y + 20; }",
        "\tInfo().m_v1 = m_game->m_player->m_lu_point;",
    ], []),

    # ItemDropConfirm → ItemDrop file
    ("ItemDrop", "DialogBox_ItemDrop", True, [
        "\tInfo().m_view = type;",
    ], []),

    # WarningBattleArea → WarningMsg file
    ("WarningMsg", "DialogBox_WarningMsg", True, [
        "\tInfo().m_view = type;",
    ], []),

    # GuildMenu: runs even when already enabled (re-enable case)
    ("GuildMenu", "DialogBox_GuildMenu", False, [
        "\tif (Info().m_mode == 1) {",
        "\t\ttext_input_manager::get().end_input();",
        "\t\ttext_input_manager::get().start_input(m_x + 75, m_y + 140, 21, m_game->m_player->m_guild_name, false, hb::client::character_name_allowed_chars);",
        "\t}",
    ], []),

    # ItemDropExternal → ItemDropAmount file
    ("ItemDropAmount", "DialogBox_ItemDropAmount", False, [
        "\tif (!is_enabled())",
        "\t{",
        "\t\tInfo().m_mode = 1;",
        "\t\tInfo().m_view = type;",
        "\t\ttext_input_manager::get().end_input();",
        "\t\tm_game->m_amount_string = std::to_string(v1);",
        "\t\ttext_input_manager::get().start_input(m_x + 40, m_y + 57, CGame::AmountStringMaxLen, m_game->m_amount_string, false, hb::client::digits_only);",
        "\t}",
        "\telse",
        "\t{",
        "\t\tif (Info().m_mode == 1)",
        "\t\t{",
        "\t\t\ttext_input_manager::get().end_input();",
        "\t\t\ttext_input_manager::get().start_input(m_x + 40, m_y + 57, CGame::AmountStringMaxLen, m_game->m_amount_string, false, hb::client::digits_only);",
        "\t\t}",
        "\t}",
    ], ["TextInputManager.h", "TextFieldRenderer.h"]),

    # Text
    ("Text", "DialogBox_Text", True, [
        "\tswitch (type) {",
        "\tcase 0:",
        "\t\tInfo().m_mode = 0;",
        "\t\tInfo().m_view = 0;",
        "\t\tbreak;",
        "\tdefault:",
        "\t\tm_game->load_text_dlg_contents(type);",
        "\t\tInfo().m_mode = 0;",
        "\t\tInfo().m_view = 0;",
        "\t\tbreak;",
        "\t}",
    ], []),

    # NpcActionQuery
    ("NpcActionQuery", "DialogBox_NpcActionQuery", False, [
        "\t// Clear previously disabled item",
        "\t{ int idx = Info().m_v1;",
        "\tif (idx >= 0 && idx < hb::shared::limits::MaxItems) m_game->m_is_item_disabled[idx] = false; }",
        "\tif (!is_enabled())",
        "\t{",
        "\t\tauto* saleDlg = get_dialog_box(DialogBoxId::SaleMenu);",
        "\t\tif (saleDlg) {",
        "\t\t\tsaleDlg->Info().m_v1 = saleDlg->Info().m_v2 = saleDlg->Info().m_v3 =",
        "\t\t\t\tsaleDlg->Info().m_v4 = saleDlg->Info().m_v5 = saleDlg->Info().m_v6 = 0;",
        "\t\t}",
        "\t\tInfo().m_mode = type;",
        "\t\tInfo().m_view = 0;",
        "\t\tInfo().m_v1 = static_cast<int>(v1);",
        "\t\tInfo().m_v2 = v2;",
        "\t}",
    ], []),

    # NpcTalk
    ("NpcTalk", "DialogBox_NpcTalk", True, [
        "\tInfo().m_mode = type;",
        "\tInfo().m_view = 0;",
        "\tInfo().m_v1 = m_game->load_text_dlg_contents2(static_cast<int>(v1) + 20);",
        "\tInfo().m_v2 = static_cast<int>(v1) + 20;",
    ], []),

    # SellOrRepair
    ("SellOrRepair", "DialogBox_SellOrRepair", True, [
        "\tInfo().m_mode = type;",
        "\tInfo().m_v1 = static_cast<int>(v1);",
        "\tInfo().m_v2 = v2;",
        "\tif (type == 2)",
        "\t{",
        "\t\tauto* saleDlg = get_dialog_box(DialogBoxId::SaleMenu);",
        "\t\tif (saleDlg) { m_x = saleDlg->m_x; m_y = saleDlg->m_y; }",
        "\t}",
    ], []),

    # Fishing
    ("Fishing", "DialogBox_Fishing", True, [
        "\tInfo().m_mode = type;",
        "\tInfo().m_v1 = static_cast<int>(v1);",
        "\tInfo().m_v2 = v2;",
        "\tm_game->m_skill_using_status = true;",
    ], []),

    # Noticement
    ("Noticement", "DialogBox_Noticement", True, [
        "\tInfo().m_mode = type;",
        "\tInfo().m_v1 = static_cast<int>(v1);",
        "\tInfo().m_v2 = v2;",
    ], []),

    # Manufacture (complex multi-sub-case)
    ("Manufacture", "DialogBox_Manufacture", False, [
        "\tswitch (type) {",
        "\tcase DialogBoxId::CharacterInfo:",
        "\tcase DialogBoxId::Inventory:",
        "\t\tif (!is_enabled())",
        "\t\t{",
        "\t\t\tInfo().m_mode = type;",
        "\t\t\tInfo().m_v1 = -1;",
        "\t\t\tInfo().m_v2 = -1;",
        "\t\t\tInfo().m_v3 = -1;",
        "\t\t\tInfo().m_v4 = -1;",
        "\t\t\tInfo().m_v5 = -1;",
        "\t\t\tInfo().m_v6 = -1;",
        "\t\t\tInfo().m_str[0] = 0;",
        "\t\t\tm_game->m_skill_using_status = true;",
        "\t\t\tm_size_x = 195;",
        "\t\t\tm_size_y = 215;",
        "\t\t\tdisable_dialog_box(DialogBoxId::ItemDropExternal);",
        "\t\t\tdisable_dialog_box(DialogBoxId::NpcActionQuery);",
        "\t\t\tdisable_dialog_box(DialogBoxId::SellOrRepair);",
        "\t\t}",
        "\t\tbreak;",
        "",
        "\tcase DialogBoxId::Magic:",
        "\t\tif (!is_enabled())",
        "\t\t{",
        "\t\t\tInfo().m_view = 0;",
        "\t\t\tInfo().m_mode = type;",
        "\t\t\tInfo().m_v1 = -1;",
        "\t\t\tInfo().m_v2 = -1;",
        "\t\t\tInfo().m_v3 = -1;",
        "\t\t\tInfo().m_v4 = -1;",
        "\t\t\tInfo().m_v5 = -1;",
        "\t\t\tInfo().m_v6 = -1;",
        "\t\t\tInfo().m_str[0] = 0;",
        "\t\t\tInfo().m_str[1] = 0;",
        "\t\t\tInfo().m_str[4] = 0;",
        "\t\t\tm_game->m_skill_using_status = true;",
        "\t\t\tbuild_item_manager::get().update_available_recipes();",
        "\t\t\tm_size_x = 270;",
        "\t\t\tm_size_y = 381;",
        "\t\t\tdisable_dialog_box(DialogBoxId::ItemDropExternal);",
        "\t\t\tdisable_dialog_box(DialogBoxId::NpcActionQuery);",
        "\t\t\tdisable_dialog_box(DialogBoxId::SellOrRepair);",
        "\t\t}",
        "\t\tbreak;",
        "",
        "\tcase DialogBoxId::WarningBattleArea:",
        "\t\tif (!is_enabled())",
        "\t\t{",
        "\t\t\tInfo().m_mode = type;",
        "\t\t\tInfo().m_str[2] = static_cast<char>(v1);",
        "\t\t\tInfo().m_str[3] = v2;",
        "\t\t\tm_size_x = 270;",
        "\t\t\tm_size_y = 381;",
        "\t\t\tm_game->m_skill_using_status = true;",
        "\t\t\tbuild_item_manager::get().update_available_recipes();",
        "\t\t\tdisable_dialog_box(DialogBoxId::ItemDropExternal);",
        "\t\t\tdisable_dialog_box(DialogBoxId::NpcActionQuery);",
        "\t\t\tdisable_dialog_box(DialogBoxId::SellOrRepair);",
        "\t\t}",
        "\t\tbreak;",
        "",
        "\tcase DialogBoxId::GuildMenu:",
        "\tcase DialogBoxId::GuildOperation:",
        "\t\tif (!is_enabled())",
        "\t\t{",
        "\t\t\tInfo().m_mode = type;",
        "\t\t\tInfo().m_v1 = -1;",
        "\t\t\tInfo().m_v2 = -1;",
        "\t\t\tInfo().m_v3 = -1;",
        "\t\t\tInfo().m_v4 = -1;",
        "\t\t\tInfo().m_v5 = -1;",
        "\t\t\tInfo().m_v6 = -1;",
        "\t\t\tInfo().m_str[0] = 0;",
        "\t\t\tInfo().m_str[1] = 0;",
        "\t\t\tm_game->m_skill_using_status = true;",
        "\t\t\tm_size_x = 195;",
        "\t\t\tm_size_y = 215;",
        "\t\t\tdisable_dialog_box(DialogBoxId::ItemDropExternal);",
        "\t\t\tdisable_dialog_box(DialogBoxId::NpcActionQuery);",
        "\t\t\tdisable_dialog_box(DialogBoxId::SellOrRepair);",
        "\t\t}",
        "\t\tbreak;",
        "\t}",
    ], ["BuildItemManager.h"]),

    # Exchange
    ("Exchange", "DialogBox_Exchange", True, [
        "\tInfo().m_mode = type;",
        "\tfor (int i = 0; i < 8; i++)",
        "\t{",
        "\t\tm_game->m_dialog_box_exchange_info[i].v1 = -1;",
        "\t\tm_game->m_dialog_box_exchange_info[i].v2 = -1;",
        "\t\tm_game->m_dialog_box_exchange_info[i].v3 = -1;",
        "\t\tm_game->m_dialog_box_exchange_info[i].v4 = -1;",
        "\t\tm_game->m_dialog_box_exchange_info[i].v5 = -1;",
        "\t\tm_game->m_dialog_box_exchange_info[i].v6 = -1;",
        "\t\tm_game->m_dialog_box_exchange_info[i].v7 = -1;",
        "\t\tm_game->m_dialog_box_exchange_info[i].inv_slot = -1;",
        "\t\tm_game->m_dialog_box_exchange_info[i].dw_v1 = 0;",
        "\t}",
        "\tdisable_dialog_box(DialogBoxId::ItemDropExternal);",
        "\tdisable_dialog_box(DialogBoxId::NpcActionQuery);",
        "\tdisable_dialog_box(DialogBoxId::SellOrRepair);",
        "\tdisable_dialog_box(DialogBoxId::Manufacture);",
    ], []),

    # Quest
    ("Quest", "DialogBox_Quest", True, [
        "\tInfo().m_mode = type;",
        "\tauto* charDlg = get_dialog_box(DialogBoxId::CharacterInfo);",
        "\tif (charDlg) { m_x = charDlg->m_x + 20; m_y = charDlg->m_y + 20; }",
    ], []),

    # Party
    ("Party", "DialogBox_Party", True, [
        "\tInfo().m_mode = type;",
        "\tauto* charDlg = get_dialog_box(DialogBoxId::CharacterInfo);",
        "\tif (charDlg) { m_x = charDlg->m_x + 20; m_y = charDlg->m_y + 20; }",
    ], []),

    # CrusadeJob: precondition check — return false to cancel enable
    ("CrusadeJob", "DialogBox_CrusadeJob", False, [
        "\tif ((m_game->m_player->m_hp <= 0) || (m_game->m_player->m_citizen == false))",
        "\t\treturn false;",
        "\tif (!is_enabled())",
        "\t{",
        "\t\tInfo().m_mode = type;",
        "\t\tm_x = 520;",
        "\t\tm_y = 65;",
        "\t\tInfo().m_v1 = static_cast<int>(v1);",
        "\t}",
        "\treturn true;",
    ], []),

    # ItemUpgrade
    ("ItemUpgrade", "DialogBox_ItemUpgrade", True, [
        "\tInfo().m_mode = type;",
        "\tInfo().m_v1 = -1;",
        "\tInfo().m_dw_v1 = 0;",
    ], []),

    # MagicShop: precondition redirect — return false to cancel, then enable NpcTalk instead
    ("MagicShop", "DialogBox_MagicShop", True, [
        "\tif (m_game->m_player->m_skill_mastery[4] == 0) {",
        "\t\tenable_dialog_box(DialogBoxId::NpcTalk, 0, 480, 0);",
        "\t\treturn false;",
        "\t}",
        "\tInfo().m_mode = 0;",
        "\tInfo().m_view = 0;",
        "\treturn true;",
    ], []),

    # Bank
    ("Bank", "DialogBox_Bank", False, [
        "\ttext_input_manager::get().end_input();",
        "\tif (!is_enabled()) {",
        "\t\tInfo().m_mode = 0;",
        "\t\tInfo().m_view = 0;",
        "\t\tenable_dialog_box(DialogBoxId::Inventory, 0, 0, 0);",
        "\t}",
    ], ["TextInputManager.h"]),

    # Slates
    ("Slates", "DialogBox_Slates", True, [
        "\tInfo().m_view = 0;",
        "\tInfo().m_mode = type;",
        "\tInfo().m_v1 = -1;",
        "\tInfo().m_v2 = -1;",
        "\tInfo().m_v3 = -1;",
        "\tInfo().m_v4 = -1;",
        "\tInfo().m_v5 = -1;",
        "\tInfo().m_v6 = -1;",
        "\tInfo().m_str[0] = 0;",
        "\tInfo().m_str[1] = 0;",
        "\tInfo().m_str[4] = 0;",
        "\tm_size_x = 180;",
        "\tm_size_y = 183;",
        "\tdisable_dialog_box(DialogBoxId::ItemDropExternal);",
        "\tdisable_dialog_box(DialogBoxId::NpcActionQuery);",
        "\tdisable_dialog_box(DialogBoxId::SellOrRepair);",
        "\tdisable_dialog_box(DialogBoxId::Manufacture);",
    ], []),

    # TesterMenu (TESTER_ONLY)
    ("TesterMenu", "DialogBox_TesterMenu", True, [
        "\tint tester_x = LOGICAL_WIDTH() - 258 - 10;",
        "\tint tester_y = LOGICAL_HEIGHT() - 339 - ICON_PANEL_HEIGHT() - 10;",
        "\tif (m_game->m_dialog_box_manager.is_enabled(DialogBoxId::LevelUpSetting))",
        "\t\ttester_y -= 30;",
        "\tm_x = tester_x;",
        "\tm_y = tester_y;",
    ], ["GlobalDef.h"]),

    # ItemCreator (TESTER_ONLY) — already has on_disable, needs on_enable
    ("ItemCreator", "DialogBox_ItemCreator", True, [
        "\tint ic_x = LOGICAL_WIDTH() - 258 * 2 - 20;",
        "\tint ic_y = LOGICAL_HEIGHT() - 339 - ICON_PANEL_HEIGHT() - 10;",
        "\tm_x = ic_x;",
        "\tm_y = ic_y;",
    ], ["GlobalDef.h"]),

    # ChangeStatsMajestic
    ("ChangeStatsMajestic", "DialogBox_ChangeStatsMajestic", True, [
        "\tauto* luDlg = get_dialog_box(DialogBoxId::LevelUpSetting);",
        "\tif (luDlg) { m_x = luDlg->m_x + 10; m_y = luDlg->m_y + 10; }",
        "\tInfo().m_mode = 0;",
        "\tInfo().m_view = 0;",
        "\tm_game->m_player->m_lu_str = m_game->m_player->m_lu_vit = m_game->m_player->m_lu_dex = 0;",
        "\tm_game->m_player->m_lu_int = m_game->m_player->m_lu_mag = m_game->m_player->m_lu_char = 0;",
        "\tm_game->m_skill_using_status = false;",
    ], []),

    # Resurrect
    ("Resurrect", "DialogBox_Resurrect", True, [
        "\tm_x = 185;",
        "\tm_y = 100;",
        "\tInfo().m_mode = 0;",
        "\tInfo().m_view = 0;",
        "\tm_game->m_skill_using_status = false;",
    ], []),
]

# ============================================================
# Data: on_disable definitions per dialog
# ============================================================
# Each entry: (file_name, class_name, code_lines, extra_includes, returns_bool)
# returns_bool=True means this on_disable can cancel the disable (Bank)

DISABLE_DEFS = [
    # ItemDropConfirm → ItemDrop
    ("ItemDrop", "DialogBox_ItemDrop", [
        "\t{ int idx = Info().m_view;",
        "\tif (idx >= 0 && idx < hb::shared::limits::MaxItems) m_game->m_is_item_disabled[idx] = false; }",
    ], [], False),

    # WarningBattleArea → WarningMsg
    ("WarningMsg", "DialogBox_WarningMsg", [
        "\t{ int idx = Info().m_view;",
        "\tif (idx >= 0 && idx < hb::shared::limits::MaxItems) m_game->m_is_item_disabled[idx] = false; }",
    ], [], False),

    # GuildMenu
    ("GuildMenu", "DialogBox_GuildMenu", [
        "\tif (Info().m_mode == 1)",
        "\t\ttext_input_manager::get().end_input();",
        "\tInfo().m_mode = 0;",
    ], []),

    # SaleMenu → Shop
    ("Shop", "DialogBox_Shop", [
        "\tshop_manager::get().clear_items();",
        "\tauto* giveDlg = get_dialog_box(DialogBoxId::GiveItem);",
        "\tif (giveDlg) {",
        "\t\tgiveDlg->Info().m_v3 = 0;",
        "\t\tgiveDlg->Info().m_v4 = 0;",
        "\t\tgiveDlg->Info().m_v5 = 0;",
        "\t\tgiveDlg->Info().m_v6 = 0;",
        "\t}",
    ], ["ShopManager.h"]),

    # Bank — cancels disable if m_mode < 0
    ("Bank", "DialogBox_Bank", [
        "\tif (Info().m_mode < 0) return false;",
        "\treturn true;",
    ], [], True),

    # ItemDropExternal → ItemDropAmount
    ("ItemDropAmount", "DialogBox_ItemDropAmount", [
        "\tif (Info().m_mode == 1) {",
        "\t\ttext_input_manager::get().end_input();",
        "\t\t{ int idx = Info().m_view;",
        "\t\tif (idx >= 0 && idx < hb::shared::limits::MaxItems) m_game->m_is_item_disabled[idx] = false; }",
        "\t}",
    ], ["TextInputManager.h"]),

    # NpcActionQuery
    ("NpcActionQuery", "DialogBox_NpcActionQuery", [
        "\t{ int idx = Info().m_v1;",
        "\tif (idx >= 0 && idx < hb::shared::limits::MaxItems) m_game->m_is_item_disabled[idx] = false; }",
    ], []),

    # NpcTalk
    ("NpcTalk", "DialogBox_NpcTalk", [
        "\tif (Info().m_v2 == 500)",
        "\t{",
        "\t\tm_game->send_command(MsgId::CommandCommon, CommonType::GetMagicAbility, 0, 0, 0, 0, 0);",
        "\t}",
    ], ["NetMessages.h"]),

    # Fishing
    ("Fishing", "DialogBox_Fishing", [
        "\tm_game->m_skill_using_status = false;",
    ], []),

    # Manufacture
    ("Manufacture", "DialogBox_Manufacture", [
        "\tauto clearItem = [&](int idx) { if (idx >= 0 && idx < hb::shared::limits::MaxItems) m_game->m_is_item_disabled[idx] = false; };",
        "\tclearItem(Info().m_v1); clearItem(Info().m_v2); clearItem(Info().m_v3);",
        "\tclearItem(Info().m_v4); clearItem(Info().m_v5); clearItem(Info().m_v6);",
        "\tm_game->m_skill_using_status = false;",
    ], []),

    # Exchange
    ("Exchange", "DialogBox_Exchange", [
        "\tfor (int i = 0; i < 8; i++)",
        "\t{",
        "\t\tint slot = m_game->m_dialog_box_exchange_info[i].inv_slot;",
        "\t\tif (slot >= 0 && slot < hb::shared::limits::MaxItems && m_game->m_is_item_disabled[slot])",
        "\t\t\tm_game->m_is_item_disabled[slot] = false;",
        "",
        "\t\tm_game->m_dialog_box_exchange_info[i].v1 = -1;",
        "\t\tm_game->m_dialog_box_exchange_info[i].v2 = -1;",
        "\t\tm_game->m_dialog_box_exchange_info[i].v3 = -1;",
        "\t\tm_game->m_dialog_box_exchange_info[i].v4 = -1;",
        "\t\tm_game->m_dialog_box_exchange_info[i].v5 = -1;",
        "\t\tm_game->m_dialog_box_exchange_info[i].v6 = -1;",
        "\t\tm_game->m_dialog_box_exchange_info[i].v7 = -1;",
        "\t\tm_game->m_dialog_box_exchange_info[i].item_id = -1;",
        "\t\tm_game->m_dialog_box_exchange_info[i].inv_slot = -1;",
        "\t\tm_game->m_dialog_box_exchange_info[i].dw_v1 = 0;",
        "\t}",
    ], []),

    # SellList
    ("SellList", "DialogBox_SellList", [
        "\tfor (int i = 0; i < game_limits::max_sell_list; i++)",
        "\t{",
        "\t\t{ int idx = m_game->m_sell_item_list[i].index;",
        "\t\tif (idx >= 0 && idx < hb::shared::limits::MaxItems) m_game->m_is_item_disabled[idx] = false; }",
        "\t\tm_game->m_sell_item_list[i].index = -1;",
        "\t\tm_game->m_sell_item_list[i].amount = 0;",
        "\t}",
    ], []),

    # ItemUpgrade
    ("ItemUpgrade", "DialogBox_ItemUpgrade", [
        "\t{ int idx = Info().m_v1;",
        "\tif (idx >= 0 && idx < hb::shared::limits::MaxItems) m_game->m_is_item_disabled[idx] = false; }",
    ], []),

    # Slates
    ("Slates", "DialogBox_Slates", [
        "\tauto clearSlot = [&](int idx) {",
        "\t\tif (idx >= 0 && idx < hb::shared::limits::MaxItems) m_game->m_is_item_disabled[idx] = false;",
        "\t};",
        "\tclearSlot(Info().m_v1);",
        "\tclearSlot(Info().m_v2);",
        "\tclearSlot(Info().m_v3);",
        "\tclearSlot(Info().m_v4);",
        "\tstd::memset(Info().m_str, 0, sizeof(Info().m_str));",
        "\tstd::memset(Info().m_str3, 0, sizeof(Info().m_str3));",
        "\tstd::memset(Info().m_str4, 0, sizeof(Info().m_str4));",
        "\tInfo().m_v1 = -1;",
        "\tInfo().m_v2 = -1;",
        "\tInfo().m_v3 = -1;",
        "\tInfo().m_v4 = -1;",
        "\tInfo().m_v5 = -1;",
        "\tInfo().m_v6 = -1;",
        "\tInfo().m_v9 = -1;",
        "\tInfo().m_v10 = -1;",
        "\tInfo().m_v11 = -1;",
        "\tInfo().m_v12 = -1;",
        "\tInfo().m_v13 = -1;",
        "\tInfo().m_v14 = -1;",
        "\tInfo().m_dw_v1 = 0;",
        "\tInfo().m_dw_v2 = 0;",
    ], []),

    # ChangeStatsMajestic
    ("ChangeStatsMajestic", "DialogBox_ChangeStatsMajestic", [
        "\tm_game->m_player->m_lu_str = 0;",
        "\tm_game->m_player->m_lu_vit = 0;",
        "\tm_game->m_player->m_lu_dex = 0;",
        "\tm_game->m_player->m_lu_int = 0;",
        "\tm_game->m_player->m_lu_mag = 0;",
        "\tm_game->m_player->m_lu_char = 0;",
    ], []),
]

# Normalize: ensure all disable defs have 5 elements
for i, d in enumerate(DISABLE_DEFS):
    if len(d) == 4:
        DISABLE_DEFS[i] = d + (False,)

# Dialogs that need cancels_text_input_on_enable() = false
NO_CANCEL_TEXT_INPUT = ["Character", "ChatHistory", "Inventory"]


def log(msg, log_file=None):
    print(msg)
    if log_file:
        log_file.write(msg + "\n")


def find_insertion_point_h(content, class_name):
    """Find where to add override declarations in the header.
    Insert before the first 'private:' or at end of class body."""
    lines = content.split('\n')

    # Find class opening
    class_start = -1
    for i, line in enumerate(lines):
        if f'class {class_name}' in line and '{' in line:
            class_start = i
            break
        elif f'class {class_name}' in line:
            # opening brace on next line
            for j in range(i+1, len(lines)):
                if '{' in lines[j]:
                    class_start = j
                    break
            break

    if class_start < 0:
        return -1

    # Look for first 'private:' section after public section
    first_private = -1
    for i in range(class_start + 1, len(lines)):
        stripped = lines[i].strip()
        if stripped == 'private:':
            first_private = i
            break
        if stripped.startswith('};'):
            # No private section, insert before closing
            first_private = i
            break

    return first_private


def add_override_to_header(filepath, class_name, method_decl, dry_run, log_file):
    """Add a method declaration to the header file."""
    content = filepath.read_text(encoding='utf-8')

    # Check if already present
    if method_decl.strip().rstrip(';') in content:
        log(f"  [SKIP] {filepath.name}: '{method_decl.strip()[:50]}...' already present", log_file)
        return content

    insert_line = find_insertion_point_h(content, class_name)
    if insert_line < 0:
        log(f"  [ERROR] {filepath.name}: Could not find class {class_name}", log_file)
        return content

    lines = content.split('\n')
    # Insert before private: or };
    indent = '\t'
    new_line = f"{indent}{method_decl}"
    lines.insert(insert_line, new_line)

    new_content = '\n'.join(lines)
    if not dry_run:
        filepath.write_text(new_content, encoding='utf-8')
    log(f"  [HEADER] {filepath.name}: Added '{method_decl.strip()}'", log_file)
    return new_content


def add_include_to_cpp(filepath, include_name, dry_run, log_file):
    """Add an #include if not already present."""
    content = filepath.read_text(encoding='utf-8')
    include_str = f'#include "{include_name}"'

    if include_str in content:
        return content

    lines = content.split('\n')

    # Find last #include line
    last_include = -1
    for i, line in enumerate(lines):
        if line.strip().startswith('#include'):
            last_include = i

    if last_include >= 0:
        lines.insert(last_include + 1, include_str)
    else:
        lines.insert(0, include_str)

    new_content = '\n'.join(lines)
    if not dry_run:
        filepath.write_text(new_content, encoding='utf-8')
    log(f"  [INCLUDE] {filepath.name}: Added {include_str}", log_file)
    return new_content


def add_method_to_cpp(filepath, class_name, method_signature, body_lines, dry_run, log_file):
    """Add a method implementation to the end of the cpp file."""
    content = filepath.read_text(encoding='utf-8')

    # Check if method already exists
    method_name = method_signature.split('(')[0].split('::')[-1]
    method_check = f"{class_name}::{method_name}"
    if method_check in content:
        log(f"  [SKIP] {filepath.name}: {method_check} already exists", log_file)
        return content

    # Build the method
    method_lines = [
        "",
        f"{method_signature}",
        "{",
    ]
    method_lines.extend(body_lines)
    method_lines.append("}")
    method_lines.append("")

    method_text = '\n'.join(method_lines)

    # Handle TESTER_ONLY files: insert before final #endif
    is_tester = False
    lines = content.split('\n')
    for line in lines[:5]:
        if '#ifdef TESTER_ONLY' in line:
            is_tester = True
            break

    if is_tester:
        # Find last #endif
        last_endif = -1
        for i in range(len(lines) - 1, -1, -1):
            if lines[i].strip().startswith('#endif'):
                last_endif = i
                break
        if last_endif >= 0:
            lines.insert(last_endif, method_text.rstrip())
            new_content = '\n'.join(lines)
        else:
            new_content = content.rstrip() + '\n' + method_text
    else:
        new_content = content.rstrip() + '\n' + method_text

    if not dry_run:
        filepath.write_text(new_content, encoding='utf-8')
    log(f"  [METHOD] {filepath.name}: Added {class_name}::{method_name}()", log_file)
    return new_content


def process_enable_defs(dry_run, log_file):
    """Add on_enable overrides to each dialog."""
    log("\n=== Processing on_enable overrides ===\n", log_file)

    for file_name, class_name, needs_first_check, code_lines, extra_includes in ENABLE_DEFS:
        h_path = CLIENT_DIR / f"DialogBox_{file_name}.h"
        cpp_path = CLIENT_DIR / f"DialogBox_{file_name}.cpp"

        if not h_path.exists():
            log(f"  [ERROR] {h_path.name} not found!", log_file)
            continue
        if not cpp_path.exists():
            log(f"  [ERROR] {cpp_path.name} not found!", log_file)
            continue

        log(f"--- {class_name} ---", log_file)

        # Skip Map — already has on_enable
        if file_name == "Map":
            log(f"  [SKIP] {file_name}: already has on_enable override", log_file)
            continue

        # All overrides return bool (base class returns bool)
        decl = "bool on_enable(int type, int64_t v1, int v2, const char* string) override;"
        add_override_to_header(h_path, class_name, decl, dry_run, log_file)

        # Add includes to cpp
        for inc in extra_includes:
            add_include_to_cpp(cpp_path, inc, dry_run, log_file)

        # Build method body
        body = []
        if needs_first_check:
            body.append("\tif (is_enabled()) return true;")
        body.extend(code_lines)

        # Add return true if the code_lines don't already end with a return
        has_return = any("return true;" in line or "return false;" in line for line in code_lines)
        if not has_return:
            body.append("\treturn true;")

        sig = f"bool {class_name}::on_enable(int type, int64_t v1, int v2, const char* string)"
        add_method_to_cpp(cpp_path, class_name, sig, body, dry_run, log_file)


def process_disable_defs(dry_run, log_file):
    """Add on_disable overrides to each dialog."""
    log("\n=== Processing on_disable overrides ===\n", log_file)

    for entry in DISABLE_DEFS:
        file_name, class_name, code_lines, extra_includes, returns_bool = entry

        h_path = CLIENT_DIR / f"DialogBox_{file_name}.h"
        cpp_path = CLIENT_DIR / f"DialogBox_{file_name}.cpp"

        if not h_path.exists():
            log(f"  [ERROR] {h_path.name} not found!", log_file)
            continue
        if not cpp_path.exists():
            log(f"  [ERROR] {cpp_path.name} not found!", log_file)
            continue

        log(f"--- {class_name} ---", log_file)

        # Skip ItemCreator — already has on_disable
        if file_name == "ItemCreator":
            log(f"  [SKIP] {file_name}: already has on_disable override", log_file)
            continue

        # All overrides return bool (base class returns bool)
        decl = "bool on_disable() override;"
        add_override_to_header(h_path, class_name, decl, dry_run, log_file)

        # Add includes to cpp
        for inc in extra_includes:
            add_include_to_cpp(cpp_path, inc, dry_run, log_file)

        # Build method body — all return bool
        body = list(code_lines)
        if not returns_bool:
            # Non-cancelling dialogs: add return true at the end
            body.append("\treturn true;")
        sig = f"bool {class_name}::on_disable()"
        add_method_to_cpp(cpp_path, class_name, sig, body, dry_run, log_file)


def process_no_cancel_text_input(dry_run, log_file):
    """Add cancels_text_input_on_enable() = false for CharacterInfo, ChatHistory, Inventory."""
    log("\n=== Processing cancels_text_input_on_enable overrides ===\n", log_file)

    for file_name in NO_CANCEL_TEXT_INPUT:
        h_path = CLIENT_DIR / f"DialogBox_{file_name}.h"
        if not h_path.exists():
            log(f"  [ERROR] {h_path.name} not found!", log_file)
            continue

        content = h_path.read_text(encoding='utf-8')
        if 'cancels_text_input_on_enable' in content:
            log(f"  [SKIP] {h_path.name}: already has cancels_text_input_on_enable", log_file)
            continue

        class_name = f"DialogBox_{file_name}"
        decl = "bool cancels_text_input_on_enable() const override { return false; }"
        add_override_to_header(h_path, class_name, decl, dry_run, log_file)


def update_idialogbox_h(dry_run, log_file):
    """Change on_enable() and on_disable() return types to bool in IDialogBox.h."""
    log("\n=== Updating IDialogBox.h ===\n", log_file)

    filepath = CLIENT_DIR / "IDialogBox.h"
    content = filepath.read_text(encoding='utf-8')

    # Change on_enable: virtual void on_enable(...) {} → virtual bool on_enable(...) { return true; }
    old_enable = "virtual void on_enable(int type, int64_t v1, int v2, const char* string) {}"
    new_enable = "virtual bool on_enable(int type, int64_t v1, int v2, const char* string) { return true; }"

    if old_enable in content:
        content = content.replace(old_enable, new_enable)
        log(f"  [UPDATED] IDialogBox.h: on_enable now returns bool", log_file)
    elif new_enable in content:
        log(f"  [SKIP] IDialogBox.h: on_enable already returns bool", log_file)
    else:
        log(f"  [ERROR] IDialogBox.h: Could not find on_enable signature to update", log_file)

    # Change on_disable: virtual void on_disable() {} → virtual bool on_disable() { return true; }
    old_disable = "virtual void on_disable() {}"
    new_disable = "virtual bool on_disable() { return true; }"

    if old_disable in content:
        content = content.replace(old_disable, new_disable)
        log(f"  [UPDATED] IDialogBox.h: on_disable now returns bool", log_file)
    elif new_disable in content:
        log(f"  [SKIP] IDialogBox.h: on_disable already returns bool", log_file)
    else:
        log(f"  [ERROR] IDialogBox.h: Could not find on_disable signature to update", log_file)

    if not dry_run:
        filepath.write_text(content, encoding='utf-8')


def update_dialogbox_manager(dry_run, log_file):
    """Replace enable/disable switches with generic flow in DialogBoxManager.cpp."""
    log("\n=== Updating DialogBoxManager.cpp ===\n", log_file)

    filepath = CLIENT_DIR / "DialogBoxManager.cpp"
    content = filepath.read_text(encoding='utf-8')
    lines = content.split('\n')

    # ---- Replace enable_dialog_box(int, ...) ----
    # Find the function start
    enable_start = -1
    for i, line in enumerate(lines):
        if 'void DialogBoxManager::enable_dialog_box(int box_id, int type, int64_t v1, int v2, const char* string)' in line:
            enable_start = i
            break

    if enable_start < 0:
        log("  [ERROR] Could not find enable_dialog_box(int...) function", log_file)
        return

    # Find the closing brace of this function
    enable_end = -1
    brace_count = 0
    found_open = False
    for i in range(enable_start, len(lines)):
        for ch in lines[i]:
            if ch == '{':
                brace_count += 1
                found_open = True
            elif ch == '}':
                brace_count -= 1
                if found_open and brace_count == 0:
                    enable_end = i
                    break
        if enable_end >= 0:
            break

    if enable_end < 0:
        log("  [ERROR] Could not find end of enable_dialog_box", log_file)
        return

    # New enable_dialog_box body
    new_enable = [
        "void DialogBoxManager::enable_dialog_box(int box_id, int type, int64_t v1, int v2, const char* string)",
        "{",
        "\tauto* dlg = get_dialog_box(box_id);",
        "\tif (!dlg) return;",
        "",
        "\tif (dlg->cancels_text_input_on_enable())",
        "\t\ttext_input_manager::get().end_input();",
        "",
        "\tif (!dlg->on_enable(type, v1, v2, string))",
        "\t\treturn;  // on_enable returned false to cancel",
        "",
        "\t// Clamp position (skip topmost-layer dialogs like HudPanel)",
        "\tif (get_z_layer(box_id) != z_layer::topmost)",
        "\t{",
        "\t\tif (dlg->is_enabled() == false)",
        "\t\t\tclamp_position(dlg);",
        "\t}",
        "",
        "\tdlg->set_enabled(true);",
        "\tif (string != nullptr)",
        "\t\tstd::snprintf(dlg->Info().m_str, sizeof(dlg->Info().m_str), \"%s\", string);",
        "",
        "\tbring_to_front(box_id);",
        "}",
    ]

    # ---- Replace disable_dialog_box(int) ----
    disable_start = -1
    for i, line in enumerate(lines):
        if 'void DialogBoxManager::disable_dialog_box(int box_id)' in line:
            disable_start = i
            break

    if disable_start < 0:
        log("  [ERROR] Could not find disable_dialog_box(int) function", log_file)
        return

    disable_end = -1
    brace_count = 0
    found_open = False
    for i in range(disable_start, len(lines)):
        for ch in lines[i]:
            if ch == '{':
                brace_count += 1
                found_open = True
            elif ch == '}':
                brace_count -= 1
                if found_open and brace_count == 0:
                    disable_end = i
                    break
        if disable_end >= 0:
            break

    if disable_end < 0:
        log("  [ERROR] Could not find end of disable_dialog_box", log_file)
        return

    # New disable_dialog_box body
    new_disable = [
        "void DialogBoxManager::disable_dialog_box(int box_id)",
        "{",
        "\tauto* dlg = get_dialog_box(box_id);",
        "\tif (!dlg || !dlg->is_enabled()) return;",
        "",
        "\tif (!dlg->on_disable()) return;  // on_disable returns false to cancel",
        "",
        "\tdlg->set_enabled(false);",
        "\tremove_from_order(box_id);",
        "}",
    ]

    # Apply replacements (do disable first since it comes after enable)
    # Replace disable
    lines[disable_start:disable_end + 1] = new_disable
    # Replace enable (indices not affected since disable comes after)
    lines[enable_start:enable_end + 1] = new_enable

    new_content = '\n'.join(lines)
    if not dry_run:
        filepath.write_text(new_content, encoding='utf-8')
    log(f"  [UPDATED] DialogBoxManager.cpp: Replaced enable/disable switches with generic flow", log_file)
    log(f"    enable_dialog_box: lines {enable_start+1}-{enable_end+1} -> {len(new_enable)} lines", log_file)
    log(f"    disable_dialog_box: lines {disable_start+1}-{disable_end+1} -> {len(new_disable)} lines", log_file)


def update_map_on_enable(dry_run, log_file):
    """Update Map's existing on_enable to return bool."""
    log("\n=== Updating DialogBox_Map on_enable return type ===\n", log_file)

    # Update header
    h_path = CLIENT_DIR / "DialogBox_Map.h"
    content = h_path.read_text(encoding='utf-8')
    old_decl = "void on_enable(int type, int64_t v1, int v2, const char* string) override;"
    new_decl = "bool on_enable(int type, int64_t v1, int v2, const char* string) override;"
    if old_decl in content:
        content = content.replace(old_decl, new_decl)
        if not dry_run:
            h_path.write_text(content, encoding='utf-8')
        log(f"  [UPDATED] DialogBox_Map.h: on_enable returns bool", log_file)
    elif new_decl in content:
        log(f"  [SKIP] DialogBox_Map.h: already returns bool", log_file)

    # Update cpp
    cpp_path = CLIENT_DIR / "DialogBox_Map.cpp"
    content = cpp_path.read_text(encoding='utf-8')
    old_sig = "void DialogBox_Map::on_enable(int type, int64_t v1, int v2, const char* string)"
    new_sig = "bool DialogBox_Map::on_enable(int type, int64_t v1, int v2, const char* string)"
    if old_sig in content:
        content = content.replace(old_sig, new_sig)
        # Add return true before closing brace of the method
        idx = content.find(new_sig)
        brace_start = content.find('{', idx)
        brace_count = 0
        end_idx = -1
        for i in range(brace_start, len(content)):
            if content[i] == '{':
                brace_count += 1
            elif content[i] == '}':
                brace_count -= 1
                if brace_count == 0:
                    end_idx = i
                    break
        if end_idx > 0:
            content = content[:end_idx] + "\treturn true;\n" + content[end_idx:]
        if not dry_run:
            cpp_path.write_text(content, encoding='utf-8')
        log(f"  [UPDATED] DialogBox_Map.cpp: on_enable returns bool", log_file)
    elif new_sig in content:
        log(f"  [SKIP] DialogBox_Map.cpp: already returns bool", log_file)


def update_itemcreator_on_disable(dry_run, log_file):
    """Update ItemCreator's existing on_disable to return bool."""
    log("\n=== Updating DialogBox_ItemCreator on_disable return type ===\n", log_file)

    # Update header
    h_path = CLIENT_DIR / "DialogBox_ItemCreator.h"
    content = h_path.read_text(encoding='utf-8')
    old_decl = "void on_disable() override;"
    new_decl = "bool on_disable() override;"
    if old_decl in content:
        content = content.replace(old_decl, new_decl)
        if not dry_run:
            h_path.write_text(content, encoding='utf-8')
        log(f"  [UPDATED] DialogBox_ItemCreator.h: on_disable returns bool", log_file)
    elif new_decl in content:
        log(f"  [SKIP] DialogBox_ItemCreator.h: already returns bool", log_file)

    # Update cpp
    cpp_path = CLIENT_DIR / "DialogBox_ItemCreator.cpp"
    content = cpp_path.read_text(encoding='utf-8')
    old_sig = "void DialogBox_ItemCreator::on_disable()"
    new_sig = "bool DialogBox_ItemCreator::on_disable()"
    if old_sig in content:
        content = content.replace(old_sig, new_sig)
        # Add return true at end of method
        # Find the closing brace of on_disable
        idx = content.find(new_sig)
        # Find the opening brace
        brace_start = content.find('{', idx)
        brace_count = 0
        end_idx = -1
        for i in range(brace_start, len(content)):
            if content[i] == '{':
                brace_count += 1
            elif content[i] == '}':
                brace_count -= 1
                if brace_count == 0:
                    end_idx = i
                    break
        if end_idx > 0:
            # Insert return true before closing brace
            content = content[:end_idx] + "\treturn true;\n" + content[end_idx:]

        if not dry_run:
            cpp_path.write_text(content, encoding='utf-8')
        log(f"  [UPDATED] DialogBox_ItemCreator.cpp: on_disable returns bool", log_file)
    elif new_sig in content:
        log(f"  [SKIP] DialogBox_ItemCreator.cpp: already returns bool", log_file)


def update_guildmenu_on_disable(dry_run, log_file):
    """GuildMenu already has text_input_manager include, but on_disable needs it."""
    # Already handled by the main include check
    pass


def update_existing_void_on_disable_to_bool(dry_run, log_file):
    """For dialogs that already have void on_disable() (non-ItemCreator),
    update their return type to bool and add return true."""
    log("\n=== Checking for existing void on_disable overrides to update ===\n", log_file)

    # Only Map has on_enable already, but we check for any existing on_disable
    # ItemCreator is handled separately above
    # Check all dialog files
    for h_path in sorted(CLIENT_DIR.glob("DialogBox_*.h")):
        if h_path.name == "DialogBox_ItemCreator.h":
            continue

        content = h_path.read_text(encoding='utf-8')
        if "void on_disable() override;" in content:
            log(f"  [NOTE] {h_path.name}: has void on_disable - needs update to bool", log_file)
            content = content.replace("void on_disable() override;", "bool on_disable() override;")
            if not dry_run:
                h_path.write_text(content, encoding='utf-8')
            log(f"  [UPDATED] {h_path.name}: on_disable returns bool", log_file)

            # Update the cpp too
            cpp_path = h_path.with_suffix('.cpp')
            if cpp_path.exists():
                cpp_content = cpp_path.read_text(encoding='utf-8')
                class_name = h_path.stem
                old_sig = f"void {class_name}::on_disable()"
                new_sig = f"bool {class_name}::on_disable()"
                if old_sig in cpp_content:
                    cpp_content = cpp_content.replace(old_sig, new_sig)
                    # Add return true before closing brace
                    idx = cpp_content.find(new_sig)
                    brace_start = cpp_content.find('{', idx)
                    brace_count = 0
                    end_idx = -1
                    for i in range(brace_start, len(cpp_content)):
                        if cpp_content[i] == '{':
                            brace_count += 1
                        elif cpp_content[i] == '}':
                            brace_count -= 1
                            if brace_count == 0:
                                end_idx = i
                                break
                    if end_idx > 0:
                        cpp_content = cpp_content[:end_idx] + "\treturn true;\n" + cpp_content[end_idx:]
                    if not dry_run:
                        cpp_path.write_text(cpp_content, encoding='utf-8')
                    log(f"  [UPDATED] {cpp_path.name}: on_disable returns bool", log_file)


def remove_unused_includes(dry_run, log_file):
    """Remove includes from DialogBoxManager.cpp that are no longer needed
    after the switch removal. We keep includes that are used elsewhere in the file."""
    log("\n=== Checking DialogBoxManager.cpp includes ===\n", log_file)

    filepath = CLIENT_DIR / "DialogBoxManager.cpp"
    content = filepath.read_text(encoding='utf-8')

    # These includes were only needed for the switch cases and can be removed
    # if they're not used elsewhere in the file after the switch removal
    candidates = [
        "BuildItemManager.h",
        "ShopManager.h",
    ]

    for inc in candidates:
        inc_str = f'#include "{inc}"'
        if inc_str in content:
            # Check if it's used elsewhere (beyond the include line itself)
            rest = content.replace(inc_str, '', 1)
            # Check for usage of the manager
            manager_name = inc.replace('.h', '').replace('Manager', '_manager')
            # BuildItemManager → build_item_manager, ShopManager → shop_manager
            if 'BuildItemManager' in inc:
                usage = 'build_item_manager'
            elif 'ShopManager' in inc:
                usage = 'shop_manager'
            else:
                continue

            if usage not in rest:
                content = content.replace(inc_str + '\n', '')
                log(f"  [REMOVED] {inc_str} (no longer needed)", log_file)

    if not dry_run:
        filepath.write_text(content, encoding='utf-8')


def main():
    parser = argparse.ArgumentParser(description="Phase 2: Enable/Disable Ownership")
    parser.add_argument("--dry-run", action="store_true", help="Preview changes without modifying files")
    args = parser.parse_args()

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    log_path = OUTPUT_DIR / "phase2_enable_disable_ownership.log"

    with open(log_path, 'w', encoding='utf-8') as log_file:
        mode_str = "DRY RUN" if args.dry_run else "APPLYING"
        log(f"Phase 2: Enable/Disable Ownership - {mode_str}", log_file)
        log(f"Timestamp: {datetime.now().isoformat()}", log_file)
        log(f"Client dir: {CLIENT_DIR}", log_file)

        # Step 1: Update IDialogBox.h — change on_disable return type
        update_idialogbox_h(args.dry_run, log_file)

        # Step 2: Process on_enable overrides
        process_enable_defs(args.dry_run, log_file)

        # Step 3: Process on_disable overrides
        process_disable_defs(args.dry_run, log_file)

        # Step 4: Update existing overrides to bool (Map on_enable, ItemCreator on_disable, any others)
        update_map_on_enable(args.dry_run, log_file)
        update_itemcreator_on_disable(args.dry_run, log_file)
        update_existing_void_on_disable_to_bool(args.dry_run, log_file)

        # Step 5: Add cancels_text_input_on_enable() = false overrides
        process_no_cancel_text_input(args.dry_run, log_file)

        # Step 6: Replace DialogBoxManager switches with generic flow
        update_dialogbox_manager(args.dry_run, log_file)

        # Step 7: Remove unused includes from DialogBoxManager.cpp
        remove_unused_includes(args.dry_run, log_file)

        log(f"\n=== Done! Log: {log_path} ===", log_file)


if __name__ == "__main__":
    main()
