/* SPDX-License-Identifier: GPL-3.0-or-later
 * Host-only stand-in for <dev/usb/usb_ioctl.h>. Field names/semantics follow
 * FreeBSD stable/11; numeric ioctl values are arbitrary (only used by fake_ugen).
 */
#pragma once
#include <stdint.h>

struct usb_fs_endpoint {
    void   **ppBuffer;
    uint32_t *pLength;
    uint32_t nFrames;
    uint32_t aFrames;
    uint16_t flags;
#define USB_FS_FLAG_SINGLE_SHORT_OK 0x0001
#define USB_FS_FLAG_MULTI_SHORT_OK  0x0002
#define USB_FS_FLAG_FORCE_SHORT     0x0004
#define USB_FS_FLAG_CLEAR_STALL     0x0008
    uint16_t timeout;
    uint16_t isoc_time_complete;
#define USB_FS_TIMEOUT_NONE 0
    int status;
};
struct usb_fs_init   { struct usb_fs_endpoint *pEndpoints; uint8_t ep_index_max; };
struct usb_fs_uninit { uint8_t dummy; };
struct usb_fs_open   { uint32_t max_bufsize; uint32_t max_frames; uint16_t max_packet_length;
                       uint8_t dev_index; uint8_t ep_index; uint8_t ep_no; };
struct usb_fs_close    { uint8_t ep_index; };
struct usb_fs_start    { uint8_t ep_index; };
struct usb_fs_stop     { uint8_t ep_index; };
struct usb_fs_complete { uint8_t ep_index; };

#define USB_FS_START    0x1001UL
#define USB_FS_STOP     0x1002UL
#define USB_FS_COMPLETE 0x1003UL
#define USB_FS_INIT     0x1004UL
#define USB_FS_UNINIT   0x1005UL
#define USB_FS_OPEN     0x1006UL
#define USB_FS_CLOSE    0x1007UL
