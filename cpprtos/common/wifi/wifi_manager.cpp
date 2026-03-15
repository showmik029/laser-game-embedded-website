#include "wifi_manager.hpp"

#include <cstdio>
#include <cstring>

#include "wifi_secrets.hpp"      // provides wifi secrets

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "lwip/netif.h"
#include "lwip/ip4_addr.h"

WifiManager::WifiManager() {
    events_ = xEventGroupCreate();
    cmdQueue_ = xQueueCreate(/*length=*/1, sizeof(WifiCmd));

    // Seed defaults from secrets
    std::strncpy(ssid_, wifi_secrets::kSsid, sizeof(ssid_) - 1);
    ssid_[sizeof(ssid_) - 1] = '\0';

    std::strncpy(password_, wifi_secrets::kPassword, sizeof(password_) - 1);
    password_[sizeof(password_) - 1] = '\0';
}

void WifiManager::start(UBaseType_t priority, uint32_t stackWords) {
    configASSERT(xTaskCreate(&WifiManager::taskTrampoline,
                         "wifi",
                         stackWords,
                         this,
                         priority,
                         &taskHandle_) == pdPASS);
}

bool WifiManager::request_connect(const char* ssid, const char* password, TickType_t timeout) {
    if (!cmdQueue_) return false;

    WifiCmd cmd{};
    cmd.type = WifiCmd::Connect;

    std::strncpy(cmd.ssid, ssid ? ssid : "", sizeof(cmd.ssid) - 1);
    cmd.ssid[sizeof(cmd.ssid) - 1] = '\0';

    std::strncpy(cmd.password, password ? password : "", sizeof(cmd.password) - 1);
    cmd.password[sizeof(cmd.password) - 1] = '\0';

    return xQueueSend(cmdQueue_, &cmd, timeout) == pdTRUE;
}

void WifiManager::get_credentials(char* ssid_out, size_t ssid_out_len,
                                  char* pw_out, size_t pw_out_len) const {
    if (ssid_out && ssid_out_len) {
        taskENTER_CRITICAL();
        std::strncpy(ssid_out, ssid_, ssid_out_len - 1);
        ssid_out[ssid_out_len - 1] = '\0';
        taskEXIT_CRITICAL();
    }
    if (pw_out && pw_out_len) {
        taskENTER_CRITICAL();
        std::strncpy(pw_out, password_, pw_out_len - 1);
        pw_out[pw_out_len - 1] = '\0';
        taskEXIT_CRITICAL();
    }
}

void WifiManager::get_ip(char* ip_out, size_t ip_out_len) const {
    if (!ip_out || !ip_out_len) return;
    taskENTER_CRITICAL();
    std::strncpy(ip_out, ip_, ip_out_len - 1);
    ip_out[ip_out_len - 1] = '\0';
    taskEXIT_CRITICAL();
}

void WifiManager::taskTrampoline(void* arg) {
    static_cast<WifiManager*>(arg)->task();
}

void WifiManager::task() {
    printf("WiFi: init...\n");
    if (cyw43_arch_init()) {
        printf("WiFi: cyw43_arch_init failed\n");
        xEventGroupSetBits(events_, WIFI_FAILED);
        vTaskDelete(nullptr);
    }

    cyw43_arch_enable_sta_mode();

    // Boot connect using defaults
    do_connect(ssid_, password_);

    for (;;) {
        WifiCmd cmd{};
        if (xQueueReceive(cmdQueue_, &cmd, pdMS_TO_TICKS(250)) == pdTRUE) {
            if (cmd.type == WifiCmd::Connect) {
                // Save as “current config”
                taskENTER_CRITICAL();
                std::strncpy(ssid_, cmd.ssid, sizeof(ssid_) - 1);
                ssid_[sizeof(ssid_) - 1] = '\0';
                std::strncpy(password_, cmd.password, sizeof(password_) - 1);
                password_[sizeof(password_) - 1] = '\0';
                taskEXIT_CRITICAL();

                do_connect(ssid_, password_);
            }
        }
    }
}

void WifiManager::do_connect(const char* ssid, const char* password) {
    // Update status bits
    xEventGroupClearBits(events_, WIFI_CONNECTED | WIFI_FAILED);
    xEventGroupSetBits(events_, WIFI_CONNECTING);

    // Best-effort disconnect first (ignore errors)
    cyw43_arch_lwip_begin();
    (void)cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
    cyw43_arch_lwip_end();

    const bool open = (password == nullptr) || (password[0] == '\0');
    const uint32_t auth = open ? CYW43_AUTH_OPEN : CYW43_AUTH_WPA2_AES_PSK;

    printf("WiFi: connecting to '%s'...\n", ssid ? ssid : "");

    int rc = cyw43_arch_wifi_connect_timeout_ms(
        ssid ? ssid : "",
        password ? password : "",
        auth,
        30000
    );

    if (rc != 0) {
        printf("WiFi: connect failed rc=%d\n", rc);
        xEventGroupClearBits(events_, WIFI_CONNECTING);
        xEventGroupSetBits(events_, WIFI_FAILED);
        return;
    }

    // Connected: snapshot IP
    char ipbuf[16] = "0.0.0.0";
    cyw43_arch_lwip_begin();
    if (netif_default) {
        const ip4_addr_t* ip = netif_ip4_addr(netif_default);
        const char* s = ip4addr_ntoa(ip);
        if (s) std::strncpy(ipbuf, s, sizeof(ipbuf) - 1);
        ipbuf[sizeof(ipbuf) - 1] = '\0';
    }
    cyw43_arch_lwip_end();

    taskENTER_CRITICAL();
    std::strncpy(ip_, ipbuf, sizeof(ip_) - 1);
    ip_[sizeof(ip_) - 1] = '\0';
    taskEXIT_CRITICAL();

    printf("WiFi: connected! IP=%s\n", ip_);

    xEventGroupClearBits(events_, WIFI_CONNECTING);
    xEventGroupSetBits(events_, WIFI_CONNECTED);
}