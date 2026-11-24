#pragma once

#include "game_screen.h"
#include "ccontrols.h"
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
