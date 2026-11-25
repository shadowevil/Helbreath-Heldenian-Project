// screen_login.cpp: Login Screen Implementation
//
//////////////////////////////////////////////////////////////////////

#include "screen_login.h"
#include "screen_select_character.h"
#include "game.h"
#include "game_mode_manager.h"
#include "input.h"
#include "global_def.h"
#include "login_version.h"
#include "asio_socket.h"
#include "misc.h"
#include "game_fonts.h"
#include "text_lib_ext.h"
#include "text_field_renderer.h"
#include "packet/shared_packets.h"
#include "audio_manager.h"
#include "panel_kit.h"
#include "renderer.h"
#include "sprite_factory.h"
#include "text_renderer.h"
#include "log.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>

using namespace hb::shared::net;

namespace
{

// interface/front_end/login_screen.png (HH-0089): the user's sheet, the 1920x1080 backdrop at the top and, below
// it, the login plate (measured from the file: x 10-607, y 1093-1426). The plate is the original 332x184 panel
// at 1.8x, drawn where the original drew it, UI (99,182) = art (418,328); everything on it is laid out in the
// original panel's pixels, times 1.8 art px, through the same cover fit as the main menu.
constexpr float art_width = 1920.0f;
constexpr float art_height = 1080.0f;
constexpr float plate_src_x = 10.0f;
constexpr float plate_src_y = 1093.0f;
constexpr float plate_w = 598.0f;
constexpr float plate_h = 334.0f;
constexpr float plate_art_x = 418.0f;
constexpr float plate_art_y = 328.0f;
constexpr float panel_scale = 1.8f;

// The original panel fades in after a delay
constexpr uint32_t fade_delay_ms = 500;
constexpr uint32_t fade_duration_ms = 200;

// Labels: gold small caps in Copperplate CC Bold, the sizes in art px (user, 2026-11-25), with the classic drop
// shadow (a black copy 1 original px right and down = 2 art px) and, on the buttons, a white hover glow
constexpr const char* label_face_path = "fonts/copperplate_cc_bold.otf";
constexpr hb::shared::render::Color label_gold{246, 190, 40};
constexpr float small_ratio = 0.82f;
constexpr float letter_spacing_art = 1.2f;
constexpr float shadow_art = 2.0f;
constexpr int glow_dilate_art = 2;       // a thin core, so the halo is mostly the smooth blur
constexpr float glow_blur_art = 6.0f;
constexpr float glow_intensity = 0.9f;   // the additive halo's strength

struct label_spec
{
	const char* text;
	float x;   // panel px: the anchor on the baseline
	float y;
	hb::client::small_caps_label::anchor where;
	float cap_px;   // art px
	bool glows;
};

using anchor = hb::client::small_caps_label::anchor;
constexpr std::array<label_spec, 5> labels{{
	{"Log In", 162.0f, 21.0f, anchor::centre, 30.0f, false},
	{"Account", 126.0f, 50.5f, anchor::right, 24.0f, false},
	{"Password", 126.0f, 75.0f, anchor::right, 24.0f, false},
	{"Connect", 83.0f, 176.0f, anchor::centre, 24.0f, true},
	{"Cancel", 255.0f, 176.0f, anchor::centre, 24.0f, true},
}};
constexpr int label_connect = 3;
constexpr int label_cancel = 4;

// Panel px rects: the original textboxes (234,222) and (234,245) minus the panel's (99,182), and the buttons
constexpr float field_x = 135.0f;
constexpr float field_y = 40.0f;
constexpr float field_step = 23.0f;
constexpr float field_w = 147.0f;
constexpr float field_h = 17.0f;
constexpr int field_pad_x = 4;   // the text starts inside the inset's bevel
constexpr int field_pad_y = 2;
constexpr float field_darken = 0.55f;
constexpr float connect_x = 41.0f;
constexpr float connect_w = 84.0f;
constexpr float cancel_x = 217.0f;
constexpr float cancel_w = 76.0f;
constexpr float button_y = 161.0f;
constexpr float button_h = 20.0f;

// Help lines: baselines in panel px, light with the default drop shadow
constexpr float help_x = 57.0f;
constexpr float help_y = 107.0f;
constexpr float help_step = 13.5f;
constexpr float help_ascent = 11.0f;   // the default font's baseline below its top, UI units
constexpr hb::shared::render::Color help_color{240, 232, 214};
constexpr std::array<const char*, 3> help_lines{
	"Enter Account Name and password.",
	"If you don't have any account,",
	"log in after creating new account.",
};

} // namespace

Screen_Login::Screen_Login(CGame* game)
    : IGameScreen(game)
{
}

void Screen_Login::on_initialize()
{
    m_game->m_arrow_pressed = 0;

    const std::string art_path = hb::shared::sprite::Sprites::get_sprite_path() + "/interface/front_end/login_screen.png";
    m_art.reset(m_game->m_Renderer->create_texture_from_file(art_path.c_str()));
    if (!m_art)
        hb::logger::error("Login screen art not loaded: {}", art_path);

    m_controls.set_screen_size(LOGICAL_WIDTH(), LOGICAL_HEIGHT());
    m_controls.set_hover_focus(false);
    m_controls.set_enter_advances_focus(true);
    m_controls.set_default_button(BTN_LOGIN);

    // Clipboard provider (via engine abstraction)
    m_controls.set_clipboard_provider(
        [] { return hb::shared::input::get_clipboard_text(); },
        [](const std::string& s) { hb::shared::input::set_clipboard_text(s); }
    );

    // Textbox render helper — draws text, cursor, and selection for all states
    // The text and caret fade in with the plate, so none shows over the bare backdrop
    auto textbox_renderer = [this](const cc::control& c) {
        if (m_plate_alpha <= 0.0f)
            return;
        auto& field = static_cast<const cc::text_input&>(c);
        hb::client::draw_text_field(field, GameClock::get_time_ms(),
            hb::shared::text::TextStyle::with_shadow(GameColors::InputValid).with_alpha(m_plate_alpha),
            hb::shared::text::TextStyle::with_shadow(GameColors::InputInvalid).with_alpha(m_plate_alpha));
    };

    // === Textboxes ===
    auto* tb_name = m_controls.add<cc::textbox>(TXT_NAME, cc::rect{0, 0, 1, 1}, 11);
    tb_name->set_character_filter(hb::client::username_allowed_chars);
    tb_name->set_validator([](const std::string& s) {
        return !s.empty() && CMisc::check_valid_name(s.c_str());
    });
    tb_name->set_render_handler(textbox_renderer);

    auto* tb_password = m_controls.add<cc::textbox>(TXT_PASSWORD, cc::rect{0, 0, 1, 1}, 11);
    tb_password->set_hidden(true);
    tb_password->set_character_filter(hb::client::password_allowed_chars);
    tb_password->set_validator([](const std::string& s) {
        return !s.empty() && CMisc::check_valid_string(s.c_str());
    });
    tb_password->set_render_handler(textbox_renderer);

    // === Buttons ===
    // The buttons are hit areas over their labels; the labels and glows are drawn in on_render
    auto* btn_login = m_controls.add<cc::button>(BTN_LOGIN, cc::rect{0, 0, 1, 1});
    btn_login->set_on_click([this](int) { submit_login(); });
    btn_login->set_click_sound([this] { audio_manager::get().play_game_sound(sound_type::effect, 14, 5); });
    btn_login->set_render_handler([](const cc::control&) {});

    auto* btn_cancel = m_controls.add<cc::button>(BTN_CANCEL, cc::rect{0, 0, 1, 1});
    btn_cancel->set_on_click([this](int) { m_game->change_game_mode(GameMode::MainMenu); });
    btn_cancel->set_click_sound([this] { audio_manager::get().play_game_sound(sound_type::effect, 14, 5); });
    btn_cancel->set_render_handler([](const cc::control&) {});

    m_controls.set_focus_order({TXT_NAME, TXT_PASSWORD, BTN_LOGIN, BTN_CANCEL});
    m_controls.set_focus(TXT_NAME);
    update_layout();
}

void Screen_Login::on_uninitialize()
{
	m_art.reset();
	for (hb::client::small_caps_label& label : m_labels)
		label.release();
	m_baked_scale = 0.0f;
}

void Screen_Login::bake_labels()
{
	// At the window's resolution: art px * (UI units per art px) * (window px per UI unit)
	hb::shared::text::ITextRenderer* text = hb::shared::text::GetTextRenderer();
	const float ui_scale = m_game->m_Renderer->ui_scale();
	const float window_per_art = m_fit.scale * ui_scale;
	m_baked_scale = window_per_art;
	const int face = text ? text->load_face(label_face_path) : -1;
	if (face <= 0)
	{
		hb::logger::error("Login labels not rendered ({})", label_face_path);
		return;
	}
	for (std::size_t i = 0; i < labels.size(); i++)
	{
		hb::client::small_caps_label::style style;
		style.face = face;
		style.big_px = labels[i].cap_px * window_per_art;
		style.small_ratio = small_ratio;
		style.spacing_px = letter_spacing_art * window_per_art;
		style.glow_intensity = glow_intensity;
		if (labels[i].glows)
		{
			style.glow_dilate = (std::max)(1, static_cast<int>(std::lround(static_cast<float>(glow_dilate_art) * window_per_art)));
			style.glow_blur = glow_blur_art * window_per_art;
		}
		m_labels[i].bake(labels[i].text, style, ui_scale);
	}
}

cc::rect Screen_Login::panel_rect(float x, float y, float w, float h) const
{
	// Panel px -> art px -> UI units through this frame's fit, edges rounded so neighbours stay flush
	const auto ui_x = [this](float px) { return m_fit.x(plate_art_x + px * panel_scale); };
	const auto ui_y = [this](float py) { return m_fit.y(plate_art_y + py * panel_scale); };
	const int left = static_cast<int>(std::lround(ui_x(x)));
	const int top = static_cast<int>(std::lround(ui_y(y)));
	return cc::rect{left, top, static_cast<int>(std::lround(ui_x(x + w))) - left, static_cast<int>(std::lround(ui_y(y + h))) - top};
}

void Screen_Login::update_layout()
{
	m_fit = hb::shared::render::cover_fit(m_game->m_Renderer->get_screen_extent(), art_width, art_height);
	if (m_label_debounce.update(m_baked_scale, m_fit.scale * m_game->m_Renderer->ui_scale()))
		bake_labels();

	// The textbox controls cover the text inside each inset, so the caret and clicks line up with the text
	for (int i = 0; i < 2; i++)
	{
		const cc::rect r = panel_rect(field_x, field_y + field_step * static_cast<float>(i), field_w, field_h);
		if (cc::control* field = m_controls.find(TXT_NAME + i))
			field->set_local_bounds(cc::rect{r.x + field_pad_x, r.y + field_pad_y, r.w - 2 * field_pad_x, r.h - 2 * field_pad_y});
	}
	if (cc::control* button = m_controls.find(BTN_LOGIN))
		button->set_local_bounds(panel_rect(connect_x, button_y, connect_w, button_h));
	if (cc::control* button = m_controls.find(BTN_CANCEL))
		button->set_local_bounds(panel_rect(cancel_x, button_y, cancel_w, button_h));
}

void Screen_Login::on_update()
{
    update_layout();
    update_controls(m_controls);

    if (m_controls.escape_pressed())
        m_game->change_game_mode(GameMode::MainMenu);
}

void Screen_Login::submit_login()
{
    auto* tb_name = m_controls.find_as<cc::textbox>(TXT_NAME);
    auto* tb_pass = m_controls.find_as<cc::textbox>(TXT_PASSWORD);

    if (!tb_name->is_valid() || !tb_pass->is_valid()) return;

    m_game->m_account_name = tb_name->text().c_str();
    m_game->m_account_password = tb_pass->text().c_str();

    // Build login packet for deferred send after connection establishes
    hb::net::LoginRequest req{};
    req.header.msg_id = MsgId::request_login;
    req.header.msg_type = 0;
    std::snprintf(req.account_name, sizeof(req.account_name), "%s", m_game->m_account_name.c_str());
    std::snprintf(req.password, sizeof(req.password), "%s", m_game->m_account_password.c_str());
    std::snprintf(req.world_name, sizeof(req.world_name), "%s", m_game->m_world_server_name.c_str());
    hb::client::fill_login_version(req, m_game->m_build_window_offset);
    m_game->set_pending_login_packet(req);

    m_game->m_l_sock = std::make_unique<hb::shared::net::ASIOSocket>(
        m_game->m_io_pool->get_context(), game_limits::socket_block_limit);
    m_game->m_l_sock->connect(m_game->m_log_server_addr.c_str(), m_game->m_log_server_port);
    m_game->m_l_sock->init_buffer_size(hb::shared::limits::MsgBufferSize);

    m_game->change_game_mode(GameMode::Connecting);
    std::snprintf(m_game->m_msg, sizeof(m_game->m_msg), "%s", "11");
}

bool Screen_Login::on_net_response(uint16_t response_type, char* data)
{
    switch (response_type) {
    case LogResMsg::Confirm:
    {
        const auto* list = hb::net::PacketCast<hb::net::PacketLogCharacterListHeader>(
            data, sizeof(hb::net::PacketLogCharacterListHeader));
        if (!list) return true;
        m_game->m_accnt_year = 0;
        m_game->m_accnt_month = 0;
        m_game->m_accnt_day = 0;
        m_game->m_ip_year = 0;
        m_game->m_ip_month = 0;
        m_game->m_ip_day = 0;
        m_game->m_total_char = std::min(static_cast<int>(list->total_chars), 4);
        for (int i = 0; i < 4; i++)
            if (m_game->m_char_list[i] != 0)
                m_game->m_char_list[i].reset();

        const auto* entries = reinterpret_cast<const hb::net::PacketLogCharacterEntry*>(
            data + sizeof(hb::net::PacketLogCharacterListHeader));
        for (int i = 0; i < m_game->m_total_char; i++)
        {
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
        set_screen<Screen_SelectCharacter>();
        return true;
    }

    case LogResMsg::Reject:
    {
        const auto* pkt = hb::net::PacketCast<hb::net::PacketLogResponseReject>(data, sizeof(hb::net::PacketLogResponseReject));
        if (!pkt) return true;
        m_game->m_block_year = pkt->block_year;
        m_game->m_block_month = pkt->block_month;
        m_game->m_block_day = pkt->block_day;
        std::snprintf(m_game->m_msg, sizeof(m_game->m_msg), "%s", "7H");
        m_game->change_game_mode(GameMode::LogResMsg);
        return true;
    }

    case LogResMsg::NotEnoughPoint:
        std::snprintf(m_game->m_msg, sizeof(m_game->m_msg), "%s", "7I");
        m_game->change_game_mode(GameMode::LogResMsg);
        return true;

    case LogResMsg::AccountLocked:
        std::snprintf(m_game->m_msg, sizeof(m_game->m_msg), "%s", "7K");
        m_game->change_game_mode(GameMode::LogResMsg);
        return true;

    case LogResMsg::ServiceNotAvailable:
        std::snprintf(m_game->m_msg, sizeof(m_game->m_msg), "%s", "7L");
        m_game->change_game_mode(GameMode::LogResMsg);
        return true;

    case LogResMsg::NotExistingAccount:
        std::snprintf(m_game->m_msg, sizeof(m_game->m_msg), "%s", "12");
        m_game->change_game_mode(GameMode::LogResMsg);
        return true;

    case LogResMsg::PasswordMismatch:
        std::snprintf(m_game->m_msg, sizeof(m_game->m_msg), "%s", "11");
        m_game->change_game_mode(GameMode::LogResMsg);
        return true;

    case LogResMsg::NotExistingWorldServer:
        std::snprintf(m_game->m_msg, sizeof(m_game->m_msg), "%s", "1Y");
        m_game->change_game_mode(GameMode::LogResMsg);
        return true;

    case LogResMsg::InvalidKoreanSsn:
        std::snprintf(m_game->m_msg, sizeof(m_game->m_msg), "%s", "1a");
        m_game->change_game_mode(GameMode::LogResMsg);
        return true;

    case LogResMsg::LessThenFifteen:
        std::snprintf(m_game->m_msg, sizeof(m_game->m_msg), "%s", "1b");
        m_game->change_game_mode(GameMode::LogResMsg);
        return true;

    case LogResMsg::InputKeyCode:
    {
        const auto* pkt = hb::net::PacketCast<hb::net::PacketLogResponseCode>(data, sizeof(hb::net::PacketLogResponseCode));
        if (!pkt) return true;
        std::memset(m_game->m_msg, 0, sizeof(m_game->m_msg));
        switch (pkt->code) {
        case 1: std::snprintf(m_game->m_msg, sizeof(m_game->m_msg), "%s", "8U"); break;
        case 2: std::snprintf(m_game->m_msg, sizeof(m_game->m_msg), "%s", "82"); break;
        case 3: std::snprintf(m_game->m_msg, sizeof(m_game->m_msg), "%s", "81"); break;
        case 4: std::snprintf(m_game->m_msg, sizeof(m_game->m_msg), "%s", "8V"); break;
        case 5: std::snprintf(m_game->m_msg, sizeof(m_game->m_msg), "%s", "8W"); break;
        }
        m_game->change_game_mode(GameMode::LogResMsg);
        return true;
    }
    }
    return false;
}

void Screen_Login::on_render()
{
	// m_fit is from this frame's on_update (or on_initialize on the frame the screen is switched in)
	hb::shared::render::IRenderer* renderer = m_game->m_Renderer;
	if (m_art)
	{
		renderer->draw_texture_region(m_art.get(), 0.0f, 0.0f, art_width, art_height,
			m_fit.origin_x, m_fit.origin_y, art_width * m_fit.scale, art_height * m_fit.scale);
	}

	// The plate with everything on it fades in: 500ms delay, then 200ms fade
	const uint32_t elapsed = get_elapsed_ms();
	const float alpha = elapsed <= fade_delay_ms ? 0.0f
		: (std::min)(1.0f, static_cast<float>(elapsed - fade_delay_ms) / static_cast<float>(fade_duration_ms));
	m_plate_alpha = alpha;
	if (alpha > 0.0f)
		draw_plate(alpha);

	// Controls (textbox values; the buttons draw nothing of their own)
	m_controls.render();

	draw_version();
	draw_copyright();
}

void Screen_Login::draw_plate(float alpha)
{
	hb::shared::render::IRenderer* renderer = m_game->m_Renderer;
	if (m_art)
	{
		renderer->draw_texture_region(m_art.get(), plate_src_x, plate_src_y, plate_w, plate_h,
			m_fit.x(plate_art_x), m_fit.y(plate_art_y), plate_w * m_fit.scale, plate_h * m_fit.scale, alpha);
	}

	// Kit inset fields at the original textbox rects
	const hb::client::panel_kit& kit = m_game->get_panel_kit();
	for (int i = 0; i < 2; i++)
	{
		const cc::rect r = panel_rect(field_x, field_y + field_step * static_cast<float>(i), field_w, field_h);
		kit.inset({r.x, r.y, r.w, r.h}, {}, field_darken, alpha);
	}

	// Labels; Connect glows when hovered or focused and both fields are valid (as the original), Cancel always
	const auto* tb_name = m_controls.find_as<cc::textbox>(TXT_NAME);
	const auto* tb_password = m_controls.find_as<cc::textbox>(TXT_PASSWORD);
	const bool can_connect = tb_name && tb_name->is_valid() && tb_password && tb_password->is_valid();
	const cc::control* connect = m_controls.find(BTN_LOGIN);
	const cc::control* cancel = m_controls.find(BTN_CANCEL);
	const float shadow = shadow_art * m_fit.scale;
	for (std::size_t i = 0; i < labels.size(); i++)
	{
		bool glow = false;
		if (static_cast<int>(i) == label_connect)
			glow = can_connect && connect && connect->is_highlighted();
		else if (static_cast<int>(i) == label_cancel)
			glow = cancel && cancel->is_highlighted();
		m_labels[i].draw(*renderer, m_fit.x(plate_art_x + labels[i].x * panel_scale), m_fit.y(plate_art_y + labels[i].y * panel_scale),
			labels[i].where, label_gold, alpha, shadow, glow);
	}

	// Help text in the default font
	const auto help_style = hb::shared::text::TextStyle::with_shadow(help_color).with_alpha(alpha);
	const int x = static_cast<int>(std::lround(m_fit.x(plate_art_x + help_x * panel_scale)));
	for (std::size_t i = 0; i < help_lines.size(); i++)
	{
		const float baseline = m_fit.y(plate_art_y + (help_y + help_step * static_cast<float>(i)) * panel_scale);
		hb::shared::text::draw_text(GameFont::Default, x, static_cast<int>(std::lround(baseline - help_ascent)), help_lines[i], help_style);
	}
}
