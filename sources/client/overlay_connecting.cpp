// overlay_connecting.cpp: "Connecting to Server..." progress overlay
//
//////////////////////////////////////////////////////////////////////

#include "overlay_connecting.h"
#include "game.h"
#include "game_mode_manager.h"
#include "common_types.h"
#include "lan_eng.h"
#include "input.h"
#include "text_lib_ext.h"
#include "game_fonts.h"
#include <format>
#include <string>
using namespace hb::client::sprite_id;

Overlay_Connecting::Overlay_Connecting(CGame* game)
    : IGameScreen(game)
{
}

void Overlay_Connecting::on_initialize()
{
    m_dwStartTime = GameClock::get_time_ms();
    m_dwAnimTime = m_dwStartTime;
}
void Overlay_Connecting::on_update()
{
    uint32_t time = GameClock::get_time_ms();
    uint32_t elapsed = time - m_dwStartTime;

    // ESC key cancels connection (only after 7 seconds when hint is shown)
    if (hb::shared::input::is_key_pressed(KeyCode::Escape))
    {
        if (elapsed > 7000)
        {
            // Close sockets
            if (m_game->m_l_sock != nullptr)
            {
                m_game->m_l_sock.reset();
            }
            if (m_game->m_g_sock != nullptr)
            {
                m_game->m_g_sock.reset();
            }

            m_game->m_is_server_changing = false;

            // clear overlay - base screen (Login, SelectCharacter, etc.) will be revealed
            clear_overlay();
            return;
        }
    }

    // Animation frame updates (for cursor/menu animations)
    if ((time - m_dwAnimTime) > 100)
    {
        m_game->m_menu_frame++;
        m_dwAnimTime = time;
    }
    if (m_game->m_menu_frame >= 8)
    {
        m_game->m_menu_dir_cnt++;
        if (m_game->m_menu_dir_cnt > 8)
        {
            m_game->m_menu_dir++;
            m_game->m_menu_dir_cnt = 1;
        }
        if (m_game->m_menu_dir > direction::northwest) m_game->m_menu_dir = direction::north;
        m_game->m_menu_frame = 0;
    }
}

void Overlay_Connecting::on_render()
{
    uint32_t time = GameClock::get_time_ms();
    uint32_t elapsed = time - m_dwStartTime;

    int dlgX, dlgY;
    // countdown heading
    const std::string txt = std::format("Connecting to Server... {:3}Sec", elapsed / 1000);
    draw_centered_notice_frame(dlgX, dlgY, txt, 72);

    // Show appropriate message based on elapsed time
    if (elapsed > 7000)
    {
        // Show ESC hint after 7 seconds
        put_aligned_string(dlgX + 18, dlgX + 301, dlgY + 100, UPDATE_SCREEN_ON_CONNECTING1);
        put_aligned_string(dlgX + 18, dlgX + 301, dlgY + 115, UPDATE_SCREEN_ON_CONNECTING2);
    }
    else
    {
        put_aligned_string(dlgX + 18, dlgX + 301, dlgY + 100, UPDATE_SCREEN_ON_CONNECTING3);
    }

    draw_version();
}
