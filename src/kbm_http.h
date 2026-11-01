/* SPDX-License-Identifier: GPL-3.0-or-later
 * Minimal single-threaded HTTP/1.0 server for the settings page + JSON API.
 * Portable BSD sockets (runs on PS5 and on the Linux host for tests).
 */
#pragma once
#include "kbm_api.h"

#define KBM_HTTP_PORT 8099

/* Starts a detached thread. Returns 0 on success, -1 if bind/listen failed. */
int  kbm_http_start(int port, KbmApi *api);
void kbm_http_stop(void);
