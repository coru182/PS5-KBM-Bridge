/* SPDX-License-Identifier: GPL-3.0-or-later
 * Input codes: HID keyboard usages 0x04..0xE7 (positional, US names),
 * plus virtual codes for mouse buttons and wheel.
 */
#pragma once
#include <stdint.h>

#define KBM_KEY_NONE        0x00
#define KBM_KEY_LCTRL       0xE0   /* modifiers: 0xE0 + bit index */
#define KBM_KEY_MOUSE1      0xF0   /* left   */
#define KBM_KEY_MOUSE2      0xF1   /* right  */
#define KBM_KEY_MOUSE3      0xF2   /* middle */
#define KBM_KEY_MOUSE4      0xF3
#define KBM_KEY_MOUSE5      0xF4
#define KBM_KEY_WHEEL_UP    0xF8
#define KBM_KEY_WHEEL_DOWN  0xF9

#define KBM_KEY_F8   0x41
#define KBM_KEY_F9   0x42
#define KBM_KEY_F10  0x43
#define KBM_KEY_F11  0x44
#define KBM_KEY_F12  0x45

/* "W", "LSHIFT", "MOUSE1", "WHEEL_UP"... NULL if unnamed. */
const char *kbm_key_name(uint8_t code);
/* Case-insensitive; returns 0 (NONE) for "" / "NONE", -1 if unknown. */
int kbm_key_from_name(const char *name);
/* Keys the user may not bind: F8 (manual start), F9 (pointer mode), F11 (settings page), F12 (stop). */
int kbm_key_reserved(uint8_t code);
