// Overlay_QueryDeleteCharacter.h: "Delete Character" confirmation overlay
//
// Asks if player wants to permanently delete a character.
// Shows the character name and Yes/No buttons.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "IGameScreen.h"
#include "CControls.h"

class Overlay_QueryDeleteCharacter : public IGameScreen
{
public:
    SCREEN_TYPE(Overlay_QueryDeleteCharacter)

    explicit Overlay_QueryDeleteCharacter(CGame* game);
    ~Overlay_QueryDeleteCharacter() override = default;

    void on_initialize() override;
    void on_update() override;
    void on_render() override;

private:
    static constexpr int BTN_YES = 1;
    static constexpr int BTN_NO = 2;

    cc::control_collection m_controls;

    uint32_t m_dwStartTime = 0;
};
