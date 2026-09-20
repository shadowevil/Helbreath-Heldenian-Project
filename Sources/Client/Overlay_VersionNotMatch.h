// Overlay_VersionNotMatch.h: Version mismatch error overlay
//
// Displays version mismatch error. OK button or ESC/Enter closes the application.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "IGameScreen.h"
#include "CControls.h"

class Overlay_VersionNotMatch : public IGameScreen
{
public:
    SCREEN_TYPE(Overlay_VersionNotMatch)

    explicit Overlay_VersionNotMatch(CGame* game);
    ~Overlay_VersionNotMatch() override = default;

    void on_initialize() override;
    void on_update() override;
    void on_render() override;

private:
    void close_app();

    static constexpr int BTN_OK = 1;

    cc::control_collection m_controls;
};
