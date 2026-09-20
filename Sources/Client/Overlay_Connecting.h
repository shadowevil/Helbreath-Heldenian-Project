// Overlay_Connecting.h: "Connecting to Server..." progress overlay
//
// Displays connection progress with countdown timer.
// ESC key cancels and returns to main menu after 7 seconds.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "IGameScreen.h"

class Overlay_Connecting : public IGameScreen
{
public:
    SCREEN_TYPE(Overlay_Connecting)

    explicit Overlay_Connecting(CGame* game);
    ~Overlay_Connecting() override = default;

    void on_initialize() override;
    void on_update() override;
    void on_render() override;

private:
    uint32_t m_dwStartTime = 0;
    uint32_t m_dwAnimTime = 0;
};
