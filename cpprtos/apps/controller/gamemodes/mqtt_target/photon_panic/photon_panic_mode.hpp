#pragma once

#include "../remote_target_base.hpp"

// ---------------------------------------------------------------------------
// Photon Panic: hit targets against a shrinking timer. Each successful hit
// reduces the allowed reaction window by 10 %. Missing the window ends
// the game and scores what was achieved so far.
// ---------------------------------------------------------------------------
class PhotonPanicMode final : public RemoteTargetBase {
public:
    PhotonPanicMode() = default;

    void on_enter() override;

protected:
    const char* mode_name()         const override { return "speedup"; }
    const char* payload_game_name() const override { return "photon_panic"; }
    const char* mode_title()        const override { return "Photon Panic"; }

    void start_game_impl(TickType_t now) override;
    bool tick_waiting(TickType_t now)    override;
    void render_body(Ssd1306I2C& display) override;

private:
    void start_round(TickType_t now);
    int  calc_points() const;

    static constexpr float kStartMs = 10000.0f;
    static constexpr float kDecay   = 0.9f;

    float current_limit_ms_ = kStartMs;
};
