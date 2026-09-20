// Overlay_QueryForceLogin.h: "Character on Use" confirmation overlay
//
// Asks if player wants to force disconnect an existing session.
// Yes/No buttons with ESC to cancel.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "IGameScreen.h"
#include "CControls.h"

class Overlay_QueryForceLogin : public IGameScreen
{
public:
    SCREEN_TYPE(Overlay_QueryForceLogin)

    explicit Overlay_QueryForceLogin(CGame* game);
    ~Overlay_QueryForceLogin() override = default;

    void on_initialize() override;
    void on_update() override;
    void on_render() override;

private:
    static constexpr int BTN_YES = 1;
    static constexpr int BTN_NO = 2;

    cc::control_collection m_controls;

    uint32_t m_dwStartTime = 0;
};
