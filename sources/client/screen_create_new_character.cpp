// screen_create_new_character.cpp: Create New Character Screen Implementation
//
//////////////////////////////////////////////////////////////////////

#include "screen_create_new_character.h"
#include "game.h"
#include "shared_calculations.h"
#include "screen_on_game.h"
#include "game_mode_manager.h"
#include "input_state_helper.h"
#include "text_field_renderer.h"
#include "global_def.h"
#include "asio_socket.h"
#include "misc.h"
#include "lan_eng.h"
#include "game_fonts.h"
#include "text_lib_ext.h"
#include "input.h"
#include "button.h"
#include "panel.h"
#include "textbox.h"
#include "renderer.h"
#include "packet/shared_packets.h"
#include "character_class.h"
#include "char_info.h"
#include "audio_manager.h"
#include "sprite_factory.h"
#include "log.h"
#include <algorithm>
#include <cmath>
#include <format>
#include <memory>
#include <string>

using namespace hb::shared::net;
using namespace hb::shared::action;

namespace
{

// interface/front_end/character_create.png (HH-0122), measured alpha bounds: the 1920x1080 backdrop at sheet
// (0,0); the eight 48x33 strip buttons in two columns to its right, rows arrows, lit arrows, +/-, lit +/-
constexpr float art_width = 1920.0f;
constexpr float art_height = 1080.0f;
constexpr float strip_w = 48.0f;
constexpr float strip_h = 33.0f;
constexpr float strip_left_x = 1922.0f;
constexpr float strip_right_x = 1973.0f;
constexpr float strip_arrow_y = 2.0f;
constexpr float strip_arrow_lit_y = 38.0f;
constexpr float strip_plus_minus_y = 75.0f;
constexpr float strip_plus_minus_lit_y = 112.0f;

// The original screen's elements keep their places: each old panel face (UI px, measured: left x 138.1-383.8,
// y 141.3-503.8; top-right x 451.9-666.3, y 141.3-310.0; bottom-right y 373.8-503.8) is mapped onto its HD face
// (art px, measured: left x 403-929, y 173-940; top-right x 1071-1523, y 173-531; bottom-right y 665-941), and the
// buttons keep their place on the bottom borders (old y 503.8-531.9, HD 941-1000). The numbers below are that
// mapping in art px; tools/front_end/make_create_backdrop.py bakes the value boxes from the same numbers, so the
// two must agree.

// Rows 0-4 appearance, 5-10 stats: centres on the old 16 px toggle pitch (the labels now sit on their rows)
constexpr float row_cy[11] = {349.15f, 383.00f, 416.85f, 450.71f, 484.56f,
	600.93f, 634.79f, 668.64f, 702.49f, 736.35f, 770.20f};
constexpr float row_label_x = 492.67f;
constexpr float row_buttons_x = 782.0f;   // the pair of strip buttons, drawn at their native 48x33
constexpr float stat_box_x0 = 691.0f;
constexpr float stat_box_x1 = 769.0f;
constexpr float box_h = 30.0f;

// Name: the prompt and label boxes are the original's text boxes; the textbox is the recess, text inset 10 px
constexpr float prompt_x0 = 415.39f;
constexpr float prompt_x1 = 882.24f;
constexpr float prompt_name_cy = 207.38f;
constexpr float prompt_appearance_cy = 313.18f;
constexpr float name_label_x0 = 400.25f;
constexpr float name_label_x1 = 687.20f;
constexpr float name_label_cy = 249.70f;
constexpr float name_box_x0 = 690.0f;
constexpr float name_box_y0 = 227.0f;
constexpr float name_box_x1 = 921.0f;
constexpr float name_box_y1 = 268.0f;
constexpr float name_text_pad = 10.0f;
constexpr float points_left_cy = 542.43f;   // new: midway between the appearance and stat blocks

// Top-right: the previews' pivots, HP/MP/SP rows
constexpr float body_pivot_x = 1355.90f;
constexpr float body_pivot_y = 567.07f;
constexpr float walk_pivot_x = 1341.15f;
constexpr float walk_pivot_y = 369.77f;
constexpr float vital_label_x = 1225.18f;
constexpr float vital_box_x0 = 1423.0f;
constexpr float vital_box_x1 = 1502.0f;
constexpr float vital_cy[3] = {423.87f, 457.81f, 491.75f};

// Bottom-right: help lines centred on the face, the original's 16 px pitch mapped
constexpr float help_x0 = 1071.0f;
constexpr float help_x1 = 1523.0f;
constexpr float help_cy0 = 747.27f;
constexpr float help_pitch = 33.97f;

// Kit buttons on the bottom borders
constexpr float button_y0 = 946.24f;
constexpr float button_y1 = 981.64f;
struct art_span
{
	float x0, x1;
};
constexpr art_span warrior_x{412.37f, 564.95f};
constexpr art_span magician_x{591.72f, 744.30f};
constexpr art_span master_x{772.40f, 924.98f};
constexpr art_span create_x{1098.67f, 1248.90f};
constexpr art_span cancel_x{1343.78f, 1494.01f};

// Text at the panels' scale (about 2.12 art px per old UI px, 1.8 per UI px now): the default face is 12 (SFML
// size); a TextStyle size goes through the renderer's GDI adjustment (size - 2), hence the + 2. The help text is
// 11 (about 20 art px, accepted in the mockup) so its widest line fits the bottom-right face.
constexpr float panel_scale = 2.116f / 1.8f;
constexpr int label_font_size = static_cast<int>(12.0f * panel_scale + 0.5f) + 2;
constexpr int help_font_size = 11 + 2;

int px(float v)
{
	return static_cast<int>(std::lround(v));
}

void wrap_step(int8_t& value, int delta, int lo, int hi)
{
	int v = value + delta;
	if (v < lo) v = hi;
	if (v > hi) v = lo;
	value = static_cast<int8_t>(v);
}

}

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

    const std::string art_path = hb::shared::sprite::Sprites::get_sprite_path() + "/interface/front_end/character_create.png";
    m_art.reset(m_game->m_Renderer->create_texture_from_file(art_path.c_str()));
    if (!m_art)
        hb::logger::error("Character create art not loaded: {}", art_path);

    // === CControls setup; every control is placed by update_layout ===
    m_controls.set_screen_size(LOGICAL_WIDTH(), LOGICAL_HEIGHT());
    m_controls.set_hover_focus(true);
    m_controls.set_enter_advances_focus(false);

    m_controls.set_clipboard_provider(
        [] { return hb::shared::input::get_clipboard_text(); },
        [](const std::string& s) { hb::shared::input::set_clipboard_text(s); }
    );

    const auto click_sound = [this] { audio_manager::get().play_game_sound(sound_type::effect, 14, 5); };

    // === Name textbox ===
    auto* tb = m_controls.add<cc::textbox>(TXT_NAME, cc::rect{0, 0, 1, 1}, 11);
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
    m_controls.add<cc::panel>(ZONE_NAME, cc::rect{0, 0, 1, 1});

    // === Row buttons (NOT in focus order): appearance left = previous, right = next; stats left = plus,
    // right = minus (the original's order) ===
    auto& nc = m_game->m_new_char;
    const auto change_row = [this, &nc](int row, int delta) {
        switch (row)
        {
        case 0: wrap_step(nc.gender, delta, 1, 2); return;
        case 1: wrap_step(nc.skin_col, delta, 1, 3); return;
        case 2: wrap_step(nc.hair_style, delta, 0, 7); return;
        case 3: wrap_step(nc.hair_col, delta, 32, 47); return;   // unified palette hair range
        case 4: wrap_step(nc.under_col, delta, 0, 7); return;
        default: break;
        }
        int8_t* const stats[] = {&nc.stat_str, &nc.stat_vit, &nc.stat_dex, &nc.stat_int, &nc.stat_mag, &nc.stat_chr};
        int8_t& stat = *stats[row - appearance_rows];
        const int8_t max_stat = static_cast<int8_t>(m_game->m_base_stat_value + m_game->m_max_creation_stat_value);
        const int8_t min_stat = static_cast<int8_t>(m_game->m_base_stat_value);
        if (delta > 0)
        {
            if (m_iNewCharPoint > 0 && stat < max_stat) { stat++; m_iNewCharPoint--; }
        }
        else
        {
            if (stat > min_stat) { stat--; m_iNewCharPoint++; }
        }
    };

    for (int row = 0; row < row_count; ++row)
    {
        const bool stat_row = row >= appearance_rows;
        const float lit_y = stat_row ? strip_plus_minus_lit_y : strip_arrow_lit_y;
        const float dark_y = stat_row ? strip_plus_minus_y : strip_arrow_y;
        for (int side = 0; side < 2; ++side)
        {
            const int delta = (side == 0) == stat_row ? 1 : -1;
            auto* btn = m_controls.add<cc::button>((side == 0 ? BTN_ROW_LEFT : BTN_ROW_RIGHT) + row, cc::rect{0, 0, 1, 1});
            btn->set_click_sound(click_sound);
            btn->set_on_click([change_row, row, delta](int) { change_row(row, delta); });
            const float src_x = side == 0 ? strip_left_x : strip_right_x;
            btn->set_render_handler([this, src_x, lit_y, dark_y](const cc::control& c) {
                if (!m_art)
                    return;
                const auto sb = c.screen_bounds();
                const float src_y = (c.is_hovered() || c.is_held()) ? lit_y : dark_y;
                m_game->m_Renderer->draw_texture_region(m_art.get(), src_x, src_y, strip_w, strip_h,
                    static_cast<float>(sb.x), static_cast<float>(sb.y), strip_w * m_fit.scale, strip_h * m_fit.scale);
            });
        }
        m_controls.add<cc::panel>(ZONE_ROW + row, cc::rect{0, 0, 1, 1});
    }

    // === Action buttons: kit buttons with the old frames' labels ===
    const auto add_action = [this, &click_sound](int id, auto on_click, auto frame_of) {
        auto* btn = m_controls.add<cc::button>(id, cc::rect{0, 0, 1, 1});
        btn->set_click_sound(click_sound);
        btn->set_on_click([this, id, on_click](int) {
            if (m_prev_focused != id) return;
            on_click();
        });
        btn->set_render_handler([this, frame_of](const cc::control& c) {
            const auto sb = c.screen_bounds();
            m_game->draw_text_button_in(frame_of(c), {sb.x, sb.y, sb.w, sb.h});
        });
    };
    add_action(BTN_CREATE, [this] { if (m_bNewCharFlag) submit_create_character(); },
        [this](const cc::control& c) { return (c.is_focused() && m_bNewCharFlag) ? 25 : 24; });
    add_action(BTN_CANCEL, [this] { m_game->change_game_mode(GameMode::SelectCharacter); },
        [](const cc::control& c) { return c.is_focused() ? 17 : 16; });
    add_action(BTN_WARRIOR, [this] { apply_class_preset(BTN_WARRIOR); },
        [this](const cc::control& c) { return (c.is_focused() || m_selected_class == BTN_WARRIOR) ? 68 : 67; });
    add_action(BTN_MAGE, [this] { apply_class_preset(BTN_MAGE); },
        [this](const cc::control& c) { return (c.is_focused() || m_selected_class == BTN_MAGE) ? 66 : 65; });
    add_action(BTN_MASTER, [this] { apply_class_preset(BTN_MASTER); },
        [this](const cc::control& c) { return (c.is_focused() || m_selected_class == BTN_MASTER) ? 64 : 63; });

    update_layout();

    // Focus order: textbox + action buttons only (row buttons excluded)
    m_controls.set_focus_order({TXT_NAME, BTN_CREATE, BTN_CANCEL, BTN_WARRIOR, BTN_MAGE, BTN_MASTER});
    m_controls.set_focus(TXT_NAME);

    // Discard any pending input from previous screen/overlay
    discard_pending_controls_input(m_controls);
}

void Screen_CreateNewCharacter::on_uninitialize()
{
    m_art.reset();
}

void Screen_CreateNewCharacter::apply_class_preset(int class_button)
{
    // Two stats at the bonus cap and one with what is left; the rest at base
    const int8_t base = static_cast<int8_t>(m_game->m_base_stat_value);
    const int8_t max_bonus = static_cast<int8_t>(m_game->m_max_creation_stat_value);
    const int8_t remaining = static_cast<int8_t>(std::clamp(m_game->m_creation_stat_points - max_bonus * 2, 0, (int)max_bonus));
    auto& nc = m_game->m_new_char;
    nc.stat_str = nc.stat_vit = nc.stat_dex = nc.stat_int = nc.stat_mag = nc.stat_chr = base;
    switch (class_button)
    {
    case BTN_WARRIOR:
        nc.stat_str = base + max_bonus;
        nc.stat_vit = base + remaining;
        nc.stat_dex = base + max_bonus;
        break;
    case BTN_MAGE:
        nc.stat_vit = base + remaining;
        nc.stat_int = base + max_bonus;
        nc.stat_mag = base + max_bonus;
        break;
    case BTN_MASTER:
        nc.stat_str = base + max_bonus;
        nc.stat_mag = base + remaining;
        nc.stat_chr = base + max_bonus;
        break;
    default:
        return;
    }
    const int total = m_game->m_base_stat_value * 6 + m_game->m_creation_stat_points;
    m_iNewCharPoint = total - (nc.stat_str + nc.stat_vit + nc.stat_dex + nc.stat_int + nc.stat_mag + nc.stat_chr);
    m_selected_class = class_button;
}

void Screen_CreateNewCharacter::update_layout()
{
    m_fit = hb::shared::render::cover_fit(m_game->m_Renderer->get_screen_extent(), art_width, art_height);

    const auto place = [this](int id, float x0, float y0, float x1, float y1) {
        const int left = px(m_fit.x(x0));
        const int top = px(m_fit.y(y0));
        if (cc::control* c = m_controls.find(id))
            c->set_local_bounds(cc::rect{left, top, px(m_fit.x(x1)) - left, px(m_fit.y(y1)) - top});
    };

    // The textbox's bounds are its text line (draw_text_field draws at the bounds' top-left in the default face),
    // centred in the recess; the name zone covers the label and the recess
    const float line_h = static_cast<float>(hb::shared::text::get_line_height(GameFont::Default)) / m_fit.scale;
    const float name_cy = (name_box_y0 + name_box_y1) / 2.0f;
    place(TXT_NAME, name_box_x0 + name_text_pad, name_cy - line_h / 2.0f, name_box_x1 - name_text_pad, name_cy + line_h / 2.0f);
    place(ZONE_NAME, name_label_x0, name_box_y0, name_box_x1, name_box_y1);

    for (int row = 0; row < row_count; ++row)
    {
        const float y0 = row_cy[row] - strip_h / 2.0f;
        const float y1 = y0 + strip_h;
        place(BTN_ROW_LEFT + row, row_buttons_x, y0, row_buttons_x + strip_w, y1);
        place(BTN_ROW_RIGHT + row, row_buttons_x + strip_w, y0, row_buttons_x + 2.0f * strip_w, y1);
        place(ZONE_ROW + row, row_label_x, y0, row_buttons_x + 2.0f * strip_w, y1);
    }

    place(BTN_WARRIOR, warrior_x.x0, button_y0, warrior_x.x1, button_y1);
    place(BTN_MAGE, magician_x.x0, button_y0, magician_x.x1, button_y1);
    place(BTN_MASTER, master_x.x0, button_y0, master_x.x1, button_y1);
    place(BTN_CREATE, create_x.x0, button_y0, create_x.x1, button_y1);
    place(BTN_CANCEL, cancel_x.x0, button_y0, cancel_x.x1, button_y1);
}

void Screen_CreateNewCharacter::draw_art_text(float x0, float y0, float x1, float y1, const char* text,
    const hb::shared::render::Color& color, int font_size, hb::shared::text::Align align) const
{
    hb::shared::text::TextStyle style = hb::shared::text::TextStyle::from_color(color);
    style.fontSize = font_size;
    const int left = px(m_fit.x(x0));
    const int top = px(m_fit.y(y0));
    hb::shared::text::draw_text_aligned(GameFont::Default, left, top, px(m_fit.x(x1)) - left, px(m_fit.y(y1)) - top,
        text, style, align);
}

void Screen_CreateNewCharacter::on_update()
{
    update_layout();

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

    // ESC -> character select
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
    using hb::shared::text::Align;

    // === Background (m_fit is from this frame's on_update, or on_initialize on the first frame) ===
    if (m_art)
    {
        m_game->m_Renderer->draw_texture_region(m_art.get(), 0.0f, 0.0f, art_width, art_height,
            m_fit.origin_x, m_fit.origin_y, art_width * m_fit.scale, art_height * m_fit.scale);
    }

    // === Left panel text ===
    const auto line = [&](float x0, float x1, float cy, const char* text, const hb::shared::render::Color& color, Align align) {
        draw_art_text(x0, cy - box_h / 2.0f, x1, cy + box_h / 2.0f, text, color, label_font_size, align);
    };
    line(prompt_x0, prompt_x1, prompt_name_cy, _BDRAW_ON_CREATE_NEW_CHARACTER1, GameColors::UIBlack, Align::Center);
    line(name_label_x0, name_label_x1, name_label_cy, DEF_MSG_CHARACTERNAME, GameColors::UIBlack, Align::Center);
    line(prompt_x0, prompt_x1, prompt_appearance_cy, _BDRAW_ON_CREATE_NEW_CHARACTER2, GameColors::UIBlack, Align::Center);

    static constexpr const char* row_labels[row_count] = {DEF_MSG_GENDER, DEF_MSG_SKINCOLOR, DEF_MSG_HAIRSTYLE,
        DEF_MSG_HAIRCOLOR, DEF_MSG_UNDERWEARCOLOR, DEF_MSG_STRENGTH, DEF_MSG_VITALITY, DEF_MSG_DEXTERITY,
        DEF_MSG_INTELLIGENCE, DEF_MSG_MAGIC, DEF_MSG_CHARISMA};
    const auto& nc = m_game->m_new_char;
    const int stat_values[] = {nc.stat_str, nc.stat_vit, nc.stat_dex, nc.stat_int, nc.stat_mag, nc.stat_chr};
    for (int row = 0; row < row_count; ++row)
    {
        line(row_label_x, row_buttons_x, row_cy[row], row_labels[row], GameColors::UIBlack, Align::MiddleLeft);
        if (row >= appearance_rows)
        {
            line(stat_box_x0, stat_box_x1, row_cy[row], std::format("{}", stat_values[row - appearance_rows]).c_str(),
                GameColors::UILabel, Align::Center);
        }
    }
    line(prompt_x0, prompt_x1, points_left_cy, std::format(_BDRAW_ON_CREATE_NEW_CHARACTER4, m_iNewCharPoint).c_str(),
        GameColors::UIBlack, Align::Center);

    // === Controls (textbox, row buttons, action buttons) ===
    m_controls.render();

    // === Character preview: the body beside the oval, the walking figure in it ===
    switch (nc.gender) {
    case 1: m_game->m_entity_state.m_owner_type = hb::shared::owner::MaleFirst; break;
    case 2: m_game->m_entity_state.m_owner_type = hb::shared::owner::FemaleFirst; break;
    }
    m_game->m_entity_state.m_owner_type += nc.skin_col - 1;
    m_game->m_entity_state.m_dir = m_game->m_menu_dir;
    m_game->m_entity_state.m_appearance.clear();
    m_game->m_entity_state.m_appearance.underwear_type = nc.under_col;
    m_game->m_entity_state.m_appearance.hair_style = nc.hair_style;
    m_game->m_entity_state.m_appearance.hair_color = nc.hair_col;
    m_game->m_entity_state.m_name.fill('\0');
    std::snprintf(m_game->m_entity_state.m_name.data(), m_game->m_entity_state.m_name.size(), "%s", nc.player_name.c_str());
    m_game->m_entity_state.m_action = Type::Move;
    m_game->m_entity_state.m_frame = m_game->m_menu_frame;

    Screen_OnGame::draw_character_body(*m_game, static_cast<short>(px(m_fit.x(body_pivot_x))),
        static_cast<short>(px(m_fit.y(body_pivot_y))), m_game->m_entity_state.m_owner_type);
    m_game->draw_object_on_move_for_menu(0, 0, px(m_fit.x(walk_pivot_x)), px(m_fit.y(walk_pivot_y)), false, time, false);

    // === Derived stats ===
    auto& fe = m_game->m_formula_engine;
    int preview_hp = 0, preview_mp = 0, preview_sp = 0;
    if (fe.has_formula("max_hp"))
    {
        preview_hp = hb::shared::calc::max_hp(fe,
            hb::shared::calc::vit{(double)nc.stat_vit}, hb::shared::calc::level{1.0},
            hb::shared::calc::str{(double)nc.stat_str}, hb::shared::calc::angelic_str{0.0});
        preview_mp = hb::shared::calc::max_mp(fe,
            hb::shared::calc::mag{(double)nc.stat_mag}, hb::shared::calc::angelic_mag{0.0},
            hb::shared::calc::level{1.0}, hb::shared::calc::intel{(double)nc.stat_int},
            hb::shared::calc::angelic_int{0.0});
        preview_sp = hb::shared::calc::max_sp(fe,
            hb::shared::calc::str{(double)nc.stat_str}, hb::shared::calc::angelic_str{0.0},
            hb::shared::calc::level{1.0});
    }
    static constexpr const char* vital_labels[] = {DEF_MSG_HITPOINT, DEF_MSG_MANAPOINT, DEF_MSG_STAMINAPOINT};
    const int vital_values[] = {preview_hp, preview_mp, preview_sp};
    for (int i = 0; i < 3; ++i)
    {
        line(vital_label_x, vital_box_x0, vital_cy[i], vital_labels[i], GameColors::UIBlack, Align::MiddleLeft);
        line(vital_box_x0, vital_box_x1, vital_cy[i], std::format("{}", vital_values[i]).c_str(), GameColors::UILabel,
            Align::Center);
    }

    m_game->draw_version();
    draw_copyright();

    // === Help text for what the mouse is over ===
    render_help_text();

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

void Screen_CreateNewCharacter::render_help_text()
{
    static constexpr const char* name_help[] = {UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER1};
    static constexpr const char* gender_help[] = {UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER2};
    static constexpr const char* skin_help[] = {UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER3};
    static constexpr const char* hair_style_help[] = {UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER4};
    static constexpr const char* hair_color_help[] = {UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER5};
    static constexpr const char* underwear_help[] = {UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER6};
    static constexpr const char* str_help[] = {UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER7, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER8,
        UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER9, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER10, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER11};
    static constexpr const char* vit_help[] = {UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER12, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER13,
        UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER14, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER15, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER16};
    static constexpr const char* dex_help[] = {UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER17, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER18,
        UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER19, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER20};
    static constexpr const char* int_help[] = {UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER21, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER22,
        UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER23, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER24};
    static constexpr const char* mag_help[] = {UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER25, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER26,
        UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER27, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER28};
    static constexpr const char* chr_help[] = {UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER29, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER30,
        UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER31, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER32};
    static constexpr const char* no_name_help[] = {UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER35};
    static constexpr const char* points_help[] = {UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER36};
    static constexpr const char* bad_name_help[] = {UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER39,
        UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER40, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER41};
    static constexpr const char* create_help[] = {UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER44, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER45,
        UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER46, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER47, UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER48};
    static constexpr const char* cancel_help[] = {UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER49};
    static constexpr const char* warrior_help[] = {UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER50};
    static constexpr const char* mage_help[] = {UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER51};
    static constexpr const char* master_help[] = {UPDATE_SCREEN_ON_CREATE_NEW_CHARACTER52};
    static constexpr std::span<const char* const> row_help[row_count] = {gender_help, skin_help, hair_style_help,
        hair_color_help, underwear_help, str_help, vit_help, dex_help, int_help, mag_help, chr_help};

    const auto hovered = [this](int id) {
        const cc::control* c = m_controls.find(id);
        return c && c->is_hovered();
    };

    std::span<const char* const> lines;
    if (hovered(ZONE_NAME))
        lines = name_help;
    else if (hovered(BTN_CREATE))
    {
        auto* tb = m_controls.find_as<cc::textbox>(TXT_NAME);
        const std::string name = tb ? tb->text() : std::string();
        if (name.empty())
            lines = no_name_help;
        else if (m_iNewCharPoint > 0)
            lines = points_help;
        else if (!CMisc::check_valid_name(name.c_str()))
            lines = bad_name_help;
        else
            lines = create_help;
    }
    else if (hovered(BTN_CANCEL))
        lines = cancel_help;
    else if (hovered(BTN_WARRIOR))
        lines = warrior_help;
    else if (hovered(BTN_MAGE))
        lines = mage_help;
    else if (hovered(BTN_MASTER))
        lines = master_help;
    else
    {
        for (int row = 0; row < row_count; ++row)
        {
            if (hovered(ZONE_ROW + row))
            {
                lines = row_help[row];
                break;
            }
        }
    }

    for (std::size_t i = 0; i < lines.size(); ++i)
    {
        const float cy = help_cy0 + help_pitch * static_cast<float>(i);
        draw_art_text(help_x0, cy - box_h / 2.0f, help_x1, cy + box_h / 2.0f, lines[i], GameColors::UIBlack,
            help_font_size, hb::shared::text::Align::Center);
    }
}
