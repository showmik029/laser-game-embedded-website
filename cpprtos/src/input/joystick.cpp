#include "joystick.hpp"

#include <cstdlib>
#include <cstdio>

#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"

enum class JoyDir : uint8_t { Neutral, Up, Down, Left, Right };

static inline uint16_t read_adc_u16(uint ch)
{
    adc_select_input(ch);
    // adc_read() returns 0..4095 (12-bit).
    return static_cast<uint16_t>(adc_read() << 4);
}

static JoyDir dir_from_xy(uint16_t x, uint16_t y,
                          uint16_t cx, uint16_t cy,
                          const JoystickTuning& t)
{
    int dx = static_cast<int>(x) - static_cast<int>(cx);
    int dy = static_cast<int>(y) - static_cast<int>(cy);
    if (t.invert_y) dy = -dy;

    if (std::abs(dx) < t.deadzone) dx = 0;
    if (std::abs(dy) < t.deadzone) dy = 0;

    if (dx == 0 && dy == 0) return JoyDir::Neutral;

    // Choose the dominant axis (prevents diagonals)
    if (std::abs(dx) >= std::abs(dy)) {
        if (dx > static_cast<int>(t.threshold))  return JoyDir::Right;
        if (dx < -static_cast<int>(t.threshold)) return JoyDir::Left;
        return JoyDir::Neutral;
    } else {
        if (dy > static_cast<int>(t.threshold))  return JoyDir::Up;
        if (dy < -static_cast<int>(t.threshold)) return JoyDir::Down;
        return JoyDir::Neutral;
    }
}

static bool enqueue_event(QueueHandle_t q, InputEvent ev)
{
    return xQueueSend(q, &ev, 0) == pdTRUE;
}

void joystick_task(void* param)
{
    auto* p = static_cast<JoystickTaskParams*>(param);

    // ADC init
    adc_init();
    adc_gpio_init(p->pins.adc_x_gpio);
    adc_gpio_init(p->pins.adc_y_gpio);

    // Button init (active-low)
    gpio_init(p->pins.btn_gpio);
    gpio_set_dir(p->pins.btn_gpio, GPIO_IN);
    gpio_pull_up(p->pins.btn_gpio);

    // Calibrate center at boot
    uint32_t sx = 0, sy = 0;
    constexpr int kSamples = 32;
    for (int i = 0; i < kSamples; i++) {
        sx += read_adc_u16(p->pins.adc_x_ch);
        sy += read_adc_u16(p->pins.adc_y_ch);
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    const uint16_t cx = static_cast<uint16_t>(sx / kSamples);
    const uint16_t cy = static_cast<uint16_t>(sy / kSamples);

    printf("Joystick center: x=%u y=%u\n", cx, cy);

    JoyDir last_dir = JoyDir::Neutral;
    TickType_t repeat_at = 0;

    bool last_pressed = false;
    TickType_t last_btn_change = 0;

    while (true) {
        const uint16_t x = read_adc_u16(p->pins.adc_x_ch);
        const uint16_t y = read_adc_u16(p->pins.adc_y_ch);
        const bool pressed = (gpio_get(p->pins.btn_gpio) == 0); // active-low

        const TickType_t now = xTaskGetTickCount();

        // Direction -> events
        const JoyDir dir = dir_from_xy(x, y, cx, cy, p->tuning);

        if (dir != last_dir) {
            last_dir = dir;
            if (dir != JoyDir::Neutral) {
                InputEvent ev{};
                switch (dir) {
                    case JoyDir::Up:    ev = InputEvent::Up; break;
                    case JoyDir::Down:  ev = InputEvent::Down; break;
                    case JoyDir::Left:  ev = InputEvent::Left; break;
                    case JoyDir::Right: ev = InputEvent::Right; break;
                    default: break;
                }
                enqueue_event(p->queue, ev);
                repeat_at = now + p->tuning.repeat_initial;
            }
        } else {
            // hold-to-repeat
            if (dir != JoyDir::Neutral && now >= repeat_at) {
                InputEvent ev{};
                switch (dir) {
                    case JoyDir::Up:    ev = InputEvent::Up; break;
                    case JoyDir::Down:  ev = InputEvent::Down; break;
                    case JoyDir::Left:  ev = InputEvent::Left; break;
                    case JoyDir::Right: ev = InputEvent::Right; break;
                    default: break;
                }
                enqueue_event(p->queue, ev);
                repeat_at = now + p->tuning.repeat_period;
            }
        }

        // Button debounce + edge detect (press triggers Select)
        if (pressed != last_pressed) {
            const TickType_t elapsed = now - last_btn_change;
            if (elapsed >= pdMS_TO_TICKS(p->tuning.debounce_ms)) {
                last_pressed = pressed;
                last_btn_change = now;
                if (pressed) {
                    enqueue_event(p->queue, InputEvent::Select);
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20)); // ~50 Hz sampling
    }
}
