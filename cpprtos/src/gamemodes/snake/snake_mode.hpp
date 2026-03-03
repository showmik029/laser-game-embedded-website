#pragma once
#include <cstdint>

#include "FreeRTOS.h"
#include "gamemodes/game_mode.hpp"

class SnakeMode final : public GameMode {
public:
    void on_enter() override;
    void on_exit() override;
    void on_input(InputEvent ev) override;
    bool tick(TickType_t now) override;
    void render(Ssd1306I2C& display) override;
    bool is_finished() const override { return finished_; }

private:
    enum class Dir : uint8_t { Up, Down, Left, Right };
    enum class Overlay : uint8_t { None, PauseMenu, GameOverMenu };

    static constexpr int kCell = 4;     // pixels per cell
    static constexpr int kTop  = 8;     // top UI bar height in pixels

    static constexpr int kBorderTop    = kTop - 1;
    static constexpr int kBorderBottom = 64 - 1;
    static constexpr int kBorderLeft   = 0;
    static constexpr int kBorderRight  = 128 - 1;

    static constexpr int kInnerW = (kBorderRight - kBorderLeft - 1); // 126 px (x=1..126)
    static constexpr int kInnerH = (kBorderBottom - kTop);           // 55 px  (y=kTop..62)

    static constexpr int kGridW = kInnerW / kCell; // 31
    static constexpr int kGridH = kInnerH / kCell; // 13
    static constexpr int kMaxLen = kGridW * kGridH;

    // Center the grid inside the bordered area.
    static constexpr int kGridX0 = 1 + (kInnerW - (kGridW * kCell)) / 2;
    static constexpr int kGridY0 = kTop + (kInnerH - (kGridH * kCell)) / 2;

    // Ring-buffer snake storage (stores packed cell index y*kGridW + x)
    uint16_t body_[kMaxLen]{};
    uint16_t head_i_ = 0;   // index of current head in ring
    uint16_t len_ = 0;

    uint8_t food_x_ = 0;
    uint8_t food_y_ = 0;

    Dir dir_ = Dir::Right;

    Overlay overlay_ = Overlay::None;
    uint8_t overlay_sel_ = 0; // selection index in overlay menu

    bool paused_ = false;
    bool game_over_ = false;
    bool finished_ = false;

    TickType_t step_period_ = 0;
    TickType_t next_step_ = 0;

    bool seeded_ = false;

private:
    void reset_game(TickType_t now);
    void place_food();
    bool cell_occupied(uint8_t x, uint8_t y, bool ignore_tail_if_moving) const;

    bool step(); // returns true if state changed (moved / ate / died)

    uint16_t pack(uint8_t x, uint8_t y) const { return static_cast<uint16_t>(y * kGridW + x); }
    uint8_t  unpack_x(uint16_t p) const { return static_cast<uint8_t>(p % kGridW); }
    uint8_t  unpack_y(uint16_t p) const { return static_cast<uint8_t>(p / kGridW); }

    uint16_t seg_at(uint16_t i_from_head) const;

    void draw_cell(Ssd1306I2C& d, uint8_t cx, uint8_t cy, bool on = true) const;
};
