#include "screen_create_account.h"
#include "game.h"
#include "game_mode_manager.h"
#include "input.h"
#include "global_def.h"
#include "asio_socket.h"
#include "misc.h"
#include "lan_eng.h"
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
#include <cmath>
#include <string>

using namespace hb::shared::net;

namespace
{

// interface/front_end/create_new_account.png (HH-0087): the main menu backdrop with a blank stone tablet. The
// original 800x600 screen maps onto the art's centre 4:3 region (x 240-1680, 1.8 art px per UI unit); the form
// is laid out in UI units on the tablet, whose top-left is UI (403,194), and drawn at the kit's own size
constexpr float art_width = 1920.0f;
constexpr float art_height = 1080.0f;
constexpr float classic_art_x = 240.0f;
constexpr float classic_art_scale = 1.8f;
constexpr float tablet_x = 403.0f;
constexpr float tablet_y = 194.0f;
constexpr int form_w = 340;
constexpr int form_h = 296;

// Form-relative layout (UI units)
constexpr int title_y = 24;
constexpr int label_x = 30;
constexpr int field_x = 102;
constexpr int field_y = 66;
constexpr int field_step = 30;
constexpr int field_w = 208;
constexpr int field_h = 18;
constexpr int field_pad_x = 5;   // the text starts inside the inset's bevel
constexpr int field_pad_y = 2;
constexpr int rule_y = 192;
constexpr int help_y = 208;
constexpr int help_step = 15;
constexpr int button_x = 40;
constexpr int button_step = 90;
constexpr int button_y = 250;
constexpr int button_w = 78;
constexpr int button_h = 20;

constexpr hb::shared::render::Color label_color{222, 206, 180};   // light parchment
constexpr hb::shared::render::Color title_fill{240, 228, 204};
constexpr hb::shared::render::Color title_glow{10, 6, 4};
constexpr hb::shared::render::Color help_color{255, 181, 0};
constexpr float field_darken = 0.55f;

constexpr const char* title_text = "New Account";

// The glow's look at 1x (a 5 px square dilate, a 2.5 px blur, 85% opaque); baked at the window's resolution
constexpr int glow_dilate_px = 2;
constexpr float glow_blur_px = 2.5f;
constexpr float glow_alpha = 0.85f;

} // namespace

Screen_CreateAccount::Screen_CreateAccount(CGame* game)
    : IGameScreen(game)
{
}

void Screen_CreateAccount::on_initialize()
{
    m_game->m_arrow_pressed = 0;

    const std::string art_path = hb::shared::sprite::Sprites::get_sprite_path() + "/interface/front_end/create_new_account.png";
    m_art.reset(m_game->m_Renderer->create_texture_from_file(art_path.c_str()));
    if (!m_art)
        hb::logger::error("Create account art not loaded: {}", art_path);

    m_controls.set_screen_size(LOGICAL_WIDTH(), LOGICAL_HEIGHT());
    m_controls.set_hover_focus(false);
    m_controls.set_enter_advances_focus(true);
    m_controls.set_default_button(BTN_CREATE);

    // Clipboard provider (via engine abstraction)
    m_controls.set_clipboard_provider(
        [] { return hb::shared::input::get_clipboard_text(); },
        [](const std::string& s) { hb::shared::input::set_clipboard_text(s); }
    );

    // Textbox render helper — draws text, cursor, and selection for all states
    auto textbox_renderer = [](const cc::control& c) {
        auto& field = static_cast<const cc::text_input&>(c);
        hb::client::draw_text_field(field, GameClock::get_time_ms(),
            hb::shared::text::TextStyle::with_shadow(GameColors::InputValid),
            hb::shared::text::TextStyle::with_shadow(GameColors::InputInvalid));
    };

    // === Textboxes ===
    auto* tb_account = m_controls.add<cc::textbox>(TXT_ACCOUNT, cc::rect{0, 0, 1, 1}, 11);
    tb_account->set_character_filter(hb::client::username_allowed_chars);
    tb_account->set_validator([](const std::string& s) {
        return !s.empty() && CMisc::check_valid_name(s.c_str());
    });
    tb_account->set_render_handler(textbox_renderer);

    auto* tb_password = m_controls.add<cc::textbox>(TXT_PASSWORD, cc::rect{0, 0, 1, 1}, 11);
    tb_password->set_hidden(true);
    tb_password->set_character_filter(hb::client::password_allowed_chars);
    tb_password->set_validator([](const std::string& s) {
        return !s.empty() && CMisc::check_valid_name(s.c_str());
    });
    tb_password->set_render_handler(textbox_renderer);

    auto* tb_confirm = m_controls.add<cc::textbox>(TXT_CONFIRM, cc::rect{0, 0, 1, 1}, 11);
    tb_confirm->set_hidden(true);
    tb_confirm->set_character_filter(hb::client::password_allowed_chars);
    tb_confirm->set_validator([this](const std::string& s) {
        auto* pw = m_controls.find_as<cc::textbox>(TXT_PASSWORD);
        return !s.empty() && pw && s == pw->text();
    });
    tb_confirm->set_render_handler(textbox_renderer);

    auto* tb_email = m_controls.add<cc::textbox>(TXT_EMAIL, cc::rect{0, 0, 1, 1}, 49);
    tb_email->set_character_filter(hb::client::email_allowed_chars);
    tb_email->set_validator([](const std::string& s) {
        return !s.empty() && CMisc::is_valid_email(s.c_str());
    });
    tb_email->set_render_handler(textbox_renderer);

    // === Buttons ===
    // Kit buttons (the small layout, 78 wide) at the control's bounds
    auto draw_button = [this](const cc::control& c, const char* label, bool lit) {
        const auto sb = c.screen_bounds();
        const hb::client::panel_kit& kit = m_game->get_panel_kit();
        hb::client::button_style style = kit.style_of(hb::client::button_layout::small);
        style.w = button_w;
        kit.button(style, label, {sb.x, sb.y, sb.w, sb.h}, lit);
    };

    auto* btn_create = m_controls.add<cc::button>(BTN_CREATE, cc::rect{0, 0, 1, 1});
    btn_create->set_on_click([this](int) { submit_create_account(); });
    btn_create->set_click_sound([this] { audio_manager::get().play_game_sound(sound_type::effect, 14, 5); });
    btn_create->set_render_handler([this, draw_button](const cc::control& c) {
        // Only highlight when all fields are valid
        bool all_valid = true;
        for (int id = TXT_ACCOUNT; id <= TXT_EMAIL; id++)
        {
            auto* tb = m_controls.find_as<cc::textbox>(id);
            if (tb && !tb->is_valid()) { all_valid = false; break; }
        }
        draw_button(c, "Create", all_valid && c.is_highlighted());
    });

    auto* btn_clear = m_controls.add<cc::button>(BTN_CLEAR, cc::rect{0, 0, 1, 1});
    btn_clear->set_click_sound([this] { audio_manager::get().play_game_sound(sound_type::effect, 14, 5); });
    btn_clear->set_on_click([this](int) {
        for (int id = TXT_ACCOUNT; id <= TXT_EMAIL; id++)
        {
            auto* tb = m_controls.find_as<cc::textbox>(id);
            if (tb) tb->text().clear();
        }
        m_controls.set_focus(TXT_ACCOUNT);
    });
    btn_clear->set_render_handler([draw_button](const cc::control& c) {
        draw_button(c, "Clear", c.is_highlighted());
    });

    auto* btn_cancel = m_controls.add<cc::button>(BTN_CANCEL, cc::rect{0, 0, 1, 1});
    btn_cancel->set_on_click([this](int) { m_game->change_game_mode(GameMode::MainMenu); });
    btn_cancel->set_click_sound([this] { audio_manager::get().play_game_sound(sound_type::effect, 14, 5); });
    btn_cancel->set_render_handler([draw_button](const cc::control& c) {
        draw_button(c, "Cancel", c.is_highlighted());
    });

    m_controls.set_focus_order({TXT_ACCOUNT, TXT_PASSWORD, TXT_CONFIRM, TXT_EMAIL,
                                BTN_CREATE, BTN_CLEAR, BTN_CANCEL});
    m_controls.set_focus(TXT_ACCOUNT);
    update_layout();
}

void Screen_CreateAccount::bake_title_glow()
{
    // Baked from the kit's title_red recipe (face, size, tracking) at the window's resolution, so the glow sits
    // exactly behind the kit's title; redone when the UI scale changes
    m_title_glow.reset();
    const float scale = m_game->m_Renderer->ui_scale();
    m_glow_scale = scale;
    hb::shared::text::ITextRenderer* text = hb::shared::text::GetTextRenderer();
    if (!text)
        return;
    const hb::client::panel_kit& kit = m_game->get_panel_kit();
    hb::shared::text::text_run run = kit.recipe_run("title_red");
    if (run.face <= 0)
        return;
    const hb::client::kit_rect ink = kit.text_ink("title_red", title_text, 0, 0, hb::client::text_anchor::middle);
    run.pixel_size = static_cast<int>(std::lround(static_cast<float>(run.pixel_size) * scale));
    run.tracking *= scale;
    const int dilate = (std::max)(1, static_cast<int>(std::lround(static_cast<float>(glow_dilate_px) * scale)));
    const float blur = glow_blur_px * scale;
    hb::shared::render::Color color = title_glow;
    color.a = static_cast<uint8_t>(glow_alpha * 255.0f);
    m_title_glow.reset(text->bake_glow(title_text, run, color, dilate, blur));
    if (!m_title_glow)
    {
        hb::logger::error("Create account title glow not rendered");
        return;
    }

    // Placement from the title's centre: the ink box and the pad around it, in UI units
    const float pad = static_cast<float>(hb::shared::text::ITextRenderer::glow_pad(dilate, blur)) / scale;
    m_glow_x = static_cast<float>(ink.x) - pad;
    m_glow_y = static_cast<float>(ink.y) - pad;
    m_glow_w = static_cast<float>(m_title_glow->get_width()) / scale;
    m_glow_h = static_cast<float>(m_title_glow->get_height()) / scale;
}

void Screen_CreateAccount::draw_title(const hb::client::panel_kit& kit, int cx, int cy) const
{
    // "New Account" in the kit serif title with a soft dark glow behind it (user, 2026-11-25: variant D), the
    // wave flourishes either side as the kit's titles have
    if (m_title_glow)
    {
        // On a whole window pixel, so the baked texels land 1:1 at a fractional UI scale
        const float scale = m_game->m_Renderer->ui_scale();
        const float x = std::round((static_cast<float>(cx) + m_glow_x) * scale) / scale;
        const float y = std::round((static_cast<float>(cy) + m_glow_y) * scale) / scale;
        m_game->m_Renderer->draw_texture_region(m_title_glow.get(), 0.0f, 0.0f,
            static_cast<float>(m_title_glow->get_width()), static_cast<float>(m_title_glow->get_height()),
            x, y, m_glow_w, m_glow_h);
    }
    kit.text("title_red", title_text, cx, cy, hb::client::text_anchor::middle, title_fill);
    const int reach = kit.text_width("title_red", title_text) / 2 + 14 + kit.flourish_size("divider_wave", 0.56f).x / 2;
    const int fy = kit.cap_middle("title_red", title_text, cy, hb::client::text_anchor::middle);
    kit.flourish("divider_wave", cx - reach, fy, false, 0.56f);
    kit.flourish("divider_wave", cx + reach, fy, true, 0.56f);
}

void Screen_CreateAccount::on_uninitialize()
{
    m_art.reset();
    m_title_glow.reset();
    m_glow_scale = 0.0f;
}

void Screen_CreateAccount::update_layout()
{
    m_fit = hb::shared::render::cover_fit(m_game->m_Renderer->get_screen_extent(), art_width, art_height);
    if (m_glow_debounce.update(m_glow_scale, m_game->m_Renderer->ui_scale()))
        bake_title_glow();

    // The form is centred on the tablet, which the fit places; it is drawn at the kit's own size
    const float centre_x = classic_art_x + (tablet_x + static_cast<float>(form_w) / 2.0f) * classic_art_scale;
    const float centre_y = (tablet_y + static_cast<float>(form_h) / 2.0f) * classic_art_scale;
    m_form_x = static_cast<int>(std::lround(m_fit.x(centre_x) - static_cast<float>(form_w) / 2.0f));
    m_form_y = static_cast<int>(std::lround(m_fit.y(centre_y) - static_cast<float>(form_h) / 2.0f));

    // The textbox controls cover the text inside each inset, so the caret and clicks line up with the text
    for (int i = 0; i < 4; i++)
    {
        if (cc::control* field = m_controls.find(TXT_ACCOUNT + i))
            field->set_local_bounds(cc::rect{m_form_x + field_x + field_pad_x, m_form_y + field_y + field_step * i + field_pad_y,
                field_w - 2 * field_pad_x, field_h - 2 * field_pad_y});
    }
    for (int i = 0; i < 3; i++)
    {
        if (cc::control* button = m_controls.find(BTN_CREATE + i))
            button->set_local_bounds(cc::rect{m_form_x + button_x + button_step * i, m_form_y + button_y, button_w, button_h});
    }
}

void Screen_CreateAccount::on_update()
{
    update_layout();
    update_controls(m_controls);

    if (m_controls.escape_pressed())
        m_game->change_game_mode(GameMode::MainMenu);
}

void Screen_CreateAccount::submit_create_account()
{
    auto* tb_name = m_controls.find_as<cc::textbox>(TXT_ACCOUNT);
    auto* tb_pass = m_controls.find_as<cc::textbox>(TXT_PASSWORD);
    auto* tb_confirm = m_controls.find_as<cc::textbox>(TXT_CONFIRM);
    auto* tb_email = m_controls.find_as<cc::textbox>(TXT_EMAIL);

    bool ready = tb_name->is_valid() && tb_pass->is_valid()
              && tb_confirm->is_valid() && tb_email->is_valid();

    if (!ready) return;

    m_game->m_arrow_pressed = 0;

    // Copy account credentials to session data
    m_game->m_account_name = tb_name->text().c_str();
    m_game->m_account_password = tb_pass->text().c_str();

    // Build CreateAccountRequest packet
    hb::net::CreateAccountRequest req{};
    req.header.msg_id = MsgId::RequestCreateNewAccount;
    req.header.msg_type = 0;
    std::snprintf(req.account_name, sizeof(req.account_name), "%s", tb_name->text().c_str());
    std::snprintf(req.password, sizeof(req.password), "%s", tb_pass->text().c_str());
    std::snprintf(req.email, sizeof(req.email), "%s", tb_email->text().c_str());

    // Store packet for sending after connection completes
    auto* p = reinterpret_cast<char*>(&req);
    m_game->m_pending_login_packet.assign(p, p + sizeof(req));

    // Connection logic
    m_game->m_l_sock = std::make_unique<hb::shared::net::ASIOSocket>(
        m_game->m_io_pool->get_context(), game_limits::socket_block_limit);
    m_game->m_l_sock->connect(m_game->m_log_server_addr.c_str(), m_game->m_log_server_port);
    m_game->m_l_sock->init_buffer_size(hb::shared::limits::MsgBufferSize);

    m_game->change_game_mode(GameMode::Connecting);
    std::snprintf(m_game->m_msg, sizeof(m_game->m_msg), "%s", "01");
}

bool Screen_CreateAccount::on_net_response(uint16_t response_type, char* data)
{
    switch (response_type) {
    case LogResMsg::NewAccountCreated:
        std::snprintf(m_game->m_msg, sizeof(m_game->m_msg), "%s", "54");
        m_game->change_game_mode(GameMode::LogResMsg);
        return true;

    case LogResMsg::NewAccountFailed:
        std::snprintf(m_game->m_msg, sizeof(m_game->m_msg), "%s", "05");
        m_game->change_game_mode(GameMode::LogResMsg);
        return true;

    case LogResMsg::AlreadyExistingAccount:
        std::snprintf(m_game->m_msg, sizeof(m_game->m_msg), "%s", "06");
        m_game->change_game_mode(GameMode::LogResMsg);
        return true;
    }
    return false;
}

void Screen_CreateAccount::on_render()
{
    auto label_style = hb::shared::text::TextStyle::with_shadow(label_color).with_bold();
    auto help_style = hb::shared::text::TextStyle::from_color(help_color);

    // m_fit and the form's place are from this frame's on_update (or on_initialize on the frame the screen is switched in)
    if (m_art)
    {
        m_game->m_Renderer->draw_texture_region(m_art.get(), 0.0f, 0.0f, art_width, art_height,
            m_fit.origin_x, m_fit.origin_y, art_width * m_fit.scale, art_height * m_fit.scale);
    }

    // The form on the tablet: kit title, labels, inset fields and rule; the controls draw the text and buttons
    const hb::client::panel_kit& kit = m_game->get_panel_kit();
    draw_title(kit, m_form_x + form_w / 2, m_form_y + title_y);
    static constexpr const char* labels[] = {"Account", "Password", "Confirm", "Email"};
    for (int i = 0; i < 4; i++)
    {
        const int y = m_form_y + field_y + field_step * i;
        hb::shared::text::draw_text(GameFont::Default, m_form_x + label_x, y + field_pad_y, labels[i], label_style);
        kit.inset({m_form_x + field_x, y, field_w, field_h}, {}, field_darken);
    }
    kit.rule(m_form_x + label_x, m_form_y + rule_y, form_w - 2 * label_x);

    // Controls (textbox values + buttons)
    m_controls.render();

    // Help text based on focused control: up to three lines under the rule, centred on the form
    int focused = m_controls.focused_id();
    auto help = [&](int line, const char* text) {
        hb::shared::text::draw_text_aligned(GameFont::Default, m_form_x, m_form_y + help_y + help_step * line, form_w, help_step,
            text, help_style, hb::shared::text::Align::TopCenter);
    };

    switch (focused)
    {
    case TXT_ACCOUNT:
        help(0, UPDATE_SCREEN_ON_CREATE_NEW_ACCOUNT1);
        help(1, UPDATE_SCREEN_ON_CREATE_NEW_ACCOUNT2);
        break;
    case TXT_PASSWORD:
        help(0, UPDATE_SCREEN_ON_CREATE_NEW_ACCOUNT4);
        break;
    case TXT_CONFIRM:
        help(0, UPDATE_SCREEN_ON_CREATE_NEW_ACCOUNT8);
        break;
    case TXT_EMAIL:
        help(0, UPDATE_SCREEN_ON_CREATE_NEW_ACCOUNT21);
        help(1, UPDATE_SCREEN_ON_CREATE_NEW_ACCOUNT22);
        help(2, UPDATE_SCREEN_ON_CREATE_NEW_ACCOUNT23);
        break;
    case BTN_CREATE:
    {
        // Compute validation flag for error messages
        auto* tb_name = m_controls.find_as<cc::textbox>(TXT_ACCOUNT);
        auto* tb_pass = m_controls.find_as<cc::textbox>(TXT_PASSWORD);
        auto* tb_confirm = m_controls.find_as<cc::textbox>(TXT_CONFIRM);
        auto* tb_email = m_controls.find_as<cc::textbox>(TXT_EMAIL);

        int flag = 0;
        if (tb_pass->text() != tb_confirm->text())                    flag = 9;
        if (!CMisc::check_valid_name(tb_pass->text().c_str()))        flag = 7;
        if (!CMisc::check_valid_name(tb_name->text().c_str()))        flag = 6;
        if (!CMisc::is_valid_email(tb_email->text().c_str()))         flag = 5;
        if (tb_confirm->text().empty())                               flag = 3;
        if (tb_pass->text().empty())                                  flag = 2;
        if (tb_name->text().empty())                                  flag = 1;

        switch (flag)
        {
        case 0: help(0, UPDATE_SCREEN_ON_CREATE_NEW_ACCOUNT33); break;
        case 1: help(0, UPDATE_SCREEN_ON_CREATE_NEW_ACCOUNT35); break;
        case 2: help(0, UPDATE_SCREEN_ON_CREATE_NEW_ACCOUNT38); break;
        case 3: help(0, UPDATE_SCREEN_ON_CREATE_NEW_ACCOUNT42); break;
        case 5: help(0, UPDATE_SCREEN_ON_CREATE_NEW_ACCOUNT50); break;
        case 6:
            help(0, UPDATE_SCREEN_ON_CREATE_NEW_ACCOUNT52);
            help(1, UPDATE_SCREEN_ON_CREATE_NEW_ACCOUNT53);
            break;
        case 7:
            help(0, UPDATE_SCREEN_ON_CREATE_NEW_ACCOUNT56);
            help(1, UPDATE_SCREEN_ON_CREATE_NEW_ACCOUNT57);
            break;
        case 9:
            help(0, UPDATE_SCREEN_ON_CREATE_NEW_ACCOUNT63);
            help(1, UPDATE_SCREEN_ON_CREATE_NEW_ACCOUNT64);
            help(2, UPDATE_SCREEN_ON_CREATE_NEW_ACCOUNT65);
            break;
        }
        break;
    }
    case BTN_CLEAR:
        help(0, UPDATE_SCREEN_ON_CREATE_NEW_ACCOUNT80);
        break;
    case BTN_CANCEL:
        help(0, UPDATE_SCREEN_ON_CREATE_NEW_ACCOUNT81);
        break;
    }

    draw_version();
    draw_copyright();
}
