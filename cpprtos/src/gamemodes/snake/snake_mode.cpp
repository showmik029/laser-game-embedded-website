#include "snake_mode.hpp"

#include <cstdio>
#include <cstdlib>

#include "pico/stdlib.h"

void SnakeMode::on_enter() {
    finished_ = false;
    paused_ = false;
    game_over_ = false;
    overlay_ = Overlay::None;
    overlay_sel_ = 0;

    step_period_ = pdMS_TO_TICKS(200);

    const TickType_t now = xTaskGetTickCount();
    reset_game(now);

    if (!seeded_) {
        seeded_ = true;
        srand((unsigned)time_us_32());
    }
}

void SnakeMode::on_exit() {
    // Nothing to clean up for now
}

void SnakeMode::reset_game(TickType_t now) {
    len_ = 4;
    head_i_ = 0;
    dir_ = Dir::Right;

    const uint8_t sx = kGridW / 2;
    const uint8_t sy = kGridH / 2;

    // Head at [0], then older segments at the end of the ring.
    body_[0] = pack(sx, sy);
    body_[kMaxLen - 1] = pack((uint8_t)(sx - 1), sy);
    body_[kMaxLen - 2] = pack((uint8_t)(sx - 2), sy);
    body_[kMaxLen - 3] = pack((uint8_t)(sx - 3), sy);

    paused_ = false;
    game_over_ = false;
    overlay_ = Overlay::None;
    overlay_sel_ = 0;

    place_food();

    next_step_ = now + step_period_;
}

uint16_t SnakeMode::seg_at(uint16_t i_from_head) const {
    const uint16_t idx = (head_i_ + kMaxLen - i_from_head) % kMaxLen;
    return body_[idx];
}

bool SnakeMode::cell_occupied(uint8_t x, uint8_t y, bool ignore_tail_if_moving) const {
    const uint16_t p = pack(x, y);
    for (uint16_t i = 0; i < len_; ++i) {
        if (ignore_tail_if_moving && i == (len_ - 1)) continue;
        if (seg_at(i) == p) return true;
    }
    return false;
}

void SnakeMode::place_food() {
    // Try random spots; fallback to scan if needed
    for (int tries = 0; tries < 300; ++tries) {
        const uint8_t x = (uint8_t)(rand() % kGridW);
        const uint8_t y = (uint8_t)(rand() % kGridH);
        if (!cell_occupied(x, y, /*ignore_tail_if_moving=*/false)) {
            food_x_ = x;
            food_y_ = y;
            return;
        }
    }

    // Scan fallback
    for (uint8_t y = 0; y < kGridH; ++y) {
        for (uint8_t x = 0; x < kGridW; ++x) {
            if (!cell_occupied(x, y, /*ignore_tail_if_moving=*/false)) {
                food_x_ = x;
                food_y_ = y;
                return;
            }
        }
    }

    // No free cells -> win
    game_over_ = true;
    overlay_ = Overlay::GameOverMenu;
    overlay_sel_ = 0;
}

void SnakeMode::on_input(InputEvent ev) {
    if (finished_) return;

    // Overlay menus (pause / game over)
    if (overlay_ != Overlay::None) {
        if (ev == InputEvent::Up || ev == InputEvent::Down) {
            overlay_sel_ ^= 1; // toggle between 0 and 1
        } else if (ev == InputEvent::Select) {
            if (overlay_ == Overlay::PauseMenu) {
                if (overlay_sel_ == 0) {
                    // Resume
                    overlay_ = Overlay::None;
                    paused_ = false;
                    next_step_ = xTaskGetTickCount() + step_period_;
                } else {
                    // Exit
                    finished_ = true;
                }
            } else { // GameOverMenu
                if (overlay_sel_ == 0) {
                    // Restart
                    reset_game(xTaskGetTickCount());
                } else {
                    // Exit
                    finished_ = true;
                }
            }
        }
        return;
    }

    // In-game
    if (ev == InputEvent::Select) {
        paused_ = true;
        overlay_ = Overlay::PauseMenu;
        overlay_sel_ = 0;
        return;
    }

    // Direction changes (no 180-degree reverse)
    switch (ev) {
        case InputEvent::Up:
            if (dir_ != Dir::Down) dir_ = Dir::Up;
            break;
        case InputEvent::Down:
            if (dir_ != Dir::Up) dir_ = Dir::Down;
            break;
        case InputEvent::Left:
            if (dir_ != Dir::Right) dir_ = Dir::Left;
            break;
        case InputEvent::Right:
            if (dir_ != Dir::Left) dir_ = Dir::Right;
            break;
        default:
            break;
    }
}

bool SnakeMode::tick(TickType_t now) {
    if (finished_) return true;

    if (overlay_ != Overlay::None) {
        return false;
    }
    if (paused_ || game_over_) return false;

    if (now >= next_step_) {
        next_step_ = now + step_period_;
        return step();
    }
    return false;
}

bool SnakeMode::step() {
    // Current head
    const uint16_t head_p = seg_at(0);
    int x = (int)unpack_x(head_p);
    int y = (int)unpack_y(head_p);

    switch (dir_) {
        case Dir::Up:    y -= 1; break;
        case Dir::Down:  y += 1; break;
        case Dir::Left:  x -= 1; break;
        case Dir::Right: x += 1; break;
    }

    // Wall collision => game over
    if (x < 0 || x >= kGridW || y < 0 || y >= kGridH) {
        game_over_ = true;
        overlay_ = Overlay::GameOverMenu;
        overlay_sel_ = 0;
        return true;
    }

    const uint8_t nx = (uint8_t)x;
    const uint8_t ny = (uint8_t)y;

    const bool grow = (nx == food_x_ && ny == food_y_);

    // Self collision
    if (cell_occupied(nx, ny, /*ignore_tail_if_moving=*/!grow)) {
        game_over_ = true;
        overlay_ = Overlay::GameOverMenu;
        overlay_sel_ = 0;
        return true;
    }

    // Advance head in ring
    head_i_ = (uint16_t)((head_i_ + 1) % kMaxLen);
    body_[head_i_] = pack(nx, ny);

    if (grow) {
        if (len_ < kMaxLen) {
            ++len_;
            place_food();
        } else {
            // Filled entire grid: win
            game_over_ = true;
            overlay_ = Overlay::GameOverMenu;
            overlay_sel_ = 0;
        }
    }

    return true;
}

void SnakeMode::draw_cell(Ssd1306I2C& d, uint8_t cx, uint8_t cy, bool on) const {
    const int px = kGridX0 + (int)cx * kCell;
    const int py = kGridY0 + (int)cy * kCell;

    for (int dy = 0; dy < (kCell - 1); ++dy) {
        for (int dx = 0; dx < (kCell - 1); ++dx) {
            d.set_pixel(px + dx, py + dy, on);
        }
    }
}

void SnakeMode::render(Ssd1306I2C& display) {
    display.clear();

    for (int x = 0; x < display.width(); ++x) {
        display.set_pixel(x, kBorderTop, true);
        display.set_pixel(x, kBorderBottom, true);
    }
    for (int y = kBorderTop; y <= kBorderBottom; ++y) {
        display.set_pixel(kBorderLeft, y, true);
        display.set_pixel(kBorderRight, y, true);
    }

    // Score line
    char top[22]{};
    const int score = (len_ >= 4) ? (int)(len_ - 4) : 0;
    std::snprintf(top, sizeof(top), "Snake  Score:%d", score);
    display.draw_text(0, 0, top);

    // Food
    draw_cell(display, food_x_, food_y_, true);

    // Snake body
    for (uint16_t i = 0; i < len_; ++i) {
        const uint16_t p = seg_at(i);
        draw_cell(display, unpack_x(p), unpack_y(p), true);
    }

    // Overlay menus
    if (overlay_ == Overlay::PauseMenu) {
        display.draw_text(0, 16, "PAUSED");
        if (overlay_sel_ == 0) display.draw_text(0, 32, ">");
        display.draw_text(10, 32, "Resume");
        if (overlay_sel_ == 1) display.draw_text(0, 40, ">");
        display.draw_text(10, 40, "Exit");
    } else if (overlay_ == Overlay::GameOverMenu) {
        display.draw_text(0, 16, "GAME OVER");
        if (overlay_sel_ == 0) display.draw_text(0, 32, ">");
        display.draw_text(10, 32, "Restart");
        if (overlay_sel_ == 1) display.draw_text(0, 40, ">");
        display.draw_text(10, 40, "Exit");
    }

    display.show();
}
