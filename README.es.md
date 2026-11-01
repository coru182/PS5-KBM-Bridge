# PS5 KBM Bridge

[English](README.md) · **Español**

Teclado y mouse en cualquier juego de PS5, sin adaptadores ni PC. Un payload para PS5 con jailbreak que lee un teclado y un mouse USB y los convierte en un DualSense virtual, con perfiles que imitan las teclas de la versión de PC de cada juego.

> [!WARNING]
> **Experimental.** Necesita una PS5 con jailbreak. Lo usás bajo tu responsabilidad.
> - Solo traduce entradas: no modifica juegos ni inyecta trucos.
> - Algunos juegos online prohíben o penalizan los adaptadores de teclado y mouse (Marvel Rivals, Rainbow Six Siege, entre otros). Pensado para jugar offline.

## Qué hace

- **DualSense virtual** a partir del receptor inalámbrico del teclado y mouse, sobre el DualSense virtual de Ghostcontrol. El mouse mueve el stick derecho por velocidad: la sensación es la misma a 30, 60 o 120 fps. Inyecta a 250 Hz.
- **Reconoce el juego** que abrís por su Title ID. Trae una base de unos 24.600 títulos de PS5 y PS4, así que conoce el nombre del juego.
- **Perfil por juego:** 42 juegos con las teclas por defecto de su versión de PC, traducidas al botón que hace lo mismo en PS5. Unos 290 juegos más usan una plantilla por género (shooter, tercera persona, souls-like, carreras). Lista completa con fuentes: [`data/PROFILES.es.md`](data/PROFILES.es.md).
- **Página de ajustes** en `http://IP-de-la-PS5:8099`, desde el celular, la PC o el propio navegador de la PS5:
  - sensibilidad, sensibilidad al apuntar (ADS) y asignación de teclas con captura;
  - qué perfil usa cada juego, y marcar un juego como "nativo".
- **Desde la PS5, sin celular:**
  - un ícono en el home (vía ShadowMountPlus) abre los ajustes;
  - **F11** los abre en medio de un juego.
  - En esa página, el mouse maneja el cursor.
- **En español o en inglés:** la página de ajustes y las notificaciones usan el idioma de tu PS5. Se puede fijar a mano en General.
- **Juegos con teclado y mouse propios** (Fortnite, Call of Duty, Battlefield 6…): te sugiere devolverle el receptor a la PS5 mientras jugás.

### Teclas reservadas

| Tecla | Qué hace |
|---|---|
| Windows | Botón PS |
| F8 | Activar el mando a mano (si desactivaste el arranque automático) |
| F9 | Mouse como cursor / como cámara |
| F10 | Solo en el ELF de diagnóstico: prueba de devolver el receptor a la PS5 |
| F11 | Abrir los ajustes en el navegador de la PS5 |
| F12 | Detener el bridge |

## Requisitos

- **PS5 con jailbreak.** Probado en firmware **11.60** con elfldr, kstuff, ShadowMountPlus y backpork. Con etaHEN también puede arrancar solo al iniciar.
- **Receptor inalámbrico Logitech `046d:c534`** (teclado y mouse por el mismo receptor). Es el único probado y el código lo busca por ese VID:PID. Otros receptores con protocolo de arranque HID podrían funcionar cambiando `TARGET_VID`/`TARGET_PID` en `src/kbm_bridge.c`, pero no está probado.
- **Para compilar:** Linux o WSL (Ubuntu) con `git`, `python3`, `make`, `gcc` y el [PS5 Payload SDK](https://github.com/ps5-payload-dev/sdk) en `/opt/ps5-payload-sdk`. Para las pruebas de la página, Node y Playwright (opcional).

## Compilar

```bash
git clone https://github.com/coru182/ps5-kbm-bridge.git
cd ps5-kbm-bridge
export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
./build_wsl.sh
```

El script hace todo:

1. Genera los perfiles, la base de títulos y la página embebida.
2. Corre las pruebas.
3. Clona Ghostcontrol y le aplica el patch.
4. Compila los dos ELF.
5. Arma la app del ícono. La primera vez baja sus dependencias a `~/.cache/ps5-kbm/`.

Resultado en `out/`:

| Archivo | Para qué |
|---|---|
| `ps5-kbm-bridge-v09.elf` | El bridge |
| `ps5-kbm-diag-v09.elf` | Diagnóstico: solo lee y mide, **nunca** crea el mando virtual, se cierra a los 5 min |
| `homebrew/PPSA51258/` | Ícono para el home (ShadowMountPlus) |
| `etaHEN/payloads/` | Arranque automático, solo si usás etaHEN |

`KBM_SKIP_LAUNCHER=1 ./build_wsl.sh` compila sin el ícono.

## Instalar y usar

1. Copiá `out/homebrew/PPSA51258/` a `/data/homebrew/PPSA51258/` en la PS5 por FTP (por ejemplo con ps5upload). Reiniciá y cargá tus payloads; ShadowMountPlus agrega el ícono **PS5 KBM Bridge** al home.
   - Sin ShadowMountPlus: mandá `ps5-kbm-bridge-v09.elf` a elfldr (puerto 9021).
2. Conectá el receptor y abrí el ícono. Se abre la página de ajustes. Mové el mouse y tocá una tecla: aparece el DualSense virtual.
3. Abrí un juego. La notificación dice qué juego reconoció y qué perfil aplicó. Para cambiar algo en medio del juego: **F11**. Volvés con el botón PS (o la tecla Windows).

Los ajustes se guardan en `/data/ps5-kbm/config.ini` (solo lo que cambiaste). El registro de diagnóstico queda en `/data/ghostpad/gc_status.log`.

**Primera vez en una consola nueva:** probá antes el ELF de diagnóstico.
1. Mové el mouse y tocá teclas.
2. Abrí un juego y volvé al home: el log tiene que mostrar `GAME PPSAxxxxx`.
3. Apretá F10: el receptor vuelve a la PS5 por 20 s y después se retoma.

### Cómo elige el perfil

Por cada juego que abrís, en este orden:

1. lo que elegiste vos para ese juego en la página;
2. el perfil propio del juego;
3. la plantilla de su género;
4. el perfil por defecto ★ (para juegos que no reconoce).

Si la base dice que el juego soporta teclado y mouse en PS5, te lo sugiere. Solo lo pasa a "nativo" sin preguntar si activás esa opción.

## Agregar o corregir un perfil

1. Creá o editá `data/profiles/<id>.json`, con las teclas de PC por defecto y el botón de PS5 de cada acción. Mirá los existentes como ejemplo.
   - No incluyas movimiento ni cámara: WASD, flechas, mouse y la tecla Windows (botón PS) se agregan solos.
2. Agregá el nombre del juego en `data/games.json`. Es una expresión regular sobre el nombre normalizado: minúsculas, sin acentos ni ™.
3. Agregá la traducción al inglés de cada acción nueva en `data/i18n/en.json`.
4. Corré `./build_wsl.sh`. `tools/gen_builtin.py` revisa cada tecla y botón (y que haya traducción), y frena si algo no es válido. También regenera `data/PROFILES.md` y `data/PROFILES.es.md`.

Perfiles de otros jugadores son bienvenidos (issues o pull requests), mejor con fuentes. Los que tienen investigación incompleta están en `data/unverified/` y no se incluyen.

## Estructura

| Carpeta | Contenido |
|---|---|
| `src/` | El bridge en C: lectura USB (`kbm_usbfs`), motor teclado/mouse → mando (`kbm_engine`), perfiles y base de títulos (`kbm_config`), servidor HTTP y API (`kbm_http`, `kbm_api`), hilos y ciclo de vida (`kbm_bridge`) |
| `web/` | La página de ajustes, embebida en el ELF por `tools/embed_web.py` |
| `data/` | Perfiles, plantillas, patrones de nombres, juegos con teclado/mouse nativo y la base de títulos |
| `launcher/` | La app del ícono (PPSA51258): le manda el ELF a elfldr y abre los ajustes |
| `tools/` | Generadores y el patch para Ghostcontrol |
| `tests/` | Pruebas en PC: motor, configuración, USB simulado, API HTTP real y la página en Chromium |

## Qué está probado

| | Estado |
|---|---|
| Lectura USB, motor, mouse independiente de la frecuencia | Probado en consola (v0.6–v0.7.7) con RE2 Remake y RE Requiem |
| Ícono, página de ajustes en la PS5, F11 en medio del juego, botón PS con la tecla Windows | Probado en consola (v0.7.x) |
| Detección del juego por Title ID | Probado en consola (v0.7) |
| Elección automática del perfil y base de títulos (v0.8) | Pruebas en PC. **Sin probar en consola** |
| Idioma de la PS5 (v0.9) | Se lee con `sceSystemServiceParamGetInt` y queda en el log (`system language: ...`). **Sin probar en consola**; si falla, la página usa el idioma del navegador y las notificaciones salen en inglés. Se puede fijar en General |
| Teclas de los perfiles por juego | Validadas contra sus fuentes, **no probadas jugando**. Las de confianza "media" tienen partes deducidas |
| Devolver el receptor a la PS5 y retomarlo (juegos "nativos") | Devolverlo funciona. **Al retomarlo hubo un caso en que el mouse dejó de responder: está en revisión** |
| Arranque automático con etaHEN | Sin probar |

**Seguridad:** la página en el puerto 8099 no tiene contraseña. Cualquiera en tu red local puede cambiar ajustes o detener el bridge. Bien para una red de casa, no para una red pública.

## Créditos y licencia

GPL-3.0-or-later (ver [`LICENSE`](LICENSE)).

- [**Ghostcontrol**](https://github.com/srbraboo/Ghostcontrol-PS5-USB-Controller-Patcher), de StonedModder (srbraboo, GPL-3.0-or-later): este proyecto está construido sobre su DualSense virtual. Su código no está en este repo: se baja del original al compilar.
- [**ps5-native-app-boilerplate**](https://github.com/blackbearreloaded/ps5-native-app-boilerplate), de BlackBearReloaded (vía [ScreenTester5](https://github.com/Marice/ScreenTester5)): herramientas del ícono.
- [**PS5 Payload SDK**](https://github.com/ps5-payload-dev/sdk), de ps5-payload-dev.
- [**PlayStation-Titles**](https://github.com/andshrew/PlayStation-Titles), de andshrew (MIT): base de títulos.

El detalle completo, con referencias y fuentes de cada perfil, está en [`CREDITS.es.md`](CREDITS.es.md). Historial de versiones en [`CHANGELOG.es.md`](CHANGELOG.es.md).
