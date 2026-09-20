// Overlay_ChangePassword.h: Password change overlay
//
// Displays password change form with input fields for old/new passwords.
// Shown over SelectCharacter screen.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "IGameScreen.h"
#include "CControls.h"
#include <string>

class Overlay_ChangePassword : public IGameScreen
{
public:
    SCREEN_TYPE(Overlay_ChangePassword)

    explicit Overlay_ChangePassword(CGame* game);
    ~Overlay_ChangePassword() override = default;

    void on_initialize() override;
    void on_update() override;
    void on_render() override;

private:
    void handle_submit();

    // Control IDs
    static constexpr int TXT_OLD_PW = 2;
    static constexpr int TXT_NEW_PW = 3;
    static constexpr int TXT_CONFIRM_PW = 4;
    static constexpr int BTN_OK = 10;
    static constexpr int BTN_CANCEL = 11;

    cc::control_collection m_controls;
    int m_prev_focused = -1;

    // Error feedback
    std::string m_error_msg;
};
