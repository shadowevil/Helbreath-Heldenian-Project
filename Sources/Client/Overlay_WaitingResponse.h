// Overlay_WaitingResponse.h: "Connected. Waiting for response..." progress overlay
//
// Displays waiting status after connection is established.
// ESC key cancels and returns to main menu after 7 seconds delay.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "IGameScreen.h"

class Overlay_WaitingResponse : public IGameScreen
{
public:
    SCREEN_TYPE(Overlay_WaitingResponse)

    explicit Overlay_WaitingResponse(CGame* game);
    ~Overlay_WaitingResponse() override = default;

    void on_initialize() override;
    void on_update() override;
    void on_render() override;

private:
    uint32_t m_dwStartTime = 0;
    uint32_t m_dwAnimTime = 0;
};
