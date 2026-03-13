#include "laser/laser.hpp"

Laser::Laser(const LaserConfig& cfg)
    : cfg_(cfg),
      button_(cfg.button_pin, cfg.button_pull, cfg.button_pressed_when_low, cfg.debounce_ms) {
    poll_ticks_        = pdMS_TO_TICKS(cfg_.poll_ms);
    hold_ticks_        = pdMS_TO_TICKS(cfg_.hold_ms);
    auto_period_ticks_ = pdMS_TO_TICKS(cfg_.auto_period_ms);
    pulse_ticks_       = pdMS_TO_TICKS(cfg_.pulse_ms);
}

void Laser::init() {
    gpio_init(cfg_.laser_pin);
    gpio_set_dir(cfg_.laser_pin, GPIO_OUT);
    gpio_put(cfg_.laser_pin, 0);

    button_.init();
}

bool Laser::start(UBaseType_t priority, uint16_t stack_words) {
    if (task_ != nullptr) return true;

    const BaseType_t ok = xTaskCreate(
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
    if (!on) forceOff();
}

void Laser::setHoldMs(uint32_t ms) {
    if (ms == 0) ms = 1;
    hold_ticks_ = pdMS_TO_TICKS(ms);
}

void Laser::setAutoPeriodMs(uint32_t ms) {
    if (ms == 0) ms = 1;
    auto_period_ticks_ = pdMS_TO_TICKS(ms);
}

void Laser::setPulseMs(uint32_t ms) {
    if (ms == 0) ms = 1;
    pulse_ticks_ = pdMS_TO_TICKS(ms);
}

void Laser::taskTrampoline(void* arg) {
    static_cast<Laser*>(arg)->taskLoop();
}

void Laser::forceOff() {
    laser_on_ = false;
    gpio_put(cfg_.laser_pin, 0);
}

void Laser::fireShot(TickType_t now) {
    if (!enabled_) return;

    ++shot_count_;
    gpio_put(cfg_.laser_pin, 1);
    laser_on_ = true;
    laser_on_until_ = now + pulse_ticks_;
}

void Laser::taskLoop() {
    TickType_t last = xTaskGetTickCount();

    press_start_ = last;
    next_auto_shot_ = last;
    forceOff();

    for (;;) {
        vTaskDelayUntil(&last, poll_ticks_);
        const TickType_t now = xTaskGetTickCount();

        if (laser_on_ && timeReached(now, laser_on_until_)) {
            laser_on_ = false;
            gpio_put(cfg_.laser_pin, 0);
        }

        button_.update(now);

        if (!enabled_) {
            continue;
        }

        if (button_.rose()) {
            press_start_ = now;
            next_auto_shot_ = now;
        }

        if (button_.pressed()) {
            if (timeReached(now, next_auto_shot_)) {
                fireShot(now);
                next_auto_shot_ = now + auto_period_ticks_;
            }
        }

        if (button_.fell()) {
            forceOff();
        }
    }
}