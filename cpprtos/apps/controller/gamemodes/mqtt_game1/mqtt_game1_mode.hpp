#pragma once

#include <cstdint>
#include <cstddef>

#include "FreeRTOS.h"

#include "gamemodes/game_mode.hpp"
#include "wifi_manager.hpp"
#include "mqtt_manager.hpp"

class MqttGame1Mode : public GameMode {
public:
    void bind(WifiManager* wifi, MqttManager* mqtt) { wifi_ = wifi; mqtt_ = mqtt; }

    void on_enter() override;
    void on_exit() override;
    void on_input(InputEvent ev) override;
    bool tick(TickType_t now) override;
    void render(Ssd1306I2C& display) override;
    bool is_finished() const override { return finished_; }

private:
    void start_next_round(TickType_t now);
    bool parse_hit(const MqttManager::RxMessage& m, const char* expected_target);

private:
    WifiManager* wifi_{nullptr};
    MqttManager* mqtt_{nullptr};

    bool finished_{false};

    static constexpr int kRounds = 10;
    int round_{0};

    const char* targets_[2] = {"pico-1", "pico-2"};
    int current_target_idx_{0};
    char current_target_[16]{};

    enum class State { NeedWifi, NeedMqtt, WaitingHit, Done, Aborted };
    State state_{State::NeedWifi};

    TickType_t last_send_{0};
    int retries_{0};
};