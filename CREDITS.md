# Credits and third-party code

**English** · [Español](CREDITS.es.md)

PS5 KBM Bridge relies on other people's work. This is the full list of what is used, where it comes from and under which license.

## Code used to build

| Project | Author | What is used | How | License |
|---|---|---|---|---|
| [Ghostcontrol – PS5 USB Controller Patcher](https://github.com/srbraboo/Ghostcontrol-PS5-USB-Controller-Patcher) | StonedModder ([srbraboo](https://github.com/srbraboo)) | The payload's base: the virtual DualSense (`scePadVirtualDeviceAddDevice` / `InsertData`), user assignment (`force_bind`), logging to `/data/ghostpad/`, notifications, and (since v0.14) the Xbox One controller support from `controller_xbox.c` (GIP handshake and button mapping), called by the bridge. | **Not included in this repo.** `build_wsl.sh` clones the original repo and `tools/apply_to_ghostcontrol.py` adds the files from `src/` and replaces the main loop of `gc_main.c` and the source list of the `Makefile`. The patch keeps `.kbm-backup` copies of the originals. | GPL-3.0-or-later |
| [ps5-native-app-boilerplate](https://github.com/blackbearreloaded/ps5-native-app-boilerplate) | BlackBearReloaded | The tools that build, convert and sign the icon app: `launcher/tooling/`, `launcher/tools/`, `launcher/runtime/` and the base of `launcher/Makefile`. | Copied as-is, with their copyright headers, from the copy vendored in [ScreenTester5](https://github.com/Marice/ScreenTester5) (Marice). See [`launcher/THIRD_PARTY_NOTICES.md`](launcher/THIRD_PARTY_NOTICES.md). | GPL-3.0-or-later |
| [PS5 Payload SDK](https://github.com/ps5-payload-dev/sdk) | John Törnblom and the ps5-payload-dev contributors | Compiler, headers (including FreeBSD's USB headers) and libraries for both ELFs and the icon. | Installed separately (`/opt/ps5-payload-sdk`); the launcher's `make deps` downloads a pinned version. Not included. | GPL-3.0-or-later |

## Included data

| Data | Author | Where | License |
|---|---|---|---|
| [PlayStation-Titles](https://github.com/andshrew/PlayStation-Titles): PS4/PS5 Title IDs and names | andshrew | `data/titledb/*.tsv`, compiled into the ELF by `tools/gen_builtin.py` | MIT, © 2022 andshrew ([`data/titledb/LICENSE-andshrew-PlayStation-Titles`](data/titledb/LICENSE-andshrew-PlayStation-Titles)) |
| Default PC and PS5 controls of each game | Public guides and wikis (PCGamingWiki, official manuals, game8, magicgameworld, shacknews, primagames, gamepressure, StrategyWiki and others) | `data/profiles/*.json`: each file lists its sources under `sources`. Summary in [`data/PROFILES.md`](data/PROFILES.md). | Only the facts (which key does what) are used, not text from the guides |
| Which PS5 games support keyboard and mouse | Push Square, Orzly, official pages (Epic, Blizzard, Ubisoft, EA), mp1st, Destructoid | `data/native.json`, with the source for each game | Facts only |

## References (read, not copied)

Read to confirm function signatures and conventions. No code was copied from them.

- **FreeBSD** `sys/dev/usb/usb_generic.c` and `usb_ioctl.h` (stable/11): how `USB_FS_COMPLETE`, `USB_FS_START` and `USB_DEVICEENUMERATE` work. `tests/fake_usb_ioctl.h` repeats the shape of the `usb_fs_*` structures so the code can be tested on a PC; the ioctl values are made up. FreeBSD is BSD-2-Clause licensed.
- **websrv**, **presence5**, **CheatRunner** and **RommPS**: signatures of `sceSystemServiceGetAppIdOfRunningBigApp`, `sceSystemServiceGetAppTitleId` and `sceSystemServiceLaunchWebBrowser`.
- **etaHEN** (`PluginManager` in a fork): the `/data/etaHEN/payloads/*.elf` + `.auto_start` convention for automatic start.
- **elfldr** (John Törnblom): the icon sends the ELF to port 9021.
- **ShadowMountPlus**: registers the apps in `/data/homebrew/<TITLEID>/` on the home screen.

## Project

Idea, console testing and direction: [coru182](https://github.com/coru182). Code written with help from Claude (Anthropic).

If you wrote something used here and it's missing or wrongly credited, please open an issue and I'll fix it.
