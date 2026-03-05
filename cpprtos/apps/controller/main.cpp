#include <cstdio>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "pico/stdlib.h"
#include "hardware/timer.h"

#include "ssd1306_i2c.hpp"
#include "joystick.hpp"
#include "menu.hpp"
#include "wifi_manager.hpp"
#include "mqtt_manager.hpp"
#include "laser/laser.hpp"

// Queue for input events (joystick directions + button)
static QueueHandle_t g_input_queue = nullptr;

struct UiTaskParams {
    WifiManager* wifi;
    MqttManager* mqtt;
};

static Laser g_laser(LaserConfig{
    .laser_pin = 13,
    .button_pin = 12,
    .button_pull = DebouncedButton::Pull::Up,
    .button_pressed_when_low = true,
    .hold_ms = 250,
    .auto_period_ms = 100,
    .pulse_ms = 20,
});

static void ui_task(void* arg)
{
    auto* params = static_cast<UiTaskParams*>(arg);

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

    Menu menu(kMainItems, sizeof(kMainItems) / sizeof(kMainItems[0]), *params->wifi, *params->mqtt);
    menu.render(display);

    InputEvent ev{};
    for (;;) {
        // Wait for input, but wake periodically so games / Wi-Fi screen can tick.
        if (xQueueReceive(g_input_queue, &ev, pdMS_TO_TICKS(200)) == pdTRUE) {
            menu.handle(ev);
            menu.render(display);
        } else {
            // Timeout: allow menus/games to update without input.
            if (menu.tick()) {
                menu.render(display);
            }
        }
    }
}

int main()
{
    stdio_init_all();
    sleep_ms(50);
    printf("Boot\n");

    g_input_queue = xQueueCreate(/*queue_length=*/8, sizeof(InputEvent));
    configASSERT(g_input_queue);

    static JoystickTaskParams joy_params = {
        .queue = g_input_queue,
        .pins = {
            .adc_x_gpio = 26,
            .adc_y_gpio = 27,
            .btn_gpio   = 22,
        },
        .tuning = {
            .deadzone       = 2500,
            .threshold      = 7000,
            .invert_x       = true,
            .invert_y       = true,
            .repeat_initial = pdMS_TO_TICKS(450),
            .repeat_period  = pdMS_TO_TICKS(150),
            .debounce_ms    = 30,
        }
    };

    static WifiManager wifi;
    wifi.start(tskIDLE_PRIORITY + 1, 1024);

    static MqttManager mqtt(wifi);
    mqtt.start("192.168.100.38", 1883, "pico-laser-ui", tskIDLE_PRIORITY + 1, 4096);

    g_laser.init();
    g_laser.start(tskIDLE_PRIORITY + 1, 512);

    static UiTaskParams ui_params { .wifi = &wifi, .mqtt = &mqtt };

    BaseType_t ok;

    ok = xTaskCreate(joystick_task, "joystick", 1024, &joy_params, tskIDLE_PRIORITY + 2, nullptr);
    configASSERT(ok == pdPASS);

    ok = xTaskCreate(ui_task, "ui", 1024, &ui_params, tskIDLE_PRIORITY + 1, nullptr);
    configASSERT(ok == pdPASS);

    vTaskStartScheduler();
    while (true) {}
}
