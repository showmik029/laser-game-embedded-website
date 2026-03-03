#include "laser/laser.hpp"

Laser::Laser(uint gpio_pin, uint32_t period_ms)
    : pin_(gpio_pin),
      enabled_(true),
      period_ticks_(pdMS_TO_TICKS(period_ms)),
      task_(nullptr) {}

void Laser::init() {
    gpio_init(pin_);
    gpio_set_dir(pin_, GPIO_OUT);
    gpio_put(pin_, 0); // laser OFF initially
}

bool Laser::start(UBaseType_t priority, uint16_t stack_words) {
    if (task_ != nullptr) return true; // already started

    BaseType_t ok = xTaskCreate(
        &Laser::taskTrampoline,
        "laser",
        stack_words,
        this,
        priority,
        &task_
    );

    return ok == pdPASS;
}

void Laser::setEnabled(bool on) {
    enabled_ = on;
    if (!on) {
        gpio_put(pin_, 0); // force OFF immediately
    }
}

void Laser::setPeriodMs(uint32_t ms) {
    if (ms == 0) ms = 1;
    period_ticks_ = pdMS_TO_TICKS(ms);
}

void Laser::taskTrampoline(void* arg) {
    static_cast<Laser*>(arg)->taskLoop();
}

void Laser::taskLoop() {
    TickType_t last = xTaskGetTickCount();
    bool state = false;

    for (;;) {
        if (enabled_) {
            state = !state;
            gpio_put(pin_, state ? 1 : 0);
        } else {
            state = false;
            gpio_put(pin_, 0);
        }

        vTaskDelayUntil(&last, period_ticks_);
    }
}