#include "button.h"

#include "constants.h"
#include "hardware/gpio.h"

void Button::init()
{
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);
    last_state_ = gpio_get(BUTTON_PIN);
    stable_state_ = last_state_;
    last_change_ = get_absolute_time();
}

bool Button::update()
{
    bool raw_state = gpio_get(BUTTON_PIN);
    absolute_time_t now = get_absolute_time();

    if (raw_state != last_state_)
    {
        last_state_ = raw_state;
        last_change_ = now;
    }

    if ((absolute_time_diff_us(last_change_, now) >= BUTTON_DEBOUNCE_US) && (stable_state_ != raw_state))
    {
        stable_state_ = raw_state;
        if (!stable_state_)
        {
            return true; // active low
        }
    }
    return false;
}
