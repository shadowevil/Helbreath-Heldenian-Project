// Overlay_Msg.h: Simple message display overlay
//
// Displays m_msg message. Auto-redirects to MainMenu after 1.5 seconds.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "IGameScreen.h"

class Overlay_Msg : public IGameScreen
{
public:
    SCREEN_TYPE(Overlay_Msg)

    explicit Overlay_Msg(CGame* game);
    ~Overlay_Msg() override = default;

    void on_initialize() override;
    void on_update() override;
    void on_render() override;

private:
    uint32_t m_dwStartTime = 0;
};
