// overlay_query_force_login.h: "Character on Use" confirmation overlay
//
// Asks if player wants to force disconnect an existing session.
// Yes/No buttons with ESC to cancel.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "game_screen.h"
#include "ccontrols.h"

class Overlay_QueryForceLogin : public IGameScreen
{
public:
    SCREEN_TYPE(Overlay_QueryForceLogin)

    explicit Overlay_QueryForceLogin(CGame* game);
    ~Overlay_QueryForceLogin() override = default;

    void on_initialize() override;
    void on_update() override;
    void on_render() override;

#ifdef DEF_CLAUDE_TESTING
    cc::control_collection* agent_controls() override { return &m_controls; }
    std::span<const agent_control_name> agent_control_names() const override
    {
        static constexpr agent_control_name names[] = {
            {BTN_YES, "query_force_login.yes"},
            {BTN_NO, "query_force_login.no"},
        };
        return names;
    }
#endif

private:
    static constexpr int BTN_YES = 1;
    static constexpr int BTN_NO = 2;

    cc::control_collection m_controls;

    uint32_t m_dwStartTime = 0;
};
