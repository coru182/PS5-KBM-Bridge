/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "kbm_api.h"
#include "kbm_keys.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int hexv(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
static void url_decode(char *dst, size_t n, const char *s, size_t len) {
    size_t o = 0;
    for (size_t i = 0; i < len && o + 1 < n; ++i) {
        if (s[i] == '+') dst[o++] = ' ';
        else if (s[i] == '%' && i + 2 < len && hexv(s[i+1]) >= 0 && hexv(s[i+2]) >= 0) {
            dst[o++] = (char)(hexv(s[i+1]) * 16 + hexv(s[i+2])); i += 2;
        } else dst[o++] = s[i];
    }
    dst[o] = 0;
}

void kbm_query_parse(const char *qs, KbmQuery *q) {
    memset(q, 0, sizeof(*q));
    while (qs && *qs && q->n < 16) {
        const char *amp = strchr(qs, '&');
        size_t len = amp ? (size_t)(amp - qs) : strlen(qs);
        const char *eq = memchr(qs, '=', len);
        size_t kl = eq ? (size_t)(eq - qs) : len;
        url_decode(q->k[q->n], sizeof(q->k[0]), qs, kl);
        if (eq) url_decode(q->v[q->n], sizeof(q->v[0]), eq + 1, len - kl - 1);
        if (q->k[q->n][0]) ++q->n;
        qs = amp ? amp + 1 : NULL;
    }
}
const char *kbm_query_get(const KbmQuery *q, const char *k) {
    for (int i = 0; i < q->n; ++i) if (!strcmp(q->k[i], k)) return q->v[i];
    return NULL;
}

int kbm_config_bind(KbmConfig *c, const char *profile, const char *action, int slot,
                    uint8_t code, char *moved_from, size_t mf_n) {
    if (moved_from && mf_n) moved_from[0] = 0;
    KbmProfile *p = kbm_config_profile(c, profile);
    KbmAction *a = kbm_profile_action(p, action);
    if (!a || slot < 0 || slot > 1 || kbm_key_reserved(code)) return -1;
    if (code) {
        for (int i = 0; i < p->n; ++i) {
            for (int s = 0; s < 2; ++s) {
                if (&p->a[i] == a && s == slot) continue;
                if (p->a[i].key[s] == code) {
                    p->a[i].key[s] = 0;
                    if (moved_from && mf_n && !moved_from[0]) snprintf(moved_from, mf_n, "%s", p->a[i].label);
                }
            }
        }
    }
    a->key[slot] = code;
    return 0;
}

static void rt_json(KbmJson *j, const KbmRuntime *r) {
    kbm_json_raw(j, "{\"version\":"); kbm_json_str(j, r->version ? r->version : "?");
    kbm_json_fmt(j, ",\"diag\":%d,\"status\":", r->diag); kbm_json_str(j, r->status);
    kbm_json_fmt(j, ",\"vda\":%d,\"receiver\":%d,\"title\":", r->vda, r->receiver); kbm_json_str(j, r->title);
    kbm_json_raw(j, ",\"title_name\":"); kbm_json_str(j, r->title_name);
    kbm_json_raw(j, ",\"title_source\":"); kbm_json_str(j, r->title_source);
    kbm_json_fmt(j, ",\"native_suggested\":%d", r->native_suggested);
    kbm_json_raw(j, ",\"profile\":"); kbm_json_str(j, r->profile);
    kbm_json_raw(j, ",\"auto_profile\":"); kbm_json_str(j, r->auto_profile);
    kbm_json_fmt(j, ",\"hz\":%d,\"kb_reports_5s\":%u,\"mouse_reports_5s\":%u,\"e2i_p50_us\":%u,\"e2i_p95_us\":%u,"
                 "\"uptime_s\":%u,\"idle_s\":%u,\"autostart_avail\":%d,\"autostart_on\":%d,\"etahen\":%d,\"pointer_mode\":%d,\"ip\":", r->hz, r->kb_reports_5s, r->mouse_reports_5s,
                 r->e2i_p50_us, r->e2i_p95_us, r->uptime_s, r->idle_s, r->autostart_avail, r->autostart_on, r->etahen, r->pointer_mode);
    kbm_json_str(j, r->ip);
    kbm_json_fmt(j, ",\"capture\":{\"active\":%d,\"profile\":", r->capture_active);
    kbm_json_str(j, r->cap_profile);
    kbm_json_raw(j, ",\"action\":"); kbm_json_str(j, r->cap_action);
    kbm_json_fmt(j, ",\"slot\":%d,\"result\":", r->cap_slot); kbm_json_str(j, r->cap_result);
    kbm_json_raw(j, "}}");
}

void kbm_runtime_set_title(KbmRuntime *rt, const KbmConfig *c, const char *tid) {
    KbmResolved r;
    kbm_config_resolve(c, tid, &r);
    snprintf(rt->title, sizeof(rt->title), "%s", tid ? tid : "");
    snprintf(rt->title_name, sizeof(rt->title_name), "%s", r.info.name);
    snprintf(rt->title_source, sizeof(rt->title_source), "%s", r.source);
    rt->native_suggested = r.native_suggested;
    snprintf(rt->profile, sizeof(rt->profile), "%s", r.p->id);
    snprintf(rt->auto_profile, sizeof(rt->auto_profile), "%s", r.auto_p->id);
}

int kbm_api_state_json(KbmApi *api, const char *edit, char *buf, size_t n) {
    KbmJson j; kbm_json_init(&j, buf, n);
    api->lock(api->user);
    kbm_json_raw(&j, "{\"runtime\":");
    rt_json(&j, api->rt);
    kbm_json_raw(&j, ",\"config\":");
    if (!j.overflow) {
        int w = kbm_config_json(api->cfg, (edit && *edit) ? edit : api->rt->profile, j.buf + j.len, j.n - j.len);
        if (w < 0) j.overflow = 1; else j.len += (size_t)w;
    }
    api->unlock(api->user);
    kbm_json_raw(&j, ",\"keys\":[");
    int first = 1;
    for (int c = 1; c < 256; ++c) {
        const char *nm = kbm_key_name((uint8_t)c);
        if (!nm || kbm_key_reserved((uint8_t)c)) continue;
        if (!first) kbm_json_raw(&j, ",");
        kbm_json_str(&j, nm);
        first = 0;
    }
    kbm_json_raw(&j, "]}");
    return j.overflow ? -1 : (int)j.len;
}

static int reply(char *out, size_t n, int ok, const char *msg, const char *extra_k, const char *extra_v) {
    KbmJson j; kbm_json_init(&j, out, n);
    kbm_json_fmt(&j, "{\"ok\":%s", ok ? "true" : "false");
    if (msg) { kbm_json_raw(&j, ok ? ",\"message\":" : ",\"error\":"); kbm_json_str(&j, msg); }
    if (extra_k && extra_v) { kbm_json_raw(&j, ",\""); kbm_json_raw(&j, extra_k); kbm_json_raw(&j, "\":"); kbm_json_str(&j, extra_v); }
    kbm_json_raw(&j, "}");
    return ok ? 200 : 400;
}

static int get_int(const KbmQuery *q, const char *k, int *v) {
    const char *s = kbm_query_get(q, k);
    if (!s || !*s) return 0;
    char *end; long x = strtol(s, &end, 10);
    if (*end) return 0;
    *v = (int)x;
    return 1;
}

int kbm_api_command(KbmApi *api, const char *path, const KbmQuery *q, char *out, size_t n) {
    KbmConfig *c = api->cfg;
    int code = 0;

    if (!strcmp(path, "/api/autostart")) {
        int on = 0;
        char msg[160] = {0};
        get_int(q, "on", &on);
        if (!api->set_autostart || api->set_autostart(api->user, on ? 1 : 0, msg, sizeof(msg)) != 0)
            return reply(out, n, 0, msg[0] ? msg : "autostart not available", NULL, NULL);
        return reply(out, n, 1, NULL, NULL, NULL);
    }
    if (!strcmp(path, "/api/stop")) {
        api->stop(api->user);
        return reply(out, n, 1, "stopping", NULL, NULL);
    }

    api->lock(api->user);
    const char *pid = kbm_query_get(q, "profile");
    if (!pid) pid = kbm_query_get(q, "id");

    if (!strcmp(path, "/api/profile_get")) {
        const KbmProfile *p = kbm_config_profile(c, pid);
        if (!p) { code = reply(out, n, 0, "unknown profile", NULL, NULL); goto done; }
        if (kbm_profile_json(p, out, n) < 0) { code = 500; reply(out, n, 0, "too large", NULL, NULL); goto done; }
        code = 200;
    } else if (!strcmp(path, "/api/lookup")) {
        /* what the database says about a Title ID (for the "remembered games" list) */
        const char *tid = kbm_query_get(q, "tid");
        KbmResolved r; kbm_config_resolve(c, tid ? tid : "", &r);
        KbmJson j; kbm_json_init(&j, out, n);
        kbm_json_fmt(&j, "{\"ok\":true,\"known\":%d,\"name\":", r.info.known); kbm_json_str(&j, r.info.name);
        kbm_json_raw(&j, ",\"profile\":"); kbm_json_str(&j, r.p->id);
        kbm_json_raw(&j, ",\"source\":"); kbm_json_str(&j, r.source);
        kbm_json_fmt(&j, ",\"native\":%d,\"native_suggested\":%d}", r.native, r.native_suggested);
        code = 200;
    } else if (!strcmp(path, "/api/profile")) {
        KbmProfile *p = kbm_config_profile(c, pid);
        if (!p) { code = reply(out, n, 0, "unknown profile", NULL, NULL); goto done; }
        get_int(q, "sens", &p->sens);
        get_int(q, "yratio", &p->yratio);
        get_int(q, "smooth_ms", &p->smooth_ms);
        get_int(q, "deadzone", &p->deadzone);
        get_int(q, "invert_y", &p->invert_y);
        get_int(q, "ads_pct", &p->ads_pct);
        kbm_profile_clamp(p);
        api->config_changed(api->user);
        code = reply(out, n, 1, NULL, NULL, NULL);
    } else if (!strcmp(path, "/api/bind")) {
        const char *act = kbm_query_get(q, "action"), *key = kbm_query_get(q, "key");
        int slot = 0; get_int(q, "slot", &slot);
        int k = kbm_key_from_name(key ? key : "");
        char moved[64];
        if (k < 0 || kbm_key_reserved((uint8_t)k)) { code = reply(out, n, 0, "invalid or reserved key", NULL, NULL); goto done; }
        if (kbm_config_bind(c, pid, act, slot, (uint8_t)k, moved, sizeof(moved)) != 0) {
            code = reply(out, n, 0, "unknown profile/action/slot", NULL, NULL); goto done;
        }
        api->config_changed(api->user);
        code = reply(out, n, 1, NULL, moved[0] ? "moved_from" : NULL, moved[0] ? moved : NULL);
    } else if (!strcmp(path, "/api/capture")) {
        const char *act = kbm_query_get(q, "action");
        int slot = 0; get_int(q, "slot", &slot);
        KbmProfile *p = kbm_config_profile(c, pid);
        if (!kbm_profile_action(p, act) || slot < 0 || slot > 1) { code = reply(out, n, 0, "unknown profile/action/slot", NULL, NULL); goto done; }
        if (!api->rt->receiver) { code = reply(out, n, 0, "receiver not captured (native mode?)", NULL, NULL); goto done; }
        KbmRuntime *r = api->rt;
        snprintf(r->cap_profile, sizeof(r->cap_profile), "%s", p->id);
        snprintf(r->cap_action, sizeof(r->cap_action), "%s", act);
        r->cap_slot = slot;
        r->cap_result[0] = 0;
        r->capture_active = 1;
        api->capture_start(api->user);
        code = reply(out, n, 1, "press the new key/button now", NULL, NULL);
    } else if (!strcmp(path, "/api/capture_cancel")) {
        if (api->rt->capture_active) {
            api->rt->capture_active = 0;
            snprintf(api->rt->cap_result, sizeof(api->rt->cap_result), "cancelled");
            api->capture_cancel(api->user);
        }
        code = reply(out, n, 1, NULL, NULL, NULL);
    } else if (!strcmp(path, "/api/title")) {
        const char *tid = kbm_query_get(q, "tid"), *target = kbm_query_get(q, "target");
        if (kbm_config_set_title(c, tid ? tid : "", target ? target : "") != 0) {
            code = reply(out, n, 0, "invalid title id or target", NULL, NULL); goto done;
        }
        api->config_changed(api->user);
        code = reply(out, n, 1, NULL, NULL, NULL);
    } else if (!strcmp(path, "/api/global")) {
        int v;
        if (get_int(q, "inject_hz", &v)) c->inject_hz = (v == 60 || v == 120 || v == 250) ? v : 0;
        if (get_int(q, "auto_activate", &v)) c->auto_activate = v ? 1 : 0;
        if (get_int(q, "release_on_exit", &v)) c->release_on_exit = v ? 1 : 0;
        if (get_int(q, "native_switch", &v)) c->native_switch = v ? 1 : 0;
        if (get_int(q, "auto_native", &v)) c->auto_native = v ? 1 : 0;
        if (get_int(q, "idle_exit_min", &v)) c->idle_exit_min = v < 0 ? 0 : (v > 240 ? 240 : v);
        if (get_int(q, "open_settings_on_launch", &v)) c->open_settings_on_launch = v ? 1 : 0;
        if (get_int(q, "pointer_sens", &v)) c->pointer_sens = v < 1 ? 1 : (v > 50 ? 50 : v);
        const char *dp = kbm_query_get(q, "default_profile");
        if (dp && kbm_valid_profile_id(dp)) snprintf(c->default_profile, sizeof(c->default_profile), "%s", dp);
        api->config_changed(api->user);
        code = reply(out, n, 1, NULL, NULL, NULL);
    } else if (!strcmp(path, "/api/pointer")) {
        int on = 0;
        get_int(q, "on", &on);
        if (api->set_pointer) api->set_pointer(api->user, on ? 1 : 0);
        code = reply(out, n, 1, NULL, NULL, NULL);
    } else if (!strcmp(path, "/api/reset")) {
        if (kbm_profile_reset(c, pid ? pid : "") != 0) { code = reply(out, n, 0, "unknown profile", NULL, NULL); goto done; }
        api->config_changed(api->user);
        code = reply(out, n, 1, NULL, NULL, NULL);
    } else {
        code = 404;
        reply(out, n, 0, "unknown endpoint", NULL, NULL);
    }
done:
    api->unlock(api->user);
    return code;
}
