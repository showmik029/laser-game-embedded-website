#include "../photon_panic_mode.hpp"

#include <cstdio>
#include <cstring>

void PhotonPanicMode::on_enter() {
    RemoteTargetBase::on_enter();
    current_limit_ms_ = kStartMs;
}

void PhotonPanicMode::start_game_impl(TickType_t now) {
    start_round(now);
}

void PhotonPanicMode::start_round(TickType_t now) {
    last_target_idx_ = pick_target(last_target_idx_);
    std::strncpy(current_target_, kTargets[last_target_idx_], sizeof(current_target_) - 1);
    current_target_[sizeof(current_target_) - 1] = '\0';
    current_bonus_zone_[0] = '\0';

    char payload[128]{};
    std::snprintf(payload, sizeof(payload),
                  "game=speedup;cmd=arm_random;target=%s;round=%d",
                  current_target_, round_ + 1);
    send_command(payload);

    round_start_tick_ = now;
    state_ = State::WaitingHit;
}

int PhotonPanicMode::calc_points() const {
    float limit_ms = kStartMs;
    int total = 0;

    for (int i = 0; i < hits_done_; ++i) {
        const float difficulty = clamp_range(1.0f - (limit_ms / kStartMs), 0.0f, 1.0f);
        const int per_hit = 100 + round_to_int(difficulty * 250.0f);
        total += per_hit;
        limit_ms *= kDecay;
    }
    return total;
}

bool PhotonPanicMode::tick_waiting(TickType_t now) {
    // Window expired -> game over
    const float elapsed_ms = ticks_to_seconds(now - round_start_tick_) * 1000.0f;
    if (elapsed_ms >= current_limit_ms_) {
        const uint32_t ammo_used = laser_ ? laser_->shot_count() : 0;
        const float accuracy     = hit_accuracy(ammo_used, hits_done_);

        const int point = calc_points();
        const int score = point + (hits_done_ * 30) + round_to_int(accuracy * 300.0f);
        const float time_speed = kStartMs / clamp_min(current_limit_ms_, 1.0f);

        finish_success(now, point, score, time_speed);
        return true;
    }

    MqttManager::RxMessage rx{};
    while (mqtt_->try_receive(rx, 0)) {
        ReplyEvent ev{};
        if (!parse_reply(rx, ev)) continue;
        if (std::strcmp(ev.target, current_target_) != 0) continue;
        if (ev.round != (round_ + 1)) continue;

        ++hits_done_;
        ++round_;
        current_limit_ms_ *= kDecay;
        start_round(now);
        return true;
    }
    return true;
}

void PhotonPanicMode::render_body(Ssd1306I2C& display) {
    char line[32]{};

    std::snprintf(line, sizeof(line), "Hits:%d", hits_done_);
    display.draw_text(0, 12, line);

    std::snprintf(line, sizeof(line), "Target:%s", current_target_[0] ? current_target_ : "-");
    display.draw_text(0, 22, line);

    std::snprintf(line, sizeof(line), "Time:%.1f", current_limit_ms_ / 1000.0f);
    display.draw_text(0, 32, line);
}
