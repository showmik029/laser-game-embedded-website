#include "ssd1306_i2c.hpp"
#include "font5x7.hpp"

#include <cstring>

#include "pico/stdlib.h"

Ssd1306I2C::Ssd1306I2C(i2c_inst_t* i2c, uint8_t addr, uint sda_gpio, uint scl_gpio, uint32_t baud_hz)
    : i2c_(i2c), addr_(addr), sda_(sda_gpio), scl_(scl_gpio), baud_(baud_hz)
{
    std::memset(buf_, 0, sizeof(buf_));
}

void Ssd1306I2C::write_cmd(uint8_t cmd)
{
    uint8_t packet[2] = { 0x00, cmd }; // 0x00 = "command" control byte
    i2c_write_blocking(i2c_, addr_, packet, 2, false);
}

void Ssd1306I2C::write_cmd_list(const uint8_t* cmds, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        write_cmd(cmds[i]);
    }
}

void Ssd1306I2C::write_data(const uint8_t* data, size_t n)
{
    // Send in chunks: [0x40 | data...]
    uint8_t packet[1 + 16];
    packet[0] = 0x40; // 0x40 = "data" control byte

    size_t sent = 0;
    while (sent < n) {
        const size_t chunk = (n - sent > 16) ? 16 : (n - sent);
        for (size_t i = 0; i < chunk; i++) {
            packet[1 + i] = data[sent + i];
        }
        i2c_write_blocking(i2c_, addr_, packet, 1 + chunk, false);
        sent += chunk;
    }
}

void Ssd1306I2C::init()
{
    // I2C pins
    i2c_init(i2c_, baud_);
    gpio_set_function(sda_, GPIO_FUNC_I2C);
    gpio_set_function(scl_, GPIO_FUNC_I2C);
    gpio_pull_up(sda_);
    gpio_pull_up(scl_);

    sleep_ms(50);

    // Init sequence for 128x64 (SSD1306)
    const uint8_t init1[] = {
        0xAE,       // Display OFF
        0xD5, 0x80, // Set display clock div
        0xA8, 0x3F, // Set multiplex (1/64)
        0xD3, 0x00, // Set display offset
        0x40,       // Set start line to 0
        0x8D, 0x14, // Charge pump ON
        0x20, 0x00, // Memory mode: horizontal addressing
        0xA1,       // Segment remap
        0xC8,       // COM scan dec
        0xDA, 0x12, // COM pins
        0x81, 0xCF, // Contrast
        0xD9, 0xF1, // Precharge
        0xDB, 0x40, // VCOM detect
        0xA4,       // Display all ON resume
        0xA6,       // Normal display
        0xAF        // Display ON
    };
    write_cmd_list(init1, sizeof(init1));

    clear();
    show();
}

void Ssd1306I2C::clear()
{
    std::memset(buf_, 0, sizeof(buf_));
}

void Ssd1306I2C::fill(bool on)
{
    std::memset(buf_, on ? 0xFF : 0x00, sizeof(buf_));
}

void Ssd1306I2C::set_pixel(int x, int y, bool on)
{
    if (x < 0 || x >= kW || y < 0 || y >= kH) return;
    const int idx = x + (y / 8) * kW;
    const uint8_t mask = (1u << (y & 7));
    if (on) buf_[idx] |= mask;
    else    buf_[idx] &= ~mask;
}

void Ssd1306I2C::draw_char(int x, int y, char c)
{
    if (c < 0x20 || c > 0x7F) c = '?';
    const uint8_t* glyph = kFont5x7[c - 0x20];

    // 5 columns, 7 pixels tall. We draw into the framebuffer.
    for (int col = 0; col < 5; col++) {
        const uint8_t bits = glyph[col];
        for (int row = 0; row < 7; row++) {
            const bool on = (bits >> row) & 0x01;
            set_pixel(x + col, y + row, on);
        }
    }
    // 1-column spacing
    for (int row = 0; row < 7; row++) {
        set_pixel(x + 5, y + row, false);
    }
}

void Ssd1306I2C::draw_text(int x, int y, const char* s)
{
    int cx = x;
    while (*s) {
        if (*s == '\n') {
            cx = x;
            y += 8;
            s++;
            continue;
        }
        draw_char(cx, y, *s++);
        cx += 6;
        if (cx > (kW - 6)) { // wrap
            cx = x;
            y += 8;
        }
        if (y > (kH - 8)) break;
    }
}

void Ssd1306I2C::show()
{
    // Set column + page address
    write_cmd(0x21); // set column address
    write_cmd(0);    // start
    write_cmd(127);  // end

    write_cmd(0x22); // set page address
    write_cmd(0);    // start page
    write_cmd(7);    // end page

    write_data(buf_, sizeof(buf_));
}
