// Screen_MainMenu.h: Main Menu Screen
//
// Handles the initial menu (Login, New Account, Quit)
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "IGameScreen.h"
#include "CControls.h"

class Screen_MainMenu : public IGameScreen
{
public:
    SCREEN_TYPE(Screen_MainMenu)

    GameMode get_game_mode() const override { return GameMode::MainMenu; }

    explicit Screen_MainMenu(CGame* game);
    ~Screen_MainMenu() override = default;

    void on_initialize() override;
    void on_update() override;
    void on_render() override;

private:
    enum control_id { BTN_LOGIN = 1, BTN_NEW_ACCOUNT = 2, BTN_QUIT = 3 };

    cc::control_collection m_controls;
};
