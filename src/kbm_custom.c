/* SPDX-License-Identifier: GPL-3.0-or-later
 * Custom profiles: load from JSON (the export format), delete, export.
 * A tiny JSON reader is enough here: one object with known keys, no floats
 * that matter, input capped by the caller (~32 KB). Pure C, host-tested.
 */
#include "kbm_config.h"
#include "kbm_keys.h"
#include "kbm_builtin_gen.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ reader */
typedef struct {
    const char *s, *e;
    int err;
    char msg[96];
} J;

static void jerr(J *j, const char *m) {
    if (j->err) return;
    j->err = 1;
    snprintf(j->msg, sizeof(j->msg), "%s", m);
}
static void ws(J *j) { while (j->s < j->e && isspace((unsigned char)*j->s)) ++j->s; }
static int peek(J *j) { ws(j); return j->s < j->e ? (unsigned char)*j->s : -1; }
static int eat(J *j, char ch) {
    if (peek(j) == ch) { ++j->s; return 1; }
    return 0;
}
static int hex4(J *j, unsigned *v) {
    *v = 0;
    for (int i = 0; i < 4; ++i) {
        if (j->s >= j->e) return 0;
        char ch = *j->s++;
        int d = ch >= '0' && ch <= '9' ? ch - '0' : ch >= 'a' && ch <= 'f' ? ch - 'a' + 10 : ch >= 'A' && ch <= 'F' ? ch - 'A' + 10 : -1;
        if (d < 0) return 0;
        *v = *v * 16 + (unsigned)d;
    }
    return 1;
}
/* Reads a string into out (UTF-8, truncated on a character boundary). */
static void jstr(J *j, char *out, size_t n) {
    size_t o = 0;
    if (n) out[0] = 0;
    if (!eat(j, '"')) { jerr(j, "expected a string"); return; }
    while (j->s < j->e && *j->s != '"') {
        unsigned cp;
        unsigned char ch = (unsigned char)*j->s++;
        char tmp[4]; int tl = 0;
        if (ch == '\\') {
            if (j->s >= j->e) break;
            char esc = *j->s++;
            switch (esc) {
            case 'n': tmp[tl++] = '\n'; break;
            case 't': tmp[tl++] = '\t'; break;
            case 'r': tmp[tl++] = '\r'; break;
            case 'b': tmp[tl++] = '\b'; break;
            case 'f': tmp[tl++] = '\f'; break;
            case 'u':
                if (!hex4(j, &cp)) { jerr(j, "bad \\u escape"); return; }
                if (cp >= 0xD800 && cp <= 0xDBFF && j->e - j->s >= 6 && j->s[0] == '\\' && j->s[1] == 'u') {
                    unsigned lo; j->s += 2;
                    if (!hex4(j, &lo)) { jerr(j, "bad \\u escape"); return; }
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                }
                if (cp < 0x80) tmp[tl++] = (char)cp;
                else if (cp < 0x800) { tmp[tl++] = (char)(0xC0 | (cp >> 6)); tmp[tl++] = (char)(0x80 | (cp & 0x3F)); }
                else if (cp < 0x10000) { tmp[tl++] = (char)(0xE0 | (cp >> 12)); tmp[tl++] = (char)(0x80 | ((cp >> 6) & 0x3F)); tmp[tl++] = (char)(0x80 | (cp & 0x3F)); }
                else { tmp[tl++] = (char)(0xF0 | (cp >> 18)); tmp[tl++] = (char)(0x80 | ((cp >> 12) & 0x3F)); tmp[tl++] = (char)(0x80 | ((cp >> 6) & 0x3F)); tmp[tl++] = (char)(0x80 | (cp & 0x3F)); }
                break;
            default: tmp[tl++] = esc; break;   /* \" \\ \/ */
            }
        } else {
            tmp[tl++] = (char)ch;
        }
        for (int i = 0; i < tl; ++i) if (o + 1 < n) out[o++] = tmp[i];
    }
    if (j->s >= j->e) { jerr(j, "unterminated string"); return; }
    ++j->s;
    while (o > 0 && o + 1 >= n && ((unsigned char)out[o - 1] & 0xC0) == 0x80) --o;   /* cut inside a character */
    if (o > 0 && o + 1 >= n && ((unsigned char)out[o - 1] & 0xC0) == 0xC0) --o;
    if (n) out[o] = 0;
}
static long jnum(J *j) {
    ws(j);
    if (j->e - j->s >= 4 && !memcmp(j->s, "true", 4)) { j->s += 4; return 1; }
    if (j->e - j->s >= 5 && !memcmp(j->s, "false", 5)) { j->s += 5; return 0; }
    char t[32]; size_t n = 0;
    while (j->s < j->e && n + 1 < sizeof(t) && (isdigit((unsigned char)*j->s) || strchr("+-.eE", *j->s))) t[n++] = *j->s++;
    t[n] = 0;
    if (!n) { jerr(j, "expected a number"); return 0; }
    return (long)strtod(t, NULL);
}
static void jskip(J *j, int depth) {
    int c = peek(j);
    if (depth > 16) { jerr(j, "too deeply nested"); return; }
    if (c == '"') { char d[2]; jstr(j, d, sizeof(d)); }
    else if (c == '{' || c == '[') {
        char close = c == '{' ? '}' : ']';
        ++j->s;
        if (eat(j, close)) return;
        do {
            if (c == '{') { char d[2]; jstr(j, d, sizeof(d)); if (!eat(j, ':')) { jerr(j, "expected ':'"); return; } }
            jskip(j, depth + 1);
        } while (!j->err && eat(j, ','));
        if (!eat(j, close)) jerr(j, "unterminated object/array");
    } else if (c == 'n') {
        if (j->e - j->s >= 4 && !memcmp(j->s, "null", 4)) j->s += 4; else jerr(j, "bad value");
    } else jnum(j);
}
/* Object: call obj_begin, then while (obj_key(j, k)) { ...read value... } */
static int obj_begin(J *j) { if (!eat(j, '{')) { jerr(j, "expected an object"); return 0; } return 1; }
static int obj_key(J *j, char *k, size_t n, int *first) {
    if (j->err) return 0;
    if (*first) { *first = 0; if (eat(j, '}')) return 0; }
    else if (!eat(j, ',')) { if (!eat(j, '}')) jerr(j, "expected ',' or '}'"); return 0; }
    jstr(j, k, n);
    if (!eat(j, ':')) { jerr(j, "expected ':'"); return 0; }
    return !j->err;
}
static int arr_begin(J *j) { if (!eat(j, '[')) { jerr(j, "expected an array"); return 0; } return 1; }
static int arr_next(J *j, int *first) {
    if (j->err) return 0;
    if (*first) { *first = 0; return !eat(j, ']'); }
    if (eat(j, ',')) return 1;
    if (!eat(j, ']')) jerr(j, "expected ',' or ']'");
    return 0;
}

/* ------------------------------------------------------------------ pool */
static size_t pool_cap(const KbmConfig *c) { return sizeof(c->pool); }
static int in_pool(const KbmConfig *c, const char *s) { return s >= c->pool && s < c->pool + pool_cap(c); }

static const char *pool_add(KbmConfig *c, const char *s) {
    size_t n = strlen(s) + 1;
    if (c->pool_len + n > pool_cap(c)) return NULL;
    char *d = c->pool + c->pool_len;
    memcpy(d, s, n);
    c->pool_len += n;
    return d;
}

/* Drop strings of replaced/deleted profiles. */
static void pool_compact(KbmConfig *c) {
    static char tmp[sizeof(((KbmConfig *)0)->pool)];
    size_t t = 0;
    for (int i = 0; i < c->n_profiles; ++i) {
        KbmProfile *p = &c->p[i];
        const char **f[64 * 2 + 6];
        int nf = 0;
        f[nf++] = &p->name; f[nf++] = &p->name_en; f[nf++] = &p->tmpl; f[nf++] = &p->notes; f[nf++] = &p->tips; f[nf++] = &p->tips_en;
        for (int a = 0; a < p->n; ++a) { f[nf++] = &p->a[a].label; f[nf++] = &p->a[a].label_en; }
        for (int k = 0; k < nf; ++k) {
            if (!*f[k] || !in_pool(c, *f[k])) continue;
            size_t n = strlen(*f[k]) + 1;
            memcpy(tmp + t, *f[k], n);
            *f[k] = c->pool + t;
            t += n;
        }
    }
    memcpy(c->pool, tmp, t);
    c->pool_len = t;
}

/* ------------------------------------------------------------------ load */
static int valid_id(const char *s) {
    size_t n = strlen(s);
    if (!n || n >= KBM_ID_LEN) return 0;
    for (size_t i = 0; i < n; ++i) if (!(islower((unsigned char)s[i]) || isdigit((unsigned char)s[i]) || s[i] == '_')) return 0;
    return 1;
}
static int layer_from(const char *s) {
    if (!*s || !strcmp(s, "foot")) return KBM_LAYER_FOOT;
    if (!strcmp(s, "vehicle") || !strcmp(s, "car")) return KBM_LAYER_VEHICLE;
    if (!strcmp(s, "both")) return KBM_LAYER_BOTH;
    if (!strcmp(s, "bike")) return KBM_LAYER_BIKE;
    if (!strcmp(s, "air")) return KBM_LAYER_AIR;
    if (s[0] >= '0' && s[0] <= '4' && !s[1]) return s[0] - '0';
    return -1;
}
static const char *layer_name(int l) {
    return l == KBM_LAYER_VEHICLE ? "vehicle" : l == KBM_LAYER_BOTH ? "both" : l == KBM_LAYER_BIKE ? "bike" : l == KBM_LAYER_AIR ? "air" : "foot";
}
static int key_from(const char *s, uint8_t *out) {
    *out = 0;
    if (!*s || !strcmp(s, "NONE")) return 0;
    int k = kbm_key_from_name(s);
    if (k <= 0) return -1;
    if (kbm_key_reserved((uint8_t)k)) return -2;
    *out = (uint8_t)k;
    return 0;
}

#define LS_ALL (KBM_OUT_LS_UP | KBM_OUT_LS_DOWN | KBM_OUT_LS_LEFT | KBM_OUT_LS_RIGHT)

typedef struct { char label[64], label_en[64]; } ActText;

int kbm_custom_load(KbmConfig *c, const char *json, size_t len, char *err, size_t errn) {
    static KbmProfile np;
    static ActText txt[KBM_MAX_ACTIONS];
    static char name[64], name_en[64], tmpl[KBM_ID_LEN], notes[1024], tips[1024], tips_en[1024];
    J j = { json, json + len, 0, {0} };
    char k[32], s[64];
    int no_auto_move = 0, have_id = 0, first;
    if (err && errn) err[0] = 0;

    memset(&np, 0, sizeof(np)); memset(txt, 0, sizeof(txt));
    name[0] = name_en[0] = tmpl[0] = notes[0] = tips[0] = tips_en[0] = 0;
    KbmMapping m; kbm_mapping_defaults(&m);
    np.sens = m.sens; np.yratio = m.yratio; np.smooth_ms = m.smooth_ms; np.deadzone = m.deadzone;
    np.invert_y = m.invert_y; np.curve = m.curve; np.ads_pct = m.ads_pct;
    np.custom = 1; np.builtin = -1;

    if (len && (unsigned char)json[0] == 0xEF && len >= 3) j.s += 3;    /* UTF-8 BOM */
    first = 1;
    if (obj_begin(&j)) while (obj_key(&j, k, sizeof(k), &first)) {
        if (!strcmp(k, "id")) { jstr(&j, np.id, sizeof(np.id)); have_id = 1; }
        else if (!strcmp(k, "name")) jstr(&j, name, sizeof(name));
        else if (!strcmp(k, "name_en")) jstr(&j, name_en, sizeof(name_en));
        else if (!strcmp(k, "template")) jstr(&j, tmpl, sizeof(tmpl));
        else if (!strcmp(k, "notes")) jstr(&j, notes, sizeof(notes));
        else if (!strcmp(k, "tips_en")) jstr(&j, tips_en, sizeof(tips_en));
        else if (!strcmp(k, "tips")) {
            if (peek(&j) == '{') {
                int f2 = 1; char k2[8];
                obj_begin(&j);
                while (obj_key(&j, k2, sizeof(k2), &f2)) {
                    if (!strcmp(k2, "es")) jstr(&j, tips, sizeof(tips));
                    else if (!strcmp(k2, "en")) jstr(&j, tips_en, sizeof(tips_en));
                    else jskip(&j, 1);
                }
            } else jstr(&j, tips, sizeof(tips));
        }
        else if (!strcmp(k, "kind")) { jstr(&j, s, sizeof(s)); np.kind = !strcmp(s, "template") ? 1 : !strcmp(s, "menu") ? 2 : 0; }
        else if (!strcmp(k, "mouse")) { jstr(&j, s, sizeof(s)); np.mouse_dpad = !strcmp(s, "dpad"); }
        else if (!strcmp(k, "sens")) np.sens = (int)jnum(&j);
        else if (!strcmp(k, "yratio")) np.yratio = (int)jnum(&j);
        else if (!strcmp(k, "smooth_ms")) np.smooth_ms = (int)jnum(&j);
        else if (!strcmp(k, "deadzone")) np.deadzone = (int)jnum(&j);
        else if (!strcmp(k, "curve")) np.curve = (int)jnum(&j);
        else if (!strcmp(k, "invert_y")) np.invert_y = (int)jnum(&j);
        else if (!strcmp(k, "ads_pct")) np.ads_pct = (int)jnum(&j);
        else if (!strcmp(k, "no_auto_move")) no_auto_move = (int)jnum(&j);
        else if (!strcmp(k, "title_ids")) {
            int f2 = 1;
            if (arr_begin(&j)) while (arr_next(&j, &f2)) {
                char t[16];
                jstr(&j, t, sizeof(t));
                for (char *q = t; *q; ++q) *q = (char)toupper((unsigned char)*q);
                if (j.err) break;
                if (!kbm_valid_title_id(t)) { snprintf(j.msg, sizeof(j.msg), "bad Title ID '%s' (like PPSA01234)", t); j.err = 1; break; }
                if (np.n_tids < KBM_MAX_PROFILE_TIDS) memcpy(np.tids[np.n_tids++], t, 10);   /* validated: 9 chars + NUL */
            }
        }
        else if (!strcmp(k, "actions")) {
            int f2 = 1;
            if (arr_begin(&j)) while (arr_next(&j, &f2)) {
                if (np.n >= KBM_MAX_ACTIONS) { snprintf(j.msg, sizeof(j.msg), "more than %d actions", KBM_MAX_ACTIONS); j.err = 1; break; }
                KbmAction *a = &np.a[np.n];
                ActText *tx = &txt[np.n];
                char ps5[96] = {0}, lay[16] = {0}, pc[2][24] = {{0}};
                int f3 = 1; char ak[16];
                if (!obj_begin(&j)) break;
                while (obj_key(&j, ak, sizeof(ak), &f3)) {
                    if (!strcmp(ak, "id")) jstr(&j, a->id, sizeof(a->id));
                    else if (!strcmp(ak, "label")) jstr(&j, tx->label, sizeof(tx->label));
                    else if (!strcmp(ak, "label_en")) jstr(&j, tx->label_en, sizeof(tx->label_en));
                    else if (!strcmp(ak, "ps5") || !strcmp(ak, "out")) jstr(&j, ps5, sizeof(ps5));
                    else if (!strcmp(ak, "layer")) {
                        if (peek(&j) == '"') jstr(&j, lay, sizeof(lay));
                        else snprintf(lay, sizeof(lay), "%ld", jnum(&j));
                    }
                    else if (!strcmp(ak, "pc") || !strcmp(ak, "keys")) {
                        if (peek(&j) == '"') jstr(&j, pc[0], sizeof(pc[0]));
                        else { int f4 = 1, ki = 0; if (arr_begin(&j)) while (arr_next(&j, &f4)) { if (ki < 2) jstr(&j, pc[ki++], sizeof(pc[0])); else jskip(&j, 3); } }
                    }
                    else jskip(&j, 3);
                }
                if (j.err) break;
                int an = np.n + 1;
                if (!a->id[0]) snprintf(a->id, sizeof(a->id), "a%d", an);
                if (!valid_id(a->id)) { snprintf(j.msg, sizeof(j.msg), "action %d: id must be a-z, 0-9, _", an); j.err = 1; break; }
                for (int i = 0; i < np.n; ++i) if (!strcmp(np.a[i].id, a->id)) { snprintf(j.msg, sizeof(j.msg), "duplicate action id '%s'", a->id); j.err = 1; }
                if (j.err) break;
                a->out = kbm_out_parse(ps5);
                if (!a->out) { snprintf(j.msg, sizeof(j.msg), "action '%s': unknown PS5 button '%.40s'", a->id, ps5); j.err = 1; break; }
                int L = layer_from(lay);
                if (L < 0) { snprintf(j.msg, sizeof(j.msg), "action '%s': layer must be foot, vehicle, both, bike or air", a->id); j.err = 1; break; }
                a->layer = (uint8_t)L;
                for (int sl = 0; sl < 2; ++sl) {
                    int r = key_from(pc[sl], &a->key[sl]);
                    if (r == -1) { snprintf(j.msg, sizeof(j.msg), "action '%s': unknown key '%s'", a->id, pc[sl]); j.err = 1; }
                    if (r == -2) { snprintf(j.msg, sizeof(j.msg), "action '%s': %s is reserved by the bridge", a->id, pc[sl]); j.err = 1; }
                }
                if (j.err) break;
                if (!tx->label[0]) snprintf(tx->label, sizeof(tx->label), "%s", a->id);
                ++np.n;
            }
        }
        else jskip(&j, 1);
    }
    if (!j.err && peek(&j) != -1) jerr(&j, "extra data after the profile");
    if (!j.err && !have_id) jerr(&j, "missing \"id\"");
    if (!j.err && !valid_id(np.id)) jerr(&j, "id must be 1-23 characters: a-z, 0-9, _");
    if (!j.err && !np.n) jerr(&j, "no actions");

    /* like the built-ins: movement and the PS button unless the file has its own */
    uint64_t all = 0;
    for (int i = 0; i < np.n; ++i) all |= np.a[i].out;
    if (!j.err && !no_auto_move && !(all & LS_ALL)) {
        static const struct { const char *id, *es, *en; uint64_t out; uint8_t k1, k2; } MV[4] = {
            {"move_up", "Avanzar", "Move forward", KBM_OUT_LS_UP, KBM_KEY_W, KBM_KEY_UP},
            {"move_down", "Retroceder", "Move back", KBM_OUT_LS_DOWN, KBM_KEY_S, KBM_KEY_DOWN},
            {"move_left", "Izquierda", "Left", KBM_OUT_LS_LEFT, KBM_KEY_A, KBM_KEY_LEFT},
            {"move_right", "Derecha", "Right", KBM_OUT_LS_RIGHT, KBM_KEY_D, KBM_KEY_RIGHT}};
        if (np.n + 4 > KBM_MAX_ACTIONS) jerr(&j, "too many actions (movement is added automatically)");
        else {
            memmove(&np.a[4], &np.a[0], sizeof(np.a[0]) * (size_t)np.n);
            memmove(&txt[4], &txt[0], sizeof(txt[0]) * (size_t)np.n);
            for (int i = 0; i < 4; ++i) {
                int arrow_taken = 0;
                for (int q = 4; q < np.n + 4; ++q)
                    if (np.a[q].layer == KBM_LAYER_FOOT || np.a[q].layer == KBM_LAYER_BOTH)
                        arrow_taken |= np.a[q].key[0] == MV[i].k2 || np.a[q].key[1] == MV[i].k2;
                memset(&np.a[i], 0, sizeof(np.a[i]));
                snprintf(np.a[i].id, sizeof(np.a[i].id), "%s", MV[i].id);
                np.a[i].out = MV[i].out; np.a[i].layer = KBM_LAYER_BOTH;
                np.a[i].key[0] = MV[i].k1; np.a[i].key[1] = arrow_taken ? 0 : MV[i].k2;
                snprintf(txt[i].label, sizeof(txt[i].label), "%s", MV[i].es);
                snprintf(txt[i].label_en, sizeof(txt[i].label_en), "%s", MV[i].en);
            }
            np.n += 4;
            for (int i = 4; i < np.n; ++i)
                for (int mv = 0; mv < 4; ++mv) if (!strcmp(np.a[i].id, MV[mv].id)) jerr(&j, "action id move_* is reserved for automatic movement");
        }
    }
    if (!j.err && !(all & KBM_BTN_PS)) {
        if (np.n >= KBM_MAX_ACTIONS) jerr(&j, "too many actions (the PS button is added automatically)");
        else {
            KbmAction *a = &np.a[np.n];
            memset(a, 0, sizeof(*a));
            snprintf(a->id, sizeof(a->id), "%s", kbm_profile_action(&np, "ps") ? "ps_button" : "ps");
            a->out = KBM_BTN_PS; a->layer = KBM_LAYER_BOTH; a->key[0] = KBM_KEY_LGUI; a->key[1] = KBM_KEY_RGUI;
            snprintf(txt[np.n].label, sizeof(txt[0].label), "Botón PS");
            snprintf(txt[np.n].label_en, sizeof(txt[0].label_en), "PS button");
            ++np.n;
        }
    }
    /* one key -> one action per layer ('both' counts in every layer); stick-only actions may be overridden */
    for (int L = 0; !j.err && L <= KBM_LAYER_AIR; ++L) {
        if (L == KBM_LAYER_BOTH) continue;
        for (int x = 0; x < np.n && !j.err; ++x) {
            const KbmAction *a = &np.a[x];
            if ((a->layer != L && a->layer != KBM_LAYER_BOTH) || !(a->out & ~LS_ALL)) continue;
            for (int y = x + 1; y < np.n && !j.err; ++y) {
                const KbmAction *b = &np.a[y];
                if ((b->layer != L && b->layer != KBM_LAYER_BOTH) || !(b->out & ~LS_ALL)) continue;
                for (int s1 = 0; s1 < 2; ++s1) for (int s2 = 0; s2 < 2; ++s2)
                    if (a->key[s1] && a->key[s1] == b->key[s2] && !j.err) {
                        snprintf(j.msg, sizeof(j.msg), "key %s used by '%s' and '%s' in the same mode", kbm_key_name(a->key[s1]), a->id, b->id);
                        j.err = 1;
                    }
            }
        }
    }
    if (j.err) { if (err && errn) snprintf(err, errn, "%s", j.msg[0] ? j.msg : "invalid profile"); return -1; }

    /* place it: replace a profile with the same id (keeping its built-in slot) or append */
    KbmProfile *dst = kbm_config_profile(c, np.id);
    if (!dst && c->n_profiles >= KBM_MAX_PROFILES) { if (err && errn) snprintf(err, errn, "too many profiles (max %d)", KBM_MAX_PROFILES); return -1; }
    np.builtin = dst ? dst->builtin : -1;

    pool_compact(c);
    size_t mark = c->pool_len;
    int fail = 0;
    #define PSTR(field, src) do { if (!fail) { const char *_p = pool_add(c, (src)); if (!_p) fail = 1; else (field) = _p; } } while (0)
    PSTR(np.name, name[0] ? name : np.id);
    PSTR(np.name_en, name_en[0] ? name_en : (name[0] ? name : np.id));
    PSTR(np.tmpl, tmpl);
    PSTR(np.notes, notes);
    PSTR(np.tips, tips);
    PSTR(np.tips_en, tips_en[0] ? tips_en : tips);
    for (int i = 0; i < np.n; ++i) {
        PSTR(np.a[i].label, txt[i].label);
        PSTR(np.a[i].label_en, txt[i].label_en[0] ? txt[i].label_en : txt[i].label);
    }
    #undef PSTR
    if (fail) {
        c->pool_len = mark;
        if (err && errn) snprintf(err, errn, "not enough room for custom profile text (delete some custom profiles)");
        return -1;
    }
    kbm_profile_clamp(&np);
    kbm_profile_snapshot_defaults(&np);
    if (!dst) dst = &c->p[c->n_profiles++];
    *dst = np;
    return (int)(dst - c->p);
}

int kbm_custom_delete(KbmConfig *c, const char *id) {
    KbmProfile *p = kbm_config_profile(c, id);
    if (!p || !p->custom) return -1;
    if (p->builtin >= 0) {
        kbm_profile_restore_builtin(p);
    } else {
        char gone[KBM_ID_LEN];
        snprintf(gone, sizeof(gone), "%s", p->id);
        int i = (int)(p - c->p);
        memmove(&c->p[i], &c->p[i + 1], sizeof(c->p[0]) * (size_t)(c->n_profiles - i - 1));
        --c->n_profiles;
        for (int t = 0; t < c->n_titles; )
            if (!strcmp(c->titles[t].target, gone)) c->titles[t] = c->titles[--c->n_titles]; else ++t;
        if (!strcmp(c->default_profile, gone)) snprintf(c->default_profile, sizeof(c->default_profile), "generic");
    }
    pool_compact(c);
    return 0;
}

/* ------------------------------------------------------------------ export */
int kbm_profile_export(const KbmConfig *c, const KbmProfile *p, char *buf, size_t n) {
    if (!p) return -1;
    KbmJson j; kbm_json_init(&j, buf, n);
    kbm_json_raw(&j, "{\n  \"format\": \"ps5-kbm-profile\",\n  \"version\": 1,\n  \"id\": "); kbm_json_str(&j, p->id);
    kbm_json_raw(&j, ",\n  \"name\": "); kbm_json_str(&j, p->name);
    kbm_json_raw(&j, ",\n  \"name_en\": "); kbm_json_str(&j, p->name_en);
    kbm_json_raw(&j, ",\n  \"kind\": "); kbm_json_str(&j, kbm_kind_name(p->kind));
    if (p->tmpl && *p->tmpl) { kbm_json_raw(&j, ",\n  \"template\": "); kbm_json_str(&j, p->tmpl); }
    kbm_json_raw(&j, ",\n  \"title_ids\": [");
    int nt = 0;
    for (int t = 0; t < p->n_tids; ++t) { if (nt++) kbm_json_raw(&j, ", "); kbm_json_str(&j, p->tids[t]); }
    for (int t = 0; t < c->n_titles; ++t) {
        if (strcmp(c->titles[t].target, p->id)) continue;
        int dup = 0;
        for (int q = 0; q < p->n_tids; ++q) dup |= !strcmp(p->tids[q], c->titles[t].tid);
        if (dup || nt >= KBM_MAX_PROFILE_TIDS) continue;
        if (nt++) kbm_json_raw(&j, ", ");
        kbm_json_str(&j, c->titles[t].tid);
    }
    kbm_json_raw(&j, "]");
    kbm_json_fmt(&j, ",\n  \"mouse\": \"%s\",\n  \"sens\": %d,\n  \"yratio\": %d,\n  \"smooth_ms\": %d,\n  \"deadzone\": %d,\n"
                 "  \"curve\": %d,\n  \"invert_y\": %d,\n  \"ads_pct\": %d,\n  \"no_auto_move\": true",
                 p->mouse_dpad ? "dpad" : "camera", p->sens, p->yratio, p->smooth_ms, p->deadzone, p->curve, p->invert_y, p->ads_pct);
    if (p->notes && *p->notes) { kbm_json_raw(&j, ",\n  \"notes\": "); kbm_json_str(&j, p->notes); }
    if ((p->tips && *p->tips) || (p->tips_en && *p->tips_en)) {
        kbm_json_raw(&j, ",\n  \"tips\": {\"es\": "); kbm_json_str(&j, p->tips);
        kbm_json_raw(&j, ", \"en\": "); kbm_json_str(&j, p->tips_en);
        kbm_json_raw(&j, "}");
    }
    kbm_json_raw(&j, ",\n  \"actions\": [");
    for (int a = 0; a < p->n; ++a) {
        const KbmAction *x = &p->a[a];
        char d[128];
        kbm_json_raw(&j, a ? ",\n    {\"id\": " : "\n    {\"id\": "); kbm_json_str(&j, x->id);
        kbm_json_raw(&j, ", \"label\": "); kbm_json_str(&j, x->label);
        kbm_json_raw(&j, ", \"label_en\": "); kbm_json_str(&j, x->label_en);
        kbm_json_raw(&j, ", \"ps5\": "); kbm_json_str(&j, kbm_out_describe(x->out, d, sizeof(d)));
        kbm_json_raw(&j, ", \"pc\": [");
        kbm_json_str(&j, x->key[0] ? kbm_key_name(x->key[0]) : "");
        kbm_json_raw(&j, ", ");
        kbm_json_str(&j, x->key[1] ? kbm_key_name(x->key[1]) : "");
        kbm_json_raw(&j, "], \"layer\": "); kbm_json_str(&j, layer_name(x->layer));
        kbm_json_raw(&j, "}");
    }
    kbm_json_raw(&j, "\n  ]\n}\n");
    return j.overflow ? -1 : (int)j.len;
}
