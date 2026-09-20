// Screen_Splash.h: Splash Screen Interface
//
// Displays a splash screen before loading. Transitions to loading
// screen after 10 seconds. Shows contributors one at a time with fade effects.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "IGameScreen.h"
#include "CControls.h"
#include <cstdint>
#include <string>
#include <array>

class Screen_Splash : public IGameScreen
{
public:
    SCREEN_TYPE(Screen_Splash)

    GameMode get_game_mode() const override { return GameMode::Splash; }

    explicit Screen_Splash(CGame* game);
    ~Screen_Splash() override = default;

    void on_initialize() override;
    void on_uninitialize() override;
    void on_update() override;
    void on_render() override;

private:
    static constexpr uint32_t SPLASH_DURATION_MS = 12000;  // 12 seconds total
    static constexpr int NUM_CONTRIBUTORS = 5;
    static constexpr uint32_t TIME_PER_CONTRIBUTOR_MS = 2000;  // 2 seconds each
    static constexpr uint32_t FADE_DURATION_MS = 400;  // 0.4 second fade in/out

    struct Credit
    {
        std::string displayLine;
        std::string url;
    };

    enum control_id
    {
        LBL_CREDIT_NAME_0 = 1,  // through LBL_CREDIT_NAME_0 + NUM_CONTRIBUTORS - 1
        LBL_CREDIT_URL_0 = 10,  // through LBL_CREDIT_URL_0 + NUM_CONTRIBUTORS - 1
    };

    cc::control_collection m_controls;
    std::array<Credit, NUM_CONTRIBUTORS> m_credits;

    float get_contributor_alpha(uint32_t elapsedMs, int contributorIndex) const;
};
