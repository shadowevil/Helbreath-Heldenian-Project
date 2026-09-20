// Overlay_QueryForceLogin.cpp: "Character on Use" confirmation overlay
//
//////////////////////////////////////////////////////////////////////

#include "Overlay_QueryForceLogin.h"
#include "Game.h"
#include "GameModeManager.h"
#include "CommonTypes.h"
#include "lan_eng.h"
#include "IInput.h"
#include "ASIOSocket.h"
#include "TextLibExt.h"
#include "GameFonts.h"
#include "Packet/SharedPackets.h"
#include "AudioManager.h"

using namespace hb::shared::net;
using namespace hb::client::sprite_id;

Overlay_QueryForceLogin::Overlay_QueryForceLogin(CGame* game)
    : IGameScreen(game)
{
}

void Overlay_QueryForceLogin::on_initialize()
{
    m_dwStartTime = GameClock::get_time_ms();

    // Play warning sound
    audio_manager::get().play_game_sound(sound_type::effect, 25, 0);

    int dlgX, dlgY;
    get_centered_dialog_pos(InterfaceNdGame4, 2, dlgX, dlgY);

    m_controls.set_screen_size(LOGICAL_WIDTH(), LOGICAL_HEIGHT());
    m_controls.set_hover_focus(false);

    // Yes button — force disconnect existing session
    auto* btn_yes = m_controls.add<cc::button>(BTN_YES, cc::rect{dlgX + 38, dlgY + 114, ui_layout::btn_size_x, ui_layout::btn_size_y});
    btn_yes->set_click_sound([this] { audio_manager::get().play_game_sound(sound_type::effect, 14, 5); });
    btn_yes->set_on_click([this](int) {
        m_game->m_enter_game_type = EnterGameMsg::NoEnterForceDisconn;

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
        m_game->m_l_sock->connect(m_game->m_log_server_addr.c_str(), m_game->m_log_server_port + (rand() % 1));
        m_game->m_l_sock->init_buffer_size(hb::shared::limits::MsgBufferSize);

        std::snprintf(m_game->m_msg, sizeof(m_game->m_msg), "%s", "33");
        m_game->change_game_mode(GameMode::Connecting);
    });
    btn_yes->set_render_handler([this](const cc::control& c) {
        auto sb = c.screen_bounds();
        int frame = c.is_highlighted() ? 19 : 18;
        m_game->m_sprite[InterfaceNdButton]->draw(sb.x, sb.y, frame);
    });

    // No button — cancel
    auto* btn_no = m_controls.add<cc::button>(BTN_NO, cc::rect{dlgX + 208, dlgY + 114, ui_layout::btn_size_x, ui_layout::btn_size_y});
    btn_no->set_click_sound([this] { audio_manager::get().play_game_sound(sound_type::effect, 14, 5); });
    btn_no->set_on_click([this](int) {
        clear_overlay();
    });
    btn_no->set_render_handler([this](const cc::control& c) {
        auto sb = c.screen_bounds();
        int frame = c.is_highlighted() ? 3 : 2;
        m_game->m_sprite[InterfaceNdButton]->draw(sb.x, sb.y, frame);
    });

    m_controls.set_focus_order({BTN_YES, BTN_NO});
    m_controls.set_focus(BTN_NO);  // Safe default
}
void Overlay_QueryForceLogin::on_update()
{
    update_controls(m_controls);

    if (m_controls.escape_pressed())
    {
        clear_overlay();
        return;
    }
}

void Overlay_QueryForceLogin::on_render()
{
    uint32_t elapsed = GameClock::get_time_ms() - m_dwStartTime;

    // Double shadow effect after initial animation period (600ms)
    if (elapsed >= 600)
    {
        m_game->m_Renderer->draw_rect_filled(0, 0, LOGICAL_MAX_X(), LOGICAL_MAX_Y(), hb::shared::render::Color::Black(128));
    }

    int dlgX, dlgY;
    draw_centered_dialog_box(InterfaceNdGame4, 2, dlgX, dlgY);

    // Title
    hb::shared::text::draw_text(GameFont::Bitmap1, dlgX + 96, dlgY + 30, "Character on Use", hb::shared::text::TextStyle::with_highlight(GameColors::UIDarkRed));

    // Message text
    put_aligned_string(dlgX + 16, dlgX + 291, dlgY + 65, UPDATE_SCREEN_ON_QUERY_FORCE_LOGIN1);
    put_aligned_string(dlgX + 16, dlgX + 291, dlgY + 85, UPDATE_SCREEN_ON_QUERY_FORCE_LOGIN2);

    // Buttons
    m_controls.render();

    draw_version();
}
