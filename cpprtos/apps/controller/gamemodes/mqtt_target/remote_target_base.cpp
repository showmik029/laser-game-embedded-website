#include "remote_target_base.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>

#include "laser/laser.hpp"

// ---------------------------------------------------------------------------
// Topic constants
// ---------------------------------------------------------------------------
constexpr const char* RemoteTargetBase::kTargets[2];
constexpr const char* RemoteTargetBase::kZones[3];

static constexpr const char* kStartTopic    = "laser-labs/gamemodes/start";
static constexpr const char* kRepliesTopic  = "laser-labs/gamemodes/replies";
static constexpr const char* kFinishedTopic = "laser-labs/gamemodes/finished";

// ---------------------------------------------------------------------------
// Utility
// ---------------------------------------------------------------------------
float RemoteTargetBase::ticks_to_seconds(TickType_t ticks) {
    return static_cast<float>(ticks) / static_cast<float>(configTICK_RATE_HZ);
}

int RemoteTargetBase::round_to_int(float v) {
    return static_cast<int>(v >= 0.0f ? (v + 0.5f) : (v - 0.5f));
}

float RemoteTargetBase::clamp_min(float v, float minimum) {
    return (v < minimum) ? minimum : v;
}

float RemoteTargetBase::clamp_range(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

float RemoteTargetBase::hit_accuracy(uint32_t ammo_used, int hits_done) {
    const float shots = static_cast<float>(std::max<uint32_t>(ammo_used, 1));
    return clamp_range(static_cast<float>(hits_done) / shots, 0.0f, 1.0f);
}

bool RemoteTargetBase::kv_get(const char* payload, const char* key, char* out, size_t out_sz) {
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

int RemoteTargetBase::kv_get_int(const char* payload, const char* key, int fallback) {
    char tmp[16]{};
    if (!kv_get(payload, key, tmp, sizeof(tmp))) return fallback;
    return std::atoi(tmp);
}

int RemoteTargetBase::pick_target(int prev) {
    int idx = rand() % 2;
    if (idx == prev) idx = (idx + 1) % 2;
    return idx;
}

const char* RemoteTargetBase::other_target(const char* target) {
    if (target && std::strcmp(target, kTargets[0]) == 0) return kTargets[1];
    return kTargets[0];
}

const char* RemoteTargetBase::random_zone() {
    return kZones[rand() % 3];
}

// ---------------------------------------------------------------------------
// MQTT helpers
// ---------------------------------------------------------------------------
void RemoteTargetBase::send_command(const char* payload) {
    if (!mqtt_ || !payload) return;
    mqtt_->publish(kStartTopic, payload, 0, 0);
    printf("%s: publish %s -> %s\n", mode_title(), kStartTopic, payload);
}

void RemoteTargetBase::send_disarm(const char* target) {
    if (!target || !target[0]) return;

    char payload[96]{};
    std::snprintf(payload, sizeof(payload),
                  "game=%s;cmd=disarm;target=%s",
                  mode_name(), target);
    send_command(payload);
}

void RemoteTargetBase::send_disarm_all() {
    send_disarm(kTargets[0]);
    send_disarm(kTargets[1]);
}

bool RemoteTargetBase::parse_reply(const MqttManager::RxMessage& rx, ReplyEvent& out) const {
    if (std::strcmp(rx.topic, kRepliesTopic) != 0) return false;
    if (std::strstr(rx.payload, "event=hit") == nullptr) return false;

    if (!kv_get(rx.payload, "target", out.target, sizeof(out.target))) return false;
    if (!kv_get(rx.payload, "zone",   out.zone,   sizeof(out.zone)))   return false;

    out.round = kv_get_int(rx.payload, "round", -1);
    out.bonus = (kv_get_int(rx.payload, "bonus", 0) != 0);
    out.valid = true;
    return true;
}

// ---------------------------------------------------------------------------
// GameMode interface
// ---------------------------------------------------------------------------
void RemoteTargetBase::set_player_name(const char* name) {
    const char* src = (name && name[0]) ? name : "default";
    std::strncpy(player_name_, src, sizeof(player_name_) - 1);
    player_name_[sizeof(player_name_) - 1] = '\0';
}

void RemoteTargetBase::on_enter() {
    finished_         = false;
    result_published_ = false;
    state_            = State::NeedWifi;

    round_      = 0;
    hits_done_  = 0;

    last_target_idx_      = -1;
    current_target_[0]    = '\0';
    current_bonus_zone_[0]= '\0';

    game_start_tick_  = 0;
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

void RemoteTargetBase::on_exit() {
    send_disarm_all();
}

void RemoteTargetBase::on_input(InputEvent ev) {
    if (ev == InputEvent::Left || ev == InputEvent::Back) {
        finish_abort();
    }
}

void RemoteTargetBase::finish_success(TickType_t now, int point, int score, float time_speed) {
    if (result_published_) {
        finished_ = true;
        state_    = State::Done;
        return;
    }

    send_disarm_all();

    const uint32_t ammo_used = laser_ ? laser_->shot_count() : 0;

    if (point  < 0) point  = 0;
    if (score  < 0) score  = 0;

    char payload[256]{};
    std::snprintf(payload, sizeof(payload),
        "{\"player_name\":\"%s\",\"game_mode\":\"%s\","
        "\"score\":%d,\"point\":%d,\"time_speed\":%.2f,\"ammo_used\":%u}",
        player_name_[0] ? player_name_ : "default",
        payload_game_name(),
        score, point, time_speed,
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
    finished_         = true;
    state_            = State::Done;
}

void RemoteTargetBase::finish_abort() {
    send_disarm_all();
    finished_ = true;
    state_    = State::Aborted;
}

bool RemoteTargetBase::tick(TickType_t now) {
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
            // Flush stale messages before starting
            MqttManager::RxMessage rx{};
            while (mqtt_->try_receive(rx, 0)) {}

            game_start_tick_  = now;
            round_start_tick_ = now;
            start_game_impl(now);
        }
        return true;
    }

    if (state_ == State::WaitingHit) {
        return tick_waiting(now);
    }

    return true;
}

void RemoteTargetBase::render(Ssd1306I2C& display) {
    display.clear();
    display.draw_text(0, 0, mode_title());

    render_body(display);

    const char* st = "Idle";
    if      (state_ == State::NeedWifi)    st = "Need Wi-Fi";
    else if (state_ == State::NeedMqtt)    st = "Need MQTT";
    else if (state_ == State::WaitingHit)  st = "Waiting";
    else if (state_ == State::Done)        st = "Done";
    else if (state_ == State::Aborted)     st = "Aborted";

    char line[32]{};
    std::snprintf(line, sizeof(line), "Shots:%u",
                  laser_ ? static_cast<unsigned>(laser_->shot_count()) : 0u);
    display.draw_text(0, 44, line);
    display.draw_text(0,  52, st);
    display.draw_text(64, 52, "L=exit");
    display.show();
}
