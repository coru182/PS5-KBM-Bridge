/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "kbm_http.h"
#include "kbm_webui_gen.h"

#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

static int g_listen_fd = -1;
static volatile int g_http_run = 0;
static KbmApi *g_api;
static char g_body[48 * 1024];   /* only the server thread touches these */
static char g_req[4096];

static void send_all(int fd, const char *p, size_t n) {
    while (n > 0) {
        ssize_t w = send(fd, p, n, MSG_NOSIGNAL);
        if (w < 0) { if (errno == EINTR) continue; return; }
        p += w; n -= (size_t)w;
    }
}

static void respond(int fd, int status, const char *ctype, const char *body, size_t len) {
    char hdr[384];
    const char *reason = status == 200 ? "OK" : status == 204 ? "No Content" :
                         status == 400 ? "Bad Request" : status == 404 ? "Not Found" : "Error";
    int h = snprintf(hdr, sizeof(hdr),
        "HTTP/1.0 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\n"
        "Access-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: GET, OPTIONS\r\n"
        "Cache-Control: no-store\r\nConnection: close\r\n\r\n",
        status, reason, ctype, len);
    send_all(fd, hdr, (size_t)h);
    if (len) send_all(fd, body, len);
}

static void handle(int fd) {
    struct timeval tv = { 2, 0 };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#ifdef SO_NOSIGPIPE
    int one = 1; setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#endif
    size_t got = 0;
    while (got + 1 < sizeof(g_req)) {
        ssize_t r = recv(fd, g_req + got, sizeof(g_req) - 1 - got, 0);
        if (r < 0 && errno == EINTR) continue;
        if (r <= 0) break;
        got += (size_t)r;
        g_req[got] = 0;
        if (strstr(g_req, "\r\n\r\n") || strstr(g_req, "\n\n")) break;
    }
    g_req[got] = 0;
    char method[8] = {0}, target[2048] = {0};
    if (sscanf(g_req, "%7s %2047s", method, target) != 2) { respond(fd, 400, "text/plain", "bad request", 11); return; }
    if (!strcmp(method, "OPTIONS")) { respond(fd, 204, "text/plain", "", 0); return; }
    if (strcmp(method, "GET")) { respond(fd, 400, "text/plain", "GET only", 8); return; }

    char *qs = strchr(target, '?');
    if (qs) *qs++ = 0;
    if (!strcmp(target, "/") || !strcmp(target, "/index.html")) {
        respond(fd, 200, "text/html; charset=utf-8", KBM_WEBUI_HTML, sizeof(KBM_WEBUI_HTML) - 1);
    } else if (!strcmp(target, "/api/state")) {
        KbmQuery q;
        kbm_query_parse(qs, &q);
        int n = kbm_api_state_json(g_api, kbm_query_get(&q, "edit"), g_body, sizeof(g_body));
        if (n < 0) respond(fd, 500, "text/plain", "state too large", 15);
        else respond(fd, 200, "application/json", g_body, (size_t)n);
    } else if (!strncmp(target, "/api/", 5)) {
        KbmQuery q;
        kbm_query_parse(qs, &q);
        int st = kbm_api_command(g_api, target, &q, g_body, sizeof(g_body));
        respond(fd, st, "application/json", g_body, strlen(g_body));
    } else {
        respond(fd, 404, "text/plain", "not found", 9);
    }
}

static void *server_thread(void *unused) {
    (void)unused;
    while (g_http_run) {
        struct pollfd p = { g_listen_fd, POLLIN, 0 };
        int r = poll(&p, 1, 500);
        if (r <= 0) continue;
        int fd = accept(g_listen_fd, NULL, NULL);
        if (fd < 0) continue;
        handle(fd);
        shutdown(fd, SHUT_RDWR);
        close(fd);
    }
    close(g_listen_fd);
    g_listen_fd = -1;
    return NULL;
}

int kbm_http_start(int port, KbmApi *api) {
    g_api = api;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons((uint16_t)port);
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(fd, (struct sockaddr *)&sin, sizeof(sin)) != 0 || listen(fd, 8) != 0) { close(fd); return -1; }
    g_listen_fd = fd;
    g_http_run = 1;
    pthread_t t;
    if (pthread_create(&t, NULL, server_thread, NULL) != 0) { g_http_run = 0; close(fd); g_listen_fd = -1; return -1; }
    pthread_detach(t);
    return 0;
}

void kbm_http_stop(void) { g_http_run = 0; }
