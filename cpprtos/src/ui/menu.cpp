#include "menu.hpp"

#include <cstring>
#include <cstdio>

#include "FreeRTOS.h"
#include "task.h"

#include "wifi_manager.hpp"

#include "gamemodes/game_mode.hpp"
#include "gamemodes/snake/snake_mode.hpp"

static const char* kCharset = " abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-.";
static SnakeMode g_snake;

static int charset_index(char c) {
    for (int i = 0; kCharset[i]; ++i) {
        if (kCharset[i] == c) return i;
    }
    return 0; // space
}

Menu::Menu(const MenuItem* items, size_t count, WifiManager& wifi)
    : items_(items), count_(count), wifi_(&wifi) {}

bool Menu::wants_periodic_refresh() const {
    return screen_ == Screen::Wifi || screen_ == Screen::Game;
}

bool Menu::tick() {
    const TickType_t now = xTaskGetTickCount();

    if (screen_ == Screen::Wifi) {
        return true;
    }

    if (screen_ == Screen::Game && active_game_) {
        const bool redraw = active_game_->tick(now);
        if (active_game_->is_finished()) {
            active_game_->on_exit();
            active_game_ = nullptr;
            screen_ = Screen::StartGame;
            start_selected_ = 0;
            return true;
        }
        return redraw;
    }

    return false;
}

void Menu::handle(InputEvent ev) {
    switch (screen_) {
        case Screen::Main:      handle_main(ev); break;
        case Screen::StartGame: handle_start_game(ev); break;
        case Screen::Options:   handle_options(ev); break;
        case Screen::Wifi:      handle_wifi(ev); break;
        case Screen::Game:      handle_game(ev); break;
        case Screen::About:
            if (ev == InputEvent::Select || ev == InputEvent::Left || ev == InputEvent::Back) {
                screen_ = Screen::Main;
            }
            break;
    }
}

void Menu::handle_main(InputEvent ev) {
    if (ev == InputEvent::Up) {
        selected_ = (selected_ == 0) ? (count_ - 1) : (selected_ - 1);
    } else if (ev == InputEvent::Down) {
        selected_ = (selected_ + 1) % count_;
    } else if (ev == InputEvent::Select) {
        if (selected_ == 0) { screen_ = Screen::StartGame; start_selected_ = 0; } // Start game
        else if (selected_ == 1) { screen_ = Screen::Options; opt_selected_ = 0; }
        else if (selected_ == 2) screen_ = Screen::About;
    }
}

void Menu::handle_start_game(InputEvent ev) {
    if (ev == InputEvent::Up) {
        start_selected_ = (start_selected_ == 0) ? 1 : 0;
    } else if (ev == InputEvent::Down) {
        start_selected_ = (start_selected_ + 1) % 2;
    } else if (ev == InputEvent::Left || ev == InputEvent::Back) {
        screen_ = Screen::Main;
    } else if (ev == InputEvent::Select) {
        if (start_selected_ == 0) { // Snake
            active_game_ = &g_snake;
            active_game_->on_enter();
            screen_ = Screen::Game;
        } else {
            screen_ = Screen::Main;
        }
    }
}

void Menu::handle_game(InputEvent ev) {
    if (!active_game_) {
        screen_ = Screen::StartGame;
        return;
    }

    active_game_->on_input(ev);

    if (active_game_->is_finished()) {
        active_game_->on_exit();
        active_game_ = nullptr;
        screen_ = Screen::StartGame;
        start_selected_ = 0;
    }
}

void Menu::handle_options(InputEvent ev) {
    if (ev == InputEvent::Up) {
        opt_selected_ = (opt_selected_ == 0) ? 1 : 0;
    } else if (ev == InputEvent::Down) {
        opt_selected_ = (opt_selected_ + 1) % 2;
    } else if (ev == InputEvent::Left || ev == InputEvent::Back) {
        screen_ = Screen::Main;
    } else if (ev == InputEvent::Select) {
        if (opt_selected_ == 0) { // Wi-Fi
            screen_ = Screen::Wifi;
            wifi_selected_ = 0;
            editing_ = EditField::None;
            cursor_ = 0;
            ensure_wifi_buffers();
        } else {
            screen_ = Screen::Main;
        }
    }
}

void Menu::handle_wifi(InputEvent ev) {
    ensure_wifi_buffers();

    // If editing SSID/PW, joystick edits characters
    if (editing_ != EditField::None) {
        if (ev == InputEvent::Select || ev == InputEvent::Back) {
            // finish edit
            trim_right_spaces(editing_ == EditField::Ssid ? ssid_ : pw_);
            editing_ = EditField::None;
            cursor_ = 0;
            return;
        }
        if (editing_ == EditField::Ssid) edit_step(ev, ssid_, 32);
        else edit_step(ev, pw_, 64);
        return;
    }

    // Not editing: navigate selection rows
    if (ev == InputEvent::Up) {
        wifi_selected_ = (wifi_selected_ == 0) ? 3 : (wifi_selected_ - 1);
    } else if (ev == InputEvent::Down) {
        wifi_selected_ = (wifi_selected_ + 1) % 4;
    } else if (ev == InputEvent::Left || ev == InputEvent::Back) {
        screen_ = Screen::Options;
    } else if (ev == InputEvent::Select) {
        if (wifi_selected_ == 0) { // SSID
            editing_ = EditField::Ssid;
            cursor_ = 0;
        } else if (wifi_selected_ == 1) { // PW
            editing_ = EditField::Password;
            cursor_ = 0;
        } else if (wifi_selected_ == 2) { // Connect
            wifi_->request_connect(ssid_, pw_);
        } else { // Back
            screen_ = Screen::Options;
        }
    }
}

void Menu::edit_step(InputEvent ev, char* buf, size_t maxLen) {
    if (ev == InputEvent::Left) {
        if (cursor_ > 0) cursor_--;
        return;
    }
    if (ev == InputEvent::Right) {
        if (cursor_ + 1 < maxLen) cursor_++;
        return;
    }
    if (ev != InputEvent::Up && ev != InputEvent::Down) return;

    char& c = buf[cursor_];
    int idx = charset_index(c);
    if (ev == InputEvent::Up) idx = (idx == 0) ? (int)std::strlen(kCharset) - 1 : (idx - 1);
    else idx = (kCharset[idx + 1] ? idx + 1 : 0);
    c = kCharset[idx];
    buf[maxLen] = '\0';
}

void Menu::trim_right_spaces(char* buf) {
    size_t n = std::strlen(buf);
    while (n > 0 && buf[n - 1] == ' ') {
        buf[n - 1] = '\0';
        --n;
    }
}

void Menu::ensure_wifi_buffers() {
    if (wifi_buf_inited_) return;

    // Pull current creds from WifiManager once
    char ss[33]{};
    char pw[65]{};
    wifi_->get_credentials(ss, sizeof(ss), pw, sizeof(pw));

    std::strncpy(ssid_, ss, sizeof(ssid_) - 1);
    ssid_[sizeof(ssid_) - 1] = '\0';

    std::strncpy(pw_, pw, sizeof(pw_) - 1);
    pw_[sizeof(pw_) - 1] = '\0';

    wifi_buf_inited_ = true;
}

void Menu::render(Ssd1306I2C& display) {
    switch (screen_) {
        case Screen::Main:      render_main(display); break;
        case Screen::StartGame: render_start_game(display); break;
        case Screen::Options:   render_options(display); break;
        case Screen::Wifi:      render_wifi(display); break;
        case Screen::About:     render_about(display); break;
        case Screen::Game:      render_game(display); break;
    }
}

void Menu::render_main(Ssd1306I2C& display) {
    display.clear();
    display.draw_text(0, 0, "Laser Labs");

    int y = 16;
    for (size_t i = 0; i < count_; i++) {
        if (i == selected_) display.draw_text(0, y, ">");
        display.draw_text(10, y, items_[i].label);
        y += 8;
    }

    display.draw_text(0, 56, "Use stick + press");
    display.show();
}

void Menu::render_start_game(Ssd1306I2C& display) {
    display.clear();
    display.draw_text(0, 0, "Start game");

    if (start_selected_ == 0) display.draw_text(0, 16, ">");
    display.draw_text(10, 16, "Snake");

    if (start_selected_ == 1) display.draw_text(0, 24, ">");
    display.draw_text(10, 24, "Back");

    display.draw_text(0, 56, "Press to select");
    display.show();
}

void Menu::render_options(Ssd1306I2C& display) {
    display.clear();
    display.draw_text(0, 0, "Options");

    const char* a = "Wi-Fi";
    const char* b = "Back";

    if (opt_selected_ == 0) display.draw_text(0, 16, ">");
    display.draw_text(10, 16, a);

    if (opt_selected_ == 1) display.draw_text(0, 24, ">");
    display.draw_text(10, 24, b);

    display.draw_text(0, 56, "Press to select");
    display.show();
}

static void draw_trunc(Ssd1306I2C& display, int x, int y, const char* s, int maxChars) {
    char tmp[32]{};
    int i = 0;
    for (; s && s[i] && i < maxChars; ++i) tmp[i] = s[i];
    tmp[i] = '\0';
    display.draw_text(x, y, tmp);
}

void Menu::render_wifi(Ssd1306I2C& display) {
    display.clear();
    display.draw_text(0, 0, "Wi-Fi");

    const EventBits_t bits = xEventGroupGetBits(wifi_->events());

    const char* status = "Idle";
    if (bits & WifiManager::WIFI_CONNECTING) status = "Connecting...";
    else if (bits & WifiManager::WIFI_CONNECTED) status = "Connected";
    else if (bits & WifiManager::WIFI_FAILED) status = "Failed";

    display.draw_text(0, 8, "Status:");
    display.draw_text(48, 8, status);

    // SSID row
    if (wifi_selected_ == 0) display.draw_text(0, 16, ">");
    display.draw_text(10, 16, "SSID:");
    draw_trunc(display, 46, 16, ssid_, 13);

    // PW row (masked)
    if (wifi_selected_ == 1) display.draw_text(0, 24, ">");
    display.draw_text(10, 24, "PW:");
    char masked[20]{};
    size_t n = std::strlen(pw_);
    size_t m = (n > 18) ? 18 : n;
    for (size_t i = 0; i < m; ++i) masked[i] = '*';
    masked[m] = '\0';
    display.draw_text(34, 24, masked);

    // Connect / Back
    if (wifi_selected_ == 2) display.draw_text(0, 32, ">");
    display.draw_text(10, 32, "Connect");

    if (wifi_selected_ == 3) display.draw_text(0, 40, ">");
    display.draw_text(10, 40, "Back");

    // Help line
    if (editing_ != EditField::None) {
        display.draw_text(0, 56, (editing_ == EditField::Ssid) ? "Edit SSID: U/D ch" : "Edit PW: U/D ch");
        // Cursor indicator (just show position)
        char buf[22]{};
        std::snprintf(buf, sizeof(buf), "Pos %u  Sel=done", (unsigned)cursor_);
        display.draw_text(0, 48, buf);
    } else {
        display.draw_text(0, 56, "Press to edit/select");
    }

    display.show();
}

void Menu::render_about(Ssd1306I2C& display) {
    display.clear();
    display.draw_text(0, 0, "About");
    display.draw_text(0, 16, "Laser Labs");
    display.draw_text(0, 26, "Pico WH + RTOS");
    display.draw_text(0, 40, "Press to go back");
    display.show();
}

void Menu::render_game(Ssd1306I2C& display) {
    if (active_game_) {
        active_game_->render(display);
    } else {
        display.clear();
        display.draw_text(0, 0, "Game");
        display.draw_text(0, 20, "No active game");
        display.draw_text(0, 36, "Press to go back");
        display.show();
        screen_ = Screen::StartGame;
    }
}
