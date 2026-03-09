#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "FreeRTOS.h"
#include "task.h"

#include "pico/stdlib.h"

#include "wifi_manager.hpp"
#include "mqtt_manager.hpp"

#include "target/target_controller.hpp"

static constexpr const char* kStartTopic    = "laser-labs/gamemodes/start";
static constexpr const char* kFinishedTopic = "laser-labs/gamemodes/finished";

static constexpr int kTotalTargets = 10;
static constexpr const char* kTargetIds[] = { "pico-1", "pico-2", "pico-3" };
static constexpr int kNumTargetIds = 3;
static constexpr uint32_t kReverseDurationMs = 10000;
static constexpr float kSpeedupStartMs = 5000.0f;
static constexpr float kSpeedupDecay   = 0.9f;

struct TargetTaskParams {
    WifiManager* wifi;
    MqttManager* mqtt;
    TargetController* target;
};

static int pick_next(int prev) {
    int n = (rand() % kNumTargetIds);
    if (n == prev) n = (n + 1) % kNumTargetIds;
    return n;
}

//CLASSIC GAMEMODE

static void run_classic(TargetController* target, MqttManager* mqtt) {
    printf("CLASSIC: game starting!\n");

    int hits_done = 0;
    int active_idx = pick_next(-1);
    uint32_t start_ms = (uint32_t)to_ms_since_boot(get_absolute_time());

    target->arm(kTargetIds[active_idx], 1);

    while (hits_done < kTotalTargets) {
        uint32_t now_ms = (uint32_t)to_ms_since_boot(get_absolute_time());
        auto ev = target->tick(now_ms);

        if (ev.hit && ev.target_id) {
            hits_done++;
            printf("CLASSIC: hit %d/%d\n", hits_done, kTotalTargets);

            if (hits_done < kTotalTargets) {
                int prev = active_idx;
                active_idx = pick_next(prev);
                target->arm(kTargetIds[active_idx], hits_done + 1);
            } else {
                target->disarm();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    uint32_t end_ms = (uint32_t)to_ms_since_boot(get_absolute_time());
    float total_s = (float)(end_ms - start_ms) / 1000.0f;

    char payload[64]{};
    std::snprintf(payload, sizeof(payload),
                  "game=classic;result=finished;time=%.2fs;hits=%d",
                  total_s, kTotalTargets);

    mqtt->publish(kFinishedTopic, payload, 0, 0);
    printf("CLASSIC: done! time=%.2fs\n", total_s);
}


//REVERSE GAMEMODE


static void run_reverse(TargetController* target, MqttManager* mqtt) {
    printf("REVERSE: game starting! 10 seconds!\n");

    int hits_done = 0;
    target->arm_all();

    uint32_t start_ms = (uint32_t)to_ms_since_boot(get_absolute_time());
    uint32_t flash_until[3] = {0, 0, 0};
    const uint32_t kFlashMs = 100;
    const uint led_gpios[3] = {15, 14, 13};

    while (true) {
        uint32_t now_ms = (uint32_t)to_ms_since_boot(get_absolute_time());

        if (now_ms - start_ms >= kReverseDurationMs) {
            target->disarm();
            break;
        }

        auto ev = target->tick_all(now_ms);
        if (ev.hit && ev.target_id && ev.round >= 0) {
            hits_done++;
            flash_until[ev.round] = now_ms + kFlashMs;
            printf("REVERSE: hit %d target=%s\n", hits_done, ev.target_id);
        }

        for (int i = 0; i < 3; i++) {
            bool flashing = (int32_t)(flash_until[i] - now_ms) > 0;
            gpio_put(led_gpios[i], flashing ? 1 : 0);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    char payload[64]{};
    std::snprintf(payload, sizeof(payload),
                  "game=reverse;result=finished;hits=%d;time=10.00s",
                  hits_done);

    mqtt->publish(kFinishedTopic, payload, 0, 0);
    printf("REVERSE: done! hits=%d\n", hits_done);
}

//SPEEDUP GAMEMODE


static void run_speedup(TargetController* target, MqttManager* mqtt) {
    printf("SPEEDUP: game starting!\n");

    int hits_done = 0;
    float time_limit_ms = kSpeedupStartMs;
    int active_idx = pick_next(-1);

    target->arm(kTargetIds[active_idx], 1);

    uint32_t target_start_ms = (uint32_t)to_ms_since_boot(get_absolute_time());
    uint32_t game_start_ms = target_start_ms;

    while (true) {
        uint32_t now_ms = (uint32_t)to_ms_since_boot(get_absolute_time());

        // Check if time ran out for current target
        if ((float)(now_ms - target_start_ms) >= time_limit_ms) {
            printf("SPEEDUP: time out! survived %d targets\n", hits_done);
            target->disarm();
            break;
        }

        auto ev = target->tick(now_ms);

        if (ev.hit && ev.target_id) {
            hits_done++;
            time_limit_ms *= kSpeedupDecay;
            printf("SPEEDUP: hit %d next_time=%.0fms\n", hits_done, time_limit_ms);

            int prev = active_idx;
            active_idx = pick_next(prev);
            target->arm(kTargetIds[active_idx], hits_done + 1);
            target_start_ms = (uint32_t)to_ms_since_boot(get_absolute_time());
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    uint32_t end_ms = (uint32_t)to_ms_since_boot(get_absolute_time());
    float total_s = (float)(end_ms - game_start_ms) / 1000.0f;

    char payload[64]{};
    std::snprintf(payload, sizeof(payload),
                  "game=speedup;result=finished;hits=%d;time=%.2fs",
                  hits_done, total_s);

    mqtt->publish(kFinishedTopic, payload, 0, 0);
    printf("SPEEDUP: done! hits=%d time=%.2fs\n", hits_done, total_s);
}

static void target_task(void* arg) {
    auto* p = static_cast<TargetTaskParams*>(arg);

    xEventGroupWaitBits(p->wifi->events(), WifiManager::WIFI_CONNECTED, pdFALSE, pdTRUE, portMAX_DELAY);
    xEventGroupWaitBits(p->mqtt->events(), MqttManager::MQTT_CONNECTED, pdFALSE, pdTRUE, portMAX_DELAY);

    printf("TARGET: ready, waiting for 'start', 'reverse' or 'speedup'\n");

    for (;;) {
        bool start_classic = false;
        bool start_reverse = false;
        bool start_speedup = false;

        while (!start_classic && !start_reverse && !start_speedup) {
            MqttManager::RxMessage rx{};
            while (p->mqtt->try_receive(rx, 0)) {
                if (std::strcmp(rx.topic, kStartTopic) == 0) {
                    if (std::strcmp(rx.payload, "start") == 0)   start_classic = true;
                    if (std::strcmp(rx.payload, "reverse") == 0) start_reverse = true;
                    if (std::strcmp(rx.payload, "speedup") == 0) start_speedup = true;
                }
            }
            vTaskDelay(pdMS_TO_TICKS(50));
        }

        if (start_classic) run_classic(p->target, p->mqtt);
        if (start_reverse) run_reverse(p->target, p->mqtt);
        if (start_speedup) run_speedup(p->target, p->mqtt);

        printf("TARGET: waiting for next game\n");
    }
}

int main() {
    stdio_init_all();
    sleep_ms(50);
    printf("Boot (target)\n");

    srand(to_ms_since_boot(get_absolute_time()));

    static WifiManager wifi;
    wifi.start(tskIDLE_PRIORITY + 1, 1024);

    static MqttManager mqtt(wifi);
    mqtt.start("10.161.4.35", 1883, "laser-target-1", tskIDLE_PRIORITY + 1, 4096);

    TargetController::Config cfg{};
    cfg.channels[0] = { "pico-1", 28, 2, 15 };
    cfg.channels[1] = { "pico-2", 27, 1, 14 };
    cfg.channels[2] = { "pico-3", 26, 0, 13 };
    cfg.num_channels = 3;
    cfg.hit_delta = 30;
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