/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "hid_boot.h"

/* USB bDescriptorType 0x04 interface, 0x05 endpoint.
 * Only accept boot-protocol HID (class=3, subclass=1, proto=1/2), alt=0.
 * This intentionally avoids interpreting vendor-specific reports as boot HID.
 */
int hid_boot_find(const uint8_t *d, size_t n, HidBootLayout *out) {
    if (!out) return 0;
    out->keyboard_iface = out->keyboard_ep = -1;
    out->mouse_iface = out->mouse_ep = -1;
    if (!d || n < 9) return 0;
    int iface = -1, protocol = 0;
    for (size_t i = 0; i + 2 <= n;) {
        uint8_t len = d[i], type = d[i + 1];
        if (len < 2 || (size_t)len > n-i) return 0;
        if (type == 4) {
            protocol = 0;
            iface = -1;
            if (len >= 9 && d[i+3] == 0 && d[i+5] == 3 && d[i+6] == 1 &&
                (d[i+7] == 1 || d[i+7] == 2)) {
                iface = d[i+2];
                protocol = d[i+7];
            }
        } else if (type == 5 && len >= 7 && iface >= 0 &&
                   (d[i+2] & 0x80) && ((d[i+3] & 3) == 3)) {
            if (protocol == 1 && out->keyboard_ep < 0) {
                out->keyboard_iface = iface; out->keyboard_ep = d[i+2];
            } else if (protocol == 2 && out->mouse_ep < 0) {
                out->mouse_iface = iface; out->mouse_ep = d[i+2];
            }
        }
        i += len;
    }
    return (out->keyboard_ep >= 0 || out->mouse_ep >= 0);
}
