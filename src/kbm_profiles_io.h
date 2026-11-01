/* SPDX-License-Identifier: GPL-3.0-or-later
 * Custom profile files: <dir>/<id>.json (default /data/ps5-kbm/profiles).
 * Used by the bridge and the host test server.
 */
#pragma once
#include <stddef.h>
#include "kbm_config.h"

typedef void (*kbm_log_fn)(void *user, const char *msg);

/* Load every *.json in dir (call before the INI, so INI tweaks apply on top).
 * Returns how many loaded; each result is reported through log (may be NULL). */
int kbm_profiles_load_dir(KbmConfig *c, const char *dir, kbm_log_fn log, void *user);
/* Write <dir>/<id>.json atomically (creates dir). 0 ok, -1 error. */
int kbm_profile_file_save(const char *dir, const char *id, const char *data, size_t len);
/* Remove <dir>/<id>.json. 0 ok or not there, -1 error. */
int kbm_profile_file_delete(const char *dir, const char *id);
