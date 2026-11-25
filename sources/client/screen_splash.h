// screen_splash.h: Splash Screen Interface
//
// Displays a splash screen before loading. Transitions to loading
// screen after 10 seconds. Shows contributors one at a time with fade effects.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "game_screen.h"
#include "ccontrols.h"
#include "primitive_types.h"
#include "texture.h"
#include <cstdint>
#include <string>
#include <array>
#include <memory>

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

#ifdef DEF_CLAUDE_TESTING
    cc::control_collection* agent_controls() override { return &m_controls; }
#endif

private:
    static constexpr uint32_t SPLASH_DURATION_MS = 12000;  // 12 seconds total
    static constexpr int NUM_CONTRIBUTORS = 5;
    static constexpr uint32_t TIME_PER_CONTRIBUTOR_MS = 2000;  // 2 seconds each
    static constexpr uint32_t FADE_DURATION_MS = 400;  // 0.4 second fade in/out
    static constexpr uint32_t skip_allowed_after_ms = 1000;  // a click skips the splash after 1 second

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
	std::unique_ptr<hb::shared::render::ITexture> m_logo;   // interface/front_end/splash_logo.png

    float get_contributor_alpha(uint32_t elapsedMs, int contributorIndex) const;
};
