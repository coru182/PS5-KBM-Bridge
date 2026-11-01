/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <stdint.h>
#include "gc_types.h"
typedef void (*kbm_inject_cb)(const ScePadData *pad);
typedef void (*kbm_notify_cb)(const char *fmt, ...);
/* Does NOT create the virtual controller: reads the Logitech receiver first.
 * Also starts the settings server on :8099 and the running-game watcher. */
int kbm_bridge_start(kbm_inject_cb inject, kbm_notify_cb notify);
/* True once kb+mouse produced input and (auto_activate or F8). Never in DIAG. */
int kbm_bridge_activation_requested(void);
/* F12, /api/stop. */
int kbm_bridge_stop_requested(void);
/* Seconds without keyboard/mouse input while the bridge holds the receiver.
 * Time spent in a native game (receiver handed back) does not count. */
uint32_t kbm_bridge_idle_seconds(void);
/* Configured idle-exit limit in seconds; 0 = never exit on idle. */
uint32_t kbm_bridge_idle_limit_s(void);
void kbm_bridge_enable_injection(void);
void kbm_bridge_set_vda(int created);
/* Stops threads; hands the receiver back if release_on_exit. Blocks <= 3 s. */
void kbm_bridge_shutdown(void);
