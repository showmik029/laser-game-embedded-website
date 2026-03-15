#pragma once

#include "../remote_target_base.hpp"

// ---------------------------------------------------------------------------
// Time Trial: both targets are armed simultaneously (arm_any). The player
// scores as many hits as possible within a fixed time window. Alternating
// targets keeps the game moving; a random bonus zone awards extra points.
// ---------------------------------------------------------------------------
class TimeTrialMode final : public RemoteTargetBase {
public:
    TimeTrialMode() = default;

    void on_enter() override;

protected:
    const char* mode_name()         const override { return "reverse"; }
    const char* payload_game_name() const override { return "time_trial"; }
    const char* mode_title()        const override { return "Time Trial"; }

    void start_game_impl(TickType_t now) override;
    bool tick_waiting(TickType_t now)    override;
    void render_body(Ssd1306I2C& display) override;

private:
    void start_opening(TickType_t now);
    void start_round(TickType_t now, const char* target);

    static constexpr uint32_t kDurationMs = 20000;

    int  normal_hits_             = 0;
    int  bonus_hits_              = 0;
    bool waiting_first_hit_       = false;
};
