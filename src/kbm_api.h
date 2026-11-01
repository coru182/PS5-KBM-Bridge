/* SPDX-License-Identifier: GPL-3.0-or-later
 * Web/companion API: state JSON + commands. Portable (host-tested); the PS5
 * bridge supplies the callbacks and the lock.
 */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "kbm_config.h"

typedef struct {
    int n;
    char k[16][24];
    char v[16][128];
} KbmQuery;

void kbm_query_parse(const char *qs, KbmQuery *q);   /* URL-decodes */
const char *kbm_query_get(const KbmQuery *q, const char *k);

typedef struct {
    const char *version;
    int diag;
    char status[24];        /* waiting | active | native | released | stopped */
    int vda;                /* virtual DualSense exists */
    int receiver;           /* receiver currently captured by the bridge */
    char title[12];         /* running game title id, "" = none */
    char title_name[96];    /* from the built-in title database, "" = unknown */
    char title_source[16];  /* why this profile: rule|game|template|default|native|auto_native|none */
    int native_suggested;   /* database: this game supports KB+M on PS5 */
    int has_layers;         /* active profile has on-foot / vehicle layers */
    int layer;              /* current layer: 0 on foot, 1 vehicle */
    int mouse_dpad;         /* active profile: mouse moves the D-pad (PS5 menu) */
    int overlay;            /* PS5 menu on screen during a game: menu profile */
    int focus_auto;         /* game/menu switch follows the PS5 focus (else the PS key) */
    char profile[KBM_ID_LEN];
    char auto_profile[KBM_ID_LEN];  /* what "Automático" picks for this title */
    int hz;                 /* effective inject rate */
    uint32_t kb_reports_5s, mouse_reports_5s;
    uint32_t e2i_p50_us, e2i_p95_us;
    uint32_t uptime_s, idle_s;
    char ip[48];
    int capture_active;
    char cap_profile[KBM_ID_LEN], cap_action[KBM_ID_LEN];
    int cap_slot;
    char cap_result[64];    /* "R", or "timeout"/"cancelled" */
    int autostart_avail;    /* /data/etaHEN/payloads/ps5-kbm-bridge.elf exists */
    int autostart_on;       /* ...and its .auto_start marker exists */
    int etahen;             /* /data/etaHEN exists (autostart only applies to etaHEN) */
    int pointer_mode;       /* mouse drives the PS5 browser cursor */
    int sys_lang;           /* PS5 system language: KBM_LANG_ES / EN, AUTO = unknown */
    int lang;               /* effective language (config, else system) */
} KbmRuntime;

typedef struct {
    KbmConfig *cfg;
    KbmRuntime *rt;
    void *user;
    void (*lock)(void *user);
    void (*unlock)(void *user);
    void (*config_changed)(void *user);                 /* lock held */
    void (*capture_start)(void *user);                  /* lock held; rt->cap_* already set */
    void (*capture_cancel)(void *user);                 /* lock held */
    void (*stop)(void *user);                           /* lock NOT held */
    /* create/remove the etaHEN .auto_start marker. 0 ok, -1 + msg. lock NOT held */
    int  (*set_autostart)(void *user, int on, char *msg, size_t n);
    void (*set_pointer)(void *user, int on);             /* lock held */
    void (*set_layer)(void *user, int layer);            /* lock held: 0 on foot, 1 vehicle */
    const char *profiles_dir;   /* custom profile files; NULL = import/delete not saved to disk */
    void (*retake)(void *user);                         /* drop and take keyboard/mouse again. lock NOT held */
    /* system part of the diagnostics report (firmware, devices, log tail). lock NOT held; returns length */
    int  (*diag_extra)(void *user, char *buf, size_t n);
} KbmApi;

/* Full state: {"runtime":{...},"config":{...},"keys":[...]}. Takes the lock.
 * config.edit holds the full detail of profile `edit` (NULL -> runtime profile). */
int kbm_api_state_json(KbmApi *api, const char *edit, char *buf, size_t n);
/* Fill title/name/source/native_suggested/profile from the resolver. Lock held. */
void kbm_runtime_set_title(KbmRuntime *rt, const KbmConfig *c, const char *tid);
/* path like "/api/bind". Writes a JSON reply into out. Returns HTTP status. */
int kbm_api_command(KbmApi *api, const char *path, const KbmQuery *q, char *out, size_t n);
/* POST /api/import: body = a profile JSON (export format). Saves it to
 * profiles_dir and applies it. Reply {"ok":true,"id":...} or an error. */
int kbm_api_import(KbmApi *api, const char *body, size_t len, char *out, size_t n);

/* GET /api/diag: plain-text report to attach to a bug report (no keystrokes). Returns length. */
int kbm_api_diag(KbmApi *api, char *buf, size_t n);

/* Assign key code to profile/action/slot. Removes the same key from other
 * actions of that profile (writes that action's id into moved_from). 0 ok, -1 err. */
int kbm_config_bind(KbmConfig *c, const char *profile, const char *action, int slot,
                    uint8_t code, char *moved_from, size_t mf_n);
