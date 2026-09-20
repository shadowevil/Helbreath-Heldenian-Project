// Screen_SelectCharacter.cpp: Select Character Screen Implementation
//
//////////////////////////////////////////////////////////////////////

#include "Screen_SelectCharacter.h"
#include "Screen_OnGame.h"
#include "Game.h"
#include "WeatherManager.h"
#include "GameModeManager.h"
#include "InputStateHelper.h"
#include "IInput.h"
#include "GlobalDef.h"
#include "ASIOSocket.h"
#include "Misc.h"
#include "lan_eng.h"
#include "GameFonts.h"
#include "TextLibExt.h"
#include "button.h"
#include "IRenderer.h"
#include "Packet/SharedPackets.h"
#include "CharInfo.h"
#include "AudioManager.h"
#include <format>
#include <string>

using namespace hb::shared::net;
using namespace hb::shared::action;
using namespace hb::client::sprite_id;

Screen_SelectCharacter::Screen_SelectCharacter(CGame* game)
    : IGameScreen(game)
    , m_dwSelCharCTime(0)
{
}

void Screen_SelectCharacter::on_initialize()
{
    weather_manager::get().set_ambient_light(1);

    m_game->m_arrow_pressed = 0;
    m_dwSelCharCTime = GameClock::get_time_ms();

    // === CControls setup ===
    m_controls.set_screen_size(LOGICAL_WIDTH(), LOGICAL_HEIGHT());
    m_controls.set_hover_focus(false);
    m_controls.set_focus_nav_horizontal(true);
    m_controls.set_enter_advances_focus(false);

    // 4 character slot buttons (in focus order)
    for (int i = 0; i < 4; ++i)
    {
        int slot_id = SLOT_1 + i;
        auto* btn = m_controls.add<cc::button>(slot_id, cc::rect{100 + i * 111 + OX, 50 + OY, 110, 200});

        btn->set_click_sound([this] { audio_manager::get().play_game_sound(sound_type::effect, 14, 5); });
        int capture_id = slot_id;
        btn->set_on_click([this, capture_id](int) {
            uint32_t now = GameClock::get_time_ms();
            if (m_enter_edge)
            {
                activate_slot(capture_id);
            }
            else if (m_last_clicked_slot == capture_id && (now - m_last_click_time) <= DOUBLE_CLICK_MS)
            {
                activate_slot(capture_id);
                m_last_clicked_slot = -1;
            }
            else
            {
                m_last_clicked_slot = capture_id;
                m_last_click_time = now;
            }
        });

        btn->set_render_handler([this, i](const cc::control& c) {
            int sprite_frame = c.is_focused() ? 62 : 61;
            m_game->m_sprite[InterfaceNdButton]->draw(OX + 110 + i * 109 - 7, 63 - 9 + OY, sprite_frame);
        });
    }

    // 5 menu buttons (NOT in focus order — hover-only highlights)
    auto* btn_enter = m_controls.add<cc::button>(BTN_ENTER, cc::rect{360 + OX, 283 + OY, 185, 32});
    btn_enter->set_on_click([this](int) { enter_game(); });
    btn_enter->set_click_sound([this] { audio_manager::get().play_game_sound(sound_type::effect, 14, 5); });

    auto* btn_new = m_controls.add<cc::button>(BTN_NEW, cc::rect{360 + OX, 316 + OY, 185, 29});
    btn_new->set_on_click([this](int) {
        if (m_game->m_total_char < 4)
            m_game->change_game_mode(GameMode::CreateNewCharacter);
    });
    btn_new->set_click_sound([this] { audio_manager::get().play_game_sound(sound_type::effect, 14, 5); });

    auto* btn_delete = m_controls.add<cc::button>(BTN_DELETE, cc::rect{360 + OX, 346 + OY, 185, 29});
    btn_delete->set_on_click([this](int) {
        int slot_index = m_controls.focused_id() - SLOT_1;
        if (slot_index >= 0 && slot_index < 4 && m_game->m_char_list[slot_index] != nullptr)
        {
            m_game->change_game_mode(GameMode::QueryDeleteCharacter);
            m_game->m_enter_game_type = slot_index + 1;
        }
    });
    btn_delete->set_click_sound([this] { audio_manager::get().play_game_sound(sound_type::effect, 14, 5); });

    auto* btn_change_pw = m_controls.add<cc::button>(BTN_CHANGE_PW, cc::rect{360 + OX, 376 + OY, 185, 29});
    btn_change_pw->set_on_click([this](int) { m_game->change_game_mode(GameMode::ChangePassword); });
    btn_change_pw->set_click_sound([this] { audio_manager::get().play_game_sound(sound_type::effect, 14, 5); });

    auto* btn_exit = m_controls.add<cc::button>(BTN_EXIT, cc::rect{360 + OX, 406 + OY, 185, 29});
    btn_exit->set_on_click([this](int) { m_game->change_game_mode(GameMode::MainMenu); });
    btn_exit->set_click_sound([this] { audio_manager::get().play_game_sound(sound_type::effect, 14, 5); });

    // Focus order: only the 4 slot buttons
    m_controls.set_focus_order({SLOT_1, SLOT_2, SLOT_3, SLOT_4});
    m_controls.set_focus(SLOT_1);

    // Sync game focus
    m_game->m_cur_focus = 1;

    // Discard any pending input from previous screen/overlay
    cc::input_state init_input;
    hb::client::fill_input_state(init_input);
    m_controls.discard_pending_input(init_input);
    m_prev_input = init_input;
}
void Screen_SelectCharacter::on_update()
{
    uint32_t time = GameClock::get_time_ms();
    m_game->m_cur_time = time;

    // Fill input state and update controls
    cc::input_state input;
    hb::client::fill_input_state(input);

    // Handle overlay suppression transition — discard input during suppression
    // and on the first frame after suppression lifts to prevent false edges
    bool suppressed = hb::shared::input::is_suppressed();
    bool discard = suppressed || m_was_suppressed;
    m_was_suppressed = suppressed;

    if (discard)
    {
        m_controls.discard_pending_input(input);
        m_prev_input = input;
        m_enter_edge = false;
    }
    else
    {
        m_enter_edge = input.key_enter && !m_prev_input.key_enter;
    }

    m_controls.update(input, time);

    // Reset click tracking when focus changes via keyboard
    if (m_controls.focused_id() != m_last_clicked_slot)
        m_last_clicked_slot = -1;

    // ESC → main menu
    if (m_controls.escape_pressed())
    {
        m_game->change_game_mode(GameMode::MainMenu);
        return;
    }

    // Sync CControls focus → game->m_cur_focus for DialogBox_CityHallMenu compatibility
    int focused = m_controls.focused_id();
    if (focused >= SLOT_1 && focused <= SLOT_4)
        m_game->m_cur_focus = focused;

    // Animation loop
    if ((time - m_dwSelCharCTime) > 100)
    {
        m_game->m_menu_frame++;
        m_dwSelCharCTime = time;
    }
    if (m_game->m_menu_frame >= 8)
    {
        m_game->m_menu_dir_cnt++;
        if (m_game->m_menu_dir_cnt > 8)
        {
            m_game->m_menu_dir++;
            m_game->m_menu_dir_cnt = 1;
        }
        m_game->m_menu_frame = 0;
    }
    if (m_game->m_menu_dir > direction::northwest) m_game->m_menu_dir = direction::north;

    m_prev_input = input;
}

void Screen_SelectCharacter::activate_slot(int slot_id)
{
    int index = slot_id - SLOT_1;
    if (m_game->m_char_list[index] != nullptr)
        enter_game();
    else
        m_game->change_game_mode(GameMode::CreateNewCharacter);
}

bool Screen_SelectCharacter::enter_game()
{
    int slot_index = m_controls.focused_id() - SLOT_1;
    if (slot_index < 0 || slot_index >= 4) return false;
    if (!m_game->m_char_list[slot_index]) return false;
    if (m_game->m_char_list[slot_index]->m_sex == 0) return false;

    m_game->m_selected_char_name = m_game->m_char_list[slot_index]->m_name.c_str();
    m_game->m_selected_char_level = static_cast<int>(m_game->m_char_list[slot_index]->m_level);
    if (!CMisc::check_valid_string(m_game->m_selected_char_name.c_str())) return false;

    m_game->m_sprite[InterfaceNdLogin]->Unload();
    m_game->m_sprite[InterfaceNdMainMenu]->Unload();
    m_game->m_enter_game_type = EnterGameMsg::New;
    m_game->m_map_name = m_game->m_char_list[slot_index]->m_map_name;

    // Build enter game packet for deferred send after connection establishes
    hb::net::EnterGameRequestFull req{};
    req.header.msg_id = MsgId::request_enter_game;
    req.header.msg_type = static_cast<uint16_t>(m_game->m_enter_game_type);
    std::snprintf(req.character_name, sizeof(req.character_name), "%s", m_game->m_selected_char_name.c_str());
    std::snprintf(req.map_name, sizeof(req.map_name), "%s", m_game->m_map_name.c_str());
    std::snprintf(req.account_name, sizeof(req.account_name), "%s", m_game->m_account_name.c_str());
    std::snprintf(req.password, sizeof(req.password), "%s", m_game->m_account_password.c_str());
    req.level = m_game->m_selected_char_level;
    std::snprintf(req.world_name, sizeof(req.world_name), "%s", m_game->m_world_server_name.c_str());
    req.version_major = hb::version::compatibility::major;
    req.version_minor = hb::version::compatibility::minor;
    req.version_patch = hb::version::compatibility::patch;
    m_game->set_pending_login_packet(req);

    m_game->m_l_sock = std::make_unique<hb::shared::net::ASIOSocket>(m_game->m_io_pool->get_context(), game_limits::socket_block_limit);
    m_game->m_l_sock->connect(m_game->m_log_server_addr.c_str(), m_game->m_log_server_port);
    m_game->m_l_sock->init_buffer_size(hb::shared::limits::MsgBufferSize);
    m_game->change_game_mode(GameMode::Connecting);
    std::snprintf(m_game->m_msg, sizeof(m_game->m_msg), "%s", "33");
    return true;
}

bool Screen_SelectCharacter::on_net_response(uint16_t response_type, char* data)
{
    switch (response_type) {
    case LogResMsg::CharacterDeleted:
    {
        const auto* list = hb::net::PacketCast<hb::net::PacketLogCharacterListHeader>(
            data, sizeof(hb::net::PacketLogCharacterListHeader));
        if (!list) return true;
        m_game->m_total_char = std::min(static_cast<int>(list->total_chars), 4);
        for (int i = 0; i < 4; i++)
            if (m_game->m_char_list[i] != 0)
                m_game->m_char_list[i].reset();

        const auto* entries = reinterpret_cast<const hb::net::PacketLogCharacterEntry*>(
            data + sizeof(hb::net::PacketLogCharacterListHeader));
        for (int i = 0; i < m_game->m_total_char; i++) {
            const auto& entry = entries[i];
            m_game->m_char_list[i] = std::make_unique<CCharInfo>();
            m_game->m_char_list[i]->m_name.assign(entry.name, strnlen(entry.name, sizeof(entry.name)));
            m_game->m_char_list[i]->m_appearance = entry.appearance;
            m_game->m_char_list[i]->m_sex = entry.sex;
            m_game->m_char_list[i]->m_skin_color = entry.skin;
            m_game->m_char_list[i]->m_level = entry.level;
            m_game->m_char_list[i]->m_exp = entry.exp;
            m_game->m_char_list[i]->m_map_name.assign(entry.map_name, strnlen(entry.map_name, sizeof(entry.map_name)));
        }
        std::snprintf(m_game->m_msg, sizeof(m_game->m_msg), "%s", "3A");
        m_game->change_game_mode(GameMode::LogResMsg);
        return true;
    }

    case EnterGameRes::Playing:
        m_game->change_game_mode(GameMode::QueryForceLogin);
        return true;

    case EnterGameRes::Confirm:
    {
        const auto* pkt = hb::net::PacketCast<hb::net::PacketLogEnterGameConfirm>(
            data, sizeof(hb::net::PacketLogEnterGameConfirm));
        if (!pkt) return true;
        m_game->m_game_server_name.assign(pkt->game_server_name, strnlen(pkt->game_server_name, sizeof(pkt->game_server_name)));
        GameModeManager::set_screen<Screen_OnGame>();
        return true;
    }

    case EnterGameRes::Reject:
    {
        const auto* pkt = hb::net::PacketCast<hb::net::PacketLogResponseCode>(data, sizeof(hb::net::PacketLogResponseCode));
        if (!pkt) return true;
        std::memset(m_game->m_msg, 0, sizeof(m_game->m_msg));
        switch (pkt->code) {
        case 1: std::snprintf(m_game->m_msg, sizeof(m_game->m_msg), "%s", "3E"); break;
        case 2: std::snprintf(m_game->m_msg, sizeof(m_game->m_msg), "%s", "3F"); break;
        case 3: std::snprintf(m_game->m_msg, sizeof(m_game->m_msg), "%s", "33"); break;
        case 4: std::snprintf(m_game->m_msg, sizeof(m_game->m_msg), "%s", "3D"); break;
        case 5: std::snprintf(m_game->m_msg, sizeof(m_game->m_msg), "%s", "3G"); break;
        case 6: std::snprintf(m_game->m_msg, sizeof(m_game->m_msg), "%s", "3Z"); break;
        case 7: std::snprintf(m_game->m_msg, sizeof(m_game->m_msg), "%s", "3J"); break;
        }
        m_game->change_game_mode(GameMode::LogResMsg);
        return true;
    }

    case EnterGameRes::ForceDisconn:
        std::snprintf(m_game->m_msg, sizeof(m_game->m_msg), "%s", "3X");
        m_game->change_game_mode(GameMode::LogResMsg);
        return true;

    case LogResMsg::NotExistingCharacter:
        std::snprintf(m_game->m_msg, sizeof(m_game->m_msg), "%s", "Not existing character!");
        m_game->change_game_mode(GameMode::Msg);
        return true;

    case LogResMsg::PasswordChangeSuccess:
        std::snprintf(m_game->m_msg, sizeof(m_game->m_msg), "%s", "6B");
        m_game->change_game_mode(GameMode::LogResMsg);
        return true;

    case LogResMsg::PasswordChangeFail:
        std::snprintf(m_game->m_msg, sizeof(m_game->m_msg), "%s", "6C");
        m_game->change_game_mode(GameMode::LogResMsg);
        return true;

    case LogResMsg::PasswordMismatch:
        std::snprintf(m_game->m_msg, sizeof(m_game->m_msg), "%s", "11");
        m_game->change_game_mode(GameMode::LogResMsg);
        return true;

    case LogResMsg::ForceChangePassword:
        std::snprintf(m_game->m_msg, sizeof(m_game->m_msg), "%s", "6M");
        m_game->change_game_mode(GameMode::LogResMsg);
        return true;
    }
    return false;
}

void Screen_SelectCharacter::on_render()
{
    // Background
    m_game->draw_new_dialog_box(InterfaceNdSelectChar, 0, 0, 0);
    m_game->draw_new_dialog_box(InterfaceNdButton, OX, OY, 50);

    // Slot highlights (via render handlers)
    m_controls.render();

    // Character previews
    render_character_previews();

    // Menu button base sprites
    m_game->draw_new_dialog_box(InterfaceNdButton, OX, OY, 51);
    m_game->draw_new_dialog_box(InterfaceNdButton, OX, OY, 52);
    m_game->draw_new_dialog_box(InterfaceNdButton, OX, OY, 53);
    m_game->draw_new_dialog_box(InterfaceNdButton, OX, OY, 54);
    m_game->draw_new_dialog_box(InterfaceNdButton, OX, OY, 55);

    // Menu button hover highlights
    auto* btn_enter = m_controls.find_as<cc::button>(BTN_ENTER);
    auto* btn_new = m_controls.find_as<cc::button>(BTN_NEW);
    auto* btn_delete = m_controls.find_as<cc::button>(BTN_DELETE);
    auto* btn_change_pw = m_controls.find_as<cc::button>(BTN_CHANGE_PW);
    auto* btn_exit = m_controls.find_as<cc::button>(BTN_EXIT);

    if (btn_enter && btn_enter->is_hovered())
        m_game->draw_new_dialog_box(InterfaceNdButton, OX, OY, 56);
    else if (btn_new && btn_new->is_hovered())
        m_game->draw_new_dialog_box(InterfaceNdButton, OX, OY, 57);
    else if (btn_delete && btn_delete->is_hovered())
        m_game->draw_new_dialog_box(InterfaceNdButton, OX, OY, 58);
    else if (btn_change_pw && btn_change_pw->is_hovered())
        m_game->draw_new_dialog_box(InterfaceNdButton, OX, OY, 59);
    else if (btn_exit && btn_exit->is_hovered())
        m_game->draw_new_dialog_box(InterfaceNdButton, OX, OY, 60);

    // Tooltip text
    render_tooltip_text();

    // Account info
    render_account_info();

    // Footer text
    hb::shared::text::draw_text_aligned(GameFont::Default, 122 + OX, 456 + OY, (315) - (122), 15, UPDATE_SCREEN_ON_SELECT_CHARACTER36, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);

    m_game->draw_version();

    // Debug: draw control bounds in fuchsia
    if (m_controls.debug_draw_enabled())
    {
        m_controls.debug_walk([this](const cc::control& c) {
            auto sb = c.screen_bounds();
            m_game->m_Renderer->draw_rect_outline(sb.x, sb.y, sb.w, sb.h,
                hb::shared::render::Color(255, 0, 255));
        });
    }
}

void Screen_SelectCharacter::render_character_previews()
{
    short sX = OX;
    short sY = 10 + OY;
    uint32_t time = GameClock::get_time_ms();

    for (int i = 0; i < 4; i++)
    {
        if (m_game->m_char_list[i] != nullptr)
        {
            switch (m_game->m_char_list[i]->m_sex) {
            case 1: m_game->m_entity_state.m_owner_type = hb::shared::owner::MaleFirst; break;
            case 2: m_game->m_entity_state.m_owner_type = hb::shared::owner::FemaleFirst; break;
            }
            m_game->m_entity_state.m_owner_type += m_game->m_char_list[i]->m_skin_color - 1;
            m_game->m_entity_state.m_dir = m_game->m_menu_dir;
            m_game->m_entity_state.m_appearance = m_game->m_char_list[i]->m_appearance;

            m_game->m_entity_state.m_name.fill('\0');
            std::snprintf(m_game->m_entity_state.m_name.data(), m_game->m_entity_state.m_name.size(), "%s", m_game->m_char_list[i]->m_name.c_str());

            m_game->m_entity_state.m_action = Type::Move;
            m_game->m_entity_state.m_frame = m_game->m_menu_frame;

            if (m_game->m_char_list[i]->m_sex != 0)
            {
                if (CMisc::check_valid_string(m_game->m_char_list[i]->m_name.data()) == true)
                {
                    m_game->m_effect_sprites[0]->draw(sX + 157 + i * 109, sY + 138, 1, hb::shared::sprite::DrawParams::additive_no_color_key(0.25f));
                    m_game->draw_object_on_move_for_menu(0, 0, sX + 157 + i * 109, sY + 138, false, time);
                    hb::shared::text::draw_text(GameFont::Default, sX + 112 + i * 109, sY + 179 - 9, m_game->m_char_list[i]->m_name.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UISelectPurple));
                    int _sLevel = m_game->m_char_list[i]->m_level;
                    std::string charInfoBuf;
                    charInfoBuf = std::format("{}", _sLevel);
                    hb::shared::text::draw_text(GameFont::Default, sX + 138 + i * 109, sY + 196 - 10, charInfoBuf.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UISelectPurple));

                    charInfoBuf = m_game->format_comma_number(m_game->m_char_list[i]->m_exp);
                    hb::shared::text::draw_text(GameFont::Default, sX + 138 + i * 109, sY + 211 - 10, charInfoBuf.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UISelectPurple));
                }
            }
        }
    }
}

void Screen_SelectCharacter::render_tooltip_text()
{
    auto* btn_enter = m_controls.find_as<cc::button>(BTN_ENTER);
    auto* btn_new = m_controls.find_as<cc::button>(BTN_NEW);
    auto* btn_delete = m_controls.find_as<cc::button>(BTN_DELETE);
    auto* btn_change_pw = m_controls.find_as<cc::button>(BTN_CHANGE_PW);
    auto* btn_exit = m_controls.find_as<cc::button>(BTN_EXIT);

    if (btn_enter && btn_enter->is_hovered())
    {
        hb::shared::text::draw_text_aligned(GameFont::Default, 98 + OX, 290 + 15 + OY, (357) - (98), 15, UPDATE_SCREEN_ON_SELECT_CHARACTER1, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
        hb::shared::text::draw_text_aligned(GameFont::Default, 98 + OX, 305 + 15 + OY, (357) - (98), 15, UPDATE_SCREEN_ON_SELECT_CHARACTER2, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
        hb::shared::text::draw_text_aligned(GameFont::Default, 98 + OX, 320 + 15 + OY, (357) - (98), 15, UPDATE_SCREEN_ON_SELECT_CHARACTER3, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
        hb::shared::text::draw_text_aligned(GameFont::Default, 98 + OX, 335 + 15 + OY, (357) - (98), 15, UPDATE_SCREEN_ON_SELECT_CHARACTER4, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
    }
    else if (btn_new && btn_new->is_hovered())
    {
        hb::shared::text::draw_text_aligned(GameFont::Default, 98 + OX, 305 + 15 + OY, (357) - (98), 15, UPDATE_SCREEN_ON_SELECT_CHARACTER5, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
    }
    else if (btn_delete && btn_delete->is_hovered())
    {
        hb::shared::text::draw_text_aligned(GameFont::Default, 98 + OX, 275 + 15 + OY, (357) - (98), 15, UPDATE_SCREEN_ON_SELECT_CHARACTER6, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
        hb::shared::text::draw_text_aligned(GameFont::Default, 98 + OX, 290 + 15 + OY, (357) - (98), 15, UPDATE_SCREEN_ON_SELECT_CHARACTER7, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
    }
    else if (btn_change_pw && btn_change_pw->is_hovered())
    {
        hb::shared::text::draw_text_aligned(GameFont::Default, 98 + OX, 305 + 15 + OY, (357) - (98), 15, UPDATE_SCREEN_ON_SELECT_CHARACTER12, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
    }
    else if (btn_exit && btn_exit->is_hovered())
    {
        hb::shared::text::draw_text_aligned(GameFont::Default, 98 + OX, 305 + 15 + OY, (357) - (98), 15, UPDATE_SCREEN_ON_SELECT_CHARACTER13, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
    }
    else
    {
        // Default tooltip based on total character count
        char total_char = 0;
        for (int i = 0; i < 4; i++)
        {
            if (m_game->m_char_list[i] != nullptr)
                total_char++;
        }

        if (total_char == 0)
        {
            hb::shared::text::draw_text_aligned(GameFont::Default, 98 + OX, 275 + 15 + OY, (357) - (98), 15, UPDATE_SCREEN_ON_SELECT_CHARACTER14, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
            hb::shared::text::draw_text_aligned(GameFont::Default, 98 + OX, 290 + 15 + OY, (357) - (98), 15, UPDATE_SCREEN_ON_SELECT_CHARACTER15, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
            hb::shared::text::draw_text_aligned(GameFont::Default, 98 + OX, 305 + 15 + OY, (357) - (98), 15, UPDATE_SCREEN_ON_SELECT_CHARACTER16, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
            hb::shared::text::draw_text_aligned(GameFont::Default, 98 + OX, 320 + 15 + OY, (357) - (98), 15, UPDATE_SCREEN_ON_SELECT_CHARACTER17, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
            hb::shared::text::draw_text_aligned(GameFont::Default, 98 + OX, 335 + 15 + OY, (357) - (98), 15, UPDATE_SCREEN_ON_SELECT_CHARACTER18, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
        }
        else if (total_char < 4)
        {
            hb::shared::text::draw_text_aligned(GameFont::Default, 98 + OX, 275 + 15 + OY, (357) - (98), 15, UPDATE_SCREEN_ON_SELECT_CHARACTER19, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
            hb::shared::text::draw_text_aligned(GameFont::Default, 98 + OX, 290 + 15 + OY, (357) - (98), 15, UPDATE_SCREEN_ON_SELECT_CHARACTER20, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
            hb::shared::text::draw_text_aligned(GameFont::Default, 98 + OX, 305 + 15 + OY, (357) - (98), 15, UPDATE_SCREEN_ON_SELECT_CHARACTER21, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
            hb::shared::text::draw_text_aligned(GameFont::Default, 98 + OX, 320 + 15 + OY, (357) - (98), 15, UPDATE_SCREEN_ON_SELECT_CHARACTER22, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
            hb::shared::text::draw_text_aligned(GameFont::Default, 98 + OX, 335 + 15 + OY, (357) - (98), 15, UPDATE_SCREEN_ON_SELECT_CHARACTER23, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
            hb::shared::text::draw_text_aligned(GameFont::Default, 98 + OX, 350 + 15 + OY, (357) - (98), 15, UPDATE_SCREEN_ON_SELECT_CHARACTER24, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
        }
        if (total_char == 4)
        {
            hb::shared::text::draw_text_aligned(GameFont::Default, 98 + OX, 290 + 15 + OY, (357) - (98), 15, UPDATE_SCREEN_ON_SELECT_CHARACTER25, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
            hb::shared::text::draw_text_aligned(GameFont::Default, 98 + OX, 305 + 15 + OY, (357) - (98), 15, UPDATE_SCREEN_ON_SELECT_CHARACTER26, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
            hb::shared::text::draw_text_aligned(GameFont::Default, 98 + OX, 320 + 15 + OY, (357) - (98), 15, UPDATE_SCREEN_ON_SELECT_CHARACTER27, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
            hb::shared::text::draw_text_aligned(GameFont::Default, 98 + OX, 335 + 15 + OY, (357) - (98), 15, UPDATE_SCREEN_ON_SELECT_CHARACTER28, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
        }
    }
}

void Screen_SelectCharacter::render_account_info()
{
    int temp_mon, temp_day, temp_hour, temp_min;
    std::string infoBuf;
    temp_mon = temp_day = temp_hour = temp_min = 0;

    // Last login tracking
    int year = 0, month = 0, day = 0, hour = 0, minute = 0;
    int64_t temp1 = 0, temp2 = 0;

    for (int i = 0; i < 4; i++)
    {
        if (m_game->m_char_list[i] != nullptr && m_game->m_char_list[i]->m_sex != 0)
        {
            temp2 = (int64_t)m_game->m_char_list[i]->m_year * 1000000 + (int64_t)m_game->m_char_list[i]->m_month * 60000 + (int64_t)m_game->m_char_list[i]->m_day * 1700 + (int64_t)m_game->m_char_list[i]->m_hour * 70 + (int64_t)m_game->m_char_list[i]->m_minute;
            if (temp1 < temp2)
            {
                year = m_game->m_char_list[i]->m_year;
                month = m_game->m_char_list[i]->m_month;
                day = m_game->m_char_list[i]->m_day;
                hour = m_game->m_char_list[i]->m_hour;
                minute = m_game->m_char_list[i]->m_minute;
                temp1 = temp2;
            }
        }
    }

    if (m_game->m_accnt_year != 0)
    {
        temp_min = (m_game->m_time_left_sec_account / 60);
        infoBuf = std::format(UPDATE_SCREEN_ON_SELECT_CHARACTER37, m_game->m_accnt_year, m_game->m_accnt_month, m_game->m_accnt_day);
    }
    else
    {
        if (m_game->m_time_left_sec_account > 0)
        {
            temp_day = (m_game->m_time_left_sec_account / (60 * 60 * 24));
            temp_hour = (m_game->m_time_left_sec_account / (60 * 60)) % 24;
            temp_min = (m_game->m_time_left_sec_account / 60) % 60;
            infoBuf = std::format(UPDATE_SCREEN_ON_SELECT_CHARACTER38, temp_day, temp_hour, temp_min);
        }
        else infoBuf = UPDATE_SCREEN_ON_SELECT_CHARACTER39;
    }
    hb::shared::text::draw_text_aligned(GameFont::Default, 98 + OX, 385 + 10 + OY, (357) - (98), 15, infoBuf.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);

    if (m_game->m_ip_year != 0)
    {
        temp_hour = (m_game->m_time_left_sec_ip / (60 * 60));
        temp_min = (m_game->m_time_left_sec_ip / 60) % 60;
        infoBuf = std::format(UPDATE_SCREEN_ON_SELECT_CHARACTER40, temp_hour, temp_min);
    }
    else
    {
        if (m_game->m_time_left_sec_ip > 0)
        {
            temp_day = (m_game->m_time_left_sec_ip / (60 * 60 * 24));
            temp_hour = (m_game->m_time_left_sec_ip / (60 * 60)) % 24;
            temp_min = (m_game->m_time_left_sec_ip / 60) % 60;
            infoBuf = std::format(UPDATE_SCREEN_ON_SELECT_CHARACTER41, temp_day, temp_hour, temp_min);
        }
        else infoBuf = UPDATE_SCREEN_ON_SELECT_CHARACTER42;
    }
    hb::shared::text::draw_text_aligned(GameFont::Default, 98 + OX, 400 + 10 + OY, (357) - (98), 15, infoBuf.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);

    if (year != 0)
    {
        infoBuf = std::format(UPDATE_SCREEN_ON_SELECT_CHARACTER43, year, month, day, hour, minute);
        hb::shared::text::draw_text_aligned(GameFont::Default, 98 + OX, 415 + 10 + OY, (357) - (98), 15, infoBuf.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
    }
}
