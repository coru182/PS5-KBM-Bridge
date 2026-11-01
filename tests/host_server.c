/* SPDX-License-Identifier: GPL-3.0-or-later
 * Host test server: the real kbm_http + kbm_api + config code with a fake
 * bridge (simulated running game, capture answered by a fake key press).
 * Usage: ./host_server PORT [CONFIG_OUT_PATH|-] [TITLE_ID]
 */
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../src/kbm_http.h"
#include "../src/kbm_keys.h"

static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static KbmConfig cfg;
static KbmRuntime rt;
static volatile int stop_flag = 0;
static const char *cfg_path = NULL;
static int changes = 0;

static void lk(void *u) { (void)u; pthread_mutex_lock(&mu); }
static void ul(void *u) { (void)u; pthread_mutex_unlock(&mu); }
static void save(void) {
    static char buf[65536];
    int n = kbm_config_write(&cfg, buf, sizeof(buf));
    if (cfg_path && n > 0) { FILE *f = fopen(cfg_path, "w"); if (f) { fwrite(buf, 1, (size_t)n, f); fclose(f); } }
}
static void changed(void *u) {
    (void)u; ++changes;
    char tid[12]; snprintf(tid, sizeof(tid), "%s", rt.title);
    kbm_runtime_set_title(&rt, &cfg, tid);
    rt.hz = kbm_config_effective_hz(&cfg);
    save();
}
static void *fake_press(void *arg) {
    (void)arg;
    usleep(400000);
    pthread_mutex_lock(&mu);
    if (rt.capture_active) {
        uint8_t code = (uint8_t)kbm_key_from_name("H");
        char moved[64];
        kbm_config_bind(&cfg, rt.cap_profile, rt.cap_action, rt.cap_slot, code, moved, sizeof(moved));
        snprintf(rt.cap_result, sizeof(rt.cap_result), "H");
        rt.capture_active = 0;
        changed(NULL);
    }
    pthread_mutex_unlock(&mu);
    return NULL;
}
static void cap_start(void *u) { (void)u; pthread_t t; pthread_create(&t, NULL, fake_press, NULL); pthread_detach(t); }
static void cap_cancel(void *u) { (void)u; }
static void do_stop(void *u) { (void)u; stop_flag = 1; }
static int fake_autostart(void *u, int on, char *msg, size_t n) {
    (void)u;
    if (!rt.autostart_avail) { snprintf(msg, n, "Primero copia ps5-kbm-bridge.elf a /data/etaHEN/payloads/ por FTP"); return -1; }
    pthread_mutex_lock(&mu); rt.autostart_on = on; pthread_mutex_unlock(&mu);
    return 0;
}

static void fake_pointer(void *u, int on) { (void)u; rt.pointer_mode = on; }
static void fake_layer(void *u, int l) { (void)u; rt.layer = l; }

int main(int argc, char **argv) {
    signal(SIGPIPE, SIG_IGN);
    int port = argc > 1 ? atoi(argv[1]) : 18099;
    cfg_path = (argc > 2 && strcmp(argv[2], "-")) ? argv[2] : NULL;
    kbm_config_defaults(&cfg);
    memset(&rt, 0, sizeof(rt));
    rt.version = "0.9-host";
    {   /* simulated PS5 system language: KBM_SYS_LANG=en|es (default es) */
        const char *sl = getenv("KBM_SYS_LANG");
        rt.sys_lang = (sl && !strcmp(sl, "en")) ? KBM_LANG_EN : (sl && !strcmp(sl, "none")) ? KBM_LANG_AUTO : KBM_LANG_ES;
    }
    snprintf(rt.status, sizeof(rt.status), "active");
    rt.vda = 1; rt.receiver = 1;
    snprintf(rt.title, sizeof(rt.title), "%s", argc > 3 ? argv[3] : "PPSA04289");   /* RE2 (2019), EU */
    snprintf(rt.ip, sizeof(rt.ip), "127.0.0.1");
    rt.kb_reports_5s = 12; rt.mouse_reports_5s = 480; rt.e2i_p50_us = 2100; rt.e2i_p95_us = 3900;
    changed(NULL); changes = 0;
    rt.autostart_avail = 1; rt.etahen = 1;
    KbmApi api = { &cfg, &rt, NULL, lk, ul, changed, cap_start, cap_cancel, do_stop, fake_autostart, fake_pointer, fake_layer };
    if (kbm_http_start(port, &api) != 0) { perror("kbm_http_start"); return 1; }
    printf("host_server listening on %d\n", port); fflush(stdout);
    while (!stop_flag) usleep(100000);
    printf("stop requested via API (changes=%d)\n", changes);
    kbm_http_stop();
    usleep(700000);
    return 0;
}
