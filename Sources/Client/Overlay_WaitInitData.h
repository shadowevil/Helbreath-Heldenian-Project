// Overlay_WaitInitData.h: Waiting for server init data overlay
//
// Displays waiting message with countdown. ESC after 7 seconds returns to MainMenu.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "IGameScreen.h"

class Overlay_WaitInitData : public IGameScreen
{
public:
    SCREEN_TYPE(Overlay_WaitInitData)

    explicit Overlay_WaitInitData(CGame* game);
    ~Overlay_WaitInitData() override = default;

    void on_initialize() override;
    void on_update() override;
    void on_render() override;

private:
    uint32_t m_dwStartTime = 0;
    int m_iFrameCount = 0;
};
