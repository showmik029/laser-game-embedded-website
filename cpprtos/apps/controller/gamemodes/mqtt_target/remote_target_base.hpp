#pragma once

#include <cstdint>
#include <cstddef>

#include "FreeRTOS.h"

#include "gamemodes/game_mode.hpp"
#include "wifi_manager.hpp"
#include "mqtt_manager.hpp"

class Laser;

// ---------------------------------------------------------------------------
// Shared base for Classic, Time Trial and Photon Panic game modes.
// Each subclass implements: mode_name(), payload_game_name(), mode_title(),
// start_game_impl(), tick_waiting() and render_body().
// ---------------------------------------------------------------------------
class RemoteTargetBase : public GameMode {
public:
    void bind(WifiManager* wifi, MqttManager* mqtt, Laser* laser) {
        wifi_  = wifi;
        mqtt_  = mqtt;
        laser_ = laser;
    }

    void set_player_name(const char* name);

    // GameMode interface
    void on_enter() override;
    void on_exit()  override;
    void on_input(InputEvent ev) override;
    bool tick(TickType_t now)    override;
    void render(Ssd1306I2C& display) override;
    bool is_finished() const override { return finished_; }

protected:

    virtual const char* mode_name()         const = 0;
    virtual const char* payload_game_name() const = 0;
    virtual const char* mode_title()        const = 0;

    // Called once when wifi+mqtt are ready. Should arm the first target and
    // set state_ = State::WaitingHit.
    virtual void start_game_impl(TickType_t now) = 0;

    // Called every tick while state_ == WaitingHit.
    // Should process incoming MQTT replies and handle timeouts.
    // Returns true if display should be redrawn.
    virtual bool tick_waiting(TickType_t now) = 0;

    // Render the game-specific body lines (y >= 12).
    virtual void render_body(Ssd1306I2C& display) = 0;

    // -----------------------------------------------------------------------
    // Helpers available to subclasses
    // -----------------------------------------------------------------------
    struct ReplyEvent {
        bool valid  = false;
        char target[16]{};
        char zone[16]{};
        int  round  = -1;
        bool bonus  = false;
    };

    enum class State : uint8_t {
        NeedWifi,
        NeedMqtt,
        WaitingHit,
        Done,
        Aborted
    };

    bool parse_reply(const MqttManager::RxMessage& rx, ReplyEvent& out) const;

    void send_command(const char* payload);
    void send_disarm(const char* target);
    void send_disarm_all();

    void finish_success(TickType_t now, int point, int score, float time_speed);
    void finish_abort();

    // Utility
    static float ticks_to_seconds(TickType_t ticks);
    static int   round_to_int(float v);
    static float clamp_min(float v, float minimum);
    static float clamp_range(float v, float lo, float hi);
    static float hit_accuracy(uint32_t ammo_used, int hits_done);
    static bool  kv_get(const char* payload, const char* key, char* out, size_t out_sz);
    static int   kv_get_int(const char* payload, const char* key, int fallback);

    // Shared target/zone helpers
    static int         pick_target(int prev);
    static const char* other_target(const char* target);
    static const char* random_zone();

    // State visible to subclasses
    State    state_          = State::NeedWifi;
    bool     finished_       = false;
    bool     result_published_ = false;

    int      round_          = 0;
    int      hits_done_      = 0;

    int      last_target_idx_    = -1;
    char     current_target_[16]{};
    char     current_bonus_zone_[16]{};

    TickType_t game_start_tick_  = 0;
    TickType_t round_start_tick_ = 0;

    char     player_name_[33]{"default"};

    WifiManager* wifi_  = nullptr;
    MqttManager* mqtt_  = nullptr;
    Laser*       laser_ = nullptr;

    static constexpr const char* kTargets[2] = { "pico-1", "pico-2" };
    static constexpr const char* kZones[3]   = { "head", "thorax", "stomach" };

private:
    static constexpr const char* kStartTopic    = "laser-labs/gamemodes/start";
    static constexpr const char* kRepliesTopic  = "laser-labs/gamemodes/replies";
    static constexpr const char* kFinishedTopic = "laser-labs/gamemodes/finished";
};
