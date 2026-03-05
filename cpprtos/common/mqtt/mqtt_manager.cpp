#include "mqtt_manager.hpp"

#include <cstdio>

extern "C" {
#include "MQTTClient.h"
}

static constexpr const char* kRepliesTopic = "laser-labs/gamemodes/replies";
static constexpr const char* kStartTopic   = "laser-labs/gamemodes/start";

MqttManager::MqttManager(WifiManager& wifi) : wifi_(wifi) {
    events_ = xEventGroupCreate();
    tx_ = xQueueCreate(/*len=*/8, sizeof(TxMessage));
    rx_ = xQueueCreate(/*len=*/8, sizeof(RxMessage));
    configASSERT(events_ && tx_ && rx_);
}

void MqttManager::start(const char* host, int port, const char* client_id,
                        UBaseType_t priority, uint32_t stackWords) {
    std::strncpy(host_, host ? host : "", sizeof(host_) - 1);
    host_[sizeof(host_) - 1] = '\0';
    port_ = port;
    std::strncpy(client_id_, client_id ? client_id : "", sizeof(client_id_) - 1);
    client_id_[sizeof(client_id_) - 1] = '\0';

    // Route C callback to this instance
    s_instance_ = this;

    xTaskCreate(&MqttManager::task_trampoline, "mqtt", stackWords, this, priority, &task_);
    configASSERT(task_);
}

bool MqttManager::publish(const char* topic, const char* payload, int qos, TickType_t timeout) {
    if (!topic || !payload) return false;

    TxMessage m{};
    std::strncpy(m.topic, topic, sizeof(m.topic) - 1);
    m.topic[sizeof(m.topic) - 1] = '\0';

    std::strncpy(m.payload, payload, sizeof(m.payload) - 1);
    m.payload[sizeof(m.payload) - 1] = '\0';

    m.qos = qos;

    return xQueueSend(tx_, &m, timeout) == pdTRUE;
}

bool MqttManager::try_receive(RxMessage& out, TickType_t timeout) {
    return xQueueReceive(rx_, &out, timeout) == pdTRUE;
}

void MqttManager::task_trampoline(void* arg) {
    static_cast<MqttManager*>(arg)->task();
}

void MqttManager::message_arrived(MessageData* md) {
    auto* self = s_instance_;
    if (!self || !md || !md->message || !md->topicName) return;

    RxMessage rx{};
    rx.topic[0] = '\0';
    rx.payload[0] = '\0';
    rx.payload_len = 0;

    // Topic extraction (handle lenstring/cstring)
    const char* t = nullptr;
    int tlen = 0;

    if (md->topicName->lenstring.data && md->topicName->lenstring.len > 0) {
        t = md->topicName->lenstring.data;
        tlen = md->topicName->lenstring.len;
    } else if (md->topicName->cstring) {
        t = md->topicName->cstring;
        tlen = (int)std::strlen(t);
    }

    if (t && tlen > 0) {
        int n = (tlen < (int)sizeof(rx.topic) - 1) ? tlen : (int)sizeof(rx.topic) - 1;
        std::memcpy(rx.topic, t, (size_t)n);
        rx.topic[n] = '\0';
    }

    // Payload
    const auto* p = static_cast<const char*>(md->message->payload);
    int plen = (int)md->message->payloadlen;
    if (p && plen > 0) {
        int n = (plen < (int)sizeof(rx.payload) - 1) ? plen : (int)sizeof(rx.payload) - 1;
        std::memcpy(rx.payload, p, (size_t)n);
        rx.payload[n] = '\0';
        rx.payload_len = n;
    }

    (void)xQueueSend(self->rx_, &rx, 0);
}

void MqttManager::task() {
    // Wait until Wi-Fi is connected
    xEventGroupWaitBits(wifi_.events(), WifiManager::WIFI_CONNECTED, pdFALSE, pdTRUE, portMAX_DELAY);

    for (;;) {
        xEventGroupClearBits(events_, MQTT_CONNECTED | MQTT_FAILED);
        xEventGroupSetBits(events_, MQTT_CONNECTING);

        printf("MQTT: connecting TCP to %s:%d\n", host_, port_);

        NetworkInit(&network_);
        int rc = NetworkConnect(&network_, (char*)host_, port_);
        if (rc != 0) {
            printf("MQTT: NetworkConnect failed rc=%d\n", rc);
            xEventGroupClearBits(events_, MQTT_CONNECTING);
            xEventGroupSetBits(events_, MQTT_FAILED);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        MQTTClientInit(&client_, &network_, /*cmd_timeout_ms=*/1000,
                       sendbuf_, sizeof(sendbuf_), readbuf_, sizeof(readbuf_));

        MQTTPacket_connectData cd = MQTTPacket_connectData_initializer;
        cd.clientID.cstring = client_id_;
        cd.keepAliveInterval = 30;
        cd.cleansession = 1;

        rc = MQTTConnect(&client_, &cd);
        if (rc != 0) {
            printf("MQTT: MQTTConnect failed rc=%d\n", rc);
            NetworkDisconnect(&network_);
            xEventGroupClearBits(events_, MQTT_CONNECTING);
            xEventGroupSetBits(events_, MQTT_FAILED);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        rc = MQTTSubscribe(&client_, (char*)kRepliesTopic, QOS0, &MqttManager::message_arrived);
        if (rc != 0) {
            printf("MQTT: Subscribe failed rc=%d\n", rc);
            MQTTDisconnect(&client_);
            NetworkDisconnect(&network_);
            xEventGroupClearBits(events_, MQTT_CONNECTING);
            xEventGroupSetBits(events_, MQTT_FAILED);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        rc = MQTTSubscribe(&client_, (char*)kStartTopic, QOS0, &MqttManager::message_arrived);
        if (rc != 0) {
            printf("MQTT: Subscribe start failed rc=%d\n", rc);
            MQTTDisconnect(&client_);
            NetworkDisconnect(&network_);
            xEventGroupClearBits(events_, MQTT_CONNECTING);
            xEventGroupSetBits(events_, MQTT_FAILED);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        printf("MQTT: connected + subscribed\n");
        xEventGroupClearBits(events_, MQTT_CONNECTING);
        xEventGroupSetBits(events_, MQTT_CONNECTED);

        // Main loop
        for (;;) {
            // If Wi-Fi dropped, break and re-wait
            const EventBits_t w = xEventGroupGetBits(wifi_.events());
            if (!(w & WifiManager::WIFI_CONNECTED)) {
                printf("MQTT: Wi-Fi not connected, reconnect later\n");
                break;
            }

            // publish pending messages
            TxMessage txm{};
            while (xQueueReceive(tx_, &txm, 0) == pdTRUE) {
                MQTTMessage msg{};
                msg.qos = (txm.qos == 1) ? QOS1 : QOS0;
                msg.retained = 0;
                msg.dup = 0;
                msg.payload = (void*)txm.payload;
                msg.payloadlen = (int)std::strlen(txm.payload);

                int prc = MQTTPublish(&client_, txm.topic, &msg);
                if (prc != 0) {
                    printf("MQTT: publish failed rc=%d\n", prc);
                    // force reconnect
                    break;
                }
            }

            // Pump incoming
            rc = MQTTYield(&client_, 100);
            if (rc != 0) {
                printf("MQTT: yield error rc=%d (reconnecting)\n", rc);
                break;
            }
        }

        // Disconnect and retry
        (void)MQTTDisconnect(&client_);
        (void)NetworkDisconnect(&network_);
        xEventGroupClearBits(events_, MQTT_CONNECTED);
        xEventGroupSetBits(events_, MQTT_FAILED);

        // If Wi-Fi is down, wait for it again
        xEventGroupWaitBits(wifi_.events(), WifiManager::WIFI_CONNECTED, pdFALSE, pdTRUE, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}