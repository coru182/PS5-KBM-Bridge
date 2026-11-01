#!/usr/bin/env python3
"""Smoke-test patch anchors on an artificial minimal upstream source."""
from apply_to_ghostcontrol import patched_ghost_main, patched_makefile

sample = '''#include "controller_xbox.h"
static void inject_pad(int slot, const ScePadData *pad) { uint32_t n=0; if ((n % 600) == 0) {} (void)slot; (void)pad; }
/* ── ugen detection ───────────────────────────────────────────────────── */
int main(void) {
    /* Clean up any orphaned VDA devices */
    for (int dh=0; dh<64; dh++) { scePadVirtualDeviceDeleteDevice(dh); }

    /* Start klog capture thread first — must be running before any VDA call */
    /* Start controller manager — handles all detection, VDA creation, USB threads */
    pthread_t mgr_tid;
    if (pthread_create(&mgr_tid, NULL, controller_manager_thread, NULL)==0) {
        pthread_detach(mgr_tid);
    }
    /* Keep-alive */
    return 0;
}
'''
makefile = '''TARGET   := ghost-control-xbox-ps5.elf
CFLAGS  += -D__PROSPERO__ -Wall
LDFLAGS += -lScePad -lSceUserService -lpthread -ldl
SRC := gc_main.c shellui_pad.c usb_helpers.c controller_nintendo.c controller_xbox.c
'''
patched = patched_ghost_main(sample)
assert '#include "kbm_bridge.h"' in patched
assert patched.index('kbm_bridge_activation_requested()') < patched.index('create_vda_for_slot(0)')
assert 'kbm_bridge_shutdown()' in patched
assert 'controller_manager_thread(' not in patched
mk = patched_makefile(makefile)
assert 'TARGET   ?= ps5-kbm-bridge.elf' in mk
assert '$(KBM_CFLAGS)' in mk and 'kbm_usbfs.c' in mk
assert '#if KBM_DIAG_ONLY' in patched
assert '!KBM_DIAG_ONLY && !vda_created' in patched
assert 'idle_lim && kbm_bridge_idle_seconds() >= idle_lim' in patched
assert 'kbm_bridge_start(kbm_forward, notify)' in patched and 'kbm_bridge_set_vda(1)' in patched
assert '(n % 15000)' in patched
assert '-lSceSystemService' in mk and 'kbm_http.c' in mk
print('PASS: local Ghostcontrol patch anchors')
