// Screen_OnGame.Hotkeys.cpp: Hotkey dispatch and handlers for gameplay screen
//
//////////////////////////////////////////////////////////////////////

#include "Screen_OnGame.h"
#include "Game.h"
#include "PacketSendHelpers.h"

#include "TextInputManager.h"
#include "MagicCastingSystem.h"
#include "ChatManager.h"
#include "AudioManager.h"
#include "ConfigManager.h"
#include "HotkeyManager.h"
#include "DialogBox_ChatHistory.h"
#include "DialogBox_ItemDropAmount.h"
#include "InventoryManager.h"
#include "lan_eng.h"
#include "GlobalDef.h"
#include "IInput.h"
#include <format>
#include <string>

using namespace hb::shared::net;
namespace MouseButton = hb::shared::input::MouseButton;

// ============== Hotkey Registration (Ctrl+letter combos fire on key-up) ==============

void Screen_OnGame::register_hotkeys()
{
	auto& hotkeys = HotkeyManager::get();
	hotkeys.clear();

	HotkeyManager::KeyCombo ctrlOnly{ KeyCode::Unknown, true, false, false };

	// Complex Ctrl combos — delegate to named methods
	hotkeys.register_hotkey({ KeyCode::A, ctrlOnly.ctrl, ctrlOnly.shift, ctrlOnly.alt }, HotkeyManager::Trigger::KeyUp,
		[this]() { hotkey_toggle_force_attack(); });
	hotkeys.register_hotkey({ KeyCode::D, ctrlOnly.ctrl, ctrlOnly.shift, ctrlOnly.alt }, HotkeyManager::Trigger::KeyUp,
		[this]() { hotkey_cycle_detail_level(); });
	hotkeys.register_hotkey({ KeyCode::S, ctrlOnly.ctrl, ctrlOnly.shift, ctrlOnly.alt }, HotkeyManager::Trigger::KeyUp,
		[this]() { hotkey_toggle_sound_and_music(); });
	hotkeys.register_hotkey({ KeyCode::T, ctrlOnly.ctrl, ctrlOnly.shift, ctrlOnly.alt }, HotkeyManager::Trigger::KeyUp,
		[this]() { hotkey_whisper_target(); });

	// Trivial Ctrl combos — inlined in lambda
	hotkeys.register_hotkey({ KeyCode::H, ctrlOnly.ctrl, ctrlOnly.shift, ctrlOnly.alt }, HotkeyManager::Trigger::KeyUp,
		[this]()
		{
			if (!hb::shared::input::is_ctrl_down() || GameModeManager::get_mode() != GameMode::MainGame || text_input_manager::get().is_active())
				return;
			if (get_dialog_box_manager().is_enabled(DialogBoxId::Help) == false)
				get_dialog_box_manager().enable_dialog_box(DialogBoxId::Help, 0, 0, 0);
			else
			{
				get_dialog_box_manager().disable_dialog_box(DialogBoxId::Help);
				get_dialog_box_manager().disable_dialog_box(DialogBoxId::Text);
			}
		});
	hotkeys.register_hotkey({ KeyCode::W, ctrlOnly.ctrl, ctrlOnly.shift, ctrlOnly.alt }, HotkeyManager::Trigger::KeyUp,
		[this]()
		{
			if (!hb::shared::input::is_ctrl_down() || GameModeManager::get_mode() != GameMode::MainGame || text_input_manager::get().is_active())
				return;
			bool enabled = config_manager::get().is_dialog_transparency_enabled();
			config_manager::get().set_dialog_transparency_enabled(!enabled);
		});
	hotkeys.register_hotkey({ KeyCode::X, ctrlOnly.ctrl, ctrlOnly.shift, ctrlOnly.alt }, HotkeyManager::Trigger::KeyUp,
		[this]()
		{
			if (!hb::shared::input::is_ctrl_down() || GameModeManager::get_mode() != GameMode::MainGame || text_input_manager::get().is_active())
				return;
			if (get_dialog_box_manager().is_enabled(DialogBoxId::SystemMenu) == false)
				get_dialog_box_manager().enable_dialog_box(DialogBoxId::SystemMenu, 0, 0, 0);
			else get_dialog_box_manager().disable_dialog_box(DialogBoxId::SystemMenu);
		});
	hotkeys.register_hotkey({ KeyCode::M, ctrlOnly.ctrl, ctrlOnly.shift, ctrlOnly.alt }, HotkeyManager::Trigger::KeyUp,
		[this]()
		{
			if (GameModeManager::get_mode() != GameMode::MainGame || !hb::shared::input::is_ctrl_down())
				return;
			if (get_dialog_box_manager().is_enabled(DialogBoxId::GuideMap) == true) get_dialog_box_manager().disable_dialog_box(DialogBoxId::GuideMap);
			else get_dialog_box_manager().enable_dialog_box(DialogBoxId::GuideMap, 0, 0, 0, 0);
		});
	hotkeys.register_hotkey({ KeyCode::R, ctrlOnly.ctrl, ctrlOnly.shift, ctrlOnly.alt }, HotkeyManager::Trigger::KeyUp,
		[this]()
		{
			if (!hb::shared::input::is_ctrl_down() || GameModeManager::get_mode() != GameMode::MainGame || text_input_manager::get().is_active())
				return;
			bool runningMode = config_manager::get().is_running_mode_enabled();
			if (runningMode)
			{
				config_manager::get().set_running_mode_enabled(false);
				m_game->add_event_list(NOTIFY_MSG_CONVERT_WALKING_MODE, 10);
			}
			else
			{
				config_manager::get().set_running_mode_enabled(true);
				m_game->add_event_list(NOTIFY_MSG_CONVERT_RUNNING_MODE, 10);
			}
		});
}

// ============== Key-up dispatch ==============

bool Screen_OnGame::on_key_up(KeyCode key)
{
	HotkeyManager::get().handle_key_up(key);
	return true;
}

// ============== Key-down dispatch ==============

bool Screen_OnGame::on_key_down(KeyCode key)
{
	// Potions fire repeatedly while held — allow auto-repeat with 500ms cooldown
	if (key == KeyCode::Insert || key == KeyCode::Delete)
	{
		static uint32_t last_potion_time = 0;
		uint32_t now = GameClock::get_time_ms();
		if (now - last_potion_time >= 500)
		{
			last_potion_time = now;
			if (key == KeyCode::Insert) hotkey_use_health_potion();
			else hotkey_use_mana_potion();
		}
		return true;
	}

	// Ignore auto-repeat key events — only act on initial key press
	if (!hb::shared::input::is_key_pressed(key)) return true;

	// Filter out keys that have no action
	switch (key) {
	case KeyCode::PageDown:
	case KeyCode::LWin:
	case KeyCode::RWin:
	case KeyCode::NumpadMultiply:
	case KeyCode::NumpadSeparator:
	case KeyCode::NumpadDecimal:
	case KeyCode::NumpadDivide:
	case KeyCode::NumLock:
	case KeyCode::ScrollLock:
		return true;
	default:
		break;
	}

	// Action keys — fire on initial key press for responsive input
	switch (key) {
	case KeyCode::NumpadAdd:
	case KeyCode::Equal:
		if (text_input_manager::get().is_active() == false)
			config_manager::get().set_zoom_map_enabled(true);
		return true;
	case KeyCode::NumpadSubtract:
	case KeyCode::Hyphen:
		if (text_input_manager::get().is_active() == false)
			config_manager::get().set_zoom_map_enabled(false);
		return true;
	case KeyCode::F1:
		if (get_dialog_box_manager().is_enabled(DialogBoxId::Help) == false)
			get_dialog_box_manager().enable_dialog_box(DialogBoxId::Help, 0, 0, 0);
		else
		{
			get_dialog_box_manager().disable_dialog_box(DialogBoxId::Help);
			get_dialog_box_manager().disable_dialog_box(DialogBoxId::Text);
		}
		return true;
	case KeyCode::F2:
		m_game->use_shortcut(2);
		return true;
	case KeyCode::F3:
		m_game->use_shortcut(3);
		return true;
	case KeyCode::F4:
		if (hb::shared::input::is_alt_down())
		{
			// Alt+F4: trigger logout countdown (same as window close)
			if ((GameModeManager::get_mode() == GameMode::MainGame) && (m_game->m_force_disconn == false))
			{
#ifdef _DEBUG
				if (m_logout_count == -1 || m_logout_count > 2)
				{
					m_logout_count = 1;
					m_logout_count_time = GameClock::get_time_ms();
				}
#else
				if (m_logout_count == -1 || m_logout_count > 11)
				{
					m_logout_count = 11;
					m_logout_count_time = GameClock::get_time_ms();
				}
#endif
			}
		}
		else
		{
			if (GameModeManager::get_mode() == GameMode::MainGame)
				magic_casting_system::get().begin_cast(m_game->m_magic_short_cut);
		}
		return true;
	case KeyCode::F5:
		if (get_dialog_box_manager().is_enabled(DialogBoxId::CharacterInfo) == false)
			get_dialog_box_manager().enable_dialog_box(DialogBoxId::CharacterInfo, 0, 0, 0);
		else get_dialog_box_manager().disable_dialog_box(DialogBoxId::CharacterInfo);
		return true;
	case KeyCode::F6:
		if (get_dialog_box_manager().is_enabled(DialogBoxId::Inventory) == false)
			get_dialog_box_manager().enable_dialog_box(DialogBoxId::Inventory, 0, 0, 0);
		else get_dialog_box_manager().disable_dialog_box(DialogBoxId::Inventory);
		return true;
	case KeyCode::F7:
		if (get_dialog_box_manager().is_enabled(DialogBoxId::Magic) == false)
			get_dialog_box_manager().enable_dialog_box(DialogBoxId::Magic, 0, 0, 0);
		else get_dialog_box_manager().disable_dialog_box(DialogBoxId::Magic);
		return true;
	case KeyCode::F8:
		if (get_dialog_box_manager().is_enabled(DialogBoxId::Skill) == false)
			get_dialog_box_manager().enable_dialog_box(DialogBoxId::Skill, 0, 0, 0);
		else get_dialog_box_manager().disable_dialog_box(DialogBoxId::Skill);
		return true;
	case KeyCode::F9:
		if (get_dialog_box_manager().is_enabled(DialogBoxId::ChatHistory) == false)
			get_dialog_box_manager().enable_dialog_box(DialogBoxId::ChatHistory, 0, 0, 0);
		else get_dialog_box_manager().disable_dialog_box(DialogBoxId::ChatHistory);
		return true;
	case KeyCode::F10:
		if (get_dialog_box_manager().is_enabled(DialogBoxId::SystemMenu) == false)
			get_dialog_box_manager().enable_dialog_box(DialogBoxId::SystemMenu, 0, 0, 0);
		else get_dialog_box_manager().disable_dialog_box(DialogBoxId::SystemMenu);
		return true;
	case KeyCode::F11:
		m_game->create_screen_shot();
		return true;
	case KeyCode::F12:
		if (text_input_manager::get().is_active()) return true;
		if (get_dialog_box_manager().is_enabled(DialogBoxId::SystemMenu) == false)
			get_dialog_box_manager().enable_dialog_box(DialogBoxId::SystemMenu, 0, 0, 0);
		else get_dialog_box_manager().disable_dialog_box(DialogBoxId::SystemMenu);
		return true;
	case KeyCode::End:
		hotkey_load_backup_chat();
		return true;
	case KeyCode::Up:
		hotkey_whisper_cycle_up();
		return true;
	case KeyCode::Right:
		m_game->m_arrow_pressed = 2;
		return true;
	case KeyCode::Down:
		hotkey_whisper_cycle_down();
		return true;
	case KeyCode::Left:
		m_game->m_arrow_pressed = 4;
		return true;
	case KeyCode::Tab:
		hotkey_tab_toggle_combat();
		return true;
	case KeyCode::Home:
		if (GameModeManager::get_mode() == GameMode::MainGame)
			m_game->send_game_packet(hb::net::make_common_command(CommonType::ToggleSafeAttackMode, m_game->m_player->m_player_x, m_game->m_player->m_player_y));
		return true;
	case KeyCode::Escape:
		hotkey_escape();
		return true;
	case KeyCode::PageUp:
		hotkey_special_ability();
		return true;
	default:
		break;
	}

	if (GameModeManager::get_mode() == GameMode::MainGame)
	{
		if (hb::shared::input::is_ctrl_down())
		{
			// Ctrl+0-9 for magic views (type=1 for explicit circle selection)
			switch (key) {
			case KeyCode::Num0: get_dialog_box_manager().enable_dialog_box(DialogBoxId::Magic, 1, 9, 0); break;
			case KeyCode::Num1: get_dialog_box_manager().enable_dialog_box(DialogBoxId::Magic, 1, 0, 0); break;
			case KeyCode::Num2: get_dialog_box_manager().enable_dialog_box(DialogBoxId::Magic, 1, 1, 0); break;
			case KeyCode::Num3: get_dialog_box_manager().enable_dialog_box(DialogBoxId::Magic, 1, 2, 0); break;
			case KeyCode::Num4: get_dialog_box_manager().enable_dialog_box(DialogBoxId::Magic, 1, 3, 0); break;
			case KeyCode::Num5: get_dialog_box_manager().enable_dialog_box(DialogBoxId::Magic, 1, 4, 0); break;
			case KeyCode::Num6: get_dialog_box_manager().enable_dialog_box(DialogBoxId::Magic, 1, 5, 0); break;
			case KeyCode::Num7: get_dialog_box_manager().enable_dialog_box(DialogBoxId::Magic, 1, 6, 0); break;
			case KeyCode::Num8: get_dialog_box_manager().enable_dialog_box(DialogBoxId::Magic, 1, 7, 0); break;
			case KeyCode::Num9: get_dialog_box_manager().enable_dialog_box(DialogBoxId::Magic, 1, 8, 0); break;
			default: break;
			}
		}
		// Only Enter key activates chat input - not every key press
		else if (key == KeyCode::Enter && (text_input_manager::get().is_active() == false) && (!hb::shared::input::is_alt_down()))
		{
			text_input_manager::get().start_input(CHAT_INPUT_X(), CHAT_INPUT_Y(), CGame::ChatMsgMaxLen, m_game->m_chat_msg);
			text_input_manager::get().set_chat_background(true);
			text_input_manager::get().clear_input();
		}
	}

	return true;
}

// ============== Complex hotkey handlers ==============

void Screen_OnGame::hotkey_use_health_potion()
{
	if (m_game->m_player->m_hp <= 0) return;
	if (m_item_using_status == true)
	{
		m_game->add_event_list(USE_RED_POTION1, 10);
		return;
	}
	if (get_dialog_box_manager().is_enabled(DialogBoxId::Exchange) == true)
	{
		m_game->add_event_list(USE_RED_POTION2, 10);
		return;
	}
	for (int i = 0; i < hb::shared::limits::MaxItems; i++)
	{
		if ((m_game->m_player->m_item_list[i] != 0) && (!inventory_manager::get().is_locked(i)))
		{
			CItem* cfg = m_game->get_item_config(m_game->m_player->m_item_list[i]->m_id_num);
			if (cfg && cfg->get_item_type() == hb::shared::item::item_type::consumable && cfg->get_item_effect_type() == hb::shared::item::ItemEffectType::HP)
			{
				{
					auto pkt = hb::net::make_common_command(CommonType::ReqUseItem, m_game->m_player->m_player_x, m_game->m_player->m_player_y);
					pkt.v1 = i;
					m_game->send_game_packet(pkt);
				}
				inventory_manager::get().lock_item(i);
				m_item_using_status = true;
				return;
			}
		}
	}
}

void Screen_OnGame::hotkey_use_mana_potion()
{
	if (m_game->m_player->m_hp <= 0) return;
	if (m_item_using_status == true)
	{
		m_game->add_event_list(USE_BLUE_POTION1, 10);
		return;
	}
	if (get_dialog_box_manager().is_enabled(DialogBoxId::Exchange) == true)
	{
		m_game->add_event_list(USE_BLUE_POTION2, 10);
		return;
	}
	for (int i = 0; i < hb::shared::limits::MaxItems; i++)
	{
		if ((m_game->m_player->m_item_list[i] != 0) && (!inventory_manager::get().is_locked(i)))
		{
			CItem* cfg = m_game->get_item_config(m_game->m_player->m_item_list[i]->m_id_num);
			if (cfg && cfg->get_item_type() == hb::shared::item::item_type::consumable && cfg->get_item_effect_type() == hb::shared::item::ItemEffectType::MP)
			{
				{
					auto pkt = hb::net::make_common_command(CommonType::ReqUseItem, m_game->m_player->m_player_x, m_game->m_player->m_player_y);
					pkt.v1 = i;
					m_game->send_game_packet(pkt);
				}
				inventory_manager::get().lock_item(i);
				m_item_using_status = true;
				return;
			}
		}
	}
}

void Screen_OnGame::hotkey_tab_toggle_combat()
{
	if (GameModeManager::get_mode() == GameMode::MainGame)
	{
		if (hb::shared::input::is_shift_down())
		{
			m_game->m_cur_focus--;
			if (m_game->m_cur_focus < 1) m_game->m_cur_focus = m_game->m_max_focus;
		}
		else
		{
			m_game->m_cur_focus++;
			if (m_game->m_cur_focus > m_game->m_max_focus) m_game->m_cur_focus = 1;
		}
		m_game->send_game_packet(hb::net::make_common_command(CommonType::ToggleCombatMode, m_game->m_player->m_player_x, m_game->m_player->m_player_y));
	}
}

void Screen_OnGame::hotkey_escape()
{
	if (GameModeManager::get_mode() == GameMode::MainGame)
	{
		// Cancel active chat input
		if (text_input_manager::get().is_active())
		{
			m_game->m_chat_msg.clear();
			text_input_manager::get().end_input();
			return;
		}

		if ((m_is_observer_mode == true) && (hb::shared::input::is_shift_down())) {
			if (m_logout_count == -1) { m_logout_count = 1; m_logout_count_time = GameClock::get_time_ms(); }
			get_dialog_box_manager().disable_dialog_box(DialogBoxId::SystemMenu);
			audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
		}
		else if (m_logout_count != -1) {
			if (m_game->m_force_disconn == false) {
				m_logout_count = -1;
				m_game->add_event_list(DLGBOX_CLICK_SYSMENU2, 10);
			}
		}
		if (m_is_get_pointing_mode == true) {
			m_is_get_pointing_mode = false;
			m_game->add_event_list(COMMAND_PROCESSOR1, 10);
		}
		m_is_f1_help_window_enabled = false;
	}
}

void Screen_OnGame::hotkey_special_ability()
{
	int i = 0;
	uint32_t time = GameClock::get_time_ms();
	if (GameModeManager::get_mode() != GameMode::MainGame) return;
	if (text_input_manager::get().is_active()) return;
	if (m_game->m_player->m_is_special_ability_enabled == true)
	{
		if (m_game->m_player->m_special_ability_type != 0) {
			m_game->send_game_packet(hb::net::make_common_command(CommonType::RequestActivateSpecAbility, m_game->m_player->m_player_x, m_game->m_player->m_player_y));
			m_game->m_player->m_is_special_ability_enabled = false;
		}
		else m_game->add_event_list(ON_KEY_UP26, 10);
	}
	else {
		if (m_game->m_player->m_special_ability_type == 0) m_game->add_event_list(ON_KEY_UP26, 10);
		else {
			std::string G_cTxt;
			if (m_game->m_player->m_playerAppearance.effect_type != 0) {
				m_game->add_event_list(ON_KEY_UP28, 10);
				return;
			}

			i = (time - m_special_ability_setting_time) / 1000;
			i = m_game->m_player->m_special_ability_time_left_sec - i;
			if (i < 0) i = 0;

			if (i < 60) {
				switch (m_game->m_player->m_special_ability_type) {
				case 1: G_cTxt = std::format(ON_KEY_UP29, i); break;
				case 2: G_cTxt = std::format(ON_KEY_UP30, i); break;
				case 3: G_cTxt = std::format(ON_KEY_UP31, i); break;
				case 4: G_cTxt = std::format(ON_KEY_UP32, i); break;
				case 5: G_cTxt = std::format(ON_KEY_UP33, i); break;
				case 50:G_cTxt = std::format(ON_KEY_UP34, i); break;
				case 51:G_cTxt = std::format(ON_KEY_UP35, i); break;
				case 52:G_cTxt = std::format(ON_KEY_UP36, i); break;
				}
			}
			else {
				switch (m_game->m_player->m_special_ability_type) {
				case 1: G_cTxt = std::format(ON_KEY_UP37, i / 60); break;
				case 2: G_cTxt = std::format(ON_KEY_UP38, i / 60); break;
				case 3: G_cTxt = std::format(ON_KEY_UP39, i / 60); break;
				case 4: G_cTxt = std::format(ON_KEY_UP40, i / 60); break;
				case 5: G_cTxt = std::format(ON_KEY_UP41, i / 60); break;
				case 50:G_cTxt = std::format(ON_KEY_UP42, i / 60); break;
				case 51:G_cTxt = std::format(ON_KEY_UP43, i / 60); break;
				case 52:G_cTxt = std::format(ON_KEY_UP44, i / 60); break;
				}
			}
			m_game->add_event_list(G_cTxt.c_str(), 10);
		}
	}
}

void Screen_OnGame::hotkey_load_backup_chat()
{
	if ((get_dialog_box_manager().is_enabled(DialogBoxId::ItemDropExternal) == true) && (get_dialog_box_manager().get_dialog_as<DialogBox_ItemDropAmount>(DialogBoxId::ItemDropExternal)->m_mode == DialogBox_ItemDropAmount::mode::input) && (get_dialog_box_manager().get_top_id() == DialogBoxId::ItemDropExternal))
	{
		return;
	}
	if ((!text_input_manager::get().is_active()) && (m_game->m_backup_chat_msg[0] != '!') && (m_game->m_backup_chat_msg[0] != '~') && (m_game->m_backup_chat_msg[0] != '^') &&
		(m_game->m_backup_chat_msg[0] != '@'))
	{
		m_game->m_chat_msg = m_game->m_backup_chat_msg;
		text_input_manager::get().start_input(CHAT_INPUT_X(), CHAT_INPUT_Y(), CGame::ChatMsgMaxLen, m_game->m_chat_msg);
		text_input_manager::get().set_chat_background(true);
	}
}

void Screen_OnGame::hotkey_whisper_cycle_up()
{
	m_game->m_arrow_pressed = 1;
	if (GameModeManager::get_mode() == GameMode::MainGame)
	{
		ChatManager::get().cycle_whisper_up();
		int idx = ChatManager::get().get_whisper_index();
		const char* name = ChatManager::get().get_whisper_target_name(idx);
		if (name != nullptr) {
			text_input_manager::get().end_input();
			m_game->m_chat_msg = std::format("/to {}", name);
			text_input_manager::get().start_input(CHAT_INPUT_X(), CHAT_INPUT_Y(), CGame::ChatMsgMaxLen, m_game->m_chat_msg);
			text_input_manager::get().set_chat_background(true);
		}
	}
}

void Screen_OnGame::hotkey_whisper_cycle_down()
{
	m_game->m_arrow_pressed = 3;
	if (GameModeManager::get_mode() == GameMode::MainGame)
	{
		ChatManager::get().cycle_whisper_down();
		int idx = ChatManager::get().get_whisper_index();
		const char* name = ChatManager::get().get_whisper_target_name(idx);
		if (name != nullptr) {
			text_input_manager::get().end_input();
			m_game->m_chat_msg = std::format("/to {}", name);
			text_input_manager::get().start_input(CHAT_INPUT_X(), CHAT_INPUT_Y(), CGame::ChatMsgMaxLen, m_game->m_chat_msg);
			text_input_manager::get().set_chat_background(true);
		}
	}
}

void Screen_OnGame::hotkey_toggle_force_attack()
{
	if (!hb::shared::input::is_ctrl_down() || GameModeManager::get_mode() != GameMode::MainGame || text_input_manager::get().is_active())
		return;
	if (m_game->m_player->m_force_attack)
	{
		m_game->m_player->m_force_attack = false;
		m_game->add_event_list(DEF_MSG_FORCEATTACK_OFF, 10);
	}
	else
	{
		m_game->m_player->m_force_attack = true;
		m_game->add_event_list(DEF_MSG_FORCEATTACK_ON, 10);
	}
}

void Screen_OnGame::hotkey_cycle_detail_level()
{
	if (!hb::shared::input::is_ctrl_down() || GameModeManager::get_mode() != GameMode::MainGame || text_input_manager::get().is_active())
		return;
	int detailLevel = config_manager::get().get_detail_level();
	detailLevel++;
	if (detailLevel > 2) detailLevel = 0;
	config_manager::get().set_detail_level(detailLevel);
	switch (detailLevel) {
	case 0:
		m_game->add_event_list(NOTIFY_MSG_DETAIL_LEVEL_LOW, 10);
		break;
	case 1:
		m_game->add_event_list(NOTIFY_MSG_DETAIL_LEVEL_MEDIUM, 10);
		break;
	case 2:
		m_game->add_event_list(NOTIFY_MSG_DETAIL_LEVEL_HIGH, 10);
		break;
	}
}

void Screen_OnGame::hotkey_toggle_sound_and_music()
{
	if (!hb::shared::input::is_ctrl_down() || GameModeManager::get_mode() != GameMode::MainGame || text_input_manager::get().is_active())
		return;
	if (audio_manager::get().is_music_enabled())
	{
		audio_manager::get().set_music_enabled(false);
		config_manager::get().set_music_enabled(false);
		audio_manager::get().stop_music();
		m_game->add_event_list(NOTIFY_MSG_MUSIC_OFF, 10);
		return;
	}
	if (audio_manager::get().is_sound_enabled())
	{
		audio_manager::get().stop_sound(sound_type::effect, 38);
		audio_manager::get().set_sound_enabled(false);
		config_manager::get().set_sound_enabled(false);
		m_game->add_event_list(NOTIFY_MSG_SOUND_OFF, 10);
		return;
	}
	if (audio_manager::get().is_sound_available())
	{
		audio_manager::get().set_music_enabled(true);
		config_manager::get().set_music_enabled(true);
		m_game->add_event_list(NOTIFY_MSG_MUSIC_ON, 10);
	}
	if (audio_manager::get().is_sound_available())
	{
		audio_manager::get().set_sound_enabled(true);
		config_manager::get().set_sound_enabled(true);
		m_game->add_event_list(NOTIFY_MSG_SOUND_ON, 10);
	}
	m_game->start_bgm();
}

void Screen_OnGame::hotkey_whisper_target()
{
	if (!hb::shared::input::is_ctrl_down() || GameModeManager::get_mode() != GameMode::MainGame || text_input_manager::get().is_active())
		return;
	std::string tempid;

	char lb, rb;
	short sX, sY, mouse_x, mouse_y, z;
	sX = get_dialog_box_manager().get_dialog_box(DialogBoxId::ChatHistory)->m_x;
	sY = get_dialog_box_manager().get_dialog_box(DialogBoxId::ChatHistory)->m_y;
	mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	z = static_cast<short>(hb::shared::input::get_mouse_wheel_delta());
	lb = hb::shared::input::is_mouse_button_down(MouseButton::Left) ? 1 : 0;
	rb = hb::shared::input::is_mouse_button_down(MouseButton::Right) ? 1 : 0;
	if (get_dialog_box_manager().is_enabled(DialogBoxId::ChatHistory) == true && (mouse_x >= sX + 20) && (mouse_x <= sX + 360) && (mouse_y >= sY + 35) && (mouse_y <= sY + 139))
	{
		char buff[64];
		int i = (139 - mouse_y + sY) / 13;
		int msg_idx = i + get_dialog_box_manager().get_dialog_as<DialogBox_ChatHistory>(DialogBoxId::ChatHistory)->m_scroll_position;
		CMsg* chat_msg = ChatManager::get().get_message(msg_idx);
		if (chat_msg == nullptr) return;
		if (chat_msg->m_pMsg[0] == ' ') { i++; chat_msg = ChatManager::get().get_message(i + get_dialog_box_manager().get_dialog_as<DialogBox_ChatHistory>(DialogBoxId::ChatHistory)->m_scroll_position); }
		if (chat_msg == nullptr) return;
		std::snprintf(buff, sizeof(buff), "%s", chat_msg->m_pMsg);
		char* sep = std::strchr(buff, ':');
		if (sep) *sep = '\0';
		tempid = std::format("/to {}", buff);
		m_game->send_chat_message(tempid.c_str());
	}
	else if (m_game->m_entity_state.is_player() && (m_game->m_entity_state.m_name[0] != '\0') && (m_ilusion_owner_h == 0)
		&& ((m_is_crusade_mode == false) || !IsHostile(m_game->m_entity_state.m_status.relationship)))
	{
		tempid = std::format("/to {}", m_game->m_entity_state.m_name.data());
		m_game->send_chat_message(tempid.c_str());
	}
	else
	{
		text_input_manager::get().end_input();
		m_game->m_chat_msg = "/to ";
		text_input_manager::get().start_input(CHAT_INPUT_X(), CHAT_INPUT_Y(), CGame::ChatMsgMaxLen, m_game->m_chat_msg);
		text_input_manager::get().set_chat_background(true);
	}
}
