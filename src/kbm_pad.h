/* SPDX-License-Identifier: GPL-3.0-or-later
 * Wired Xbox controllers -> the same virtual DualSense as the keyboard/mouse.
 *  - Xbox One / Series / Elite and compatible pads speak GIP (interface class
 *    0xFF, subclass 0x47, protocol 0xD0). The handshake and report parsing come
 *    from Ghostcontrol's controller_xbox.c (hardware-confirmed on PS5 with an
 *    Xbox One S 045e:02ea); the bridge only finds and drives the interface.
 *  - Xbox 360 wired pads speak XInput (0xFF/0x5D/0x01): plain 20-byte reports,
 *    no handshake. Parsed here.
 * Pure C, host-tested.
 */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "kbm_engine.h"

#define KBM_PAD_NONE 0
#define KBM_PAD_GIP  1     /* Xbox One / Series */
#define KBM_PAD_X360 2     /* Xbox 360 */

typedef struct {
    int type;              /* KBM_PAD_* */
    int iface;
    int ep_in, ep_out;     /* -1 if absent */
} KbmPadLayout;

typedef struct {
    int connected;
    uint32_t buttons;      /* KBM_BTN_* (same bits as the DualSense) */
    uint8_t lx, ly, rx, ry, l2, r2;
} KbmPadState;

/* Raw USB configuration descriptor -> first Xbox controller interface. 1 if found. */
int  kbm_pad_find(const uint8_t *desc, size_t len, KbmPadLayout *out);
const char *kbm_pad_name(int type);   /* "Xbox One/Series" | "Xbox 360" | "" */
/* Xbox 360 input report (type 0x00, 20 bytes). 1 = state updated, 0 = other message. */
int  kbm_pad_parse_x360(const uint8_t *buf, size_t len, KbmPadState *st);
/* Put the controller on top of the keyboard/mouse frame: buttons are combined,
 * a stick the controller is moving wins over the keyboard/mouse, triggers take the max. */
void kbm_pad_merge(KbmFrame *f, const KbmPadState *p);
