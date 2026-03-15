#pragma once
#include <cstdint>
#include <cstddef>

#include "hardware/i2c.h"

// Minimal SSD1306 driver for 128x64 over I2C
class Ssd1306I2C {
public:
    Ssd1306I2C(i2c_inst_t* i2c, uint8_t addr, uint sda_gpio, uint scl_gpio, uint32_t baud_hz);

    void init();
    void clear();                  // clear framebuffer
    void fill(bool on);            // fill framebuffer
    void show();                   // push framebuffer to display

    void set_pixel(int x, int y, bool on = true);
    void draw_char(int x, int y, char c);
    void draw_text(int x, int y, const char* s);

    int width()  const { return 128; }
    int height() const { return 64; }

private:
    void write_cmd(uint8_t cmd);
    void write_cmd_list(const uint8_t* cmds, size_t n);
    void write_data(const uint8_t* data, size_t n);

    i2c_inst_t* i2c_;
    uint8_t addr_;
    uint sda_;
    uint scl_;
    uint32_t baud_;

    static constexpr int kW = 128;
    static constexpr int kH = 64;
    static constexpr int kBufSize = kW * kH / 8;
    uint8_t buf_[kBufSize];
};
