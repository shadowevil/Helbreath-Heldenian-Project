// screen_main_menu.h: Main Menu Screen
//
// Handles the initial menu (Login, New Account, Quit)
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "game_screen.h"
#include "ccontrols.h"

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

#ifdef DEF_CLAUDE_TESTING
    cc::control_collection* agent_controls() override { return &m_controls; }
    std::span<const agent_control_name> agent_control_names() const override
    {
        static constexpr agent_control_name names[] = {
            {BTN_LOGIN, "main_menu.login"},
            {BTN_NEW_ACCOUNT, "main_menu.new_account"},
            {BTN_QUIT, "main_menu.quit"},
        };
        return names;
    }
#endif

private:
    enum control_id { BTN_LOGIN = 1, BTN_NEW_ACCOUNT = 2, BTN_QUIT = 3 };

    cc::control_collection m_controls;
};
