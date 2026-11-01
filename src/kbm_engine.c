/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "kbm_engine.h"
#include "kbm_keys.h"
#include <string.h>

static int clampi(int n, int lo, int hi) { return n < lo ? lo : (n > hi ? hi : n); }

void kbm_mapping_defaults(KbmMapping *m) {
    memset(m, 0, sizeof(*m));
    m->sens = 8;
    m->yratio = 100;
    m->smooth_ms = 16;
    m->deadzone = 0;    /* v0.6 had none and felt right in RE2R */
    m->invert_y = 0;
    m->wheel_ms = 80;
    m->ads_pct = 100;
    m->pointer_sens = 10;
}

void kbm_init(KbmEngine *e) {
    memset(e, 0, sizeof(*e));
    kbm_mapping_defaults(&e->map);
    e->keyboard_owner = -1;
    e->mouse_owner = -1;
    e->captured = -1;
}

static void press_edge(KbmEngine *e, uint8_t code, int now_down) {
    int was = e->down[code];
    e->down[code] = (uint8_t)(now_down ? 1 : 0);
    if (!now_down) { e->suppress[code] = 0; return; }
    if (!was && e->capture_armed) {
        e->capture_armed = 0;
        e->captured = code;
        e->suppress[code] = 1;   /* the captured press must not reach the game */
    }
}

int kbm_keyboard(KbmEngine *e, int owner, const uint8_t *r, size_t n) {
    if (!r || n < 8 || owner < 0) return 0;
    if (e->keyboard_owner >= 0 && e->keyboard_owner != owner) return 0;
    /* 0x01..0x03 = ErrorRollOver/POSTFail/Undefined: keep previous state. */
    for (int i = 2; i < 8; ++i) if (r[i] >= 1 && r[i] <= 3) return 0;
    e->keyboard_owner = owner;
    uint8_t now[256];
    memset(now, 0, sizeof(now));
    for (int b = 0; b < 8; ++b) if (r[0] & (1u << b)) now[0xE0 + b] = 1;
    for (int i = 2; i < 8; ++i) if (r[i] >= 4 && r[i] < 0xE0) now[r[i]] = 1;
    for (int c = 0x04; c <= 0xE7; ++c) if (now[c] != e->down[c]) press_edge(e, (uint8_t)c, now[c]);
    return 1;
}

int kbm_mouse(KbmEngine *e, int owner, const uint8_t *r, size_t n, uint64_t t_ns) {
    if (!r || n < 3 || owner < 0) return 0;
    if (e->mouse_owner >= 0 && e->mouse_owner != owner) return 0;
    e->mouse_owner = owner;
    for (int b = 0; b < 5; ++b) {
        int d = (r[0] >> b) & 1;
        if (d != e->down[KBM_KEY_MOUSE1 + b]) press_edge(e, (uint8_t)(KBM_KEY_MOUSE1 + b), d);
    }
    int dx = (int8_t)r[1], dy = (int8_t)r[2];
    if (dx || dy) {
        unsigned i = e->ring_head;
        e->ring[i].t = t_ns; e->ring[i].dx = (int16_t)dx; e->ring[i].dy = (int16_t)dy;
        e->ring_head = (i + 1) % KBM_MOUSE_RING;
        if (e->ring_n < KBM_MOUSE_RING) ++e->ring_n;
    }
    if (n >= 4 && r[3]) {
        uint64_t until = t_ns + (uint64_t)e->map.wheel_ms * 1000000ull;
        int8_t w = (int8_t)r[3];
        if (w > 0) e->wheel_up_until = until; else e->wheel_down_until = until;
        if (e->capture_armed) {   /* wheel can be captured too */
            e->capture_armed = 0;
            e->captured = w > 0 ? KBM_KEY_WHEEL_UP : KBM_KEY_WHEEL_DOWN;
            if (w > 0) e->wheel_up_until = 0; else e->wheel_down_until = 0;
        }
    }
    return 1;
}

void kbm_disconnect(KbmEngine *e, int owner) {
    if (e->keyboard_owner == owner) {
        e->keyboard_owner = -1;
        for (int c = 0x04; c <= 0xE7; ++c) { e->down[c] = 0; e->suppress[c] = 0; }
    }
    if (e->mouse_owner == owner) {
        e->mouse_owner = -1;
        for (int c = KBM_KEY_MOUSE1; c <= KBM_KEY_MOUSE5; ++c) { e->down[c] = 0; e->suppress[c] = 0; }
        e->ring_n = 0;
        e->wheel_up_until = e->wheel_down_until = 0;
    }
}

void kbm_reset_motion(KbmEngine *e) {
    e->ring_n = 0;
    e->wheel_up_until = e->wheel_down_until = 0;
}

int kbm_is_down(const KbmEngine *e, uint8_t code) { return e->down[code] && !e->suppress[code]; }
void kbm_capture_arm(KbmEngine *e) { e->capture_armed = 1; e->captured = -1; }
void kbm_capture_cancel(KbmEngine *e) { e->capture_armed = 0; }

/* stick units (-127..127) from a velocity in counts/ms.
 * sens_pct = sensitivity * 100 (so ADS multipliers keep precision). */
static int stick_axis(long v_milli, long sens_pct, int ratio_pct, int deadzone) {
    /* v_milli = counts per ms * 1000. Gain 16 stick units per (count/ms) per
     * sens step: matches v0.6 at 60 Hz (8 units per count per 16.7 ms frame). */
    long s = v_milli * sens_pct * 16 / 100000;
    s = s * ratio_pct / 100;
    if (s == 0) return 0;
    long mag = s < 0 ? -s : s;
    long out = deadzone + mag * (127 - deadzone) / 127;
    if (out > 127) out = 127;
    return (int)(s < 0 ? -out : out);
}

static int is_mouse_code(int c) { return c >= KBM_KEY_MOUSE1 && c <= KBM_KEY_WHEEL_DOWN; }

void kbm_frame(KbmEngine *e, uint64_t t_ns, KbmFrame *o) {
    memset(o, 0, sizeof(*o));
    const int ptr = e->pointer_mode;
    uint64_t mask = 0;
    for (int c = 0; c < 256; ++c) {
        if (!e->down[c] || e->suppress[c]) continue;
        if (ptr && is_mouse_code(c)) continue;   /* pointer mode owns the mouse */
        mask |= e->map.out[c];
    }
    if (ptr) {
        if (e->down[KBM_KEY_MOUSE1] && !e->suppress[KBM_KEY_MOUSE1]) mask |= KBM_BTN_CROSS;   /* click */
        if (e->down[KBM_KEY_MOUSE2] && !e->suppress[KBM_KEY_MOUSE2]) mask |= KBM_BTN_CIRCLE;  /* back  */
    } else {
        if (e->wheel_up_until > t_ns)   mask |= e->map.out[KBM_KEY_WHEEL_UP];
        if (e->wheel_down_until > t_ns) mask |= e->map.out[KBM_KEY_WHEEL_DOWN];
    }

    o->buttons = (uint32_t)(mask & 0xffffffffu);
    if (o->buttons & KBM_BTN_L2) o->l2 = 255;
    if (o->buttons & KBM_BTN_R2) o->r2 = 255;

    int up = (mask & KBM_OUT_LS_UP) != 0, dn = (mask & KBM_OUT_LS_DOWN) != 0;
    int lf = (mask & KBM_OUT_LS_LEFT) != 0, rt = (mask & KBM_OUT_LS_RIGHT) != 0;
    int x = rt - lf, y = dn - up;
    int amp = (x && y) ? 90 : 127; /* normalise diagonals */
    o->lx = (uint8_t)(128 + amp * x);
    o->ly = (uint8_t)(128 + amp * y);

    /* mouse velocity over the last smooth_ms */
    int win = clampi(e->map.smooth_ms, 4, 60);
    uint64_t win_ns = (uint64_t)win * 1000000ull;
    long sx = 0, sy = 0;
    for (unsigned k = 0; k < e->ring_n; ++k) {
        unsigned i = (e->ring_head + KBM_MOUSE_RING - 1 - k) % KBM_MOUSE_RING;
        if (e->ring[i].t + win_ns <= t_ns) break;      /* older than window */
        if (e->ring[i].t > t_ns) continue;
        sx += e->ring[i].dx; sy += e->ring[i].dy;
    }
    long vx = sx * 1000 / win, vy = sy * 1000 / win;   /* counts/ms * 1000 */

    if (ptr) {
        /* PS5 browser: left stick moves the cursor, right stick scrolls. */
        long psens = (long)clampi(e->map.pointer_sens, 1, 50) * 100;
        int px = stick_axis(vx, psens, 100, 0), py = stick_axis(vy, psens, 100, 0);
        if (px || py) {
            o->lx = (uint8_t)(128 + clampi(px, -127, 127));
            o->ly = (uint8_t)(128 + clampi(py, -127, 127));
        }
        o->rx = 128;
        o->ry = 128;
        if (e->wheel_up_until > t_ns)   o->ry = 128 - 100;
        if (e->wheel_down_until > t_ns) o->ry = 128 + 100;
        return;
    }

    long sens_pct = (long)clampi(e->map.sens, 1, 50) * 100;
    if (o->buttons & KBM_BTN_L2)                       /* aiming (ADS) */
        sens_pct = sens_pct * clampi(e->map.ads_pct, 25, 400) / 100;
    int dz = clampi(e->map.deadzone, 0, 90);
    int rx = stick_axis(vx, sens_pct, 100, dz);
    int ry = stick_axis(vy, sens_pct, clampi(e->map.yratio, 25, 400), dz);
    if (e->map.invert_y) ry = -ry;
    o->rx = (uint8_t)(128 + clampi(rx, -127, 127));
    o->ry = (uint8_t)(128 + clampi(ry, -127, 127));
}
