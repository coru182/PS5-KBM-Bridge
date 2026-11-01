/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "kbm_config.h"
#include "kbm_keys.h"
#include "kbm_builtin_gen.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ defaults */
_Static_assert(KBM_BUILTIN_COUNT <= KBM_MAX_PROFILES, "raise KBM_MAX_PROFILES");

static void copy_str(char *dst, size_t n, const char *src) {
    if (!n) return;
    size_t i = 0;
    if (src) for (; src[i] && i + 1 < n; ++i) dst[i] = src[i];
    dst[i] = 0;
}

static void profile_from_builtin(KbmProfile *p, int bi) {
    const KbmBuiltinProfile *d = &KBM_BUILTIN[bi];
    memset(p, 0, sizeof(*p));
    copy_str(p->id, sizeof(p->id), d->id);
    p->name = d->name;
    p->name_en = d->name_en;
    KbmMapping m; kbm_mapping_defaults(&m);
    p->sens = m.sens; p->yratio = m.yratio; p->smooth_ms = m.smooth_ms;
    p->deadzone = m.deadzone; p->invert_y = m.invert_y;
    p->ads_pct = d->ads_pct;
    p->builtin = bi;
    p->n = d->n < KBM_MAX_ACTIONS ? d->n : KBM_MAX_ACTIONS;
    for (int i = 0; i < p->n; ++i) {
        copy_str(p->a[i].id, sizeof(p->a[i].id), d->a[i].id);
        p->a[i].label = d->a[i].label;
        p->a[i].label_en = d->a[i].label_en;
        p->a[i].out = d->a[i].out;
        p->a[i].layer = d->a[i].layer;
        p->a[i].key[0] = kbm_key_reserved(d->a[i].k1) ? 0 : d->a[i].k1;
        p->a[i].key[1] = kbm_key_reserved(d->a[i].k2) ? 0 : d->a[i].k2;
    }
}

void kbm_config_defaults(KbmConfig *c) {
    memset(c, 0, sizeof(*c));
    c->inject_hz = 0;
    c->auto_activate = 1;
    c->release_on_exit = 1;
    c->native_switch = 1;
    c->auto_native = 0;
    c->idle_exit_min = 0;
    c->open_settings_on_launch = 1;
    c->pointer_sens = 10;
    c->lang = KBM_LANG_AUTO;
    copy_str(c->default_profile, sizeof(c->default_profile), "generic");
    c->n_profiles = KBM_BUILTIN_COUNT;
    for (int i = 0; i < KBM_BUILTIN_COUNT; ++i) profile_from_builtin(&c->p[i], i);
}

int kbm_profile_reset(KbmConfig *c, const char *id) {
    KbmProfile *p = kbm_config_profile(c, id);
    if (!p) return -1;
    profile_from_builtin(p, p->builtin);
    return 0;
}

int kbm_profile_modified(const KbmProfile *p) {
    KbmProfile d;
    profile_from_builtin(&d, p->builtin);
    if (p->sens != d.sens || p->yratio != d.yratio || p->smooth_ms != d.smooth_ms || p->deadzone != d.deadzone ||
        p->invert_y != d.invert_y || p->ads_pct != d.ads_pct || p->n != d.n) return 1;
    for (int i = 0; i < p->n; ++i)
        if (p->a[i].key[0] != d.a[i].key[0] || p->a[i].key[1] != d.a[i].key[1]) return 1;
    return 0;
}

/* ------------------------------------------------------------------ language */
int kbm_lang_effective(const KbmConfig *c, int sys) {
    if (c->lang == KBM_LANG_ES || c->lang == KBM_LANG_EN) return c->lang;
    return sys == KBM_LANG_ES ? KBM_LANG_ES : KBM_LANG_EN;
}
const char *kbm_lang_code(int lang) { return lang == KBM_LANG_ES ? "es" : lang == KBM_LANG_EN ? "en" : "auto"; }
int kbm_lang_from_system(int id) {
    /* SCE system language ids: 3 = Spanish (Spain), 20 = Spanish (Latin America). */
    if (id == 3 || id == 20) return KBM_LANG_ES;
    return (id >= 0 && id < 64) ? KBM_LANG_EN : KBM_LANG_AUTO;
}

/* ------------------------------------------------------------------ title database */
int kbm_title_lookup(const char *tid, KbmTitleInfo *out) {
    out->known = 0; out->name = ""; out->builtin = -1; out->native = 0;
    if (!kbm_valid_title_id(tid)) return 0;
    uint32_t prefix;
    if (!strncmp(tid, "PPSA", 4)) prefix = 0;
    else if (!strncmp(tid, "CUSA", 4)) prefix = 1;
    else return 0;
    uint32_t key = prefix * 100000u + (uint32_t)atoi(tid + 4);
    int lo = 0, hi = KBM_TITLE_COUNT - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        uint32_t k = KBM_TITLES[mid].key;
        if (k == key) {
            const KbmConceptEntry *ce = &KBM_CONCEPTS[KBM_TITLES[mid].concept];
            out->known = 1;
            out->name = KBM_CONCEPT_NAMES + ce->name_off;
            out->builtin = ce->profile;
            out->native = ce->native;
            return 1;
        }
        if (k < key) lo = mid + 1; else hi = mid - 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ lookups */
KbmProfile *kbm_config_profile(KbmConfig *c, const char *id) {
    if (!id) return NULL;
    for (int i = 0; i < c->n_profiles; ++i) if (!strcmp(c->p[i].id, id)) return &c->p[i];
    return NULL;
}
const KbmProfile *kbm_config_profile_c(const KbmConfig *c, const char *id) {
    return kbm_config_profile((KbmConfig *)c, id);
}
KbmAction *kbm_profile_action(KbmProfile *p, const char *id) {
    if (!p || !id) return NULL;
    for (int i = 0; i < p->n; ++i) if (!strcmp(p->a[i].id, id)) return &p->a[i];
    return NULL;
}

int kbm_valid_title_id(const char *s) {
    if (!s || strlen(s) != 9) return 0;
    for (int i = 0; i < 4; ++i) if (s[i] < 'A' || s[i] > 'Z') return 0;
    for (int i = 4; i < 9; ++i) if (s[i] < '0' || s[i] > '9') return 0;
    return 1;
}
int kbm_valid_profile_id(const char *s) {
    for (int i = 0; s && i < KBM_BUILTIN_COUNT; ++i) if (!strcmp(KBM_BUILTIN[i].id, s)) return 1;
    return 0;
}

const char *kbm_config_title_target(const KbmConfig *c, const char *tid) {
    if (!tid || !*tid) return NULL;
    for (int i = 0; i < c->n_titles; ++i) if (!strcmp(c->titles[i].tid, tid)) return c->titles[i].target;
    return NULL;
}

int kbm_config_set_title(KbmConfig *c, const char *tid, const char *target) {
    if (!kbm_valid_title_id(tid)) return -1;
    int remove = !target || !*target || !strcmp(target, "default") || !strcmp(target, "auto");
    if (!remove && strcmp(target, "native") && !kbm_valid_profile_id(target)) return -1;
    for (int i = 0; i < c->n_titles; ++i) {
        if (!strcmp(c->titles[i].tid, tid)) {
            if (remove) { c->titles[i] = c->titles[--c->n_titles]; return 0; }
            copy_str(c->titles[i].target, sizeof(c->titles[i].target), target);
            return 0;
        }
    }
    if (remove) return 0;
    if (c->n_titles >= KBM_MAX_TITLES) return -1;
    copy_str(c->titles[c->n_titles].tid, sizeof(c->titles[0].tid), tid);
    copy_str(c->titles[c->n_titles].target, sizeof(c->titles[0].target), target);
    ++c->n_titles;
    return 0;
}

void kbm_config_resolve(const KbmConfig *c, const char *tid, KbmResolved *r) {
    memset(r, 0, sizeof(*r));
    kbm_title_lookup(tid, &r->info);
    const KbmProfile *ap = NULL;
    const char *asrc;
    if (r->info.builtin >= 0 && r->info.builtin < c->n_profiles) {
        ap = &c->p[r->info.builtin];
        asrc = KBM_BUILTIN[r->info.builtin].kind == 0 ? "game" : "template";
    } else if (!tid || !*tid) {
        /* no game running: PS5 home screen, library, store... */
        ap = kbm_config_profile_c(c, "ps_menu");
        asrc = "menu";
        if (!ap) { ap = kbm_config_profile_c(c, c->default_profile); asrc = "none"; }
        if (!ap) ap = &c->p[0];
    } else {
        ap = kbm_config_profile_c(c, c->default_profile);
        if (!ap) ap = &c->p[0];
        asrc = "default";
    }
    r->auto_p = ap;
    r->p = ap;
    r->source = asrc;
    const char *rule = kbm_config_title_target(c, tid);
    if (rule && !strcmp(rule, "native")) { r->native = 1; r->source = "native"; return; }
    const KbmProfile *rp = rule ? kbm_config_profile_c(c, rule) : NULL;
    if (rp) { r->p = rp; r->source = "rule"; return; }
    if (r->info.native) {
        if (c->auto_native) { r->native = 1; r->source = "auto_native"; }
        else r->native_suggested = 1;
    }
}

int kbm_config_title_is_native(const KbmConfig *c, const char *tid) {
    KbmResolved r; kbm_config_resolve(c, tid, &r);
    return r.native;
}

const KbmProfile *kbm_config_profile_for(const KbmConfig *c, const char *tid) {
    KbmResolved r; kbm_config_resolve(c, tid, &r);
    return r.p;
}

int kbm_config_effective_hz(const KbmConfig *c) {
    return (c->inject_hz == 60 || c->inject_hz == 120 || c->inject_hz == 250) ? c->inject_hz : 250;
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
void kbm_profile_clamp(KbmProfile *p) {
    p->sens = clampi(p->sens, 1, 50);
    p->yratio = clampi(p->yratio, 25, 400);
    p->smooth_ms = clampi(p->smooth_ms, 4, 60);
    p->deadzone = clampi(p->deadzone, 0, 90);
    p->invert_y = p->invert_y ? 1 : 0;
    p->ads_pct = clampi(p->ads_pct, 25, 400);
}

/* For each layer: a key bound by an action of that layer uses only those
 * actions; otherwise it uses the actions marked BOTH (movement, PS, pause...). */
void kbm_profile_to_mapping(const KbmProfile *p, KbmMapping *m) {
    kbm_mapping_defaults(m);
    m->sens = p->sens; m->yratio = p->yratio; m->smooth_ms = p->smooth_ms;
    m->deadzone = p->deadzone; m->invert_y = p->invert_y;
    m->ads_pct = p->ads_pct;
    m->mouse_dpad = KBM_BUILTIN[p->builtin].mouse_dpad;
    static uint64_t spec[2][256], both[256];
    static uint8_t bound[2][256];
    memset(spec, 0, sizeof(spec)); memset(both, 0, sizeof(both)); memset(bound, 0, sizeof(bound));
    for (int i = 0; i < p->n; ++i) {
        const KbmAction *a = &p->a[i];
        if (a->layer == KBM_LAYER_VEHICLE) m->has_layers = 1;
        for (int s = 0; s < 2; ++s) {
            uint8_t k = a->key[s];
            if (!k || kbm_key_reserved(k)) continue;
            if (a->layer == KBM_LAYER_BOTH) both[k] |= a->out;
            else { int L = a->layer == KBM_LAYER_VEHICLE; spec[L][k] |= a->out; bound[L][k] = 1; }
        }
    }
    for (int k = 0; k < 256; ++k) {
        m->out[k] = bound[0][k] ? spec[0][k] : both[k];
        m->out_veh[k] = bound[1][k] ? spec[1][k] : both[k];
    }
}

int kbm_profile_has_layers(const KbmProfile *p) {
    for (int i = 0; i < p->n; ++i) if (p->a[i].layer == KBM_LAYER_VEHICLE) return 1;
    return 0;
}

/* ------------------------------------------------------------------ out names */
static const struct { uint64_t bit; const char *name; } OUTS[] = {
    {KBM_OUT_LS_UP,"LS_UP"},{KBM_OUT_LS_DOWN,"LS_DOWN"},{KBM_OUT_LS_LEFT,"LS_LEFT"},{KBM_OUT_LS_RIGHT,"LS_RIGHT"},
    {KBM_BTN_CROSS,"CROSS"},{KBM_BTN_CIRCLE,"CIRCLE"},{KBM_BTN_SQUARE,"SQUARE"},{KBM_BTN_TRIANGLE,"TRIANGLE"},
    {KBM_BTN_L1,"L1"},{KBM_BTN_R1,"R1"},{KBM_BTN_L2,"L2"},{KBM_BTN_R2,"R2"},{KBM_BTN_L3,"L3"},{KBM_BTN_R3,"R3"},
    {KBM_BTN_OPTIONS,"OPTIONS"},{KBM_BTN_TOUCH,"TOUCHPAD"},{KBM_BTN_PS,"PS"},
    {KBM_BTN_UP,"DPAD_UP"},{KBM_BTN_DOWN,"DPAD_DOWN"},{KBM_BTN_LEFT,"DPAD_LEFT"},{KBM_BTN_RIGHT,"DPAD_RIGHT"},
    {KBM_OUT_MODE_TOGGLE,"MODE"},
};
const char *kbm_out_describe(uint64_t out, char *buf, size_t n) {
    size_t len = 0;
    if (n) buf[0] = 0;
    for (size_t i = 0; i < sizeof(OUTS) / sizeof(OUTS[0]); ++i) {
        if (!(out & OUTS[i].bit)) continue;
        int w = snprintf(buf + len, n > len ? n - len : 0, "%s%s", len ? "+" : "", OUTS[i].name);
        if (w < 0 || (size_t)w >= n - len) break;
        len += (size_t)w;
    }
    return buf;
}

/* ------------------------------------------------------------------ INI parse */
static char *trim(char *s) {
    while (*s && isspace((unsigned char)*s)) ++s;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) *--e = 0;
    return s;
}

static void parse_keys(KbmAction *a, char *v) {
    char *comma = strchr(v, ',');
    char *k2 = NULL;
    if (comma) { *comma = 0; k2 = trim(comma + 1); }
    int c1 = kbm_key_from_name(trim(v));
    int c2 = k2 ? kbm_key_from_name(k2) : 0;
    a->key[0] = (uint8_t)(c1 > 0 && !kbm_key_reserved((uint8_t)c1) ? c1 : 0);
    a->key[1] = (uint8_t)(c2 > 0 && !kbm_key_reserved((uint8_t)c2) ? c2 : 0);
}

int kbm_config_parse(KbmConfig *c, const char *text) {
    char line[256];
    int applied = 0;
    enum { S_NONE, S_GLOBAL, S_TITLES, S_PROFILE } sec = S_NONE;
    KbmProfile *prof = NULL;
    const char *p = text;
    while (p && *p) {
        size_t i = 0;
        while (*p && *p != '\n') { if (i + 1 < sizeof(line)) line[i++] = *p; ++p; }
        if (*p == '\n') ++p;
        line[i] = 0;
        char *s = trim(line);
        if (!*s || *s == '#' || *s == ';') continue;
        if (*s == '[') {
            char *e = strchr(s, ']');
            if (!e) { sec = S_NONE; continue; }
            *e = 0; ++s;
            if (!strcmp(s, "global")) sec = S_GLOBAL;
            else if (!strcmp(s, "titles")) sec = S_TITLES;
            else if (!strncmp(s, "profile.", 8) && (prof = kbm_config_profile(c, s + 8))) sec = S_PROFILE;
            else sec = S_NONE;
            continue;
        }
        char *eq = strchr(s, '=');
        if (!eq) continue;
        *eq = 0;
        char *k = trim(s), *v = trim(eq + 1);
        int iv = atoi(v);
        if (sec == S_GLOBAL) {
            if (!strcmp(k, "inject_hz")) c->inject_hz = iv;
            else if (!strcmp(k, "auto_activate")) c->auto_activate = iv ? 1 : 0;
            else if (!strcmp(k, "release_on_exit")) c->release_on_exit = iv ? 1 : 0;
            else if (!strcmp(k, "native_switch")) c->native_switch = iv ? 1 : 0;
            else if (!strcmp(k, "auto_native")) c->auto_native = iv ? 1 : 0;
            else if (!strcmp(k, "idle_exit_min")) c->idle_exit_min = iv < 0 ? 0 : (iv > 240 ? 240 : iv);
            else if (!strcmp(k, "open_settings_on_launch")) c->open_settings_on_launch = iv ? 1 : 0;
            else if (!strcmp(k, "lang")) c->lang = !strcmp(v, "es") ? KBM_LANG_ES : !strcmp(v, "en") ? KBM_LANG_EN : KBM_LANG_AUTO;
            else if (!strcmp(k, "pointer_sens")) c->pointer_sens = iv < 1 ? 1 : (iv > 50 ? 50 : iv);
            else if (!strcmp(k, "default_profile") && kbm_valid_profile_id(v))
                copy_str(c->default_profile, sizeof(c->default_profile), v);
            else continue;
            ++applied;
        } else if (sec == S_TITLES) {
            if (kbm_config_set_title(c, k, v) == 0) ++applied;
        } else if (sec == S_PROFILE && prof) {
            if (!strcmp(k, "sens")) prof->sens = iv;
            else if (!strcmp(k, "yratio")) prof->yratio = iv;
            else if (!strcmp(k, "smooth_ms")) prof->smooth_ms = iv;
            else if (!strcmp(k, "deadzone")) prof->deadzone = iv;
            else if (!strcmp(k, "invert_y")) prof->invert_y = iv;
            else if (!strcmp(k, "ads_pct")) prof->ads_pct = iv;
            else if (!strncmp(k, "key.", 4)) {
                KbmAction *a = kbm_profile_action(prof, k + 4);
                if (!a) continue;
                parse_keys(a, v);
            } else continue;
            kbm_profile_clamp(prof);
            ++applied;
        }
    }
    return applied;
}

/* ------------------------------------------------------------------ INI write */
typedef struct { char *b; size_t n, len; int over; } Out;
static void outf(Out *o, const char *fmt, ...) {
    if (o->over) return;
    va_list ap; va_start(ap, fmt);
    int w = vsnprintf(o->b + o->len, o->n - o->len, fmt, ap);
    va_end(ap);
    if (w < 0 || (size_t)w >= o->n - o->len) { o->over = 1; return; }
    o->len += (size_t)w;
}
static const char *kn(uint8_t k) { const char *s = kbm_key_name(k); return (k && s) ? s : "NONE"; }

int kbm_config_write(const KbmConfig *c, char *buf, size_t n) {
    Out o = { buf, n, 0, 0 };
    if (!n) return -1;
    buf[0] = 0;
    outf(&o, "# PS5 KBM Bridge config (v2). Editable from http://<PS5-IP>:8099\n"
             "# Only profiles you changed are stored; the rest come built into the bridge.\n");
    outf(&o, "[global]\ninject_hz=%d\nauto_activate=%d\nrelease_on_exit=%d\nnative_switch=%d\nauto_native=%d\nidle_exit_min=%d\nopen_settings_on_launch=%d\npointer_sens=%d\nlang=%s\ndefault_profile=%s\n\n",
         c->inject_hz, c->auto_activate, c->release_on_exit, c->native_switch, c->auto_native, c->idle_exit_min, c->open_settings_on_launch, c->pointer_sens, kbm_lang_code(c->lang), c->default_profile);
    outf(&o, "[titles]\n");
    for (int i = 0; i < c->n_titles; ++i) outf(&o, "%s=%s\n", c->titles[i].tid, c->titles[i].target);
    for (int i = 0; i < c->n_profiles; ++i) {
        const KbmProfile *p = &c->p[i];
        if (!kbm_profile_modified(p)) continue;
        outf(&o, "\n[profile.%s]\nsens=%d\nyratio=%d\nsmooth_ms=%d\ndeadzone=%d\ninvert_y=%d\nads_pct=%d\n",
             p->id, p->sens, p->yratio, p->smooth_ms, p->deadzone, p->invert_y, p->ads_pct);
        for (int a = 0; a < p->n; ++a)
            outf(&o, "key.%s=%s,%s\n", p->a[a].id, kn(p->a[a].key[0]), kn(p->a[a].key[1]));
    }
    return o.over ? -1 : (int)o.len;
}

/* ------------------------------------------------------------------ JSON */
void kbm_json_init(KbmJson *j, char *buf, size_t n) { j->buf = buf; j->n = n; j->len = 0; j->overflow = 0; if (n) buf[0] = 0; }
static void jput(KbmJson *j, char ch) {
    if (j->len + 1 >= j->n) { j->overflow = 1; return; }
    j->buf[j->len++] = ch; j->buf[j->len] = 0;
}
void kbm_json_raw(KbmJson *j, const char *s) { while (*s) jput(j, *s++); }
void kbm_json_str(KbmJson *j, const char *s) {
    jput(j, '"');
    for (; s && *s; ++s) {
        unsigned char ch = (unsigned char)*s;
        if (ch == '"' || ch == '\\') { jput(j, '\\'); jput(j, (char)ch); }
        else if (ch < 0x20) { char t[8]; snprintf(t, sizeof(t), "\\u%04x", ch); kbm_json_raw(j, t); }
        else jput(j, (char)ch);
    }
    jput(j, '"');
}
void kbm_json_int(KbmJson *j, long long v) { char t[32]; snprintf(t, sizeof(t), "%lld", v); kbm_json_raw(j, t); }
void kbm_json_fmt(KbmJson *j, const char *fmt, ...) {
    char t[512];
    va_list ap; va_start(ap, fmt); vsnprintf(t, sizeof(t), fmt, ap); va_end(ap);
    kbm_json_raw(j, t);
}

static const char *kind_name(int bi) {
    return KBM_BUILTIN[bi].kind == 0 ? "game" : KBM_BUILTIN[bi].kind == 2 ? "menu" : "template";
}

static void profile_detail(KbmJson *j, const KbmProfile *p) {
    const KbmBuiltinProfile *b = &KBM_BUILTIN[p->builtin];
    kbm_json_raw(j, "{\"id\":"); kbm_json_str(j, p->id);
    kbm_json_raw(j, ",\"name\":"); kbm_json_str(j, p->name);
    kbm_json_raw(j, ",\"name_en\":"); kbm_json_str(j, p->name_en);
    kbm_json_raw(j, ",\"kind\":"); kbm_json_str(j, kind_name(p->builtin));
    kbm_json_raw(j, ",\"tmpl\":"); kbm_json_str(j, b->tmpl);
    kbm_json_raw(j, ",\"notes\":"); kbm_json_str(j, b->notes);
    kbm_json_fmt(j, ",\"conf\":%d,\"native\":%d,\"mod\":%d,\"layers\":%d", b->confidence, b->native, kbm_profile_modified(p), kbm_profile_has_layers(p));
    kbm_json_fmt(j, ",\"sens\":%d,\"yratio\":%d,\"smooth_ms\":%d,\"deadzone\":%d,\"invert_y\":%d,\"ads_pct\":%d,\"actions\":[",
                 p->sens, p->yratio, p->smooth_ms, p->deadzone, p->invert_y, p->ads_pct);
    for (int a = 0; a < p->n; ++a) {
        char d[96];
        if (a) jput(j, ',');
        kbm_json_raw(j, "{\"id\":"); kbm_json_str(j, p->a[a].id);
        kbm_json_raw(j, ",\"label\":"); kbm_json_str(j, p->a[a].label);
        kbm_json_raw(j, ",\"label_en\":"); kbm_json_str(j, p->a[a].label_en);
        kbm_json_fmt(j, ",\"layer\":%d", p->a[a].layer);
        kbm_json_raw(j, ",\"out\":"); kbm_json_str(j, kbm_out_describe(p->a[a].out, d, sizeof(d)));
        kbm_json_raw(j, ",\"keys\":[");
        kbm_json_str(j, p->a[a].key[0] ? kn(p->a[a].key[0]) : "");
        jput(j, ',');
        kbm_json_str(j, p->a[a].key[1] ? kn(p->a[a].key[1]) : "");
        kbm_json_raw(j, "]}");
    }
    kbm_json_raw(j, "]}");
}

int kbm_profile_json(const KbmProfile *p, char *buf, size_t n) {
    if (!p) return -1;
    KbmJson j; kbm_json_init(&j, buf, n);
    profile_detail(&j, p);
    return j.overflow ? -1 : (int)j.len;
}

int kbm_config_json(const KbmConfig *c, const char *edit, char *buf, size_t n) {
    KbmJson j; kbm_json_init(&j, buf, n);
    kbm_json_fmt(&j, "{\"inject_hz\":%d,\"effective_hz\":%d,\"auto_activate\":%d,\"release_on_exit\":%d,"
                 "\"native_switch\":%d,\"auto_native\":%d,\"idle_exit_min\":%d,\"open_settings_on_launch\":%d,\"pointer_sens\":%d,"
                 "\"title_db\":%d,\"lang\":\"%s\",\"default_profile\":", c->inject_hz, kbm_config_effective_hz(c),
                 c->auto_activate, c->release_on_exit, c->native_switch, c->auto_native, c->idle_exit_min,
                 c->open_settings_on_launch, c->pointer_sens, KBM_TITLE_COUNT, kbm_lang_code(c->lang));
    kbm_json_str(&j, c->default_profile);
    kbm_json_raw(&j, ",\"titles\":[");
    for (int i = 0; i < c->n_titles; ++i) {
        KbmTitleInfo ti; kbm_title_lookup(c->titles[i].tid, &ti);
        if (i) jput(&j, ',');
        kbm_json_raw(&j, "{\"tid\":"); kbm_json_str(&j, c->titles[i].tid);
        kbm_json_raw(&j, ",\"name\":"); kbm_json_str(&j, ti.name);
        kbm_json_raw(&j, ",\"target\":"); kbm_json_str(&j, c->titles[i].target);
        jput(&j, '}');
    }
    kbm_json_raw(&j, "],\"profiles\":[");
    for (int i = 0; i < c->n_profiles; ++i) {
        const KbmProfile *p = &c->p[i];
        const KbmBuiltinProfile *b = &KBM_BUILTIN[p->builtin];
        if (i) jput(&j, ',');
        kbm_json_raw(&j, "{\"id\":"); kbm_json_str(&j, p->id);
        kbm_json_raw(&j, ",\"name\":"); kbm_json_str(&j, p->name);
        kbm_json_raw(&j, ",\"name_en\":"); kbm_json_str(&j, p->name_en);
        kbm_json_raw(&j, ",\"kind\":"); kbm_json_str(&j, kind_name(p->builtin));
        kbm_json_raw(&j, ",\"tmpl\":"); kbm_json_str(&j, b->tmpl);
        kbm_json_fmt(&j, ",\"conf\":%d,\"native\":%d,\"mod\":%d}", b->confidence, b->native, kbm_profile_modified(p));
    }
    kbm_json_raw(&j, "],\"edit\":");
    const KbmProfile *e = edit ? kbm_config_profile_c(c, edit) : NULL;
    if (e) profile_detail(&j, e); else kbm_json_raw(&j, "null");
    kbm_json_raw(&j, "}");
    return j.overflow ? -1 : (int)j.len;
}
