/* SPDX-License-Identifier: GPL-3.0-or-later
 * PS5 KBM Bridge — any USB boot-protocol keyboard + mouse (one receiver or two devices)
 * -> configurable mapping -> Ghostcontrol virtual DualSense.
 *
 * EXPERIMENTAL. Built as part of Ghostcontrol, not standalone.
 *
 *  - USB: kbm_usbfs (drains the single USB_FS completion queue; v0.6 fix)
 *  - Auto activation: no F8 needed once keyboard AND mouse produced input
 *  - Settings: http://<PS5>:8099, /data/ps5-kbm/config.ini
 *  - Running game: sceSystemServiceGetAppIdOfRunningBigApp + GetAppTitleId
 *  - Native titles: receiver handed back with USB_DEVICEENUMERATE, retaken later
 *
 * Build flags:  -DKBM_DIAG_ONLY=1  read + measure only, never creates a pad.
 */
#include "kbm_bridge.h"
#include "kbm_api.h"
#include "kbm_profiles_io.h"
#include "kbm_config.h"
#include "kbm_engine.h"
#include "kbm_http.h"
#include "kbm_keys.h"
#include "kbm_usbfs.h"
#include "hid_boot.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <dev/usb/usb.h>
#include <dev/usb/usb_endian.h>
#include <dev/usb/usb_ioctl.h>

#ifndef KBM_DIAG_ONLY
#define KBM_DIAG_ONLY 0
#endif
#define KBM_VERSION "0.12"

extern void ghostpad_status_log(const char *fmt, ...);
#define KLOG(...) ghostpad_status_log("[KBM] " __VA_ARGS__)

/* libSceSystemService (signatures as used by ps5-payload-dev/websrv). */
extern int sceSystemServiceGetAppIdOfRunningBigApp(void);
extern int sceSystemServiceGetAppTitleId(int app_id, char *title_id);
extern int sceSystemServiceKillApp(int app_id, int how, int reason, int core_dump);
/* as in ps5-payload-sdk samples/browser (UserService is initialised by gc_main) */
extern int sceSystemServiceLaunchWebBrowser(const char *uri, void *reserved);
/* PS4/PS5 system parameter 1 = system language. Present in the SDK stubs;
 * the result is logged at startup (not assumed to work on every firmware). */
extern int sceSystemServiceParamGetInt(int param_id, int *value);
#define KBM_SYS_PARAM_LANG 1

/* Which app has focus. Verified with the diagnostics ELF on FW 11.60: the
 * output buffer is the 1st argument and its first int32 is the focused app id
 * (the game's id while playing, the system UI's id on the PS5 menu). The 2nd
 * argument gets a spare zeroed buffer in case another firmware uses it. */
extern int sceSystemServiceGetAppFocusedAppStatus(void *a, void *b);
static int focused_app_id(int *out) {
    static unsigned char b1[1024] __attribute__((aligned(16))), b2[1024] __attribute__((aligned(16)));
    memset(b1, 0, sizeof(b1)); memset(b2, 0, sizeof(b2));
    int rc = sceSystemServiceGetAppFocusedAppStatus(b1, b2);
    int32_t id; memcpy(&id, b1, sizeof(id));
    *out = id;
    return rc;
}

#if KBM_DIAG_ONLY
/* Diagnostics only: which app has focus (game vs. PS5 menu)? The signatures
 * are not documented, so both get two zeroed 1 KiB buffers (works whether the
 * output pointer is the 1st or 2nd argument) and the raw bytes are logged.
 * Not linked into the bridge ELF. */
extern int sceLncUtilGetAppFocusedAppStatus(void *a, void *b);
static void focus_probe(int big_app) {
    static unsigned char last[2][96];
    static int last_rc[2] = {0x7fffffff, 0x7fffffff};
    static unsigned char b1[1024] __attribute__((aligned(16))), b2[1024] __attribute__((aligned(16)));
    for (int k = 0; k < 2; ++k) {
        memset(b1, 0, sizeof(b1)); memset(b2, 0, sizeof(b2));
        int rc = k == 0 ? sceSystemServiceGetAppFocusedAppStatus(b1, b2) : sceLncUtilGetAppFocusedAppStatus(b1, b2);
        unsigned char cur[96];
        memcpy(cur, b1, 48); memcpy(cur + 48, b2, 48);
        if (rc == last_rc[k] && !memcmp(cur, last[k], sizeof(cur))) continue;
        last_rc[k] = rc; memcpy(last[k], cur, sizeof(cur));
        char hex[2][100];
        for (int h = 0; h < 2; ++h) {
            for (int i = 0; i < 48; ++i) snprintf(hex[h] + i * 2, 3, "%02x", cur[h * 48 + i]);
        }
        KLOG("FOCUS %s rc=0x%x big_app=0x%x a=%s b=%s\n", k == 0 ? "SystemService" : "LncUtil", (unsigned)rc, (unsigned)big_app, hex[0], hex[1]);
    }
}
#endif
#define KBM_SETTINGS_URL "http://127.0.0.1:8099/"

/* Home-screen launcher app (launcher/, ShadowMountPlus title). The bridge closes
 * it as soon as it has done its job (it only hands this ELF to elfldr). */
#define KBM_LAUNCHER_TID "PPSA51258"

#define KBM_BUS_MIN 0
#define KBM_BUS_MAX 2
#define KBM_DEV_MIN 2
#define KBM_DEV_MAX 31
/* Any USB keyboard / mouse that offers the HID boot protocol: a combo
 * receiver (one device, both interfaces) or a keyboard and a mouse on two
 * devices. Tested receiver: Logitech 046d:c534. */
#define KBM_LOGITECH_VID 0x046d
#define KBM_LOGITECH_PID 0xc534
#define KBM_SONY_VID     0x054c   /* never touch PlayStation controllers */
#define MAX_WORKERS 2
#define KBM_POLL_MS 2
#define KBM_CFG_DIR  "/data/ps5-kbm"
#define KBM_CFG_PATH "/data/ps5-kbm/config.ini"
#define KBM_PROFILES_DIR "/data/ps5-kbm/profiles"
#define KBM_CAPTURE_TIMEOUT_S 10
/* etaHEN starts <dir>/<name>.elf at load when <name>.elf.auto_start exists. */
#define KBM_AUTOSTART_ELF    "/data/etaHEN/payloads/ps5-kbm-bridge.elf"
#define KBM_AUTOSTART_MARKER "/data/etaHEN/payloads/ps5-kbm-bridge.elf.auto_start"

typedef struct {
    int active;
    int idx;                 /* engine owner id */
    char path[32];
    uint16_t vid, pid;
    HidBootLayout layout;    /* only the interfaces this worker took (-1 = not used) */
} KbmWorker;

/* ---- shared state: everything below is guarded by g_lock unless noted ---- */
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static KbmConfig g_cfg;
static KbmRuntime g_rt;
static KbmEngine g_engine;
static KbmApi g_api;
static int g_cfg_dirty = 0;
static uint64_t g_capture_started_ns = 0;
static uint64_t g_pending_evt_ns = 0;

static KbmWorker g_workers[MAX_WORKERS];  /* guarded by g_lock */
static int workers_busy_locked(void) {
    int n = 0;
    for (int i = 0; i < MAX_WORKERS; ++i) n += g_workers[i].active;
    return n;
}
static kbm_inject_cb g_inject;
static kbm_notify_cb g_notify;
static volatile int g_injection_enabled = 0;
static volatile int g_activation_requested = 0;
static volatile int g_stop_requested = 0;
static volatile int g_running = 0;
static volatile int g_want_capture = 1;  /* 0 while a native title runs */
static volatile uint64_t g_diag_release_until = 0;
static volatile uint64_t g_keyboard_activity = 0;
static volatile uint64_t g_mouse_activity = 0;
static volatile uint64_t g_last_input_ns = 0;
static volatile uint32_t g_win_kb = 0, g_win_mouse = 0;
static uint64_t g_start_ns = 0;

static uint64_t mono_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}
static void sleep_ns(uint64_t ns) {
    struct timespec ts = { (time_t)(ns / 1000000000ull), (long)(ns % 1000000000ull) };
    while (nanosleep(&ts, &ts) != 0 && errno == EINTR) {}
}
static void lock(void *u) { (void)u; pthread_mutex_lock(&g_lock); }
static void unlock(void *u) { (void)u; pthread_mutex_unlock(&g_lock); }
#define NOTIFY(...) do { if (g_notify) g_notify(__VA_ARGS__); } while (0)
static int g_sys_lang = KBM_LANG_AUTO;          /* detected PS5 system language */
static int lang_en(void) { return kbm_lang_effective(&g_cfg, g_sys_lang) == KBM_LANG_EN; }
const char *kbm_tr(const char *es, const char *en) { return lang_en() ? en : es; }
/* Same arguments for both formats. */
#define NOTIFY2(es, en, ...) do { if (g_notify) g_notify(lang_en() ? (en) : (es), __VA_ARGS__); } while (0)

static void set_status(const char *s) { snprintf(g_rt.status, sizeof(g_rt.status), "%s", s); }
static void refresh_status_locked(void) {
    if (g_stop_requested) set_status("stopped");
    else if (!g_want_capture) set_status("native");
    else if (g_diag_release_until) set_status("released");
    else if (KBM_DIAG_ONLY) set_status("diag");
    else if (g_rt.vda && g_rt.receiver) set_status("active");
    else set_status("waiting");
}

/* ---------------------------------------------------------------- config */
/* PS key during a game: the PS5 menu is on screen while the game stays open.
 * g_overlay switches to the menu profile until the PS key is pressed again
 * or the running title changes. */
static int g_overlay = 0, g_overlay_saved_layer = KBM_LAYER_FOOT;
static unsigned g_ps_seen = 0;
static int g_focus_ok = -1;   /* focus API: -1 not tried, 0 failed (PS-key fallback), 1 works */

static void apply_profile_locked(const char *title) {
    kbm_runtime_set_title(&g_rt, &g_cfg, title);
    if (g_overlay && title && title[0] && kbm_config_profile_c(&g_cfg, "ps_menu")) {
        snprintf(g_rt.profile, sizeof(g_rt.profile), "ps_menu");
        snprintf(g_rt.title_source, sizeof(g_rt.title_source), "overlay");
        g_rt.has_layers = 0;
        g_rt.mouse_dpad = 1;
    }
    g_rt.overlay = g_overlay && title && title[0];
    const KbmProfile *p = kbm_config_profile_c(&g_cfg, g_rt.profile);
    if (!p) p = &g_cfg.p[0];
    KbmMapping m;
    kbm_profile_to_mapping(p, &m);
    m.pointer_sens = g_cfg.pointer_sens;
    g_engine.map = m;
    kbm_set_layer(&g_engine, g_engine.layer);   /* back to on foot if this profile has no layers */
    g_rt.has_layers = m.has_layers ? (int)m.layers : 0;
    g_rt.layer = g_engine.layer;
    g_rt.hz = kbm_config_effective_hz(&g_cfg);
}

/* lock held */
static void set_overlay_locked(int on, const char *why) {
    if (on == g_overlay) return;
    if (on) g_overlay_saved_layer = g_engine.layer;
    g_overlay = on;
    kbm_reset_motion(&g_engine);
    apply_profile_locked(g_rt.title);
    if (!on) kbm_set_layer(&g_engine, g_overlay_saved_layer);
    g_rt.layer = g_engine.layer;
    KLOG("%s: %s profile (%s)\n", g_rt.title, on ? "PS5 menu" : "game", why);
}

static void config_changed(void *u) {   /* lock held */
    (void)u;
    apply_profile_locked(g_rt.title);
    g_cfg_dirty = 1;
}

static void save_config(void) {
    static char buf[32768];
    pthread_mutex_lock(&g_lock);
    int n = kbm_config_write(&g_cfg, buf, sizeof(buf));
    g_cfg_dirty = 0;
    pthread_mutex_unlock(&g_lock);
    if (n <= 0) { KLOG("config too large to save\n"); return; }
    mkdir(KBM_CFG_DIR, 0755);
    int fd = open(KBM_CFG_DIR "/config.ini.tmp", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) { KLOG("config save open errno=%d\n", errno); return; }
    ssize_t w = write(fd, buf, (size_t)n);
    close(fd);
    if (w != n || rename(KBM_CFG_DIR "/config.ini.tmp", KBM_CFG_PATH) != 0)
        KLOG("config save failed errno=%d\n", errno);
}

static void log_line(void *u, const char *m) { (void)u; KLOG("%s\n", m); }

static void load_config(void) {
    static char buf[32768];
    kbm_config_defaults(&g_cfg);
    int ncustom = kbm_profiles_load_dir(&g_cfg, KBM_PROFILES_DIR, log_line, NULL);   /* before the INI: its tweaks apply on top */
    if (ncustom) KLOG("%d custom profile(s) from %s\n", ncustom, KBM_PROFILES_DIR);
    int fd = open(KBM_CFG_PATH, O_RDONLY);
    if (fd < 0) { KLOG("no %s yet: using defaults (PC-style RE keys)\n", KBM_CFG_PATH); g_cfg_dirty = 1; return; }
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) { g_cfg_dirty = 1; return; }
    buf[n] = 0;
    int applied = kbm_config_parse(&g_cfg, buf);
    KLOG("config loaded: %d settings, default profile=%s, %d remembered games\n",
         applied, g_cfg.default_profile, g_cfg.n_titles);
}

/* ---------------------------------------------------------------- API glue */
static void capture_start(void *u) {   /* lock held */
    (void)u;
    kbm_capture_arm(&g_engine);
    g_capture_started_ns = mono_ns();
}
static void capture_cancel(void *u) { (void)u; kbm_capture_cancel(&g_engine); }

/* Pointer mode: mouse drives the PS5 browser cursor. lock held. */
static volatile int g_pointer_notify = -1;   /* -1 none, 0 off, 1 on: shown by the watcher */
static volatile int g_open_settings_req = 0;  /* F11: the watcher opens the settings page */
static void set_pointer_locked(int on, const char *why) {
    if (g_engine.pointer_mode == on) return;
    g_engine.pointer_mode = on;
    g_rt.pointer_mode = on;
    kbm_reset_motion(&g_engine);
    g_pointer_notify = on;
    KLOG("pointer mode %s (%s)\n", on ? "ON" : "off", why);
}
static const char *layer_name(int l) {
    return l == KBM_LAYER_VEHICLE ? "car" : l == KBM_LAYER_BIKE ? "bike" : l == KBM_LAYER_AIR ? "aircraft" : "on foot";
}
static void api_set_pointer(void *u, int on) { (void)u; set_pointer_locked(on, "web UI"); }
static void api_set_layer(void *u, int layer) {   /* lock held */
    (void)u;
    kbm_set_layer(&g_engine, layer);
    g_rt.layer = g_engine.layer;
    KLOG("MODE -> %s (web UI)\n", layer_name(g_engine.layer));
}
static void api_stop(void *u) { (void)u; g_stop_requested = 1; KLOG("stop requested from web UI\n"); }

static int file_exists(const char *p) { struct stat st; return stat(p, &st) == 0; }
static void refresh_autostart(void) {
    int avail = file_exists(KBM_AUTOSTART_ELF), on = avail && file_exists(KBM_AUTOSTART_MARKER);
    int etahen = file_exists("/data/etaHEN");
    pthread_mutex_lock(&g_lock);
    g_rt.etahen = etahen;
    g_rt.autostart_avail = avail;
    g_rt.autostart_on = on;
    pthread_mutex_unlock(&g_lock);
}
static int api_set_autostart(void *u, int on, char *msg, size_t n) {
    (void)u;
    if (!file_exists(KBM_AUTOSTART_ELF)) {
        snprintf(msg, n, "Primero copia ps5-kbm-bridge.elf a /data/etaHEN/payloads/ por FTP");
        return -1;
    }
    int rc = 0;
    if (on) {
        int fd = open(KBM_AUTOSTART_MARKER, O_WRONLY | O_CREAT, 0644);
        if (fd < 0) rc = -1; else close(fd);
    } else if (file_exists(KBM_AUTOSTART_MARKER) && unlink(KBM_AUTOSTART_MARKER) != 0) {
        rc = -1;
    }
    if (rc) snprintf(msg, n, "no se pudo cambiar el autoarranque (errno %d)", errno);
    else KLOG("etaHEN autostart %s\n", on ? "ENABLED" : "disabled");
    refresh_autostart();
    return rc;
}

/* ---------------------------------------------------------------- injector */
static void *inject_thread(void *unused) {
    (void)unused;
    uint64_t next = mono_ns(), next_stats = 0;
    KbmSamples ins = {0}, e2i = {0};
    int was_enabled = 0;
    uint64_t frames = 0;
    while (g_running && !g_stop_requested) {
        pthread_mutex_lock(&g_lock);
        int hz = kbm_config_effective_hz(&g_cfg);
        pthread_mutex_unlock(&g_lock);
        const uint64_t period = 1000000000ull / (uint64_t)hz;
        if (!g_injection_enabled) { sleep_ns(16000000); next = mono_ns(); continue; }
        if (!was_enabled) {
            was_enabled = 1;
            pthread_mutex_lock(&g_lock);
            kbm_reset_motion(&g_engine);
            g_pending_evt_ns = 0;
            pthread_mutex_unlock(&g_lock);
            next_stats = mono_ns() + 5000000000ull;
        }
        KbmFrame f;
        ScePadData pad;
        uint64_t now = mono_ns(), evt;
        pthread_mutex_lock(&g_lock);
        kbm_frame(&g_engine, now, &f);
        /* PS leaves the browser: hand the mouse back to the camera */
        if (g_engine.pointer_mode && (f.buttons & KBM_BTN_PS)) set_pointer_locked(0, "PS button");
        evt = g_pending_evt_ns;
        g_pending_evt_ns = 0;
        pthread_mutex_unlock(&g_lock);
        memset(&pad, 0, sizeof(pad));
        pad.buttons = f.buttons;
        pad.leftStick.x = f.lx;  pad.leftStick.y = f.ly;
        pad.rightStick.x = f.rx; pad.rightStick.y = f.ry;
        pad.analogButtons.l2 = f.l2; pad.analogButtons.r2 = f.r2;
        pad.quat.w = 1.0f;
        pad.connected = 1;
        uint64_t t0 = mono_ns();
        g_inject(&pad);
        uint64_t t1 = mono_ns();
        ++frames;
        kbm_samples_add(&ins, (uint32_t)((t1 - t0) / 1000u));
        if (evt && t1 > evt) kbm_samples_add(&e2i, (uint32_t)((t1 - evt) / 1000u));
        if (t1 >= next_stats) {
            uint32_t n1, a50, a95, a99, amx, n2, b50, b95, b99, bmx;
            kbm_samples_take(&ins, &n1, &a50, &a95, &a99, &amx);
            kbm_samples_take(&e2i, &n2, &b50, &b95, &b99, &bmx);
            pthread_mutex_lock(&g_lock);
            if (n2) { g_rt.e2i_p50_us = b50; g_rt.e2i_p95_us = b95; }
            pthread_mutex_unlock(&g_lock);
            int long_window = (t1 - g_start_ns) > 120000000000ull;
            KLOG("INJECT %ds hz=%d frames=%u insert_us p50=%u p95=%u max=%u | usb_event->insert_us n=%u p50=%u p95=%u p99=%u max=%u | total=%llu\n",
                 long_window ? 60 : 5, hz, n1, a50, a95, amx, n2, b50, b95, b99, bmx, (unsigned long long)frames);
            next_stats = t1 + (long_window ? 60000000000ull : 5000000000ull);
        }
        next += period;
        now = mono_ns();
        if (next > now) sleep_ns(next - now);
        else next = now;
    }
    return NULL;
}

/* ---------------------------------------------------------------- probe */
static int probe_boot_device(const char *path, HidBootLayout *layout, uint16_t *vid_out, uint16_t *pid_out) {
    int fd = open(path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) return 0;
    uint8_t data[2048];
    struct usb_gen_descriptor ugd;
    memset(&ugd, 0, sizeof(ugd));
    ugd.ugd_data = data;
    ugd.ugd_maxlen = sizeof(data);
    struct usb_device_descriptor dev;
    memset(&dev, 0, sizeof(dev));
    int have = ioctl(fd, USB_GET_DEVICE_DESC, &dev) == 0 && UGETW(dev.idVendor) != KBM_SONY_VID;
    int ok = have && ioctl(fd, USB_GET_FULL_DESC, &ugd) == 0 &&
             ugd.ugd_actlen <= sizeof(data) &&
             hid_boot_find(data, ugd.ugd_actlen, layout);
    if (vid_out) *vid_out = have ? UGETW(dev.idVendor) : 0;
    if (pid_out) *pid_out = have ? UGETW(dev.idProduct) : 0;
    close(fd);
    return ok;
}

/* After a release: does the PS5 own the interfaces again? (0 = driver attached) */
static void log_native_driver_state(void) {
    int found = 0;
    for (int bus = KBM_BUS_MIN; bus <= KBM_BUS_MAX; bus++) {
        for (int dev = KBM_DEV_MIN; dev <= KBM_DEV_MAX; dev++) {
            char path[32];
            HidBootLayout l;
            snprintf(path, sizeof(path), "/dev/ugen%d.%d", bus, dev);
            if (!probe_boot_device(path, &l, NULL, NULL)) continue;
            int fd = open(path, O_RDONLY | O_NONBLOCK);
            if (fd < 0) continue;
            int a = l.keyboard_iface, b = l.mouse_iface;
            int ka = a >= 0 && ioctl(fd, USB_IFACE_DRIVER_ACTIVE, &a) == 0;
            int ma = b >= 0 && ioctl(fd, USB_IFACE_DRIVER_ACTIVE, &b) == 0;
            close(fd);
            KLOG("RELEASE check %s: PS5 driver keyboard=%s mouse=%s\n", path,
                 l.keyboard_iface < 0 ? "-" : ka ? "ATTACHED" : "none", l.mouse_iface < 0 ? "-" : ma ? "ATTACHED" : "none");
            found = 1;
        }
    }
    if (!found) KLOG("RELEASE check: no keyboard/mouse visible yet (still re-enumerating?)\n");
}

static int set_boot_protocol(int fd, int iface) {
    struct usb_ctl_request req;
    memset(&req, 0, sizeof(req));
    req.ucr_request.bmRequestType = UT_WRITE_CLASS_INTERFACE;
    req.ucr_request.bRequest = 0x0b;
    USETW(req.ucr_request.wValue, 0);
    USETW(req.ucr_request.wIndex, (uint16_t)iface);
    USETW(req.ucr_request.wLength, 0);
    return ioctl(fd, USB_DO_REQUEST, &req);
}
static int sys_ioctl(int fd, unsigned long req, void *arg) { return ioctl(fd, req, arg); }

/* HID GET_PROTOCOL: 0 = boot, 1 = report, -1 = request failed. */
static int get_protocol(int fd, int iface) {
    struct usb_ctl_request req;
    uint8_t v = 0xff;
    memset(&req, 0, sizeof(req));
    req.ucr_data = &v;
    req.ucr_request.bmRequestType = UT_READ_CLASS_INTERFACE;
    req.ucr_request.bRequest = 0x03;
    USETW(req.ucr_request.wValue, 0);
    USETW(req.ucr_request.wIndex, (uint16_t)iface);
    USETW(req.ucr_request.wLength, 1);
    if (ioctl(fd, USB_DO_REQUEST, &req) != 0) return -1;
    return v;
}
static int g_take_count = 0;

/* ---------------------------------------------------------------- report sink */
typedef struct {
    int owner;               /* worker index */
    unsigned mouse_logged, kb_logged;
    int f8_was, f9_was, f10_was, f11_was, f12_was;
} KbmSinkCtx;

static int on_report(void *user, int kind, const uint8_t *buf, uint32_t len, uint64_t t_ns) {
    KbmSinkCtx *c = (KbmSinkCtx *)user;
    int ok, active = 0, captured = -1;
    char moved[64] = {0};
    pthread_mutex_lock(&g_lock);
    int layer_before = g_engine.layer;
    ok = kind == KBM_KIND_KEYBOARD ? kbm_keyboard(&g_engine, c->owner, buf, len)
                                   : kbm_mouse(&g_engine, c->owner, buf, len, t_ns);
    if (g_engine.layer != layer_before) {
        g_rt.layer = g_engine.layer;
        KLOG("MODE -> %s (mode key%s)\n", layer_name(g_engine.layer), g_engine.pointer_mode ? ", pointer mode on" : "");
    }
    if (g_engine.ps_presses != g_ps_seen) {          /* PS key: game <-> PS5 menu (only if focus can't be read) */
        g_ps_seen = g_engine.ps_presses;
        if (g_rt.title[0] && g_focus_ok != 1) set_overlay_locked(!g_overlay, "PS key");
    }
    if (ok && g_injection_enabled && g_pending_evt_ns == 0) g_pending_evt_ns = t_ns;
    if (ok && g_engine.captured >= 0) {
        captured = g_engine.captured;
        g_engine.captured = -1;
        if (g_rt.capture_active) {
            if (kbm_key_reserved((uint8_t)captured)) {
                snprintf(g_rt.cap_result, sizeof(g_rt.cap_result), "reserved");
            } else {
                kbm_config_bind(&g_cfg, g_rt.cap_profile, g_rt.cap_action, g_rt.cap_slot,
                                (uint8_t)captured, moved, sizeof(moved));
                const char *nm = kbm_key_name((uint8_t)captured);
                snprintf(g_rt.cap_result, sizeof(g_rt.cap_result), "%s", nm ? nm : "?");
                config_changed(NULL);
            }
            g_rt.capture_active = 0;
        }
    }
    pthread_mutex_unlock(&g_lock);
    if (!ok) return -1;
    g_last_input_ns = t_ns;
    if (captured >= 0) {
        const char *nm = kbm_key_name((uint8_t)captured);
        KLOG("capture -> %s%s%s\n", nm ? nm : "?", moved[0] ? " (moved from " : "", moved[0] ? moved : "");
    }

    if (kind == KBM_KIND_KEYBOARD) {
        ++g_win_kb;
        int slots = 0, f8 = 0, f9 = 0, f10 = 0, f11 = 0, f12 = 0;
        for (int j = 2; j < 8; ++j) {
            if (buf[j]) ++slots;
            if (buf[j] == KBM_KEY_F8) f8 = 1;
            if (buf[j] == KBM_KEY_F9) f9 = 1;
            if (buf[j] == KBM_KEY_F10) f10 = 1;
            if (buf[j] == KBM_KEY_F11) f11 = 1;
            if (buf[j] == KBM_KEY_F12) f12 = 1;
        }
        active = (buf[0] != 0) || slots > 0;
        if (active && g_keyboard_activity++ == 0) KLOG("keyboard activity confirmed (keycodes not logged)\n");
        if (c->kb_logged < 12) { ++c->kb_logged; KLOG("kb sample #%u (take %d) len=%u modifiers_set=%d keys_down=%d\n", c->kb_logged, g_take_count, (unsigned)len, buf[0] != 0, slots); }
        if (f12 && !c->f12_was) { g_stop_requested = 1; KLOG("F12 pressed: stop requested\n"); }
        if (f8 && !c->f8_was && !KBM_DIAG_ONLY && !g_activation_requested && g_mouse_activity) {
            g_activation_requested = 1;
            KLOG("F8 pressed: virtual controller activation requested\n");
        }
        if (f10 && !c->f10_was && KBM_DIAG_ONLY && !g_diag_release_until) {
            g_diag_release_until = mono_ns() + 20000000000ull;
            KLOG("F10 (diag): RELEASE TEST — handing receiver to the PS5 for 20 s\n");
            NOTIFY("%s", kbm_tr("KBM diag: teclado/mouse devueltos a la PS5 por 20 s", "KBM diag: keyboard/mouse handed back to the PS5 for 20 s"));
        }
        if (f10 && !c->f10_was && !KBM_DIAG_ONLY) {
            int started = 0;
            pthread_mutex_lock(&g_lock);
            if (g_rt.vda && !g_engine.calib_active) {
                set_pointer_locked(0, "dead-zone finder");
                kbm_calib_start(&g_engine, mono_ns());
                started = 1;
            }
            pthread_mutex_unlock(&g_lock);
            if (started) {
                KLOG("F10: dead-zone finder started\n");
                NOTIFY("%s", kbm_tr("KBM: midiendo la zona muerta. La camara va a girar despacio: hace clic apenas empiece a moverse (Esc cancela)",
                                    "KBM: measuring the dead zone. The camera will turn slowly: click as soon as it starts moving (Esc cancels)"));
            }
        }
        if (f9 && !c->f9_was && !KBM_DIAG_ONLY) {
            pthread_mutex_lock(&g_lock);
            set_pointer_locked(!g_engine.pointer_mode, "F9");
            pthread_mutex_unlock(&g_lock);
        }
        if (f11 && !c->f11_was && !KBM_DIAG_ONLY) {
            g_open_settings_req = 1;
            KLOG("F11 pressed: opening settings page in the PS5 browser\n");
        }
        c->f8_was = f8; c->f9_was = f9; c->f10_was = f10; c->f11_was = f11; c->f12_was = f12;
    } else {
        ++g_win_mouse;
        active = buf[0] != 0 || buf[1] != 0 || buf[2] != 0;
        if (active && g_mouse_activity++ == 0) KLOG("mouse activity confirmed\n");
        if (c->mouse_logged < 5) { ++c->mouse_logged; KLOG("mouse sample #%u (take %d) len=%u bytes=%02x %02x %02x %02x\n", c->mouse_logged, g_take_count, (unsigned)len, buf[0], buf[1], buf[2], len > 3 ? buf[3] : 0); }
    }
    if (!KBM_DIAG_ONLY && !g_activation_requested && g_keyboard_activity && g_mouse_activity) {
        pthread_mutex_lock(&g_lock);
        int autoa = g_cfg.auto_activate;
        pthread_mutex_unlock(&g_lock);
        if (autoa) {
            g_activation_requested = 1;
            KLOG("keyboard+mouse validated: auto-activating virtual DualSense (no F8 needed)\n");
        }
    }
    return active ? 1 : 0;
}

static void log_window(KbmUsbFs *u, const char *path, uint32_t pr, uint32_t pi, uint32_t pl, uint32_t pe, uint64_t now, int secs) {
    static const char *names[3] = {"?", "kb", "mouse"};
    for (int i = 0; i < u->n_ep; ++i) {
        KbmEp *e = &u->ep[i];
        KbmEpStats *s = &e->win;
        uint32_t n, p50, p95, p99, mx;
        kbm_samples_take(&e->gaps, &n, &p50, &p95, &p99, &mx);
        uint64_t age_ms = e->last_report_ns ? (now - e->last_report_ns) / 1000000ull : 0;
        KLOG("RATE%ds %s ep=0x%02x rep=%u act=%u rej=%u st[ok=%u can=%u stall=%u to=%u oth=%u/%d] "
             "start[ok=%u busy=%u err=%u/%d] gap_us p50=%u p95=%u max=%u last_ms=%llu total=%llu\n",
             secs, names[e->kind < 3 ? e->kind : 0], e->ep_addr, s->reports, s->active, s->rejected,
             s->st_ok, s->st_cancelled, s->st_stalled, s->st_timeout, s->st_other, s->last_other_status,
             s->start_ok, s->start_ebusy, s->start_err, s->last_start_errno,
             p50, p95, mx, (unsigned long long)age_ms, (unsigned long long)e->total_reports);
        memset(s, 0, sizeof(*s));
    }
    KLOG("IO%ds %s completions=%u bad_index=%u complete_err=%u/%d max_batch=%u poll[ready=%u idle=%u late=%u err=%u]\n",
         secs, path, u->completions, u->bad_index, u->complete_err, u->last_complete_errno,
         u->max_batch, pr, pi, pl, pe);
    u->completions = u->bad_index = u->complete_err = u->max_batch = 0;
}

/* ---------------------------------------------------------------- device worker */
static void *device_thread(void *arg) {
    KbmWorker *w = (KbmWorker *)arg;
    HidBootLayout *l = &w->layout;
    static KbmUsbFs usb[MAX_WORKERS];
    static KbmSinkCtx ctxs[MAX_WORKERS];
    KbmUsbFs *u = &usb[w->idx];
    KbmSinkCtx *ctxp = &ctxs[w->idx];
#define ctx (*ctxp)
    int fd = -1, have_fs = 0, sleep_mode = 0, release = 0;
    uint32_t pr = 0, pi = 0, pl = 0, pe = 0, spurious = 0;

    memset(&ctx, 0, sizeof(ctx));
    ctx.owner = w->idx;
    KLOG("worker %d %s %04x:%04x%s keyboard=%d/0x%x mouse=%d/0x%x\n", w->idx, w->path, w->vid, w->pid,
         (w->vid == KBM_LOGITECH_VID && w->pid == KBM_LOGITECH_PID) ? " (Logitech receiver)" : "",
         l->keyboard_iface, l->keyboard_ep & 0xff, l->mouse_iface, l->mouse_ep & 0xff);
    fd = open(w->path, O_RDWR);
    if (fd < 0) { KLOG("open %s errno=%d\n", w->path, errno); goto done; }
    if (l->keyboard_iface >= 0) {
        int iface = l->keyboard_iface;
        int rc = ioctl(fd, USB_IFACE_DRIVER_DETACH, &iface);
        KLOG("keyboard detach iface=%d rc=%d errno=%d\n", iface, rc, rc < 0 ? errno : 0);
    }
    if (l->mouse_iface >= 0 && l->mouse_iface != l->keyboard_iface) {
        int iface = l->mouse_iface;
        int rc = ioctl(fd, USB_IFACE_DRIVER_DETACH, &iface);
        KLOG("mouse detach iface=%d rc=%d errno=%d\n", iface, rc, rc < 0 ? errno : 0);
    }
    if (kbm_usbfs_init(u, fd, sys_ioctl, on_report, &ctx) != 0) { KLOG("FS_INIT errno=%d\n", errno); goto done; }
    have_fs = 1;
    if (l->keyboard_iface >= 0 && set_boot_protocol(fd, l->keyboard_iface) != 0) { KLOG("SET_PROTOCOL kb errno=%d\n", errno); goto done; }
    if (l->mouse_iface >= 0 && set_boot_protocol(fd, l->mouse_iface) != 0) { KLOG("SET_PROTOCOL mouse errno=%d\n", errno); goto done; }
    {
        int a = l->keyboard_iface, b = l->mouse_iface;
        int kd = a >= 0 && ioctl(fd, USB_IFACE_DRIVER_ACTIVE, &a) == 0;
        int md = b >= 0 && ioctl(fd, USB_IFACE_DRIVER_ACTIVE, &b) == 0;
        ++g_take_count;
        KLOG("TAKE %d: PS5 driver after detach kb=%s mouse=%s; protocol kb=%d mouse=%d (0=boot)\n", g_take_count,
             kd ? "STILL-ATTACHED" : "none", md ? "STILL-ATTACHED" : "none",
             l->keyboard_iface >= 0 ? get_protocol(fd, l->keyboard_iface) : -2,
             l->mouse_iface >= 0 ? get_protocol(fd, l->mouse_iface) : -2);
    }
    {
        int idx = 0;
        if (l->keyboard_ep >= 0 && kbm_usbfs_open_ep(u, idx++, (uint8_t)l->keyboard_ep, KBM_KIND_KEYBOARD) != 0) { KLOG("FS_OPEN kb errno=%d\n", errno); goto done; }
        if (l->mouse_ep >= 0 && kbm_usbfs_open_ep(u, idx++, (uint8_t)l->mouse_ep, KBM_KIND_MOUSE) != 0) { KLOG("FS_OPEN mouse errno=%d\n", errno); goto done; }
    }
    for (int i = 0; i < u->n_ep; ++i)
        if (kbm_usbfs_arm(u, i) < 0) { KLOG("initial arm %d errno=%d\n", i, errno); goto done; }
    KLOG("USB boot HID v" KBM_VERSION " ready: %s%s%s req=%u/%u%s\n", w->path,
         l->keyboard_ep >= 0 ? " keyboard" : "", l->mouse_ep >= 0 ? " mouse" : "",
         u->ep[0].req_len, u->n_ep > 1 ? u->ep[1].req_len : 0, KBM_DIAG_ONLY ? " [DIAG: no virtual pad]" : "");
    pthread_mutex_lock(&g_lock);
    g_rt.receiver = 1;
    refresh_status_locked();
    pthread_mutex_unlock(&g_lock);

    uint64_t next_stats = mono_ns() + 5000000000ull;
    while (g_running && !g_stop_requested && g_want_capture && !g_diag_release_until) {
        int p = 0;
        if (!sleep_mode) {
            struct pollfd pfd = { fd, POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM, 0 };
            p = poll(&pfd, 1, KBM_POLL_MS);
            if (p > 0) ++pr; else if (p == 0) ++pi; else { ++pe; if (errno != EINTR) sleep_ns(1000000); }
        } else sleep_ns(1000000);
        uint64_t now = mono_ns();
        int rc = kbm_usbfs_service(u, now, 1);
        if (rc < 0) { KLOG("USB lost/fatal on %s errno=%d\n", w->path, errno); goto done; }
        if (p == 0 && rc > 0) pl += (uint32_t)rc;
        if (p > 0 && rc == 0 && ++spurious > 5000 && !sleep_mode) {
            sleep_mode = 1;
            KLOG("poll() spurious wakeups: falling back to 1 ms sleep\n");
        }
        if (now >= next_stats) {
            int long_window = (now - g_start_ns) > 120000000000ull;
            log_window(u, w->path, pr, pi, pl, pe, now, long_window ? 60 : 5);
            pr = pi = pl = pe = spurious = 0;
            next_stats = now + (long_window ? 60000000000ull : 5000000000ull);
        }
    }
    pthread_mutex_lock(&g_lock);
    release = (!g_want_capture) || g_diag_release_until || (g_stop_requested && g_cfg.release_on_exit);
    pthread_mutex_unlock(&g_lock);
done:
    if (have_fs) kbm_usbfs_teardown(u);
    pthread_mutex_lock(&g_lock);
    kbm_disconnect(&g_engine, w->idx);       /* release every key/button of this device */
    g_rt.receiver = workers_busy_locked() > 1;   /* other devices still held? (this one is leaving) */
    if (g_rt.capture_active) { g_rt.capture_active = 0; snprintf(g_rt.cap_result, sizeof(g_rt.cap_result), "cancelled"); }
    refresh_status_locked();
    pthread_mutex_unlock(&g_lock);
    if (fd >= 0 && release) {
        int dummy = 0;
        int rc = ioctl(fd, USB_DEVICEENUMERATE, &dummy);
        KLOG("RELEASE: USB_DEVICEENUMERATE on %s rc=%d errno=%d (PS5 should re-attach its keyboard/mouse drivers)\n",
             w->path, rc, rc < 0 ? errno : 0);
    }
    if (fd >= 0) close(fd);
    if (release && !g_stop_requested) {
        sleep_ns(3000000000ull);
        log_native_driver_state();
    }
    pthread_mutex_lock(&g_lock);
    w->active = 0;
    w->path[0] = '\0';
    pthread_mutex_unlock(&g_lock);
    KLOG("worker %d stopped (release=%d)\n", w->idx, release);
    return NULL;
#undef ctx
}

static void *scanner_thread(void *unused) {
    (void)unused;
    while (g_running && !g_stop_requested) {
        uint64_t now = mono_ns();
        if (g_diag_release_until && now >= g_diag_release_until) {
            pthread_mutex_lock(&g_lock);
            int busy = workers_busy_locked();
            pthread_mutex_unlock(&g_lock);
            if (!busy) {
                g_diag_release_until = 0;
                KLOG("F10 (diag): RETAKE — detaching PS5 drivers again\n");
                NOTIFY("%s", kbm_tr("KBM diag: retomando teclado/mouse", "KBM diag: taking keyboard/mouse back"));
            }
        }
        /* which roles are already covered, which devices are already in use */
        pthread_mutex_lock(&g_lock);
        int can = g_want_capture && !g_diag_release_until;
        int have_kb = 0, have_mouse = 0, free_slot = -1;
        char used[MAX_WORKERS][32];
        for (int i = 0; i < MAX_WORKERS; ++i) {
            used[i][0] = 0;
            if (g_workers[i].active) {
                have_kb |= g_workers[i].layout.keyboard_ep >= 0;
                have_mouse |= g_workers[i].layout.mouse_ep >= 0;
                snprintf(used[i], sizeof(used[i]), "%s", g_workers[i].path);
            } else if (free_slot < 0) free_slot = i;
        }
        pthread_mutex_unlock(&g_lock);
        can = can && free_slot >= 0 && !(have_kb && have_mouse);
        for (int bus = KBM_BUS_MIN; can && bus <= KBM_BUS_MAX; bus++) {
            for (int dev = KBM_DEV_MIN; can && dev <= KBM_DEV_MAX; dev++) {
                char path[32];
                HidBootLayout layout;
                uint16_t vid = 0, pid = 0;
                snprintf(path, sizeof(path), "/dev/ugen%d.%d", bus, dev);
                int in_use = 0;
                for (int i = 0; i < MAX_WORKERS; ++i) in_use |= !strcmp(used[i], path);
                if (in_use || !probe_boot_device(path, &layout, &vid, &pid)) continue;
                /* take only the roles still missing */
                if (have_kb) layout.keyboard_iface = layout.keyboard_ep = -1;
                if (have_mouse) layout.mouse_iface = layout.mouse_ep = -1;
                if (layout.keyboard_ep < 0 && layout.mouse_ep < 0) continue;
                KLOG("matched USB %04x:%04x at %s:%s%s\n", vid, pid, path,
                     layout.keyboard_ep >= 0 ? " keyboard" : "", layout.mouse_ep >= 0 ? " mouse" : "");
                pthread_mutex_lock(&g_lock);
                KbmWorker *w = &g_workers[free_slot];
                w->idx = free_slot;
                w->layout = layout;
                w->vid = vid; w->pid = pid;
                snprintf(w->path, sizeof(w->path), "%s", path);
                w->active = 1;
                pthread_mutex_unlock(&g_lock);
                pthread_t tid;
                if (pthread_create(&tid, NULL, device_thread, w) == 0) pthread_detach(tid);
                else { pthread_mutex_lock(&g_lock); w->active = 0; pthread_mutex_unlock(&g_lock); }
                can = 0;   /* one new device per pass; the next pass looks for what's still missing */
            }
        }
        sleep_ns(1000000000ull);
    }
    return NULL;
}

/* ---------------------------------------------------------------- game watcher */
static void read_title(char *out, size_t n, int *app_id_out) {
    out[0] = 0;
    int app = sceSystemServiceGetAppIdOfRunningBigApp();
    *app_id_out = app;
    if (app <= 0) return;   /* no game (home screen / system app) */
    char tid[64];
    memset(tid, 0, sizeof(tid));
    int rc = sceSystemServiceGetAppTitleId(app, tid);
    tid[sizeof(tid) - 1] = 0;
    if (rc == 0 && kbm_valid_title_id(tid)) snprintf(out, n, "%s", tid);
}

static void update_ip(void) {
    struct ifaddrs *ifa = NULL, *i;
    if (getifaddrs(&ifa) != 0) return;
    for (i = ifa; i; i = i->ifa_next) {
        if (!i->ifa_addr || i->ifa_addr->sa_family != AF_INET) continue;
        struct sockaddr_in *s = (struct sockaddr_in *)i->ifa_addr;
        uint32_t a = ntohl(s->sin_addr.s_addr);
        if ((a >> 24) == 127) continue;
        char txt[48];
        if (!inet_ntop(AF_INET, &s->sin_addr, txt, sizeof(txt))) continue;
        pthread_mutex_lock(&g_lock);
        snprintf(g_rt.ip, sizeof(g_rt.ip), "%s", txt);
        pthread_mutex_unlock(&g_lock);
        break;
    }
    freeifaddrs(ifa);
}

static void *watch_thread(void *unused) {
    (void)unused;
    char last[12] = "";
    int first = 1, last_app = -2, last_layer = -1, empty_reads = 0;
    uint64_t launcher_seen_ns = 0, open_browser_at = 0;
    unsigned tick = 0;
    while (g_running && !g_stop_requested) {
        char tid[12];
        int app = 0;
        read_title(tid, sizeof(tid), &app);
        /* A game can read as "no game" for a moment (e.g. while the system shows
         * an overlay). Keep the previous title for up to 3 s so the profile and
         * the on-foot / vehicle mode are not reset by a blip. */
        if (!tid[0] && last[0] && !first) {
            if (++empty_reads <= 3) {
                if (empty_reads == 1) KLOG("title read empty (app_id=0x%x): keeping %s for now\n", (unsigned)app, last);
                snprintf(tid, sizeof(tid), "%s", last);
            }
        } else empty_reads = 0;
#if KBM_DIAG_ONLY
        if ((tick % 2) == 0) focus_probe(app);
#endif
        uint64_t now = mono_ns();
        if (!strcmp(tid, KBM_LAUNCHER_TID)) {
            /* our launcher is in the foreground: let its notification show, then close it */
            if (!launcher_seen_ns) launcher_seen_ns = now;
            if (now - launcher_seen_ns >= 1500000000ull) {
                int rc = sceSystemServiceKillApp(app, -1, 0, 0);
                KLOG("closed home-screen launcher %s (app_id=0x%x) rc=0x%x\n", KBM_LAUNCHER_TID, (unsigned)app, (unsigned)rc);
                launcher_seen_ns = 0;
                pthread_mutex_lock(&g_lock);
                int want_page = g_cfg.open_settings_on_launch;
                pthread_mutex_unlock(&g_lock);
                if (want_page) open_browser_at = now + 800000000ull;  /* after the app is gone */
            }
            tid[0] = 0;   /* not a game: no profile/native decisions for it */
            app = 0;
        } else {
            launcher_seen_ns = 0;
        }
        if (g_open_settings_req) {
            /* F11: no launcher to close; a running game stays suspended behind the browser */
            g_open_settings_req = 0;
            NOTIFY("%s", kbm_tr("KBM: abriendo ajustes. Boton PS (tecla Windows) para volver al juego",
                                "KBM: opening settings. PS button (Windows key) to go back to the game"));
            open_browser_at = now;
        }
        if (open_browser_at && now >= open_browser_at) {
            open_browser_at = 0;
            int rc = sceSystemServiceLaunchWebBrowser(KBM_SETTINGS_URL, NULL);
            KLOG("settings page in PS5 browser: sceSystemServiceLaunchWebBrowser(%s) rc=0x%x\n", KBM_SETTINGS_URL, (unsigned)rc);
            if (rc == 0) {
                pthread_mutex_lock(&g_lock);
                set_pointer_locked(1, "settings page opened");
                pthread_mutex_unlock(&g_lock);
            }
        }
        pthread_mutex_lock(&g_lock);
        int calib = -1, calib_dz = 0;
        int mode_msg = -1;                          /* on foot / vehicle switch to announce */
        if (g_rt.has_layers && g_engine.layer != last_layer) {
            if (last_layer >= 0 && g_cfg.notify_mode) mode_msg = g_engine.layer;
            last_layer = g_engine.layer;
        } else if (!g_rt.has_layers) last_layer = -1;
        char calib_prof[64] = "";
        if (g_engine.calib_result != -1) {           /* dead-zone finder finished */
            calib = g_engine.calib_result;
            g_engine.calib_result = -1;
            KbmProfile *cp = kbm_config_profile(&g_cfg, g_rt.profile);
            if (calib >= 0 && cp) {
                cp->deadzone = calib;
                kbm_profile_clamp(cp);
                calib_dz = cp->deadzone;
                snprintf(calib_prof, sizeof(calib_prof), "%s", cp->name);
                config_changed(NULL);
            }
        }
        int changed = first || strcmp(tid, last) != 0;
        if (changed) {
            g_overlay = 0;                              /* new game (or closed): leave the menu state */
            if (g_engine.layer != KBM_LAYER_FOOT) KLOG("MODE -> on foot (title changed to %s)\n", tid[0] ? tid : "none");
            kbm_set_layer(&g_engine, KBM_LAYER_FOOT);   /* new game: start on foot */
            kbm_reset_motion(&g_engine);                /* don't carry mouse motion across profiles */
            apply_profile_locked(tid);
            if (tid[0]) {
                set_pointer_locked(0, "game started");
                KLOG("title %s \"%s\": profile=%s (%s)%s\n", tid, g_rt.title_name[0] ? g_rt.title_name : "?",
                     g_rt.profile, g_rt.title_source, g_rt.native_suggested ? " [db: native KB+M on PS5]" : "");
            }
        }
        int native = tid[0] && g_cfg.native_switch && kbm_config_title_is_native(&g_cfg, tid);
        char prof[KBM_ID_LEN], game[64], pname[64];
        snprintf(prof, sizeof(prof), "%s", g_rt.profile);
        snprintf(game, sizeof(game), "%s", g_rt.title_name[0] ? g_rt.title_name : tid);
        {
            const KbmProfile *pp = kbm_config_profile_c(&g_cfg, g_rt.profile);
            snprintf(pname, sizeof(pname), "%s", pp ? pp->name : g_rt.profile);
        }
        int suggest = g_rt.native_suggested;
        /* capture timeout */
        if (g_rt.capture_active && now - g_capture_started_ns > KBM_CAPTURE_TIMEOUT_S * 1000000000ull) {
            g_rt.capture_active = 0;
            kbm_capture_cancel(&g_engine);
            snprintf(g_rt.cap_result, sizeof(g_rt.cap_result), "timeout");
        }
        g_rt.uptime_s = (uint32_t)((now - g_start_ns) / 1000000000ull);
        g_rt.layer = g_engine.layer;
        g_rt.idle_s = kbm_bridge_idle_seconds();
        if ((tick % 5) == 4) {
            g_rt.kb_reports_5s = g_win_kb; g_rt.mouse_reports_5s = g_win_mouse;
            g_win_kb = 0; g_win_mouse = 0;
        }
        int dirty = g_cfg_dirty;
        pthread_mutex_unlock(&g_lock);
        if (mode_msg >= 0)
            NOTIFY("%s", mode_msg == KBM_LAYER_VEHICLE ? kbm_tr("KBM: modo vehiculo", "KBM: vehicle mode")
                       : mode_msg == KBM_LAYER_BIKE ? kbm_tr("KBM: modo bici", "KBM: bike mode")
                       : mode_msg == KBM_LAYER_AIR ? kbm_tr("KBM: modo aire", "KBM: aircraft mode")
                                                   : kbm_tr("KBM: modo a pie", "KBM: on-foot mode"));
        if (calib >= 0) {
            KLOG("dead-zone finder: %d -> profile %s deadzone=%d\n", calib, calib_prof, calib_dz);
            NOTIFY2("KBM: zona muerta = %d (perfil %s). Ajustable en la pagina", "KBM: dead zone = %d (profile %s). Adjustable on the page", calib_dz, calib_prof);
        } else if (calib == -2 || calib == -3) {
            KLOG("dead-zone finder: %s\n", calib == -2 ? "cancelled" : "no click before the end of the ramp");
            NOTIFY("%s", calib == -2 ? kbm_tr("KBM: medicion cancelada", "KBM: measurement cancelled")
                                     : kbm_tr("KBM: no hubo clic; la zona muerta no cambio", "KBM: no click; dead zone unchanged"));
        }

        if (changed || app != last_app) {
            KLOG("GAME %s (app_id=0x%x) -> profile=%s%s\n", tid[0] ? tid : "none", (unsigned)app, prof,
                 native ? " NATIVE" : "");
            last_app = app;
        }
        if (changed && !first) {
            if (tid[0] && !native) NOTIFY("KBM: %s -> %s%s", game, pname,
                                          suggest ? kbm_tr(" (tiene teclado/mouse nativo: F11)", " (supports keyboard/mouse natively: F11)") : "");
        }
        if (KBM_DIAG_ONLY) {
            if (changed && native) KLOG("DIAG: %s is marked native; bridge build would release the receiver here\n", tid);
        } else if (native && g_want_capture) {
            g_want_capture = 0;
            KLOG("native title %s: releasing receiver to the PS5\n", tid);
            NOTIFY2("KBM: %s usa teclado/mouse nativo. Receptor liberado", "KBM: %s uses native keyboard/mouse. Receiver released", game);
        } else if (!native && !g_want_capture) {
            g_want_capture = 1;
            KLOG("left native title: retaking receiver\n");
            NOTIFY("%s", kbm_tr("KBM: retomando teclado/mouse", "KBM: taking keyboard/mouse back"));
        }
        pthread_mutex_lock(&g_lock);
        refresh_status_locked();
        pthread_mutex_unlock(&g_lock);
        if (!g_want_capture) g_last_input_ns = now;  /* native time is not idle time */
        if (g_pointer_notify >= 0) {
            int on = g_pointer_notify;
            g_pointer_notify = -1;
            if (on) NOTIFY("%s", kbm_tr("KBM: modo puntero (mouse = cursor). F9 o boton PS para volver",
                                        "KBM: pointer mode (mouse = cursor). F9 or PS button to go back"));
            else NOTIFY("%s", kbm_tr("KBM: mouse = camara", "KBM: mouse = camera"));
        }
        if (dirty) save_config();
        if ((tick % 30) == 0) update_ip();
        if ((tick % 5) == 0) refresh_autostart();
        snprintf(last, sizeof(last), "%s", tid);
        first = 0;
        ++tick;
        for (int i = 0; i < 10 && !g_open_settings_req && g_running && !g_stop_requested; ++i) {
            sleep_ns(100000000ull);   /* 1 s, but react to F11 within ~0.1 s */
            /* game or PS5 menu on screen? (every 200 ms while a game is running) */
            if ((i & 1) || !tid[0] || app <= 0 || g_focus_ok == 0 || KBM_DIAG_ONLY) continue;
            int fa = 0, rc = focused_app_id(&fa);
            if (rc != 0) {
                g_focus_ok = 0;
                KLOG("focus: sceSystemServiceGetAppFocusedAppStatus rc=0x%x -> using the PS key to switch to the menu profile\n", (unsigned)rc);
                continue;
            }
            if (g_focus_ok != 1) { g_focus_ok = 1; KLOG("focus: detection works (focused=0x%x game=0x%x)\n", (unsigned)fa, (unsigned)app); }
            pthread_mutex_lock(&g_lock);
            g_rt.focus_auto = 1;
            if (fa != app) {
                if (!g_overlay) KLOG("focus: 0x%x (not the game)\n", (unsigned)fa);
                set_overlay_locked(1, "PS5 menu has focus");
            } else {
                set_overlay_locked(0, "game has focus");
                if (g_engine.pointer_mode) set_pointer_locked(0, "game has focus again");
            }
            pthread_mutex_unlock(&g_lock);
        }
    }
    if (g_cfg_dirty) save_config();
    return NULL;
}

/* ---------------------------------------------------------------- public */
int kbm_bridge_start(kbm_inject_cb inject, kbm_notify_cb notify) {
    if (!inject) return -1;
    g_inject = inject;
    g_notify = notify;
    g_start_ns = mono_ns();
    g_last_input_ns = g_start_ns;
    kbm_init(&g_engine);
    memset(&g_rt, 0, sizeof(g_rt));
    g_rt.version = KBM_VERSION;
    g_rt.diag = KBM_DIAG_ONLY;
    load_config();
    {
        int id = -1;
        int rc = sceSystemServiceParamGetInt(KBM_SYS_PARAM_LANG, &id);
        g_sys_lang = rc == 0 ? kbm_lang_from_system(id) : KBM_LANG_AUTO;
        g_rt.sys_lang = g_sys_lang;
        KLOG("system language: rc=0x%x id=%d -> %s (setting: %s)\n", (unsigned)rc, id,
             g_sys_lang == KBM_LANG_ES ? "es" : g_sys_lang == KBM_LANG_EN ? "en" : "unknown, using English",
             kbm_lang_code(g_cfg.lang));
    }
    apply_profile_locked("");
    refresh_status_locked();
    g_running = 1;

    g_api.cfg = &g_cfg; g_api.rt = &g_rt; g_api.user = NULL;
    g_api.lock = lock; g_api.unlock = unlock; g_api.config_changed = config_changed;
    g_api.capture_start = capture_start; g_api.capture_cancel = capture_cancel; g_api.stop = api_stop;
    g_api.set_autostart = api_set_autostart;
    g_api.set_pointer = api_set_pointer;
    g_api.set_layer = api_set_layer;
    g_api.profiles_dir = KBM_PROFILES_DIR;
    /* Single instance: the settings port doubles as a lock. If it is taken,
     * another bridge (e.g. autostarted by etaHEN) already owns the receiver. */
    if (kbm_http_start(KBM_HTTP_PORT, &g_api) != 0) {
        int e = errno;
        KLOG("settings server :%d failed errno=%d: another bridge is probably running; exiting\n", KBM_HTTP_PORT, e);
        NOTIFY("%s", kbm_tr("KBM: ya hay un bridge corriendo (ajustes en http://IP-PS5:8099)",
                            "KBM: a bridge is already running (settings at http://PS5-IP:8099)"));
        g_running = 0;
        return -5;
    }
    KLOG("settings server on port %d\n", KBM_HTTP_PORT);

    pthread_t a, b, c;
    if (pthread_create(&a, NULL, inject_thread, NULL) != 0) { g_running = 0; return -2; }
    pthread_detach(a);
    if (pthread_create(&b, NULL, scanner_thread, NULL) != 0) { g_running = 0; return -3; }
    pthread_detach(b);
    if (pthread_create(&c, NULL, watch_thread, NULL) != 0) { g_running = 0; return -4; }
    pthread_detach(c);
    KLOG("v" KBM_VERSION "%s started: profile=%s hz=%d auto_activate=%d native_switch=%d idle_exit_min=%d autostart_elf=%d\n",
         KBM_DIAG_ONLY ? " DIAG" : "", g_rt.profile, g_rt.hz, g_cfg.auto_activate, g_cfg.native_switch,
         g_cfg.idle_exit_min, file_exists(KBM_AUTOSTART_ELF));
    return 0;
}

int kbm_bridge_activation_requested(void) { return KBM_DIAG_ONLY ? 0 : g_activation_requested; }
int kbm_bridge_stop_requested(void) { return g_stop_requested; }
uint32_t kbm_bridge_idle_limit_s(void) {
    pthread_mutex_lock(&g_lock);
    uint32_t m = (uint32_t)g_cfg.idle_exit_min;
    pthread_mutex_unlock(&g_lock);
    return m * 60u;
}
uint32_t kbm_bridge_idle_seconds(void) {
    uint64_t last = g_last_input_ns, now = mono_ns();
    if (!last || now <= last) return 0;
    return (uint32_t)((now - last) / 1000000000ull);
}
void kbm_bridge_enable_injection(void) { if (!KBM_DIAG_ONLY) g_injection_enabled = 1; }
void kbm_bridge_set_vda(int created) {
    pthread_mutex_lock(&g_lock);
    g_rt.vda = created;
    refresh_status_locked();
    pthread_mutex_unlock(&g_lock);
}
void kbm_bridge_shutdown(void) {
    g_injection_enabled = 0;
    g_stop_requested = 1;
    /* wait for the worker to tear down (and hand the receiver back) */
    for (int i = 0; i < 30; ++i) {
        pthread_mutex_lock(&g_lock);
        int busy = workers_busy_locked();
        pthread_mutex_unlock(&g_lock);
        if (!busy) break;
        sleep_ns(100000000ull);
    }
    if (g_cfg_dirty) save_config();
    kbm_http_stop();
    g_running = 0;
}
