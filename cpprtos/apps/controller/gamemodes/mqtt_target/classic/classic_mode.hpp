#pragma once

#include "../remote_target_base.hpp"

// ---------------------------------------------------------------------------
// Classic: hit 10 randomly-armed targets as fast as possible.
// Score is based on completion time and accuracy.
// ---------------------------------------------------------------------------
class ClassicMode final : public RemoteTargetBase {
public:
    ClassicMode() = default;

protected:
    const char* mode_name()         const override { return "classic"; }
    const char* payload_game_name() const override { return "classic"; }
    const char* mode_title()        const override { return "Classic"; }

    void start_game_impl(TickType_t now) override;
    bool tick_waiting(TickType_t now)    override;
    void render_body(Ssd1306I2C& display) override;

private:
    void start_round(TickType_t now);

    static constexpr int kRounds = 10;
};
