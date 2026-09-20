// Screen_SelectCharacter.h: Select Character Screen Class
//
// Handles character selection, navigation to creation/password screens.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "IGameScreen.h"
#include "control_collection.h"
#include <cstdint>
#include <memory>

class Screen_SelectCharacter : public IGameScreen
{
public:
    SCREEN_TYPE(Screen_SelectCharacter)

    GameMode get_game_mode() const override { return GameMode::SelectCharacter; }

    explicit Screen_SelectCharacter(CGame* game);
    ~Screen_SelectCharacter() override = default;

    void on_initialize() override;
    void on_update() override;
    void on_render() override;
    bool on_net_response(uint16_t response_type, char* data) override;

    bool enter_game();

private:
    void activate_slot(int slot_id);
    void render_character_previews();
    void render_tooltip_text();
    void render_account_info();

    // CControls
    cc::control_collection m_controls;
    int m_last_clicked_slot = -1;
    uint32_t m_last_click_time = 0;
    bool m_enter_edge = false;
    bool m_was_suppressed = false;
    cc::input_state m_prev_input{};

    // Double-click window for slot activation
    static constexpr uint32_t DOUBLE_CLICK_MS = 400;

    // Animation timer
    uint32_t m_dwSelCharCTime;

    // Offset for centering 640x480 content in 800x600 base resolution
    static constexpr short OX = 80;
    static constexpr short OY = 60;

    // Control IDs
    static constexpr int SLOT_1 = 1;
    static constexpr int SLOT_2 = 2;
    static constexpr int SLOT_3 = 3;
    static constexpr int SLOT_4 = 4;
    static constexpr int BTN_ENTER = 10;
    static constexpr int BTN_NEW = 11;
    static constexpr int BTN_DELETE = 12;
    static constexpr int BTN_CHANGE_PW = 13;
    static constexpr int BTN_EXIT = 14;
};
