#include "menu.hpp"

#include <cstring>
#include <cstdio>

#include "FreeRTOS.h"
#include "task.h"

#include "wifi_manager.hpp"
#include "mqtt_manager.hpp"

#include "gamemodes/game_mode.hpp"
#include "gamemodes/snake/snake_mode.hpp"
#include "gamemodes/mqtt_game1/mqtt_game1_mode.hpp"

static const char* kCharset = " abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-.";
static SnakeMode g_snake;
static MqttGame1Mode g_game1;

static int charset_index(char c) {
    for (int i = 0; kCharset[i]; ++i) {
        if (kCharset[i] == c) return i;
    }
    return 0;
}

static void draw_trunc(Ssd1306I2C& display, int x, int y, const char* s, int maxChars) {
    char tmp[32]{};
    int i = 0;
    for (; s && s[i] && i < maxChars; ++i) tmp[i] = s[i];
    tmp[i] = '\0';
    display.draw_text(x, y, tmp);
}

Menu::Menu(const MenuItem* items, size_t count, WifiManager& wifi, MqttManager& mqtt)
    : items_(items), count_(count), wifi_(&wifi), mqtt_(&mqtt) {
    g_game1.bind(wifi_, mqtt_);
    ensure_player_name();
    g_game1.set_player_name(player_name_);
}

bool Menu::wants_periodic_refresh() const {
    return screen_ == Screen::Wifi ||
           screen_ == Screen::WifiEdit ||
           screen_ == Screen::PlayerName ||
           screen_ == Screen::PlayerNameEdit ||
           screen_ == Screen::Game;
}

bool Menu::tick() {
    const TickType_t now = xTaskGetTickCount();

    if (screen_ == Screen::Wifi ||
        screen_ == Screen::WifiEdit ||
        screen_ == Screen::PlayerName ||
        screen_ == Screen::PlayerNameEdit) {
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
        case Screen::Main:           handle_main(ev); break;
        case Screen::StartGame:      handle_start_game(ev); break;
        case Screen::Options:        handle_options(ev); break;
        case Screen::Wifi:           handle_wifi(ev); break;
        case Screen::WifiEdit:       handle_wifi_edit(ev); break;
        case Screen::PlayerName:     handle_player_name(ev); break;
        case Screen::PlayerNameEdit: handle_player_name_edit(ev); break;
        case Screen::Game:           handle_game(ev); break;
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
        if (selected_ == 0) {
            screen_ = Screen::StartGame;
            start_selected_ = 0;
        } else if (selected_ == 1) {
            screen_ = Screen::Options;
            opt_selected_ = 0;
        } else if (selected_ == 2) {
            screen_ = Screen::About;
        }
    }
}

void Menu::handle_start_game(InputEvent ev) {
    constexpr size_t kCount = 3;

    if (ev == InputEvent::Up) {
        start_selected_ = (start_selected_ == 0) ? (kCount - 1) : (start_selected_ - 1);
    } else if (ev == InputEvent::Down) {
        start_selected_ = (start_selected_ + 1) % kCount;
    } else if (ev == InputEvent::Left || ev == InputEvent::Back) {
        screen_ = Screen::Main;
    } else if (ev == InputEvent::Select) {
        if (start_selected_ == 0) {
            active_game_ = &g_snake;
            active_game_->on_enter();
            screen_ = Screen::Game;
        } else if (start_selected_ == 1) {
            ensure_player_name();
            g_game1.set_player_name(player_name_);
            active_game_ = &g_game1;
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
    constexpr size_t kCount = 3; // Wi-Fi, Player name, Back

    if (ev == InputEvent::Up) {
        opt_selected_ = (opt_selected_ == 0) ? (kCount - 1) : (opt_selected_ - 1);
    } else if (ev == InputEvent::Down) {
        opt_selected_ = (opt_selected_ + 1) % kCount;
    } else if (ev == InputEvent::Left || ev == InputEvent::Back) {
        screen_ = Screen::Main;
    } else if (ev == InputEvent::Select) {
        if (opt_selected_ == 0) {
            screen_ = Screen::Wifi;
            wifi_selected_ = 0;
            editing_ = EditField::None;
            ensure_wifi_buffers();
        } else if (opt_selected_ == 1) {
            ensure_player_name();
            screen_ = Screen::PlayerName;
        } else {
            screen_ = Screen::Main;
        }
    }
}

void Menu::handle_wifi(InputEvent ev) {
    ensure_wifi_buffers();

    if (ev == InputEvent::Up) {
        wifi_selected_ = (wifi_selected_ == 0) ? 3 : (wifi_selected_ - 1);
    } else if (ev == InputEvent::Down) {
        wifi_selected_ = (wifi_selected_ + 1) % 4;
    } else if (ev == InputEvent::Left || ev == InputEvent::Back) {
        screen_ = Screen::Options;
    } else if (ev == InputEvent::Select) {
        if (wifi_selected_ == 0) {
            wifi_editor_enter(EditField::Ssid);
        } else if (wifi_selected_ == 1) {
            wifi_editor_enter(EditField::Password);
        } else if (wifi_selected_ == 2) {
            trim_right_spaces(ssid_);
            trim_right_spaces(pw_);
            wifi_->request_connect(ssid_, pw_);
        } else {
            screen_ = Screen::Options;
        }
    }
}

void Menu::handle_wifi_edit(InputEvent ev) {
    if (ev == InputEvent::Left || ev == InputEvent::Back) {
        trim_right_spaces(editing_ == EditField::Ssid ? ssid_ : pw_);
        editing_ = EditField::None;
        screen_ = Screen::Wifi;
        return;
    }

    if (ev == InputEvent::Up) {
        wifi_editor_step(-1);
        return;
    }
    if (ev == InputEvent::Down) {
        wifi_editor_step(+1);
        return;
    }
    if (ev == InputEvent::Select) {
        wifi_editor_append();
        return;
    }
    if (ev == InputEvent::Right) {
        wifi_editor_backspace();
        return;
    }
}

void Menu::handle_player_name(InputEvent ev) {
    ensure_player_name();

    if (ev == InputEvent::Left || ev == InputEvent::Back) {
        screen_ = Screen::Options;
    } else if (ev == InputEvent::Select) {
        player_name_editor_enter();
    }
}

void Menu::handle_player_name_edit(InputEvent ev) {
    if (ev == InputEvent::Left || ev == InputEvent::Back) {
        trim_right_spaces(player_name_);
        if (player_name_[0] == '\0') {
            std::strncpy(player_name_, "default", sizeof(player_name_) - 1);
            player_name_[sizeof(player_name_) - 1] = '\0';
        }
        g_game1.set_player_name(player_name_);
        screen_ = Screen::PlayerName;
        return;
    }

    if (ev == InputEvent::Up) {
        player_name_editor_step(-1);
        return;
    }
    if (ev == InputEvent::Down) {
        player_name_editor_step(+1);
        return;
    }
    if (ev == InputEvent::Select) {
        player_name_editor_append();
        return;
    }
    if (ev == InputEvent::Right) {
        player_name_editor_backspace();
        return;
    }
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

    char ss[33]{};
    char pw[65]{};
    wifi_->get_credentials(ss, sizeof(ss), pw, sizeof(pw));

    std::strncpy(ssid_, ss, sizeof(ssid_) - 1);
    ssid_[sizeof(ssid_) - 1] = '\0';

    std::strncpy(pw_, pw, sizeof(pw_) - 1);
    pw_[sizeof(pw_) - 1] = '\0';

    wifi_buf_inited_ = true;
}

void Menu::wifi_editor_enter(EditField field) {
    ensure_wifi_buffers();
    editing_ = field;
    picker_idx_ = charset_index('a');
    screen_ = Screen::WifiEdit;
}

char Menu::wifi_editor_current_char() const {
    const int n = (int)std::strlen(kCharset);
    if (n <= 0) return ' ';
    const int idx = (int)(picker_idx_ % (size_t)n);
    return kCharset[idx];
}

void Menu::wifi_editor_step(int delta) {
    const int n = (int)std::strlen(kCharset);
    if (n <= 0) return;
    int idx = (int)picker_idx_;
    idx = (idx + delta) % n;
    if (idx < 0) idx += n;
    picker_idx_ = (size_t)idx;
}

void Menu::wifi_editor_append() {
    if (editing_ == EditField::None) return;

    char* buf = (editing_ == EditField::Ssid) ? ssid_ : pw_;
    const size_t maxLen = (editing_ == EditField::Ssid) ? 32 : 64;

    size_t len = std::strlen(buf);
    if (len >= maxLen) return;

    buf[len] = wifi_editor_current_char();
    buf[len + 1] = '\0';
}

void Menu::wifi_editor_backspace() {
    if (editing_ == EditField::None) return;

    char* buf = (editing_ == EditField::Ssid) ? ssid_ : pw_;
    size_t len = std::strlen(buf);
    if (len == 0) return;

    buf[len - 1] = '\0';
}

void Menu::ensure_player_name() {
    if (player_name_inited_) return;

    std::strncpy(player_name_, "default", sizeof(player_name_) - 1);
    player_name_[sizeof(player_name_) - 1] = '\0';
    player_picker_idx_ = (size_t)charset_index('a');
    player_name_inited_ = true;
}

void Menu::player_name_editor_enter() {
    ensure_player_name();
    player_picker_idx_ = (size_t)charset_index('a');
    screen_ = Screen::PlayerNameEdit;
}

char Menu::player_name_editor_current_char() const {
    const int n = (int)std::strlen(kCharset);
    if (n <= 0) return ' ';
    const int idx = (int)(player_picker_idx_ % (size_t)n);
    return kCharset[idx];
}

void Menu::player_name_editor_step(int delta) {
    const int n = (int)std::strlen(kCharset);
    if (n <= 0) return;
    int idx = (int)player_picker_idx_;
    idx = (idx + delta) % n;
    if (idx < 0) idx += n;
    player_picker_idx_ = (size_t)idx;
}

void Menu::player_name_editor_append() {
    ensure_player_name();

    constexpr size_t kMaxLen = 32;
    size_t len = std::strlen(player_name_);
    if (len >= kMaxLen) return;

    player_name_[len] = player_name_editor_current_char();
    player_name_[len + 1] = '\0';
}

void Menu::player_name_editor_backspace() {
    ensure_player_name();

    size_t len = std::strlen(player_name_);
    if (len == 0) return;

    player_name_[len - 1] = '\0';
}

void Menu::render(Ssd1306I2C& display) {
    switch (screen_) {
        case Screen::Main:           render_main(display); break;
        case Screen::StartGame:      render_start_game(display); break;
        case Screen::Options:        render_options(display); break;
        case Screen::Wifi:           render_wifi(display); break;
        case Screen::WifiEdit:       render_wifi_edit(display); break;
        case Screen::PlayerName:     render_player_name(display); break;
        case Screen::PlayerNameEdit: render_player_name_edit(display); break;
        case Screen::About:          render_about(display); break;
        case Screen::Game:           render_game(display); break;
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
    display.draw_text(10, 24, "Game1");

    if (start_selected_ == 2) display.draw_text(0, 32, ">");
    display.draw_text(10, 32, "Back");

    display.draw_text(0, 56, "Press to select");
    display.show();
}

void Menu::render_options(Ssd1306I2C& display) {
    display.clear();
    display.draw_text(0, 0, "Options");

    if (opt_selected_ == 0) display.draw_text(0, 16, ">");
    display.draw_text(10, 16, "Wi-Fi");

    if (opt_selected_ == 1) display.draw_text(0, 24, ">");
    display.draw_text(10, 24, "Player name");

    if (opt_selected_ == 2) display.draw_text(0, 32, ">");
    display.draw_text(10, 32, "Back");

    display.draw_text(0, 56, "Press to select");
    display.show();
}

void Menu::render_wifi(Ssd1306I2C& display) {
    display.clear();
    display.draw_text(0, 0, "Wi-Fi");

    const EventBits_t bits = xEventGroupGetBits(wifi_->events());

    const char* status = "Idle";
    if (bits & WifiManager::WIFI_CONNECTING) status = "Connecting.";
    else if (bits & WifiManager::WIFI_CONNECTED) status = "Connected";
    else if (bits & WifiManager::WIFI_FAILED) status = "Failed";

    display.draw_text(0, 8, "Status:");
    display.draw_text(48, 8, status);

    if (wifi_selected_ == 0) display.draw_text(0, 16, ">");
    display.draw_text(10, 16, "SSID:");
    draw_trunc(display, 46, 16, ssid_, 13);

    if (wifi_selected_ == 1) display.draw_text(0, 24, ">");
    display.draw_text(10, 24, "PW:");
    char masked[20]{};
    size_t n = std::strlen(pw_);
    size_t m = (n > 18) ? 18 : n;
    for (size_t i = 0; i < m; ++i) masked[i] = '*';
    masked[m] = '\0';
    display.draw_text(34, 24, masked);

    if (wifi_selected_ == 2) display.draw_text(0, 32, ">");
    display.draw_text(10, 32, "Connect");

    if (wifi_selected_ == 3) display.draw_text(0, 40, ">");
    display.draw_text(10, 40, "Back");

    display.draw_text(0, 56, "Press=edit  Left=back");
    display.show();
}

void Menu::render_wifi_edit(Ssd1306I2C& display) {
    display.clear();

    const bool is_ssid = (editing_ == EditField::Ssid);
    const char* title = is_ssid ? "Edit SSID" : "Edit PW";
    display.draw_text(0, 0, title);

    const char* value = is_ssid ? ssid_ : pw_;
    size_t len = std::strlen(value);

    display.draw_text(0, 8, "Value:");

    constexpr int kLineChars = 21;
    constexpr int kShowChars = kLineChars * 2;

    const char* start = value;
    if (len > (size_t)kShowChars) start = value + (len - (size_t)kShowChars);

    char line1[kLineChars + 1]{};
    int i = 0;
    while (start[i] && i < kLineChars) { line1[i] = start[i]; ++i; }
    line1[i] = '\0';
    display.draw_text(0, 16, (len == 0) ? "<empty>" : line1);

    if (len > (size_t)kLineChars) {
        char line2[kLineChars + 1]{};
        int j = 0;
        const char* s2 = start + kLineChars;
        while (s2[j] && j < kLineChars) { line2[j] = s2[j]; ++j; }
        line2[j] = '\0';
        display.draw_text(0, 24, line2);
    }

    display.draw_text(0, 36, "Pick:");
    char pickbuf[16]{};
    const char c = wifi_editor_current_char();
    if (c == ' ') {
        std::snprintf(pickbuf, sizeof(pickbuf), "<space>");
    } else {
        std::snprintf(pickbuf, sizeof(pickbuf), "[%c]", c);
    }
    display.draw_text(40, 36, pickbuf);

    display.draw_text(0, 48, "U/D pick  P add");
    display.draw_text(0, 56, "R del    L done");
    display.show();
}

void Menu::render_player_name(Ssd1306I2C& display) {
    ensure_player_name();

    display.clear();
    display.draw_text(0, 0, "Player name");
    display.draw_text(0, 12, "Current:");
    draw_trunc(display, 0, 22, player_name_, 21);
    display.draw_text(0, 48, "Press=edit");
    display.draw_text(0, 56, "Left=back");
    display.show();
}

void Menu::render_player_name_edit(Ssd1306I2C& display) {
    ensure_player_name();

    display.clear();
    display.draw_text(0, 0, "Edit player");

    const char* value = player_name_;
    size_t len = std::strlen(value);

    display.draw_text(0, 8, "Value:");

    constexpr int kLineChars = 21;
    constexpr int kShowChars = kLineChars * 2;

    const char* start = value;
    if (len > (size_t)kShowChars) start = value + (len - (size_t)kShowChars);

    char line1[kLineChars + 1]{};
    int i = 0;
    while (start[i] && i < kLineChars) { line1[i] = start[i]; ++i; }
    line1[i] = '\0';
    display.draw_text(0, 16, (len == 0) ? "<empty>" : line1);

    if (len > (size_t)kLineChars) {
        char line2[kLineChars + 1]{};
        int j = 0;
        const char* s2 = start + kLineChars;
        while (s2[j] && j < kLineChars) { line2[j] = s2[j]; ++j; }
        line2[j] = '\0';
        display.draw_text(0, 24, line2);
    }

    display.draw_text(0, 36, "Pick:");
    char pickbuf[16]{};
    const char c = player_name_editor_current_char();
    if (c == ' ') {
        std::snprintf(pickbuf, sizeof(pickbuf), "<space>");
    } else {
        std::snprintf(pickbuf, sizeof(pickbuf), "[%c]", c);
    }
    display.draw_text(40, 36, pickbuf);

    display.draw_text(0, 48, "U/D pick  P add");
    display.draw_text(0, 56, "R del    L done");
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