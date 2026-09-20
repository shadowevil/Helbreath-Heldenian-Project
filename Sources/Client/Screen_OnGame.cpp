// Screen_OnGame.cpp: Main gameplay screen implementation
//
//////////////////////////////////////////////////////////////////////

#include "Screen_OnGame.h"
#include "Player.h"
#include "Game.h"
#include "EventListManager.h"
#include "TextInputManager.h"
#include "InventoryManager.h"
#include "ItemSpriteMetadata.h"
#include "GameModeManager.h"
#include "CommonTypes.h"
#include "performance_monitor.h"
#include "AudioManager.h"
#include "WeatherManager.h"
#include "ChatManager.h"
#include "ItemNameFormatter.h"
#include "ItemTooltip.h"
#include "ConfigManager.h"
#include "BalanceConstants.h"
#include "IInput.h"
#include "GlobalDef.h"
#include "lan_eng.h"
#include "GameFonts.h"
#include "TextLibExt.h"
#include "SpellAoE.h"
#include "Magic.h"
#include "DialogBox_ItemDropAmount.h"
#include "DialogBox_Party.h"
#include "DialogBox_SellList.h"
#include "DialogBox_Skill.h"
#include "DialogBox_Exchange.h"
#include "DialogBox_NpcActionQuery.h"
#include "Log.h"
#ifdef TESTER_ONLY
#include "DialogBox_ItemCreator.h"
#endif // TESTER_ONLY
#include <string>
#include <memory>
#include <format>
#include <algorithm>
#include <charconv>
#include <climits>
#include "Packet/SharedPackets.h"
#include "PacketSendHelpers.h"
#include "CombatSystem.h"
#include "TeleportManager.h"
#include "GameTimer.h"



using namespace hb::shared::net;
namespace MouseButton = hb::shared::input::MouseButton;

using namespace hb::shared::action;

using namespace hb::shared::item;
using namespace hb::client::config;
using namespace hb::client::sprite_id;

std::unique_ptr<CPlayer> Screen_OnGame::s_player;

Screen_OnGame::Screen_OnGame(CGame* game)
    : IGameScreen(game)
    , m_player_renderer(*game)
    , m_npc_renderer(*game)
{
    m_player_renderer.set_screen(this);
    m_npc_renderer.set_screen(this);
}

void Screen_OnGame::create_player()
{
    s_player = std::make_unique<CPlayer>();
}

void Screen_OnGame::destroy_player()
{
    s_player.reset();
}

void Screen_OnGame::on_initialize()
{
    // Create the player
    Screen_OnGame::create_player();
    m_game->m_player = s_player.get();
    combat_system::get().set_player(*s_player);

    // Reset CGame-level state
    m_game->m_check_connection_time = 0;
    m_game->m_Camera.set_shake(0);

    for (int r = 0; r < 4; r++) m_game->m_config_retry[r] = CGame::ConfigRetryLevel::None;
    m_game->m_config_request_time = 0;
    m_game->m_init_data_ready = false;
    m_game->m_configs_ready = false;

    CursorTarget::reset_selection_click_time();

    m_game->m_net_lag_count = 0;
    m_game->m_latency_ms = -1;
    m_game->m_last_net_msg_id = 0;
    m_game->m_last_net_msg_time = 0;
    m_game->m_last_net_msg_size = 0;
    m_game->m_last_net_recv_time = 0;
    m_game->m_last_npc_event_time = 0;

    if (m_game->m_effect_manager) m_game->m_effect_manager->clear_all_effects();

    m_floating_text.clear_all();

    ChatManager::get().clear_messages();
    ChatManager::get().clear_whispers();

    m_game->m_force_disconn = false;

    CursorTarget::clear_selection();

    teleport_manager::get().reset();

    // Reset gameplay state (now on Screen_OnGame)
    m_is_get_pointing_mode = false;
    m_wait_for_new_click = false;
    m_magic_cast_time = 0;
    m_point_command_type = -1;

    m_skill_using_status = false;
    m_item_using_status = false;
    m_using_slate = false;

    m_down_skill_index = -1;

    m_ilusion_owner_h = 0;
    m_ilusion_owner_type = 0;

    m_draw_flag = 0;
    m_is_crusade_mode = false;

    m_env_effect_time = GameClock::get_time_ms();

    for (int i = 0; i < game_limits::max_text_dlg_lines; i++)
    {
        if (m_msg_text_list[i] != 0)
            m_msg_text_list[i].reset();

        if (m_msg_text_list2[i] != 0)
            m_msg_text_list2[i].reset();

        if (m_agree_msg_text_list[i] != 0)
            m_agree_msg_text_list[i].reset();
    }

    m_logout_count = -1;
    m_logout_count_time = 0;
    m_quest.who = 0;
    m_quest.quest_type = 0;
    m_quest.contribution = 0;
    m_quest.target_type = 0;
    m_quest.target_count = 0;
    m_quest.current_count = 0;
    m_quest.x = 0;
    m_quest.y = 0;
    m_quest.range = 0;
    m_quest.is_quest_completed = false;
    m_is_observer_mode = false;
    m_is_observer_commanded = false;
    m_special_ability_setting_time = 0;
    m_is_f1_help_window_enabled = false;
    for (int i = 0; i < hb::shared::limits::MaxCrusadeStructures; i++)
    {
        m_crusade_structure_info[i].type = 0;
        m_crusade_structure_info[i].side = 0;
        m_crusade_structure_info[i].x = 0;
        m_crusade_structure_info[i].y = 0;
    }
    m_commander_command_requested_time = 0;
    m_top_msg_last_sec = 0;
    m_top_msg_time = 0;

    m_gate_posit_x = m_gate_posit_y = -1;
    m_heldenian_aresden_left_tower = -1;
    m_heldenian_elvine_left_tower = -1;
    m_heldenian_aresden_flags = -1;
    m_heldenian_elvine_flags = -1;
    m_is_xmas = false;
    m_total_party_member = 0;
    m_party_status = 0;
    m_gizon_item_upgrade_left = 0;


    // Copy session identity into the player
    s_player->m_player_name = m_game->m_selected_char_name;
    s_player->m_level = m_game->m_selected_char_level;

    // Create dialog box manager — owned by this screen
    m_dialog_box_manager = std::make_unique<DialogBoxManager>(*m_game, *s_player);
    m_dialog_box_manager->initialize_dialog_boxes();

    // Reset dialog state that init_game_settings couldn't do (dialogs didn't exist yet)
    m_dialog_box_manager->get_dialog_as<DialogBox_Skill>(DialogBoxId::Skill)->m_is_down_skill_pending = false;
    m_dialog_box_manager->reset_all_for_map_change();
    m_dialog_box_manager->get_dialog_as<DialogBox_SellList>(DialogBoxId::SellList)->reset();
    m_dialog_box_manager->get_dialog_as<DialogBox_Party>(DialogBoxId::Party)->reset_members();
    // GuideMap is enabled in init_data_response_handler() after map data is loaded.
    // Enabling it here would render before m_map_size_x/y are set, causing div-by-zero.

    m_network_message_manager = std::make_unique<NetworkMessageManager>(m_game);

    register_hotkeys();

    m_guild_manager.set_game(m_game);
    m_fishing_manager.set_game(m_game);
    m_crafting_manager.set_game(m_game);
    m_quest_manager.set_game(m_game);

    m_time = GameClock::get_time_ms();
    m_game->m_fps_time = m_time;
    m_game->m_check_conn_time = m_time;
    m_game->m_check_spr_time = m_time;
    m_game->m_check_chat_time = m_time;
    m_dwPrevChatTime = 0;

    if (audio_manager::get().is_music_enabled())
        m_game->start_bgm();

    // Connect to the game server. The player and dialog boxes are ready to
    // handle any responses the server sends back.
    m_game->connect_to_game_server();
}

void Screen_OnGame::on_uninitialize()
{
    text_input_manager::get().end_input();
    audio_manager::get().stop_music();
    m_network_message_manager.reset();
    m_dialog_box_manager.reset();
    m_game->m_player = nullptr;
}

bool Screen_OnGame::on_text_input(uint32_t codepoint)
{
    // Auto-activate chat on printable keypress when toggle-to-chat is disabled
    if (!text_input_manager::get().is_active()
        && !config_manager::get().is_toggle_to_chat_enabled()
        && GameModeManager::get_active_overlay() == nullptr
        && codepoint >= 32 && codepoint != 127)
    {
        text_input_manager::get().start_input(
            CHAT_INPUT_X(), CHAT_INPUT_Y(), CGame::ChatMsgMaxLen, m_game->m_chat_msg);
        text_input_manager::get().set_chat_background(true);
        text_input_manager::get().clear_input();
        return true;
    }
    return false;
}

void Screen_OnGame::on_update()
{
    std::string G_cTxt;
    short val, absX, absY, tX, tY;
    int amount;

    m_time = GameClock::get_time_ms();

    m_sMsX = static_cast<short>(hb::shared::input::get_mouse_x());
    m_sMsY = static_cast<short>(hb::shared::input::get_mouse_y());
    m_sMsZ = static_cast<short>(hb::shared::input::get_mouse_wheel_delta());
    m_cLB = (hb::shared::input::is_mouse_button_pressed(MouseButton::Left) || hb::shared::input::is_mouse_button_down(MouseButton::Left)) ? 1 : 0;
    m_cRB = (hb::shared::input::is_mouse_button_pressed(MouseButton::Right) || hb::shared::input::is_mouse_button_down(MouseButton::Right)) ? 1 : 0;

    m_game->m_cur_time = GameClock::get_time_ms();

    // Sync manager singletons with game state
    audio_manager::get().set_listener_position(m_game->m_player->m_player_x, m_game->m_player->m_player_y);

    // Pump CControls input for text_input_manager (before Enter key handling)
    text_input_manager::get().update(m_time);

    // Enter key handling
    if (hb::shared::input::is_key_pressed(KeyCode::Enter) == true)
    {
        if ((m_game->get_dialog_box_manager().is_enabled(DialogBoxId::ItemDropExternal) == true) && (m_game->get_dialog_box_manager().get_dialog_as<DialogBox_ItemDropAmount>(DialogBoxId::ItemDropExternal)->m_mode == DialogBox_ItemDropAmount::mode::input) && (m_game->get_dialog_box_manager().get_top_id() == DialogBoxId::ItemDropExternal)) {
            text_input_manager::get().end_input();

            if (m_skill_using_status == true) {
                m_game->add_event_list(UPDATE_SCREEN_ONGAME1, 10);
                return;
            }

            if ((m_game->get_dialog_box_manager().is_enabled(DialogBoxId::NpcActionQuery) == true) && ((m_game->get_dialog_box_manager().get_dialog_as<DialogBox_NpcActionQuery>(DialogBoxId::NpcActionQuery)->m_mode == DialogBox_NpcActionQuery::mode::give_to_player) || (m_game->get_dialog_box_manager().get_dialog_as<DialogBox_NpcActionQuery>(DialogBoxId::NpcActionQuery)->m_mode == DialogBox_NpcActionQuery::mode::sell_to_shop))) {
                m_game->add_event_list(UPDATE_SCREEN_ONGAME1, 10);
                return;
            }

            if ((m_game->get_dialog_box_manager().is_enabled(DialogBoxId::ItemDropConfirm) == true) || (m_game->get_dialog_box_manager().is_enabled(DialogBoxId::SellOrRepair) == true) || (m_game->get_dialog_box_manager().is_enabled(DialogBoxId::Manufacture) == true)) {
                m_game->add_event_list(UPDATE_SCREEN_ONGAME1, 10);
                return;
            }

            if (m_game->m_amount_string.empty()) return;
            {
                uint64_t parsed_amount = 0;
                auto [ptr, ec] = std::from_chars(m_game->m_amount_string.data(), m_game->m_amount_string.data() + m_game->m_amount_string.size(), parsed_amount);
                if (ec != std::errc{}) return;
                uint64_t item_count = m_game->m_player->m_item_list[m_game->get_dialog_box_manager().get_dialog_as<DialogBox_ItemDropAmount>(DialogBoxId::ItemDropExternal)->m_item_index]->m_instance.count;
                if (parsed_amount > item_count) parsed_amount = item_count;
                amount = static_cast<int>(std::min<uint64_t>(parsed_amount, INT_MAX));
            }

            if (amount != 0) {
                if (m_game->m_player->m_item_list[m_game->get_dialog_box_manager().get_dialog_as<DialogBox_ItemDropAmount>(DialogBoxId::ItemDropExternal)->m_item_index]->m_instance.count >= static_cast<uint64_t>(amount)) {
                    if (m_game->get_dialog_box_manager().get_dialog_as<DialogBox_ItemDropAmount>(DialogBoxId::ItemDropExternal)->m_drop_x != 0) {
                        absX = abs(m_game->get_dialog_box_manager().get_dialog_as<DialogBox_ItemDropAmount>(DialogBoxId::ItemDropExternal)->m_drop_x - m_game->m_player->m_player_x);
                        absY = abs(m_game->get_dialog_box_manager().get_dialog_as<DialogBox_ItemDropAmount>(DialogBoxId::ItemDropExternal)->m_drop_y - m_game->m_player->m_player_y);

                        if ((absX == 0) && (absY == 0))
                            m_game->add_event_list(UPDATE_SCREEN_ONGAME5, 10);
                        else if ((absX <= 8) && (absY <= 8)) {
                            switch (m_game->get_dialog_box_manager().get_dialog_as<DialogBox_ItemDropAmount>(DialogBoxId::ItemDropExternal)->m_drop_target_type) {
                            case 1: case 2: case 3: case 4: case 5: case 6:
                            {
                                auto* dropDlg = m_game->get_dialog_box_manager().get_dialog_as<DialogBox_ItemDropAmount>(DialogBoxId::ItemDropExternal);
                                auto* npcDlg = m_game->get_dialog_box_manager().get_dialog_as<DialogBox_NpcActionQuery>(DialogBoxId::NpcActionQuery);
                                npcDlg->enable_with_target(1, dropDlg->m_item_index, dropDlg->m_drop_target_type, amount, m_game->m_comm_object_id, dropDlg->m_drop_x, dropDlg->m_drop_y, dropDlg->m_target_name);
                                tX = m_sMsX - 117; tY = m_sMsY - 50;
                                if (tX < 0) tX = 0;
                                if ((tX + 235) > LOGICAL_MAX_X()) tX = LOGICAL_MAX_X() - 235;
                                if (tY < 0) tY = 0;
                                if ((tY + 100) > LOGICAL_MAX_Y()) tY = LOGICAL_MAX_Y() - 100;
                                npcDlg->m_x = tX;
                                npcDlg->m_y = tY;
                            }   break;
                            case 20:
                            {
                                auto* dropDlg = m_game->get_dialog_box_manager().get_dialog_as<DialogBox_ItemDropAmount>(DialogBoxId::ItemDropExternal);
                                auto* npcDlg = m_game->get_dialog_box_manager().get_dialog_as<DialogBox_NpcActionQuery>(DialogBoxId::NpcActionQuery);
                                npcDlg->enable_with_target(3, dropDlg->m_item_index, dropDlg->m_drop_target_type, amount, m_game->m_comm_object_id, dropDlg->m_drop_x, dropDlg->m_drop_y, dropDlg->m_target_name);
                                tX = m_sMsX - 117; tY = m_sMsY - 50;
                                if (tX < 0) tX = 0;
                                if ((tX + 235) > LOGICAL_MAX_X()) tX = LOGICAL_MAX_X() - 235;
                                if (tY < 0) tY = 0;
                                if ((tY + 100) > LOGICAL_MAX_Y()) tY = LOGICAL_MAX_Y() - 100;
                                npcDlg->m_x = tX;
                                npcDlg->m_y = tY;
                            }   break;
                            case 15: case 24:
                            {
                                auto* dropDlg = m_game->get_dialog_box_manager().get_dialog_as<DialogBox_ItemDropAmount>(DialogBoxId::ItemDropExternal);
                                auto* npcDlg = m_game->get_dialog_box_manager().get_dialog_as<DialogBox_NpcActionQuery>(DialogBoxId::NpcActionQuery);
                                npcDlg->enable_with_target(2, dropDlg->m_item_index, dropDlg->m_drop_target_type, amount, m_game->m_comm_object_id, 0, 0, dropDlg->m_target_name);
                                tX = m_sMsX - 117; tY = m_sMsY - 50;
                                if (tX < 0) tX = 0;
                                if ((tX + 235) > LOGICAL_MAX_X()) tX = LOGICAL_MAX_X() - 235;
                                if (tY < 0) tY = 0;
                                if ((tY + 100) > LOGICAL_MAX_Y()) tY = LOGICAL_MAX_Y() - 100;
                                npcDlg->m_x = tX;
                                npcDlg->m_y = tY;
                            }   break;
                            case 1000:
                            {
                                int ex_item = m_game->get_dialog_box_manager().get_dialog_as<DialogBox_ItemDropAmount>(DialogBoxId::ItemDropExternal)->m_drop_target_id;
                                if (m_game->get_dialog_box_manager().get_dialog_as<DialogBox_Exchange>(DialogBoxId::Exchange)->m_slots[0].v1 == -1) m_game->get_dialog_box_manager().get_dialog_as<DialogBox_Exchange>(DialogBoxId::Exchange)->m_slots[0].inv_slot = ex_item;
                                else if (m_game->get_dialog_box_manager().get_dialog_as<DialogBox_Exchange>(DialogBoxId::Exchange)->m_slots[1].v1 == -1) m_game->get_dialog_box_manager().get_dialog_as<DialogBox_Exchange>(DialogBoxId::Exchange)->m_slots[1].inv_slot = ex_item;
                                else if (m_game->get_dialog_box_manager().get_dialog_as<DialogBox_Exchange>(DialogBoxId::Exchange)->m_slots[2].v1 == -1) m_game->get_dialog_box_manager().get_dialog_as<DialogBox_Exchange>(DialogBoxId::Exchange)->m_slots[2].inv_slot = ex_item;
                                else if (m_game->get_dialog_box_manager().get_dialog_as<DialogBox_Exchange>(DialogBoxId::Exchange)->m_slots[3].v1 == -1) m_game->get_dialog_box_manager().get_dialog_as<DialogBox_Exchange>(DialogBoxId::Exchange)->m_slots[3].inv_slot = ex_item;
                                else return;
                                inventory_manager::get().lock_item(ex_item);
                                {
                                	auto pkt = hb::net::make_common_command(CommonType::set_exchange_item, m_game->m_player->m_player_x, m_game->m_player->m_player_y);
                                	pkt.v1 = ex_item;
                                	pkt.v2 = amount;
                                	m_game->send_game_packet(pkt);
                                }
                                break;
                            }
                            case 1001:
                            {
                                auto* sellDlg = m_game->get_dialog_box_manager().get_dialog_as<DialogBox_SellList>(DialogBoxId::SellList);
                                int drop_id = m_game->get_dialog_box_manager().get_dialog_as<DialogBox_ItemDropAmount>(DialogBoxId::ItemDropExternal)->m_drop_target_id;
                                if (sellDlg->add_item(drop_id, amount))
                                    inventory_manager::get().lock_item(drop_id);
                                else
                                    m_game->add_event_list(UPDATE_SCREEN_ONGAME6, 10);
                            }
                                break;
                            case 1002:
                                if (inventory_manager::get().get_bank_item_count() >= (m_game->m_max_bank_items - 1)) m_game->add_event_list(DLGBOX_CLICK_NPCACTION_QUERY9, 10);
                                else {
                                    CItem* cfg = m_game->get_item_config(m_game->m_player->m_item_list[m_game->get_dialog_box_manager().m_give_item.item_index]->m_id_num);
                                    if (cfg)
                                    {
                                    	auto pkt = hb::net::make_common_command_str(CommonType::GiveItemToChar, m_game->m_player->m_player_x, m_game->m_player->m_player_y, m_game->get_dialog_box_manager().m_give_item.item_index);
                                    	pkt.v1 = amount;
                                    	pkt.v2 = m_game->get_dialog_box_manager().m_give_item.target_x;
                                    	pkt.v3 = m_game->get_dialog_box_manager().m_give_item.target_y;
                                    	std::snprintf(pkt.text, sizeof(pkt.text), "%s", cfg->m_name);
                                    	pkt.v4 = m_game->get_dialog_box_manager().m_give_item.object_id;
                                    	m_game->send_game_packet(pkt);
                                    }
                                }
                                break;
                            default:
                            {
                                CItem* cfg = m_game->get_item_config(m_game->m_player->m_item_list[m_game->get_dialog_box_manager().get_dialog_as<DialogBox_ItemDropAmount>(DialogBoxId::ItemDropExternal)->m_item_index]->m_id_num);
                                if (cfg)
                                {
                                	auto pkt = hb::net::make_common_command_str(CommonType::GiveItemToChar, m_game->m_player->m_player_x, m_game->m_player->m_player_y, static_cast<char>(m_game->get_dialog_box_manager().get_dialog_as<DialogBox_ItemDropAmount>(DialogBoxId::ItemDropExternal)->m_item_index));
                                	pkt.v1 = amount;
                                	pkt.v2 = m_game->get_dialog_box_manager().get_dialog_as<DialogBox_ItemDropAmount>(DialogBoxId::ItemDropExternal)->m_drop_x;
                                	pkt.v3 = m_game->get_dialog_box_manager().get_dialog_as<DialogBox_ItemDropAmount>(DialogBoxId::ItemDropExternal)->m_drop_y;
                                	std::snprintf(pkt.text, sizeof(pkt.text), "%s", cfg->m_name);
                                	m_game->send_game_packet(pkt);
                                }
                            }
                                break;
                            }
                            inventory_manager::get().lock_item(m_game->get_dialog_box_manager().get_dialog_as<DialogBox_ItemDropAmount>(DialogBoxId::ItemDropExternal)->m_item_index);
                        }
                        else m_game->add_event_list(UPDATE_SCREEN_ONGAME7, 10);
                    }
                    else {
                        if (amount <= 0) m_game->add_event_list(UPDATE_SCREEN_ONGAME8, 10);
                        else {
                            CItem* cfg = m_game->get_item_config(m_game->m_player->m_item_list[m_game->get_dialog_box_manager().get_dialog_as<DialogBox_ItemDropAmount>(DialogBoxId::ItemDropExternal)->m_item_index]->m_id_num);
                            if (cfg)
                            {
                            	auto pkt = hb::net::make_common_command_str(CommonType::ItemDrop, m_game->m_player->m_player_x, m_game->m_player->m_player_y);
                            	pkt.v1 = m_game->get_dialog_box_manager().get_dialog_as<DialogBox_ItemDropAmount>(DialogBoxId::ItemDropExternal)->m_item_index;
                            	pkt.v2 = amount;
                            	std::snprintf(pkt.text, sizeof(pkt.text), "%s", cfg->m_name);
                            	m_game->send_game_packet(pkt);
                            }
                            inventory_manager::get().lock_item(m_game->get_dialog_box_manager().get_dialog_as<DialogBox_ItemDropAmount>(DialogBoxId::ItemDropExternal)->m_item_index);
                        }
                    }
                }
                else m_game->add_event_list(UPDATE_SCREEN_ONGAME9, 10);
            }
            m_game->get_dialog_box_manager().disable_dialog_box(DialogBoxId::ItemDropExternal);
        }
#ifdef TESTER_ONLY
        else if ((m_game->get_dialog_box_manager().is_enabled(DialogBoxId::ItemCreator) == true) &&
                 (m_game->get_dialog_box_manager().get_top_id() == DialogBoxId::ItemCreator))
        {
            auto* dlg = dynamic_cast<DialogBox_ItemCreator*>(
                m_game->get_dialog_box_manager().get_dialog_box(DialogBoxId::ItemCreator));
            if (dlg) dlg->on_enter_pressed();
        }
#endif // TESTER_ONLY
        else
        {
            if (!text_input_manager::get().is_active()) {
                switch (m_game->m_backup_chat_msg[0]) {
                case '!': case '@': case '#': case '$': case '^':
                    m_game->m_chat_msg.clear();
                    m_game->m_chat_msg += m_game->m_backup_chat_msg[0];
                    text_input_manager::get().start_input(CHAT_INPUT_X(), CHAT_INPUT_Y(), CGame::ChatMsgMaxLen, m_game->m_chat_msg);
                    text_input_manager::get().set_chat_background(true);
                    break;
                default:
                    text_input_manager::get().start_input(CHAT_INPUT_X(), CHAT_INPUT_Y(), CGame::ChatMsgMaxLen, m_game->m_chat_msg);
                    text_input_manager::get().set_chat_background(true);
                    text_input_manager::get().clear_input();
                    break;
                }
            }
            else {
                G_cTxt = text_input_manager::get().get_input_string();
                text_input_manager::get().end_input();
                m_game->m_backup_chat_msg = G_cTxt.c_str();
                if ((m_game->m_cur_time - m_dwPrevChatTime) >= 700) {
                    m_dwPrevChatTime = m_game->m_cur_time;
                    if (!G_cTxt.empty()) {
                        if (!ChatManager::get().is_shout_enabled() && G_cTxt[0] == '!') {
                            m_game->add_event_list(BCHECK_LOCAL_CHAT_COMMAND9, 10);
                        }
                        else {
                            m_game->send_chat_message(G_cTxt.c_str());
                        }
                    }
                }
            }
        }
    }

    // Calculate viewport tile coordinates
    m_pivot_x = m_game->m_map_data->m_pivot_x;
    m_pivot_y = m_game->m_map_data->m_pivot_y;
    val = m_game->m_Camera.get_x() - (m_pivot_x * 32);
    m_sDivX = val / 32;
    m_sModX = val % 32;
    val = m_game->m_Camera.get_y() - (m_pivot_y * 32);
    m_sDivY = val / 32;
    m_sModY = val % 32;

    // Logout countdown
    if (m_logout_count > 0) {
        if ((m_time - m_logout_count_time) > 1000) {
            m_logout_count--;
            m_logout_count_time = m_time;
            G_cTxt = std::format(UPDATE_SCREEN_ONGAME13, m_logout_count);
            m_game->add_event_list(G_cTxt.c_str(), 10);
        }
    }
    if (m_logout_count == 0) {
        m_logout_count = -1;
        m_game->write_settings();
        m_game->m_g_sock.reset();
        audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
        audio_manager::get().stop_sound(sound_type::effect, 38);
        audio_manager::get().stop_music();
        m_game->change_game_mode(GameMode::MainMenu);
        return;
    }

    // Restart countdown
    if (m_game->m_restart_count > 0) {
        if ((m_time - m_game->m_restart_count_time) > 1000) {
            m_game->m_restart_count--;
            m_game->m_restart_count_time = m_time;
            G_cTxt = std::format(UPDATE_SCREEN_ONGAME14, m_game->m_restart_count);
            m_game->add_event_list(G_cTxt.c_str(), 10);
        }
    }
    if (m_game->m_restart_count == 0) {
        m_game->m_restart_count = -1;
        {
			hb::net::PacketRequestHeaderOnly req{};
			req.header.msg_id = MsgId::RequestRestart;
			req.header.msg_type = 0;
			m_game->send_game_packet(req);
		}
        return;
    }

    // update frame counters and process commands
    int update_ret = m_game->m_map_data->object_frame_counter(m_game->m_player->m_player_name, m_game->m_Camera.get_x(), m_game->m_Camera.get_y());
    if (m_game->m_effect_manager) m_game->m_effect_manager->update();
    // Command unlock logic — determines when the player can act again after an action.
    // Three unlock paths, checked in priority order:
    //
    // 1. Movement pipeline (Quick Actions): unlocks at 95% move progress for smooth chaining.
    //    Only applies to MOVE/RUN — attack commands always reset cmd to STOP after execution.
    //
    // 2. Rotation unlock: allows quick direction changes during STOP animation (100ms min).
    //    Also serves as the primary unlock after attack/damage animations finish and
    //    transition the tile to STOP. Respects the attack swing time floor.
    //
    // 3. Animation completion (update_ret == 2): fires once when any animation finishes.
    //    Respects the attack swing time floor.
    //
    // 4. Attack cooldown expiry: continuous per-frame check that catches the case where
    //    path 3 fired before the attack time elapsed (e.g., short damage animation replaced
    //    a longer attack). Only active when the current lock is from the attack itself
    //    (cmdTime <= attackEnd), never for subsequent movement locks.
    if (!m_game->m_player->m_Controller.is_command_available()) {
        char cmd = m_game->m_player->m_Controller.get_command();

        // Path 1: Movement pipeline — unlock at 95% motion progress for smooth chaining.
        // cmd stays as MOVE/RUN during movement (not reset to STOP until destination reached).
        // No attack end time check needed: movement can only start after attack cooldown expires.
        if (config_manager::get().is_quick_actions_enabled() &&
            (cmd == Type::Move || cmd == Type::Run)) {
            int dX = m_game->m_player->m_player_x - m_game->m_map_data->m_pivot_x;
            int dY = m_game->m_player->m_player_y - m_game->m_map_data->m_pivot_y;
            if (dX >= 0 && dX < MapDataSizeX && dY >= 0 && dY < MapDataSizeY) {
                auto& motion = m_game->m_map_data->m_data[dX][dY].m_motion;
                if (motion.m_is_moving && motion.m_progress >= 0.95f) {
                    m_game->m_player->m_Controller.set_command_available(true);
                    m_game->m_player->m_Controller.set_command_time(0);
                }
            }
        }
        // Path 2: Rotation unlock + post-attack/damage unlock.
        // After attack/magic/damage execution, cmd is always STOP. The tile animation
        // transitions to STOP when the action animation finishes. Once both cmd and tile
        // are STOP, unlock — but respect the attack swing time floor so interrupted attacks
        // (damage replacing attack) don't unlock before the server's expected swing time.
        else if (cmd == Type::stop) {
            int dX = m_game->m_player->m_player_x - m_game->m_map_data->m_pivot_x;
            int dY = m_game->m_player->m_player_y - m_game->m_map_data->m_pivot_y;
            if (dX >= 0 && dX < MapDataSizeX && dY >= 0 && dY < MapDataSizeY) {
                int8_t animAction = m_game->m_map_data->m_data[dX][dY].m_animation.m_action;
                if (animAction == Type::stop) {
                    uint32_t cmdTime = m_game->m_player->m_Controller.get_command_time();
                    uint32_t attack_end = m_game->m_player->m_Controller.get_attack_end_time();
                    if (cmdTime == 0 || ((m_time - cmdTime) >= 100 &&
                        (attack_end == 0 || m_time >= attack_end))) {
                        m_game->m_player->m_Controller.set_command_available(true);
                        m_game->m_player->m_Controller.set_command_time(0);
                    }
                }
            }
        }

        // Path 4: Attack cooldown expiry — catches the case where the damage animation
        // finished (update_ret == 2 was blocked) but the tile hasn't transitioned to STOP
        // yet (so path 2 can't fire). Only active when this lock is from the attack that
        // set attackEnd (cmdTime <= attackEnd), not for subsequent movement locks.
        // Also skip if a blocking animation is still playing — the player must wait
        // for damage stun or dash slide to finish even if attackEndTime has expired.
        uint32_t attack_end = m_game->m_player->m_Controller.get_attack_end_time();
        uint32_t cmd_time = m_game->m_player->m_Controller.get_command_time();
        if (attack_end != 0 && cmd_time <= attack_end && GameClock::get_time_ms() >= attack_end) {
            int x4 = m_game->m_player->m_player_x - m_game->m_map_data->m_pivot_x;
            int y4 = m_game->m_player->m_player_y - m_game->m_map_data->m_pivot_y;
            bool blocking_anim_playing = false;
            if (x4 >= 0 && x4 < MapDataSizeX && y4 >= 0 && y4 < MapDataSizeY) {
                int8_t animAction = m_game->m_map_data->m_data[x4][y4].m_animation.m_action;
                blocking_anim_playing = (animAction == Type::Damage || animAction == Type::DamageMove
                    || animAction == Type::AttackMove);
            }
            if (!blocking_anim_playing) {
                m_game->m_player->m_Controller.set_attack_end_time(0);
                m_game->m_player->m_Controller.set_command_available(true);
                m_game->m_player->m_Controller.set_command_time(0);
            }
        }
    }

    // Path 3: Animation completion — fires once when any animation finishes.
    // Respects attack swing time floor to prevent early unlock from short damage animations.
    if (update_ret == 2) {
        uint32_t now = GameClock::get_time_ms();
        uint32_t attack_end = m_game->m_player->m_Controller.get_attack_end_time();
        if (attack_end == 0 || now >= attack_end) {
            m_game->m_player->m_Controller.set_command_available(true);
            m_game->m_player->m_Controller.set_command_time(0);
        }
    }
    weather_manager::get().update(m_game->m_cur_time);

    m_game->command_processor(m_sMsX, m_sMsY,
        ((m_sDivX + m_pivot_x) * 32 + m_sModX + m_sMsX - 17) / 32 + 1,
        ((m_sDivY + m_pivot_y) * 32 + m_sModY + m_sMsY - 17) / 32 + 1,
        m_cLB, m_cRB);

    m_game->m_Camera.update(m_time);

    // Observer mode camera (additional updates for keyboard-driven movement)
    if (m_is_observer_mode) {
        if ((m_time - m_game->m_observer_cam_time) > 25) {
            m_game->m_observer_cam_time = m_time;
            m_game->m_Camera.update(m_time);
        }
    }

    // Glare color shift — triangle wave: 0..200..0 over 4 seconds.
    // Used as a channel reduction for non-primary colors in additive glare rendering.
    // Range 0-200 matches original DDraw proportion (0-25 on 5-bit channels ≈ 81%).
    {
        uint32_t phase = GameClock::get_time_ms() % 4000;
        if (phase < 2000)
            m_draw_flag = (phase * 200) / 2000;
        else
            m_draw_flag = 200 - ((phase - 2000) * 200) / 2000;
    }
}

void Screen_OnGame::on_render()
{
    // update all dialog boxes first (before drawing)
    m_game->get_dialog_box_manager().update_all();

    // update entity motion interpolation for all tiles
    uint32_t time = m_game->m_cur_time;
    for (int y = 0; y < MapDataSizeY; y++) {
        for (int x = 0; x < MapDataSizeX; x++) {
            m_game->m_map_data->m_data[x][y].m_motion.update(time);
        }
    }

    // Snap camera to player position BEFORE drawing to eliminate character vibration
    // This ensures viewport and entity position use the same motion offset
    if (!m_is_observer_mode)
    {
        int playerDX = m_game->m_player->m_player_x - m_game->m_map_data->m_pivot_x;
        int playerDY = m_game->m_player->m_player_y - m_game->m_map_data->m_pivot_y;
        if (playerDX >= 0 && playerDX < MapDataSizeX && playerDY >= 0 && playerDY < MapDataSizeY)
        {
            // Only apply motion offset when alive — dead players don't move,
            // but NPCs walking over the corpse tile update its motion state
            int camX, camY;
            if (m_game->m_player->m_hp > 0)
            {
                auto& motion = m_game->m_map_data->m_data[playerDX][playerDY].m_motion;
                camX = (m_game->m_player->m_player_x - VIEW_CENTER_TILE_X()) * 32
                     + static_cast<int>(motion.m_current_offset_x) - 16;
                camY = (m_game->m_player->m_player_y - VIEW_CENTER_TILE_Y()) * 32
                     + static_cast<int>(motion.m_current_offset_y) - 16;
            }
            else
            {
                camX = (m_game->m_player->m_player_x - VIEW_CENTER_TILE_X()) * 32 - 16;
                camY = (m_game->m_player->m_player_y - VIEW_CENTER_TILE_Y()) * 32 - 16;
            }
            m_game->m_Camera.snap_to(camX, camY);

            // Recalculate viewport to match snapped camera
            int val = m_game->m_Camera.get_x() - (m_pivot_x * 32);
            m_sDivX = val / 32;
            m_sModX = val % 32;
            val = m_game->m_Camera.get_y() - (m_pivot_y * 32);
            m_sDivY = val / 32;
            m_sModY = val % 32;
        }
    }

    // Apply camera shake after position snap so it affects rendering
    if (m_game->m_Camera.apply_shake())
    {
        int val = m_game->m_Camera.get_x() - (m_pivot_x * 32);
        m_sDivX = val / 32;
        m_sModX = val % 32;
        val = m_game->m_Camera.get_y() - (m_pivot_y * 32);
        m_sDivY = val / 32;
        m_sModY = val % 32;
    }

    // Main scene rendering
    FrameTiming::begin_profile(ProfileStage::draw_background);
    draw_background(m_sDivX, m_sModX, m_sDivY, m_sModY);
    FrameTiming::end_profile(ProfileStage::draw_background);

    FrameTiming::begin_profile(ProfileStage::draw_effect_lights);
    m_game->m_effect_manager->draw_effect_lights();
    FrameTiming::end_profile(ProfileStage::draw_effect_lights);

    // Tile grid BEFORE objects (so entities draw on top)
    draw_tile_grid();

    FrameTiming::begin_profile(ProfileStage::draw_objects);
    draw_objects(m_pivot_x, m_pivot_y, m_sDivX, m_sDivY, m_sModX, m_sModY, m_sMsX, m_sMsY);
    FrameTiming::end_profile(ProfileStage::draw_objects);

    FrameTiming::begin_profile(ProfileStage::draw_effects);
    m_game->m_effect_manager->draw_effects();
    FrameTiming::end_profile(ProfileStage::draw_effects);

#ifdef _DEBUG
    draw_spell_target_overlay();
#endif

    // Patching grid overlay (after effects, on top of everything for debug)
    draw_patching_grid();

    FrameTiming::begin_profile(ProfileStage::DrawWeather);
    weather_manager::get().draw();
    FrameTiming::end_profile(ProfileStage::DrawWeather);

    FrameTiming::begin_profile(ProfileStage::DrawChat);
    m_floating_text.draw_all(-100, 0, LOGICAL_WIDTH(), LOGICAL_HEIGHT(), m_game->m_cur_time, m_game->m_Renderer);
    FrameTiming::end_profile(ProfileStage::DrawChat);

    // Apocalypse map effects
    if (m_game->m_map_index == 26) {
        m_game->m_effect_sprites[89]->draw(1296 - m_game->m_Camera.get_x(), 1283 - m_game->m_Camera.get_y(), m_game->m_entity_state.m_effect_frame % 12, hb::shared::sprite::DrawParams::alpha_blend(0.5f));
        m_game->m_effect_sprites[89]->draw(1520 - m_game->m_Camera.get_x(), 1123 - m_game->m_Camera.get_y(), m_game->m_entity_state.m_effect_frame % 12, hb::shared::sprite::DrawParams::alpha_blend(0.5f));
        m_game->m_effect_sprites[89]->draw(1488 - m_game->m_Camera.get_x(), 3971 - m_game->m_Camera.get_y(), m_game->m_entity_state.m_effect_frame % 12, hb::shared::sprite::DrawParams::alpha_blend(0.5f));
        m_game->m_effect_sprites[93]->draw(2574 - m_game->m_Camera.get_x(), 3677 - m_game->m_Camera.get_y(), m_game->m_entity_state.m_effect_frame % 12, hb::shared::sprite::DrawParams::alpha_blend(0.5f));
        m_game->m_effect_sprites[93]->draw(3018 - m_game->m_Camera.get_x(), 3973 - m_game->m_Camera.get_y(), m_game->m_entity_state.m_effect_frame % 12, hb::shared::sprite::DrawParams::alpha_blend(0.5f));
    }
    else if (m_game->m_map_index == 27) {
        m_game->m_effect_sprites[89]->draw(1293 - m_game->m_Camera.get_x(), 3657 - m_game->m_Camera.get_y(), m_game->m_entity_state.m_effect_frame % 12, hb::shared::sprite::DrawParams::alpha_blend(0.5f));
        m_game->m_effect_sprites[89]->draw(944 - m_game->m_Camera.get_x(), 3881 - m_game->m_Camera.get_y(), m_game->m_entity_state.m_effect_frame % 12, hb::shared::sprite::DrawParams::alpha_blend(0.5f));
        m_game->m_effect_sprites[89]->draw(1325 - m_game->m_Camera.get_x(), 4137 - m_game->m_Camera.get_y(), m_game->m_entity_state.m_effect_frame % 12, hb::shared::sprite::DrawParams::alpha_blend(0.5f));
        m_game->m_effect_sprites[89]->draw(1648 - m_game->m_Camera.get_x(), 3913 - m_game->m_Camera.get_y(), m_game->m_entity_state.m_effect_frame % 12, hb::shared::sprite::DrawParams::alpha_blend(0.5f));
    }

    // Apocalypse gate
    if ((m_gate_posit_x >= m_game->m_Camera.get_x() / 32) && (m_gate_posit_x <= m_game->m_Camera.get_x() / 32 + VIEW_TILE_WIDTH())
        && (m_gate_posit_y >= m_game->m_Camera.get_y() / 32) && (m_gate_posit_y <= m_game->m_Camera.get_y() / 32 + VIEW_TILE_HEIGHT())) {
        m_game->m_effect_sprites[101]->draw(m_gate_posit_x * 32 - m_game->m_Camera.get_x() - 96, m_gate_posit_y * 32 - m_game->m_Camera.get_y() - 69, m_game->m_entity_state.m_effect_frame % 30, hb::shared::sprite::DrawParams::alpha_blend(0.5f));
    }

    // UI rendering
    FrameTiming::begin_profile(ProfileStage::DrawDialogs);
    m_game->get_dialog_box_manager().draw_all();
    FrameTiming::end_profile(ProfileStage::DrawDialogs);

    FrameTiming::begin_profile(ProfileStage::DrawMisc);
    if (text_input_manager::get().is_active()) {
        if (text_input_manager::get().shows_chat_background())
            m_game->m_Renderer->draw_rect_filled(0, LOGICAL_HEIGHT() - 69, LOGICAL_MAX_X(), 18, hb::shared::render::Color::Black(128));
        text_input_manager::get().show_input();
    }

    event_list_manager::get().show_events(m_game->m_cur_time);

    // Item tooltip on cursor
    short tooltip_item_id = CursorTarget::get_selected_id();
    if ((CursorTarget::GetSelectedType() == SelectedObjectType::Item) &&
        (tooltip_item_id >= 0) && (tooltip_item_id < hb::shared::limits::MaxItems) &&
        (m_game->m_player->m_item_list[tooltip_item_id] != 0))
    {
        render_item_tooltip();
    }

    // Druncncity bubbles (throttled to ~30/sec regardless of FPS)
    if (m_game->m_map_index == 25 && (m_game->m_cur_time - m_dwLastBubbleTime) >= 33)
    {
        m_dwLastBubbleTime = m_game->m_cur_time;
        m_game->m_effect_manager->add_effect(EffectType::BUBBLES_DRUNK, m_game->m_Camera.get_x() + rand() % LOGICAL_MAX_X(), m_game->m_Camera.get_y() + rand() % LOGICAL_MAX_Y(), 0, 0, -1 * (rand() % 80), 1);
    }

    // Heldenian tower count
    if ((m_heldenian_aresden_left_tower != -1) && (m_game->m_cur_location.starts_with("BtField"))) {
        std::string G_cTxt;
        G_cTxt = std::format("Aresden Flags : {}", m_heldenian_aresden_flags);
        hb::shared::text::draw_text(GameFont::Default, 10, 140, G_cTxt.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIWhite));
        G_cTxt = std::format("Elvine Flags : {}", m_heldenian_elvine_flags);
        hb::shared::text::draw_text(GameFont::Default, 10, 160, G_cTxt.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIWhite));
        G_cTxt = std::format("Aresden's rest building number : {}", m_heldenian_aresden_left_tower);
        hb::shared::text::draw_text(GameFont::Default, 10, 180, G_cTxt.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIWhite));
        G_cTxt = std::format("Elvine's rest building number : {}", m_heldenian_elvine_left_tower);
        hb::shared::text::draw_text(GameFont::Default, 10, 200, G_cTxt.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIWhite));
    }

    draw_top_msg();

    FrameTiming::end_profile(ProfileStage::DrawMisc);

    // FPS, latency, and profiling display moved to RenderFrame (global, all screens)
}

void Screen_OnGame::render_item_tooltip()
{
	std::string G_cTxt;
	short target_id = CursorTarget::get_selected_id();
    CItem* item = m_game->m_player->m_item_list[target_id].get();
    if (!item) return;
    CItem* cfg = m_game->get_item_config(item->m_id_num);
    if (!cfg) return;

    char item_color = item->m_instance.item_color;
    auto tooltip_draw = m_game->get_item_draw(cfg->m_display_id, item_atlas::pack, cfg->sprite_is_female());
    hb::shared::sprite::ISprite* sprite = tooltip_draw.sprite;
    int16_t frame = tooltip_draw.frame;
    bool is_equippable = cfg->is_armor() || cfg->is_weapon() || cfg->is_accessory();

    if (item_color != 0) {
        const auto& drag_tint = m_game->m_color_palette[item_color];
        sprite->draw(m_sMsX - CursorTarget::get_drag_dist_x(), m_sMsY - CursorTarget::get_drag_dist_y(), frame, hb::shared::sprite::DrawParams::tint(drag_tint.r, drag_tint.g, drag_tint.b));
    }
    else sprite->draw(m_sMsX - CursorTarget::get_drag_dist_x(), m_sMsY - CursorTarget::get_drag_dist_y(), frame);

    auto itemInfo = item_name_formatter::get().format(item);

    item_tooltip tooltip;

    // 1. Name — dye-colored for prefixed items, special color, or white
    bool has_prefix = item->m_instance.prefix_type != static_cast<uint8_t>(hb::shared::item::AttributePrefixType::None);
    if (has_prefix && item_color != 0)
    {
        const auto& dye = m_game->m_color_palette[item_color];
        tooltip.add_line(itemInfo.name, {dye.r, dye.g, dye.b, 255});
    }
    else if (itemInfo.is_special)
        tooltip.add_line(itemInfo.name, GameColors::UIItemName_Special);
    else
        tooltip.add_line(itemInfo.name, GameColors::UIWhite);

    // 2. Classification
    if (cfg->m_armor_class == armor_class::clothing)
    {
        const char* slot = equip_pos_name(cfg->get_equip_pos());
        if (cfg->get_equip_pos() == EquipPos::Leggings)
            slot = cfg->m_is_skirt ? "Skirt" : "Pants";
        else if (cfg->get_equip_pos() == EquipPos::Body)
            slot = (cfg->m_gender_requirement == 2) ? "Bodice" : "Shirt";
        G_cTxt = std::format("Clothing - {}", slot);
        tooltip.add_line(G_cTxt, GameColors::InfoGrayLight);
    }
    else if (cfg->m_armor_class == armor_class::armor)
    {
        const char* slot = equip_pos_name(cfg->get_equip_pos());
        if (cfg->get_equip_pos() == EquipPos::Body)
            slot = "Chest";
        G_cTxt = std::format("Armor - {}", slot);
        tooltip.add_line(G_cTxt, GameColors::InfoGrayLight);
    }
    else if (cfg->is_weapon())
    {
        const char* slot = equip_pos_name(cfg->get_equip_pos());
        G_cTxt = std::format("Weapon - {}", slot);
        tooltip.add_line(G_cTxt, GameColors::InfoGrayLight);
    }
    else if (cfg->get_item_sub_type() == item_sub_type::angelic)
    {
        tooltip.add_line("Accessory - Angelic Pendant", GameColors::InfoGrayLight);
    }
    else if (cfg->get_item_sub_type() == item_sub_type::pendant)
    {
        tooltip.add_line("Accessory - Pendant", GameColors::InfoGrayLight);
    }
    else if (cfg->is_accessory())
    {
        const char* slot = equip_pos_name(cfg->get_equip_pos());
        G_cTxt = std::format("Accessory - {}", slot);
        tooltip.add_line(G_cTxt, GameColors::InfoGrayLight);
    }

    // 3. Base combat stats with inline modifiers
    // Collect inline modifiers from effects
    std::string damage_mod_str;
    std::string defense_mod_str;
    std::string weight_mod_str;
    std::string durability_mod_str;
    for (const auto& eff : itemInfo.effects)
    {
        if (eff.category == effect_category::inline_damage)
        {
            if (damage_mod_str.empty()) damage_mod_str = " (";
            else damage_mod_str += ", ";
            damage_mod_str += eff.label;
        }
        else if (eff.category == effect_category::inline_defense)
        {
            if (defense_mod_str.empty()) defense_mod_str = " (";
            else defense_mod_str += ", ";
            defense_mod_str += eff.label;
        }
        else if (eff.category == effect_category::inline_weight)
        {
            if (weight_mod_str.empty()) weight_mod_str = " (";
            else weight_mod_str += ", ";
            weight_mod_str += eff.label;
        }
        else if (eff.category == effect_category::inline_durability)
        {
            durability_mod_str = eff.value;
        }
    }
    if (!damage_mod_str.empty()) damage_mod_str += ")";
    if (!defense_mod_str.empty()) defense_mod_str += ")";
    if (!weight_mod_str.empty()) weight_mod_str += ")";

    // Weapon damage
    if (cfg->get_equip_pos() == EquipPos::RightHand || cfg->get_equip_pos() == EquipPos::TwoHand)
    {
        auto range = cfg->get_damage_range();
        G_cTxt = std::format("Damage: {}-{}", range.min, range.max);
        if (!damage_mod_str.empty())
            tooltip.add_dual_line(G_cTxt, GameColors::InfoGrayLight, damage_mod_str, GameColors::UIItemName_Special);
        else
            tooltip.add_line(G_cTxt, GameColors::InfoGrayLight);
    }
    // Shield/armor defense
    else if (cfg->is_armor() || cfg->get_equip_pos() == EquipPos::LeftHand)
    {
        G_cTxt = std::format("Defence: +{}%", cfg->m_item_effect_value1);
        if (!defense_mod_str.empty())
            tooltip.add_dual_line(G_cTxt, GameColors::InfoGrayLight, defense_mod_str, GameColors::UIItemName_Special);
        else
            tooltip.add_line(G_cTxt, GameColors::InfoGrayLight);
    }

    // 4. Standalone bonuses
    for (const auto& eff : itemInfo.effects)
    {
        if (eff.category != effect_category::standalone) continue;
        if (eff.label.empty() && eff.value.empty()) continue;
        if (eff.value.empty())
            tooltip.add_line(eff.label, GameColors::InfoGrayLight);
        else
            tooltip.add_dual_line(eff.label, GameColors::InfoGrayLight, eff.value, GameColors::UIItemName_Special);
    }

    // 5. Consumable info — HP/MP/SP potions and food
    auto effectType = cfg->get_item_effect_type();
    if (effectType == ItemEffectType::HP)
    {
        auto range = parse_dice(cfg->m_item_effect_value1, cfg->m_item_effect_value2, cfg->m_item_effect_value3);
        G_cTxt = std::format(TOOLTIP_RESTORES_HP, range.min, range.max);
        tooltip.add_line(G_cTxt, GameColors::InfoGrayLight);
    }
    else if (effectType == ItemEffectType::MP)
    {
        auto range = parse_dice(cfg->m_item_effect_value1, cfg->m_item_effect_value2, cfg->m_item_effect_value3);
        G_cTxt = std::format(TOOLTIP_RESTORES_MP, range.min, range.max);
        tooltip.add_line(G_cTxt, GameColors::InfoGrayLight);
    }
    else if (effectType == ItemEffectType::SP)
    {
        auto range = parse_dice(cfg->m_item_effect_value1, cfg->m_item_effect_value2, cfg->m_item_effect_value3);
        G_cTxt = std::format(TOOLTIP_RESTORES_SP, range.min, range.max);
        tooltip.add_line(G_cTxt, GameColors::InfoGrayLight);
    }
    else if (effectType == ItemEffectType::HPStock)
    {
        auto range = parse_dice(cfg->m_item_effect_value1, cfg->m_item_effect_value2, cfg->m_item_effect_value3);
        G_cTxt = std::format(DRAW_DIALOGBOX_SHOP28, range.min, range.max);
        tooltip.add_line(G_cTxt, GameColors::InfoGrayLight);
    }

    // 6. Requirements
    if (cfg->m_level_requirement != 0)
    {
        G_cTxt = std::format("{}: {}", DRAW_DIALOGBOX_SHOP24, cfg->m_level_requirement);
        tooltip.add_line(G_cTxt, GameColors::InfoGrayLight);
    }

    int light_pct = item->get_light_percent();
    int eff_weight = (light_pct > 0) ? cfg->m_weight * (100 - light_pct) / 100 : cfg->m_weight;
    if (is_equippable && eff_weight >= hb::shared::balance::equip_str_threshold)
    {
        int req_str = static_cast<int>(std::ceil(eff_weight / static_cast<float>(hb::shared::balance::weight_units_per_stone)));
        if (cfg->get_equip_pos() == EquipPos::RightHand || cfg->get_equip_pos() == EquipPos::TwoHand)
        {
            int full_speed_str = cfg->m_swing_speed * hb::shared::balance::swing_str_divisor;
            G_cTxt = std::format("Required Str: {} ({} full speed)", req_str, full_speed_str);
        }
        else
        {
            G_cTxt = std::format("Required Str: {}", req_str);
        }
        tooltip.add_line(G_cTxt, GameColors::InfoGrayLight);
    }

    // 7. Weight — using shared weight functions, inline light modifier
    if (cfg->m_weight > 0)
    {
        float unit_stones = CItem::weight_to_stones(eff_weight);
        if (cfg->is_stackable() && item->m_instance.count > 1)
        {
            int stack_raw = CItem::calc_item_stack_weight(eff_weight, static_cast<int>(item->m_instance.count));
            float total_stones = CItem::weight_to_stones(stack_raw);
            G_cTxt = std::format(TOOLTIP_WEIGHT_STACK, total_stones, unit_stones);
        }
        else
        {
            G_cTxt = std::format(TOOLTIP_WEIGHT, unit_stones);
        }
        if (!weight_mod_str.empty())
            tooltip.add_dual_line(G_cTxt, GameColors::InfoGrayLight, weight_mod_str, GameColors::UIItemName_Special);
        else
            tooltip.add_line(G_cTxt, GameColors::InfoGrayLight);
    }

    // 8. Durability / Usages
    if (is_equippable)
    {
        if (!durability_mod_str.empty())
        {
            // Strong prefix: show boosted durability with green values
            int boost_pct = 0;
            try { boost_pct = std::stoi(durability_mod_str.substr(1)); } catch (...) {}
            int boosted_max = cfg->m_durability * (100 + boost_pct) / 100;
            G_cTxt = std::format("Durability: {}/{}", item->m_instance.cur_durability, boosted_max);
            std::string boost_str = std::format(" ({})", durability_mod_str);
            tooltip.add_dual_line(G_cTxt, GameColors::UIItemName_Special, boost_str, GameColors::UIItemName_Special);
        }
        else
        {
            G_cTxt = std::format(UPDATE_SCREEN_ONGAME10, item->m_instance.cur_durability, cfg->m_durability);
            tooltip.add_line(G_cTxt, GameColors::InfoGrayLight);
        }
    }
    else if (effectType == ItemEffectType::AlterItemDrop && cfg->m_durability > 0)
    {
        G_cTxt = std::format(TOOLTIP_USAGES, item->m_instance.cur_durability, cfg->m_durability);
        tooltip.add_line(G_cTxt, GameColors::InfoGrayLight);
    }

    // 9. Stack count
    if (cfg->is_stackable())
    {
        auto count = std::count_if(m_game->m_player->m_item_list.begin(), m_game->m_player->m_item_list.end(),
            [item](const std::unique_ptr<CItem>& otherItem) {
                return otherItem != nullptr && otherItem->m_id_num == item->m_id_num;
            });

        if (count > 1)
        {
            G_cTxt = std::format(DEF_MSG_TOTAL_NUMBER, static_cast<int>(count));
            tooltip.add_line(G_cTxt, GameColors::UIDescription);
        }
    }

    tooltip.draw(m_sMsX, m_sMsY + 25, m_game->m_Renderer);
}

//=============================================================================
// draw_spell_target_overlay - Debug overlay showing spell AoE tiles (DEBUG ONLY)
// Draws faint blue outlines on tiles that would be affected by the current spell
// while the player is in targeting mode (m_is_get_pointing_mode).
//=============================================================================
void Screen_OnGame::draw_spell_target_overlay()
{
#ifndef _DEBUG
    return;
#else
    if (!m_is_get_pointing_mode) return;
    if (m_point_command_type < 100) return;

    int magicId = m_point_command_type - 100;
    if (magicId < 0 || magicId >= hb::shared::limits::MaxMagicType) return;
    if (!m_game->m_magic_cfg_list[magicId]) return;

    CMagic* magic = m_game->m_magic_cfg_list[magicId].get();
    if (magic->m_type == 0) return;

    int casterX = m_game->m_player->m_player_x;
    int casterY = m_game->m_player->m_player_y;

    // Mouse world tile (same formula as command_processor call on line 383)
    int targetX = ((m_sDivX + m_pivot_x) * 32 + m_sModX + m_sMsX - 17) / 32 + 1;
    int targetY = ((m_sDivY + m_pivot_y) * 32 + m_sModY + m_sMsY - 17) / 32 + 1;

    spell_aoe_params params;
    params.m_magic_type = magic->m_type;
    params.m_aoe_radius_x = magic->m_aoe_radius_x;
    params.m_aoe_radius_y = magic->m_aoe_radius_y;
    params.m_dynamic_pattern = magic->m_dynamic_pattern;
    params.m_dynamic_radius = magic->m_dynamic_radius;

    constexpr int MAX_TILES = 512;
    spell_aoe_tile tiles[MAX_TILES];
    int tileCount = SpellAoE::calculate_tiles(params,
        casterX, casterY, targetX, targetY,
        tiles, MAX_TILES);

    if (tileCount <= 0) return;

    constexpr int HALF_TILE = 16;
    hb::shared::render::Color overlayColor(100, 180, 255, 60);

    for (int i = 0; i < tileCount; i++) {
        int sx = (tiles[i].m_x - (m_sDivX + m_pivot_x)) * 32 - m_sModX - HALF_TILE;
        int sy = (tiles[i].m_y - (m_sDivY + m_pivot_y)) * 32 - m_sModY - HALF_TILE;
        m_game->m_Renderer->draw_rect_outline(sx, sy, 32, 32, overlayColor);
    }
#endif
}

//=============================================================================
// draw_tile_grid - Simple dark grid lines showing tile boundaries (DEBUG ONLY)
// Enabled via F12 > Graphics > Tile Grid toggle
// Called BEFORE objects so it appears below entities
//=============================================================================
void Screen_OnGame::draw_tile_grid()
{
#ifndef _DEBUG
    return;
#else
    if (!config_manager::get().is_tile_grid_enabled()) return;

    constexpr int TILE_SIZE = 32;
    constexpr int HALF_TILE = 16;
    constexpr float GRID_ALPHA = 0.18f;

    int screenW = LOGICAL_WIDTH();
    int screenH = LOGICAL_HEIGHT();

    // draw dark gray grid lines (subtle)
    hb::shared::render::Color gridColor(40, 40, 40, static_cast<uint8_t>(GRID_ALPHA * 255));
    for (int x = -m_sModX + HALF_TILE; x <= screenW; x += TILE_SIZE) {
        m_game->m_Renderer->draw_line(x, 0, x, screenH, gridColor);
    }
    for (int y = -m_sModY + HALF_TILE; y <= screenH; y += TILE_SIZE) {
        m_game->m_Renderer->draw_line(0, y, screenW, y, gridColor);
    }
#endif
}

//=============================================================================
// draw_patching_grid - Debug grid with direction zone colors (DEBUG ONLY)
// Enabled via F12 > Graphics > Patching Grid toggle
//=============================================================================
void Screen_OnGame::draw_patching_grid()
{
#ifndef _DEBUG
    return;
#else
    if (!config_manager::get().is_patching_grid_enabled()) return;

    constexpr int TILE_SIZE = 32;
    constexpr int HALF_TILE = 16;
    constexpr float ZONE_ALPHA = 0.25f;
    constexpr float GRID_ALPHA = 0.4f;

    int screenW = LOGICAL_WIDTH();
    int screenH = LOGICAL_HEIGHT();

    short playerX = m_game->m_player->m_player_x;
    short playerY = m_game->m_player->m_player_y;

    // Asymmetric zones algorithm (N/S 3:1, E/W 4:1)
    auto calcDir = [](short playerX, short playerY, short destX, short destY) -> int {
        short dx = destX - playerX;
        short dy = destY - playerY;
        if (dx == 0 && dy == 0) return 0;

        short absX = (dx < 0) ? -dx : dx;
        short absY = (dy < 0) ? -dy : dy;

        // Cardinal directions
        if (absY == 0) return (dx > 0) ? 3 : 7;  // E or W
        if (absX == 0) return (dy < 0) ? 1 : 5;  // N or S
        if (absY >= absX * 3) return (dy < 0) ? 1 : 5;  // N or S zone
        if (absX >= absY * 4) return (dx > 0) ? 3 : 7;  // E or W zone

        // Diagonal fallback
        if (dx > 0 && dy < 0) return 2;  // NE
        if (dx > 0 && dy > 0) return 4;  // SE
        if (dx < 0 && dy > 0) return 6;  // SW
        return 8;  // NW
    };

    auto getDirColor = [](int dir, int& r, int& g, int& b) {
        switch (dir) {
            case 1: case 3: case 5: case 7: r = 0; g = 200; b = 0; break;
            case 2: case 4: case 6: case 8: r = 200; g = 0; b = 0; break;
            default: r = 150; g = 0; b = 200; break;
        }
    };

    int startTileX = m_sDivX + m_pivot_x;
    int startTileY = m_sDivY + m_pivot_y;
    int tilesX = (screenW / TILE_SIZE) + 3;
    int tilesY = (screenH / TILE_SIZE) + 3;

    for (int ty = -1; ty < tilesY; ty++) {
        for (int tx = -1; tx < tilesX; tx++) {
            short mapX = static_cast<short>(startTileX + tx);
            short mapY = static_cast<short>(startTileY + ty);

            int dir = calcDir(playerX, playerY, mapX, mapY);
            int r, g, b;
            getDirColor(dir, r, g, b);

            int screenX = tx * TILE_SIZE - m_sModX - HALF_TILE;
            int screenY = ty * TILE_SIZE - m_sModY - HALF_TILE;

            hb::shared::render::Color zoneColor(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b), static_cast<uint8_t>(ZONE_ALPHA * 255));
            for (int i = 0; i < TILE_SIZE; i++) {
                m_game->m_Renderer->draw_line(screenX, screenY + i, screenX + TILE_SIZE, screenY + i, zoneColor);
            }
        }
    }

    // draw subtle dark grid lines over the colored zones
    hb::shared::render::Color overlayGridColor(20, 20, 20, static_cast<uint8_t>(0.35f * 255));
    for (int x = -m_sModX + HALF_TILE; x <= screenW; x += TILE_SIZE) {
        m_game->m_Renderer->draw_line(x, 0, x, screenH, overlayGridColor);
    }
    for (int y = -m_sModY + HALF_TILE; y <= screenH; y += TILE_SIZE) {
        m_game->m_Renderer->draw_line(0, y, screenW, y, overlayGridColor);
    }
#endif
}

void Screen_OnGame::item_drop_external_screen(char item_id, short mouse_x, short mouse_y)
{
    std::string name;
    short owner_type, dialog_x, dialog_y;
    short npc_config_id = -1;
    hb::shared::entity::PlayerStatus status;

    if (inventory_manager::get().check_item_operation_enabled(item_id) == false) return;

    if ((m_game->m_mcx != 0) && (m_game->m_mcy != 0) && (abs(m_game->m_player->m_player_x - m_game->m_mcx) <= 8) && (abs(m_game->m_player->m_player_y - m_game->m_mcy) <= 8))
    {
        name.clear();
        m_game->m_map_data->get_owner(m_game->m_mcx, m_game->m_mcy, name, &owner_type, &status, &m_game->m_comm_object_id, &npc_config_id);
        if (m_game->m_player->m_player_name == name)
        {
        }
        else
        {
            CItem* cfg = m_game->get_item_config(m_game->m_player->m_item_list[item_id]->m_id_num);
            if (cfg && (cfg->is_stackable())
                && (m_game->m_player->m_item_list[item_id]->m_instance.count > 1))
            {
                m_game->get_dialog_box_manager().get_dialog_box(DialogBoxId::ItemDropExternal)->m_x = mouse_x - 140;
                m_game->get_dialog_box_manager().get_dialog_box(DialogBoxId::ItemDropExternal)->m_y = mouse_y - 70;
                if (m_game->get_dialog_box_manager().get_dialog_box(DialogBoxId::ItemDropExternal)->m_y < 0) m_game->get_dialog_box_manager().get_dialog_box(DialogBoxId::ItemDropExternal)->m_y = 0;
                auto* dropDlg = m_game->get_dialog_box_manager().get_dialog_as<DialogBox_ItemDropAmount>(DialogBoxId::ItemDropExternal);
                if (hb::shared::owner::can_receive_items(owner_type) && dropDlg)
                {
                    dropDlg->m_drop_x = m_game->m_mcx;
                    dropDlg->m_drop_y = m_game->m_mcy;
                    dropDlg->m_drop_target_type = owner_type;
                    dropDlg->m_drop_target_id = m_game->m_comm_object_id;
                    std::memset(dropDlg->m_target_name, 0, sizeof(dropDlg->m_target_name));
                    if (owner_type < 10)
                        std::snprintf(dropDlg->m_target_name, sizeof(dropDlg->m_target_name), "%s", name.c_str());
                    else
                        std::snprintf(dropDlg->m_target_name, sizeof(dropDlg->m_target_name), "%s", m_game->get_npc_config_name_by_id(npc_config_id));
                }
                else if (dropDlg)
                {
                    dropDlg->m_drop_x = 0;
                    dropDlg->m_drop_y = 0;
                    dropDlg->m_drop_target_type = 0;
                    dropDlg->m_drop_target_id = 0;
                    std::memset(dropDlg->m_target_name, 0, sizeof(dropDlg->m_target_name));
                }
                m_game->get_dialog_box_manager().enable_dialog_box(DialogBoxId::ItemDropExternal, item_id, static_cast<int64_t>(m_game->m_player->m_item_list[item_id]->m_instance.count), 0);
            }
            else
            {
                switch (owner_type) {
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                {
                    auto* npcDlg = m_game->get_dialog_box_manager().get_dialog_as<DialogBox_NpcActionQuery>(DialogBoxId::NpcActionQuery);
                    npcDlg->enable_with_target(1, item_id, owner_type, 1, m_game->m_comm_object_id, m_game->m_mcx, m_game->m_mcy, name.c_str());
                    dialog_x = mouse_x - 117;
                    dialog_y = mouse_y - 50;
                    if (dialog_x < 0) dialog_x = 0;
                    if ((dialog_x + 235) > LOGICAL_MAX_X()) dialog_x = LOGICAL_MAX_X() - 235;
                    if (dialog_y < 0) dialog_y = 0;
                    if ((dialog_y + 100) > LOGICAL_MAX_Y()) dialog_y = LOGICAL_MAX_Y() - 100;
                    npcDlg->m_x = dialog_x;
                    npcDlg->m_y = dialog_y;
                }	break;

                case hb::shared::owner::Kennedy:
                    if (cfg)
                        {
                        	auto pkt = hb::net::make_common_command_str(CommonType::GiveItemToChar, m_game->m_player->m_player_x, m_game->m_player->m_player_y, item_id);
                        	pkt.v1 = 1;
                        	pkt.v2 = m_game->m_mcx;
                        	pkt.v3 = m_game->m_mcy;
                        	std::snprintf(pkt.text, sizeof(pkt.text), "%s", cfg->m_name);
                        	pkt.v4 = m_game->m_comm_object_id;
                        	m_game->send_game_packet(pkt);
                        }
                    break;

                case hb::shared::owner::Howard: // Howard
                {
                    auto* npcDlg = m_game->get_dialog_box_manager().get_dialog_as<DialogBox_NpcActionQuery>(DialogBoxId::NpcActionQuery);
                    npcDlg->enable_with_target(3, item_id, owner_type, 1, m_game->m_comm_object_id, m_game->m_mcx, m_game->m_mcy, m_game->get_npc_config_name_by_id(npc_config_id));
                    dialog_x = mouse_x - 117;
                    dialog_y = mouse_y - 50;
                    if (dialog_x < 0) dialog_x = 0;
                    if ((dialog_x + 235) > LOGICAL_MAX_X()) dialog_x = LOGICAL_MAX_X() - 235;
                    if (dialog_y < 0) dialog_y = 0;
                    if ((dialog_y + 100) > LOGICAL_MAX_Y()) dialog_y = LOGICAL_MAX_Y() - 100;
                    npcDlg->m_x = dialog_x;
                    npcDlg->m_y = dialog_y;
                }	break;

                case hb::shared::owner::ShopKeeper: // ShopKeeper-W
                case hb::shared::owner::Tom: // Tom
                {
                    auto* npcDlg = m_game->get_dialog_box_manager().get_dialog_as<DialogBox_NpcActionQuery>(DialogBoxId::NpcActionQuery);
                    npcDlg->enable_with_target(2, item_id, owner_type, 1, m_game->m_comm_object_id, m_game->m_mcx, m_game->m_mcy, m_game->get_npc_config_name_by_id(npc_config_id));
                    dialog_x = mouse_x - 117;
                    dialog_y = mouse_y - 50;
                    if (dialog_x < 0) dialog_x = 0;
                    if ((dialog_x + 235) > LOGICAL_MAX_X()) dialog_x = LOGICAL_MAX_X() - 235;
                    if (dialog_y < 0) dialog_y = 0;
                    if ((dialog_y + 100) > LOGICAL_MAX_Y()) dialog_y = LOGICAL_MAX_Y() - 100;
                    npcDlg->m_x = dialog_x;
                    npcDlg->m_y = dialog_y;
                }	break;

                default:
                    if (cfg)
                    {
                        if (m_game->item_drop_history(m_game->m_player->m_item_list[item_id]->m_id_num))
                        {
                            m_game->get_dialog_box_manager().get_dialog_box(DialogBoxId::ItemDropConfirm)->m_x = mouse_x - 140;
                            m_game->get_dialog_box_manager().get_dialog_box(DialogBoxId::ItemDropConfirm)->m_y = mouse_y - 70;
                            if (m_game->get_dialog_box_manager().get_dialog_box(DialogBoxId::ItemDropConfirm)->m_y < 0) m_game->get_dialog_box_manager().get_dialog_box(DialogBoxId::ItemDropConfirm)->m_y = 0;
                            m_game->get_dialog_box_manager().enable_dialog_box(DialogBoxId::ItemDropConfirm, item_id, static_cast<int64_t>(m_game->m_player->m_item_list[item_id]->m_instance.count), 0);
                        }
                        else
                        {
                            {
                            	auto pkt = hb::net::make_common_command_str(CommonType::ItemDrop, m_game->m_player->m_player_x, m_game->m_player->m_player_y);
                            	pkt.v1 = item_id;
                            	pkt.v2 = 1;
                            	std::snprintf(pkt.text, sizeof(pkt.text), "%s", cfg->m_name);
                            	m_game->send_game_packet(pkt);
                            }
                        }
                    }
                    break;
                }
            }
            inventory_manager::get().lock_item(item_id);
        }
    }
    else
    {
        CItem* cfg2 = m_game->get_item_config(m_game->m_player->m_item_list[item_id]->m_id_num);
        if (cfg2 && (cfg2->is_stackable())
            && (m_game->m_player->m_item_list[item_id]->m_instance.count > 1))
        {
            m_game->get_dialog_box_manager().get_dialog_box(DialogBoxId::ItemDropExternal)->m_x = mouse_x - 140;
            m_game->get_dialog_box_manager().get_dialog_box(DialogBoxId::ItemDropExternal)->m_y = mouse_y - 70;
            if (m_game->get_dialog_box_manager().get_dialog_box(DialogBoxId::ItemDropExternal)->m_y < 0) m_game->get_dialog_box_manager().get_dialog_box(DialogBoxId::ItemDropExternal)->m_y = 0;
            if (auto* dropDlg2 = m_game->get_dialog_box_manager().get_dialog_as<DialogBox_ItemDropAmount>(DialogBoxId::ItemDropExternal))
            {
                dropDlg2->m_drop_x = 0;
                dropDlg2->m_drop_y = 0;
                dropDlg2->m_drop_target_type = 0;
                dropDlg2->m_drop_target_id = 0;
                std::memset(dropDlg2->m_target_name, 0, sizeof(dropDlg2->m_target_name));
            }
            m_game->get_dialog_box_manager().enable_dialog_box(DialogBoxId::ItemDropExternal, item_id, static_cast<int64_t>(m_game->m_player->m_item_list[item_id]->m_instance.count), 0);
        }
        else
        {
            if (m_game->item_drop_history(m_game->m_player->m_item_list[item_id]->m_id_num))
            {
                m_game->get_dialog_box_manager().get_dialog_box(DialogBoxId::ItemDropConfirm)->m_x = mouse_x - 140;
                m_game->get_dialog_box_manager().get_dialog_box(DialogBoxId::ItemDropConfirm)->m_y = mouse_y - 70;
                if (m_game->get_dialog_box_manager().get_dialog_box(DialogBoxId::ItemDropConfirm)->m_y < 0) m_game->get_dialog_box_manager().get_dialog_box(DialogBoxId::ItemDropConfirm)->m_y = 0;
                m_game->get_dialog_box_manager().enable_dialog_box(DialogBoxId::ItemDropConfirm, item_id, static_cast<int64_t>(m_game->m_player->m_item_list[item_id]->m_instance.count), 0);
            }
            else
            {
                if (cfg2)
                {
                	auto pkt = hb::net::make_common_command_str(CommonType::ItemDrop, m_game->m_player->m_player_x, m_game->m_player->m_player_y);
                	pkt.v1 = item_id;
                	pkt.v2 = 1;
                	std::snprintf(pkt.text, sizeof(pkt.text), "%s", cfg2->m_name);
                	m_game->send_game_packet(pkt);
                }
            }
        }
        inventory_manager::get().lock_item(item_id);
    }
}
