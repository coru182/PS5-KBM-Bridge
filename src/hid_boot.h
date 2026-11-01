/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <stdint.h>
#include <stddef.h>

typedef struct {
    int keyboard_iface, keyboard_ep; /* -1 if absent */
    int mouse_iface, mouse_ep;
} HidBootLayout;

/* Parse raw USB config descriptor. Does not depend on PS5 or host USB APIs. */
int hid_boot_find(const uint8_t *raw, size_t len, HidBootLayout *out);
