#pragma once

#include "pico/stdlib.h"

class Button
{
public:
    void init();
    bool update();

private:
    bool last_state_ = true;
    bool stable_state_ = true;
    absolute_time_t last_change_{};
};
