#include <cstdio>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "pico/stdlib.h"
#include "hardware/timer.h"

#include "ssd1306_i2c.hpp"
#include "joystick.hpp"
#include "menu.hpp"

extern "C" {
uint32_t read_runtime_ctr(void) {
    return timer_hw->timerawl;
}
}

// Queue for input events (joystick directions + button)
static QueueHandle_t g_input_queue = nullptr;

static void ui_task(void*)
{
    // OLED: I2C1 on GP14(SDA) + GP15(SCL), address 0x3C, 400kHz
    Ssd1306I2C display(i2c1, 0x3C, /*sda=*/14, /*scl=*/15, /*baud=*/400000);
    display.init();
    display.clear();
    display.draw_text(0, 0, "Laser Labs");
    display.draw_text(0, 10, "Booting...");
    display.show();

    static constexpr MenuItem kMainItems[] = {
        { "Start game" },
        { "Options"   },
        { "About"     },
    };

    Menu menu(kMainItems, sizeof(kMainItems) / sizeof(kMainItems[0]));
    menu.render(display);

    InputEvent ev{};
    while (true) {
        if (xQueueReceive(g_input_queue, &ev, portMAX_DELAY) == pdTRUE) {
            menu.handle(ev);
            menu.render(display);
        }
    }
}

int main()
{
    stdio_init_all();

    // Small delay so UART comes up cleanly.
    sleep_ms(50);
    printf("Boot\n");

    g_input_queue = xQueueCreate(/*queue_length=*/8, sizeof(InputEvent));
    configASSERT(g_input_queue);

    // Joystick task reads ADC + button and posts InputEvent into g_input_queue.
    static JoystickTaskParams joy_params = {
        .queue = g_input_queue,
        .pins = {
            .adc_x_gpio = 26, // GP26 / ADC0
            .adc_y_gpio = 27, // GP27 / ADC1
            .btn_gpio   = 22, // GP22 (active-low, pull-up)
        },
        .tuning = {
            .deadzone      = 2500,                 // +/- around center
            .threshold     = 7000,                 // direction trigger
            .invert_y      = true,                 // flip if your "up" feels inverted
            .repeat_initial = pdMS_TO_TICKS(450),  // hold-to-repeat
            .repeat_period  = pdMS_TO_TICKS(150),
            .debounce_ms    = 30,
        }
    };

    xTaskCreate(joystick_task, "joystick", 256, &joy_params, tskIDLE_PRIORITY + 2, nullptr);
    xTaskCreate(ui_task,       "ui",       512, nullptr,     tskIDLE_PRIORITY + 1, nullptr);

    vTaskStartScheduler();
    while (true) { /* should never get here */ }
}
