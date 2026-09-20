// Screen_CreateNewCharacter.cpp: Create New Character Screen Implementation
//
//////////////////////////////////////////////////////////////////////

#include "Screen_CreateNewCharacter.h"
#include "Game.h"
#include "SharedCalculations.h"
#include "Screen_OnGame.h"
#include "GameModeManager.h"
#include "InputStateHelper.h"
#include "TextFieldRenderer.h"
#include "GlobalDef.h"
#include "ASIOSocket.h"
#include "Misc.h"
#include "lan_eng.h"
#include "GameFonts.h"
#include "TextLibExt.h"
#include "IInput.h"
#include "button.h"
#include "textbox.h"
#include "toggle_button.h"
#include "IRenderer.h"
#include "Packet/SharedPackets.h"
#include "CharacterClass.h"
#include "CharInfo.h"
#include "AudioManager.h"
#include <algorithm>
#include <format>
#include <memory>

using namespace hb::shared::net;
using namespace hb::shared::action;
using namespace hb::client::sprite_id;

Screen_CreateNewCharacter::Screen_CreateNewCharacter(CGame* game)
    : IGameScreen(game)
    , m_iNewCharPoint(game->m_creation_stat_points)
    , m_dwNewCharMTime(0)
    , m_bNewCharFlag(false)
{
}

void Screen_CreateNewCharacter::on_initialize()
{
    // Initialize character creation defaults
    m_game->m_new_char.gender = rand() % 2 + 1;
    m_game->m_new_char.skin_col = rand() % 3 + 1;
    m_game->m_new_char.hair_style = rand() % 8;
    m_game->m_new_char.hair_col = 32 + (rand() % 16);
    m_game->m_new_char.under_col = rand() % 8;
    m_game->m_new_char.stat_str = static_cast<int8_t>(m_game->m_base_stat_value);
    m_game->m_new_char.stat_vit = static_cast<int8_t>(m_game->m_base_stat_value);
    m_game->m_new_char.stat_dex = static_cast<int8_t>(m_game->m_base_stat_value);
    m_game->m_new_char.stat_int = static_cast<int8_t>(m_game->m_base_stat_value);
    m_game->m_new_char.stat_mag = static_cast<int8_t>(m_game->m_base_stat_value);
    m_game->m_new_char.stat_chr = static_cast<int8_t>(m_game->m_base_stat_value);

    m_iNewCharPoint = m_game->m_creation_stat_points;
    m_bNewCharFlag = false;
    m_game->m_arrow_pressed = 0;
    m_dwNewCharMTime = GameClock::get_time_ms();

    // === CControls setup ===
    m_controls.set_screen_size(LOGICAL_WIDTH(), LOGICAL_HEIGHT());
    m_controls.set_hover_focus(true);
    m_controls.set_enter_advances_focus(false);

    m_controls.set_clipboard_provider(
        [] { return hb::shared::input::get_clipboard_text(); },
        [](const std::string& s) { hb::shared::input::set_clipboard_text(s); }
    );

    // === Name textbox ===
    auto* tb = m_controls.add<cc::textbox>(TXT_NAME, cc::rect{ 272, 167, 108, 19 }, 11);
    tb->set_character_filter(hb::client::character_name_allowed_chars);
    tb->set_validator([](const std::string& s) {
        return !s.empty() && CMisc::check_valid_name(s.c_str());
    });
    tb->set_render_handler([this](const cc::control& c) {
        auto& field = static_cast<const cc::text_input&>(c);
        hb::client::draw_text_field(field, GameClock::get_time_ms(),
            hb::shared::text::TextStyle::from_color(GameColors::UILabel),
            hb::shared::text::TextStyle::from_color(GameColors::UILabel));
    });

    // === Appearance toggles (NOT in focus order) ===
    // Layout: minus on left (0,0,21,13), plus on right (23,0,21,13)
    // Gender: 1-2 wrap
    auto* tog_gender = m_controls.add<cc::toggle_button>(TOG_GENDER,
        cc::rect{316, 217 + (16 * 0), 43, 15}, cc::rect{0, 0, 21, 15}, cc::rect{22, 0, 21, 15});
    tog_gender->set_click_sound([this] { audio_manager::get().play_game_sound(sound_type::effect, 14, 5); });
    tog_gender->set_on_change([this](int, int delta) {
        m_game->m_new_char.gender += delta;
        if (m_game->m_new_char.gender < 1) m_game->m_new_char.gender = 2;
        if (m_game->m_new_char.gender > 2) m_game->m_new_char.gender = 1;
    });

    // Skin color: 1-3 wrap
    auto* tog_skin = m_controls.add<cc::toggle_button>(TOG_SKIN,
        cc::rect{ 316, 217 + (16 * 1), 43, 15}, cc::rect{0, 0, 21, 15}, cc::rect{22, 0, 21, 15});
    tog_skin->set_click_sound([this] { audio_manager::get().play_game_sound(sound_type::effect, 14, 5); });
    tog_skin->set_on_change([this](int, int delta) {
        m_game->m_new_char.skin_col += delta;
        if (m_game->m_new_char.skin_col < 1) m_game->m_new_char.skin_col = 3;
        if (m_game->m_new_char.skin_col > 3) m_game->m_new_char.skin_col = 1;
    });

    // Hair style: 0-7 wrap
    auto* tog_hair_style = m_controls.add<cc::toggle_button>(TOG_HAIR_STYLE,
        cc::rect{ 316, 217 + (16 * 2), 43, 15 }, cc::rect{ 0, 0, 21, 15 }, cc::rect{ 22, 0, 21, 15 });
    tog_hair_style->set_click_sound([this] { audio_manager::get().play_game_sound(sound_type::effect, 14, 5); });
    tog_hair_style->set_on_change([this](int, int delta) {
        m_game->m_new_char.hair_style += delta;
        if (m_game->m_new_char.hair_style < 0) m_game->m_new_char.hair_style = 7;
        if (m_game->m_new_char.hair_style > 7) m_game->m_new_char.hair_style = 0;
    });

    // Hair color: 32-47 wrap (unified palette hair range)
    auto* tog_hair_color = m_controls.add<cc::toggle_button>(TOG_HAIR_COLOR,
        cc::rect{ 316, 217 + (16 * 3), 43, 15 }, cc::rect{ 0, 0, 21, 15 }, cc::rect{ 22, 0, 21, 15 });
    tog_hair_color->set_click_sound([this] { audio_manager::get().play_game_sound(sound_type::effect, 14, 5); });
    tog_hair_color->set_on_change([this](int, int delta) {
        m_game->m_new_char.hair_col += delta;
        if (m_game->m_new_char.hair_col < 32) m_game->m_new_char.hair_col = 47;
        if (m_game->m_new_char.hair_col > 47) m_game->m_new_char.hair_col = 32;
    });

    // Underwear color: 0-7 wrap
    auto* tog_underwear = m_controls.add<cc::toggle_button>(TOG_UNDERWEAR,
        cc::rect{ 316, 217 + (16 * 4), 43, 15 }, cc::rect{ 0, 0, 21, 15 }, cc::rect{ 22, 0, 21, 15 });
    tog_underwear->set_click_sound([this] { audio_manager::get().play_game_sound(sound_type::effect, 14, 5); });
    tog_underwear->set_on_change([this](int, int delta) {
        m_game->m_new_char.under_col += delta;
        if (m_game->m_new_char.under_col < 0) m_game->m_new_char.under_col = 7;
        if (m_game->m_new_char.under_col > 7) m_game->m_new_char.under_col = 0;
    });

    // === Stat toggles (NOT in focus order) ===
    // Layout: plus on LEFT (0,0,21,13), minus on RIGHT (23,0,21,13)
    // (Original UI has left=increase, right=decrease for stats)

    auto make_stat_handler = [this](int8_t& stat) {
        return [this, &stat](int, int delta) {
            int8_t max_stat = static_cast<int8_t>(m_game->m_base_stat_value + m_game->m_max_creation_stat_value);
            int8_t min_stat = static_cast<int8_t>(m_game->m_base_stat_value);
            if (delta > 0)
            {
                if (m_iNewCharPoint > 0 && stat < max_stat) { stat++; m_iNewCharPoint--; }
            }
            else
            {
                if (stat > min_stat) { stat--; m_iNewCharPoint++; }
            }
        };
    };

    auto& nc = m_game->m_new_char;

    auto make_stat_toggle = [&](int id, int y, int8_t& stat) {
        auto* tog = m_controls.add<cc::toggle_button>(id, cc::rect{316, y, 43, 15},
            cc::rect{ 22, 0, 21, 15 }, cc::rect{ 0, 0, 21, 15 });
        tog->set_click_sound([this] { audio_manager::get().play_game_sound(sound_type::effect, 14, 5); });
        tog->set_on_change(make_stat_handler(stat));
    };
    make_stat_toggle(TOG_STR, 336 + (16 * 0), nc.stat_str);
    make_stat_toggle(TOG_VIT, 336 + (16 * 1), nc.stat_vit);
    make_stat_toggle(TOG_DEX, 336 + (16 * 2), nc.stat_dex);
    make_stat_toggle(TOG_INT, 336 + (16 * 3), nc.stat_int);
    make_stat_toggle(TOG_MAG, 336 + (16 * 4), nc.stat_mag);
    make_stat_toggle(TOG_CHR, 336 + (16 * 5), nc.stat_chr);

    // === Action buttons ===
    // Create button
    auto* btn_create = m_controls.add<cc::button>(BTN_CREATE, cc::rect{464, 505, ui_layout::btn_size_x, ui_layout::btn_size_y});
    btn_create->set_click_sound([this] { audio_manager::get().play_game_sound(sound_type::effect, 14, 5); });
    btn_create->set_on_click([this](int) {
        if (m_prev_focused != BTN_CREATE) return;
        if (!m_bNewCharFlag) return;
        submit_create_character();
    });
    btn_create->set_render_handler([this](const cc::control& c) {
        auto sb = c.screen_bounds();
        int frame = (c.is_focused() && m_bNewCharFlag) ? 25 : 24;
        m_game->m_sprite[InterfaceNdButton]->draw(sb.x, sb.y, frame);
    });

    // Cancel button
    auto* btn_cancel = m_controls.add<cc::button>(BTN_CANCEL, cc::rect{580, 505, ui_layout::btn_size_x, ui_layout::btn_size_y });
    btn_cancel->set_click_sound([this] { audio_manager::get().play_game_sound(sound_type::effect, 14, 5); });
    btn_cancel->set_on_click([this](int) {
        if (m_prev_focused != BTN_CANCEL) return;
        m_game->change_game_mode(GameMode::SelectCharacter);
    });
    btn_cancel->set_render_handler([this](const cc::control& c) {
        auto sb = c.screen_bounds();
        m_game->m_sprite[InterfaceNdButton]->draw(sb.x, sb.y, c.is_focused() ? 17 : 16);
    });

    // Warrior preset button
    auto* btn_warrior = m_controls.add<cc::button>(BTN_WARRIOR, cc::rect{140, 505, ui_layout::btn_size_x, ui_layout::btn_size_y});
    btn_warrior->set_click_sound([this] { audio_manager::get().play_game_sound(sound_type::effect, 14, 5); });
    btn_warrior->set_on_click([this](int) {
        if (m_prev_focused != BTN_WARRIOR) return;
        int8_t base = static_cast<int8_t>(m_game->m_base_stat_value);
        int8_t max_bonus = static_cast<int8_t>(m_game->m_max_creation_stat_value);
        int8_t remaining = static_cast<int8_t>(std::clamp(m_game->m_creation_stat_points - max_bonus * 2, 0, (int)max_bonus));
        m_game->m_new_char.stat_str = base + max_bonus;
        m_game->m_new_char.stat_vit = base + remaining;
        m_game->m_new_char.stat_dex = base + max_bonus;
        m_game->m_new_char.stat_int = base;
        m_game->m_new_char.stat_mag = base;
        m_game->m_new_char.stat_chr = base;
        int total = m_game->m_base_stat_value * 6 + m_game->m_creation_stat_points;
        m_iNewCharPoint = total - (m_game->m_new_char.stat_str + m_game->m_new_char.stat_vit
            + m_game->m_new_char.stat_dex + m_game->m_new_char.stat_int
            + m_game->m_new_char.stat_mag + m_game->m_new_char.stat_chr);
        m_selected_class = BTN_WARRIOR;
    });
    btn_warrior->set_render_handler([this](const cc::control& c) {
        auto sb = c.screen_bounds();
        bool highlight = c.is_focused() || m_selected_class == BTN_WARRIOR;
        m_game->m_sprite[InterfaceNdButton]->draw(sb.x, sb.y, highlight ? 68 : 67);
    });

    // Mage preset button
    auto* btn_mage = m_controls.add<cc::button>(BTN_MAGE, cc::rect{225, 505, ui_layout::btn_size_x, ui_layout::btn_size_y });
    btn_mage->set_click_sound([this] { audio_manager::get().play_game_sound(sound_type::effect, 14, 5); });
    btn_mage->set_on_click([this](int) {
        if (m_prev_focused != BTN_MAGE) return;
        int8_t base = static_cast<int8_t>(m_game->m_base_stat_value);
        int8_t max_bonus = static_cast<int8_t>(m_game->m_max_creation_stat_value);
        int8_t remaining = static_cast<int8_t>(std::clamp(m_game->m_creation_stat_points - max_bonus * 2, 0, (int)max_bonus));
        m_game->m_new_char.stat_str = base;
        m_game->m_new_char.stat_vit = base + remaining;
        m_game->m_new_char.stat_dex = base;
        m_game->m_new_char.stat_int = base + max_bonus;
        m_game->m_new_char.stat_mag = base + max_bonus;
        m_game->m_new_char.stat_chr = base;
        int total = m_game->m_base_stat_value * 6 + m_game->m_creation_stat_points;
        m_iNewCharPoint = total - (m_game->m_new_char.stat_str + m_game->m_new_char.stat_vit
            + m_game->m_new_char.stat_dex + m_game->m_new_char.stat_int
            + m_game->m_new_char.stat_mag + m_game->m_new_char.stat_chr);
        m_selected_class = BTN_MAGE;
    });
    btn_mage->set_render_handler([this](const cc::control& c) {
        auto sb = c.screen_bounds();
        bool highlight = c.is_focused() || m_selected_class == BTN_MAGE;
        m_game->m_sprite[InterfaceNdButton]->draw(sb.x, sb.y, highlight ? 66 : 65);
    });

    // Priest preset button
    auto* btn_priest = m_controls.add<cc::button>(BTN_MASTER, cc::rect{310, 505, ui_layout::btn_size_x, ui_layout::btn_size_y });
    btn_priest->set_click_sound([this] { audio_manager::get().play_game_sound(sound_type::effect, 14, 5); });
    btn_priest->set_on_click([this](int) {
        if (m_prev_focused != BTN_MASTER) return;
        int8_t base = static_cast<int8_t>(m_game->m_base_stat_value);
        int8_t max_bonus = static_cast<int8_t>(m_game->m_max_creation_stat_value);
        int8_t remaining = static_cast<int8_t>(std::clamp(m_game->m_creation_stat_points - max_bonus * 2, 0, (int)max_bonus));
        m_game->m_new_char.stat_str = base + max_bonus;
        m_game->m_new_char.stat_vit = base;
        m_game->m_new_char.stat_dex = base;
        m_game->m_new_char.stat_int = base;
        m_game->m_new_char.stat_mag = base + remaining;
        m_game->m_new_char.stat_chr = base + max_bonus;
        int total = m_game->m_base_stat_value * 6 + m_game->m_creation_stat_points;
        m_iNewCharPoint = total - (m_game->m_new_char.stat_str + m_game->m_new_char.stat_vit
            + m_game->m_new_char.stat_dex + m_game->m_new_char.stat_int
            + m_game->m_new_char.stat_mag + m_game->m_new_char.stat_chr);
        m_selected_class = BTN_MASTER;
    });
    btn_priest->set_render_handler([this](const cc::control& c) {
        auto sb = c.screen_bounds();
        bool highlight = c.is_focused() || m_selected_class == BTN_MASTER;
        m_game->m_sprite[InterfaceNdButton]->draw(sb.x, sb.y, highlight ? 64 : 63);
    });

    // Focus order: textbox + action buttons only (toggles excluded)
    m_controls.set_focus_order({TXT_NAME, BTN_CREATE, BTN_CANCEL, BTN_WARRIOR, BTN_MAGE, BTN_MASTER});
    m_controls.set_focus(TXT_NAME);

    // Discard any pending input from previous screen/overlay
    discard_pending_controls_input(m_controls);
}
void Screen_CreateNewCharacter::on_update()
{
    uint32_t time = GameClock::get_time_ms();

    // Fill input state and update controls
    cc::input_state input;
    hb::client::fill_input_state(input);

    // Handle overlay suppression transition
    bool suppressed = hb::shared::input::is_suppressed();
    bool discard = suppressed || m_was_suppressed;
    m_was_suppressed = suppressed;

    if (discard)
        m_controls.discard_pending_input(input);

    m_controls.update(input, time);

    // Track focus for next frame's double-click detection
    m_prev_focused = m_controls.focused_id();

    // ESC → character select
    if (m_controls.escape_pressed())
    {
        m_game->change_game_mode(GameMode::SelectCharacter);
        return;
    }

    // Compute validation flag
    auto* tb = m_controls.find_as<cc::textbox>(TXT_NAME);
    m_bNewCharFlag = tb && tb->is_valid() && m_iNewCharPoint == 0;

    // Animation loop
    if ((time - m_dwNewCharMTime) > 100)
    {
        m_game->m_menu_frame++;
        m_dwNewCharMTime = time;
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
}

void Screen_CreateNewCharacter::submit_create_character()
{
    auto* tb = m_controls.find_as<cc::textbox>(TXT_NAME);
    if (!tb) return;

    const std::string& name = tb->text();
    if (!CMisc::check_valid_name(name.c_str())) return;

    m_game->m_new_char.player_name = name.c_str();

    // Build create character packet for deferred send after connection establishes
    hb::net::CreateCharacterRequest req{};
    req.header.msg_id = MsgId::RequestCreateNewCharacter;
    req.header.msg_type = 0;
    std::snprintf(req.character_name, sizeof(req.character_name), "%s", m_game->m_new_char.player_name.c_str());
    std::snprintf(req.account_name, sizeof(req.account_name), "%s", m_game->m_account_name.c_str());
    std::snprintf(req.password, sizeof(req.password), "%s", m_game->m_account_password.c_str());
    std::snprintf(req.world_name, sizeof(req.world_name), "%s", m_game->m_world_server_name.c_str());
    req.gender = static_cast<uint8_t>(m_game->m_new_char.gender);
    req.skin = static_cast<uint8_t>(m_game->m_new_char.skin_col);
    req.hairstyle = static_cast<uint8_t>(m_game->m_new_char.hair_style);
    req.haircolor = static_cast<uint8_t>(m_game->m_new_char.hair_col);
    req.underware = static_cast<uint8_t>(m_game->m_new_char.under_col);
    req.str = static_cast<uint8_t>(m_game->m_new_char.stat_str);
    req.vit = static_cast<uint8_t>(m_game->m_new_char.stat_vit);
    req.dex = static_cast<uint8_t>(m_game->m_new_char.stat_dex);
    req.intl = static_cast<uint8_t>(m_game->m_new_char.stat_int);
    req.mag = static_cast<uint8_t>(m_game->m_new_char.stat_mag);
    req.chr = static_cast<uint8_t>(m_game->m_new_char.stat_chr);

    // Map UI button to protocol class_type; default to warrior if none selected
    using namespace hb::shared::character_class;
    uint8_t cls = warrior;
    if (m_selected_class == BTN_MAGE) cls = mage;
    else if (m_selected_class == BTN_MASTER) cls = master;
    req.class_type = cls;

    m_game->set_pending_login_packet(req);

    m_game->m_l_sock = std::make_unique<hb::shared::net::ASIOSocket>(m_game->m_io_pool->get_context(), game_limits::socket_block_limit);
    m_game->m_l_sock->connect(m_game->m_log_server_addr.c_str(), m_game->m_log_server_port);
    m_game->m_l_sock->init_buffer_size(hb::shared::limits::MsgBufferSize);
    m_game->change_game_mode(GameMode::Connecting);
    std::snprintf(m_game->m_msg, sizeof(m_game->m_msg), "%s", "22");
}

bool Screen_CreateNewCharacter::on_net_response(uint16_t response_type, char* data)
{
    switch (response_type) {
    case LogResMsg::NewCharacterCreated:
    {
        const auto* list = hb::net::PacketCast<hb::net::PacketLogNewCharacterCreatedHeader>(
            data, sizeof(hb::net::PacketLogNewCharacterCreatedHeader));
        if (!list) return true;

        m_game->m_total_char = std::min(static_cast<int>(list->total_chars), 4);
        for (int i = 0; i < 4; i++)
            if (m_game->m_char_list[i] != 0) m_game->m_char_list[i].reset();

        const auto* entries = reinterpret_cast<const hb::net::PacketLogCharacterEntry*>(
            data + sizeof(hb::net::PacketLogNewCharacterCreatedHeader));
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
        std::snprintf(m_game->m_msg, sizeof(m_game->m_msg), "%s", "47");
        m_game->change_game_mode(GameMode::LogResMsg);
        return true;
    }

    case LogResMsg::NewCharacterFailed:
        std::snprintf(m_game->m_msg, sizeof(m_game->m_msg), "%s", "28");
        m_game->change_game_mode(GameMode::LogResMsg);
        return true;

    case LogResMsg::AlreadyExistingCharacter:
        std::snprintf(m_game->m_msg, sizeof(m_game->m_msg), "%s", "29");
        m_game->change_game_mode(GameMode::LogResMsg);
        return true;
    }
    return false;
}

void Screen_CreateNewCharacter::on_render()
{
    uint32_t time = GameClock::get_time_ms();

    // === Background ===
    m_game->draw_new_dialog_box(InterfaceNdNewChar, 0, 0, 0, true);
    m_game->draw_new_dialog_box(InterfaceNdButton, OX, OY, 69, true);

    // === Static text labels ===
    hb::shared::text::draw_text_aligned(GameFont::Default, 64 + OX, 90 + OY, (282) - (64), 15, _BDRAW_ON_CREATE_NEW_CHARACTER1, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
    hb::shared::text::draw_text_aligned(GameFont::Default, 57 + OX, 110 + OY, (191) - (57), 15, DEF_MSG_CHARACTERNAME, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
    hb::shared::text::draw_text_aligned(GameFont::Default, 64 + OX, 140 + OY, (282) - (64), 15, _BDRAW_ON_CREATE_NEW_CHARACTER2, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
    hb::shared::text::draw_text(GameFont::Default, 100 + OX, 160 + OY, DEF_MSG_GENDER, hb::shared::text::TextStyle::from_color(GameColors::UIBlack));
    hb::shared::text::draw_text(GameFont::Default, 100 + OX, 175 + OY, DEF_MSG_SKINCOLOR, hb::shared::text::TextStyle::from_color(GameColors::UIBlack));
    hb::shared::text::draw_text(GameFont::Default, 100 + OX, 190 + OY, DEF_MSG_HAIRSTYLE, hb::shared::text::TextStyle::from_color(GameColors::UIBlack));
    hb::shared::text::draw_text(GameFont::Default, 100 + OX, 205 + OY, DEF_MSG_HAIRCOLOR, hb::shared::text::TextStyle::from_color(GameColors::UIBlack));
    hb::shared::text::draw_text(GameFont::Default, 100 + OX, 220 + OY, DEF_MSG_UNDERWEARCOLOR, hb::shared::text::TextStyle::from_color(GameColors::UIBlack));
    hb::shared::text::draw_text(GameFont::Default, 100 + OX, 275 + OY, DEF_MSG_STRENGTH, hb::shared::text::TextStyle::from_color(GameColors::UIBlack));
    hb::shared::text::draw_text(GameFont::Default, 100 + OX, 292 + OY, DEF_MSG_VITALITY, hb::shared::text::TextStyle::from_color(GameColors::UIBlack));
    hb::shared::text::draw_text(GameFont::Default, 100 + OX, 309 + OY, DEF_MSG_DEXTERITY, hb::shared::text::TextStyle::from_color(GameColors::UIBlack));
    hb::shared::text::draw_text(GameFont::Default, 100 + OX, 326 + OY, DEF_MSG_INTELLIGENCE, hb::shared::text::TextStyle::from_color(GameColors::UIBlack));
    hb::shared::text::draw_text(GameFont::Default, 100 + OX, 343 + OY, DEF_MSG_MAGIC, hb::shared::text::TextStyle::from_color(GameColors::UIBlack));
    hb::shared::text::draw_text(GameFont::Default, 100 + OX, 360 + OY, DEF_MSG_CHARISMA, hb::shared::text::TextStyle::from_color(GameColors::UIBlack));

    // === Stat values ===
    int row = 0;
    std::string txt;
    txt = std::format("{}", m_game->m_new_char.stat_str);
    hb::shared::text::draw_text(GameFont::Default, 204 + OX, 277 + OY + 16 * row++, txt.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UILabel));
    txt = std::format("{}", m_game->m_new_char.stat_vit);
    hb::shared::text::draw_text(GameFont::Default, 204 + OX, 277 + OY + 16 * row++, txt.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UILabel));
    txt = std::format("{}", m_game->m_new_char.stat_dex);
    hb::shared::text::draw_text(GameFont::Default, 204 + OX, 277 + OY + 16 * row++, txt.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UILabel));
    txt = std::format("{}", m_game->m_new_char.stat_int);
    hb::shared::text::draw_text(GameFont::Default, 204 + OX, 277 + OY + 16 * row++, txt.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UILabel));
    txt = std::format("{}", m_game->m_new_char.stat_mag);
    hb::shared::text::draw_text(GameFont::Default, 204 + OX, 277 + OY + 16 * row++, txt.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UILabel));
    txt = std::format("{}", m_game->m_new_char.stat_chr);
    hb::shared::text::draw_text(GameFont::Default, 204 + OX, 277 + OY + 16 * row++, txt.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UILabel));

    // === Controls (textbox text/cursor + action button highlights) ===
    m_controls.render();

    // === Character preview ===
    switch (m_game->m_new_char.gender) {
    case 1: m_game->m_entity_state.m_owner_type = hb::shared::owner::MaleFirst; break;
    case 2: m_game->m_entity_state.m_owner_type = hb::shared::owner::FemaleFirst; break;
    }
    m_game->m_entity_state.m_owner_type += m_game->m_new_char.skin_col - 1;
    m_game->m_entity_state.m_dir = m_game->m_menu_dir;
    m_game->m_entity_state.m_appearance.clear();
    m_game->m_entity_state.m_appearance.underwear_type = m_game->m_new_char.under_col;
    m_game->m_entity_state.m_appearance.hair_style = m_game->m_new_char.hair_style;
    m_game->m_entity_state.m_appearance.hair_color = m_game->m_new_char.hair_col;
    m_game->m_entity_state.m_name.fill('\0');
    std::snprintf(m_game->m_entity_state.m_name.data(), m_game->m_entity_state.m_name.size(), "%s", m_game->m_new_char.player_name.c_str());
    m_game->m_entity_state.m_action = Type::Move;
    m_game->m_entity_state.m_frame = m_game->m_menu_frame;

    Screen_OnGame::draw_character_body(*m_game, 507 + OX, 267 + OY, m_game->m_entity_state.m_owner_type);
    m_game->draw_object_on_move_for_menu(0, 0, 500 + OX, 174 + OY, false, time, false);

    // === Derived stats ===
    row = 0;
    auto& fe = m_game->m_formula_engine;
    int preview_hp = 0, preview_mp = 0, preview_sp = 0;
    if (fe.has_formula("max_hp"))
    {
        preview_hp = hb::shared::calc::max_hp(fe,
            hb::shared::calc::vit{(double)m_game->m_new_char.stat_vit}, hb::shared::calc::level{1.0},
            hb::shared::calc::str{(double)m_game->m_new_char.stat_str}, hb::shared::calc::angelic_str{0.0});
        preview_mp = hb::shared::calc::max_mp(fe,
            hb::shared::calc::mag{(double)m_game->m_new_char.stat_mag}, hb::shared::calc::angelic_mag{0.0},
            hb::shared::calc::level{1.0}, hb::shared::calc::intel{(double)m_game->m_new_char.stat_int},
            hb::shared::calc::angelic_int{0.0});
        preview_sp = hb::shared::calc::max_sp(fe,
            hb::shared::calc::str{(double)m_game->m_new_char.stat_str}, hb::shared::calc::angelic_str{0.0},
            hb::shared::calc::level{1.0});
    }
    hb::shared::text::draw_text(GameFont::Default, 445 + OX, 192 + OY, DEF_MSG_HITPOINT, hb::shared::text::TextStyle::from_color(GameColors::UIBlack));
    txt = std::format("{}", preview_hp);
    hb::shared::text::draw_text(GameFont::Default, 550 + OX, 192 + OY + 16 * row++, txt.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UILabel));
    hb::shared::text::draw_text(GameFont::Default, 445 + OX, 208 + OY, DEF_MSG_MANAPOINT, hb::shared::text::TextStyle::from_color(GameColors::UIBlack));
    txt = std::format("{}", preview_mp);
    hb::shared::text::draw_text(GameFont::Default, 550 + OX, 192 + OY + 16 * row++, txt.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UILabel));
    hb::shared::text::draw_text(GameFont::Default, 445 + OX, 224 + OY, DEF_MSG_STAMINAPOINT, hb::shared::text::TextStyle::from_color(GameColors::UIBlack));
    txt = std::format("{}", preview_sp);
    hb::shared::text::draw_text(GameFont::Default, 550 + OX, 192 + OY + 16 * row++, txt.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UILabel));

    m_game->draw_version();

    // === Tooltips ===
    render_tooltips();

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

void Screen_CreateNewCharacter::render_tooltips()
{
    short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
    short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
    int i = 0;

    // Name field tooltip zone
    if ((mouse_x >= 65 + 4 - 127 + OX) && (mouse_x <= 275 + 4 + OX) && (mouse_y >= 65 + 45 + OY) && (mouse_y <= 82 + 45 + OY))
    {
        hb::shared::text::draw_text_aligned(GameFont::Default, 370 + OX, 345 + OY, (580) - (370), 15, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER1, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
    }
    // Gender tooltip
    else if ((mouse_x >= 261 + 4 - 212 + OX) && (mouse_x <= 289 + 4 + OX) && (mouse_y >= 111 + 45 + OY) && (mouse_y <= 124 + 45 + OY))
    {
        hb::shared::text::draw_text_aligned(GameFont::Default, 370 + OX, 345 + OY, (580) - (370), 15, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER2, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
    }
    // Skin color tooltip
    else if ((mouse_x >= 261 + 4 - 212 + OX) && (mouse_x <= 289 + 4 + OX) && (mouse_y >= 126 + 45 + OY) && (mouse_y <= 139 + 45 + OY))
    {
        hb::shared::text::draw_text_aligned(GameFont::Default, 370 + OX, 345 + OY, (580) - (370), 15, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER3, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
    }
    // Hair style tooltip
    else if ((mouse_x >= 261 + 4 - 212 + OX) && (mouse_x <= 289 + 4 + OX) && (mouse_y >= 141 + 45 + OY) && (mouse_y <= 154 + 45 + OY))
    {
        hb::shared::text::draw_text_aligned(GameFont::Default, 370 + OX, 345 + OY, (580) - (370), 15, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER4, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
    }
    // Hair color tooltip
    else if ((mouse_x >= 261 + 4 - 212 + OX) && (mouse_x <= 289 + 4 + OX) && (mouse_y >= 156 + 45 + OY) && (mouse_y <= 169 + 45 + OY))
    {
        hb::shared::text::draw_text_aligned(GameFont::Default, 370 + OX, 345 + OY, (580) - (370), 15, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER5, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
    }
    // Underwear color tooltip
    else if ((mouse_x >= 261 + 4 - 212 + OX) && (mouse_x <= 289 + 4 + OX) && (mouse_y >= 171 + 45 + OY) && (mouse_y <= 184 + 45 + OY))
    {
        hb::shared::text::draw_text_aligned(GameFont::Default, 370 + OX, 345 + OY, (580) - (370), 15, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER6, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
    }
    // Str tooltip
    else if ((mouse_x >= 240 + 4 - 175 + OX) && (mouse_x <= 268 + 4 + OX) && (mouse_y >= 231 + 45 + OY) && (mouse_y <= 244 + 45 + OY))
    {
        i = 0;
        hb::shared::text::draw_text_aligned(GameFont::Default, 370 + OX, 345 + OY + 16 * i++, (580) - (370), 15, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER7, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
        hb::shared::text::draw_text_aligned(GameFont::Default, 370 + OX, 345 + OY + 16 * i++, (580) - (370), 15, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER8, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
        hb::shared::text::draw_text_aligned(GameFont::Default, 370 + OX, 345 + OY + 16 * i++, (580) - (370), 15, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER9, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
        hb::shared::text::draw_text_aligned(GameFont::Default, 370 + OX, 345 + OY + 16 * i++, (580) - (370), 15, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER10, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
        hb::shared::text::draw_text_aligned(GameFont::Default, 370 + OX, 345 + OY + 16 * i++, (580) - (370), 15, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER11, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
    }
    // Vit tooltip
    else if ((mouse_x >= 240 + 4 - 175 + OX) && (mouse_x <= 268 + 4 + OX) && (mouse_y >= 246 + 45 + OY) && (mouse_y <= 259 + 45 + OY))
    {
        i = 0;
        hb::shared::text::draw_text_aligned(GameFont::Default, 370 + OX, 345 + OY + 16 * i++, (580) - (370), 15, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER12, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
        hb::shared::text::draw_text_aligned(GameFont::Default, 370 + OX, 345 + OY + 16 * i++, (580) - (370), 15, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER13, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
        hb::shared::text::draw_text_aligned(GameFont::Default, 370 + OX, 345 + OY + 16 * i++, (580) - (370), 15, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER14, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
        hb::shared::text::draw_text_aligned(GameFont::Default, 370 + OX, 345 + OY + 16 * i++, (580) - (370), 15, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER15, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
        hb::shared::text::draw_text_aligned(GameFont::Default, 370 + OX, 345 + OY + 16 * i++, (580) - (370), 15, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER16, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
    }
    // Dex tooltip
    else if ((mouse_x >= 240 + 4 - 175 + OX) && (mouse_x <= 268 + 4 + OX) && (mouse_y >= 261 + 45 + OY) && (mouse_y <= 274 + 45 + OY))
    {
        i = 0;
        hb::shared::text::draw_text_aligned(GameFont::Default, 370 + OX, 345 + OY + 16 * i++, (580) - (370), 15, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER17, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
        hb::shared::text::draw_text_aligned(GameFont::Default, 370 + OX, 345 + OY + 16 * i++, (580) - (370), 15, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER18, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
        hb::shared::text::draw_text_aligned(GameFont::Default, 370 + OX, 345 + OY + 16 * i++, (580) - (370), 15, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER19, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
        hb::shared::text::draw_text_aligned(GameFont::Default, 370 + OX, 345 + OY + 16 * i++, (580) - (370), 15, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER20, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
    }
    // Int tooltip
    else if ((mouse_x >= 240 + 4 - 175 + OX) && (mouse_x <= 268 + 4 + OX) && (mouse_y >= 276 + 45 + OY) && (mouse_y <= 289 + 45 + OY))
    {
        i = 0;
        hb::shared::text::draw_text_aligned(GameFont::Default, 370 + OX, 345 + OY + 16 * i++, (580) - (370), 15, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER21, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
        hb::shared::text::draw_text_aligned(GameFont::Default, 370 + OX, 345 + OY + 16 * i++, (580) - (370), 15, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER22, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
        hb::shared::text::draw_text_aligned(GameFont::Default, 370 + OX, 345 + OY + 16 * i++, (580) - (370), 15, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER23, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
        hb::shared::text::draw_text_aligned(GameFont::Default, 370 + OX, 345 + OY + 16 * i++, (580) - (370), 15, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER24, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
    }
    // Mag tooltip
    else if ((mouse_x >= 240 + 4 - 175 + OX) && (mouse_x <= 268 + 4 + OX) && (mouse_y >= 291 + 45 + OY) && (mouse_y <= 304 + 45 + OY))
    {
        i = 0;
        hb::shared::text::draw_text_aligned(GameFont::Default, 370 + OX, 345 + OY + 16 * i++, (580) - (370), 15, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER25, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
        hb::shared::text::draw_text_aligned(GameFont::Default, 370 + OX, 345 + OY + 16 * i++, (580) - (370), 15, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER26, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
        hb::shared::text::draw_text_aligned(GameFont::Default, 370 + OX, 345 + OY + 16 * i++, (580) - (370), 15, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER27, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
        hb::shared::text::draw_text_aligned(GameFont::Default, 370 + OX, 345 + OY + 16 * i++, (580) - (370), 15, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER28, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
    }
    // Chr tooltip
    else if ((mouse_x >= 240 + 4 - 175 + OX) && (mouse_x <= 268 + 4 + OX) && (mouse_y >= 306 + 45 + OY) && (mouse_y <= 319 + 45 + OY))
    {
        i = 0;
        hb::shared::text::draw_text_aligned(GameFont::Default, 370 + OX, 345 + OY + 16 * i++, (580) - (370), 15, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER29, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
        hb::shared::text::draw_text_aligned(GameFont::Default, 370 + OX, 345 + OY + 16 * i++, (580) - (370), 15, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER30, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
        hb::shared::text::draw_text_aligned(GameFont::Default, 370 + OX, 345 + OY + 16 * i++, (580) - (370), 15, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER31, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
        hb::shared::text::draw_text_aligned(GameFont::Default, 370 + OX, 345 + OY + 16 * i++, (580) - (370), 15, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER32, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
    }
    // Create button tooltip
    else if ((mouse_x >= 384 + OX) && (mouse_x <= 384 + 72 + OX) && (mouse_y >= 445 + OY) && (mouse_y <= 445 + 15 + OY))
    {
        auto* tb = m_controls.find_as<cc::textbox>(TXT_NAME);
        const std::string& name = tb ? tb->text() : std::string();

        if (name.empty())
        {
            i = 0;
            hb::shared::text::draw_text_aligned(GameFont::Default, 370 + OX, 345 + OY + 16 * i++, (580) - (370), 15, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER35, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
        }
        else if (m_iNewCharPoint > 0)
        {
            i = 0;
            hb::shared::text::draw_text_aligned(GameFont::Default, 370 + OX, 345 + OY + 16 * i++, (580) - (370), 15, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER36, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
        }
        else if (!CMisc::check_valid_name(name.c_str()))
        {
            i = 0;
            hb::shared::text::draw_text_aligned(GameFont::Default, 370 + OX, 345 + OY + 16 * i++, (580) - (370), 15, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER39, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
            hb::shared::text::draw_text_aligned(GameFont::Default, 370 + OX, 345 + OY + 16 * i++, (580) - (370), 15, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER40, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
            hb::shared::text::draw_text_aligned(GameFont::Default, 370 + OX, 345 + OY + 16 * i++, (580) - (370), 15, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER41, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
        }
        else
        {
            i = 0;
            hb::shared::text::draw_text_aligned(GameFont::Default, 370 + OX, 345 + OY + 16 * i++, (580) - (370), 15, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER44, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
            hb::shared::text::draw_text_aligned(GameFont::Default, 370 + OX, 345 + OY + 16 * i++, (580) - (370), 15, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER45, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
            hb::shared::text::draw_text_aligned(GameFont::Default, 370 + OX, 345 + OY + 16 * i++, (580) - (370), 15, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER46, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
            hb::shared::text::draw_text_aligned(GameFont::Default, 370 + OX, 345 + OY + 16 * i++, (580) - (370), 15, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER47, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
            hb::shared::text::draw_text_aligned(GameFont::Default, 370 + OX, 345 + OY + 16 * i++, (580) - (370), 15, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER48, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
        }
    }
    // Cancel button tooltip
    else if ((mouse_x >= 500 + OX) && (mouse_x <= 500 + 72 + OX) && (mouse_y >= 445 + OY) && (mouse_y <= 445 + 15 + OY))
    {
        hb::shared::text::draw_text_aligned(GameFont::Default, 370 + OX, 345 + OY, (580) - (370), 15, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER49, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
    }
    // Warrior button tooltip
    else if ((mouse_x >= 60 + OX) && (mouse_x <= 60 + 72 + OX) && (mouse_y >= 445 + OY) && (mouse_y <= 445 + 15 + OY))
    {
        hb::shared::text::draw_text_aligned(GameFont::Default, 370 + OX, 345 + OY, (580) - (370), 15, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER50, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
    }
    // Mage button tooltip
    else if ((mouse_x >= 145 + OX) && (mouse_x <= 145 + 72 + OX) && (mouse_y >= 445 + OY) && (mouse_y <= 445 + 15 + OY))
    {
        hb::shared::text::draw_text_aligned(GameFont::Default, 370 + OX, 345 + OY, (580) - (370), 15, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER51, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
    }
    // Priest button tooltip
    else if ((mouse_x >= 230 + OX) && (mouse_x <= 230 + 72 + OX) && (mouse_y >= 445 + OY) && (mouse_y <= 445 + 15 + OY))
    {
        hb::shared::text::draw_text_aligned(GameFont::Default, 370 + OX, 345 + OY, (580) - (370), 15, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER52, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
    }
}
