#include "mqtt_game1_mode.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

static constexpr const char* kStartTopic   = "laser-labs/gamemodes/start";
static constexpr const char* kRepliesTopic = "laser-labs/gamemodes/replies";

static constexpr TickType_t kHitTimeout = pdMS_TO_TICKS(15000);
static constexpr int kMaxRetries = 2;

void MqttGame1Mode::on_enter() {
    finished_ = false;
    round_ = 0;
    retries_ = 0;
    state_ = State::NeedWifi;

    // seed rand once-ish
    static bool seeded = false;
    if (!seeded) {
        seeded = true;
        srand((unsigned)to_ms_since_boot(get_absolute_time()));
    }

    current_target_[0] = '\0';
}

void MqttGame1Mode::on_exit() {
    // Optional: publish a "stop" message later if you want
}

void MqttGame1Mode::on_input(InputEvent ev) {
    if (ev == InputEvent::Left || ev == InputEvent::Back) {
        state_ = State::Aborted;
        finished_ = true;
    }
}

bool MqttGame1Mode::parse_hit(const MqttManager::RxMessage& m, const char* expected_target) {
    // We expect targets to publish something like:
    // "game=game1;target=pico-1;event=hit;round=3"
    // We'll accept any payload containing:
    // "target=<expected_target>" AND ("hit" OR "event=hit")
    if (std::strcmp(m.topic, kRepliesTopic) != 0) return false;

    if (!expected_target || expected_target[0] == '\0') return false;

    char needle[32]{};
    std::snprintf(needle, sizeof(needle), "target=%s", expected_target);

    const bool has_target = (std::strstr(m.payload, needle) != nullptr);
    const bool has_hit = (std::strstr(m.payload, "event=hit") != nullptr) || (std::strstr(m.payload, "hit") != nullptr);

    return has_target && has_hit;
}

void MqttGame1Mode::start_next_round(TickType_t now) {
    current_target_idx_ = rand() % 2;
    std::strncpy(current_target_, targets_[current_target_idx_], sizeof(current_target_) - 1);
    current_target_[sizeof(current_target_) - 1] = '\0';

    char payload[160]{};
    std::snprintf(payload, sizeof(payload),
                  "game=game1;target=%s;round=%d",
                  current_target_, round_ + 1);

    if (mqtt_) {
        mqtt_->publish(kStartTopic, payload, /*qos=*/0, /*timeout=*/0);
        printf("GAME1: publish %s -> %s\n", kStartTopic, payload);
    }

    last_send_ = now;
    retries_ = 0;
    state_ = State::WaitingHit;
}

bool MqttGame1Mode::tick(TickType_t now) {
    if (!wifi_ || !mqtt_) return true;

    const EventBits_t wb = xEventGroupGetBits(wifi_->events());
    const EventBits_t mb = xEventGroupGetBits(mqtt_->events());

    if (state_ == State::NeedWifi) {
        if (wb & WifiManager::WIFI_CONNECTED) state_ = State::NeedMqtt;
        return true;
    }

    if (state_ == State::NeedMqtt) {
        if (mb & MqttManager::MQTT_CONNECTED) {
            start_next_round(now);
            return true;
        }
        return true;
    }

    if (state_ == State::WaitingHit) {
        // Drain incoming replies (non-blocking)
        MqttManager::RxMessage rx{};
        while (mqtt_->try_receive(rx, 0)) {
            if (parse_hit(rx, current_target_)) {
                printf("GAME1: HIT confirmed for %s (round %d)\n", current_target_, round_ + 1);
                round_++;
                if (round_ >= kRounds) {
                    state_ = State::Done;
                    finished_ = true;
                } else {
                    start_next_round(now);
                }
                return true;
            }
        }

        // Timeout => retry publish a couple times
        if ((now - last_send_) > kHitTimeout) {
            if (retries_ < kMaxRetries) {
                retries_++;
                char payload[160]{};
                std::snprintf(payload, sizeof(payload),
                              "game=game1;target=%s;round=%d;retry=%d",
                              current_target_, round_ + 1, retries_);

                mqtt_->publish(kStartTopic, payload, 0, 0);
                printf("GAME1: timeout -> retry %d for %s\n", retries_, current_target_);
                last_send_ = now;
                return true;
            } else {
                printf("GAME1: timeout -> abort\n");
                state_ = State::Aborted;
                finished_ = true;
                return true;
            }
        }

        return false;
    }

    return true;
}

void MqttGame1Mode::render(Ssd1306I2C& display) {
    display.clear();
    display.draw_text(0, 0, "Game1 (MQTT)");

    char line[32]{};
    std::snprintf(line, sizeof(line), "Round: %d/%d", round_, kRounds);
    display.draw_text(0, 16, line);

    std::snprintf(line, sizeof(line), "Target: %s", (current_target_[0] ? current_target_ : "-"));
    display.draw_text(0, 24, line);

    const char* st = "Idle";
    if (state_ == State::NeedWifi)  st = "Need Wi-Fi";
    if (state_ == State::NeedMqtt)  st = "Need MQTT";
    if (state_ == State::WaitingHit) st = "Waiting hit";
    if (state_ == State::Done)      st = "Done!";
    if (state_ == State::Aborted)   st = "Aborted";

    display.draw_text(0, 40, "Status:");
    display.draw_text(54, 40, st);

    display.draw_text(0, 56, "Left=exit");
    display.show();
}