#include "Screen_CreateAccount.h"
#include "Game.h"
#include "GameModeManager.h"
#include "IInput.h"
#include "GlobalDef.h"
#include "ASIOSocket.h"
#include "Misc.h"
#include "lan_eng.h"
#include "GameFonts.h"
#include "TextLibExt.h"
#include "TextFieldRenderer.h"
#include "Packet/SharedPackets.h"
#include "AudioManager.h"

using namespace hb::shared::net;
using namespace hb::client::sprite_id;

Screen_CreateAccount::Screen_CreateAccount(CGame* game)
    : IGameScreen(game)
{
}

void Screen_CreateAccount::on_initialize()
{
    m_game->m_arrow_pressed = 0;

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
    auto* tb_account = m_controls.add<cc::textbox>(TXT_ACCOUNT, cc::rect{340, 210, 250, 18}, 11);
    tb_account->set_character_filter(hb::client::username_allowed_chars);
    tb_account->set_validator([](const std::string& s) {
        return !s.empty() && CMisc::check_valid_name(s.c_str());
    });
    tb_account->set_render_handler(textbox_renderer);

    auto* tb_password = m_controls.add<cc::textbox>(TXT_PASSWORD, cc::rect{340, 232, 250, 18}, 11);
    tb_password->set_hidden(true);
    tb_password->set_character_filter(hb::client::password_allowed_chars);
    tb_password->set_validator([](const std::string& s) {
        return !s.empty() && CMisc::check_valid_name(s.c_str());
    });
    tb_password->set_render_handler(textbox_renderer);

    auto* tb_confirm = m_controls.add<cc::textbox>(TXT_CONFIRM, cc::rect{340, 254, 250, 18}, 11);
    tb_confirm->set_hidden(true);
    tb_confirm->set_character_filter(hb::client::password_allowed_chars);
    tb_confirm->set_validator([this](const std::string& s) {
        auto* pw = m_controls.find_as<cc::textbox>(TXT_PASSWORD);
        return !s.empty() && pw && s == pw->text();
    });
    tb_confirm->set_render_handler(textbox_renderer);

    auto* tb_email = m_controls.add<cc::textbox>(TXT_EMAIL, cc::rect{340, 276, 250, 18}, 49);
    tb_email->set_character_filter(hb::client::email_allowed_chars);
    tb_email->set_validator([](const std::string& s) {
        return !s.empty() && CMisc::is_valid_email(s.c_str());
    });
    tb_email->set_render_handler(textbox_renderer);

    // === Buttons ===
    auto* btn_create = m_controls.add<cc::button>(BTN_CREATE, cc::rect{297, 398, 72, 20});
    btn_create->set_on_click([this](int) { submit_create_account(); });
    btn_create->set_click_sound([this] { audio_manager::get().play_game_sound(sound_type::effect, 14, 5); });
    btn_create->set_render_handler([this](const cc::control& c) {
        auto sb = c.screen_bounds();
        // Only highlight when all fields are valid
        bool all_valid = true;
        for (int id = TXT_ACCOUNT; id <= TXT_EMAIL; id++)
        {
            auto* tb = m_controls.find_as<cc::textbox>(id);
            if (tb && !tb->is_valid()) { all_valid = false; break; }
        }
        int frame = (all_valid && c.is_highlighted()) ? 25 : 24;
        m_game->m_sprite[InterfaceNdButton]->draw(sb.x, sb.y, frame);
    });

    auto* btn_clear = m_controls.add<cc::button>(BTN_CLEAR, cc::rect{392, 398, 72, 20});
    btn_clear->set_click_sound([this] { audio_manager::get().play_game_sound(sound_type::effect, 14, 5); });
    btn_clear->set_on_click([this](int) {
        for (int id = TXT_ACCOUNT; id <= TXT_EMAIL; id++)
        {
            auto* tb = m_controls.find_as<cc::textbox>(id);
            if (tb) tb->text().clear();
        }
        m_controls.set_focus(TXT_ACCOUNT);
    });
    btn_clear->set_render_handler([this](const cc::control& c) {
        auto sb = c.screen_bounds();
        int frame = c.is_highlighted() ? 27 : 26;
        m_game->m_sprite[InterfaceNdButton]->draw(sb.x, sb.y, frame);
    });

    auto* btn_cancel = m_controls.add<cc::button>(BTN_CANCEL, cc::rect{488, 398, 72, 20});
    btn_cancel->set_on_click([this](int) { m_game->change_game_mode(GameMode::MainMenu); });
    btn_cancel->set_click_sound([this] { audio_manager::get().play_game_sound(sound_type::effect, 14, 5); });
    btn_cancel->set_render_handler([this](const cc::control& c) {
        auto sb = c.screen_bounds();
        int frame = c.is_highlighted() ? 17 : 16;
        m_game->m_sprite[InterfaceNdButton]->draw(sb.x, sb.y, frame);
    });

    m_controls.set_focus_order({TXT_ACCOUNT, TXT_PASSWORD, TXT_CONFIRM, TXT_EMAIL,
                                BTN_CREATE, BTN_CLEAR, BTN_CANCEL});
    m_controls.set_focus(TXT_ACCOUNT);
}
void Screen_CreateAccount::on_update()
{
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
    auto label_style = hb::shared::text::TextStyle::from_color(GameColors::UINearWhite).with_bold();
    auto help_style = hb::shared::text::TextStyle::from_color(hb::shared::render::Color(255, 181, 0));

    // Background
    draw_new_dialog_box(InterfaceNdNewAccount, 0, 0, 0, true);

    // Backdrop panel behind input fields
    m_game->m_Renderer->draw_rounded_rect_filled(258, 198, 344, 108, 8,
        hb::shared::render::Color::Black(200));

    // Field labels
    hb::shared::text::draw_text(GameFont::Default, 270, 210, "Account:", label_style);
    hb::shared::text::draw_text(GameFont::Default, 270, 232, "Password:", label_style);
    hb::shared::text::draw_text(GameFont::Default, 270, 254, "Confirm:", label_style);
    hb::shared::text::draw_text(GameFont::Default, 270, 276, "Email:", label_style);

    // Controls (textbox values + buttons)
    m_controls.render();

    // Help text based on focused control
    int focused = m_controls.focused_id();
    constexpr int hx = 290;
    constexpr int hw = 575 - 290;
    auto help = [&](int y, const char* text) {
        hb::shared::text::draw_text_aligned(GameFont::Default, hx, y, hw, 15,
            text, help_style, hb::shared::text::Align::TopCenter);
    };

    switch (focused)
    {
    case TXT_ACCOUNT:
        help(330, UPDATE_SCREEN_ON_CREATE_NEW_ACCOUNT1);
        help(345, UPDATE_SCREEN_ON_CREATE_NEW_ACCOUNT2);
        break;
    case TXT_PASSWORD:
        help(330, UPDATE_SCREEN_ON_CREATE_NEW_ACCOUNT4);
        break;
    case TXT_CONFIRM:
        help(330, UPDATE_SCREEN_ON_CREATE_NEW_ACCOUNT8);
        break;
    case TXT_EMAIL:
        help(330, UPDATE_SCREEN_ON_CREATE_NEW_ACCOUNT21);
        help(345, UPDATE_SCREEN_ON_CREATE_NEW_ACCOUNT22);
        help(360, UPDATE_SCREEN_ON_CREATE_NEW_ACCOUNT23);
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
        case 0: help(330, UPDATE_SCREEN_ON_CREATE_NEW_ACCOUNT33); break;
        case 1: help(330, UPDATE_SCREEN_ON_CREATE_NEW_ACCOUNT35); break;
        case 2: help(330, UPDATE_SCREEN_ON_CREATE_NEW_ACCOUNT38); break;
        case 3: help(330, UPDATE_SCREEN_ON_CREATE_NEW_ACCOUNT42); break;
        case 5: help(330, UPDATE_SCREEN_ON_CREATE_NEW_ACCOUNT50); break;
        case 6:
            help(330, UPDATE_SCREEN_ON_CREATE_NEW_ACCOUNT52);
            help(345, UPDATE_SCREEN_ON_CREATE_NEW_ACCOUNT53);
            break;
        case 7:
            help(330, UPDATE_SCREEN_ON_CREATE_NEW_ACCOUNT56);
            help(345, UPDATE_SCREEN_ON_CREATE_NEW_ACCOUNT57);
            break;
        case 9:
            help(330, UPDATE_SCREEN_ON_CREATE_NEW_ACCOUNT63);
            help(345, UPDATE_SCREEN_ON_CREATE_NEW_ACCOUNT64);
            help(360, UPDATE_SCREEN_ON_CREATE_NEW_ACCOUNT65);
            break;
        }
        break;
    }
    case BTN_CLEAR:
        help(330, UPDATE_SCREEN_ON_CREATE_NEW_ACCOUNT80);
        break;
    case BTN_CANCEL:
        help(330, UPDATE_SCREEN_ON_CREATE_NEW_ACCOUNT81);
        break;
    }

    draw_version();
}
