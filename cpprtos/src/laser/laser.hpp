#pragma once

#include "FreeRTOS.h"
#include "task.h"
#include "hardware/gpio.h"
#include <cstdint>

class Laser {
public:
    explicit Laser(uint gpio_pin = 13, uint32_t period_ms = 250);

    void init();                 // sets GPIO direction + default LOW
    bool start(UBaseType_t priority = tskIDLE_PRIORITY + 1,
               uint16_t stack_words = configMINIMAL_STACK_SIZE + 128);

    void setEnabled(bool on);
    void setPeriodMs(uint32_t ms);

private:
    static void taskTrampoline(void* arg);
    void taskLoop();

    uint pin_;
    volatile bool enabled_;
    volatile TickType_t period_ticks_;
    TaskHandle_t task_;
};