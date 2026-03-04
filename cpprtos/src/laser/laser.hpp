#pragma once

#include "FreeRTOS.h"
#include "task.h"
#include "hardware/gpio.h"
#include <cstdint>

#include "input/debounced_button.hpp"

struct LaserConfig {
    uint laser_pin = 13;
    uint button_pin = 12;

    DebouncedButton::Pull button_pull = DebouncedButton::Pull::Up;
    bool button_pressed_when_low = true;

    // Timings
    uint32_t poll_ms = 5;
    uint32_t debounce_ms = 30;

    uint32_t hold_ms = 250;
    uint32_t auto_period_ms = 100;
    uint32_t pulse_ms = 20;
};

class Laser {
public:
    explicit Laser(const LaserConfig& cfg = {});

    void init(); // init laser GPIO + button GPIO
    bool start(UBaseType_t priority = tskIDLE_PRIORITY + 1,
               uint16_t stack_words = configMINIMAL_STACK_SIZE + 128);

    void setEnabled(bool on);

    // Optional runtime tuning
    void setHoldMs(uint32_t ms);
    void setAutoPeriodMs(uint32_t ms);
    void setPulseMs(uint32_t ms);

private:
    static void taskTrampoline(void* arg);
    void taskLoop();

    static inline bool timeReached(TickType_t now, TickType_t target) {
        return (int32_t)(now - target) >= 0;
    }

    void fireShot(TickType_t now);
    void forceOff();

    LaserConfig cfg_;
    DebouncedButton button_;

    volatile bool enabled_{true};
    TaskHandle_t task_{nullptr};

    TickType_t poll_ticks_{pdMS_TO_TICKS(5)};
    TickType_t hold_ticks_{pdMS_TO_TICKS(250)};
    TickType_t auto_period_ticks_{pdMS_TO_TICKS(100)};
    TickType_t pulse_ticks_{pdMS_TO_TICKS(20)};

    // State
    TickType_t press_start_{0};
    TickType_t next_auto_shot_{0};

    bool laser_on_{false};
    TickType_t laser_on_until_{0};
};