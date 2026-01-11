#pragma once
#include <cstddef>
#include <cstdint>

#include "joystick.hpp"
#include "ssd1306_i2c.hpp"

struct MenuItem {
    const char* label;
};

class Menu {
public:
    Menu(const MenuItem* items, size_t count);

    void handle(InputEvent ev);
    void render(Ssd1306I2C& display);

private:
    enum class Screen : uint8_t { Main, Options, About, Stub };

    void render_main(Ssd1306I2C& display);
    void render_about(Ssd1306I2C& display);
    void render_stub(Ssd1306I2C& display, const char* title);

    const MenuItem* items_;
    size_t count_;
    size_t selected_ = 0;
    Screen screen_ = Screen::Main;
};
