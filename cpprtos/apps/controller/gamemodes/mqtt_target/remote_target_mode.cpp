#include "remote_target_mode.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "laser/laser.hpp"

static constexpr const char* kStartTopic    = "laser-labs/gamemodes/start";
static constexpr const char* kRepliesTopic  = "laser-labs/gamemodes/replies";
static constexpr const char* kFinishedTopic = "laser-labs/gamemodes/finished";

static constexpr const char* kTargets[2] = { "pico-1", "pico-2" };
static constexpr const char* kZones[3]   = { "head", "thorax", "stomach" };

static bool kv_get(const char* payload, const char* key, char* out, size_t out_sz) {
    if (!payload || !key || !out || out_sz == 0) return false;

    char needle[32]{};
    std::snprintf(needle, sizeof(needle), "%s=", key);

    const char* p = std::strstr(payload, needle);
    if (!p) return false;
    p += std::strlen(needle);

    size_t i = 0;
    while (p[i] && p[i] != ';' && (i + 1) < out_sz) {
        out[i] = p[i];
        ++i;
    }
    out[i] = '\0';
    return i > 0;
}

static int kv_get_int(const char* payload, const char* key, int fallback) {
    char tmp[16]{};
    if (!kv_get(payload, key, tmp, sizeof(tmp))) return fallback;
    return std::atoi(tmp);
}

static float ticks_to_seconds(TickType_t ticks) {
    return static_cast<float>(ticks) / static_cast<float>(configTICK_RATE_HZ);
}

static int round_to_int(float v) {
    return static_cast<int>(v >= 0.0f ? (v + 0.5f) : (v - 0.5f));
}

static float clamp_min(float v, float minimum) {
    return (v < minimum) ? minimum : v;
}

void RemoteTargetMode::set_player_name(const char* name) {
    const char* src = (name && name[0]) ? name : "default";
    std::strncpy(player_name_, src, sizeof(player_name_) - 1);
    player_name_[sizeof(player_name_) - 1] = '\0';
}

const char* RemoteTargetMode::mode_name() const {
    switch (kind_) {
        case RemoteGameKind::Classic:        return "classic";
        case RemoteGameKind::ReverseClassic: return "reverse";
        case RemoteGameKind::Speedup:        return "speedup";
    }
    return "classic";
}

const char* RemoteTargetMode::payload_game_name() const {
    switch (kind_) {
        case RemoteGameKind::Classic:        return "classic";
        case RemoteGameKind::ReverseClassic: return "time_trial";
        case RemoteGameKind::Speedup:        return "photon_panic";
    }
    return "classic";
}

const char* RemoteTargetMode::mode_title() const {
    switch (kind_) {
        case RemoteGameKind::Classic:        return "Classic";
        case RemoteGameKind::ReverseClassic: return "Time Trial";
        case RemoteGameKind::Speedup:        return "Photon Panic";
    }
    return "Classic";
}

int RemoteTargetMode::pick_target(int prev) const {
    int idx = rand() % 2;
    if (idx == prev) idx = (idx + 1) % 2;
    return idx;
}

const char* RemoteTargetMode::other_target(const char* target) const {
    if (target && std::strcmp(target, kTargets[0]) == 0) return kTargets[1];
    return kTargets[0];
}

const char* RemoteTargetMode::random_zone() const {
    return kZones[rand() % 3];
}

void RemoteTargetMode::send_command(const char* payload) {
    if (!mqtt_ || !payload) return;
    mqtt_->publish(kStartTopic, payload, 0, 0);
    printf("%s: publish %s -> %s\n", mode_title(), kStartTopic, payload);
}

void RemoteTargetMode::send_disarm(const char* target) {
    if (!target || !target[0]) return;

    char payload[96]{};
    std::snprintf(payload, sizeof(payload),
                  "game=%s;cmd=disarm;target=%s",
                  mode_name(), target);
    send_command(payload);
}

void RemoteTargetMode::send_disarm_all() {
    send_disarm(kTargets[0]);
    send_disarm(kTargets[1]);
}

void RemoteTargetMode::on_enter() {
    finished_ = false;
    result_published_ = false;
    state_ = State::NeedWifi;

    round_ = 0;
    hits_done_ = 0;
    normal_hits_ = 0;
    bonus_hits_ = 0;

    last_target_idx_ = -1;
    current_target_[0] = '\0';
    current_bonus_zone_[0] = '\0';

    reverse_waiting_first_hit_ = false;
    current_speedup_limit_ms_ = kSpeedupStartMs;

    game_start_tick_ = 0;
    round_start_tick_ = 0;

    static bool seeded = false;
    if (!seeded) {
        seeded = true;
        srand(static_cast<unsigned>(to_ms_since_boot(get_absolute_time())));
    }

    if (laser_) {
        laser_->reset_shot_count();
        laser_->setEnabled(true);
    }
}

void RemoteTargetMode::on_exit() {
    send_disarm_all();
}

void RemoteTargetMode::on_input(InputEvent ev) {
    if (ev == InputEvent::Left || ev == InputEvent::Back) {
        finish_abort();
    }
}

void RemoteTargetMode::start_classic_round(TickType_t now) {
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

void RemoteTargetMode::start_speedup_round(TickType_t now) {
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

void RemoteTargetMode::start_reverse_opening(TickType_t now) {
    current_target_[0] = '\0';
    current_bonus_zone_[0] = '\0';
    reverse_waiting_first_hit_ = true;

    char payload1[128]{};
    std::snprintf(payload1, sizeof(payload1),
                  "game=reverse;cmd=arm_any;target=%s;round=%d",
                  kTargets[0], round_ + 1);

    char payload2[128]{};
    std::snprintf(payload2, sizeof(payload2),
                  "game=reverse;cmd=arm_any;target=%s;round=%d",
                  kTargets[1], round_ + 1);

    send_command(payload1);
    send_command(payload2);

    round_start_tick_ = now;
    state_ = State::WaitingHit;
}

void RemoteTargetMode::start_reverse_round(TickType_t now, const char* target) {
    send_disarm_all();

    std::strncpy(current_target_, target, sizeof(current_target_) - 1);
    current_target_[sizeof(current_target_) - 1] = '\0';
    reverse_waiting_first_hit_ = false;

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

void RemoteTargetMode::start_game(TickType_t now) {
    if (!mqtt_) return;

    MqttManager::RxMessage rx{};
    while (mqtt_->try_receive(rx, 0)) {
        // flush stale messages
    }

    game_start_tick_ = now;
    round_start_tick_ = now;

    switch (kind_) {
        case RemoteGameKind::Classic:
            start_classic_round(now);
            break;
        case RemoteGameKind::ReverseClassic:
            start_reverse_opening(now);
            break;
        case RemoteGameKind::Speedup:
            start_speedup_round(now);
            break;
    }
}

bool RemoteTargetMode::parse_reply(const MqttManager::RxMessage& rx, ReplyEvent& out) const {
    if (std::strcmp(rx.topic, kRepliesTopic) != 0) return false;
    if (std::strstr(rx.payload, "event=hit") == nullptr) return false;

    if (!kv_get(rx.payload, "target", out.target, sizeof(out.target))) return false;
    if (!kv_get(rx.payload, "zone", out.zone, sizeof(out.zone))) return false;

    out.round = kv_get_int(rx.payload, "round", -1);
    out.bonus = (kv_get_int(rx.payload, "bonus", 0) != 0);
    out.valid = true;
    return true;
}

void RemoteTargetMode::finish_success(TickType_t now) {
    if (result_published_) {
        finished_ = true;
        state_ = State::Done;
        return;
    }

    send_disarm_all();

    const uint32_t ammo_used = laser_ ? laser_->shot_count() : 0;
    const float total_time_s = clamp_min(ticks_to_seconds(now - game_start_tick_), 0.01f);

    int point = 0;
    int score = 0;
    float time_speed = 0.0f;

    if (kind_ == RemoteGameKind::Classic) {
        const float time_factor = kClassicBaseTimeS / clamp_min(total_time_s, 0.1f);
        const float ammo_factor = static_cast<float>(kClassicBaseAmmo) /
                                  clamp_min(static_cast<float>(ammo_used == 0 ? 1 : ammo_used), 1.0f);
        const float weighted = 0.666f * time_factor + 0.333f * ammo_factor;

        point = round_to_int(10.0f * weighted);
        score = round_to_int(static_cast<float>(point) * ammo_factor);
        time_speed = time_factor;
    } else if (kind_ == RemoteGameKind::ReverseClassic) {
        const int raw_points = normal_hits_ + (bonus_hits_ * 2);
        const float pace = static_cast<float>(raw_points) / 10.0f;
        const float ammo_factor = static_cast<float>(kReverseBaseAmmo) /
                                  clamp_min(static_cast<float>(ammo_used == 0 ? 1 : ammo_used), 1.0f);
        const float weighted = 0.666f * pace + 0.333f * ammo_factor;

        point = round_to_int(10.0f * weighted);
        score = round_to_int(static_cast<float>(point) * ammo_factor);
        time_speed = pace;
    } else {
        const int baseline_ammo = (hits_done_ > 0) ? (hits_done_ * 10) : 1;
        const float ammo_factor = static_cast<float>(baseline_ammo) /
                                  clamp_min(static_cast<float>(ammo_used == 0 ? 1 : ammo_used), 1.0f);

        point = hits_done_;
        score = round_to_int(static_cast<float>(point) * (0.85f + 0.15f * ammo_factor));
        time_speed = total_time_s;
    }

    if (point < 0) point = 0;
    if (score < 0) score = 0;

    char payload[256]{};
    std::snprintf(payload, sizeof(payload),
              "{\"player_name\":\"%s\",\"game_mode\":\"%s\",\"score\":%d,\"point\":%d,\"time_speed\":%.2f,\"ammo_used\":%u}",
              player_name_[0] ? player_name_ : "default",
              payload_game_name(),
              score,
              point,
              time_speed,
              static_cast<unsigned>(ammo_used));

    char celebrate1[96]{};
    char celebrate2[96]{};

    std::snprintf(celebrate1, sizeof(celebrate1),
                  "game=%s;cmd=celebrate;target=pico-1", mode_name());
    std::snprintf(celebrate2, sizeof(celebrate2),
                  "game=%s;cmd=celebrate;target=pico-2", mode_name());

    send_command(celebrate1);
    send_command(celebrate2);

    if (mqtt_) {
        mqtt_->publish(kFinishedTopic, payload, 0, 0);
        printf("%s: publish %s -> %s\n", mode_title(), kFinishedTopic, payload);
    }

    result_published_ = true;
    finished_ = true;
    state_ = State::Done;
}

void RemoteTargetMode::finish_abort() {
    send_disarm_all();
    finished_ = true;
    state_ = State::Aborted;
}

bool RemoteTargetMode::tick(TickType_t now) {
    if (!wifi_ || !mqtt_) return true;

    const EventBits_t wb = xEventGroupGetBits(wifi_->events());
    const EventBits_t mb = xEventGroupGetBits(mqtt_->events());

    if (state_ == State::NeedWifi) {
        if (wb & WifiManager::WIFI_CONNECTED) {
            state_ = State::NeedMqtt;
        }
        return true;
    }

    if (state_ == State::NeedMqtt) {
        if (mb & MqttManager::MQTT_CONNECTED) {
            start_game(now);
        }
        return true;
    }

    if (state_ == State::WaitingHit) {
        if (kind_ == RemoteGameKind::ReverseClassic) {
            const uint32_t elapsed_ms =
                static_cast<uint32_t>(ticks_to_seconds(now - game_start_tick_) * 1000.0f);
            if (elapsed_ms >= kReverseDurationMs) {
                finish_success(now);
                return true;
            }
        }

        if (kind_ == RemoteGameKind::Speedup) {
            const float elapsed_ms =
                ticks_to_seconds(now - round_start_tick_) * 1000.0f;
            if (elapsed_ms >= current_speedup_limit_ms_) {
                finish_success(now);
                return true;
            }
        }

        MqttManager::RxMessage rx{};
        while (mqtt_->try_receive(rx, 0)) {
            ReplyEvent ev{};
            if (!parse_reply(rx, ev)) {
                continue;
            }

            if (kind_ == RemoteGameKind::Classic) {
                if (std::strcmp(ev.target, current_target_) != 0) continue;
                if (ev.round != (round_ + 1)) continue;

                ++hits_done_;
                ++round_;

                if (round_ >= kClassicRounds) {
                    finish_success(now);
                } else {
                    start_classic_round(now);
                }
                return true;
            }

            if (kind_ == RemoteGameKind::Speedup) {
                if (std::strcmp(ev.target, current_target_) != 0) continue;
                if (ev.round != (round_ + 1)) continue;

                ++hits_done_;
                ++round_;
                current_speedup_limit_ms_ *= kSpeedupDecay;
                start_speedup_round(now);
                return true;
            }

            if (kind_ == RemoteGameKind::ReverseClassic) {
                if (ev.round != (round_ + 1)) continue;

                if (reverse_waiting_first_hit_) {
                    ++hits_done_;
                    ++round_;
                    if (ev.bonus) ++bonus_hits_;
                    else ++normal_hits_;

                    start_reverse_round(now, other_target(ev.target));
                    return true;
                }

                if (std::strcmp(ev.target, current_target_) != 0) continue;

                ++hits_done_;
                ++round_;
                if (ev.bonus) ++bonus_hits_;
                else ++normal_hits_;

                start_reverse_round(now, other_target(ev.target));
                return true;
            }
        }

        return true;
    }

    return true;
}

void RemoteTargetMode::render(Ssd1306I2C& display) {
    display.clear();
    display.draw_text(0, 0, mode_title());

    char line[32]{};

    if (kind_ == RemoteGameKind::Classic) {
        std::snprintf(line, sizeof(line), "Round %d/%d", round_, kClassicRounds);
        display.draw_text(0, 12, line);

        std::snprintf(line, sizeof(line), "Target:%s", current_target_[0] ? current_target_ : "-");
        display.draw_text(0, 22, line);
    } else if (kind_ == RemoteGameKind::ReverseClassic) {
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
    } else {
        std::snprintf(line, sizeof(line), "Hits:%d", hits_done_);
        display.draw_text(0, 12, line);

        std::snprintf(line, sizeof(line), "Target:%s", current_target_[0] ? current_target_ : "-");
        display.draw_text(0, 22, line);

        std::snprintf(line, sizeof(line), "Time:%.1f", current_speedup_limit_ms_ / 1000.0f);
        display.draw_text(0, 32, line);
    }

    const char* st = "Idle";
    if (state_ == State::NeedWifi) st = "Need Wi-Fi";
    else if (state_ == State::NeedMqtt) st = "Need MQTT";
    else if (state_ == State::WaitingHit) st = "Waiting";
    else if (state_ == State::Done) st = "Done";
    else if (state_ == State::Aborted) st = "Aborted";

    std::snprintf(line, sizeof(line), "Shots:%u", laser_ ? static_cast<unsigned>(laser_->shot_count()) : 0);
    display.draw_text(0, 44, line);

    display.draw_text(0, 52, st);
    display.draw_text(64, 52, "L=exit");
    display.show();
}