#pragma once
#include <cstddef>
#include <cstdint>

#include "joystick.hpp"
#include "ssd1306_i2c.hpp"

class WifiManager;
class MqttManager;
class GameMode;
class Laser;

struct MenuItem {
    const char* label;
};

class Menu {
public:
    Menu(const MenuItem* items, size_t count, WifiManager& wifi, MqttManager& mqtt, Laser& laser);

    void handle(InputEvent ev);
    void render(Ssd1306I2C& display);

    bool tick();
    bool wants_periodic_refresh() const;

private:
    enum class Screen : uint8_t {
        Main,
        StartGame,
        Options,
        Wifi,
        WifiEdit,
        PlayerName,
        PlayerNameEdit,
        About,
        Game
    };

    enum class EditField : uint8_t { None, Ssid, Password };

    void render_main(Ssd1306I2C& display);
    void render_start_game(Ssd1306I2C& display);
    void render_options(Ssd1306I2C& display);
    void render_wifi(Ssd1306I2C& display);
    void render_wifi_edit(Ssd1306I2C& display);
    void render_player_name(Ssd1306I2C& display);
    void render_player_name_edit(Ssd1306I2C& display);
    void render_about(Ssd1306I2C& display);
    void render_game(Ssd1306I2C& display);

    void ensure_wifi_buffers();
    void trim_right_spaces(char* buf);

    void wifi_editor_enter(EditField field);
    char wifi_editor_current_char() const;
    void wifi_editor_step(int delta);
    void wifi_editor_append();
    void wifi_editor_backspace();

    void ensure_player_name();
    void player_name_editor_enter();
    char player_name_editor_current_char() const;
    void player_name_editor_step(int delta);
    void player_name_editor_append();
    void player_name_editor_backspace();

    void handle_main(InputEvent ev);
    void handle_start_game(InputEvent ev);
    void handle_options(InputEvent ev);
    void handle_wifi(InputEvent ev);
    void handle_wifi_edit(InputEvent ev);
    void handle_player_name(InputEvent ev);
    void handle_player_name_edit(InputEvent ev);
    void handle_game(InputEvent ev);

    const MenuItem* items_;
    size_t count_;
    size_t selected_ = 0;

    Screen screen_ = Screen::Main;

    // 0=Snake, 1=Classic, 2=Reverse, 3=Speedup, 4=Back
    size_t start_selected_ = 0;

    size_t opt_selected_ = 0;

    size_t wifi_selected_ = 0;
    EditField editing_ = EditField::None;
    size_t picker_idx_ = 1;

    bool wifi_buf_inited_ = false;
    char ssid_[33]{};
    char pw_[65]{};

    bool player_name_inited_ = false;
    size_t player_picker_idx_ = 1;
    char player_name_[33]{};

    GameMode* active_game_ = nullptr;

    WifiManager* wifi_;
    MqttManager* mqtt_;
};