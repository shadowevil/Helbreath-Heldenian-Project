// Overlay_WaitInitData.cpp: Waiting for server init data overlay
//
//////////////////////////////////////////////////////////////////////

#include "Overlay_WaitInitData.h"
#include "Game.h"
#include "GameModeManager.h"
#include "CommonTypes.h"
#include "lan_eng.h"
#include "IInput.h"
#include "TextLibExt.h"
#include "GameFonts.h"
#include <format>
using namespace hb::client::sprite_id;

Overlay_WaitInitData::Overlay_WaitInitData(CGame* game)
    : IGameScreen(game)
{
}

void Overlay_WaitInitData::on_initialize()
{
    m_dwStartTime = GameClock::get_time_ms();
    m_iFrameCount = 0;
}
void Overlay_WaitInitData::on_update()
{
    uint32_t time = GameClock::get_time_ms();
    uint32_t elapsed = time - m_dwStartTime;

    m_iFrameCount++;
    if (m_iFrameCount > 100) m_iFrameCount = 100;

    // ESC key returns to MainMenu (only after 7 seconds)
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

            m_game->change_game_mode(GameMode::MainMenu);
            return;
        }
    }
}

void Overlay_WaitInitData::on_render()
{
    std::string G_cTxt;
    uint32_t time = GameClock::get_time_ms();
    uint32_t elapsed = time - m_dwStartTime;

    if (elapsed >= 3000)
    {
        int dlgX, dlgY;
        draw_centered_dialog_box(InterfaceNdGame4, 2, dlgX, dlgY);

        G_cTxt = std::format("Waiting for response... {}sec", elapsed / 1000);
        hb::shared::text::draw_text(GameFont::Bitmap1, dlgX + 54, dlgY + 65, G_cTxt.c_str(), hb::shared::text::TextStyle::with_highlight(GameColors::UIDarkRed));

        if (elapsed > 7000)
        {
            put_aligned_string(dlgX + 12, dlgX + 305, dlgY + 95, UPDATE_SCREEN_ON_WAIT_INIT_DATA1);
            put_aligned_string(dlgX + 12, dlgX + 305, dlgY + 110, UPDATE_SCREEN_ON_WAIT_INIT_DATA2);
        }
        else
        {
            put_aligned_string(dlgX + 12, dlgX + 305, dlgY + 100, UPDATE_SCREEN_ON_WAIT_INIT_DATA3);
        }

        draw_version();
    }
}
