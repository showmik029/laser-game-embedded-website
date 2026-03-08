#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "FreeRTOS.h"
#include "task.h"

#include "pico/stdlib.h"

#include "wifi_manager.hpp"
#include "mqtt_manager.hpp"

#include "target/target_controller.hpp"

static constexpr const char* kStartTopic   = "laser-labs/gamemodes/start";
static constexpr const char* kRepliesTopic = "laser-labs/gamemodes/replies";

static bool kv_get(const char* payload, const char* key, char* out, size_t out_len) {
    if (!payload || !key || !out || out_len == 0) return false;

    const size_t klen = std::strlen(key);
    const char* p = payload;

    while ((p = std::strstr(p, key)) != nullptr) {
        if (p != payload && p[-1] != ';') { p += klen; continue; } // not a key start
        p += klen;
        if (*p != '=') continue;
        p++;

        size_t n = 0;
        while (*p && *p != ';' && n + 1 < out_len) out[n++] = *p++;
        out[n] = '\0';
        return n > 0;
    }
    return false;
}

static int kv_get_int(const char* payload, const char* key, int def = -1) {
    char buf[16]{};
    if (!kv_get(payload, key, buf, sizeof(buf))) return def;
    return std::atoi(buf);
}

struct TargetTaskParams {
    WifiManager* wifi;
    MqttManager* mqtt;
    TargetController* target;
};

static void target_task(void* arg) {
    auto* p = static_cast<TargetTaskParams*>(arg);

    // Wait Wi-Fi then MQTT
    xEventGroupWaitBits(p->wifi->events(), WifiManager::WIFI_CONNECTED, pdFALSE, pdTRUE, portMAX_DELAY);
    xEventGroupWaitBits(p->mqtt->events(), MqttManager::MQTT_CONNECTED, pdFALSE, pdTRUE, portMAX_DELAY);

    printf("TARGET: ready\n");

    for (;;) {
        // Process MQTT messages
        MqttManager::RxMessage rx{};
        while (p->mqtt->try_receive(rx, 0)) {
            if (std::strcmp(rx.topic, kStartTopic) == 0) {
                char target_id[16]{};
                if (kv_get(rx.payload, "target", target_id, sizeof(target_id))) {
                    int round = kv_get_int(rx.payload, "round", -1);
                    printf("TARGET: arm target=%s round=%d\n", target_id, round);
                    p->target->arm(target_id, round);
                }
            }
        }

        // Detect hits
        uint32_t now_ms = (uint32_t)to_ms_since_boot(get_absolute_time());
        auto ev = p->target->tick(now_ms);
        if (ev.hit && ev.target_id) {
            char payload[128]{};
            if (ev.round >= 0) {
                std::snprintf(payload, sizeof(payload),
                              "game=game1;target=%s;event=hit;round=%d",
                              ev.target_id, ev.round);
            } else {
                std::snprintf(payload, sizeof(payload),
                              "game=game1;target=%s;event=hit",
                              ev.target_id);
            }

            p->mqtt->publish(kRepliesTopic, payload, 0, 0);
            printf("TARGET: HIT -> %s\n", payload);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

int main() {
    stdio_init_all();
    sleep_ms(50);
    printf("Boot (target)\n");

    static WifiManager wifi;
    wifi.start(tskIDLE_PRIORITY + 1, 1024);

    static MqttManager mqtt(wifi);
    mqtt.start("192.168.100.38", 1883, "laser-target-emu", tskIDLE_PRIORITY + 1, 4096);

    TargetController::Config cfg{};
    cfg.channels[0] = { "pico-1", 27, 1, 14 };
    cfg.channels[1] = { "pico-2", 26, 0, 15 };
    cfg.hit_delta = 30;          // testing required for this
    cfg.arm_timeout_ms = 15000;
    cfg.cooldown_ms = 1000;
    cfg.debug_print_ms = 0; // 0 = debug print off, otherwise print every x ms

    static TargetController target(cfg);
    target.init();

    static TargetTaskParams params{ .wifi = &wifi, .mqtt = &mqtt, .target = &target };
    configASSERT(xTaskCreate(target_task, "target", 2048, &params, tskIDLE_PRIORITY + 1, nullptr) == pdPASS);

    vTaskStartScheduler();
    for (;;) {}
}