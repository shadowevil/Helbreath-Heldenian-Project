// screen_create_new_character.h: Create New Character Screen Class
//
// Handles character creation UI including appearance customization and stat allocation.
//
// The art is the HD sheet (HH-0122): interface/front_end/character_create.png, a 1920x1080 backdrop cover-fit
// like the other front-end screens, with the arrow and +/- buttons in a strip beside it. The original layout is
// mapped onto the HD panels keeping every element's place; tools/front_end/make_create_backdrop.py bakes the
// value-box recesses from the same numbers.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "game_screen.h"
#include "control_collection.h"
#include "primitive_types.h"
#include "text_lib.h"
#include "texture.h"
#include <cstdint>
#include <memory>
#include <span>

class Screen_CreateNewCharacter : public IGameScreen
{
public:
    SCREEN_TYPE(Screen_CreateNewCharacter)

    GameMode get_game_mode() const override { return GameMode::CreateNewCharacter; }

    explicit Screen_CreateNewCharacter(CGame* game);
    ~Screen_CreateNewCharacter() override = default;

    void on_initialize() override;
    void on_uninitialize() override;
    void on_update() override;
    void on_render() override;
    bool on_net_response(uint16_t response_type, char* data) override;

#ifdef DEF_CLAUDE_TESTING
    cc::control_collection* agent_controls() override { return &m_controls; }
    std::span<const agent_control_name> agent_control_names() const override
    {
        static constexpr agent_control_name names[] = {
            {TXT_NAME, "create_character.name"},
            {BTN_ROW_LEFT + 0, "create_character.gender.minus"},
            {BTN_ROW_RIGHT + 0, "create_character.gender.plus"},
            {BTN_ROW_LEFT + 1, "create_character.skin.minus"},
            {BTN_ROW_RIGHT + 1, "create_character.skin.plus"},
            {BTN_ROW_LEFT + 2, "create_character.hair_style.minus"},
            {BTN_ROW_RIGHT + 2, "create_character.hair_style.plus"},
            {BTN_ROW_LEFT + 3, "create_character.hair_color.minus"},
            {BTN_ROW_RIGHT + 3, "create_character.hair_color.plus"},
            {BTN_ROW_LEFT + 4, "create_character.underwear.minus"},
            {BTN_ROW_RIGHT + 4, "create_character.underwear.plus"},
            // Stats: plus on the left, minus on the right (the original's order)
            {BTN_ROW_RIGHT + 5, "create_character.str.minus"},
            {BTN_ROW_LEFT + 5, "create_character.str.plus"},
            {BTN_ROW_RIGHT + 6, "create_character.vit.minus"},
            {BTN_ROW_LEFT + 6, "create_character.vit.plus"},
            {BTN_ROW_RIGHT + 7, "create_character.dex.minus"},
            {BTN_ROW_LEFT + 7, "create_character.dex.plus"},
            {BTN_ROW_RIGHT + 8, "create_character.int.minus"},
            {BTN_ROW_LEFT + 8, "create_character.int.plus"},
            {BTN_ROW_RIGHT + 9, "create_character.mag.minus"},
            {BTN_ROW_LEFT + 9, "create_character.mag.plus"},
            {BTN_ROW_RIGHT + 10, "create_character.chr.minus"},
            {BTN_ROW_LEFT + 10, "create_character.chr.plus"},
            {BTN_CREATE, "create_character.create"},
            {BTN_CANCEL, "create_character.cancel"},
            {BTN_WARRIOR, "create_character.warrior"},
            {BTN_MAGE, "create_character.mage"},
            {BTN_MASTER, "create_character.master"},
        };
        return names;
    }
#endif

private:
    void submit_create_character();
    void apply_class_preset(int class_button);
    void update_layout();   // this frame's fit; places every control on the HD panels
    void render_help_text();
    // Text through this frame's fit: a box in art px, the text aligned in it
    void draw_art_text(float x0, float y0, float x1, float y1, const char* text, const hb::shared::render::Color& color,
        int font_size, hb::shared::text::Align align) const;

    // CControls
    cc::control_collection m_controls;
    int m_prev_focused = -1;
    int m_selected_class = -1;
    bool m_was_suppressed = false;

    // Character creation state
    int m_iNewCharPoint;
    uint32_t m_dwNewCharMTime;
    bool m_bNewCharFlag;

    // HD art and this frame's cover fit
    std::unique_ptr<hb::shared::render::ITexture> m_art;   // staged interface/front_end/character_create.png
    hb::shared::render::art_fit m_fit;

    // Rows: 0-4 appearance (gender, skin, hair style, hair colour, underwear), 5-10 stats (str, vit, dex, int,
    // mag, chr)
    static constexpr int row_count = 11;
    static constexpr int appearance_rows = 5;

    // Control IDs
    static constexpr int TXT_NAME = 1;
    static constexpr int BTN_ROW_LEFT = 100;    // + row: the row's left strip button
    static constexpr int BTN_ROW_RIGHT = 120;   // + row: the row's right strip button
    static constexpr int ZONE_NAME = 140;       // help-text hover zones (cc::panel, drawn by nothing)
    static constexpr int ZONE_ROW = 141;        // + row
    // Action buttons
    static constexpr int BTN_CREATE = 30;
    static constexpr int BTN_CANCEL = 31;
    static constexpr int BTN_WARRIOR = 32;
    static constexpr int BTN_MAGE = 33;
    static constexpr int BTN_MASTER = 34;
};
