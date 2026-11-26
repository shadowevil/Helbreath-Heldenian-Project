// screen_select_character.h: Select Character Screen Class
//
// Handles character selection, navigation to creation/password screens.
//
// The art is the HD sheet (HH-0108): interface/front_end/character_select.png, a 1920x1080 backdrop cover-fit
// like the other front-end screens, plus the normal and selected slot cards beside it.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "game_screen.h"
#include "control_collection.h"
#include "primitive_types.h"
#include "texture.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

class Screen_SelectCharacter : public IGameScreen
{
public:
    SCREEN_TYPE(Screen_SelectCharacter)

    GameMode get_game_mode() const override { return GameMode::SelectCharacter; }

    explicit Screen_SelectCharacter(CGame* game);
    ~Screen_SelectCharacter() override = default;

    void on_initialize() override;
    void on_uninitialize() override;
    void on_update() override;
    void on_render() override;
    bool on_net_response(uint16_t response_type, char* data) override;

    bool enter_game();

#ifdef DEF_CLAUDE_TESTING
    cc::control_collection* agent_controls() override { return &m_controls; }
    std::span<const agent_control_name> agent_control_names() const override
    {
        static constexpr agent_control_name names[] = {
            {SLOT_1, "select_character.slot1"},
            {SLOT_2, "select_character.slot2"},
            {SLOT_3, "select_character.slot3"},
            {SLOT_4, "select_character.slot4"},
            {BTN_ENTER, "select_character.enter"},
            {BTN_NEW, "select_character.new"},
            {BTN_DELETE, "select_character.delete"},
            {BTN_CHANGE_PW, "select_character.change_password"},
            {BTN_EXIT, "select_character.exit"},
        };
        return names;
    }
#endif

private:
    void activate_slot(int slot_id);
    void update_layout();   // this frame's fit; places the cards, the plank group and their controls
    float slot_x(int slot, float card_px) const;   // window position of a card-local art px
    float slot_y(float card_px) const;
    // Window position of a point in 800x600 UI coords (the original screen's), through this frame's fit
    float classic_x(float ui) const;
    float classic_y(float ui) const;
    // Plank text lines centred both ways in a box given in UI coords
    void draw_box_lines(float box_x0, float box_y0, float box_x1, float box_y1, std::span<const char* const> lines) const;
    void draw_menu_button(std::size_t index, int frame);   // frame 51-60 into that button's face
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

    // HD art and this frame's cover fit
    std::unique_ptr<hb::shared::render::ITexture> m_art;   // staged interface/front_end/character_select.png
    hb::shared::render::art_fit m_fit;

    // Top-left of each menu button's face (UI), placed on the HD plank by update_layout
    std::array<cc::point, 5> m_button_face{};

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
