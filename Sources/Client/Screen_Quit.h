// Screen_Quit.h: Quit Screen Interface
//
// Exit confirmation screen with 10-second auto-quit.
// Input is ignored for first 3 seconds to prevent accidental exit.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "IGameScreen.h"

class Screen_Quit : public IGameScreen
{
public:
    SCREEN_TYPE(Screen_Quit)

    GameMode get_game_mode() const override { return GameMode::Quit; }

    explicit Screen_Quit(CGame* game);
    ~Screen_Quit() override = default;

    void on_initialize() override;
    void on_update() override;
    void on_render() override;

private:
    uint32_t m_dwStartTime = 0;

    // Timing constants (milliseconds)
    static constexpr uint32_t FADE_IN_MS = 500;         // Fade in duration
    static constexpr uint32_t INPUT_ACTIVE_MS = 3000;   // 3 seconds before input is active
    static constexpr uint32_t AUTO_QUIT_MS = 10000;     // 10 seconds total
};
