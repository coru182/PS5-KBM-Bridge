/* SPDX-License-Identifier: GPL-3.0-or-later
 * PS5 KBM Bridge v0.6 — FreeBSD ugen USB_FS reader for several interrupt-IN
 * endpoints on ONE device file descriptor.
 *
 * KEY FACT (verified against FreeBSD stable/11 sys/dev/usb/usb_generic.c):
 * USB_FS_COMPLETE does NOT take an endpoint as input. Each fd has ONE queue
 * of finished transfers; the ioctl pops the oldest one and WRITES the endpoint
 * index into usb_fs_complete.ep_index, then copies status/length/data out for
 * THAT endpoint. Asking "is endpoint i done?" is not something the API can do.
 * v0.5 assumed it could, which misrouted completions between keyboard and mouse.
 */
#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef KBM_HOST_TEST
#include "fake_usb_ioctl.h"
#else
#include <sys/types.h>
#include <sys/ioctl.h>
#include <dev/usb/usb.h>
#include <dev/usb/usb_ioctl.h>
#endif

#define KBM_KIND_KEYBOARD 1
#define KBM_KIND_MOUSE    2

#define KBM_MAX_EP      2
#define KBM_MAX_REPORT  64
#define KBM_SAMPLES     256

/* usb_error_t values from FreeBSD dev/usb/usbdi.h (checked in PS5 SDK headers). */
#define KBM_USB_ERR_CANCELLED 5
#define KBM_USB_ERR_TIMEOUT   20
#define KBM_USB_ERR_STALLED   22

typedef struct {
    uint32_t n;
    uint32_t v[KBM_SAMPLES]; /* microseconds */
} KbmSamples;

void kbm_samples_add(KbmSamples *s, uint32_t us);
/* Computes percentiles of the collected samples, then clears them. */
void kbm_samples_take(KbmSamples *s, uint32_t *n, uint32_t *p50, uint32_t *p95,
                      uint32_t *p99, uint32_t *max);

typedef struct {
    uint32_t start_ok, start_ebusy, start_err;
    int      last_start_errno;
    uint32_t st_ok, st_cancelled, st_stalled, st_timeout, st_other;
    int      last_other_status;
    uint32_t len0, len1_2, len3_4, len5_7, len8, len9p;
    uint32_t reports;   /* status ok with len > 0 */
    uint32_t active;    /* reports with any key / motion / button */
    uint32_t rejected;  /* reports the engine refused */
} KbmEpStats;

typedef struct {
    uint8_t  buffer[KBM_MAX_REPORT];
    void    *bufs[1];
    uint32_t lens[1];
    uint8_t  ep_addr;
    uint8_t  kind;
    uint16_t max_packet;
    uint32_t req_len;
    int      open;
    int      in_flight;
    int      clear_stall_next;
    int      consecutive_start_err;
    uint64_t last_report_ns;
    uint64_t total_reports;
    KbmEpStats win;     /* reset every stats window */
    KbmSamples gaps;    /* inter-report interval while the stream is active */
} KbmEp;

typedef int  (*kbm_ioctl_fn)(int fd, unsigned long req, void *arg);
/* Called for every successful report, routed by the kernel's ep_index. */
typedef int  (*kbm_report_fn)(void *user, int kind, const uint8_t *buf,
                              uint32_t len, uint64_t t_ns);

typedef struct {
    int fd;
    kbm_ioctl_fn ioc;
    int n_ep;
    int init_ok;
    struct usb_fs_endpoint fs[KBM_MAX_EP];
    KbmEp ep[KBM_MAX_EP];
    kbm_report_fn on_report;
    void *user;
    /* window stats, I/O level */
    uint32_t completions, bad_index, complete_err, max_batch;
    int      last_complete_errno;
} KbmUsbFs;

/* All return 0 on success, -1 on failure (errno preserved). */
int  kbm_usbfs_init(KbmUsbFs *u, int fd, kbm_ioctl_fn ioc,
                    kbm_report_fn on_report, void *user);
int  kbm_usbfs_open_ep(KbmUsbFs *u, int idx, uint8_t ep_addr, int kind);
/* 1 armed (or kernel says already pending), 0 transient failure, -1 fatal. */
int  kbm_usbfs_arm(KbmUsbFs *u, int idx);
/* Drain EVERY queued completion, dispatch by returned ep_index, then re-arm
 * idle endpoints if rearm != 0. Returns completions handled, -1 on device loss. */
int  kbm_usbfs_service(KbmUsbFs *u, uint64_t now_ns, int rearm);
void kbm_usbfs_teardown(KbmUsbFs *u);
