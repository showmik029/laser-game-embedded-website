#pragma once

#include "FreeRTOS.h"
#include "task.h"
#include "hardware/gpio.h"
#include <cstdint>

class DebouncedButton {
public:
    enum class Pull { None, Up, Down };

    DebouncedButton(uint pin,
                    Pull pull = Pull::Up,
                    bool pressed_when_low = true,
                    uint32_t debounce_ms = 30)
        : pin_(pin),
          pull_(pull),
          pressed_when_low_(pressed_when_low),
          debounce_ticks_(pdMS_TO_TICKS(debounce_ms)) {}

    void init() {
        gpio_init(pin_);
        gpio_set_dir(pin_, GPIO_IN);

        if (pull_ == Pull::Up) gpio_pull_up(pin_);
        else if (pull_ == Pull::Down) gpio_pull_down(pin_);
        else gpio_disable_pulls(pin_);

        last_raw_ = readRaw();
        debounced_raw_ = last_raw_;
        last_change_tick_ = xTaskGetTickCount();
        pressed_ = decodePressed(debounced_raw_);
        rose_ = fell_ = false;
    }

    void update(TickType_t now) {
        rose_ = false;
        fell_ = false;

        const bool raw = readRaw();
        if (raw != last_raw_) {
            last_raw_ = raw;
            last_change_tick_ = now;
            return;
        }

        // stable raw long enough -> accept as debounced
        if (debounced_raw_ != raw && (now - last_change_tick_) >= debounce_ticks_) {
            const bool prev_pressed = pressed_;
            debounced_raw_ = raw;
            pressed_ = decodePressed(debounced_raw_);

            rose_ = (!prev_pressed && pressed_);
            fell_ = (prev_pressed && !pressed_);
        }
    }

    bool pressed() const { return pressed_; }
    bool rose() const { return rose_; } // pressed edge
    bool fell() const { return fell_; } // released edge

private:
    bool readRaw() const { return gpio_get(pin_) != 0; }

    bool decodePressed(bool raw_level) const {
        // If pressed_when_low == true: pressed when raw is 0 (GPIO tied to GND)
        return pressed_when_low_ ? !raw_level : raw_level;
    }

    uint pin_;
    Pull pull_;
    bool pressed_when_low_;
    TickType_t debounce_ticks_;

    bool last_raw_{true};
    bool debounced_raw_{true};
    TickType_t last_change_tick_{0};

    bool pressed_{false};
    bool rose_{false};
    bool fell_{false};
};