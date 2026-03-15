#include "pico_mqtt_platform.h"

#include "pico/time.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"

#include <string.h>

/* ---------------- Timer ---------------- */

static uint64_t now_ms(void) {
    return (uint64_t)to_ms_since_boot(get_absolute_time());
}

void TimerInit(Timer* timer) {
    timer->end_ms = 0;
}

char TimerIsExpired(Timer* timer) {
    return (int64_t)(timer->end_ms - now_ms()) <= 0;
}

void TimerCountdownMS(Timer* timer, unsigned int timeout_ms) {
    timer->end_ms = now_ms() + (uint64_t)timeout_ms;
}

void TimerCountdown(Timer* timer, unsigned int timeout_s) {
    timer->end_ms = now_ms() + (uint64_t)timeout_s * 1000ULL;
}

int TimerLeftMS(Timer* timer) {
    int64_t left = (int64_t)(timer->end_ms - now_ms());
    return left > 0 ? (int)left : 0;
}

/* ---------------- Network ---------------- */

static int net_wait_readable(int sock, int timeout_ms) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(sock, &fds);

    struct timeval tv;
    tv.tv_sec  = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int rc = lwip_select(sock + 1, &fds, NULL, NULL, &tv);
    return rc; // >0 readable, 0 timeout, <0 error
}

static int net_wait_writable(int sock, int timeout_ms) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(sock, &fds);

    struct timeval tv;
    tv.tv_sec  = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int rc = lwip_select(sock + 1, NULL, &fds, NULL, &tv);
    return rc; // >0 writable, 0 timeout, <0 error
}

static int net_read(Network* n, unsigned char* buffer, int len, int timeout_ms) {
    if (n->my_socket < 0) return -1;

    int w = net_wait_readable(n->my_socket, timeout_ms);
    if (w == 0) return 0;     // timeout (Paho will keep trying until Timer expires)
    if (w < 0) return -1;     // select error

    int rc = lwip_recv(n->my_socket, buffer, len, 0);
    return rc; // >0 bytes, 0 closed, <0 error
}

static int net_write(Network* n, unsigned char* buffer, int len, int timeout_ms) {
    if (n->my_socket < 0) return -1;

    int sent = 0;
    while (sent < len) {
        int w = net_wait_writable(n->my_socket, timeout_ms);
        if (w == 0) return -1;  // timeout
        if (w < 0) return -1;

        int rc = lwip_send(n->my_socket, buffer + sent, len - sent, 0);
        if (rc <= 0) return rc; // error or closed
        sent += rc;
    }
    return sent;
}

static void net_disconnect(Network* n) {
    if (n->my_socket >= 0) {
        lwip_close(n->my_socket);
        n->my_socket = -1;
    }
}

void NetworkInit(Network* n) {
    n->my_socket  = -1;
    n->mqttread   = net_read;
    n->mqttwrite  = net_write;
    n->disconnect = net_disconnect;
}

int NetworkConnect(Network* n, char* host, int port) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char portstr[8];
    memset(portstr, 0, sizeof(portstr));
    // simple int->string
    int p = port;
    int idx = 0;
    char tmp[8];
    do { tmp[idx++] = (char)('0' + (p % 10)); p /= 10; } while (p && idx < (int)sizeof(tmp));
    for (int i = 0; i < idx; ++i) portstr[i] = tmp[idx - 1 - i];

    struct addrinfo* res = NULL;
    int err = lwip_getaddrinfo(host, portstr, &hints, &res);
    if (err != 0 || !res) return -1;

    int s = lwip_socket(res->ai_family, res->ai_socktype, 0);
    if (s < 0) {
        lwip_freeaddrinfo(res);
        return -1;
    }

    if (lwip_connect(s, res->ai_addr, res->ai_addrlen) < 0) {
        lwip_close(s);
        lwip_freeaddrinfo(res);
        return -1;
    }

    lwip_freeaddrinfo(res);
    n->my_socket = s;
    return 0;
}

void NetworkDisconnect(Network* n) {
    net_disconnect(n);
}