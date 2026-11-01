/* SPDX-License-Identifier: GPL-3.0-or-later
 * Host test: a fake ugen kernel that models FreeBSD's USB_FS semantics
 * (ONE completion FIFO per fd, USB_FS_COMPLETE pops it and RETURNS ep_index,
 * copy-out goes to THAT endpoint's buffers, START on a pending xfer = EBUSY,
 * STOP of a pending xfer queues a CANCELLED completion).
 *
 * Scenario (same for both readers): mouse streams a report every 8 ms for
 * 3 s, user holds W for 2 s, then taps E. We run:
 *   1) a faithful re-implementation of the v0.5 reader (per-endpoint
 *      "poll COMPLETE for my ep", 30 ms timeout, 1 ms round-robin)
 *   2) the v0.6 kbm_usbfs reader
 * and compare what reaches the mapping engine.
 *
 * This is a MODEL. It shows the mechanism; real-console numbers will differ.
 */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "../src/kbm_usbfs.h"
#include "../src/kbm_engine.h"

/* ------------------------------------------------------------------ fake kernel */
#define QN 8192
typedef struct { uint8_t d[64]; uint32_t len; } Rep;
typedef struct {
    int open, pending;
    uint16_t mps; uint32_t req; uint16_t timeout; uint32_t elapsed;
    Rep q[QN]; int qh, qt;
    int st; uint32_t frames, len; uint8_t data[64];
} FEp;
static struct {
    struct usb_fs_endpoint *uep; int max;
    FEp ep[KBM_MAX_EP];
    int used[QN]; int uh, ut;
    uint32_t now_ms;
} K;

static void k_reset(void) { memset(&K, 0, sizeof(K)); }
static void k_complete(int i, int st, const Rep *r) {
    FEp *e = &K.ep[i];
    e->pending = 0; e->st = st;
    e->frames = r ? 1 : 0; e->len = r ? r->len : 0;
    if (r) memcpy(e->data, r->d, r->len);
    K.used[K.ut++ % QN] = i;
}
static int fake_ioctl(int fd, unsigned long req, void *arg) {
    (void)fd;
    switch (req) {
    case USB_FS_INIT: { struct usb_fs_init *a = arg; K.uep = a->pEndpoints; K.max = a->ep_index_max; return 0; }
    case USB_FS_UNINIT: return 0;
    case USB_FS_OPEN: { struct usb_fs_open *a = arg;
        if (a->ep_index >= K.max) { errno = EINVAL; return -1; }
        K.ep[a->ep_index].open = 1;
        K.ep[a->ep_index].mps = (a->ep_no == 0x81) ? 8 : 8; /* Logitech: both 8 */
        a->max_packet_length = K.ep[a->ep_index].mps;
        return 0; }
    case USB_FS_CLOSE: { struct usb_fs_close *a = arg; K.ep[a->ep_index].open = 0; return 0; }
    case USB_FS_START: { struct usb_fs_start *a = arg; FEp *e = &K.ep[a->ep_index];
        if (e->pending) { errno = EBUSY; return -1; }
        struct usb_fs_endpoint *u = &K.uep[a->ep_index];
        e->req = u->pLength[0]; e->timeout = u->timeout; e->elapsed = 0; e->pending = 1;
        return 0; }
    case USB_FS_STOP: { struct usb_fs_stop *a = arg; FEp *e = &K.ep[a->ep_index];
        if (e->pending) k_complete(a->ep_index, KBM_USB_ERR_CANCELLED, NULL);
        return 0; }
    case USB_FS_COMPLETE: { struct usb_fs_complete *c = arg;
        if (K.uh == K.ut) { errno = EBUSY; return -1; }
        int i = K.used[K.uh++ % QN];
        c->ep_index = (uint8_t)i;          /* <- the kernel decides */
        struct usb_fs_endpoint *u = &K.uep[i];
        FEp *e = &K.ep[i];
        u->status = e->st; u->aFrames = e->frames;
        if (e->frames) { memcpy(u->ppBuffer[0], e->data, e->len); u->pLength[0] = e->len; }
        return 0; }
    }
    errno = ENOTTY; return -1;
}
static void dev_push(int i, const uint8_t *d, uint32_t len) {
    FEp *e = &K.ep[i]; Rep *r = &e->q[e->qt++ % QN]; memcpy(r->d, d, len); r->len = len;
}
/* one millisecond of bus time: a pending transfer gets the device's next report */
static void k_tick(void) {
    K.now_ms++;
    for (int i = 0; i < KBM_MAX_EP; ++i) {
        FEp *e = &K.ep[i];
        if (!e->pending) continue;
        if (e->qh != e->qt) {
            Rep *r = &e->q[e->qh++ % QN];
            if (r->len > e->req) k_complete(i, 18 /* overrun-ish */, NULL);
            else k_complete(i, 0, r);
        } else if (e->timeout && ++e->elapsed >= e->timeout) {
            k_complete(i, KBM_USB_ERR_TIMEOUT, NULL);
        }
    }
}

/* ------------------------------------------------------------------ scenario */
#define T_END 3200
static int mouse_sent;
static void scenario_step(uint32_t t) {
    static const uint8_t kb_up[8] = {0};
    uint8_t w[8] = {0,0,0x1a,0,0,0,0,0}, e[8] = {0,0,0x08,0,0,0,0,0};
    if (t == 100) dev_push(0, w, 8);
    if (t == 2100) dev_push(0, kb_up, 8);
    if (t == 2500) dev_push(0, e, 8);
    if (t == 2560) dev_push(0, kb_up, 8);
    if (t >= 50 && t < 3050 && (t % 8) == 0) {
        uint8_t m[4] = {0, 3, (uint8_t)-1, 0};
        dev_push(1, m, 4); ++mouse_sent;
    }
}

typedef struct {
    KbmEngine eng;
    int kb_reports, mouse_reports, kb_phantom_release;
    int w_samples, w_held_ok;       /* engine says W is down while the user holds it */
    int e_seen;
} Sink;
static int w_down(const KbmEngine *e) { return kbm_is_down(e, 0x1a); }
static void sink_kb(Sink *s, const uint8_t *b, uint32_t n, uint32_t t) {
    if (!kbm_keyboard(&s->eng, 0, b, n)) return;
    ++s->kb_reports;
    int any = 0; for (int i=2;i<8;++i) if (b[i]) any = 1;
    if (!any && t > 110 && t < 2100) ++s->kb_phantom_release;
    for (int i=2;i<8;++i) if (b[i]==0x08) s->e_seen = 1;
}
static void sink_sample(Sink *s, uint32_t t) {
    if (t >= 150 && t < 2080 && (t % 10) == 0) { ++s->w_samples; if (w_down(&s->eng)) ++s->w_held_ok; }
}

/* ------------------------------------------------------------------ v0.5 reader (faithful copy of logic) */
typedef struct { uint8_t buffer[64]; void *bufs[1]; uint32_t lens[1]; uint8_t idx, kind; int in_flight; } LegacyIn;
static int legacy_arm(struct usb_fs_endpoint *ep, LegacyIn *u) {
    memset(u->buffer, 0, sizeof(u->buffer));
    u->lens[0] = (u->kind == 1) ? 8u : 4u;
    ep->ppBuffer = u->bufs; ep->pLength = u->lens; ep->nFrames = 1; ep->aFrames = 0;
    ep->status = 0; ep->timeout = 30;
    ep->flags = USB_FS_FLAG_SINGLE_SHORT_OK | USB_FS_FLAG_MULTI_SHORT_OK;
    struct usb_fs_start st = { .ep_index = u->idx };
    if (fake_ioctl(0, USB_FS_START, &st) == 0) { u->in_flight = 1; return 1; }
    if (errno == EBUSY) { struct usb_fs_stop sp = { .ep_index = u->idx }; fake_ioctl(0, USB_FS_STOP, &sp); return 0; }
    return -1;
}
static int legacy_poll(struct usb_fs_endpoint *ep, LegacyIn *u, uint32_t *len) {
    struct usb_fs_complete c = { .ep_index = u->idx };   /* v0.5 bug: input is ignored */
    if (fake_ioctl(0, USB_FS_COMPLETE, &c) != 0) return errno == EBUSY ? 0 : 2;
    u->in_flight = 0;                                    /* ...and assumes it was u->idx */
    if (ep->status != 0) return 2;
    uint32_t max = (u->kind == 1) ? 8u : 4u;
    if (u->lens[0] == 0 || u->lens[0] > max) return 2;
    *len = u->lens[0]; return 1;
}
static void run_legacy(Sink *s) {
    k_reset(); mouse_sent = 0; memset(s, 0, sizeof(*s)); kbm_init(&s->eng);
    struct usb_fs_endpoint eps[2]; memset(eps, 0, sizeof(eps));
    struct usb_fs_init in = { eps, 2 }; fake_ioctl(0, USB_FS_INIT, &in);
    LegacyIn L[2];
    for (int i = 0; i < 2; ++i) {
        struct usb_fs_open op = { .ep_index = (uint8_t)i, .ep_no = (uint8_t)(0x81 + i), .max_bufsize = 64, .max_frames = 1 };
        fake_ioctl(0, USB_FS_OPEN, &op);
        memset(&L[i], 0, sizeof(L[i])); L[i].idx = (uint8_t)i; L[i].kind = (uint8_t)(i + 1); L[i].bufs[0] = L[i].buffer;
        legacy_arm(&eps[i], &L[i]);
    }
    for (uint32_t t = 0; t < T_END; ++t) {
        for (int i = 0; i < 2; ++i) {
            if (!L[i].in_flight) { legacy_arm(&eps[i], &L[i]); continue; }
            uint32_t len = 0;
            if (legacy_poll(&eps[i], &L[i], &len) != 1) continue;
            if (i == 0) sink_kb(s, L[i].buffer, len, t);
            else if (kbm_mouse(&s->eng, 0, L[i].buffer, len, (uint64_t)t * 1000000ull)) ++s->mouse_reports;
        }
        sink_sample(s, t);
        scenario_step(t); k_tick();     /* usleep(1000) */
    }
}

/* ------------------------------------------------------------------ v0.6 reader */
static uint32_t g_t;
static int v06_report(void *user, int kind, const uint8_t *b, uint32_t n, uint64_t t_ns) {
    Sink *s = user;
    if (kind == KBM_KIND_KEYBOARD) { sink_kb(s, b, n, g_t); return 1; }
    if (kbm_mouse(&s->eng, 0, b, n, t_ns)) { ++s->mouse_reports; return 1; }
    return -1;
}
static void run_v06(Sink *s, KbmUsbFs *u) {
    k_reset(); mouse_sent = 0; memset(s, 0, sizeof(*s)); kbm_init(&s->eng);
    assert(kbm_usbfs_init(u, 0, fake_ioctl, v06_report, s) == 0);
    assert(kbm_usbfs_open_ep(u, 0, 0x81, KBM_KIND_KEYBOARD) == 0);
    assert(kbm_usbfs_open_ep(u, 1, 0x82, KBM_KIND_MOUSE) == 0);
    assert(u->ep[0].req_len == 8 && u->ep[1].req_len == 8); /* = wMaxPacketSize */
    assert(kbm_usbfs_arm(u, 0) == 1 && kbm_usbfs_arm(u, 1) == 1);
    for (g_t = 0; g_t < T_END; ++g_t) {
        assert(kbm_usbfs_service(u, (uint64_t)g_t * 1000000ull, 1) >= 0);
        sink_sample(s, g_t);
        scenario_step(g_t); k_tick();  /* poll(…, 2 ms) worst case ≈ 1–2 ms */
    }
    kbm_usbfs_teardown(u);
}

int main(void) {
    Sink a, b; static KbmUsbFs u;
    run_legacy(&a);
    int sent = mouse_sent;
    printf("v0.5 model: mouse %d/%d delivered, kb reports=%d, phantom releases while W held=%d, "
           "W visible %d/%d samples, E tap seen=%d\n",
           a.mouse_reports, sent, a.kb_reports, a.kb_phantom_release, a.w_held_ok, a.w_samples, a.e_seen);
    run_v06(&b, &u);
    printf("v0.6 model: mouse %d/%d delivered, kb reports=%d, phantom releases while W held=%d, "
           "W visible %d/%d samples, E tap seen=%d, bad_index=%u\n",
           b.mouse_reports, mouse_sent, b.kb_reports, b.kb_phantom_release, b.w_held_ok, b.w_samples,
           b.e_seen, u.bad_index);

    /* v0.6 must be exact */
    assert(b.mouse_reports == mouse_sent);
    assert(b.kb_reports == 4);
    assert(b.kb_phantom_release == 0);
    assert(b.w_held_ok == b.w_samples);
    assert(b.e_seen);
    assert(u.bad_index == 0);
    assert(u.ep[0].win.st_timeout == 0 && u.ep[1].win.st_timeout == 0); /* no timeout churn */
    /* the v0.5 logic must show the defect in the same model */
    assert(a.mouse_reports < sent / 2 || a.kb_phantom_release > 0 || a.w_held_ok < a.w_samples);

    /* percentile helper */
    KbmSamples sm = {0}; uint32_t n,p50,p95,p99,mx;
    for (uint32_t i = 1; i <= 100; ++i) kbm_samples_add(&sm, i);
    kbm_samples_take(&sm, &n, &p50, &p95, &p99, &mx);
    assert(n == 100 && p50 == 50 && p95 == 95 && p99 == 99 && mx == 100 && sm.n == 0);

    puts("PASS: USB_FS completion routing (fake FreeBSD ugen model), v0.5 defect reproduced, percentiles");
    return 0;
}
