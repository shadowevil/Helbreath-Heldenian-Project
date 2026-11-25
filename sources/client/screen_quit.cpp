// screen_quit.cpp: Quit Screen Implementation
//
//////////////////////////////////////////////////////////////////////

#include "screen_quit.h"
#include "game.h"
#include "game_mode_manager.h"
#include "common_types.h"
#include "input.h"
#include "sprite_id.h"
using namespace hb::client::sprite_id;


namespace MouseButton = hb::shared::input::MouseButton;

Screen_Quit::Screen_Quit(CGame* game)
    : IGameScreen(game)
{
}

void Screen_Quit::on_initialize()
{
    m_dwStartTime = GameClock::get_time_ms();

    // Close game socket
    if (m_game->m_g_sock != nullptr)
    {
        m_game->m_g_sock.reset();
    }
}
void Screen_Quit::on_update()
{
    uint32_t elapsed = GameClock::get_time_ms() - m_dwStartTime;

    // After 3 seconds, allow input to skip
    if (elapsed >= INPUT_ACTIVE_MS)
    {
        // Handle escape/enter to quit
        if (hb::shared::input::is_key_pressed(KeyCode::Escape) || hb::shared::input::is_key_pressed(KeyCode::Enter))
        {
            m_game->change_game_mode(GameMode::Null);
            hb::shared::render::Window::close();
            return;
        }

        // Check for mouse click
        if (hb::shared::input::is_mouse_button_pressed(MouseButton::Left))
        {
            m_game->change_game_mode(GameMode::Null);
            hb::shared::render::Window::close();
            return;
        }
    }

    // Auto-quit after 10 seconds
    if (elapsed >= AUTO_QUIT_MS)
    {
        m_game->change_game_mode(GameMode::Null);
        hb::shared::render::Window::close();
        return;
    }
}

void Screen_Quit::on_render()
{
    draw_new_dialog_box(InterfaceNdQuit, 0, 0, 0, true);

    // Fade in the quit dialog over 500ms
    uint32_t elapsed = GameClock::get_time_ms() - m_dwStartTime;
    if (elapsed >= FADE_IN_MS)
    {
        draw_new_dialog_box(InterfaceNdQuit, 335, 183, 1, true);
    }
    else
    {
        float alpha = static_cast<float>(elapsed) / static_cast<float>(FADE_IN_MS);
        m_game->m_sprite[InterfaceNdQuit]->draw(335, 183, 1, hb::shared::sprite::DrawParams::alpha_blend(alpha));
    }

    draw_version();
    draw_copyright();
}
