#pragma once

#include "IGameScreen.h"
#include "CControls.h"
#include <cstdint>

class Screen_CreateAccount : public IGameScreen {
public:
    SCREEN_TYPE(Screen_CreateAccount)

    GameMode get_game_mode() const override { return GameMode::CreateNewAccount; }

    explicit Screen_CreateAccount(CGame* game);
    ~Screen_CreateAccount() override = default;

    void on_initialize() override;
    void on_update() override;
    void on_render() override;
    bool on_net_response(uint16_t response_type, char* data) override;

private:
    void submit_create_account();

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
};
