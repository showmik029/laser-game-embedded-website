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
    // Laser output
    gpio_init(cfg_.laser_pin);
    gpio_set_dir(cfg_.laser_pin, GPIO_OUT);
    gpio_put(cfg_.laser_pin, 0);

    // Button input
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

    gpio_put(cfg_.laser_pin, 1);
    laser_on_ = true;

    const TickType_t until = now + pulse_ticks_;
    if (!laser_on_ || timeReached(until, laser_on_until_)) {
        laser_on_until_ = until;
    } else {
        if (timeReached(until, laser_on_until_)) laser_on_until_ = until;
    }
    if (timeReached(until, laser_on_until_)) laser_on_until_ = until;
}

void Laser::taskLoop() {
    TickType_t last = xTaskGetTickCount();

    // Initialize state from current button reading
    press_start_ = last;
    next_auto_shot_ = last + hold_ticks_;
    forceOff();

    for (;;) {
        vTaskDelayUntil(&last, poll_ticks_);
        const TickType_t now = xTaskGetTickCount();

        // turn laser OFF when pulse ends
        if (laser_on_ && timeReached(now, laser_on_until_)) {
            laser_on_ = false;
            gpio_put(cfg_.laser_pin, 0);
        }

        button_.update(now);

        if (!enabled_) {
            // ignore input while disabled
            continue;
        }

        if (button_.rose()) {
            // tap -> immediate single shot
            press_start_ = now;
            next_auto_shot_ = now + hold_ticks_;
            fireShot(now);
        }

        if (button_.pressed()) {
            // after hold threshold -> auto-fire
            if (timeReached(now, press_start_ + hold_ticks_)) {
                if (timeReached(now, next_auto_shot_)) {
                    fireShot(now);
                    next_auto_shot_ = now + auto_period_ticks_;
                }
            }
        }

        if (button_.fell()) {
            // released -> stop firing and force OFF
            forceOff();
        }
    }
}