#pragma once
#include "FreeRTOS.h"
#include "joystick.hpp"
#include "ssd1306_i2c.hpp"

// Minimal interface so different games can plug into the UI.
class GameMode {
public:
    virtual ~GameMode() = default;

    // Called when the game is launched.
    virtual void on_enter() = 0;

    // Called when leaving the game (back to menus).
    virtual void on_exit() = 0;

    // Called on joystick events.
    virtual void on_input(InputEvent ev) = 0;

    // Called periodically (e.g. on UI task timeout). Return true if display should be redrawn.
    virtual bool tick(TickType_t now) = 0;

    // Draw the full frame.
    virtual void render(Ssd1306I2C& display) = 0;

    // If true, UI should exit the game back to Start Game menu.
    virtual bool is_finished() const = 0;
};
