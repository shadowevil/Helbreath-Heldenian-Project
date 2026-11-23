#pragma once
#include <cstdint>
#include "common_types.h"

class GameTimer
{
public:
    bool check_and_reset()
    {
        uint32_t now = GameClock::get_time_ms();
        if (now - last_tick_ms_ >= interval_ms_) {
            last_tick_ms_ = now;
            return true;
        }
        return false;
    }
private:
    uint32_t last_tick_ms_ = 0;
    static constexpr uint32_t interval_ms_ = 1000;
};
