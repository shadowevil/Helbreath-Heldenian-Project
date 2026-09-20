// Screen_CreateNewCharacter.h: Create New Character Screen Class
//
// Handles character creation UI including appearance customization and stat allocation.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "IGameScreen.h"
#include "control_collection.h"
#include <cstdint>

class Screen_CreateNewCharacter : public IGameScreen
{
public:
    SCREEN_TYPE(Screen_CreateNewCharacter)

    GameMode get_game_mode() const override { return GameMode::CreateNewCharacter; }

    explicit Screen_CreateNewCharacter(CGame* game);
    ~Screen_CreateNewCharacter() override = default;

    void on_initialize() override;
    void on_update() override;
    void on_render() override;
    bool on_net_response(uint16_t response_type, char* data) override;

private:
    void submit_create_character();
    void render_tooltips();

    // CControls
    cc::control_collection m_controls;
    int m_prev_focused = -1;
    int m_selected_class = -1;
    bool m_was_suppressed = false;

    // Character creation state
    int m_iNewCharPoint;
    uint32_t m_dwNewCharMTime;
    bool m_bNewCharFlag;

    // Offset for centering 640x480 content in 800x600 base resolution
    static constexpr short OX = 80;
    static constexpr short OY = 60;

    // Control IDs
    static constexpr int TXT_NAME = 1;
    // Appearance toggles
    static constexpr int TOG_GENDER = 10;
    static constexpr int TOG_SKIN = 11;
    static constexpr int TOG_HAIR_STYLE = 12;
    static constexpr int TOG_HAIR_COLOR = 13;
    static constexpr int TOG_UNDERWEAR = 14;
    // Stat toggles
    static constexpr int TOG_STR = 20;
    static constexpr int TOG_VIT = 21;
    static constexpr int TOG_DEX = 22;
    static constexpr int TOG_INT = 23;
    static constexpr int TOG_MAG = 24;
    static constexpr int TOG_CHR = 25;
    // Action buttons
    static constexpr int BTN_CREATE = 30;
    static constexpr int BTN_CANCEL = 31;
    static constexpr int BTN_WARRIOR = 32;
    static constexpr int BTN_MAGE = 33;
    static constexpr int BTN_MASTER = 34;
};
