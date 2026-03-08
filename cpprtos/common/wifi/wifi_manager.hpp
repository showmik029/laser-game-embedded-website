#pragma once

#include <cstddef>
#include <cstdint>

#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"
#include "queue.h"

class WifiManager {
public:
    enum : EventBits_t {
        WIFI_CONNECTED  = (1u << 0),
        WIFI_CONNECTING = (1u << 1),
        WIFI_FAILED     = (1u << 2),
    };

    WifiManager();

    void start(UBaseType_t priority = tskIDLE_PRIORITY + 2,
               uint32_t stackWords = 2048);

    // Returns true if the command was queued.
    bool request_connect(const char* ssid, const char* password, TickType_t timeout = 0);

    // Snapshot helpers
    void get_credentials(char* ssid_out, size_t ssid_out_len,
                         char* pw_out, size_t pw_out_len) const;

    void get_ip(char* ip_out, size_t ip_out_len) const;

    EventGroupHandle_t events() const { return events_; }

private:
    struct WifiCmd {
        enum Type : uint8_t { Connect = 1 } type;
        char ssid[33];
        char password[65];
    };

    static void taskTrampoline(void* arg);
    void task();
    void do_connect(const char* ssid, const char* password);

    EventGroupHandle_t events_{nullptr};
    TaskHandle_t taskHandle_{nullptr};
    QueueHandle_t cmdQueue_{nullptr};

    // Stored “current config” + last IP
    char ssid_[33]{};
    char password_[65]{};
    char ip_[16]{"0.0.0.0"};
};