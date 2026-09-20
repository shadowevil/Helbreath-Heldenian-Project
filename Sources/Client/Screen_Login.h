// Screen_Login.h: Login Screen Class
//
// Handles user login interaction, credential entry, and server connection request.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "IGameScreen.h"
#include "CControls.h"

class Screen_Login : public IGameScreen
{
public:
    SCREEN_TYPE(Screen_Login)

    GameMode get_game_mode() const override { return GameMode::Login; }

    explicit Screen_Login(CGame* game);
    ~Screen_Login() override = default;

    void on_initialize() override;
    void on_update() override;
    void on_render() override;
    bool on_net_response(uint16_t response_type, char* data) override;

private:
    void submit_login();

    enum control_id
    {
        TXT_NAME = 1,
        TXT_PASSWORD = 2,
        BTN_LOGIN = 3,
        BTN_CANCEL = 4,
    };

    cc::control_collection m_controls;
};
