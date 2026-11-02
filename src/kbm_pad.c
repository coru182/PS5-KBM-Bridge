/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "kbm_pad.h"
#include <string.h>

int kbm_pad_find(const uint8_t *d, size_t n, KbmPadLayout *out) {
    if (!out) return 0;
    out->type = KBM_PAD_NONE; out->iface = out->ep_in = out->ep_out = -1;
    if (!d || n < 9) return 0;
    int cur = KBM_PAD_NONE, iface = -1;
    for (size_t i = 0; i + 2 <= n;) {
        uint8_t len = d[i], type = d[i + 1];
        if (len < 2 || (size_t)len > n - i) return 0;
        if (type == 4 && len >= 9) {                      /* interface */
            if (out->type != KBM_PAD_NONE) break;         /* first match only */
            cur = KBM_PAD_NONE; iface = -1;
            if (d[i + 3] == 0 && d[i + 5] == 0xFF) {
                if (d[i + 6] == 0x47 && d[i + 7] == 0xD0) cur = KBM_PAD_GIP;
                else if (d[i + 6] == 0x5D && d[i + 7] == 0x01) cur = KBM_PAD_X360;
                if (cur) iface = d[i + 2];
            }
        } else if (type == 5 && len >= 7 && cur && (d[i + 3] & 3) == 3) {   /* interrupt endpoint */
            if ((d[i + 2] & 0x80) && out->ep_in < 0) out->ep_in = d[i + 2];
            else if (!(d[i + 2] & 0x80) && out->ep_out < 0) out->ep_out = d[i + 2];
            if (out->ep_in >= 0) { out->type = cur; out->iface = iface; }
        }
        i += len;
    }
    if (out->type == KBM_PAD_NONE) { out->iface = out->ep_in = out->ep_out = -1; return 0; }
    return 1;
}

const char *kbm_pad_name(int type) {
    return type == KBM_PAD_GIP ? "Xbox One/Series" : type == KBM_PAD_X360 ? "Xbox 360" : "";
}

/* int16 stick -> 0..255 with a small dead zone, rescaled so movement starts smoothly */
static uint8_t axis(int16_t v, int invert) {
    const int dz = 4000;
    int x = v, mag = x < 0 ? -x : x;
    if (mag <= dz) return 128;
    if (mag > 32767) mag = 32767;
    int out = (mag - dz) * 127 / (32767 - dz);
    int s = (x < 0) != (invert != 0) ? -1 : 1;
    int r = 128 + s * out;
    return (uint8_t)(r < 0 ? 0 : r > 255 ? 255 : r);
}

int kbm_pad_parse_x360(const uint8_t *b, size_t len, KbmPadState *st) {
    if (!b || len < 20 || b[0] != 0x00 || b[1] < 0x14) return 0;
    uint32_t btn = 0;
    if (b[2] & 0x01) btn |= KBM_BTN_UP;
    if (b[2] & 0x02) btn |= KBM_BTN_DOWN;
    if (b[2] & 0x04) btn |= KBM_BTN_LEFT;
    if (b[2] & 0x08) btn |= KBM_BTN_RIGHT;
    if (b[2] & 0x10) btn |= KBM_BTN_OPTIONS;     /* Start */
    if (b[2] & 0x20) btn |= KBM_BTN_CREATE;      /* Back -> Create, like View on Xbox One (Ghostcontrol) */
    if (b[2] & 0x40) btn |= KBM_BTN_L3;
    if (b[2] & 0x80) btn |= KBM_BTN_R3;
    if (b[3] & 0x01) btn |= KBM_BTN_L1;
    if (b[3] & 0x02) btn |= KBM_BTN_R1;
    if (b[3] & 0x04) btn |= KBM_BTN_PS;          /* Guide */
    if (b[3] & 0x10) btn |= KBM_BTN_CROSS;       /* A */
    if (b[3] & 0x20) btn |= KBM_BTN_CIRCLE;      /* B */
    if (b[3] & 0x40) btn |= KBM_BTN_SQUARE;      /* X */
    if (b[3] & 0x80) btn |= KBM_BTN_TRIANGLE;    /* Y */
    st->l2 = b[4]; st->r2 = b[5];
    if (st->l2 > 16) btn |= KBM_BTN_L2;
    if (st->r2 > 16) btn |= KBM_BTN_R2;
    st->lx = axis((int16_t)(b[6] | (b[7] << 8)), 0);
    st->ly = axis((int16_t)(b[8] | (b[9] << 8)), 1);     /* XInput: up is positive */
    st->rx = axis((int16_t)(b[10] | (b[11] << 8)), 0);
    st->ry = axis((int16_t)(b[12] | (b[13] << 8)), 1);
    st->buttons = btn;
    st->connected = 1;
    return 1;
}

static int moved(uint8_t x, uint8_t y) {
    return x < 128 - 12 || x > 128 + 12 || y < 128 - 12 || y > 128 + 12;
}

void kbm_pad_merge(KbmFrame *f, const KbmPadState *p) {
    if (!p || !p->connected) return;
    f->buttons |= p->buttons;
    if (moved(p->lx, p->ly)) { f->lx = p->lx; f->ly = p->ly; }
    if (moved(p->rx, p->ry)) { f->rx = p->rx; f->ry = p->ry; }
    if (p->l2 > f->l2) f->l2 = p->l2;
    if (p->r2 > f->r2) f->r2 = p->r2;
}
