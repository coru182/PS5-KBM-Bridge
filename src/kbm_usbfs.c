/* SPDX-License-Identifier: GPL-3.0-or-later
 * PS5 KBM Bridge v0.6 — see kbm_usbfs.h for the USB_FS_COMPLETE semantics.
 */
#include "kbm_usbfs.h"

#include <errno.h>
#include <string.h>

/* ---------- small percentile helper (no malloc, no libm) ---------- */

void kbm_samples_add(KbmSamples *s, uint32_t us) {
    if (s->n < KBM_SAMPLES) s->v[s->n++] = us;
    else s->v[(us ^ s->n++) % KBM_SAMPLES] = us; /* cheap reservoir-ish overwrite */
}

static void isort(uint32_t *a, uint32_t n) {
    for (uint32_t i = 1; i < n; ++i) {
        uint32_t x = a[i], j = i;
        while (j > 0 && a[j - 1] > x) { a[j] = a[j - 1]; --j; }
        a[j] = x;
    }
}

void kbm_samples_take(KbmSamples *s, uint32_t *n, uint32_t *p50, uint32_t *p95,
                      uint32_t *p99, uint32_t *max) {
    uint32_t cnt = s->n < KBM_SAMPLES ? s->n : KBM_SAMPLES;
    *n = s->n;
    if (cnt == 0) { *p50 = *p95 = *p99 = *max = 0; s->n = 0; return; }
    isort(s->v, cnt);
    *p50 = s->v[(cnt - 1) * 50 / 100];
    *p95 = s->v[(cnt - 1) * 95 / 100];
    *p99 = s->v[(cnt - 1) * 99 / 100];
    *max = s->v[cnt - 1];
    s->n = 0;
}

/* ---------- USB_FS ---------- */

int kbm_usbfs_init(KbmUsbFs *u, int fd, kbm_ioctl_fn ioc,
                   kbm_report_fn on_report, void *user) {
    struct usb_fs_init init;
    memset(u, 0, sizeof(*u));
    u->fd = fd;
    u->ioc = ioc;
    u->on_report = on_report;
    u->user = user;
    memset(&init, 0, sizeof(init));
    init.pEndpoints = u->fs;
    init.ep_index_max = KBM_MAX_EP;
    if (u->ioc(fd, USB_FS_INIT, &init) != 0) return -1;
    u->init_ok = 1;
    return 0;
}

int kbm_usbfs_open_ep(KbmUsbFs *u, int idx, uint8_t ep_addr, int kind) {
    struct usb_fs_open op;
    if (idx < 0 || idx >= KBM_MAX_EP) { errno = EINVAL; return -1; }
    KbmEp *e = &u->ep[idx];
    memset(e, 0, sizeof(*e));
    e->bufs[0] = e->buffer;
    e->ep_addr = ep_addr;
    e->kind = (uint8_t)kind;
    memset(&op, 0, sizeof(op));
    op.ep_index = (uint8_t)idx;
    op.ep_no = ep_addr;
    op.max_bufsize = KBM_MAX_REPORT;
    op.max_frames = 1;
    if (u->ioc(u->fd, USB_FS_OPEN, &op) != 0) return -1;
    e->open = 1;
    e->max_packet = op.max_packet_length;
    /* Request EXACTLY one packet. Asking for more than wMaxPacketSize makes the
     * host controller keep collecting full packets until the buffer fills
     * (that is very likely why v0.3 saw "length=64": eight 8-byte keyboard
     * reports glued together, i.e. 8 key events of latency per completion). */
    uint32_t req = e->max_packet ? e->max_packet : 8u;
    if (op.max_bufsize && req > op.max_bufsize) req = op.max_bufsize;
    if (req > KBM_MAX_REPORT) req = KBM_MAX_REPORT;
    e->req_len = req;
    if (idx + 1 > u->n_ep) u->n_ep = idx + 1;
    return 0;
}

int kbm_usbfs_arm(KbmUsbFs *u, int idx) {
    KbmEp *e = &u->ep[idx];
    struct usb_fs_endpoint *f = &u->fs[idx];
    struct usb_fs_start st;
    if (!e->open) return -1;
    e->lens[0] = e->req_len;
    f->ppBuffer = e->bufs;
    f->pLength = e->lens;
    f->nFrames = 1;
    f->aFrames = 0;
    f->status = 0;
    /* No timeout: an interrupt-IN endpoint simply NAKs until the device has a
     * report. A 30 ms timeout (v0.5) only created ~27 useless completions/s
     * per idle endpoint and re-arm churn. Shutdown uses USB_FS_STOP instead. */
    f->timeout = USB_FS_TIMEOUT_NONE;
    f->flags = USB_FS_FLAG_SINGLE_SHORT_OK | USB_FS_FLAG_MULTI_SHORT_OK;
    if (e->clear_stall_next) f->flags |= USB_FS_FLAG_CLEAR_STALL;
    memset(&st, 0, sizeof(st));
    st.ep_index = (uint8_t)idx;
    if (u->ioc(u->fd, USB_FS_START, &st) == 0) {
        e->in_flight = 1;
        e->clear_stall_next = 0;
        e->consecutive_start_err = 0;
        ++e->win.start_ok;
        return 1;
    }
    int err = errno;
    if (err == EBUSY) {
        /* Kernel: transfer still pending. It WILL complete through the queue.
         * Never STOP here (v0.5 did, cancelling live transfers). */
        e->in_flight = 1;
        ++e->win.start_ebusy;
        return 1;
    }
    ++e->win.start_err;
    e->win.last_start_errno = err;
    if (err == ENXIO || err == ENODEV) return -1;
    if (++e->consecutive_start_err >= 50) return -1;
    return 0;
}

static void count_len(KbmEpStats *s, uint32_t len) {
    if (len == 0) ++s->len0;
    else if (len <= 2) ++s->len1_2;
    else if (len <= 4) ++s->len3_4;
    else if (len <= 7) ++s->len5_7;
    else if (len == 8) ++s->len8;
    else ++s->len9p;
}

static void handle_completion(KbmUsbFs *u, int idx, uint64_t now_ns) {
    KbmEp *e = &u->ep[idx];
    struct usb_fs_endpoint *f = &u->fs[idx];
    e->in_flight = 0;
    int status = f->status;
    if (status == 0) {
        ++e->win.st_ok;
        uint32_t len = f->aFrames > 0 ? e->lens[0] : 0;
        if (len > e->req_len) len = e->req_len; /* paranoia: never trust > buffer */
        count_len(&e->win, len);
        if (len == 0) return;
        ++e->win.reports;
        ++e->total_reports;
        if (e->last_report_ns && now_ns > e->last_report_ns) {
            uint64_t gap_us = (now_ns - e->last_report_ns) / 1000u;
            if (gap_us < 1000000u) kbm_samples_add(&e->gaps, (uint32_t)gap_us);
        }
        e->last_report_ns = now_ns;
        if (u->on_report) {
            int r = u->on_report(u->user, e->kind, e->buffer, len, now_ns);
            if (r < 0) ++e->win.rejected;
            else if (r > 0) ++e->win.active;
        }
    } else if (status == KBM_USB_ERR_CANCELLED) {
        ++e->win.st_cancelled;
    } else if (status == KBM_USB_ERR_STALLED) {
        ++e->win.st_stalled;
        e->clear_stall_next = 1;
    } else if (status == KBM_USB_ERR_TIMEOUT) {
        ++e->win.st_timeout;
    } else {
        ++e->win.st_other;
        e->win.last_other_status = status;
    }
}

int kbm_usbfs_service(KbmUsbFs *u, uint64_t now_ns, int rearm) {
    int handled = 0;
    for (int guard = 0; guard < 256; ++guard) {
        struct usb_fs_complete c;
        memset(&c, 0, sizeof(c));
        if (u->ioc(u->fd, USB_FS_COMPLETE, &c) != 0) {
            int err = errno;
            if (err == EBUSY) break;            /* queue empty */
            if (err == EINTR) continue;
            if (err == ENXIO || err == ENODEV || err == ENOTTY) return -1;
            ++u->complete_err;
            u->last_complete_errno = err;
            break;
        }
        ++handled;
        ++u->completions;
        /* The kernel tells us WHICH endpoint finished. Trust only that. */
        if (c.ep_index >= u->n_ep || !u->ep[c.ep_index].open) {
            ++u->bad_index;
            continue;
        }
        handle_completion(u, c.ep_index, now_ns);
    }
    if ((uint32_t)handled > u->max_batch) u->max_batch = (uint32_t)handled;
    if (rearm) {
        for (int i = 0; i < u->n_ep; ++i) {
            if (u->ep[i].open && !u->ep[i].in_flight) {
                if (kbm_usbfs_arm(u, i) < 0) return -1;
            }
        }
    }
    return handled;
}

void kbm_usbfs_teardown(KbmUsbFs *u) {
    for (int i = 0; i < u->n_ep; ++i) {
        if (u->ep[i].open && u->ep[i].in_flight) {
            struct usb_fs_stop sp;
            memset(&sp, 0, sizeof(sp));
            sp.ep_index = (uint8_t)i;
            (void)u->ioc(u->fd, USB_FS_STOP, &sp);
        }
    }
    u->on_report = NULL;               /* no input after shutdown began */
    (void)kbm_usbfs_service(u, 0, 0); /* swallow the cancel events */
    for (int i = 0; i < u->n_ep; ++i) {
        if (u->ep[i].open) {
            struct usb_fs_close cl;
            memset(&cl, 0, sizeof(cl));
            cl.ep_index = (uint8_t)i;
            (void)u->ioc(u->fd, USB_FS_CLOSE, &cl);
            u->ep[i].open = 0;
            u->ep[i].in_flight = 0;
        }
    }
    if (u->init_ok) {
        struct usb_fs_uninit un;
        memset(&un, 0, sizeof(un));
        (void)u->ioc(u->fd, USB_FS_UNINIT, &un);
        u->init_ok = 0;
    }
}
