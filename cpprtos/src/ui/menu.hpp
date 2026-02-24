#pragma once
#include <cstddef>
#include <cstdint>

#include "joystick.hpp"
#include "ssd1306_i2c.hpp"

class WifiManager;

struct MenuItem {
    const char* label;
};

class Menu {
public:
    Menu(const MenuItem* items, size_t count, WifiManager& wifi);

    void handle(InputEvent ev);
    void render(Ssd1306I2C& display);

    // Used by UI task to auto-refresh the Wi-Fi screen
    bool wants_periodic_refresh() const;

private:
    enum class Screen : uint8_t { Main, Options, Wifi, About, Stub };
    enum class EditField : uint8_t { None, Ssid, Password };

    void render_main(Ssd1306I2C& display);
    void render_options(Ssd1306I2C& display);
    void render_wifi(Ssd1306I2C& display);
    void render_about(Ssd1306I2C& display);
    void render_stub(Ssd1306I2C& display, const char* title);

    void ensure_wifi_buffers();
    void trim_right_spaces(char* buf);

    void handle_main(InputEvent ev);
    void handle_options(InputEvent ev);
    void handle_wifi(InputEvent ev);

    void edit_step(InputEvent ev, char* buf, size_t maxLen);

    const MenuItem* items_;
    size_t count_;
    size_t selected_ = 0;

    Screen screen_ = Screen::Main;

    // Options screen selection
    size_t opt_selected_ = 0; // 0=Wi-Fi, 1=Back

    // Wi-Fi screen selection/editing
    size_t wifi_selected_ = 0; // 0=SSID, 1=PW, 2=Connect, 3=Back
    EditField editing_ = EditField::None;
    size_t cursor_ = 0;

    // Local editable buffers (not saved)
    bool wifi_buf_inited_ = false;
    char ssid_[33]{};
    char pw_[65]{};

    WifiManager* wifi_;
};