#pragma once
#include <cstddef>
#include <cstdint>

#include "joystick.hpp"
#include "ssd1306_i2c.hpp"

class WifiManager;
class GameMode;

struct MenuItem {
    const char* label;
};

class Menu {
public:
    Menu(const MenuItem* items, size_t count, WifiManager& wifi);

    void handle(InputEvent ev);
    void render(Ssd1306I2C& display);

    // Returns true if the display should be redrawn.
    bool tick();

    // Used by UI task to auto-refresh certain screens
    bool wants_periodic_refresh() const;

private:
    enum class Screen : uint8_t { Main, StartGame, Options, Wifi, WifiEdit, About, Game };
    enum class EditField : uint8_t { None, Ssid, Password };

    void render_main(Ssd1306I2C& display);
    void render_start_game(Ssd1306I2C& display);
    void render_options(Ssd1306I2C& display);
    void render_wifi(Ssd1306I2C& display);
    void render_wifi_edit(Ssd1306I2C& display);
    void render_about(Ssd1306I2C& display);
    void render_game(Ssd1306I2C& display);

    void ensure_wifi_buffers();
    void trim_right_spaces(char* buf);

    // Wi-Fi editor helpers
    void wifi_editor_enter(EditField field);
    char wifi_editor_current_char() const;
    void wifi_editor_step(int delta);
    void wifi_editor_append();
    void wifi_editor_backspace();

    void handle_main(InputEvent ev);
    void handle_start_game(InputEvent ev);
    void handle_options(InputEvent ev);
    void handle_wifi(InputEvent ev);
    void handle_wifi_edit(InputEvent ev);
    void handle_game(InputEvent ev);

    const MenuItem* items_;
    size_t count_;
    size_t selected_ = 0;

    Screen screen_ = Screen::Main;

    // Start Game screen selection
    size_t start_selected_ = 0; // 0=Snake, 1=Back

    // Options screen selection
    size_t opt_selected_ = 0; // 0=Wi-Fi, 1=Back

    // Wi-Fi screen selection/editing
    size_t wifi_selected_ = 0; // 0=SSID, 1=PW, 2=Connect, 3=Back
    EditField editing_ = EditField::None;
    size_t picker_idx_ = 1; // index into charset for editor

    // Local editable buffers (not saved)
    bool wifi_buf_inited_ = false;
    char ssid_[33]{};
    char pw_[65]{};

    // Active game (owned elsewhere)
    GameMode* active_game_ = nullptr;

    WifiManager* wifi_;
};
