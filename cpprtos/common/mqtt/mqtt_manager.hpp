#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "event_groups.h"
#include "pico_mqtt_platform.h"

#include "wifi_manager.hpp"

extern "C" {
#include "MQTTClient.h"   // from paho.mqtt.embedded-c
}

class MqttManager {
public:
    enum : EventBits_t {
        MQTT_CONNECTED  = (1u << 0),
        MQTT_CONNECTING = (1u << 1),
        MQTT_FAILED     = (1u << 2),
    };

    struct RxMessage {
        char topic[80];
        char payload[200];
        int  payload_len;
    };

    MqttManager(WifiManager& wifi);

    void start(const char* host, int port, const char* client_id,
               UBaseType_t priority = tskIDLE_PRIORITY + 1,
               uint32_t stackWords = 4096);

    bool publish(const char* topic, const char* payload, int qos = 0, TickType_t timeout = 0);

    // Non-blocking/short blocking receive for game modes
    bool try_receive(RxMessage& out, TickType_t timeout = 0);

    EventGroupHandle_t events() const { return events_; }

private:
    struct TxMessage {
        char topic[80];
        char payload[200];
        int qos;
    };

    static void task_trampoline(void* arg);
    void task();

    static void message_arrived(MessageData* md);

private:
    WifiManager& wifi_;

    EventGroupHandle_t events_{nullptr};
    TaskHandle_t task_{nullptr};
    QueueHandle_t tx_{nullptr};
    QueueHandle_t rx_{nullptr};

    char host_[64]{};
    int port_{1883};
    char client_id_[64]{};

    // Paho state (owned by mqtt task)
    Network network_{};
    MQTTClient client_{};

    // Paho buffers
    unsigned char sendbuf_[512]{};
    unsigned char readbuf_[512]{};

    static inline MqttManager* s_instance_ = nullptr;
};