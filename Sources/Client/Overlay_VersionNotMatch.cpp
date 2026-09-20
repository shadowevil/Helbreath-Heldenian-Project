// Overlay_VersionNotMatch.cpp: Version mismatch error overlay
//
//////////////////////////////////////////////////////////////////////

#include "Overlay_VersionNotMatch.h"
#include "Game.h"
#include "GameModeManager.h"
#include "RendererFactory.h"
#include "lan_eng.h"
#include "IInput.h"
#include "AudioManager.h"
using namespace hb::client::sprite_id;

Overlay_VersionNotMatch::Overlay_VersionNotMatch(CGame* game)
    : IGameScreen(game)
{
}

void Overlay_VersionNotMatch::on_initialize()
{
    // Close game socket
    if (m_game->m_g_sock != nullptr)
    {
        m_game->m_g_sock.reset();
    }

    int dlgX, dlgY;
    get_centered_dialog_pos(InterfaceNdGame4, 2, dlgX, dlgY);

    m_controls.set_screen_size(LOGICAL_WIDTH(), LOGICAL_HEIGHT());
    m_controls.set_hover_focus(false);
    m_controls.set_default_button(BTN_OK);

    auto* btn_ok = m_controls.add<cc::button>(BTN_OK, cc::rect{dlgX + 208, dlgY + 119, ui_layout::btn_size_x, ui_layout::btn_size_y});
    btn_ok->set_click_sound([this] { audio_manager::get().play_game_sound(sound_type::effect, 14, 5); });
    btn_ok->set_on_click([this](int) {
        close_app();
    });
    btn_ok->set_render_handler([this](const cc::control& c) {
        auto sb = c.screen_bounds();
        int frame = c.is_highlighted() ? 1 : 0;
        m_game->m_sprite[InterfaceNdButton]->draw(sb.x, sb.y, frame);
    });

    m_controls.set_focus_order({BTN_OK});
    m_controls.set_focus(BTN_OK);

    discard_pending_controls_input(m_controls);
}
void Overlay_VersionNotMatch::close_app()
{
    m_game->change_game_mode(GameMode::Null);
    hb::shared::render::Window::close();
}

void Overlay_VersionNotMatch::on_update()
{
    update_controls(m_controls);

    // ESC also closes app
    if (m_controls.escape_pressed())
    {
        close_app();
        return;
    }
}

void Overlay_VersionNotMatch::on_render()
{
    draw_new_dialog_box(InterfaceNdQuit, 0, 0, 0, true);

    int dlgX, dlgY;
    draw_centered_dialog_box(InterfaceNdGame4, 2, dlgX, dlgY);
    put_aligned_string(dlgX + 6, dlgX + 312, dlgY + 35, UPDATE_SCREEN_ON_VERSION_NO_MATCH1);
    put_aligned_string(dlgX + 6, dlgX + 312, dlgY + 55, UPDATE_SCREEN_ON_VERSION_NO_MATCH2);

    // OK button
    m_controls.render();

    draw_version();
}
