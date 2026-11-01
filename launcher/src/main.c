/* SPDX-License-Identifier: GPL-3.0-or-later
 * PS5 KBM Bridge — home-screen launcher (ShadowMountPlus title PPSA51258).
 *
 * Does one thing: hands /app0/ps5-kbm-bridge.elf to the ELF loader that is
 * already running on the console (elfldr, 127.0.0.1:9021) — exactly what you
 * would otherwise do by hand from the PC — and shows a notification.
 * The bridge closes this launcher by itself a moment later (it sees this
 * title running and calls sceSystemServiceKillApp) and then opens its settings
 * page (http://127.0.0.1:8099/) in the PS5 browser. If the bridge is already
 * running, the same happens without starting it again.
 *
 * Only libkernel calls are used (no libc), so it does not depend on which
 * functions the boilerplate's clean-room libc.prx provides.
 */
#include <stddef.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>

int sceKernelSendNotificationRequest(int unk0, void *req, size_t size, int unk1);

#define ELF_PATH      "/app0/ps5-kbm-bridge.elf"
#define ELFLDR_PORT   9021
#define BRIDGE_PORT   8099
#define MAX_ELF       (4u * 1024u * 1024u)

static uint8_t g_elf[MAX_ELF];

typedef struct { char unk[45]; char message[3075]; } NotifyRequest;

/* volatile so the compiler cannot turn it into a libc memset() call */
static void zero(void *p, size_t n) { volatile char *c = (volatile char *)p; while (n--) *c++ = 0; }

static void notify(const char *msg) {
    static NotifyRequest req;
    size_t i = 0;
    zero(&req, sizeof(req));
    while (msg[i] && i + 1 < sizeof(req.message)) { req.message[i] = msg[i]; ++i; }
    sceKernelSendNotificationRequest(0, &req, sizeof(req), 0);
}

static void sleep_ms(long ms) {
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

/* connect to 127.0.0.1:port; returns fd or -1 */
static int connect_local(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    struct sockaddr_in sa;
    zero(&sa, sizeof(sa));
    sa.sin_len = sizeof(sa);
    sa.sin_family = AF_INET;
    sa.sin_port = (uint16_t)(((port & 0xff) << 8) | ((port >> 8) & 0xff));   /* htons */
    sa.sin_addr.s_addr = 0x0100007fu;                                         /* 127.0.0.1, network order */
    if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) != 0) { close(fd); return -1; }
    return fd;
}

static long read_elf(void) {
    int fd = open(ELF_PATH, O_RDONLY, 0);
    if (fd < 0) return -1;
    long total = 0;
    for (;;) {
        ssize_t n = read(fd, g_elf + total, MAX_ELF - (size_t)total);
        if (n < 0) { close(fd); return -1; }
        if (n == 0) break;
        total += n;
        if ((size_t)total >= MAX_ELF) { close(fd); return -1; }
    }
    close(fd);
    if (total < 4 || g_elf[0] != 0x7f || g_elf[1] != 'E' || g_elf[2] != 'L' || g_elf[3] != 'F') return -1;
    return total;
}

static int send_all(int fd, const uint8_t *p, long n) {
    while (n > 0) {
        ssize_t w = send(fd, p, (size_t)n, 0);
        if (w <= 0) return -1;
        p += w; n -= w;
    }
    return 0;
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    int fd = connect_local(BRIDGE_PORT);
    if (fd >= 0) {
        close(fd);
        notify("PS5 KBM Bridge: abriendo ajustes / opening settings...");
    } else {
        long n = read_elf();
        if (n < 0) {
            notify("PS5 KBM Bridge: falta / missing ps5-kbm-bridge.elf");
        } else if ((fd = connect_local(ELFLDR_PORT)) < 0) {
            notify("PS5 KBM Bridge: elfldr no responde / not responding (9021)");
        } else {
            int bad = send_all(fd, g_elf, n);
            close(fd);
            notify(bad ? "PS5 KBM Bridge: fallo el envio / send to elfldr failed"
                       : "PS5 KBM Bridge: iniciado / started");
        }
    }
    /* The bridge closes this title (sceSystemServiceKillApp). If anything went
     * wrong, close it with the PS button / Options > Close. Never busy-wait. */
    for (;;) sleep_ms(1000);
    return 0;
}
