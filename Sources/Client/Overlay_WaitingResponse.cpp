// Overlay_WaitingResponse.cpp: "Connected. Waiting for response..." progress overlay
//
//////////////////////////////////////////////////////////////////////

#include "Overlay_WaitingResponse.h"
#include "Game.h"
#include "GameModeManager.h"
#include "CommonTypes.h"
#include "lan_eng.h"
#include "IInput.h"
#include "TextLibExt.h"
#include "GameFonts.h"
using namespace hb::client::sprite_id;

Overlay_WaitingResponse::Overlay_WaitingResponse(CGame* game)
    : IGameScreen(game)
{
}

void Overlay_WaitingResponse::on_initialize()
{
    m_dwStartTime = GameClock::get_time_ms();
    m_dwAnimTime = m_dwStartTime;
}
void Overlay_WaitingResponse::on_update()
{
    uint32_t time = GameClock::get_time_ms();
    uint32_t elapsed = time - m_dwStartTime;

    // ESC key cancels (only after 7 seconds to wait for response)
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

            // clear overlay - base screen will be revealed
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

void Overlay_WaitingResponse::on_render()
{
    uint32_t time = GameClock::get_time_ms();
    uint32_t elapsed = time - m_dwStartTime;

    int dlgX, dlgY;
    draw_centered_dialog_box(InterfaceNdGame4, 2, dlgX, dlgY);

    // draw status text
    hb::shared::text::draw_text(GameFont::Bitmap1, dlgX + 37, dlgY + 65, "Connected. Waiting for response...", hb::shared::text::TextStyle::with_highlight(GameColors::UIDarkRed));

    // Show appropriate message based on elapsed time
    if (elapsed > 7000)
    {
        // Show ESC hint after 7 seconds
        put_aligned_string(dlgX + 18, dlgX + 301, dlgY + 100, UPDATE_SCREEN_ON_WATING_RESPONSE1);
        put_aligned_string(dlgX + 18, dlgX + 301, dlgY + 115, UPDATE_SCREEN_ON_WATING_RESPONSE2);
    }
    else
    {
        put_aligned_string(dlgX + 18, dlgX + 301, dlgY + 100, UPDATE_SCREEN_ON_WATING_RESPONSE3);
    }

    draw_version();
}
