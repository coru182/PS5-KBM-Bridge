#!/usr/bin/env python3
"""Overlay PS5 KBM Bridge v0.8 on the srbraboo Ghostcontrol v5 sources.

No GitHub writes. Patch only a local clone. Fails closed if upstream layout differs.
The upstream repository (and third-party source) remain under GPL-3.0-or-later.
"""
from pathlib import Path
import argparse
import shutil
import sys

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"
FILE_NAMES = ["hid_boot.c", "hid_boot.h", "kbm_engine.c", "kbm_engine.h",
              "kbm_usbfs.c", "kbm_usbfs.h", "kbm_keys.c", "kbm_keys.h",
              "kbm_config.c", "kbm_config.h", "kbm_builtin_gen.c", "kbm_builtin_gen.h", "kbm_api.c", "kbm_api.h",
              "kbm_http.c", "kbm_http.h", "kbm_webui_gen.h",
              "kbm_bridge.c", "kbm_bridge.h"]


def replace_once(text: str, old: str, new: str, filename: str) -> str:
    n = text.count(old)
    if n != 1:
        raise ValueError(f"{filename}: expected 1 anchor, found {n}. "
                         "Upstream may have changed; no files have been modified.")
    return text.replace(old, new, 1)


def patched_ghost_main(main: str) -> str:
    if '"kbm_bridge.h"' in main:
        raise ValueError("gc_main.c already contains KBM bridge; refusing double patch")
    main = replace_once(main, 'if ((n % 600) == 0)', 'if ((n % 15000) == 0)', 'gc_main.c')
    main = replace_once(main, '#include "controller_xbox.h"',
                        '#include "controller_xbox.h"\n#include "kbm_bridge.h"',
                        'gc_main.c')
    marker = '/* ── ugen detection ───────────────────────────────────────────────────── */'
    callback = '''/* PS5-KBM-Bridge integration: one virtual DualSense, two USB HID inputs. */
static void kbm_forward(const ScePadData *pad) { inject_pad(0, pad); }

'''
    main = replace_once(main, marker, callback + marker, 'gc_main.c')
    # Never signal arbitrary PIDs from stale /data/ghostpad/gc_main.pid.
    old_start = main.find('    /* Kill previous instance */')
    old_end = main.find('    /* Init slots */', old_start)
    if old_start >= 0:
        if old_end < 0:
            raise ValueError('gc_main.c: PID cleanup anchor changed')
        main = main[:old_start] + '    /* No stale PID kill: fully reboot before every test. */\n\n' + main[old_end:]
    # Ghostcontrol's blanket DeleteDevice(0..63) can disrupt unrelated devices.
    cleanup_start = main.find('    /* Clean up any orphaned VDA devices */')
    cleanup_end = main.find('    /* Start klog capture thread first', cleanup_start)
    if cleanup_start < 0 or cleanup_end < 0:
        raise ValueError('gc_main.c: startup cleanup anchor changed')
    main = main[:cleanup_start] + '    /* Preserve other virtual controllers. */\n\n' + main[cleanup_end:]
    a = main.find('    /* Start controller manager — handles all detection, VDA creation, USB threads */')
    b = main.find('    /* Keep-alive */', a)
    if a < 0 or b < 0 or b <= a or 'controller_manager_thread' not in main[a:b]:
        raise ValueError("gc_main.c: controller manager anchor changed")
    replacement = '''    /* KBM v0.11. The DualSense is created only after the Logitech receiver has
     * produced BOTH keyboard and mouse input (automatically, or F8 if
     * auto-activation is off in the settings). -DKBM_DIAG_ONLY=1 never creates
     * one and exits after 5 minutes. Ghostcontrol's controller manager stays off. */
#ifndef KBM_DIAG_ONLY
#define KBM_DIAG_ONLY 0
#endif
#if KBM_DIAG_ONLY
#define KBM_TAG "KBM v0.11 DIAG"
#define KBM_DIAG_TICKS 1500      /* 5 min @ 200 ms */
#else
#define KBM_TAG "KBM v0.11"
#define KBM_DIAG_TICKS 0
#endif
/* Optional idle exit (web UI: General). Default 0 = never: when etaHEN
 * autostarts the bridge it stays running as a service. F12 / web UI stop anytime. */
    if (kbm_bridge_start(kbm_forward, notify) != 0) {
        gp_log("KBM: USB bridge startup failed\\n");
        return 1;
    }
#if KBM_DIAG_ONLY
    notify(kbm_tr(KBM_TAG ": mueve el mouse, manten W, F10 prueba liberar. F12 sale (5 min)",
                  KBM_TAG ": move the mouse, hold W, F10 = release test. F12 exits (5 min)"));
#else
    notify(kbm_tr(KBM_TAG ": mueve el mouse y toca una tecla para activar. F12 sale",
                  KBM_TAG ": move the mouse and press a key to start. F12 exits"));
#endif

    int vda_created = 0;
    const char *stop_reason = "unknown";
    for (int tick = 0; ; tick++) {
        if (kbm_bridge_stop_requested()) { stop_reason = "F12 or web UI"; break; }
        if (KBM_DIAG_ONLY && tick >= KBM_DIAG_TICKS) { stop_reason = "diag 5 min done"; break; }
        uint32_t idle_lim = kbm_bridge_idle_limit_s();
        if (idle_lim && kbm_bridge_idle_seconds() >= idle_lim) {
            stop_reason = "idle limit reached (no keyboard/mouse input)";
            break;
        }
        if (!KBM_DIAG_ONLY && !vda_created && kbm_bridge_activation_requested()) {
            gp_log("KBM: activation; creating virtual DualSense\\n");
            int32_t h = create_vda_for_slot(0);
            if (h >= 0) {
                g_slots[0].handle = h;
                g_slots[0].vdi_ready = 1;
                kbm_bridge_enable_injection();
                kbm_bridge_set_vda(1);
                vda_created = 1;
                notify(kbm_tr(KBM_TAG ": DualSense activo. Ajustes: F11 o http://IP-PS5:8099",
                                  KBM_TAG ": DualSense active. Settings: F11 or http://PS5-IP:8099"));
            } else {
                gp_log("KBM: VDA create failed\\n");
                break;
            }
        }
        if ((tick % 300) == 0)
            gp_log("KBM: alive tick=%d VDA=%d diag=%d idle_s=%u\\n", tick, vda_created,
                   KBM_DIAG_ONLY, kbm_bridge_idle_seconds());
        usleep(200000);
    }
    gp_log("KBM: shutting down, reason: %s\\n", stop_reason);
    kbm_bridge_shutdown();
    if (vda_created) {
        kbm_bridge_set_vda(0);
        int d = scePadVirtualDeviceDeleteDevice(g_slots[0].handle);
        gp_log("KBM: delete virtual device return 0x%08x\\n", (uint32_t)d);
        g_slots[0].handle = -1;
        g_slots[0].vdi_ready = 0;
    }
    gp_log("KBM: payload exit\\n");
    notify(kbm_tr(KBM_TAG " detenido", KBM_TAG " stopped"));
    return 0;

'''
    return main[:a] + replacement + main[b:]


def patched_makefile(mk: str) -> str:
    mk = replace_once(mk, 'TARGET   := ghost-control-xbox-ps5.elf',
                      'TARGET   ?= ps5-kbm-bridge.elf', 'Makefile')
    mk = replace_once(mk, 'CFLAGS  += -D__PROSPERO__',
                      'CFLAGS  += $(KBM_CFLAGS) -D__PROSPERO__', 'Makefile')
    mk = replace_once(mk,
            'SRC := gc_main.c shellui_pad.c usb_helpers.c controller_nintendo.c controller_xbox.c',
            'SRC := gc_main.c shellui_pad.c usb_helpers.c controller_nintendo.c controller_xbox.c kbm_engine.c hid_boot.c kbm_usbfs.c kbm_keys.c kbm_config.c kbm_builtin_gen.c kbm_api.c kbm_http.c kbm_bridge.c',
            'Makefile')
    mk = replace_once(mk, 'LDFLAGS += -lScePad -lSceUserService',
                      'LDFLAGS += -lScePad -lSceUserService -lSceSystemService', 'Makefile')
    return mk


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('ghostcontrol_dir', type=Path,
                    help='path to locally cloned srbraboo/Ghostcontrol-PS5-USB-Controller-Patcher')
    ap.add_argument('--reapply', action='store_true',
                    help='undo a previous KBM overlay (from the .kbm-backup files) and apply this version')
    args = ap.parse_args()
    payload = args.ghostcontrol_dir.resolve() / 'payload'
    if not payload.is_dir():
        ap.error(f"{payload} not found")
    gc = payload / 'gc_main.c'
    mf = payload / 'Makefile'
    if args.reapply:
        gb, mb = gc.with_suffix('.c.kbm-backup'), mf.with_name('Makefile.kbm-backup')
        if not (gb.exists() and mb.exists()):
            print("ERROR: --reapply needs gc_main.c.kbm-backup and Makefile.kbm-backup", file=sys.stderr)
            return 1
        shutil.copy2(gb, gc); shutil.copy2(mb, mf); gb.unlink(); mb.unlink()
        for f in list(payload.glob('kbm_*')) + list(payload.glob('hid_boot.*')):
            f.unlink()
        for f in payload.glob('*.o'):
            f.unlink()
        print("previous KBM overlay removed")
    try:
        old_gc = gc.read_text(encoding='utf-8')
        old_mf = mf.read_text(encoding='utf-8')
        new_gc = patched_ghost_main(old_gc)
        new_mf = patched_makefile(old_mf)
        for name in FILE_NAMES:
            if (payload / name).exists():
                raise ValueError(f"{name}: already exists in upstream clone")
        if (gc.with_suffix('.c.kbm-backup').exists() or mf.with_name('Makefile.kbm-backup').exists()):
            raise ValueError('existing KBM backups detected; refusing to overwrite')
    except (ValueError, OSError) as e:
        print(f"ERROR: {e}", file=sys.stderr)
        return 1
    # Only modify filesystem once all checks pass.
    shutil.copy2(gc, gc.with_suffix('.c.kbm-backup'))
    shutil.copy2(mf, mf.with_name('Makefile.kbm-backup'))
    for name in FILE_NAMES:
        shutil.copy2(SRC / name, payload / name)
    gc.write_text(new_gc, encoding='utf-8')
    mf.write_text(new_mf, encoding='utf-8')
    print(f"KBM overlay applied to: {payload}")
    print("Build in WSL/Linux with: make -C payload clean all PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk")
    return 0


if __name__ == '__main__':
    sys.exit(main())
