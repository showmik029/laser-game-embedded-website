#pragma once
#include <cstdint>
#include <cstddef>

#include "pico/stdlib.h"

class TargetController {
public:
    struct Channel {
        const char* id;
        uint adc_gpio;
        uint adc_input;
        uint led_gpio;
    };

    struct Config {
        Channel channels[3] = {
            { "pico-1", 28, 2, 15 },
            { "pico-2", 27, 1, 14 },
            { "pico-3", 26, 0, 13 },
        };
        int num_channels = 3;

        int baseline_samples = 25;
        int sample_delay_ms  = 5;

        uint16_t hit_delta = 500;
        uint32_t arm_timeout_ms = 15000;
        uint32_t cooldown_ms    = 600;

        uint32_t debug_print_ms = 200;
    };

    struct HitEvent {
        bool hit = false;
        const char* target_id = nullptr;
        int round = -1;
    };

    explicit TargetController(const Config& cfg);

    void init();
    void arm(const char* target_id, int round);
    void arm_all();
    HitEvent tick(uint32_t now_ms);
    HitEvent tick_all(uint32_t now_ms);
    void disarm();

    const char* armed_target() const { return armed_id_; }
    int armed_round() const { return armed_round_; }

private:
    int find_channel(const char* id) const;
    uint16_t read_adc(int idx) const;

private:
    Config cfg_;

    int armed_idx_ = -1;
    const char* armed_id_ = nullptr;
    int armed_round_ = -1;

    uint16_t baseline_ = 0;
    uint32_t armed_since_ms_ = 0;
    uint32_t cooldown_until_ms_ = 0;

    uint32_t last_debug_ms_ = 0;

    uint16_t baselines_all_[3] = {};
    uint32_t cooldowns_all_[3] = {};
};