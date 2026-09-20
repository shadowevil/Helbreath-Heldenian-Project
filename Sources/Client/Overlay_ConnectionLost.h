// Overlay_ConnectionLost.h: Connection lost notification overlay
//
// Displays "Connection Lost!" message. Auto-redirects to MainMenu after 5 seconds.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "IGameScreen.h"

class Overlay_ConnectionLost : public IGameScreen
{
public:
    SCREEN_TYPE(Overlay_ConnectionLost)

    explicit Overlay_ConnectionLost(CGame* game);
    ~Overlay_ConnectionLost() override = default;

    void on_initialize() override;
    void on_update() override;
    void on_render() override;

private:
    uint32_t m_dwStartTime = 0;
    int m_iFrameCount = 0;
};
