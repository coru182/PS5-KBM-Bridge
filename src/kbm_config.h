/* SPDX-License-Identifier: GPL-3.0-or-later
 * Profiles (actions -> keys), per-title rules and global options.
 * Built-in profiles and the title database come from kbm_builtin_gen.c
 * (generated from data/ by tools/gen_builtin.py). The INI only stores what
 * the user changed. Pure C, host-tested.
 */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "kbm_engine.h"

#define KBM_MAX_ACTIONS  32
#define KBM_MAX_PROFILES 96
#define KBM_MAX_TITLES   64
#define KBM_ID_LEN       24

#define KBM_LANG_AUTO 0
#define KBM_LANG_ES   1
#define KBM_LANG_EN   2


typedef struct {
    char id[KBM_ID_LEN];
    const char *label, *label_en;   /* built-in strings (Spanish / English) */
    uint64_t out;
    uint8_t key[2];
    uint8_t layer;                  /* KBM_LAYER_FOOT / VEHICLE / BOTH */
} KbmAction;

typedef struct {
    char id[KBM_ID_LEN];
    const char *name, *name_en;     /* built-in strings (Spanish / English) */
    int sens, yratio, smooth_ms, deadzone, invert_y;
    int ads_pct;          /* sensitivity % while aiming (L2 held) */
    int builtin;          /* index into KBM_BUILTIN */
    int n;
    KbmAction a[KBM_MAX_ACTIONS];
} KbmProfile;

typedef struct {
    char tid[12];
    char target[KBM_ID_LEN];   /* "native" or a profile id */
} KbmTitleRule;

typedef struct {
    int inject_hz;        /* 0 = auto (250), or 60 / 120 / 250 */
    int auto_activate;    /* create the DualSense as soon as kb+mouse respond */
    int release_on_exit;  /* hand the receiver back to the PS5 on exit */
    int native_switch;    /* release/retake for titles marked "native" */
    int auto_native;      /* also release for games the database lists as native KB+M */
    int idle_exit_min;    /* exit after N min without input; 0 = never (service mode) */
    int open_settings_on_launch; /* home-screen icon opens the settings page in the PS5 browser */
    int pointer_sens;     /* cursor speed in pointer mode (PS5 browser), 1..50 */
    int lang;             /* KBM_LANG_AUTO (follow the PS5 system language) / ES / EN */
    char default_profile[KBM_ID_LEN];   /* for games the database doesn't know */
    int n_titles;
    KbmTitleRule titles[KBM_MAX_TITLES];
    int n_profiles;
    KbmProfile p[KBM_MAX_PROFILES];
} KbmConfig;

/* What the title database knows about a Title ID. */
typedef struct {
    int known;            /* Title ID is in the database */
    const char *name;     /* game name ("" if unknown) */
    int builtin;          /* KBM_BUILTIN index assigned to the game, -1 none */
    int native;           /* PS5 version supports KB+M natively */
} KbmTitleInfo;
int kbm_title_lookup(const char *tid, KbmTitleInfo *out);   /* returns known */

/* Which profile a running title gets, and why. */
typedef struct {
    const KbmProfile *p;  /* never NULL */
    const KbmProfile *auto_p; /* what "Automático" would pick (ignoring the user rule) */
    int native;           /* hand the receiver to the game (before native_switch) */
    int native_suggested; /* database says native KB+M, user hasn't decided */
    const char *source;   /* "rule" | "game" | "template" | "default" | "native" | "auto_native" | "none" */
    KbmTitleInfo info;
} KbmResolved;
void kbm_config_resolve(const KbmConfig *c, const char *tid, KbmResolved *r);

/* Effective language: config choice, else the PS5 system language
 * (sys = KBM_LANG_ES / KBM_LANG_EN, or KBM_LANG_AUTO if unknown -> English). */
int kbm_lang_effective(const KbmConfig *c, int sys);
const char *kbm_lang_code(int lang);      /* "auto" | "es" | "en" */
/* PS5 system language id (sceSystemServiceParamGetInt, param 1) -> ES / EN. */
int kbm_lang_from_system(int id);

void kbm_config_defaults(KbmConfig *c);
int  kbm_profile_reset(KbmConfig *c, const char *id);       /* 0 ok, -1 unknown */
int  kbm_profile_modified(const KbmProfile *p);             /* differs from built-in */
/* Parse INI text over the current contents (call defaults first). Returns
 * number of lines applied. Never fails hard: unknown lines are ignored. */
int  kbm_config_parse(KbmConfig *c, const char *text);
/* Serialize to INI (only modified profiles). Returns length or -1 if buf too small. */
int  kbm_config_write(const KbmConfig *c, char *buf, size_t n);
/* JSON for the web UI: globals, titles (with names), profile summaries and the
 * full detail of profile `edit` (NULL/unknown -> no detail). */
int  kbm_config_json(const KbmConfig *c, const char *edit, char *buf, size_t n);
/* Full detail of one profile as JSON. -1 unknown / too small. */
int  kbm_profile_json(const KbmProfile *p, char *buf, size_t n);

KbmProfile *kbm_config_profile(KbmConfig *c, const char *id);
const KbmProfile *kbm_config_profile_c(const KbmConfig *c, const char *id);
KbmAction  *kbm_profile_action(KbmProfile *p, const char *id);
const char *kbm_config_title_target(const KbmConfig *c, const char *tid);
int  kbm_config_set_title(KbmConfig *c, const char *tid, const char *target); /* "" / "auto" removes */
/* Shorthands over kbm_config_resolve(). */
const KbmProfile *kbm_config_profile_for(const KbmConfig *c, const char *tid);
int  kbm_config_title_is_native(const KbmConfig *c, const char *tid);
int  kbm_config_effective_hz(const KbmConfig *c);
void kbm_profile_to_mapping(const KbmProfile *p, KbmMapping *m);
void kbm_profile_clamp(KbmProfile *p);
int  kbm_profile_has_layers(const KbmProfile *p);   /* has vehicle-layer actions */

int  kbm_valid_title_id(const char *s);
int  kbm_valid_profile_id(const char *s);
/* "LS_DOWN+CIRCLE" */
const char *kbm_out_describe(uint64_t out, char *buf, size_t n);

/* tiny JSON helpers shared with the HTTP layer */
typedef struct { char *buf; size_t n, len; int overflow; } KbmJson;
void kbm_json_init(KbmJson *j, char *buf, size_t n);
void kbm_json_raw(KbmJson *j, const char *s);
void kbm_json_str(KbmJson *j, const char *s);   /* quoted + escaped */
void kbm_json_int(KbmJson *j, long long v);
void kbm_json_fmt(KbmJson *j, const char *fmt, ...);
