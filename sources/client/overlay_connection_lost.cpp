// overlay_connection_lost.cpp: Connection lost notification overlay
//
//////////////////////////////////////////////////////////////////////

#include "overlay_connection_lost.h"
#include "game.h"
#include "game_mode_manager.h"
#include "common_types.h"
#include "lan_eng.h"
#include "audio_manager.h"
#include "text_lib_ext.h"
#include "game_fonts.h"
using namespace hb::client::sprite_id;

Overlay_ConnectionLost::Overlay_ConnectionLost(CGame* game)
    : IGameScreen(game)
{
}

void Overlay_ConnectionLost::on_initialize()
{
    m_dwStartTime = GameClock::get_time_ms();
    m_iFrameCount = 0;

    // stop sounds
    audio_manager::get().stop_sound(sound_type::effect, 38);
    audio_manager::get().stop_music();
}
void Overlay_ConnectionLost::on_update()
{
    m_iFrameCount++;
    if (m_iFrameCount > 100) m_iFrameCount = 100;

    // Auto-transition to MainMenu after 5 seconds
    uint32_t elapsed = GameClock::get_time_ms() - m_dwStartTime;
    if (elapsed > 5000)
    {
        m_game->change_game_mode(GameMode::MainMenu);
    }
}

void Overlay_ConnectionLost::on_render()
{
    int dlgX, dlgY;
    draw_centered_dialog_box(InterfaceNdGame4, 2, dlgX, dlgY);
    hb::shared::text::draw_text(GameFont::Bitmap1, dlgX + 64, dlgY + 55, "Connection Lost!", hb::shared::text::TextStyle::with_highlight(GameColors::UIDarkRed));
    put_string(dlgX + 60, dlgY + 85, UPDATE_SCREEN_ON_CONNECTION_LOST, GameColors::UIBlack);
    draw_version();
}
