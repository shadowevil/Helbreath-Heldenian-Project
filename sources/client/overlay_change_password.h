// overlay_change_password.h: Password change overlay
//
// Displays password change form with input fields for old/new passwords.
// Shown over SelectCharacter screen.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "game_screen.h"
#include "ccontrols.h"
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

#ifdef DEF_CLAUDE_TESTING
    cc::control_collection* agent_controls() override { return &m_controls; }
    std::span<const agent_control_name> agent_control_names() const override
    {
        static constexpr agent_control_name names[] = {
            {TXT_OLD_PW, "change_password.old"},
            {TXT_NEW_PW, "change_password.new"},
            {TXT_CONFIRM_PW, "change_password.confirm"},
            {BTN_OK, "change_password.ok"},
            {BTN_CANCEL, "change_password.cancel"},
        };
        return names;
    }
#endif

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
