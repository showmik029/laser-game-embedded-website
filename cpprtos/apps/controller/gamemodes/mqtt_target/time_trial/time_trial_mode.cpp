#include "../time_trial_mode.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

void TimeTrialMode::on_enter() {
    RemoteTargetBase::on_enter();
    normal_hits_       = 0;
    bonus_hits_        = 0;
    waiting_first_hit_ = false;
}

void TimeTrialMode::start_game_impl(TickType_t now) {
    start_opening(now);
}

void TimeTrialMode::start_opening(TickType_t now) {
    current_target_[0]    = '\0';
    current_bonus_zone_[0]= '\0';
    waiting_first_hit_    = true;

    char p1[128]{};
    char p2[128]{};
    std::snprintf(p1, sizeof(p1),
                  "game=reverse;cmd=arm_any;target=%s;round=%d",
                  kTargets[0], round_ + 1);
    std::snprintf(p2, sizeof(p2),
                  "game=reverse;cmd=arm_any;target=%s;round=%d",
                  kTargets[1], round_ + 1);

    send_command(p1);
    send_command(p2);

    round_start_tick_ = now;
    state_ = State::WaitingHit;
}

void TimeTrialMode::start_round(TickType_t now, const char* target) {
    send_disarm_all();

    std::strncpy(current_target_, target, sizeof(current_target_) - 1);
    current_target_[sizeof(current_target_) - 1] = '\0';
    waiting_first_hit_ = false;

    current_bonus_zone_[0] = '\0';
    const bool use_bonus = ((rand() % 10) == 0);
    if (use_bonus) {
        std::strncpy(current_bonus_zone_, random_zone(), sizeof(current_bonus_zone_) - 1);
        current_bonus_zone_[sizeof(current_bonus_zone_) - 1] = '\0';
    }

    char payload[160]{};
    if (current_bonus_zone_[0]) {
        std::snprintf(payload, sizeof(payload),
                      "game=reverse;cmd=arm_any;target=%s;round=%d;bonus_zone=%s",
                      current_target_, round_ + 1, current_bonus_zone_);
    } else {
        std::snprintf(payload, sizeof(payload),
                      "game=reverse;cmd=arm_any;target=%s;round=%d",
                      current_target_, round_ + 1);
    }

    send_command(payload);
    round_start_tick_ = now;
    state_ = State::WaitingHit;
}

bool TimeTrialMode::tick_waiting(TickType_t now) {
    // Time's up?
    const uint32_t elapsed_ms =
        static_cast<uint32_t>(ticks_to_seconds(now - game_start_tick_) * 1000.0f);
    if (elapsed_ms >= kDurationMs) {
        const uint32_t ammo_used = laser_ ? laser_->shot_count() : 0;
        const float accuracy     = hit_accuracy(ammo_used, hits_done_);

        const int point      = (normal_hits_ * 125) + (bonus_hits_ * 250);
        const int pace_bonus = round_to_int(
            static_cast<float>(point) / (static_cast<float>(kDurationMs) / 1000.0f) * 4.0f);
        const int score      = point + pace_bonus + round_to_int(accuracy * 250.0f);
        const float time_speed =
            static_cast<float>(hits_done_) / (static_cast<float>(kDurationMs) / 1000.0f);

        finish_success(now, point, score, time_speed);
        return true;
    }

    MqttManager::RxMessage rx{};
    while (mqtt_->try_receive(rx, 0)) {
        ReplyEvent ev{};
        if (!parse_reply(rx, ev)) continue;
        if (ev.round != (round_ + 1)) continue;

        if (waiting_first_hit_) {
            ++hits_done_;
            ++round_;
            if (ev.bonus) ++bonus_hits_;
            else          ++normal_hits_;
            start_round(now, other_target(ev.target));
            return true;
        }

        if (std::strcmp(ev.target, current_target_) != 0) continue;

        ++hits_done_;
        ++round_;
        if (ev.bonus) ++bonus_hits_;
        else          ++normal_hits_;
        start_round(now, other_target(ev.target));
        return true;
    }
    return true;
}

void TimeTrialMode::render_body(Ssd1306I2C& display) {
    char line[32]{};

    std::snprintf(line, sizeof(line), "Hits:%d", hits_done_);
    display.draw_text(0, 12, line);

    if (current_target_[0]) {
        std::snprintf(line, sizeof(line), "Next:%s", current_target_);
    } else {
        std::snprintf(line, sizeof(line), "Shoot either");
    }
    display.draw_text(0, 22, line);

    if (current_bonus_zone_[0]) {
        std::snprintf(line, sizeof(line), "Bonus:%s", current_bonus_zone_);
        display.draw_text(0, 32, line);
    }
}
