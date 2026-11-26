// screen_select_character.cpp: Select Character Screen Implementation
//
//////////////////////////////////////////////////////////////////////

#include "screen_select_character.h"
#include "screen_on_game.h"
#include "game.h"
#include "weather_manager.h"
#include "game_mode_manager.h"
#include "input_state_helper.h"
#include "input.h"
#include "global_def.h"
#include "login_version.h"
#include "asio_socket.h"
#include "misc.h"
#include "lan_eng.h"
#include "game_fonts.h"
#include "text_lib_ext.h"
#include "button.h"
#include "renderer.h"
#include "packet/shared_packets.h"
#include "char_info.h"
#include "audio_manager.h"
#include "sprite_factory.h"
#include "log.h"
#include <cmath>
#include <format>
#include <string>

using namespace hb::shared::net;
using namespace hb::shared::action;

namespace
{

// interface/front_end/character_select.png (HH-0108), measured alpha bounds: the 1920x1080 backdrop sits 2 px
// into the sheet; the cards are 229x402, normal above selected, in the strip to its right
constexpr float art_width = 1920.0f;
constexpr float art_height = 1080.0f;
constexpr float backdrop_src_x = 2.0f;
constexpr float backdrop_src_y = 2.0f;
constexpr float card_w = 229.0f;
constexpr float card_h = 402.0f;
constexpr float card_src_x = 1932.0f;
constexpr float card_normal_src_y = 2.0f;
constexpr float card_selected_src_y = 414.0f;

// The four cards in a row with 20 art px gaps, centred on the roped frame (x 960), just under the title plank
// (its bottom at art y 86; the rope's top at 45)
constexpr float card_pitch = card_w + 20.0f;
constexpr float card_row_x = 960.0f - (4.0f * card_w + 3.0f * 20.0f) / 2.0f;
constexpr float card_row_y = 97.0f;

// Card-local art px: text starts 6 px inside each box and is centred on it; the walking preview's feet stand
// where the old preview stood in the old oval (83% down, centred)
constexpr float card_name_x = 23.0f;
constexpr float card_value_x = 79.0f;
constexpr float card_text_w = 190.0f;
constexpr float card_name_cy = 282.0f;   // box centres (boxes y 267-297, 304-330, 336-362)
constexpr float card_level_cy = 317.0f;
constexpr float card_exp_cy = 349.0f;
constexpr float card_box_h = 24.0f;
constexpr float card_feet_x = 112.0f;
constexpr float card_feet_y = 206.0f;

// The original 800x600 screen maps to the art as art = 240 + ui * 1.8. The plank holds the original's three
// recessed boxes and button column, laid out on the HD plank's inset (art x 488-1432, y 578-927) with one margin
// all round: the help and account boxes share their left and right edges, stacked in the original's 99:53 height
// ratio; the button column sits against the inset's right edge. tools/front_end/make_select_backdrop.py bakes the
// boxes from the same numbers, so the two must agree. The footer strip, on the plank's bottom border (the inset's
// ridge at art y 930 to the plank's edge at 994), shares the boxes' edges and is centred in the border.
constexpr float classic_scale = 1.8f;
constexpr float classic_art_x = 240.0f;
constexpr float inset_left = (488.0f - classic_art_x) / classic_scale;
constexpr float inset_top = 578.0f / classic_scale;
constexpr float inset_right = (1433.0f - classic_art_x) / classic_scale;
constexpr float inset_bottom = 928.0f / classic_scale;
// The original inset (005.png, UI x 173-619, y 338-505) to the HD one: the scale of the buttons and plank text
constexpr float inset_kx = (inset_right - inset_left) / (619.0f - 173.0f);
constexpr float inset_ky = (inset_bottom - inset_top) / (505.0f - 338.0f);
constexpr float plank_margin = 7.0f;

// Menu buttons: the original 176x24 faces at the inset's scale, the column against the inset's right edge and
// spread over the boxes' height; each hit rect is the original's margin around its face, scaled the same
constexpr int button_face_w = static_cast<int>(176.0f * inset_kx + 0.5f);
constexpr int button_face_h = static_cast<int>(24.0f * inset_ky + 0.5f);
constexpr float button_x = inset_right - plank_margin - button_face_w;
constexpr cc::rect scaled_hit(int x, int y, int w, int h)
{
	return {static_cast<int>(x * inset_kx - 0.5f), static_cast<int>(y * inset_ky - 0.5f),
		static_cast<int>(w * inset_kx + 0.5f), static_cast<int>(h * inset_ky + 0.5f)};
}

struct ui_box
{
	float left, top, right, bottom;
};
constexpr float box_left = inset_left + plank_margin;
constexpr float box_right = button_x - plank_margin;
constexpr float box_top = inset_top + plank_margin;
constexpr float box_bottom = inset_bottom - plank_margin;
constexpr float help_bottom = box_top + (box_bottom - box_top - plank_margin) * 99.0f / (99.0f + 53.0f);
constexpr ui_box help_box{box_left, box_top, box_right, help_bottom};
constexpr ui_box account_box{box_left, help_bottom + plank_margin, box_right, box_bottom};
constexpr float footer_strip_h = 20.0f * inset_ky;   // the original strip, y 512-531
constexpr float footer_strip_mid = (930.0f + 994.0f) / 2.0f / classic_scale;
constexpr ui_box footer_box{box_left, footer_strip_mid - footer_strip_h / 2.0f, box_right, footer_strip_mid + footer_strip_h / 2.0f};
constexpr float button_pitch = (box_bottom - box_top - button_face_h) / 4.0f;

// Plank text at the inset's scale: the default face is 12 (SFML size); a TextStyle size goes through the
// renderer's GDI adjustment (size - 2), hence the + 2. Lines are the original 15 px apart, scaled.
constexpr int plank_font_size = static_cast<int>(12.0f * inset_ky + 0.5f) + 2;
constexpr float plank_line_h = 15.0f * inset_ky;

}

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

    const std::string art_path = hb::shared::sprite::Sprites::get_sprite_path() + "/interface/front_end/character_select.png";
    m_art.reset(m_game->m_Renderer->create_texture_from_file(art_path.c_str()));
    if (!m_art)
        hb::logger::error("Character select art not loaded: {}", art_path);

    // 4 character slot buttons (in focus order); every control is placed by update_layout
    for (int i = 0; i < 4; ++i)
    {
        int slot_id = SLOT_1 + i;
        auto* btn = m_controls.add<cc::button>(slot_id, cc::rect{0, 0, 1, 1});

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
            if (!m_art)
                return;
            const float src_y = c.is_focused() ? card_selected_src_y : card_normal_src_y;
            m_game->m_Renderer->draw_texture_region(m_art.get(), card_src_x, src_y, card_w, card_h,
                slot_x(i, 0.0f), slot_y(0.0f), card_w * m_fit.scale, card_h * m_fit.scale);
        });
    }

    // 5 menu buttons (NOT in focus order — hover-only highlights)
    auto* btn_enter = m_controls.add<cc::button>(BTN_ENTER, cc::rect{0, 0, 1, 1});
    btn_enter->set_on_click([this](int) { enter_game(); });
    btn_enter->set_click_sound([this] { audio_manager::get().play_game_sound(sound_type::effect, 14, 5); });

    auto* btn_new = m_controls.add<cc::button>(BTN_NEW, cc::rect{0, 0, 1, 1});
    btn_new->set_on_click([this](int) {
        if (m_game->m_total_char < 4)
            m_game->change_game_mode(GameMode::CreateNewCharacter);
    });
    btn_new->set_click_sound([this] { audio_manager::get().play_game_sound(sound_type::effect, 14, 5); });

    auto* btn_delete = m_controls.add<cc::button>(BTN_DELETE, cc::rect{0, 0, 1, 1});
    btn_delete->set_on_click([this](int) {
        int slot_index = m_controls.focused_id() - SLOT_1;
        if (slot_index >= 0 && slot_index < 4 && m_game->m_char_list[slot_index] != nullptr)
        {
            m_game->change_game_mode(GameMode::QueryDeleteCharacter);
            m_game->m_enter_game_type = slot_index + 1;
        }
    });
    btn_delete->set_click_sound([this] { audio_manager::get().play_game_sound(sound_type::effect, 14, 5); });

    auto* btn_change_pw = m_controls.add<cc::button>(BTN_CHANGE_PW, cc::rect{0, 0, 1, 1});
    btn_change_pw->set_on_click([this](int) { m_game->change_game_mode(GameMode::ChangePassword); });
    btn_change_pw->set_click_sound([this] { audio_manager::get().play_game_sound(sound_type::effect, 14, 5); });

    auto* btn_exit = m_controls.add<cc::button>(BTN_EXIT, cc::rect{0, 0, 1, 1});
    btn_exit->set_on_click([this](int) { m_game->change_game_mode(GameMode::MainMenu); });
    btn_exit->set_click_sound([this] { audio_manager::get().play_game_sound(sound_type::effect, 14, 5); });

    update_layout();

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

void Screen_SelectCharacter::on_uninitialize()
{
	m_art.reset();
}

float Screen_SelectCharacter::slot_x(int slot, float card_px) const
{
	return m_fit.x(card_row_x + card_pitch * static_cast<float>(slot) + card_px);
}

float Screen_SelectCharacter::slot_y(float card_px) const
{
	return m_fit.y(card_row_y + card_px);
}

void Screen_SelectCharacter::update_layout()
{
	m_fit = hb::shared::render::cover_fit(m_game->m_Renderer->get_screen_extent(), art_width, art_height);

	// Each slot's control covers its card, edges rounded so the hit area matches the drawn card
	for (int i = 0; i < 4; ++i)
	{
		const int left = static_cast<int>(std::lround(slot_x(i, 0.0f)));
		const int top = static_cast<int>(std::lround(slot_y(0.0f)));
		const int right = static_cast<int>(std::lround(slot_x(i, card_w)));
		const int bottom = static_cast<int>(std::lround(slot_y(card_h)));
		if (cc::control* slot = m_controls.find(SLOT_1 + i))
			slot->set_local_bounds(cc::rect{left, top, right - left, bottom - top});
	}

	// Menu buttons: the original hit rects relative to their faces (Start's is taller), scaled
	static constexpr struct { int id; cc::rect hit; } menu[] = {
		{BTN_ENTER, scaled_hit(-4, -5, 185, 32)},
		{BTN_NEW, scaled_hit(-4, -2, 185, 29)},
		{BTN_DELETE, scaled_hit(-4, -2, 185, 29)},
		{BTN_CHANGE_PW, scaled_hit(-4, -2, 185, 29)},
		{BTN_EXIT, scaled_hit(-4, -2, 185, 29)},
	};
	for (std::size_t k = 0; k < m_button_face.size(); ++k)
	{
		m_button_face[k] = {static_cast<int>(std::lround(classic_x(button_x))),
			static_cast<int>(std::lround(classic_y(box_top + button_pitch * static_cast<float>(k))))};
		if (cc::control* button = m_controls.find(menu[k].id))
		{
			button->set_local_bounds(cc::rect{m_button_face[k].x + menu[k].hit.x, m_button_face[k].y + menu[k].hit.y,
				menu[k].hit.w, menu[k].hit.h});
		}
	}
}

float Screen_SelectCharacter::classic_x(float ui) const
{
	return m_fit.x(classic_art_x + ui * classic_scale);
}

float Screen_SelectCharacter::classic_y(float ui) const
{
	return m_fit.y(ui * classic_scale);
}

void Screen_SelectCharacter::draw_box_lines(float box_x0, float box_y0, float box_x1, float box_y1,
	std::span<const char* const> lines) const
{
	// The block of lines centred in the box both ways, each line centred across it
	const int left = static_cast<int>(std::lround(classic_x(box_x0)));
	const int width = static_cast<int>(std::lround(classic_x(box_x1))) - left;
	const float block_h = plank_line_h * static_cast<float>(lines.size());
	const float top = (box_y0 + box_y1 - block_h) / 2.0f;
	hb::shared::text::TextStyle style = hb::shared::text::TextStyle::from_color(GameColors::UIBlack);
	style.fontSize = plank_font_size;
	for (std::size_t i = 0; i < lines.size(); ++i)
	{
		const int y0 = static_cast<int>(std::lround(classic_y(top + plank_line_h * static_cast<float>(i))));
		const int y1 = static_cast<int>(std::lround(classic_y(top + plank_line_h * static_cast<float>(i + 1))));
		hb::shared::text::draw_text_aligned(GameFont::Default, left, y0, width, y1 - y0, lines[i], style,
			hb::shared::text::Align::Center);
	}
}

void Screen_SelectCharacter::draw_menu_button(std::size_t index, int frame)
{
	m_game->draw_text_button_in(frame, {m_button_face[index].x, m_button_face[index].y, button_face_w, button_face_h});
}

void Screen_SelectCharacter::on_update()
{
    update_layout();

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
    hb::client::fill_login_version(req, m_game->m_build_window_offset);
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
    // Background (m_fit is from this frame's on_update, or on_initialize on the first frame)
    if (m_art)
    {
        m_game->m_Renderer->draw_texture_region(m_art.get(), backdrop_src_x, backdrop_src_y, art_width, art_height,
            m_fit.origin_x, m_fit.origin_y, art_width * m_fit.scale, art_height * m_fit.scale);
    }

    // Slot cards (via render handlers)
    m_controls.render();

    // Character previews
    render_character_previews();

    // Menu button base sprites
    draw_menu_button(0, 51);
    draw_menu_button(1, 52);
    draw_menu_button(2, 53);
    draw_menu_button(3, 54);
    draw_menu_button(4, 55);

    // Menu button hover highlights
    auto* btn_enter = m_controls.find_as<cc::button>(BTN_ENTER);
    auto* btn_new = m_controls.find_as<cc::button>(BTN_NEW);
    auto* btn_delete = m_controls.find_as<cc::button>(BTN_DELETE);
    auto* btn_change_pw = m_controls.find_as<cc::button>(BTN_CHANGE_PW);
    auto* btn_exit = m_controls.find_as<cc::button>(BTN_EXIT);

    if (btn_enter && btn_enter->is_hovered())
        draw_menu_button(0, 56);
    else if (btn_new && btn_new->is_hovered())
        draw_menu_button(1, 57);
    else if (btn_delete && btn_delete->is_hovered())
        draw_menu_button(2, 58);
    else if (btn_change_pw && btn_change_pw->is_hovered())
        draw_menu_button(3, 59);
    else if (btn_exit && btn_exit->is_hovered())
        draw_menu_button(4, 60);

    // Tooltip text
    render_tooltip_text();

    // Account info
    render_account_info();

    // Footer text
    static constexpr const char* footer[] = {UPDATE_SCREEN_ON_SELECT_CHARACTER36};
    draw_box_lines(footer_box.left, footer_box.top, footer_box.right, footer_box.bottom, footer);

    m_game->draw_version();
    draw_copyright();

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
    uint32_t time = GameClock::get_time_ms();
    const auto px = [](float v) { return static_cast<int>(std::lround(v)); };
    const int text_w = px(card_text_w * m_fit.scale);
    const int text_h = px(card_box_h * m_fit.scale);
    const auto card_text = [&](int slot, float x, float cy, const char* text) {
        hb::shared::text::draw_text_aligned(GameFont::Default, px(slot_x(slot, x)), px(slot_y(cy) - text_h / 2.0f),
            text_w, text_h, text, hb::shared::text::TextStyle::from_color(GameColors::UISelectPurple),
            hb::shared::text::Align::MiddleLeft);
    };

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
                    const int feet_x = px(slot_x(i, card_feet_x));
                    const int feet_y = px(slot_y(card_feet_y));
                    m_game->m_effect_sprites[0]->draw(feet_x, feet_y, 1, hb::shared::sprite::DrawParams::additive_no_color_key(0.25f));
                    m_game->draw_object_on_move_for_menu(0, 0, feet_x, feet_y, false, time);
                    card_text(i, card_name_x, card_name_cy, m_game->m_char_list[i]->m_name.c_str());
                    card_text(i, card_value_x, card_level_cy, std::format("{}", m_game->m_char_list[i]->m_level).c_str());
                    card_text(i, card_value_x, card_exp_cy, m_game->format_comma_number(m_game->m_char_list[i]->m_exp).c_str());
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

    static constexpr const char* enter_help[] = {UPDATE_SCREEN_ON_SELECT_CHARACTER1, UPDATE_SCREEN_ON_SELECT_CHARACTER2,
        UPDATE_SCREEN_ON_SELECT_CHARACTER3, UPDATE_SCREEN_ON_SELECT_CHARACTER4};
    static constexpr const char* new_help[] = {UPDATE_SCREEN_ON_SELECT_CHARACTER5};
    static constexpr const char* delete_help[] = {UPDATE_SCREEN_ON_SELECT_CHARACTER6, UPDATE_SCREEN_ON_SELECT_CHARACTER7};
    static constexpr const char* change_pw_help[] = {UPDATE_SCREEN_ON_SELECT_CHARACTER12};
    static constexpr const char* exit_help[] = {UPDATE_SCREEN_ON_SELECT_CHARACTER13};
    static constexpr const char* no_chars_help[] = {UPDATE_SCREEN_ON_SELECT_CHARACTER14, UPDATE_SCREEN_ON_SELECT_CHARACTER15,
        UPDATE_SCREEN_ON_SELECT_CHARACTER16, UPDATE_SCREEN_ON_SELECT_CHARACTER17, UPDATE_SCREEN_ON_SELECT_CHARACTER18};
    static constexpr const char* some_chars_help[] = {UPDATE_SCREEN_ON_SELECT_CHARACTER19, UPDATE_SCREEN_ON_SELECT_CHARACTER20,
        UPDATE_SCREEN_ON_SELECT_CHARACTER21, UPDATE_SCREEN_ON_SELECT_CHARACTER22, UPDATE_SCREEN_ON_SELECT_CHARACTER23,
        UPDATE_SCREEN_ON_SELECT_CHARACTER24};
    static constexpr const char* full_help[] = {UPDATE_SCREEN_ON_SELECT_CHARACTER25, UPDATE_SCREEN_ON_SELECT_CHARACTER26,
        UPDATE_SCREEN_ON_SELECT_CHARACTER27, UPDATE_SCREEN_ON_SELECT_CHARACTER28};

    std::span<const char* const> lines;
    if (btn_enter && btn_enter->is_hovered())
        lines = enter_help;
    else if (btn_new && btn_new->is_hovered())
        lines = new_help;
    else if (btn_delete && btn_delete->is_hovered())
        lines = delete_help;
    else if (btn_change_pw && btn_change_pw->is_hovered())
        lines = change_pw_help;
    else if (btn_exit && btn_exit->is_hovered())
        lines = exit_help;
    else
    {
        // Default tooltip based on total character count
        int total_char = 0;
        for (int i = 0; i < 4; i++)
        {
            if (m_game->m_char_list[i] != nullptr)
                total_char++;
        }
        lines = total_char == 0 ? std::span<const char* const>(no_chars_help)
            : total_char < 4 ? std::span<const char* const>(some_chars_help) : std::span<const char* const>(full_help);
    }
    draw_box_lines(help_box.left, help_box.top, help_box.right, help_box.bottom, lines);
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
    // Up to three lines (account time, IP time, last login), the blank ones left out so the rest centre
    std::array<std::string, 3> texts;
    std::array<const char*, 3> lines{};
    std::size_t count = 0;
    const auto add_line = [&](std::string text) {
        if (text.find_first_not_of(' ') == std::string::npos)
            return;
        texts[count] = std::move(text);
        lines[count] = texts[count].c_str();
        ++count;
    };
    add_line(infoBuf);

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
    add_line(infoBuf);

    if (year != 0)
        add_line(std::format(UPDATE_SCREEN_ON_SELECT_CHARACTER43, year, month, day, hour, minute));

    draw_box_lines(account_box.left, account_box.top, account_box.right, account_box.bottom,
        std::span<const char* const>(lines.data(), count));
}
