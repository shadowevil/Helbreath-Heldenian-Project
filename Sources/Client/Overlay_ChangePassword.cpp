// Overlay_ChangePassword.cpp: Password change overlay
//
//////////////////////////////////////////////////////////////////////

#include "Overlay_ChangePassword.h"
#include "Game.h"
#include "GameModeManager.h"
#include "CommonTypes.h"
#include "lan_eng.h"
#include "IInput.h"
#include "ASIOSocket.h"
#include "Misc.h"
#include "GameFonts.h"
#include "TextLibExt.h"
#include "TextFieldRenderer.h"
#include "Packet/SharedPackets.h"
#include "AudioManager.h"
#include <string>

using namespace hb::shared::net;
using namespace hb::client::sprite_id;

Overlay_ChangePassword::Overlay_ChangePassword(CGame* game)
    : IGameScreen(game)
{
}

void Overlay_ChangePassword::on_initialize()
{
    m_error_msg.clear();
    m_game->m_arrow_pressed = 0;

    int dlgX, dlgY;
    get_centered_dialog_pos(InterfaceNdGame4, 0, dlgX, dlgY);

    m_controls.set_screen_size(LOGICAL_WIDTH(), LOGICAL_HEIGHT());
    m_controls.set_hover_focus(false);
    m_controls.set_enter_advances_focus(true);
    m_controls.set_default_button(BTN_OK);

    m_controls.set_clipboard_provider(
        [] { return hb::shared::input::get_clipboard_text(); },
        [](const std::string& s) { hb::shared::input::set_clipboard_text(s); }
    );

    // Textbox render helper
    auto textbox_renderer = [](const cc::control& c) {
        auto& field = static_cast<const cc::text_input&>(c);
        hb::client::draw_text_field(field, GameClock::get_time_ms(),
            hb::shared::text::TextStyle::with_shadow(GameColors::InputValid),
            hb::shared::text::TextStyle::with_shadow(GameColors::InputInvalid));
    };

    // === Textboxes ===
    // Account name is displayed as static text in on_render() — not editable.
    // handle_submit() reads directly from m_game->m_account_name.

    auto* tb_old_pw = m_controls.add<cc::textbox>(TXT_OLD_PW, cc::rect{dlgX + 161, dlgY + 67, 125, 17}, 11);
    tb_old_pw->set_hidden(true);
    tb_old_pw->set_character_filter(hb::client::password_allowed_chars);
    tb_old_pw->set_validator([](const std::string& s) {
        return !s.empty() && CMisc::check_valid_string(s.c_str());
    });
    tb_old_pw->set_render_handler(textbox_renderer);

    auto* tb_new_pw = m_controls.add<cc::textbox>(TXT_NEW_PW, cc::rect{dlgX + 161, dlgY + 91, 125, 17}, 11);
    tb_new_pw->set_hidden(true);
    tb_new_pw->set_character_filter(hb::client::password_allowed_chars);
    tb_new_pw->set_validator([](const std::string& s) {
        return s.size() >= 8 && CMisc::check_valid_string(s.c_str());
    });
    tb_new_pw->set_render_handler(textbox_renderer);

    auto* tb_confirm = m_controls.add<cc::textbox>(TXT_CONFIRM_PW, cc::rect{dlgX + 161, dlgY + 115, 125, 17}, 11);
    tb_confirm->set_hidden(true);
    tb_confirm->set_character_filter(hb::client::password_allowed_chars);
    tb_confirm->set_validator([this](const std::string& s) {
        auto* pw = m_controls.find_as<cc::textbox>(TXT_NEW_PW);
        return !s.empty() && pw && s == pw->text();
    });
    tb_confirm->set_render_handler(textbox_renderer);

    // === Buttons ===
    auto* btn_ok = m_controls.add<cc::button>(BTN_OK, cc::rect{dlgX + 44, dlgY + 208, ui_layout::btn_size_x, ui_layout::btn_size_y});
    btn_ok->set_click_sound([this] { audio_manager::get().play_game_sound(sound_type::effect, 14, 5); });
    btn_ok->set_on_click([this](int) {
        handle_submit();
    });
    btn_ok->set_render_handler([this](const cc::control& c) {
        auto sb = c.screen_bounds();
        // Only highlight when all fields are valid and passwords match
        auto* tb_old = m_controls.find_as<cc::textbox>(TXT_OLD_PW);
        auto* tb_new = m_controls.find_as<cc::textbox>(TXT_NEW_PW);
        auto* tb_conf = m_controls.find_as<cc::textbox>(TXT_CONFIRM_PW);
        bool all_valid = tb_old && tb_old->is_valid()
                      && tb_new && tb_new->is_valid()
                      && tb_conf && tb_conf->is_valid()
                      && tb_old->text() != tb_new->text();
        int frame = (all_valid && c.is_highlighted()) ? 21 : 20;
        m_game->m_sprite[InterfaceNdButton]->draw(sb.x, sb.y, frame);
    });

    auto* btn_cancel = m_controls.add<cc::button>(BTN_CANCEL, cc::rect{dlgX + 217, dlgY + 208, ui_layout::btn_size_x, ui_layout::btn_size_y});
    btn_cancel->set_click_sound([this] { audio_manager::get().play_game_sound(sound_type::effect, 14, 5); });
    btn_cancel->set_on_click([this](int) {
        clear_overlay();
    });
    btn_cancel->set_render_handler([this](const cc::control& c) {
        auto sb = c.screen_bounds();
        int frame = c.is_highlighted() ? 17 : 16;
        m_game->m_sprite[InterfaceNdButton]->draw(sb.x, sb.y, frame);
    });

    m_controls.set_focus_order({TXT_OLD_PW, TXT_NEW_PW, TXT_CONFIRM_PW, BTN_OK, BTN_CANCEL});
    m_controls.set_focus(TXT_OLD_PW);

    discard_pending_controls_input(m_controls);
}
void Overlay_ChangePassword::handle_submit()
{
    auto* tb_old = m_controls.find_as<cc::textbox>(TXT_OLD_PW);
    auto* tb_new = m_controls.find_as<cc::textbox>(TXT_NEW_PW);
    auto* tb_conf = m_controls.find_as<cc::textbox>(TXT_CONFIRM_PW);

    const auto& account_name = m_game->m_account_name;
    if (account_name.empty())
    {
        m_error_msg = "No account loaded.";
        return;
    }
    if (tb_old->text().empty() || !CMisc::check_valid_string(tb_old->text().c_str()))
    {
        m_error_msg = "Please enter your current password.";
        return;
    }
    if (tb_new->text().size() < 8)
    {
        m_error_msg = "New password must be at least 8 characters.";
        return;
    }
    if (!CMisc::check_valid_string(tb_new->text().c_str()))
    {
        m_error_msg = "New password contains invalid characters.";
        return;
    }
    if (tb_new->text() != tb_conf->text())
    {
        m_error_msg = "Password confirmation does not match.";
        return;
    }
    if (tb_old->text() == tb_new->text())
    {
        m_error_msg = "New password must be different from current.";
        return;
    }
    m_error_msg.clear();

    // Store current password for verification
    m_game->m_account_password = tb_old->text();

    // Build ChangePasswordRequest packet
    hb::net::ChangePasswordRequest req{};
    req.header.msg_id = MsgId::RequestChangePassword;
    req.header.msg_type = 0;
    std::snprintf(req.account_name, sizeof(req.account_name), "%s", account_name.c_str());
    std::snprintf(req.password, sizeof(req.password), "%s", tb_old->text().c_str());
    std::snprintf(req.new_password, sizeof(req.new_password), "%s", tb_new->text().c_str());
    std::snprintf(req.new_password_confirm, sizeof(req.new_password_confirm), "%s", tb_conf->text().c_str());

    // Store packet for sending after connection completes
    auto* p = reinterpret_cast<char*>(&req);
    m_game->m_pending_login_packet.assign(p, p + sizeof(req));

    // Create connection
    m_game->m_l_sock = std::make_unique<hb::shared::net::ASIOSocket>(m_game->m_io_pool->get_context(), game_limits::socket_block_limit);
    m_game->m_l_sock->connect(m_game->m_log_server_addr.c_str(), m_game->m_log_server_port);
    m_game->m_l_sock->init_buffer_size(hb::shared::limits::MsgBufferSize);

    std::snprintf(m_game->m_msg, sizeof(m_game->m_msg), "%s", "41");

    m_game->change_game_mode(GameMode::Connecting);
}

void Overlay_ChangePassword::on_update()
{
    update_controls(m_controls);

    // Clear error when focus changes to a text field
    int focused = m_controls.focused_id();
    if (focused != m_prev_focused)
    {
        if (focused >= TXT_OLD_PW && focused <= TXT_CONFIRM_PW)
            m_error_msg.clear();
        m_prev_focused = focused;
    }

    if (m_controls.escape_pressed())
    {
        m_game->change_game_mode(GameMode::MainMenu);
        return;
    }
}

void Overlay_ChangePassword::on_render()
{
    int dlgX, dlgY;
    draw_centered_dialog_box(InterfaceNdGame4, 0, dlgX, dlgY);
    draw_new_dialog_box(InterfaceNdText, dlgX, dlgY, 13);
    draw_new_dialog_box(InterfaceNdGame4, dlgX + 157, dlgY + 109, 7);

    // draw labels
    put_string(dlgX + 53, dlgY + 43, UPDATE_SCREEN_ON_CHANGE_PASSWORD1, GameColors::UILabel);
    put_string(dlgX + 161, dlgY + 43, m_game->m_account_name.c_str(), GameColors::InputValid);
    put_string(dlgX + 53, dlgY + 67, UPDATE_SCREEN_ON_CHANGE_PASSWORD2, GameColors::UILabel);
    put_string(dlgX + 53, dlgY + 91, UPDATE_SCREEN_ON_CHANGE_PASSWORD3, GameColors::UILabel);
    put_string(dlgX + 53, dlgY + 115, UPDATE_SCREEN_ON_CHANGE_PASSWORD4, GameColors::UILabel);

    // Controls (textboxes + buttons)
    m_controls.render();

    // Help text or error message
    if (!m_error_msg.empty())
    {
        hb::shared::text::draw_text_aligned(GameFont::Default, dlgX, dlgY + 146, 334, 15, m_error_msg.c_str(),
            hb::shared::text::TextStyle::from_color({195, 25, 25}), hb::shared::text::Align::TopCenter);
    }
    else
    {
        put_aligned_string(dlgX, dlgX + 334, dlgY + 146, UPDATE_SCREEN_ON_CHANGE_PASSWORD5);
        put_aligned_string(dlgX, dlgX + 334, dlgY + 161, UPDATE_SCREEN_ON_CHANGE_PASSWORD6);
        put_aligned_string(dlgX, dlgX + 334, dlgY + 176, UPDATE_SCREEN_ON_CHANGE_PASSWORD7);
    }

    draw_version();
}
