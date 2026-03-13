#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "FreeRTOS.h"
#include "task.h"

#include "pico/stdlib.h"

#include "wifi_manager.hpp"
#include "mqtt_manager.hpp"
#include "target/target_controller.hpp"

#ifndef TARGET_BOARD_ID
#define TARGET_BOARD_ID "pico-1"
#endif

#ifndef TARGET_MQTT_CLIENT_ID
#define TARGET_MQTT_CLIENT_ID "laser-target-pico-1"
#endif

static constexpr const char* kStartTopic   = "laser-labs/gamemodes/start";
static constexpr const char* kRepliesTopic = "laser-labs/gamemodes/replies";

struct TargetTaskParams {
    WifiManager* wifi;
    MqttManager* mqtt;
    TargetController* target;
};

static bool kv_get(const char* payload, const char* key, char* out, size_t out_sz) {
    if (!payload || !key || !out || out_sz == 0) return false;

    char needle[32]{};
    std::snprintf(needle, sizeof(needle), "%s=", key);

    const char* p = std::strstr(payload, needle);
    if (!p) return false;
    p += std::strlen(needle);

    size_t i = 0;
    while (p[i] && p[i] != ';' && (i + 1) < out_sz) {
        out[i] = p[i];
        ++i;
    }
    out[i] = '\0';
    return i > 0;
}

static int kv_get_int(const char* payload, const char* key, int fallback) {
    char tmp[16]{};
    if (!kv_get(payload, key, tmp, sizeof(tmp))) return fallback;
    return std::atoi(tmp);
}

static void target_task(void* arg) {
    auto* p = static_cast<TargetTaskParams*>(arg);

    xEventGroupWaitBits(p->wifi->events(), WifiManager::WIFI_CONNECTED, pdFALSE, pdTRUE, portMAX_DELAY);
    xEventGroupWaitBits(p->mqtt->events(), MqttManager::MQTT_CONNECTED, pdFALSE, pdTRUE, portMAX_DELAY);

    printf("TARGET[%s]: ready\n", p->target->board_id());

    for (;;) {
        MqttManager::RxMessage rx{};
        while (p->mqtt->try_receive(rx, 0)) {
            if (std::strcmp(rx.topic, kStartTopic) != 0) {
                continue;
            }

            char target_id[16]{};
            if (!kv_get(rx.payload, "target", target_id, sizeof(target_id))) {
                continue;
            }

            if (std::strcmp(target_id, p->target->board_id()) != 0) {
                continue;
            }

            char cmd[24]{};
            if (!kv_get(rx.payload, "cmd", cmd, sizeof(cmd))) {
                continue;
            }

            const int round = kv_get_int(rx.payload, "round", -1);

            if (std::strcmp(cmd, "arm_zone") == 0) {
                char zone[16]{};
                if (kv_get(rx.payload, "zone", zone, sizeof(zone))) {
                    printf("TARGET[%s]: arm_zone zone=%s round=%d\n",
                           p->target->board_id(), zone, round);
                    p->target->arm_zone(zone, round);

                }
            } else if (std::strcmp(cmd, "arm_random") == 0) {
                printf("TARGET[%s]: arm_random round=%d\n", p->target->board_id(), round);
                p->target->arm_random_zone(round);

            } else if (std::strcmp(cmd, "arm_any") == 0) {
                char bonus_zone[16]{};
                const bool has_bonus = kv_get(rx.payload, "bonus_zone", bonus_zone, sizeof(bonus_zone));
                printf("TARGET[%s]: arm_any round=%d bonus=%s\n",
                       p->target->board_id(),
                       round,
                       has_bonus ? bonus_zone : "-");
                p->target->arm_any(round, has_bonus ? bonus_zone : nullptr);

            } else if (std::strcmp(cmd, "disarm") == 0) {
                printf("TARGET[%s]: disarm\n", p->target->board_id());
                p->target->disarm();

            } else if (std::strcmp(cmd, "celebrate") == 0) {
                printf("TARGET[%s]: celebrate\n", p->target->board_id());
                p->target->disarm();
                p->target->flash_all(3, 90, 70);
            }
        }

        const uint32_t now_ms = static_cast<uint32_t>(to_ms_since_boot(get_absolute_time()));
        const auto ev = p->target->tick(now_ms);
        if (ev.hit && ev.board_id && ev.zone_id) {
            char payload[160]{};
            std::snprintf(payload, sizeof(payload),
                          "target=%s;zone=%s;event=hit;round=%d;bonus=%d",
                          ev.board_id,
                          ev.zone_id,
                          ev.round,
                          ev.bonus ? 1 : 0);

            p->mqtt->publish(kRepliesTopic, payload, 0, 0);
            printf("TARGET[%s]: HIT -> %s\n", p->target->board_id(), payload);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

int main() {
    stdio_init_all();
    sleep_ms(50);
    printf("Boot (target %s)\n", TARGET_BOARD_ID);

    srand(static_cast<unsigned>(to_ms_since_boot(get_absolute_time())));

    static WifiManager wifi;
    wifi.start(tskIDLE_PRIORITY + 1, 1024);

    static MqttManager mqtt(wifi);
    mqtt.start("10.161.6.54", 1883, TARGET_MQTT_CLIENT_ID, tskIDLE_PRIORITY + 1, 4096);

    TargetController::Config cfg{};
    cfg.board_id = TARGET_BOARD_ID;
    cfg.channels[0] = { "head",    28, 2, 15 };
    cfg.channels[1] = { "thorax",  27, 1, 14 };
    cfg.channels[2] = { "stomach", 26, 0, 13 };
    cfg.num_channels = 3;
    cfg.hit_delta = 45;
    cfg.arm_timeout_ms = 15000;
    cfg.cooldown_ms = 1000;
    cfg.debug_print_ms = 0;

    static TargetController target(cfg);
    target.init();

    static TargetTaskParams params{ .wifi = &wifi, .mqtt = &mqtt, .target = &target };
    configASSERT(xTaskCreate(target_task, "target", 2048, &params, tskIDLE_PRIORITY + 1, nullptr) == pdPASS);

    vTaskStartScheduler();
    for (;;) {}
}