/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/kbm_engine.h"
#include "../src/kbm_config.h"
#include "../src/kbm_keys.h"
#include "../src/hid_boot.h"

#define MS(x) ((uint64_t)(x) * 1000000ull)

static void load_profile(KbmEngine *e, KbmConfig *c, const char *id) {
    kbm_profile_to_mapping(kbm_config_profile(c, id), &e->map);
}
static void kb(KbmEngine *e, uint8_t mods, uint8_t k1, uint8_t k2) {
    uint8_t r[8] = {mods, 0, k1, k2, 0, 0, 0, 0};
    assert(kbm_keyboard(e, 0, r, 8));
}

static void test_keys(void) {
    assert(kbm_key_from_name("w") == 0x1a && kbm_key_from_name("LSHIFT") == 0xe1);
    assert(kbm_key_from_name("mouse2") == KBM_KEY_MOUSE2 && kbm_key_from_name("") == 0);
    assert(kbm_key_from_name("BOGUS") == -1);
    assert(!strcmp(kbm_key_name(0x2c), "SPACE"));
    assert(kbm_key_reserved(KBM_KEY_F12) && kbm_key_reserved(KBM_KEY_F8) && kbm_key_reserved(KBM_KEY_F9) && kbm_key_reserved(KBM_KEY_F11) && kbm_key_reserved(KBM_KEY_F10));
}

static void test_re2r_defaults(void) {
    KbmConfig c; kbm_config_defaults(&c);
    KbmEngine e; kbm_init(&e); load_profile(&e, &c, "re2r");
    KbmFrame f;
    kbm_frame(&e, MS(0), &f);
    assert(f.buttons == 0 && f.lx == 128 && f.ly == 128 && f.rx == 128 && f.ry == 128);
    kb(&e, 0, 0x1a, 0x07);                         /* W + D */
    kbm_frame(&e, MS(1), &f);
    assert(f.lx == 218 && f.ly == 38);             /* normalised diagonal */
    kb(&e, 0x02, 0x15, 0);                          /* LSHIFT + R */
    kbm_frame(&e, MS(2), &f);
    assert((f.buttons & (KBM_BTN_L3 | KBM_BTN_SQUARE)) == (KBM_BTN_L3 | KBM_BTN_SQUARE));
    assert(f.lx == 128 && f.ly == 128);            /* W/D released */
    kb(&e, 0, 0x14, 0);                             /* Q = quick turn combo */
    kbm_frame(&e, MS(3), &f);
    assert((f.buttons & KBM_BTN_CIRCLE) && f.ly == 255);
    kb(&e, 0, 0, 0);
    uint8_t m[4] = {0x03, 0, 0, 0};                 /* LMB+RMB */
    assert(kbm_mouse(&e, 0, m, 4, MS(4)));
    kbm_frame(&e, MS(4), &f);
    assert((f.buttons & (KBM_BTN_R2 | KBM_BTN_L2)) == (KBM_BTN_R2 | KBM_BTN_L2) && f.l2 == 255 && f.r2 == 255);
    kbm_disconnect(&e, 0);
    kbm_frame(&e, MS(5), &f);
    assert(f.buttons == 0 && f.l2 == 0);
    kb(&e, 0x08, 0, 0);                             /* left Windows key */
    kbm_frame(&e, MS(6), &f);
    assert(f.buttons == KBM_BTN_PS);
    kb(&e, 0x80, 0, 0);                             /* right Windows key */
    kbm_frame(&e, MS(7), &f);
    assert(f.buttons == KBM_BTN_PS);
}

static void test_requiem_wheel(void) {
    KbmConfig c; kbm_config_defaults(&c);
    KbmEngine e; kbm_init(&e); load_profile(&e, &c, "requiem");
    KbmFrame f;
    uint8_t up[4] = {0, 0, 0, 1};
    kbm_mouse(&e, 0, up, 4, MS(100));
    kbm_frame(&e, MS(110), &f); assert(f.buttons & KBM_BTN_LEFT);   /* WHEEL_UP -> dpad left */
    kbm_frame(&e, MS(200), &f); assert(!(f.buttons & KBM_BTN_LEFT)); /* pulse ended (80 ms) */
    kb(&e, 0, 0x1b, 0);                                              /* X = quick turn */
    kbm_frame(&e, MS(201), &f); assert((f.buttons & KBM_BTN_CIRCLE) && f.ly == 255);
}

/* Same physical mouse motion must give the same stick at 60/120/250 Hz. */
static double mean_rx_at(int hz, int smooth, int *minv, int *maxv) {
    KbmEngine e; kbm_init(&e);
    e.map.sens = 8; e.map.smooth_ms = smooth; e.map.deadzone = 0;
    uint64_t t_mouse = 0, frame_ns = 1000000000ull / (uint64_t)hz;
    double sum = 0; int n = 0; *minv = 999; *maxv = -999;
    uint64_t next_frame = 0;
    for (uint64_t t = 0; t < MS(2000); t += 100000) {            /* 0.1 ms steps */
        if (t >= t_mouse) { uint8_t r[4] = {0, 3, 0, 0}; kbm_mouse(&e, 0, r, 4, t); t_mouse += MS(8); } /* 125 Hz */
        if (t >= next_frame) {
            KbmFrame f; kbm_frame(&e, t, &f);
            if (t > MS(200)) { int v = f.rx - 128; sum += v; ++n; if (v < *minv) *minv = v; if (v > *maxv) *maxv = v; }
            next_frame += frame_ns;
        }
    }
    return sum / n;
}
static void test_rate_independent_mouse(void) {
    int mn, mx;
    double a = mean_rx_at(60, 16, &mn, &mx);
    double b = mean_rx_at(120, 16, &mn, &mx);
    double c = mean_rx_at(250, 16, &mn, &mx);
    printf("mouse 3 counts/8ms -> mean rx: 60Hz=%.1f 120Hz=%.1f 250Hz=%.1f (250Hz range %d..%d)\n", a, b, c, mn, mx);
    assert(a > 40 && a < 56);           /* ~48: same feel as v0.6 at sens 8 */
    assert(abs((int)(a - c)) <= 4 && abs((int)(b - c)) <= 4);
    assert(mx - mn <= 30);              /* no 0/96 flicker between frames */
    /* stops promptly: after motion ends, stick returns to centre within window */
    KbmEngine e; kbm_init(&e); e.map.smooth_ms = 16;
    uint8_t r[4] = {0, 10, 0, 0}; kbm_mouse(&e, 0, r, 4, MS(10));
    KbmFrame f; kbm_frame(&e, MS(12), &f); assert(f.rx > 128);
    kbm_frame(&e, MS(27), &f); assert(f.rx == 128);
}

static void test_ads_and_pointer(void) {
    KbmConfig c; kbm_config_defaults(&c);
    KbmEngine e; kbm_init(&e); load_profile(&e, &c, "requiem");
    assert(e.map.ads_pct == 150);
    uint8_t mv[4] = {0, 2, 0, 0};
    KbmFrame f;
    kbm_mouse(&e, 0, mv, 4, MS(10)); kbm_frame(&e, MS(12), &f);
    int hip = f.rx - 128;
    uint8_t aim[4] = {0x02, 2, 0, 0};                    /* RMB held + same motion */
    kbm_mouse(&e, 0, aim, 4, MS(40)); kbm_frame(&e, MS(42), &f);
    int ads = f.rx - 128;
    assert(f.buttons & KBM_BTN_L2);
    printf("ADS: hip=%d aim=%d (ads_pct=150)\n", hip, ads);
    assert(ads > hip && ads >= hip * 14 / 10);
    /* pointer mode: mouse -> left stick, clicks -> cross/circle, wheel -> RS scroll, RS camera off */
    e.pointer_mode = 1;
    uint8_t click[4] = {0x01, 5, 0, 0};
    kbm_mouse(&e, 0, click, 4, MS(80)); kbm_frame(&e, MS(82), &f);
    assert(f.lx > 128 && f.rx == 128 && f.ry == 128);
    assert((f.buttons & KBM_BTN_CROSS) && !(f.buttons & (KBM_BTN_R2 | KBM_BTN_L2)));
    uint8_t wheel[4] = {0, 0, 0, (uint8_t)-1};
    kbm_mouse(&e, 0, wheel, 4, MS(100)); kbm_frame(&e, MS(105), &f);
    assert(f.ry > 128 && !(f.buttons & KBM_BTN_RIGHT));   /* scrolls, no weapon switch */
    kb(&e, 0, 0x09, 0);                                    /* keyboard still maps (F = cross) */
    kbm_frame(&e, MS(106), &f); assert(f.buttons & KBM_BTN_CROSS);
    e.pointer_mode = 0;
}

static void test_capture(void) {
    KbmConfig c; kbm_config_defaults(&c);
    KbmEngine e; kbm_init(&e); load_profile(&e, &c, "re2r");
    kb(&e, 0, 0x15, 0);                 /* R already held: not a new press */
    kbm_capture_arm(&e);
    kb(&e, 0, 0x15, 0x0b);              /* H newly pressed */
    assert(e.captured == 0x0b && !e.capture_armed);
    KbmFrame f; kbm_frame(&e, MS(1), &f);
    assert(f.buttons & KBM_BTN_SQUARE);  /* R still maps */
    assert(!kbm_is_down(&e, 0x0b));      /* captured key suppressed until release */
    kb(&e, 0, 0, 0); kb(&e, 0, 0x0b, 0);
    assert(kbm_is_down(&e, 0x0b));
    kbm_capture_arm(&e);
    uint8_t m[4] = {0x08, 0, 0, 0};      /* MOUSE4 */
    kbm_mouse(&e, 0, m, 4, MS(2));
    assert(e.captured == KBM_KEY_MOUSE4);
}

static void test_config_roundtrip(void) {
    static KbmConfig a, b;
    static char buf[49152], buf2[49152];
    kbm_config_defaults(&a);
    KbmProfile *p = kbm_config_profile(&a, "requiem");
    p->sens = 13; p->invert_y = 1; p->smooth_ms = 25;
    kbm_profile_action(p, "reload")->key[0] = (uint8_t)kbm_key_from_name("T");
    assert(kbm_config_set_title(&a, "PPSA01234", "native") == 0);
    assert(kbm_config_set_title(&a, "CUSA00001", "requiem") == 0);
    assert(kbm_config_set_title(&a, "bad", "native") == -1);
    assert(kbm_config_set_title(&a, "PPSA09999", "nope") == -1);
    a.inject_hz = 120;
    a.idle_exit_min = 45;
    a.open_settings_on_launch = 0;
    int n = kbm_config_write(&a, buf, sizeof(buf));
    assert(n > 0);
    kbm_config_defaults(&b);
    assert(kbm_config_parse(&b, buf) > 30);
    int n2 = kbm_config_write(&b, buf2, sizeof(buf2));
    assert(n == n2 && !memcmp(buf, buf2, (size_t)n));
    assert(kbm_config_title_is_native(&b, "PPSA01234"));
    assert(!strcmp(kbm_config_profile_for(&b, "CUSA00001")->id, "requiem"));
    assert(!strcmp(kbm_config_profile_for(&b, "PPSA77777")->id, "generic"));   /* unknown game -> default */
    assert(kbm_config_effective_hz(&b) == 120);
    assert(b.idle_exit_min == 45 && b.open_settings_on_launch == 0);
    b.inject_hz = 0; assert(kbm_config_effective_hz(&b) == 250);
    /* reserved and garbage values are refused, values clamped */
    kbm_config_parse(&b, "[profile.re2r]\nkey.reload=F12,NONE\nsens=999\n[global]\ndefault_profile=zzz\n");
    assert(kbm_profile_action(kbm_config_profile(&b, "re2r"), "reload")->key[0] == 0);
    assert(kbm_config_profile(&b, "re2r")->sens == 50);
    assert(!strcmp(b.default_profile, "generic"));
    /* removal */
    assert(kbm_config_set_title(&b, "PPSA01234", "") == 0 && !kbm_config_title_is_native(&b, "PPSA01234"));
    /* JSON is produced and fits */
    assert(kbm_config_json(&a, "requiem", buf, sizeof(buf)) > 0);
    FILE *fp = fopen("config_test.json", "w"); fputs(buf, fp); fclose(fp);
    char d[64]; assert(!strcmp(kbm_out_describe(KBM_OUT_LS_DOWN | KBM_BTN_CIRCLE, d, sizeof(d)), "LS_DOWN+CIRCLE"));
    assert(kbm_profile_reset(&a, "requiem") == 0 && kbm_config_profile(&a, "requiem")->sens == 8);
}

static void test_title_db(void) {
    static KbmConfig c;
    static char buf[8192];
    kbm_config_defaults(&c);
    KbmResolved r;
    /* RE2 (2019) PS5 EU: built-in game profile */
    kbm_config_resolve(&c, "PPSA04289", &r);
    assert(r.info.known && !strcmp(r.p->id, "re2r") && !strcmp(r.source, "game") && !r.native);
    /* Requiem: the Title ID the user's console reported */
    kbm_config_resolve(&c, "PPSA31246", &r);
    assert(!strcmp(r.p->id, "requiem") && strstr(r.info.name, "equiem"));
    /* RE4 2016 PS4 port shares the name with the remake -> override to the template */
    kbm_config_resolve(&c, "CUSA04885", &r);
    assert(r.info.known && !strcmp(r.p->id, "tps") && !strcmp(r.source, "template"));
    kbm_config_resolve(&c, "PPSA07412", &r);
    assert(!strcmp(r.p->id, "re4_remake"));
    /* Fortnite: native KB+M on PS5 -> only a suggestion unless auto_native */
    kbm_config_resolve(&c, "PPSA01923", &r);
    assert(r.info.known && r.native_suggested && !r.native);
    c.auto_native = 1;
    kbm_config_resolve(&c, "PPSA01923", &r);
    assert(r.native && !strcmp(r.source, "auto_native"));
    c.auto_native = 0;
    /* user rule beats everything; "auto" removes it */
    assert(kbm_config_set_title(&c, "PPSA04289", "generic") == 0);
    kbm_config_resolve(&c, "PPSA04289", &r);
    assert(!strcmp(r.p->id, "generic") && !strcmp(r.source, "rule"));
    assert(kbm_config_set_title(&c, "PPSA01923", "native") == 0);
    kbm_config_resolve(&c, "PPSA01923", &r);
    assert(r.native && !r.native_suggested && !strcmp(r.source, "native"));
    assert(kbm_config_set_title(&c, "PPSA04289", "auto") == 0);
    kbm_config_resolve(&c, "PPSA04289", &r);
    assert(!strcmp(r.source, "game"));
    /* unknown / empty */
    kbm_config_resolve(&c, "PPSA00000", &r);
    assert(!r.info.known && !strcmp(r.source, "default") && !strcmp(r.p->id, "generic"));
    kbm_config_resolve(&c, "", &r);
    assert(!strcmp(r.source, "menu") && !strcmp(r.p->id, "ps_menu"));
    /* every built-in: has move + PS actions, no reserved keys, at most 32 actions */
    for (int i = 0; i < c.n_profiles; ++i) {
        const KbmProfile *p = &c.p[i];
        assert(p->n > 0 && p->n <= KBM_MAX_ACTIONS);
        int ps = 0;
        for (int a = 0; a < p->n; ++a) {
            for (int s = 0; s < 2; ++s) assert(!kbm_key_reserved(p->a[a].key[s]));
            if (!strcmp(p->a[a].id, "ps")) ps = 1;
        }
        assert(ps);
        assert(!kbm_profile_modified(p));
        assert(kbm_profile_json(p, buf, sizeof(buf)) > 0);
    }
    /* Elden Ring uses the arrows for the D-pad on PC: they must not also move the stick */
    {
        KbmMapping m; kbm_profile_to_mapping(kbm_config_profile(&c, "elden_ring"), &m);
        assert(m.out[kbm_key_from_name("UP")] == KBM_BTN_UP && m.out[kbm_key_from_name("W")] == KBM_OUT_LS_UP);
        kbm_profile_to_mapping(kbm_config_profile(&c, "re2r"), &m);
        assert(m.out[kbm_key_from_name("UP")] == KBM_OUT_LS_UP);
    }
    /* untouched config stores no profile sections */
    static char ini[4096];
    kbm_config_defaults(&c);
    assert(kbm_config_write(&c, ini, sizeof(ini)) > 0 && !strstr(ini, "[profile."));
    /* a v0.7 config.ini keeps working (same profile/action ids) */
    kbm_config_defaults(&c);
    int ap = kbm_config_parse(&c, "[global]\ndefault_profile=requiem\nnative_switch=1\n[titles]\nPPSA31246=requiem\n"
                                  "[profile.re2r]\nsens=12\nads_pct=100\nkey.run=LSHIFT,NONE\nkey.reload=T,NONE\nkey.ps=LGUI,RGUI\n");
    assert(ap == 8);
    assert(!strcmp(c.default_profile, "requiem"));
    assert(kbm_config_profile(&c, "re2r")->sens == 12 && kbm_profile_modified(kbm_config_profile(&c, "re2r")));
    assert(kbm_profile_action(kbm_config_profile(&c, "re2r"), "reload")->key[0] == kbm_key_from_name("T"));
    kbm_config_resolve(&c, "PPSA31246", &r);
    assert(!strcmp(r.p->id, "requiem") && !strcmp(r.source, "rule"));
    /* language: system id mapping, config override, INI, English labels */
    assert(kbm_lang_from_system(3) == KBM_LANG_ES && kbm_lang_from_system(20) == KBM_LANG_ES);
    assert(kbm_lang_from_system(1) == KBM_LANG_EN && kbm_lang_from_system(18) == KBM_LANG_EN);
    assert(kbm_lang_from_system(-1) == KBM_LANG_AUTO);
    kbm_config_defaults(&c);
    assert(kbm_lang_effective(&c, KBM_LANG_ES) == KBM_LANG_ES && kbm_lang_effective(&c, KBM_LANG_AUTO) == KBM_LANG_EN);
    kbm_config_parse(&c, "[global]\nlang=en\n");
    assert(c.lang == KBM_LANG_EN && kbm_lang_effective(&c, KBM_LANG_ES) == KBM_LANG_EN);
    assert(kbm_config_write(&c, ini, sizeof(ini)) > 0 && strstr(ini, "lang=en\n"));
    kbm_config_parse(&c, "[global]\nlang=auto\n");
    assert(c.lang == KBM_LANG_AUTO);
    assert(!strcmp(kbm_config_profile(&c, "generic")->name_en, "Generic (all buttons)"));
    assert(!strcmp(kbm_profile_action(kbm_config_profile(&c, "re2r"), "reload")->label_en, "Reload"));
    for (int i = 0; i < c.n_profiles; ++i)
        for (int a = 0; a < c.p[i].n; ++a) assert(c.p[i].a[a].label && c.p[i].a[a].label_en && *c.p[i].a[a].label_en);
    printf("title db: %d built-in profiles OK\n", c.n_profiles);
}

/* GTA Trilogy DE: on-foot and vehicle layers, switched by F (enter/exit) or B */
static void test_vehicle_layers(void) {
    static KbmConfig c;
    kbm_config_defaults(&c);
    KbmResolved r;
    kbm_config_resolve(&c, "PPSA03525", &r);               /* San Andreas DE (EU) */
    assert(!strcmp(r.p->id, "gta_trilogy_de") && !strcmp(r.source, "game"));
    kbm_config_resolve(&c, "PPSA03531", &r);               /* Vice City DE (EU) */
    assert(!strcmp(r.p->id, "gta_trilogy_de"));
    kbm_config_resolve(&c, "CUSA26613", &r);               /* GTA III DE (PS4, EU) */
    assert(!strcmp(r.p->id, "gta_trilogy_de"));
    assert(kbm_profile_has_layers(r.p));
    KbmEngine e; kbm_init(&e); load_profile(&e, &c, "gta_trilogy_de");
    assert(e.map.has_layers && e.layer == KBM_LAYER_FOOT);
    const uint8_t W = 0x1a, F = 0x09, B = 0x05, SPACE = 0x2c, A = 0x04;
    KbmFrame f;
    kb(&e, 0, W, 0); kbm_frame(&e, 1, &f);                 /* on foot: W walks */
    assert(f.ly < 128 && !(f.buttons & KBM_BTN_R2));
    kb(&e, 0x02, 0, 0); kbm_frame(&e, 2, &f);
    assert(f.buttons & KBM_BTN_CROSS);                     /* Shift = sprint (DE PC keys) */
    kb(&e, 0, SPACE, 0); kbm_frame(&e, 2, &f);
    assert(f.buttons & KBM_BTN_SQUARE);                    /* Space = jump */
    kb(&e, 0, F, 0); kbm_frame(&e, 3, &f);                 /* enter vehicle */
    assert((f.buttons & KBM_BTN_TRIANGLE) && e.layer == KBM_LAYER_VEHICLE);
    kb(&e, 0, 0, 0);
    kb(&e, 0, W, A); kbm_frame(&e, 4, &f);                 /* car: W accelerates, A still steers */
    assert((f.buttons & KBM_BTN_R2) && f.r2 == 255 && f.ly == 128 && f.lx < 128);
    kb(&e, 0, SPACE, 0); kbm_frame(&e, 5, &f);
    assert(f.buttons == KBM_BTN_R1);                       /* handbrake, not jump */
    kb(&e, 0, 0x16, 0); kbm_frame(&e, 5, &f);
    assert(f.buttons == KBM_BTN_L2 && f.ly == 128);        /* S brakes (no weapon wheel, no stick) */
    kb(&e, 0, 0x2b, 0); kbm_frame(&e, 5, &f);
    assert(f.buttons == KBM_BTN_RIGHT);                    /* Tab = radio in the car */
    kb(&e, 0, 0, 0);
    uint8_t m2[4] = {2, 0, 0, 0}; kbm_mouse(&e, 0, m2, 4, 6); kbm_frame(&e, 6, &f);
    assert(!(f.buttons & KBM_BTN_L2));                     /* right click does nothing in the car */
    uint8_t m0[4] = {0, 0, 0, 0}; kbm_mouse(&e, 0, m0, 4, 7);
    kb(&e, 0, F, 0); kb(&e, 0, 0, 0);                      /* exit vehicle */
    assert(e.layer == KBM_LAYER_FOOT);
    kb(&e, 0, B, 0); kb(&e, 0, 0, 0); assert(e.layer == KBM_LAYER_VEHICLE);   /* manual resync */
    kb(&e, 0, B, 0); kb(&e, 0, 0, 0); assert(e.layer == KBM_LAYER_FOOT);
    e.pointer_mode = 1; kb(&e, 0, B, 0); kb(&e, 0, 0, 0); assert(e.layer == KBM_LAYER_FOOT);
    e.pointer_mode = 0;
    /* Classic scheme variant: same keys, classic buttons */
    {
        KbmEngine k; kbm_init(&k); load_profile(&k, &c, "gta_trilogy_classic");
        kb(&k, 0, F, 0); kb(&k, 0, 0, 0); assert(k.layer == KBM_LAYER_VEHICLE);
        kb(&k, 0, W, 0); kbm_frame(&k, 9, &f); assert(f.buttons == KBM_BTN_CROSS);   /* accelerate = Cross */
        kb(&k, 0, 0x16, 0); kbm_frame(&k, 9, &f); assert(f.buttons == KBM_BTN_SQUARE); /* brake = Square */
        kb(&k, 0, 0, 0);
    }
    /* GTA V: same idea, PC-style driving */
    {
        KbmEngine v; kbm_init(&v); load_profile(&v, &c, "gta5");
        assert(v.map.has_layers);
        const uint8_t LSHIFT_MOD = 0x02;                   /* left shift in the modifier byte */
        kb(&v, 0, W, 0); kbm_frame(&v, 1, &f); assert(f.ly < 128 && !(f.buttons & KBM_BTN_R2));
        kb(&v, 0, F, 0); kb(&v, 0, 0, 0); assert(v.layer == KBM_LAYER_VEHICLE);
        kb(&v, 0, W, 0); kbm_frame(&v, 2, &f); assert((f.buttons & KBM_BTN_R2) && f.ly == 128);
        kb(&v, 0, SPACE, 0); kbm_frame(&v, 3, &f); assert(f.buttons == KBM_BTN_R1);   /* handbrake */
        kb(&v, LSHIFT_MOD, 0, 0); kbm_frame(&v, 4, &f); assert(f.buttons == 0);        /* no sprint in a car */
        kb(&v, 0, 0, 0);
        uint8_t both[4] = {3, 0, 0, 0}; kbm_mouse(&v, 0, both, 4, 5); kbm_frame(&v, 5, &f);
        assert((f.buttons & KBM_BTN_L1) && (f.buttons & KBM_BTN_R1) && !(f.buttons & (KBM_BTN_L2 | KBM_BTN_R2)));  /* drive-by */
        uint8_t none[4] = {0, 0, 0, 0}; kbm_mouse(&v, 0, none, 4, 6);
        kb(&v, 0, F, 0); kb(&v, 0, 0, 0); assert(v.layer == KBM_LAYER_FOOT);
        kbm_mouse(&v, 0, both, 4, 7); kbm_frame(&v, 7, &f);
        assert((f.buttons & KBM_BTN_R2) && (f.buttons & KBM_BTN_L2));                  /* on foot: aim + fire */
        kbm_mouse(&v, 0, none, 4, 8);
    }
    /* profiles without vehicle actions never switch */
    KbmEngine g; kbm_init(&g); load_profile(&g, &c, "re2r");
    assert(!g.map.has_layers); kbm_set_layer(&g, KBM_LAYER_VEHICLE); assert(g.layer == KBM_LAYER_FOOT);
}

/* No game running: PS5 menu profile, keys and mouse move the D-pad */
static void test_ps_menu(void) {
    static KbmConfig c;
    kbm_config_defaults(&c);
    KbmResolved r;
    kbm_config_resolve(&c, "", &r);
    assert(!strcmp(r.p->id, "ps_menu") && !strcmp(r.source, "menu"));
    KbmEngine e; kbm_init(&e); load_profile(&e, &c, "ps_menu");
    assert(e.map.mouse_dpad && e.map.pointer_sens == 10);
    KbmFrame f;
    const uint8_t W = 0x1a, UP = 0x52, ENTER = 0x28, F = 0x09, E = 0x08, SPACE = 0x2c, ESC = 0x29;
    kb(&e, 0, W, 0); kbm_frame(&e, 1, &f); assert(f.buttons == KBM_BTN_UP && f.lx == 128 && f.ly == 128);
    kb(&e, 0, UP, 0); kbm_frame(&e, 2, &f); assert(f.buttons == KBM_BTN_UP);
    kb(&e, 0, ENTER, 0); kbm_frame(&e, 3, &f); assert(f.buttons == KBM_BTN_CROSS);
    kb(&e, 0, F, 0); kbm_frame(&e, 4, &f); assert(f.buttons == KBM_BTN_CROSS);
    kb(&e, 0, E, 0); kbm_frame(&e, 5, &f); assert(f.buttons == KBM_BTN_CROSS);
    kb(&e, 0, SPACE, 0); kbm_frame(&e, 6, &f); assert(f.buttons == KBM_BTN_OPTIONS);
    kb(&e, 0, ESC, 0); kbm_frame(&e, 7, &f); assert(f.buttons == KBM_BTN_CIRCLE);
    kb(&e, 0, 0, 0);
    /* mouse: 120 counts right (default speed) = one D-pad step, no camera */
    const uint64_t MS = 1000000ull;
    uint64_t t = 1000 * MS;
    for (int i = 0; i < 13; ++i) { uint8_t m[4] = {0, 10, 0, 0}; kbm_mouse(&e, 0, m, 4, t + i * MS); }
    t += 13 * MS;
    kbm_frame(&e, t, &f); assert(f.buttons == KBM_BTN_RIGHT && f.rx == 128 && f.ry == 128);
    kbm_frame(&e, t + 70 * MS, &f); assert(f.buttons == 0);         /* released after 60 ms */
    kbm_frame(&e, t + 120 * MS, &f); assert(f.buttons == 0);        /* only 10 counts left */
    for (int i = 0; i < 30; ++i) { uint8_t m[4] = {0, 0, (uint8_t)(int8_t)-10, 0}; kbm_mouse(&e, 0, m, 4, t + (130 + i) * MS); }
    kbm_frame(&e, t + 160 * MS, &f); assert(f.buttons == KBM_BTN_UP);   /* vertical swipe: up */
    kbm_frame(&e, t + 900 * MS, &f); assert(f.buttons == 0);        /* idle: leftover dropped */
    uint8_t click[4] = {1, 0, 0, 0}; kbm_mouse(&e, 0, click, 4, t + 901 * MS);
    kbm_frame(&e, t + 902 * MS, &f); assert(f.buttons & KBM_BTN_CROSS);   /* left click selects */
    /* a game profile keeps the mouse on the camera */
    kbm_config_resolve(&c, "PPSA04289", &r);
    assert(!strcmp(r.p->id, "re2r"));
}

/* Stick shaping: radial dead-zone jump, small-movement boost, dead-zone finder */
static void test_stick_shaping(void) {
    static KbmConfig c;
    kbm_config_defaults(&c);
    const uint64_t MS = 1000000ull;
    KbmFrame f;
    /* the same slow mouse motion with dz 0 / curve 0, then dz 30 / curve 50 */
    int out[3];
    for (int k = 0; k < 3; ++k) {
        KbmEngine e; kbm_init(&e); load_profile(&e, &c, "requiem");
        e.map.deadzone = k ? 30 : 0; e.map.curve = k == 2 ? 50 : 0;
        uint64_t t = 1000 * MS;
        for (int i = 0; i < 8; ++i) { uint8_t m[4] = {0, 1, 0, 0}; kbm_mouse(&e, 0, m, 4, t + i * 2 * MS); }
        kbm_frame(&e, t + 16 * MS, &f);
        out[k] = f.rx - 128;
        assert(f.ry == 128);                               /* pure X stays pure X (radial) */
    }
    printf("slow mouse -> rx: linear=%d deadzone30=%d deadzone30+curve50=%d\n", out[0], out[1], out[2]);
    assert(out[0] > 0 && out[1] >= 30 + out[0] * 97 / 127 - 1 && out[2] > out[1]);
    /* diagonal: radial saturation keeps the direction */
    {
        KbmEngine e; kbm_init(&e); load_profile(&e, &c, "requiem");
        e.map.deadzone = 20;
        uint64_t t = 1000 * MS;
        for (int i = 0; i < 8; ++i) { uint8_t m[4] = {0, 60, 60, 0}; kbm_mouse(&e, 0, m, 4, t + i * 2 * MS); }
        kbm_frame(&e, t + 16 * MS, &f);
        int x = f.rx - 128, y = f.ry - 128;
        assert(x > 80 && y > 80 && (x - y) < 3 && (y - x) < 3 && x * x + y * y <= 128 * 128);
    }
    /* dead-zone finder: ramp on the right stick, click stops it */
    {
        KbmEngine e; kbm_init(&e); load_profile(&e, &c, "requiem");
        uint64_t t = 5000 * MS;
        kbm_calib_start(&e, t);
        kbm_frame(&e, t + 600 * MS, &f); assert(f.rx == 128 + 10 && f.ry == 128);
        uint8_t m[4] = {0, 50, 0, 0}; kbm_mouse(&e, 0, m, 4, t + 1200 * MS);   /* mouse motion ignored */
        kbm_frame(&e, t + 1500 * MS, &f); assert(f.rx == 128 + 25);
        uint8_t click[4] = {1, 0, 0, 0}; kbm_mouse(&e, 0, click, 4, t + 1510 * MS);
        assert(!e.calib_active && e.calib_result == 25 - KBM_CALIB_REACTION);
        kbm_frame(&e, t + 1520 * MS, &f); assert(!(f.buttons & KBM_BTN_R2));     /* the click didn't fire */
        uint8_t up[4] = {0, 0, 0, 0}; kbm_mouse(&e, 0, up, 4, t + 1530 * MS);
        kbm_calib_start(&e, t + 2000 * MS);                                      /* Esc cancels */
        kb(&e, 0, 0x29, 0); assert(e.calib_result == -2); kb(&e, 0, 0, 0);
        kbm_calib_start(&e, t + 3000 * MS);                                      /* nothing pressed */
        kbm_frame(&e, t + 3000 * MS + (KBM_CALIB_MAX + 2) * KBM_CALIB_STEP_NS, &f);
        assert(!e.calib_active && e.calib_result == -3);
    }
}

static void test_usb_descriptor_parser(void) {
    const uint8_t composite[] = {
      9,2,59,0,2,1,0,0x80,50,
      9,4,0,0,1,3,1,1,0, 9,0x21,0x11,0x01,0,1,0x22,50,0, 7,5,0x81,0x03,8,0,10,
      9,4,1,0,1,3,1,2,0, 9,0x21,0x11,0x01,0,1,0x22,50,0, 7,5,0x82,0x03,4,0,10
    };
    HidBootLayout l;
    assert(hid_boot_find(composite, sizeof(composite), &l));
    assert(l.keyboard_iface==0 && l.keyboard_ep==0x81 && l.mouse_iface==1 && l.mouse_ep==0x82);
    uint8_t malformed[] = {9,2,9,0,1,1,0,0,50, 0,4};
    assert(!hid_boot_find(malformed,sizeof(malformed),&l));
}

int main(void) {
    test_keys();
    test_re2r_defaults();
    test_requiem_wheel();
    test_rate_independent_mouse();
    test_ads_and_pointer();
    test_capture();
    test_config_roundtrip();
    test_title_db();
    test_vehicle_layers();
    test_ps_menu();
    test_stick_shaping();
    test_usb_descriptor_parser();
    puts("PASS: keys, RE2R/Requiem defaults, combos, wheel, rate-independent mouse, capture, config INI/JSON, title DB + auto profile, vehicle layers, PS5 menu profile, stick shaping + dead-zone finder, HID parser");
    return 0;
}
