// Screen_MainMenu.cpp: Main Menu Screen implementation
//
//////////////////////////////////////////////////////////////////////

#include "Screen_MainMenu.h"
#include "Game.h"
#include "GameModeManager.h"
#include "IInput.h"
#include "GlobalDef.h"
#include "AudioManager.h"
using namespace hb::client::sprite_id;

Screen_MainMenu::Screen_MainMenu(CGame* game)
    : IGameScreen(game)
{
}

void Screen_MainMenu::on_initialize()
{
    m_game->m_sprite.remove(InterfaceNdLoading);
    m_game->m_arrow_pressed = 0;

    m_controls.set_screen_size(LOGICAL_WIDTH(), LOGICAL_HEIGHT());
    m_controls.set_hover_focus(false);

    auto make_renderer = [this](int focused_frame) {
        return [this, focused_frame](const cc::control& c) {
            if (c.is_highlighted())
            {
                auto sb = c.screen_bounds();
                m_game->m_sprite[InterfaceNdMainMenu]->draw(sb.x, sb.y, focused_frame);
            }
        };
    };

    auto* btn_login = m_controls.add<cc::button>(BTN_LOGIN, cc::rect{465, 238, 164, 22});
    btn_login->set_on_click([this](int) { m_game->change_game_mode(GameMode::Login); });
    btn_login->set_click_sound([this] { audio_manager::get().play_game_sound(sound_type::effect, 14, 5); });
    btn_login->set_render_handler(make_renderer(1));

    auto* btn_account = m_controls.add<cc::button>(BTN_NEW_ACCOUNT, cc::rect{465, 276, 164, 22});
    btn_account->set_on_click([this](int) { m_game->change_game_mode(GameMode::CreateNewAccount); });
    btn_account->set_click_sound([this] { audio_manager::get().play_game_sound(sound_type::effect, 14, 5); });
    btn_account->set_render_handler(make_renderer(2));

    auto* btn_quit = m_controls.add<cc::button>(BTN_QUIT, cc::rect{465, 315, 164, 22});
    btn_quit->set_on_click([this](int) { m_game->change_game_mode(GameMode::Quit); });
    btn_quit->set_click_sound([this] { audio_manager::get().play_game_sound(sound_type::effect, 14, 5); });
    btn_quit->set_render_handler(make_renderer(3));

    m_controls.set_focus_order({BTN_LOGIN, BTN_NEW_ACCOUNT, BTN_QUIT});
    m_controls.set_focus(BTN_LOGIN);
}
void Screen_MainMenu::on_update()
{
    update_controls(m_controls);
}

void Screen_MainMenu::on_render()
{
    draw_new_dialog_box(InterfaceNdMainMenu, 0, 0, 0, true);
    m_controls.render();
    draw_version();
}
