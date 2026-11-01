/* SPDX-License-Identifier: GPL-3.0-or-later
 * PS5 KBM Bridge — configurable keyboard/mouse -> DualSense mapping core.
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
/* On key press: on foot <-> car (MODE_TOGGLE), or next mode in the cycle
 * on foot -> car -> bike -> aircraft (MODE_CYCLE, skips modes the profile lacks). */
#define KBM_OUT_MODE_TOGGLE (1ull << 36)
#define KBM_OUT_MODE_CYCLE  (1ull << 37)

/* Key layers: a profile may bind a key differently on foot and in a vehicle. */
#define KBM_LAYER_FOOT    0
#define KBM_LAYER_VEHICLE 1   /* car (and anything driven like one) */
#define KBM_LAYER_BOTH    2   /* action layer only: applies in every mode */
#define KBM_LAYER_BIKE    3   /* bicycles */
#define KBM_LAYER_AIR     4   /* planes and helicopters */
#define KBM_LAYER_BIT(l)  (1u << (l))

#define KBM_MOUSE_RING 64

typedef struct {
    uint32_t buttons;
    uint8_t lx, ly, rx, ry, l2, r2;
} KbmFrame;

typedef struct {
    uint64_t out[256];   /* input code -> output mask (on foot / only layer) */
    uint64_t out_veh[256]; /* same, car layer (used when has_layers) */
    uint64_t out_bike[256], out_air[256];
    int has_layers;      /* profile defines vehicle actions */
    unsigned layers;     /* KBM_LAYER_BIT() of the vehicle modes it defines */
    int mouse_dpad;      /* menus: mouse movement -> D-pad steps (pointer_sens = speed), no camera */
    int sens;            /* 1..50  */
    int yratio;          /* 25..400 % of X sensitivity */
    int smooth_ms;       /* 4..60 velocity window */
    int deadzone;        /* 0..90 game's right-stick dead zone to jump over (radial) */
    int curve;           /* 0..100 boost for small movements (0 = linear) */
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
    int layer;               /* KBM_LAYER_FOOT / KBM_LAYER_VEHICLE */
    /* mouse_dpad: accumulated counts and the current D-pad step */
    int32_t dpad_ax, dpad_ay;
    uint64_t dpad_last_move, dpad_until, dpad_next;
    uint32_t dpad_mask;
    /* dead-zone finder: right stick ramps up slowly; the player clicks when the
     * camera starts to move. calib_result: -1 running/none, >=0 measured value,
     * -2 cancelled (Esc), -3 nothing pressed before the end of the ramp. */
    int calib_active, calib_result, calib_value;
    unsigned ps_presses;     /* new presses of a key mapped to the PS button (not in pointer mode) */
    uint64_t calib_start;
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
void kbm_set_layer(KbmEngine *e, int layer);
void kbm_calib_start(KbmEngine *e, uint64_t t_ns);
#define KBM_CALIB_STEP_NS   60000000ull   /* one stick unit every 60 ms */
#define KBM_CALIB_MAX       100           /* ramp end (stick units, 127 = full) */
#define KBM_CALIB_REACTION  4             /* ~240 ms reaction time, subtracted */   /* clamps to foot if the mapping has no layers */
void kbm_frame(KbmEngine *e, uint64_t t_ns, KbmFrame *out);
int  kbm_is_down(const KbmEngine *e, uint8_t code);
void kbm_capture_arm(KbmEngine *e);
void kbm_capture_cancel(KbmEngine *e);
