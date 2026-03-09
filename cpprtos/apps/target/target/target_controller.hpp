#pragma once
#include <cstdint>
#include <cstddef>

#include "pico/stdlib.h"

class TargetController {
public:
    struct Channel {
        const char* zone_id;
        uint adc_gpio;
        uint adc_input;
        uint led_gpio;
    };

    struct Config {
        const char* board_id = "pico-1";

        Channel channels[3] = {
            { "head",    28, 2, 15 },
            { "thorax",  27, 1, 14 },
            { "stomach", 26, 0, 13 },
        };
        int num_channels = 3;

        int baseline_samples = 25;
        int sample_delay_ms  = 5;

        uint16_t hit_delta = 30;
        uint32_t arm_timeout_ms = 15000;
        uint32_t cooldown_ms    = 600;

        uint32_t debug_print_ms = 0;
    };

    struct HitEvent {
        bool hit = false;
        const char* board_id = nullptr;
        const char* zone_id = nullptr;
        int round = -1;
        bool bonus = false;
    };

    explicit TargetController(const Config& cfg);

    void init();

    void arm_zone(const char* zone_id, int round);
    void arm_random_zone(int round);
    void arm_any(int round, const char* bonus_zone = nullptr);

    HitEvent tick(uint32_t now_ms);
    void disarm();

    void flash_zone(const char* zone_id, int times = 1, uint32_t on_ms = 60, uint32_t off_ms = 40);
    void flash_all(int times = 3, uint32_t on_ms = 90, uint32_t off_ms = 70);

    const char* board_id() const { return cfg_.board_id; }
    const char* armed_zone() const { return armed_zone_id_; }
    int armed_round() const { return armed_round_; }

private:
    int find_channel(const char* zone_id) const;
    uint16_t read_adc(int idx) const;
    uint16_t calibrate_channel(int idx) const;
    void clear_leds();
    void flash_idx(int idx, int times, uint32_t on_ms, uint32_t off_ms);

private:
    Config cfg_;

    bool armed_ = false;
    bool allow_any_ = false;

    int armed_idx_ = -1;
    const char* armed_zone_id_ = nullptr;
    int armed_round_ = -1;

    int bonus_idx_ = -1;

    uint16_t baselines_[3] = {};
    uint32_t armed_since_ms_ = 0;
    uint32_t cooldown_until_ms_ = 0;
    uint32_t last_debug_ms_ = 0;
};