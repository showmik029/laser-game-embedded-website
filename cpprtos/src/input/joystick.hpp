#pragma once
#include <cstdint>

#include "FreeRTOS.h"
#include "queue.h"

enum class InputEvent : uint8_t {
    Up,
    Down,
    Left,
    Right,
    Select,
    Back,
};

struct JoystickPins {
    uint adc_x_gpio; // e.g. 26
    uint adc_y_gpio; // e.g. 27
    uint btn_gpio;   // e.g. 22

    // ADC channel index (GPIO26->ADC0, GPIO27->ADC1)
    uint adc_x_ch = 0;
    uint adc_y_ch = 1;
};

struct JoystickTuning {
    uint16_t deadzone;       // +/- around center treated as neutral
    uint16_t threshold;      // how far to move before a direction triggers
    bool invert_x;
    bool invert_y;

    TickType_t repeat_initial;
    TickType_t repeat_period;

    uint32_t debounce_ms;
};

struct JoystickTaskParams {
    QueueHandle_t queue;
    JoystickPins pins;
    JoystickTuning tuning;
};

// FreeRTOS task that posts InputEvent to params->queue.
void joystick_task(void* param);
