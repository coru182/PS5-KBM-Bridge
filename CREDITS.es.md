# Créditos y código de terceros

[English](CREDITS.md) · **Español**

PS5 KBM Bridge se apoya en trabajo de otras personas. Esta es la lista completa de lo que se usa, de dónde sale y bajo qué licencia.

## Código que se usa para compilar

| Proyecto | Autor | Qué se usa | Cómo | Licencia |
|---|---|---|---|---|
| [Ghostcontrol – PS5 USB Controller Patcher](https://github.com/srbraboo/Ghostcontrol-PS5-USB-Controller-Patcher) | StonedModder ([srbraboo](https://github.com/srbraboo)) | La base del payload: el DualSense virtual (`scePadVirtualDeviceAddDevice` / `InsertData`), la asignación al usuario (`force_bind`), el registro en `/data/ghostpad/` y las notificaciones. | **No se incluye en este repo.** `build_wsl.sh` clona el repo original y `tools/apply_to_ghostcontrol.py` le agrega los archivos de `src/` y reemplaza el bucle principal de `gc_main.c` y la lista de fuentes del `Makefile`. El patch guarda copias `.kbm-backup` de los originales. | GPL-3.0-or-later |
| [ps5-native-app-boilerplate](https://github.com/blackbearreloaded/ps5-native-app-boilerplate) | BlackBearReloaded | Las herramientas para compilar, convertir y firmar la app del ícono: `launcher/tooling/`, `launcher/tools/`, `launcher/runtime/` y la base de `launcher/Makefile`. | Copiadas tal cual, con sus encabezados de copyright, desde la copia incluida en [ScreenTester5](https://github.com/Marice/ScreenTester5) (Marice). Ver [`launcher/THIRD_PARTY_NOTICES.md`](launcher/THIRD_PARTY_NOTICES.md). | GPL-3.0-or-later |
| [PS5 Payload SDK](https://github.com/ps5-payload-dev/sdk) | John Törnblom y colaboradores de ps5-payload-dev | Compilador, cabeceras (incluidas las de USB de FreeBSD) y bibliotecas para los dos ELF y el ícono. | Se instala aparte (`/opt/ps5-payload-sdk`); `make deps` del launcher baja una versión fija. No se incluye. | GPL-3.0-or-later |

## Datos incluidos

| Datos | Autor | Dónde | Licencia |
|---|---|---|---|
| [PlayStation-Titles](https://github.com/andshrew/PlayStation-Titles): lista de Title IDs y nombres de PS4/PS5 | andshrew | `data/titledb/*.tsv`, y compilada dentro del ELF por `tools/gen_builtin.py` | MIT, © 2022 andshrew ([`data/titledb/LICENSE-andshrew-PlayStation-Titles`](data/titledb/LICENSE-andshrew-PlayStation-Titles)) |
| Controles por defecto de PC y de PS5 de cada juego | Guías y wikis públicas (PCGamingWiki, manuales oficiales, game8, magicgameworld, shacknews, primagames, gamepressure, StrategyWiki y otras) | `data/profiles/*.json`: cada archivo lista sus fuentes en `sources`. Resumen en [`data/PROFILES.es.md`](data/PROFILES.es.md). | Se toman solo los datos (qué tecla hace qué), no textos de las guías |
| Qué juegos de PS5 soportan teclado y mouse | Push Square, Orzly, páginas oficiales (Epic, Blizzard, Ubisoft, EA), mp1st, Destructoid | `data/native.json`, con la fuente de cada juego | Solo datos |

## Referencias (consultadas, no copiadas)

Se leyeron para confirmar firmas de funciones y convenciones. No se copió código de ellos.

- **FreeBSD** `sys/dev/usb/usb_generic.c` y `usb_ioctl.h` (stable/11): cómo funcionan `USB_FS_COMPLETE`, `USB_FS_START` y `USB_DEVICEENUMERATE`. `tests/fake_usb_ioctl.h` repite la forma de las estructuras `usb_fs_*` para poder probar en una PC; los valores de los ioctl son inventados. FreeBSD usa licencia BSD-2-Clause.
- **websrv**, **presence5**, **CheatRunner** y **RommPS**: firmas de `sceSystemServiceGetAppIdOfRunningBigApp`, `sceSystemServiceGetAppTitleId` y `sceSystemServiceLaunchWebBrowser`.
- **etaHEN** (`PluginManager` de un fork): la convención de `/data/etaHEN/payloads/*.elf` + `.auto_start` para el arranque automático.
- **elfldr** (John Törnblom): el ícono le manda el ELF al puerto 9021.
- **ShadowMountPlus**: registra las apps de `/data/homebrew/<TITLEID>/` en el home.

## Proyecto

Idea, pruebas en consola y dirección: [coru182](https://github.com/coru182). Código escrito con ayuda de Claude (Anthropic).

Si sos autor de algo que se usa acá y falta o está mal acreditado, abrí un issue y lo corrijo.
