// Overlay_Msg.cpp: Simple message display overlay
//
//////////////////////////////////////////////////////////////////////

#include "Overlay_Msg.h"
#include "Game.h"
#include "GameModeManager.h"
#include "CommonTypes.h"
#include "GameFonts.h"
#include "TextLibExt.h"

Overlay_Msg::Overlay_Msg(CGame* game)
    : IGameScreen(game)
{
}

void Overlay_Msg::on_initialize()
{
    m_dwStartTime = GameClock::get_time_ms();
}
void Overlay_Msg::on_update()
{
    // Auto-transition to MainMenu after 1.5 seconds
    uint32_t elapsed = GameClock::get_time_ms() - m_dwStartTime;
    if (elapsed > 1500)
    {
        m_game->change_game_mode(GameMode::MainMenu);
    }
}

void Overlay_Msg::on_render()
{
    hb::shared::text::draw_text(GameFont::Default, 10, 10, m_game->m_msg,
                      hb::shared::text::TextStyle::with_shadow(GameColors::UIWarningRed));
    draw_version();
}
