#include "../classic_mode.hpp"

#include <cstdio>
#include <cstring>

void ClassicMode::start_game_impl(TickType_t now) {
    start_round(now);
}

void ClassicMode::start_round(TickType_t now) {
    last_target_idx_ = pick_target(last_target_idx_);
    std::strncpy(current_target_, kTargets[last_target_idx_], sizeof(current_target_) - 1);
    current_target_[sizeof(current_target_) - 1] = '\0';
    current_bonus_zone_[0] = '\0';

    char payload[128]{};
    std::snprintf(payload, sizeof(payload),
                  "game=classic;cmd=arm_random;target=%s;round=%d",
                  current_target_, round_ + 1);
    send_command(payload);

    round_start_tick_ = now;
    state_ = State::WaitingHit;
}

bool ClassicMode::tick_waiting(TickType_t now) {
    MqttManager::RxMessage rx{};
    while (mqtt_->try_receive(rx, 0)) {
        ReplyEvent ev{};
        if (!parse_reply(rx, ev)) continue;
        if (std::strcmp(ev.target, current_target_) != 0) continue;
        if (ev.round != (round_ + 1)) continue;

        ++hits_done_;
        ++round_;

        if (round_ >= kRounds) {
            // Scoring
            const float total_time_s = clamp_min(ticks_to_seconds(now - game_start_tick_), 0.01f);
            const uint32_t ammo_used = laser_ ? laser_->shot_count() : 0;
            const float accuracy     = hit_accuracy(ammo_used, hits_done_);

            const int base_points  = hits_done_ * 100;
            const float speed_factor =
                clamp_range(20.0f / clamp_min(total_time_s, 0.1f), 0.5f, 2.5f);
            const int speed_bonus  = round_to_int((speed_factor - 0.5f) * 400.0f);
            const int point        = base_points + speed_bonus;
            const int score        = point + 250 + round_to_int(accuracy * 300.0f);

            finish_success(now, point, score, speed_factor);
        } else {
            start_round(now);
        }
        return true;
    }
    return true;
}

void ClassicMode::render_body(Ssd1306I2C& display) {
    char line[32]{};

    std::snprintf(line, sizeof(line), "Round %d/%d", round_, kRounds);
    display.draw_text(0, 12, line);

    std::snprintf(line, sizeof(line), "Target:%s", current_target_[0] ? current_target_ : "-");
    display.draw_text(0, 22, line);
}
