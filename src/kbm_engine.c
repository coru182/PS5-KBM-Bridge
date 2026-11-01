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
    e->calib_result = -1;
}

static const uint64_t *layer_table(const KbmEngine *e) {
    return (e->map.has_layers && e->layer == KBM_LAYER_VEHICLE) ? e->map.out_veh : e->map.out;
}

static void press_edge(KbmEngine *e, uint8_t code, int now_down) {
    int was = e->down[code];
    e->down[code] = (uint8_t)(now_down ? 1 : 0);
    if (!now_down) { e->suppress[code] = 0; return; }
    if (!was && e->calib_active) {             /* dead-zone finder: any key/click stops it */
        e->calib_active = 0;
        e->suppress[code] = 1;
        if (code == 0x29) e->calib_result = -2;   /* Esc */
        else { int v = e->calib_value - KBM_CALIB_REACTION; e->calib_result = v < 0 ? 0 : v; }
        return;
    }
    if (!was && e->capture_armed) {
        e->capture_armed = 0;
        e->captured = code;
        e->suppress[code] = 1;   /* the captured press must not reach the game */
        return;
    }
    if (!was && e->map.has_layers && (layer_table(e)[code] & KBM_OUT_MODE_TOGGLE))
        e->layer = e->layer == KBM_LAYER_VEHICLE ? KBM_LAYER_FOOT : KBM_LAYER_VEHICLE;
    if (!was && !e->pointer_mode && (layer_table(e)[code] & KBM_BTN_PS)) ++e->ps_presses;
}

void kbm_calib_start(KbmEngine *e, uint64_t t_ns) {
    e->calib_active = 1;
    e->calib_result = -1;
    e->calib_value = 0;
    e->calib_start = t_ns;
}

void kbm_set_layer(KbmEngine *e, int layer) {
    e->layer = (e->map.has_layers && layer == KBM_LAYER_VEHICLE) ? KBM_LAYER_VEHICLE : KBM_LAYER_FOOT;
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
    if ((dx || dy) && e->map.mouse_dpad && !e->pointer_mode) {
        e->dpad_ax += dx; e->dpad_ay += dy;
        e->dpad_last_move = t_ns;
    }
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
    e->dpad_ax = e->dpad_ay = 0;
    e->dpad_mask = 0; e->dpad_until = e->dpad_next = 0;
    e->ring_n = 0;
    e->wheel_up_until = e->wheel_down_until = 0;
}

int kbm_is_down(const KbmEngine *e, uint8_t code) { return e->down[code] && !e->suppress[code]; }
void kbm_capture_arm(KbmEngine *e) { e->capture_armed = 1; e->captured = -1; }
void kbm_capture_cancel(KbmEngine *e) { e->capture_armed = 0; }

/* stick units (-127..127) from a velocity in counts/ms.
 * sens_pct = sensitivity * 100 (so ADS multipliers keep precision). */
static long isqrt(long v) {
    long r = 0, b = 1L << 30;
    while (b > v) b >>= 2;
    while (b) {
        if (v >= r + b) { v -= r + b; r = (r >> 1) + b; } else r >>= 1;
        b >>= 2;
    }
    return r;
}

/* Right stick from raw (unclamped) stick units. Radial, like a real stick:
 * the direction is kept, the length is saturated at 127, optionally boosted
 * for small values (curve) and lifted over the game's dead zone (dz).
 * dz = 0 and curve = 0 keep the per-axis behaviour of v0.6-v0.9. */
static void shape_stick(long sx, long sy, int dz, int curve, int *ox, int *oy) {
    if (!sx && !sy) { *ox = *oy = 0; return; }
    if (!dz && !curve) {
        *ox = (int)(sx > 127 ? 127 : sx < -127 ? -127 : sx);
        *oy = (int)(sy > 127 ? 127 : sy < -127 ? -127 : sy);
        return;
    }
    long mag = isqrt(sx * sx + sy * sy);
    if (mag < 1) mag = 1;
    long m = mag > 127 ? 127 : mag;
    long xn = m * 1024 / 127;                              /* 0..1024 */
    if (curve) xn += (isqrt(xn * 1024) - xn) * curve / 100; /* blend toward sqrt */
    long out = dz + xn * (127 - dz) / 1024;
    if (out > 127) out = 127;
    *ox = (int)(sx * out / mag);
    *oy = (int)(sy * out / mag);
}

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

/* Menus: every `step` mouse counts on the dominant axis = one D-pad press
 * (60 ms down, at most ~9 steps per second). Stale motion is dropped. */
#define KBM_DPAD_PRESS_NS  60000000ull
#define KBM_DPAD_PERIOD_NS 110000000ull
#define KBM_DPAD_IDLE_NS   300000000ull
static uint32_t mouse_dpad_step(KbmEngine *e, uint64_t t_ns) {
    if (e->dpad_mask && t_ns < e->dpad_until) return e->dpad_mask;
    e->dpad_mask = 0;
    if (t_ns > e->dpad_last_move + KBM_DPAD_IDLE_NS) { e->dpad_ax = e->dpad_ay = 0; return 0; }
    if (t_ns < e->dpad_next) return 0;
    int32_t step = 1200 / clampi(e->map.pointer_sens, 1, 50);
    if (step < 8) step = 8;
    int32_t ax = e->dpad_ax < 0 ? -e->dpad_ax : e->dpad_ax, ay = e->dpad_ay < 0 ? -e->dpad_ay : e->dpad_ay;
    if (ax < step && ay < step) return 0;
    if (ax >= ay) {
        e->dpad_mask = e->dpad_ax > 0 ? KBM_BTN_RIGHT : KBM_BTN_LEFT;
        e->dpad_ax += e->dpad_ax > 0 ? -step : step;
        e->dpad_ay = 0;
    } else {
        e->dpad_mask = e->dpad_ay > 0 ? KBM_BTN_DOWN : KBM_BTN_UP;
        e->dpad_ay += e->dpad_ay > 0 ? -step : step;
        e->dpad_ax = 0;
    }
    /* never queue more than two steps: a big swipe shouldn't scroll forever */
    if (e->dpad_ax > 2 * step) e->dpad_ax = 2 * step;
    if (e->dpad_ax < -2 * step) e->dpad_ax = -2 * step;
    if (e->dpad_ay > 2 * step) e->dpad_ay = 2 * step;
    if (e->dpad_ay < -2 * step) e->dpad_ay = -2 * step;
    e->dpad_until = t_ns + KBM_DPAD_PRESS_NS;
    e->dpad_next = t_ns + KBM_DPAD_PERIOD_NS;
    return e->dpad_mask;
}

void kbm_frame(KbmEngine *e, uint64_t t_ns, KbmFrame *o) {
    memset(o, 0, sizeof(*o));
    const int ptr = e->pointer_mode;
    const uint64_t *tab = layer_table(e);
    uint64_t mask = 0;
    for (int c = 0; c < 256; ++c) {
        if (!e->down[c] || e->suppress[c]) continue;
        if (ptr && is_mouse_code(c)) continue;   /* pointer mode owns the mouse */
        mask |= tab[c];
    }
    if (ptr) {
        if (e->down[KBM_KEY_MOUSE1] && !e->suppress[KBM_KEY_MOUSE1]) mask |= KBM_BTN_CROSS;   /* click */
        if (e->down[KBM_KEY_MOUSE2] && !e->suppress[KBM_KEY_MOUSE2]) mask |= KBM_BTN_CIRCLE;  /* back  */
    } else {
        if (e->wheel_up_until > t_ns)   mask |= tab[KBM_KEY_WHEEL_UP];
        if (e->wheel_down_until > t_ns) mask |= tab[KBM_KEY_WHEEL_DOWN];
        if (e->map.mouse_dpad) mask |= mouse_dpad_step(e, t_ns);
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

    if (e->map.mouse_dpad) { o->rx = 128; o->ry = 128; return; }   /* menus: no camera */

    if (e->calib_active) {                             /* dead-zone finder ramp */
        uint64_t steps = t_ns > e->calib_start ? (t_ns - e->calib_start) / KBM_CALIB_STEP_NS : 0;
        if (steps > KBM_CALIB_MAX) { e->calib_active = 0; e->calib_result = -3; steps = 0; }
        e->calib_value = (int)steps;
        o->rx = (uint8_t)(128 + e->calib_value);
        o->ry = 128;
        return;
    }

    long sens_pct = (long)clampi(e->map.sens, 1, 50) * 100;
    if (o->buttons & KBM_BTN_L2)                       /* aiming (ADS) */
        sens_pct = sens_pct * clampi(e->map.ads_pct, 25, 400) / 100;
    int dz = clampi(e->map.deadzone, 0, 90);
    long stx = vx * sens_pct * 16 / 100000;
    long sty = vy * sens_pct * 16 / 100000 * clampi(e->map.yratio, 25, 400) / 100;
    int rx, ry;
    shape_stick(stx, sty, dz, clampi(e->map.curve, 0, 100), &rx, &ry);
    if (e->map.invert_y) ry = -ry;
    o->rx = (uint8_t)(128 + clampi(rx, -127, 127));
    o->ry = (uint8_t)(128 + clampi(ry, -127, 127));
}
