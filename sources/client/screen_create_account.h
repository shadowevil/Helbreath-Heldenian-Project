#pragma once

#include "game_screen.h"
#include "ccontrols.h"
#include "bake_debounce.h"
#include "panel_kit.h"
#include "primitive_types.h"
#include "texture.h"
#include <cstdint>
#include <memory>

class Screen_CreateAccount : public IGameScreen {
public:
    SCREEN_TYPE(Screen_CreateAccount)

    GameMode get_game_mode() const override { return GameMode::CreateNewAccount; }

    explicit Screen_CreateAccount(CGame* game);
    ~Screen_CreateAccount() override = default;

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
            {TXT_ACCOUNT, "create_account.account"},
            {TXT_PASSWORD, "create_account.password"},
            {TXT_CONFIRM, "create_account.confirm"},
            {TXT_EMAIL, "create_account.email"},
            {BTN_CREATE, "create_account.create"},
            {BTN_CLEAR, "create_account.clear"},
            {BTN_CANCEL, "create_account.cancel"},
        };
        return names;
    }
#endif

private:
    void submit_create_account();
    void update_layout();   // the form on the art's tablet, from this frame's fit (HH-0087)
    void bake_title_glow();   // the title's glow, at the window's resolution
    void draw_title(const hb::client::panel_kit& kit, int cx, int cy) const;

    enum control_id
    {
        TXT_ACCOUNT = 1,
        TXT_PASSWORD = 2,
        TXT_CONFIRM = 3,
        TXT_EMAIL = 4,
        BTN_CREATE = 5,
        BTN_CLEAR = 6,
        BTN_CANCEL = 7,
    };

    cc::control_collection m_controls;
    std::unique_ptr<hb::shared::render::ITexture> m_art;   // staged interface/front_end/create_new_account.png
    hb::shared::render::art_fit m_fit;
    int m_form_x = 0;   // the form's top-left in UI units
    int m_form_y = 0;
    std::unique_ptr<hb::shared::render::ITexture> m_title_glow;   // baked; redone when the UI scale changes
    float m_glow_scale = 0.0f;   // the UI scale it was baked for; 0 = not yet
    hb::client::bake_debounce m_glow_debounce;
    float m_glow_x = 0.0f;       // its rect relative to the title's centre, in UI units
    float m_glow_y = 0.0f;
    float m_glow_w = 0.0f;
    float m_glow_h = 0.0f;
};
