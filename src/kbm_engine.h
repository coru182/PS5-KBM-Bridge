/* SPDX-License-Identifier: GPL-3.0-or-later
 * PS5 KBM Bridge v0.7 — configurable keyboard/mouse -> DualSense mapping core.
 * Pure C, no PS5 dependencies (host-tested).
 *
 * Mouse -> right stick is VELOCITY based: counts per millisecond measured over
 * a fixed time window (smooth_ms). The stick value therefore does not depend
 * on how often frames are built (60/120/250 Hz) nor on the game's frame rate.
 */
#pragma once
#include <stdint.h>
#include <stddef.h>

/* ScePad button bits (low 32 bits of an output mask). */
#define KBM_BTN_L3       0x00000002u
#define KBM_BTN_R3       0x00000004u
#define KBM_BTN_OPTIONS  0x00000008u
#define KBM_BTN_UP       0x00000010u
#define KBM_BTN_RIGHT    0x00000020u
#define KBM_BTN_DOWN     0x00000040u
#define KBM_BTN_LEFT     0x00000080u
#define KBM_BTN_L2       0x00000100u
#define KBM_BTN_R2       0x00000200u
#define KBM_BTN_L1       0x00000400u
#define KBM_BTN_R1       0x00000800u
#define KBM_BTN_TRIANGLE 0x00001000u
#define KBM_BTN_CIRCLE   0x00002000u
#define KBM_BTN_CROSS    0x00004000u
#define KBM_BTN_SQUARE   0x00008000u
#define KBM_BTN_TOUCH    0x00100000u
#define KBM_BTN_PS       0x00010000u   /* Ghostcontrol: PS/home button via VDI */
/* Left-stick directions live above bit 32 of the output mask. */
#define KBM_OUT_LS_UP    (1ull << 32)
#define KBM_OUT_LS_DOWN  (1ull << 33)
#define KBM_OUT_LS_LEFT  (1ull << 34)
#define KBM_OUT_LS_RIGHT (1ull << 35)

#define KBM_MOUSE_RING 64

typedef struct {
    uint32_t buttons;
    uint8_t lx, ly, rx, ry, l2, r2;
} KbmFrame;

typedef struct {
    uint64_t out[256];   /* input code -> output mask */
    int sens;            /* 1..50  */
    int yratio;          /* 25..400 % of X sensitivity */
    int smooth_ms;       /* 4..60 velocity window */
    int deadzone;        /* 0..90 minimum stick deflection once moving */
    int invert_y;
    int wheel_ms;        /* how long a wheel notch "holds" its output */
    int ads_pct;         /* 25..400 % of sens while aiming (any key mapped to L2 held) */
    int pointer_sens;    /* 1..50 cursor speed in pointer mode */
} KbmMapping;

typedef struct {
    KbmMapping map;
    uint8_t down[256];       /* raw state of every input code */
    uint8_t suppress[256];   /* ignore until released (used after capture) */
    int keyboard_owner, mouse_owner;
    struct { uint64_t t; int16_t dx, dy; } ring[KBM_MOUSE_RING];
    unsigned ring_n, ring_head;
    uint64_t wheel_up_until, wheel_down_until;
    /* capture: next NEW key/mouse-button press is reported instead of mapped */
    /* pointer mode (PS5 browser): mouse -> left stick cursor, LMB = cross,
     * RMB = circle, wheel -> right stick scroll. Keyboard mapping unchanged. */
    int pointer_mode;
    int capture_armed;
    int captured;            /* -1 none, else code */
} KbmEngine;

void kbm_init(KbmEngine *e);
void kbm_mapping_defaults(KbmMapping *m);  /* params only; out[] cleared */
/* Return 1 on accepted report, 0 rejected/other owner. */
int  kbm_keyboard(KbmEngine *e, int owner, const uint8_t *report, size_t n);
int  kbm_mouse(KbmEngine *e, int owner, const uint8_t *report, size_t n, uint64_t t_ns);
void kbm_disconnect(KbmEngine *e, int owner);
void kbm_reset_motion(KbmEngine *e);
void kbm_frame(KbmEngine *e, uint64_t t_ns, KbmFrame *out);
int  kbm_is_down(const KbmEngine *e, uint8_t code);
void kbm_capture_arm(KbmEngine *e);
void kbm_capture_cancel(KbmEngine *e);
