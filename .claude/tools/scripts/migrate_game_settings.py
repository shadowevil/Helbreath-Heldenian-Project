#!/usr/bin/env python3
"""migrate_game_settings.py — Move gameplay members from CGame to Screen_OnGame

Touches ~30 files with ~350 systematic changes. Script appropriate because
all changes follow the same mechanical pattern (member access re-routing).

Usage:
  python migrate_game_settings.py --dry-run    # Preview to Scripts/output/
  python migrate_game_settings.py --verify     # Collision/conflict check
  python migrate_game_settings.py              # Apply changes
"""

import os
import re
import sys
from pathlib import Path

CLIENT = Path(r"Z:/Helbreath-3.82/Sources/Client")
OUTPUT = Path(r"Z:/Helbreath-3.82/Scripts/output")

# ─── Members moving from CGame to Screen_OnGame (sorted longest-first) ──────

MOVED = sorted([
	'm_is_get_pointing_mode', 'm_wait_for_new_click', 'm_magic_cast_time',
	'm_point_command_type', 'm_skill_using_status', 'm_item_using_status',
	'm_using_slate', 'm_down_skill_index', 'm_ilusion_owner_h', 'm_ilusion_owner_type',
	'm_draw_flag', 'm_is_crusade_mode', 'm_env_effect_time',
	'm_msg_text_list2', 'm_msg_text_list', 'm_agree_msg_text_list',
	'm_logout_count_time', 'm_logout_count',
	'm_fightzone_number_temp', 'm_fightzone_number',
	'm_quest', 'm_is_observer_mode', 'm_is_observer_commanded',
	'm_special_ability_setting_time', 'm_is_f1_help_window_enabled',
	'm_crusade_structure_info', 'm_commander_command_requested_time',
	'm_top_msg_last_sec', 'm_top_msg_time', 'm_top_msg',
	'm_gate_posit_x', 'm_gate_posit_y',
	'm_heldenian_aresden_left_tower', 'm_heldenian_elvine_left_tower',
	'm_heldenian_aresden_flags', 'm_heldenian_elvine_flags',
	'm_is_xmas', 'm_total_party_member', 'm_party_status',
	'm_gizon_item_upgrade_left',
], key=len, reverse=True)

ALT = '|'.join(re.escape(m) for m in MOVED)

# ─── Regex patterns ─────────────────────────────────────────────────────────

# m_game->m_MEMBER  (pointer access from dialog boxes, Screen_OnGame files, managers)
RE_MGAME_PTR = re.compile(r'm_game->(' + ALT + r')\b')

# game->m_MEMBER  (pointer, not preceded by word char — excludes m_game->)
RE_GAME_PTR = re.compile(r'(?<!\w)game->(' + ALT + r')\b')

# m_game.m_MEMBER  (reference access from renderers, DialogBoxManager)
RE_MGAME_REF = re.compile(r'm_game\.(' + ALT + r')\b')

# game.m_MEMBER  (reference, not preceded by word char — RenderHelpers)
RE_GAME_REF = re.compile(r'(?<!\w)game\.(' + ALT + r')\b')

# Standalone m_MEMBER in CGame methods (not preceded by -> or . or word char)
RE_DIRECT = re.compile(r'(?<!->)(?<!\.)(?<!\w)(' + ALT + r')\b')

# ─── File categories ────────────────────────────────────────────────────────

# Cat A: Screen_OnGame methods — m_game->m_MEMBER → m_MEMBER
CAT_A = [
	'Screen_OnGame.DrawObjects.cpp',
	'Screen_OnGame.Hotkeys.cpp',
	'Screen_OnGame.Network.cpp',
	# Screen_OnGame.cpp handled specially (also inlines init_game_settings)
]

# Cat B: Renderers — m_game.m_MEMBER → m_screen->m_MEMBER
CAT_B = ['PlayerRenderer.cpp', 'NpcRenderer.cpp']

# Cat C: m_game-> files — m_game->m_MEMBER → m_game->on_game()->m_MEMBER
CAT_C = [
	'DialogBox_CityHallMenu.cpp', 'DialogBox_Commander.cpp',
	'DialogBox_Constructor.cpp', 'DialogBox_GuildMenu.cpp',
	'DialogBox_GuildHallMenu.cpp', 'DialogBox_HudPanel.cpp',
	'DialogBox_Soldier.cpp', 'DialogBox_Inventory.cpp',
	'DialogBox_Party.cpp', 'DialogBox_Skill.cpp',
	'DialogBox_ChangeStatsMajestic.cpp', 'DialogBox_Fishing.cpp',
	'DialogBox_Manufacture.cpp', 'DialogBox_Resurrect.cpp',
	'DialogBox_SysMenu.cpp', 'DialogBox_Quest.cpp',
	'DialogBox_Text.cpp', 'DialogBox_NpcTalk.cpp',
	'DialogBox_Help.cpp', 'DialogBox_ItemUpgrade.cpp',
	'DialogBox_LevelUpSetting.cpp',
	'MagicCastingSystem.cpp', 'EventListManager.cpp',
	'InventoryManager.cpp', 'QuestManager.cpp',
]

# Cat D: NetworkMessages — game->m_MEMBER → game->on_game()->m_MEMBER
CAT_D = [
	'NetworkMessages_Combat.cpp', 'NetworkMessages_Skills.cpp',
	'NetworkMessages_Slates.cpp', 'NetworkMessages_Crusade.cpp',
	'NetworkMessages_System.cpp', 'NetworkMessages_Party.cpp',
	'NetworkMessages_Items.cpp', 'NetworkMessages_Apocalypse.cpp',
	'NetworkMessages_Heldenian.cpp', 'NetworkMessages_Stats.cpp',
]

# ─── Helpers ────────────────────────────────────────────────────────────────

def read_file(path):
	# Default newline handling: \r\n → \n on read
	with open(path, 'r', encoding='utf-8') as f:
		return f.read()

def write_file(path, content):
	# Force Unix line endings
	with open(path, 'w', encoding='utf-8', newline='\n') as f:
		f.write(content)

def add_include(content, header):
	"""Add #include after the last existing #include block."""
	inc = f'#include "{header}"'
	if inc in content:
		return content
	# Find last #include line
	lines = content.split('\n')
	last_inc = -1
	for i, line in enumerate(lines):
		if line.strip().startswith('#include'):
			last_inc = i
	if last_inc >= 0:
		lines.insert(last_inc + 1, inc)
	return '\n'.join(lines)

# ─── Game.h transformations ─────────────────────────────────────────────────

# Exact text blocks to remove from Game.h (member declarations)
GAME_H_REMOVALS = [
	# m_quest struct (lines 257-262)
	"""\tstruct {
\t\tbool is_quest_completed;
\t\tshort who, quest_type, contribution, target_type, target_count, x, y, range;
\t\tshort current_count;
\t\tstd::string target_name;
\t} m_quest;\n""",

	# m_crusade_structure_info struct (lines 264-268)
	"""\tstruct {
\t\tshort x, y;
\t\tchar type;
\t\tchar side;
\t} m_crusade_structure_info[hb::shared::limits::MaxCrusadeStructures];\n""",

	# m_msg_text_list (line 295)
	"\tstd::array<std::unique_ptr<CMsg>, game_limits::max_text_dlg_lines> m_msg_text_list;\n",
	# m_msg_text_list2 (line 296)
	"\tstd::array<std::unique_ptr<CMsg>, game_limits::max_text_dlg_lines> m_msg_text_list2;\n",
	# m_agree_msg_text_list (line 297)
	"\tstd::array<std::unique_ptr<CMsg>, game_limits::max_text_dlg_lines> m_agree_msg_text_list;\n",

	# Time members (lines 309, 313-316)
	"\tuint32_t m_logout_count_time;\n",
	"\tuint32_t m_special_ability_setting_time;\n",
	"\tuint32_t m_commander_command_requested_time;\n",
	"\tuint32_t m_top_msg_time;\n",
	"\tuint32_t m_env_effect_time;\n",

	# Bool members (lines 327-332, 335, 337)
	"\tbool m_is_get_pointing_mode;\n",
	"\tbool m_wait_for_new_click;  // After magic cast, ignore held click until released\n",
	"\tuint32_t m_magic_cast_time;  // Timestamp when magic was cast (for post-cast delay)\n",
	"\tbool m_skill_using_status;\n",
	"\tbool m_item_using_status;\n",
	"\tbool m_is_observer_mode, m_is_observer_commanded;\n",
	"\tbool m_is_crusade_mode;\n",
	"\tbool m_is_f1_help_window_enabled;\n",

	# Int members (lines 351-352, 354, 359, 361, 364)
	"\tint m_fightzone_number;\n",
	"\tint m_fightzone_number_temp;\n",
	"\tint m_point_command_type;\n",
	"\tint m_down_skill_index;\n",
	"\tint m_ilusion_owner_h;\n",
	"\tint m_draw_flag;\n",

	# Mixed members (lines 369, 371-373, 385)
	"\tunsigned char m_top_msg_last_sec;\n",
	"\tint m_total_party_member;\n",
	"\tint m_party_status;\n",
	"\tint m_gizon_item_upgrade_left;\n",
	"\tint  m_logout_count;\n",

	# Char member (line 412)
	"\tchar m_ilusion_owner_type;\n",

	# String member (line 418)
	"\tstd::string m_top_msg;\n",

	# Gate/heldenian/xmas/slate (lines 459-466)
	"\tint  m_gate_posit_x, m_gate_posit_y;\n",
	"\tint m_heldenian_aresden_left_tower;\n",
	"\tint m_heldenian_elvine_left_tower;\n",
	"\tint m_heldenian_aresden_flags;\n",
	"\tint m_heldenian_elvine_flags;\n",
	"\tbool m_is_xmas;\n",
	"\tbool m_using_slate;\n",

	# init_game_settings declaration (line 182)
	"\tvoid init_game_settings();\n",
]

def transform_game_h(content):
	"""Remove moved member declarations and init_game_settings from Game.h."""
	changes = []

	for block in GAME_H_REMOVALS:
		if block in content:
			content = content.replace(block, '', 1)
			# Extract member name for logging
			short = block.strip()[:60].replace('\n', ' | ')
			changes.append(f"  Removed: {short}...")
		else:
			short = block.strip()[:60].replace('\n', ' | ')
			changes.append(f"  WARNING: Could not find block to remove: {short}...")

	# Add forward declaration and on_game() method
	# Add after the existing 'class floating_text_manager;' forward decl
	fwd_marker = "class floating_text_manager;"
	if fwd_marker in content and "class Screen_OnGame;" not in content:
		content = content.replace(
			fwd_marker,
			fwd_marker + "\nclass Screen_OnGame;",
			1
		)
		changes.append("  Added forward declaration: class Screen_OnGame;")

	# Add on_game() method declaration after get_dialog_box_manager()
	on_game_decl = "\n\t// Convenience accessor — routes through the active Screen_OnGame gameplay state\n\tScreen_OnGame* on_game();\n"
	marker = "\tDialogBoxManager& get_dialog_box_manager();"
	if marker in content and "Screen_OnGame* on_game();" not in content:
		content = content.replace(marker, marker + on_game_decl, 1)
		changes.append("  Added: Screen_OnGame* on_game(); declaration")

	return content, changes

# ─── Screen_OnGame.h transformations ────────────────────────────────────────

SCREEN_ON_GAME_NEW_MEMBERS = (
	"    // --- Gameplay state (moved from CGame) ---\n"
	"    bool m_is_get_pointing_mode = false;\n"
	"    bool m_wait_for_new_click = false;\n"
	"    uint32_t m_magic_cast_time = 0;\n"
	"    int m_point_command_type = -1;\n"
	"    bool m_skill_using_status = false;\n"
	"    bool m_item_using_status = false;\n"
	"    bool m_using_slate = false;\n"
	"    int m_down_skill_index = -1;\n"
	"    int m_ilusion_owner_h = 0;\n"
	"    char m_ilusion_owner_type = 0;\n"
	"    int m_draw_flag = 0;\n"
	"    bool m_is_crusade_mode = false;\n"
	"    uint32_t m_env_effect_time = 0;\n"
	"    std::array<std::unique_ptr<CMsg>, game_limits::max_text_dlg_lines> m_msg_text_list;\n"
	"    std::array<std::unique_ptr<CMsg>, game_limits::max_text_dlg_lines> m_msg_text_list2;\n"
	"    std::array<std::unique_ptr<CMsg>, game_limits::max_text_dlg_lines> m_agree_msg_text_list;\n"
	"    int m_logout_count = -1;\n"
	"    uint32_t m_logout_count_time = 0;\n"
	"    int m_fightzone_number = 0;\n"
	"    int m_fightzone_number_temp = 0;\n"
	"    struct {\n"
	"        bool is_quest_completed = false;\n"
	"        short who = 0, quest_type = 0, contribution = 0, target_type = 0, target_count = 0, x = 0, y = 0, range = 0;\n"
	"        short current_count = 0;\n"
	"        std::string target_name;\n"
	"    } m_quest;\n"
	"    bool m_is_observer_mode = false;\n"
	"    bool m_is_observer_commanded = false;\n"
	"    uint32_t m_special_ability_setting_time = 0;\n"
	"    bool m_is_f1_help_window_enabled = false;\n"
	"    struct {\n"
	"        short x = 0, y = 0;\n"
	"        char type = 0;\n"
	"        char side = 0;\n"
	"    } m_crusade_structure_info[hb::shared::limits::MaxCrusadeStructures]{};\n"
	"    uint32_t m_commander_command_requested_time = 0;\n"
	"    unsigned char m_top_msg_last_sec = 0;\n"
	"    uint32_t m_top_msg_time = 0;\n"
	"    std::string m_top_msg;\n"
	"    int m_gate_posit_x = -1;\n"
	"    int m_gate_posit_y = -1;\n"
	"    int m_heldenian_aresden_left_tower = -1;\n"
	"    int m_heldenian_elvine_left_tower = -1;\n"
	"    int m_heldenian_aresden_flags = -1;\n"
	"    int m_heldenian_elvine_flags = -1;\n"
	"    bool m_is_xmas = false;\n"
	"    int m_total_party_member = 0;\n"
	"    int m_party_status = 0;\n"
	"    int m_gizon_item_upgrade_left = 0;\n"
)

def transform_screen_on_game_h(content):
	"""Add moved member declarations to Screen_OnGame.h."""
	changes = []

	# Add includes needed for the new members
	if '#include "ChatMsg.h"' not in content:
		content = add_include(content, 'ChatMsg.h')
		changes.append('  Added #include "ChatMsg.h"')
	if '#include "GameConstants.h"' not in content:
		content = add_include(content, 'GameConstants.h')
		changes.append('  Added #include "GameConstants.h"')
	if '#include "GlobalDef.h"' not in content:
		content = add_include(content, 'GlobalDef.h')
		changes.append('  Added #include "GlobalDef.h"')

	# Insert new public members before the second private: section (screen-specific state)
	# Replace "private:\n    // Screen-specific state" with "public:\n    new members\n\nprivate:\n    // Screen-specific state"
	old_marker = "private:\n    // Screen-specific state (previously file-scope static variables)"
	new_marker = (
		"public:\n"
		+ SCREEN_ON_GAME_NEW_MEMBERS
		+ "\nprivate:\n"
		+ "    // Screen-specific state (previously file-scope static variables)"
	)
	if old_marker in content:
		content = content.replace(old_marker, new_marker, 1)
		changes.append("  Added ~40 gameplay member declarations (public section)")
	else:
		changes.append("  WARNING: Could not find insertion marker for new members")

	return content, changes

# ─── Game.cpp transformations ───────────────────────────────────────────────

# The init_game_settings function body to delete (lines 1541-1642)
INIT_GAME_SETTINGS_START = "void CGame::init_game_settings()\n{"
INIT_GAME_SETTINGS_END_MARKER = "// create_new_guild_response_handler MOVED"

# on_game() implementation to add
ON_GAME_IMPL = """
Screen_OnGame* CGame::on_game()
{
\treturn get_active_screen_as<Screen_OnGame>();
}

"""

def transform_game_cpp(content):
	"""Delete init_game_settings, add on_game(), fix remaining refs."""
	changes = []

	# 1. Delete init_game_settings function body
	start = content.find(INIT_GAME_SETTINGS_START)
	end_marker = content.find(INIT_GAME_SETTINGS_END_MARKER)
	if start >= 0 and end_marker >= 0:
		content = content[:start] + content[end_marker:]
		changes.append("  Deleted init_game_settings() function body")
	else:
		changes.append("  WARNING: Could not find init_game_settings() to delete")

	# 2. Add on_game() implementation after the deleted function location
	if "Screen_OnGame* CGame::on_game()" not in content:
		# Insert before the marker that was left
		content = content.replace(
			INIT_GAME_SETTINGS_END_MARKER,
			ON_GAME_IMPL + INIT_GAME_SETTINGS_END_MARKER,
			1
		)
		changes.append("  Added on_game() implementation")

	# 3. Remove constructor line: m_is_observer_mode = false;
	old_ctor = "\tm_is_observer_mode = false;\n"
	if old_ctor in content:
		content = content.replace(old_ctor, '', 1)
		changes.append("  Removed m_is_observer_mode init from constructor")

	# 4. Replace remaining direct member accesses in CGame methods.
	# These are m_MEMBER accesses that are NOT preceded by -> or .
	# We replace them with on_game()->m_MEMBER.
	# We must be careful to only replace in method bodies, not in:
	# - Comments (lines starting with //)
	# - String literals
	# - The deleted function (already handled above)

	lines = content.split('\n')
	new_lines = []
	total_replacements = 0
	for line in lines:
		stripped = line.lstrip()
		# Skip comment-only lines
		if stripped.startswith('//'):
			new_lines.append(line)
			continue
		# Skip lines that are string literals containing member names (unlikely but safe)
		if stripped.startswith('"'):
			new_lines.append(line)
			continue
		# Apply replacement: standalone m_MEMBER → on_game()->m_MEMBER
		# But NOT if already prefixed with -> or . (handled by lookbehind)
		new_line, n = RE_DIRECT.subn(r'on_game()->\1', line)
		if n > 0:
			total_replacements += n
		new_lines.append(new_line)
	content = '\n'.join(new_lines)
	if total_replacements:
		changes.append(f"  Replaced {total_replacements} direct member refs with on_game()->")

	return content, changes

# ─── Screen_OnGame.cpp transformations ──────────────────────────────────────

I = "    "  # 4-space indent for Screen_OnGame files
I2 = "        "  # 8-space indent

INLINED_INIT = (
	f"{I}// Reset CGame-level state\n"
	f"{I}m_game->m_check_connection_time = 0;\n"
	f"{I}m_game->m_Camera.set_shake(0);\n"
	f"\n"
	f"{I}for (int r = 0; r < 4; r++) m_game->m_config_retry[r] = CGame::ConfigRetryLevel::None;\n"
	f"{I}m_game->m_config_request_time = 0;\n"
	f"{I}m_game->m_init_data_ready = false;\n"
	f"{I}m_game->m_configs_ready = false;\n"
	f"\n"
	f"{I}CursorTarget::reset_selection_click_time();\n"
	f"\n"
	f"{I}m_game->m_net_lag_count = 0;\n"
	f"{I}m_game->m_latency_ms = -1;\n"
	f"{I}m_game->m_last_net_msg_id = 0;\n"
	f"{I}m_game->m_last_net_msg_time = 0;\n"
	f"{I}m_game->m_last_net_msg_size = 0;\n"
	f"{I}m_game->m_last_net_recv_time = 0;\n"
	f"{I}m_game->m_last_npc_event_time = 0;\n"
	f"\n"
	f"{I}if (m_game->m_effect_manager) m_game->m_effect_manager->clear_all_effects();\n"
	f"\n"
	f"{I}m_floating_text.clear_all();\n"
	f"\n"
	f"{I}ChatManager::get().clear_messages();\n"
	f"{I}ChatManager::get().clear_whispers();\n"
	f"\n"
	f"{I}m_game->m_force_disconn = false;\n"
	f"\n"
	f"{I}CursorTarget::clear_selection();\n"
	f"\n"
	f"{I}teleport_manager::get().reset();\n"
	f"\n"
	f"{I}// Reset gameplay state (now on Screen_OnGame)\n"
	f"{I}m_is_get_pointing_mode = false;\n"
	f"{I}m_wait_for_new_click = false;\n"
	f"{I}m_magic_cast_time = 0;\n"
	f"{I}m_point_command_type = -1;\n"
	f"\n"
	f"{I}m_skill_using_status = false;\n"
	f"{I}m_item_using_status = false;\n"
	f"{I}m_using_slate = false;\n"
	f"\n"
	f"{I}m_down_skill_index = -1;\n"
	f"\n"
	f"{I}m_ilusion_owner_h = 0;\n"
	f"{I}m_ilusion_owner_type = 0;\n"
	f"\n"
	f"{I}m_draw_flag = 0;\n"
	f"{I}m_is_crusade_mode = false;\n"
	f"\n"
	f"{I}m_env_effect_time = GameClock::get_time_ms();\n"
	f"\n"
	f"{I}for (int i = 0; i < game_limits::max_text_dlg_lines; i++)\n"
	f"{I}{{\n"
	f"{I2}if (m_msg_text_list[i] != 0)\n"
	f"{I2}    m_msg_text_list[i].reset();\n"
	f"\n"
	f"{I2}if (m_msg_text_list2[i] != 0)\n"
	f"{I2}    m_msg_text_list2[i].reset();\n"
	f"\n"
	f"{I2}if (m_agree_msg_text_list[i] != 0)\n"
	f"{I2}    m_agree_msg_text_list[i].reset();\n"
	f"{I}}}\n"
	f"\n"
	f"{I}m_logout_count = -1;\n"
	f"{I}m_logout_count_time = 0;\n"
	f"{I}m_fightzone_number = 0;\n"
	f"{I}m_fightzone_number_temp = 0;\n"
	f"{I}m_quest.who = 0;\n"
	f"{I}m_quest.quest_type = 0;\n"
	f"{I}m_quest.contribution = 0;\n"
	f"{I}m_quest.target_type = 0;\n"
	f"{I}m_quest.target_count = 0;\n"
	f"{I}m_quest.current_count = 0;\n"
	f"{I}m_quest.x = 0;\n"
	f"{I}m_quest.y = 0;\n"
	f"{I}m_quest.range = 0;\n"
	f"{I}m_quest.is_quest_completed = false;\n"
	f"{I}m_is_observer_mode = false;\n"
	f"{I}m_is_observer_commanded = false;\n"
	f"{I}m_special_ability_setting_time = 0;\n"
	f"{I}m_is_f1_help_window_enabled = false;\n"
	f"{I}for (int i = 0; i < hb::shared::limits::MaxCrusadeStructures; i++)\n"
	f"{I}{{\n"
	f"{I2}m_crusade_structure_info[i].type = 0;\n"
	f"{I2}m_crusade_structure_info[i].side = 0;\n"
	f"{I2}m_crusade_structure_info[i].x = 0;\n"
	f"{I2}m_crusade_structure_info[i].y = 0;\n"
	f"{I}}}\n"
	f"{I}m_commander_command_requested_time = 0;\n"
	f"{I}m_top_msg_last_sec = 0;\n"
	f"{I}m_top_msg_time = 0;\n"
	f"\n"
	f"{I}m_gate_posit_x = m_gate_posit_y = -1;\n"
	f"{I}m_heldenian_aresden_left_tower = -1;\n"
	f"{I}m_heldenian_elvine_left_tower = -1;\n"
	f"{I}m_heldenian_aresden_flags = -1;\n"
	f"{I}m_heldenian_elvine_flags = -1;\n"
	f"{I}m_is_xmas = false;\n"
	f"{I}m_total_party_member = 0;\n"
	f"{I}m_party_status = 0;\n"
	f"{I}m_gizon_item_upgrade_left = 0;\n"
)

def transform_screen_on_game_cpp(content):
	"""Replace init_game_settings() call with inlined init, then do Cat A replacement."""
	changes = []

	# 1. Replace the init_game_settings() call with inlined code
	old_call_block = (
		"    // Reset all gameplay state and create the player\n"
		"    m_game->init_game_settings();\n"
		"\n"
		"    // Wire the static player into CGame for gameplay access\n"
		"    m_game->m_player = s_player.get();"
	)

	new_call_block = (
		"    // Create the player\n"
		"    Screen_OnGame::create_player();\n"
		"    m_game->m_player = s_player.get();\n"
		"    combat_system::get().set_player(*s_player);\n"
		"\n"
	) + INLINED_INIT

	if old_call_block in content:
		content = content.replace(old_call_block, new_call_block, 1)
		changes.append("  Replaced init_game_settings() call with inlined initialization")
	else:
		changes.append("  WARNING: Could not find init_game_settings() call block to replace")

	# 2. Add CombatSystem include if needed
	if '#include "CombatSystem.h"' not in content:
		content = add_include(content, 'CombatSystem.h')
		changes.append('  Added #include "CombatSystem.h"')

	# 3. Add GameClock include if needed (for GameClock::get_time_ms())
	if '#include "GameTimer.h"' not in content and 'GameClock::' in content:
		content = add_include(content, 'GameTimer.h')
		changes.append('  Added #include "GameTimer.h"')

	# 4. Apply Cat A replacement: m_game->m_MEMBER → m_MEMBER
	new_content, n = RE_MGAME_PTR.subn(r'\1', content)
	if n:
		changes.append(f"  m_game->m_MEMBER → m_MEMBER: {n} replacements")
		content = new_content

	return content, changes

# ─── RenderHelpers.cpp transformation ───────────────────────────────────────

def transform_render_helpers(content):
	"""Replace game.m_draw_flag with game.on_game()->m_draw_flag."""
	changes = []
	new_content, n = RE_GAME_REF.subn(r'game.on_game()->\1', content)
	if n:
		changes.append(f"  game.m_MEMBER → game.on_game()->m_MEMBER: {n} replacements")
		content = new_content
	if n and '#include "Screen_OnGame.h"' not in content:
		content = add_include(content, 'Screen_OnGame.h')
		changes.append('  Added #include "Screen_OnGame.h"')
	return content, changes

# ─── DialogBoxManager.cpp transformation ────────────────────────────────────

def transform_dialog_box_manager(content):
	"""Replace m_game.m_MEMBER with m_game.on_game()->m_MEMBER."""
	changes = []
	new_content, n = RE_MGAME_REF.subn(r'm_game.on_game()->\1', content)
	if n:
		changes.append(f"  m_game.m_MEMBER → m_game.on_game()->m_MEMBER: {n} replacements")
		content = new_content
	if n and '#include "Screen_OnGame.h"' not in content:
		content = add_include(content, 'Screen_OnGame.h')
		changes.append('  Added #include "Screen_OnGame.h"')
	return content, changes

# ─── Generic category transforms ────────────────────────────────────────────

def transform_cat_a(content):
	"""Cat A: m_game->m_MEMBER → m_MEMBER"""
	changes = []
	new_content, n = RE_MGAME_PTR.subn(r'\1', content)
	if n:
		changes.append(f"  m_game->m_MEMBER → m_MEMBER: {n} replacements")
	return new_content, changes

def transform_cat_b(content):
	"""Cat B: m_game.m_MEMBER → m_screen->m_MEMBER"""
	changes = []
	new_content, n = RE_MGAME_REF.subn(r'm_screen->\1', content)
	if n:
		changes.append(f"  m_game.m_MEMBER → m_screen->m_MEMBER: {n} replacements")
	return new_content, changes

def transform_cat_c(content):
	"""Cat C: m_game->m_MEMBER → m_game->on_game()->m_MEMBER"""
	changes = []
	new_content, n = RE_MGAME_PTR.subn(r'm_game->on_game()->\1', content)
	if n:
		changes.append(f"  m_game->m_MEMBER → m_game->on_game()->m_MEMBER: {n} replacements")
		content = new_content
	if n and '#include "Screen_OnGame.h"' not in content:
		content = add_include(content, 'Screen_OnGame.h')
		changes.append('  Added #include "Screen_OnGame.h"')
	return content, changes

def transform_cat_d(content):
	"""Cat D: game->m_MEMBER → game->on_game()->m_MEMBER"""
	changes = []
	new_content, n = RE_GAME_PTR.subn(r'game->on_game()->\1', content)
	if n:
		changes.append(f"  game->m_MEMBER → game->on_game()->m_MEMBER: {n} replacements")
		content = new_content
	if n and '#include "Screen_OnGame.h"' not in content:
		content = add_include(content, 'Screen_OnGame.h')
		changes.append('  Added #include "Screen_OnGame.h"')
	return content, changes

# ─── Verify mode ────────────────────────────────────────────────────────────

def verify():
	"""Check for collisions and conflicts."""
	issues = []

	# Check that moved members don't exist in Shared/ or SFMLEngine/
	shared_dir = CLIENT.parent / "Dependencies" / "Shared"
	sfml_dir = CLIENT.parent / "SFMLEngine"
	for d, label in [(shared_dir, "Shared"), (sfml_dir, "SFMLEngine")]:
		if not d.exists():
			continue
		for ext in ('*.h', '*.cpp'):
			for f in d.rglob(ext):
				try:
					text = read_file(f)
				except:
					continue
				for m in MOVED:
					if m in text:
						issues.append(f"COLLISION: {m} found in {label}/{f.name}")

	# Check for C++ keyword conflicts (member names matching keywords)
	keywords = {'class', 'struct', 'enum', 'int', 'bool', 'char', 'void', 'return',
				'if', 'else', 'for', 'while', 'switch', 'case', 'break', 'continue',
				'public', 'private', 'protected', 'virtual', 'override', 'const',
				'static', 'inline', 'template', 'typename', 'namespace', 'using'}
	for m in MOVED:
		bare = m[2:]  # strip m_
		if bare in keywords:
			issues.append(f"KEYWORD CONFLICT: {m} strips to keyword '{bare}'")

	# Check that on_game() doesn't already exist
	game_h = read_file(CLIENT / "Game.h")
	if "on_game()" in game_h:
		issues.append("WARNING: on_game() already exists in Game.h")

	# Check that Screen_OnGame.h doesn't already have these members
	sog_h = read_file(CLIENT / "Screen_OnGame.h")
	for m in MOVED[:5]:  # spot check
		if re.search(r'\b' + re.escape(m) + r'\b', sog_h):
			issues.append(f"DUPLICATE: {m} already declared in Screen_OnGame.h")

	# Check all files in categories exist
	all_files = CAT_A + CAT_B + CAT_C + CAT_D + [
		'Screen_OnGame.cpp', 'Game.h', 'Screen_OnGame.h', 'Game.cpp',
		'RenderHelpers.cpp', 'DialogBoxManager.cpp'
	]
	for f in all_files:
		if not (CLIENT / f).exists():
			issues.append(f"MISSING FILE: {f}")

	return issues

# ─── Main ───────────────────────────────────────────────────────────────────

def main():
	mode = '--dry-run' if '--dry-run' in sys.argv else ('--verify' if '--verify' in sys.argv else 'apply')

	if mode == '--verify':
		print("=== Verification ===")
		issues = verify()
		if issues:
			for issue in issues:
				print(f"  {issue}")
			print(f"\n{len(issues)} issue(s) found.")
		else:
			print("  No issues found. Safe to apply.")
		return

	# Build list of (filepath, transform_func) pairs
	transforms = []

	# Special files
	transforms.append((CLIENT / 'Game.h', transform_game_h))
	transforms.append((CLIENT / 'Screen_OnGame.h', transform_screen_on_game_h))
	transforms.append((CLIENT / 'Game.cpp', transform_game_cpp))
	transforms.append((CLIENT / 'Screen_OnGame.cpp', transform_screen_on_game_cpp))
	transforms.append((CLIENT / 'RenderHelpers.cpp', transform_render_helpers))
	transforms.append((CLIENT / 'DialogBoxManager.cpp', transform_dialog_box_manager))

	# Category files
	for f in CAT_A:
		transforms.append((CLIENT / f, transform_cat_a))
	for f in CAT_B:
		transforms.append((CLIENT / f, transform_cat_b))
	for f in CAT_C:
		transforms.append((CLIENT / f, transform_cat_c))
	for f in CAT_D:
		transforms.append((CLIENT / f, transform_cat_d))

	# Process all files
	all_changes = {}
	total_files = 0
	total_changes = 0

	for filepath, transform_func in transforms:
		if not filepath.exists():
			print(f"SKIP (not found): {filepath.name}")
			continue

		content = read_file(filepath)
		new_content, changes = transform_func(content)

		if changes:
			all_changes[filepath.name] = changes
			total_files += 1
			total_changes += len(changes)

		if mode == '--dry-run':
			# Write preview to output dir
			OUTPUT.mkdir(parents=True, exist_ok=True)
			out_path = OUTPUT / filepath.name
			write_file(out_path, new_content)
		else:
			# Apply changes
			if new_content != content:
				write_file(filepath, new_content)

	# Report (force UTF-8 for arrows in change descriptions)
	import io
	sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
	print(f"\n{'=== DRY RUN ===' if mode == '--dry-run' else '=== APPLIED ==='}")
	print(f"Files processed: {total_files}")
	print(f"Change groups: {total_changes}")
	print()
	for filename, changes in sorted(all_changes.items()):
		print(f"{filename}:")
		for c in changes:
			print(f"  {c}")
		print()

	if mode == '--dry-run':
		print(f"Preview files written to: {OUTPUT}")
		print("Run without --dry-run to apply.")

if __name__ == '__main__':
	main()
