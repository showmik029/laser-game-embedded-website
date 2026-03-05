#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

    /* ---- Timer (C-only, no Pico headers here) ---- */
    typedef struct Timer {
        uint64_t end_ms;
    } Timer;

    void TimerInit(Timer* timer);
    char TimerIsExpired(Timer* timer);
    void TimerCountdownMS(Timer* timer, unsigned int timeout_ms);
    void TimerCountdown(Timer* timer, unsigned int timeout_s);
    int TimerLeftMS(Timer* timer);

    /* ---- Network (lwIP sockets) ---- */
    typedef struct Network Network;
    struct Network {
        int my_socket;
        int (*mqttread)(Network* n, unsigned char* buffer, int len, int timeout_ms);
        int (*mqttwrite)(Network* n, unsigned char* buffer, int len, int timeout_ms);
        void (*disconnect)(Network* n);
    };

    void NetworkInit(Network* n);
    int  NetworkConnect(Network* n, char* host, int port);
    void NetworkDisconnect(Network* n);

#ifdef __cplusplus
}
#endif