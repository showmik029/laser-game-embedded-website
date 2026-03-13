#include "target_controller.hpp"

#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "hardware/adc.h"

TargetController::TargetController(const Config& cfg) : cfg_(cfg) {}

void TargetController::init() {
    for (int i = 0; i < cfg_.num_channels; ++i) {
        gpio_init(cfg_.channels[i].led_gpio);
        gpio_set_dir(cfg_.channels[i].led_gpio, GPIO_OUT);
        gpio_put(cfg_.channels[i].led_gpio, 0);
    }

    adc_init();
    for (int i = 0; i < cfg_.num_channels; ++i) {
        adc_gpio_init(cfg_.channels[i].adc_gpio);
    }
}

void TargetController::clear_leds() {
    for (int i = 0; i < cfg_.num_channels; ++i) {
        gpio_put(cfg_.channels[i].led_gpio, 0);
    }
}

int TargetController::find_channel(const char* zone_id) const {
    if (!zone_id) return -1;

    for (int i = 0; i < cfg_.num_channels; ++i) {
        if (cfg_.channels[i].zone_id &&
            std::strcmp(cfg_.channels[i].zone_id, zone_id) == 0) {
            return i;
        }
    }
    return -1;
}

uint16_t TargetController::read_adc(int idx) const {
    adc_select_input(cfg_.channels[idx].adc_input);
    return adc_read();
}

uint16_t TargetController::calibrate_channel(int idx) const {
    uint32_t sum = 0;
    for (int i = 0; i < cfg_.baseline_samples; ++i) {
        sum += read_adc(idx);
        sleep_ms(cfg_.sample_delay_ms);
    }
    return static_cast<uint16_t>(sum / static_cast<uint32_t>(cfg_.baseline_samples));
}

void TargetController::flash_idx(int idx, int times, uint32_t on_ms, uint32_t off_ms) {
    if (idx < 0 || idx >= cfg_.num_channels) return;

    for (int t = 0; t < times; ++t) {
        gpio_put(cfg_.channels[idx].led_gpio, 1);
        sleep_ms(on_ms);
        gpio_put(cfg_.channels[idx].led_gpio, 0);
        if (t + 1 < times) {
            sleep_ms(off_ms);
        }
    }
}

void TargetController::flash_zone(const char* zone_id, int times, uint32_t on_ms, uint32_t off_ms) {
    const int idx = find_channel(zone_id);
    if (idx < 0) return;
    flash_idx(idx, times, on_ms, off_ms);
}

void TargetController::flash_all(int times, uint32_t on_ms, uint32_t off_ms) {
    for (int t = 0; t < times; ++t) {
        for (int i = 0; i < cfg_.num_channels; ++i) {
            gpio_put(cfg_.channels[i].led_gpio, 1);
        }
        sleep_ms(on_ms);

        for (int i = 0; i < cfg_.num_channels; ++i) {
            gpio_put(cfg_.channels[i].led_gpio, 0);
        }

        if (t + 1 < times) {
            sleep_ms(off_ms);
        }
    }
}

void TargetController::arm_zone(const char* zone_id, int round) {
    clear_leds();

    const int idx = find_channel(zone_id);
    if (idx < 0) {
        disarm();
        return;
    }

    baselines_[idx] = calibrate_channel(idx);

    armed_ = true;
    allow_any_ = false;
    armed_idx_ = idx;
    armed_zone_id_ = cfg_.channels[idx].zone_id;
    armed_round_ = round;
    bonus_idx_ = -1;
    armed_since_ms_ = to_ms_since_boot(get_absolute_time());

    gpio_put(cfg_.channels[idx].led_gpio, 1);

    printf("ARM_ZONE: board=%s zone=%s round=%d baseline=%u\n",
           cfg_.board_id, armed_zone_id_, armed_round_, baselines_[idx]);
}

void TargetController::arm_random_zone(int round) {
    if (cfg_.num_channels <= 0) {
        disarm();
        return;
    }

    const int idx = rand() % cfg_.num_channels;
    arm_zone(cfg_.channels[idx].zone_id, round);
}

void TargetController::arm_any(int round, const char* bonus_zone) {
    clear_leds();

    for (int i = 0; i < cfg_.num_channels; ++i) {
        baselines_[i] = calibrate_channel(i);
    }

    armed_ = true;
    allow_any_ = true;
    armed_idx_ = -1;
    armed_zone_id_ = nullptr;
    armed_round_ = round;
    armed_since_ms_ = to_ms_since_boot(get_absolute_time());

    bonus_idx_ = find_channel(bonus_zone);
    if (bonus_idx_ >= 0) {
        gpio_put(cfg_.channels[bonus_idx_].led_gpio, 1);
    }

    printf("ARM_ANY: board=%s round=%d bonus=%s\n",
           cfg_.board_id,
           armed_round_,
           (bonus_idx_ >= 0) ? cfg_.channels[bonus_idx_].zone_id : "-");
}

void TargetController::disarm() {
    clear_leds();

    armed_ = false;
    allow_any_ = false;
    armed_idx_ = -1;
    armed_zone_id_ = nullptr;
    armed_round_ = -1;
    bonus_idx_ = -1;
}

TargetController::HitEvent TargetController::tick(uint32_t now_ms) {
    HitEvent ev{};

    if (!armed_) return ev;

    if ((now_ms - armed_since_ms_) > cfg_.arm_timeout_ms) {
        disarm();
        return ev;
    }

    if ((int32_t)(now_ms - cooldown_until_ms_) < 0) {
        return ev;
    }

    if (!allow_any_) {
        const uint16_t v = read_adc(armed_idx_);
        int diff = static_cast<int>(v) - static_cast<int>(baselines_[armed_idx_]);
        if (diff < 0) diff = -diff;

        if (cfg_.debug_print_ms > 0 && (now_ms - last_debug_ms_) >= cfg_.debug_print_ms) {
            last_debug_ms_ = now_ms;
            printf("ADC: board=%s zone=%s v=%u baseline=%u diff=%d thr=%u\n",
                   cfg_.board_id,
                   armed_zone_id_ ? armed_zone_id_ : "?",
                   v,
                   baselines_[armed_idx_],
                   diff,
                   cfg_.hit_delta);
        }

        if (static_cast<uint16_t>(diff) > cfg_.hit_delta) {
            ev.hit = true;
            ev.board_id = cfg_.board_id;
            ev.zone_id = armed_zone_id_;
            ev.round = armed_round_;
            ev.bonus = false;

            // brief confirmation flash for exact-zone modes too
            flash_idx(armed_idx_, 1, 60, 0);

            disarm();
            cooldown_until_ms_ = now_ms + cfg_.cooldown_ms;
        }

        return ev;
    }

    for (int i = 0; i < cfg_.num_channels; ++i) {
        const uint16_t v = read_adc(i);
        int diff = static_cast<int>(v) - static_cast<int>(baselines_[i]);
        if (diff < 0) diff = -diff;

        if (cfg_.debug_print_ms > 0 && (now_ms - last_debug_ms_) >= cfg_.debug_print_ms) {
            last_debug_ms_ = now_ms;
            printf("ADC_ANY: board=%s zone=%s v=%u baseline=%u diff=%d thr=%u\n",
                   cfg_.board_id,
                   cfg_.channels[i].zone_id,
                   v,
                   baselines_[i],
                   diff,
                   cfg_.hit_delta);
        }

        if (static_cast<uint16_t>(diff) > cfg_.hit_delta) {
            ev.hit = true;
            ev.board_id = cfg_.board_id;
            ev.zone_id = cfg_.channels[i].zone_id;
            ev.round = armed_round_;
            ev.bonus = (i == bonus_idx_);

            // this is the reverse-mode style confirmation flash you asked for
            flash_idx(i, 1, 70, 0);

            disarm();
            cooldown_until_ms_ = now_ms + cfg_.cooldown_ms;
            return ev;
        }
    }

    return ev;
}