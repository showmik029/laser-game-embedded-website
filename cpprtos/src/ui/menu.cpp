#include "menu.hpp"

Menu::Menu(const MenuItem* items, size_t count)
    : items_(items), count_(count)
{
}

void Menu::handle(InputEvent ev)
{
    switch (screen_) {
        case Screen::Main:
            if (ev == InputEvent::Up) {
                selected_ = (selected_ == 0) ? (count_ - 1) : (selected_ - 1);
            } else if (ev == InputEvent::Down) {
                selected_ = (selected_ + 1) % count_;
            } else if (ev == InputEvent::Select) {
                // Index-based mapping for now (simple & explicit)
                if (selected_ == 0) screen_ = Screen::Stub;     // "Start game"
                else if (selected_ == 1) screen_ = Screen::Options;
                else if (selected_ == 2) screen_ = Screen::About;
            }
            break;

        case Screen::About:
        case Screen::Options:
        case Screen::Stub:
            // Any of these inputs returns to main
            if (ev == InputEvent::Select || ev == InputEvent::Left || ev == InputEvent::Back) {
                screen_ = Screen::Main;
            }
            break;
    }
}

void Menu::render(Ssd1306I2C& display)
{
    switch (screen_) {
        case Screen::Main:    render_main(display); break;
        case Screen::Options: render_stub(display, "Options"); break;
        case Screen::Stub:    render_stub(display, "Start game"); break;
        case Screen::About:   render_about(display); break;
    }
}

void Menu::render_main(Ssd1306I2C& display)
{
    display.clear();
    display.draw_text(0, 0, "Laser Labs");

    // Menu items start at y=16, 8px per row.
    int y = 16;
    for (size_t i = 0; i < count_; i++) {
        if (i == selected_) display.draw_text(0, y, ">");
        display.draw_text(10, y, items_[i].label);
        y += 8;
    }

    display.draw_text(0, 56, "Use stick + press");
    display.show();
}

void Menu::render_about(Ssd1306I2C& display)
{
    display.clear();
    display.draw_text(0, 0, "About");
    display.draw_text(0, 16, "Laser Labs");
    display.draw_text(0, 26, "Pico WH + RTOS");
    display.draw_text(0, 40, "Press to go back");
    display.show();
}

void Menu::render_stub(Ssd1306I2C& display, const char* title)
{
    display.clear();
    display.draw_text(0, 0, title);
    display.draw_text(0, 20, "Not implemented");
    display.draw_text(0, 36, "Press to go back");
    display.show();
}
