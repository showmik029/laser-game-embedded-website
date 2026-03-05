#include "target_controller.hpp"

#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "hardware/adc.h"

TargetController::TargetController(const Config& cfg) : cfg_(cfg) {}

void TargetController::init() {
    // LEDs
    for (int i = 0; i < cfg_.num_channels; ++i) {
        gpio_init(cfg_.channels[i].led_gpio);
        gpio_set_dir(cfg_.channels[i].led_gpio, GPIO_OUT);
        gpio_put(cfg_.channels[i].led_gpio, 0);
    }

    // ADC
    adc_init();
    for (int i = 0; i < cfg_.num_channels; ++i) {
        adc_gpio_init(cfg_.channels[i].adc_gpio);
    }
}

int TargetController::find_channel(const char* id) const {
    if (!id) return -1;
    for (int i = 0; i < cfg_.num_channels; ++i) {
        if (cfg_.channels[i].id && std::strcmp(cfg_.channels[i].id, id) == 0) return i;
    }
    return -1;
}

uint16_t TargetController::read_adc(int idx) const {
    adc_select_input(cfg_.channels[idx].adc_input);
    return adc_read(); // 12-bit (0..4095)
}

void TargetController::arm(const char* target_id, int round) {
    // turn off all LEDs
    for (int i = 0; i < cfg_.num_channels; ++i) gpio_put(cfg_.channels[i].led_gpio, 0);

    int idx = find_channel(target_id);
    if (idx < 0) {
        disarm();
        return;
    }

    armed_idx_ = idx;
    armed_id_ = cfg_.channels[idx].id;
    armed_round_ = round;
    armed_since_ms_ = to_ms_since_boot(get_absolute_time());

    // LED on
    gpio_put(cfg_.channels[idx].led_gpio, 1);

    // Baseline average
    uint32_t sum = 0;
    for (int i = 0; i < cfg_.baseline_samples; ++i) {
        sum += read_adc(idx);
        sleep_ms(cfg_.sample_delay_ms);
    }
    baseline_ = (uint16_t)(sum / (uint32_t)cfg_.baseline_samples);
    printf("ARM: %s round=%d baseline=%u\n", armed_id_, armed_round_, baseline_);
}

void TargetController::disarm() {
    if (armed_idx_ >= 0) {
        gpio_put(cfg_.channels[armed_idx_].led_gpio, 0);
    }
    armed_idx_ = -1;
    armed_id_ = nullptr;
    armed_round_ = -1;
}

TargetController::HitEvent TargetController::tick(uint32_t now_ms) {
    HitEvent ev{};

    if (armed_idx_ < 0) return ev;

    // timeout
    if (now_ms - armed_since_ms_ > cfg_.arm_timeout_ms) {
        disarm();
        return ev;
    }

    // cooldown after a hit
    if ((int32_t)(now_ms - cooldown_until_ms_) < 0) return ev;

    uint16_t v = read_adc(armed_idx_);
    int diff = (int)v - (int)baseline_;
    if (diff < 0) diff = -diff;

    if (cfg_.debug_print_ms > 0 && (now_ms - last_debug_ms_) >= cfg_.debug_print_ms) {
        last_debug_ms_ = now_ms;
        printf("ADC: %s v=%u baseline=%u diff=%d (thr=%u)\n",
               armed_id_ ? armed_id_ : "?", v, baseline_, diff, cfg_.hit_delta);
    }

    if ((uint16_t)diff > cfg_.hit_delta) {
        ev.hit = true;
        ev.target_id = armed_id_;
        ev.round = armed_round_;

        // disarm + cooldown
        disarm();
        cooldown_until_ms_ = now_ms + cfg_.cooldown_ms;
    }

    return ev;
}