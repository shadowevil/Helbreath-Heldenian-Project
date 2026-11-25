// screen_login.h: Login Screen Class
//
// Handles user login interaction, credential entry, and server connection request.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "game_screen.h"
#include "bake_debounce.h"
#include "ccontrols.h"
#include "primitive_types.h"
#include "small_caps_label.h"
#include "texture.h"
#include <array>
#include <memory>

class Screen_Login : public IGameScreen
{
public:
    SCREEN_TYPE(Screen_Login)

    GameMode get_game_mode() const override { return GameMode::Login; }

    explicit Screen_Login(CGame* game);
    ~Screen_Login() override = default;

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
            {TXT_NAME, "login.name"},
            {TXT_PASSWORD, "login.password"},
            {BTN_LOGIN, "login.ok"},
            {BTN_CANCEL, "login.cancel"},
        };
        return names;
    }
#endif

private:
    void submit_login();
    void update_layout();   // the plate's controls from this frame's fit (HH-0089)
    void bake_labels();     // the small-caps labels and glows at the window's resolution
    void draw_plate(float alpha);
    cc::rect panel_rect(float x, float y, float w, float h) const;   // original panel px -> UI units

    enum control_id
    {
        TXT_NAME = 1,
        TXT_PASSWORD = 2,
        BTN_LOGIN = 3,
        BTN_CANCEL = 4,
    };

    cc::control_collection m_controls;
    std::unique_ptr<hb::shared::render::ITexture> m_art;   // staged interface/front_end/login_screen.png
    hb::shared::render::art_fit m_fit;
    std::array<hb::client::small_caps_label, 5> m_labels;   // title, account, password, connect, cancel
    float m_plate_alpha = 0.0f;   // this frame's plate fade; the textbox text follows it
    float m_baked_scale = 0.0f;   // window px per art px they were baked for; 0 = not yet
    hb::client::bake_debounce m_label_debounce;
};
