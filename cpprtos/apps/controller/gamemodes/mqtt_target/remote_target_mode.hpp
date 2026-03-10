#pragma once

#include <cstdint>
#include <cstddef>

#include "FreeRTOS.h"

#include "gamemodes/game_mode.hpp"
#include "wifi_manager.hpp"
#include "mqtt_manager.hpp"

class Laser;

enum class RemoteGameKind : uint8_t {
    Classic,
    ReverseClassic,
    Speedup
};

class RemoteTargetMode : public GameMode {
public:
    explicit RemoteTargetMode(RemoteGameKind kind) : kind_(kind) {}

    void bind(WifiManager* wifi, MqttManager* mqtt, Laser* laser) {
        wifi_ = wifi;
        mqtt_ = mqtt;
        laser_ = laser;
    }

    void set_player_name(const char* name);

    void on_enter() override;
    void on_exit() override;
    void on_input(InputEvent ev) override;
    bool tick(TickType_t now) override;
    void render(Ssd1306I2C& display) override;
    bool is_finished() const override { return finished_; }

private:
    struct ReplyEvent {
        bool valid = false;
        char target[16]{};
        char zone[16]{};
        int round = -1;
        bool bonus = false;
    };

    enum class State : uint8_t {
        NeedWifi,
        NeedMqtt,
        WaitingHit,
        Done,
        Aborted
    };

    void start_game(TickType_t now);
    void start_classic_round(TickType_t now);
    void start_speedup_round(TickType_t now);
    void start_reverse_opening(TickType_t now);
    void start_reverse_round(TickType_t now, const char* target);

    void send_command(const char* payload);
    void send_disarm(const char* target);
    void send_disarm_all();

    bool parse_reply(const MqttManager::RxMessage& rx, ReplyEvent& out) const;

    int pick_target(int prev) const;
    const char* other_target(const char* target) const;
    const char* random_zone() const;

    void finish_success(TickType_t now);
    void finish_abort();

    const char* mode_name() const;
    const char* mode_title() const;
    const char* payload_game_name() const;

private:
    WifiManager* wifi_{nullptr};
    MqttManager* mqtt_{nullptr};
    Laser* laser_{nullptr};

    RemoteGameKind kind_;
    State state_{State::NeedWifi};
    bool finished_{false};
    bool result_published_{false};

    char player_name_[33]{"default"};

    static constexpr int kClassicRounds = 10;
    static constexpr int kClassicBaseAmmo = 133;
    static constexpr float kClassicBaseTimeS = 20.0f;

    static constexpr uint32_t kReverseDurationMs = 20000;
    static constexpr int kReverseBaseAmmo = 133;

    static constexpr float kSpeedupStartMs = 10000.0f;
    static constexpr float kSpeedupDecay = 0.9f;

    int round_{0};
    int hits_done_{0};
    int normal_hits_{0};
    int bonus_hits_{0};

    int last_target_idx_{-1};

    char current_target_[16]{};
    char current_bonus_zone_[16]{};

    bool reverse_waiting_first_hit_{false};
    float current_speedup_limit_ms_{kSpeedupStartMs};

    TickType_t game_start_tick_{0};
    TickType_t round_start_tick_{0};
};