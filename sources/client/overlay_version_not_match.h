// overlay_version_not_match.h: Version mismatch error overlay
//
// Displays version mismatch error. OK button or ESC/Enter closes the application.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "game_screen.h"
#include "ccontrols.h"
#include "screen_quit.h"

class Overlay_VersionNotMatch : public IGameScreen
{
public:
    SCREEN_TYPE(Overlay_VersionNotMatch)

    explicit Overlay_VersionNotMatch(CGame* game);
    ~Overlay_VersionNotMatch() override = default;

    void on_initialize() override;
	void on_uninitialize() override;
    void on_update() override;
    void on_render() override;

#ifdef DEF_CLAUDE_TESTING
    cc::control_collection* agent_controls() override { return &m_controls; }
    std::span<const agent_control_name> agent_control_names() const override
    {
        static constexpr agent_control_name names[] = {
            {BTN_OK, "version_not_match.ok"},
        };
        return names;
    }
#endif

private:
    void close_app();

    static constexpr int BTN_OK = 1;

    cc::control_collection m_controls;
	exit_backdrop m_backdrop;
};
