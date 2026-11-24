// overlay_query_delete_character.h: "Delete Character" confirmation overlay
//
// Asks if player wants to permanently delete a character.
// Shows the character name and Yes/No buttons.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "game_screen.h"
#include "ccontrols.h"

class Overlay_QueryDeleteCharacter : public IGameScreen
{
public:
    SCREEN_TYPE(Overlay_QueryDeleteCharacter)

    explicit Overlay_QueryDeleteCharacter(CGame* game);
    ~Overlay_QueryDeleteCharacter() override = default;

    void on_initialize() override;
    void on_update() override;
    void on_render() override;

#ifdef DEF_CLAUDE_TESTING
    cc::control_collection* agent_controls() override { return &m_controls; }
    std::span<const agent_control_name> agent_control_names() const override
    {
        static constexpr agent_control_name names[] = {
            {BTN_YES, "query_delete_character.yes"},
            {BTN_NO, "query_delete_character.no"},
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
