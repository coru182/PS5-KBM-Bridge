# Historial de versiones

[English](CHANGELOG.md) · **Español**

## v0.10.1: GTA V se maneja como en PC

- **GTA V** ahora tiene las capas de teclas a pie / en vehículo (como el perfil de GTA Trilogy), con las teclas de vehículo de PC: W/S acelerar/frenar, Espacio freno de mano, clic derecho apunta desde el vehículo (L1), clic izquierdo dispara (L1+R1), E bocina, H luces, Q rueda de radios, `.` siguiente radio, R cámara cinemática. **F** cambia de modo al subir o bajar; **B** lo cambia a mano.
- Opción nueva en General: notificación de la PS5 al cambiar de modo (apagada por defecto).
- Los perfiles ahora pueden tener hasta 48 acciones.
- El bridge **no** lee la memoria del juego para saber si estás manejando: haría falta conocer direcciones de memoria por juego y por actualización, y funcionaría como una herramienta de trucos. El cambio sigue a la tecla F.

## v0.10: más cerca de nativo en todos los juegos

Cada juego trata el stick derecho a su manera: algunos ignoran los movimientos chicos (zona muerta) y aceleran la cámara de a poco. Por eso RE2 Remake se sentía nativo y RE Requiem o GTA V pesados con los mismos ajustes del bridge.

- **Medidor de zona muerta (F10, adentro del juego):** la cámara gira despacio a la derecha; hacé clic apenas empiece a moverse. El valor (menos ~240 ms de tiempo de reacción) se guarda como zona muerta del perfil activo. Esc cancela. F10 pasa a ser tecla reservada.
- **Compensación radial de la zona muerta:** los movimientos chicos del mouse saltan directo por encima de la zona muerta del juego sin cambiar de dirección (antes se compensaba cada eje por separado, lo que podía agregar deriva vertical).
- **Impulso a movimientos chicos** (0–100 %, por perfil): sube los valores chicos del stick para juegos con curva de respuesta suave/exponencial.
- **Consejos de ajustes del juego** en la página: RE Requiem (zona muerta del stick derecho 5–7, aceleración de cámara al máximo, curva Lineal) y GTA V (zona muerta de apuntado/vista al mínimo, aceleración de apuntado/vista al máximo), más una guía general para todos los perfiles.
- Los perfiles con zona muerta 0 e impulso 0 (lo que viene por defecto, por ejemplo RE2 Remake) se comportan exactamente igual que antes.

## v0.9.2: perfil para el menú de la PS5

- Nuevo **perfil "Menú de la PS5"**, que se usa solo mientras no hay ningún juego abierto (home, biblioteca, tienda, ajustes):
  - WASD o flechas = cruceta; Enter, F, E o clic izquierdo = ✕; Esc, Borrar o clic derecho = ○; Espacio = Options; Tab = △; Q = □; Z/X o los botones laterales del mouse = L1/R1; C/V = L2/R2; rueda del mouse = arriba/abajo; tecla Windows = botón PS.
  - **Mover el mouse mueve la selección de a un paso** (cruceta), en vez de la cámara. La velocidad es el control "Velocidad del cursor y del mouse en el menú" en General.
- Se edita como cualquier otro perfil ("Editar el perfil del menú" en la página cuando no hay juego abierto).
- Con un juego abierto (aunque vuelvas al home con el juego en segundo plano) sigue el perfil del juego.

## v0.9.1: GTA Trilogy y modo vehículo

- **Perfil nuevo: GTA: The Trilogy – Definitive Edition** (GTA III, Vice City y San Andreas, versiones de PS5 y PS4). Los botones de PS5 siguen el esquema Moderno. Las teclas de PC son las de la versión original de PC más los cambios de la Definitive Edition que aparecen en las fuentes (Tab = rueda de armas); no encontré una lista completa de teclas de PC de la Definitive Edition, así que alguna puede diferir.
- **Modo vehículo:** un perfil ahora puede asignar teclas distintas a pie y en un vehículo. En el perfil de GTA, W acelera (R2) en el auto y camina a pie, Espacio es freno de mano en vez de correr, etc. **F** (subir/bajar) cambia de modo; **B** lo cambia a mano si se desincroniza (por ejemplo, si apretás F sin un auto cerca). La página muestra el modo actual y agrupa las teclas en Siempre / A pie / En vehículo.
- Bicis y aviones usan otros botones en estos juegos y el perfil no los cubre.
- Al reasignar una tecla, solo se saca de las acciones que están activas al mismo tiempo (mismo modo).

## v0.9: español e inglés

- La página de ajustes y las notificaciones de la PS5 están en **español e inglés**. Por defecto siguen el idioma de la PS5 (se lee con `sceSystemServiceParamGetInt`; el resultado queda en el log). Si no se puede leer, la página usa el idioma del navegador y las notificaciones salen en inglés.
- Nuevo selector **Idioma / Language** en General: Automático, Español o English. Se guarda en `config.ini` (`lang=`).
- Las 520 acciones de los perfiles tienen su texto en inglés (`data/i18n/en.json`). Los nombres de teclas también cambian: en inglés se muestran con el teclado de EE. UU.
- El repo está en inglés por defecto, con versión en español de cada documento.

## v0.8: reconoce el juego y le pone su perfil solo

- **Base de títulos integrada:** unos 24.600 Title IDs de PS5 y PS4 (andshrew/PlayStation-Titles, licencia MIT). Cuando abrís un juego, el bridge lee su Title ID, busca el nombre y aplica el perfil que le corresponde. La notificación muestra el nombre del juego y el perfil elegido.
- **Perfiles por juego**, investigados con fuentes: cada uno traduce las **teclas por defecto de la versión de PC** al botón del DualSense que hace lo mismo en PS5. La lista con nivel de confianza y fuentes está en [`data/PROFILES.es.md`](data/PROFILES.es.md).
- **Plantillas por género** (shooter en primera persona, tercera persona con armas, acción/souls-like, carreras) para unos 280 juegos más que todavía no tienen perfil propio.
- **Prioridad:** lo que elegiste para ese juego → perfil del juego → plantilla de su género → perfil por defecto ★ (para juegos que no reconozco).
- **Teclado/mouse nativo:** si la investigación dice que la versión de PS5 soporta teclado y mouse (Fortnite, Call of Duty, Battlefield 6, FFXIV…), la página te lo **sugiere**; no lo activa sola salvo que marques la opción en "General".
- **Página de ajustes:** en "Juego actual" ves el nombre del juego y botones: Automático, Nativo, plantillas y "Otro juego…" (lista A–Z). El editor de perfiles ahora es una lista agrupada (En uso / Plantillas / Juegos).
- **Config:** `config.ini` solo guarda lo que cambiaste. Tu config de v0.7 sigue funcionando: se conservan los cambios de teclas de RE2, Requiem y Genérico.
- **Ojo:** si en v0.7 pusiste Requiem como perfil por defecto, los juegos que el bridge no reconoce van a usar Requiem. Para volver a Genérico: Perfil → Editar otro perfil… → Genérico → "Usar como perfil por defecto".

**42 juegos con perfil propio.** Seis tienen investigación incompleta y **no** vienen incluidos: AC Shadows, Kingdom Come Deliverance II, Far Cry 6, Marvel Rivals, Ratchet & Clank y Sniper Elite 5 (están en `data/unverified/`). Stellar Blade, Monster Hunter Wilds y Star Wars Jedi: Survivor no se llegaron a investigar. Esos 9 usan la plantilla de su género.

Agregar o corregir un perfil: editá o creá `data/profiles/<id>.json` y su patrón de nombre en `data/games.json`. `build_wsl.sh` corre `tools/gen_builtin.py`, que valida teclas, botones y tamaños y genera `src/kbm_builtin_gen.c`.


## v0.7.7: elegir el perfil del juego desde la PS5

- En "Juego actual", el desplegable se reemplazó por **botones** (Por defecto / RE2-RE4 / Requiem / Genérico / Nativo). En el navegador de la PS5 el desplegable no funcionaba: la página se refrescaba cada segundo y reemplazaba el desplegable mientras estaba abierto.
- Las partes de la página con las que se interactúa solo se redibujan cuando cambian, y la página no se refresca mientras hay un desplegable abierto.
- Las pestañas de "Perfil" dicen que solo eligen qué perfil **editás**. Al lado hay un botón **"Usar este perfil en <juego>"**.

**v0.7.6.1 (solo compilación):** las dependencias del ícono ahora se guardan una sola vez en `~/.cache/ps5-kbm/launcher-deps` y todas las versiones las reutilizan. Ya no hay que copiar `.deps` a mano. Si la descarga quedó incompleta, el script lo detecta y la vuelve a bajar. El bridge es el mismo que en v0.7.6.


## v0.7.6: ajustes en medio del juego, sin celular

- **F11** abre la página de ajustes en el navegador de la PS5 desde cualquier lugar, incluso con un juego abierto, y activa el modo puntero. Volvés con el **botón PS** (o la tecla Windows), que también devuelve el mouse a la cámara. Los cambios se aplican al instante.
- **No abras el ícono con un juego abierto:** la PS5 te pediría cerrar el juego. Dentro del juego usá F11.
- F11 queda reservada, igual que F8, F9 y F12.
- **No verificado en consola:** que el navegador se abra encima de un juego dejándolo en pausa. Si no se abre, el log registra el código de retorno de `sceSystemServiceLaunchWebBrowser`.


## v0.7.5

- **Modo puntero para el navegador de la PS5:** el mouse mueve el cursor (stick izquierdo), clic izquierdo = ✕, clic derecho = ○ y la rueda hace scroll (stick derecho). El teclado sigue igual.
  - Se activa solo cuando el bridge abre la página de ajustes desde el ícono.
  - Se desactiva con el **botón PS** (o tecla Windows), cuando se abre un juego, o con **F9**, que lo alterna a mano. También hay un botón en la página.
  - La velocidad del cursor se ajusta en General. F9 queda reservada, igual que F8 y F12.
- **Sensibilidad al apuntar (ADS):** un nuevo control por perfil, de 25 a 400 %, que se aplica mientras está apretada cualquier tecla que mande L2 (por defecto, clic derecho). El perfil de RE Requiem arranca en 150 % y los otros en 100 %. Si al 400 % el juego sigue lento, es que el juego limita el giro con el stick al máximo; en ese caso subí también la sensibilidad de apuntado dentro del juego.


## v0.7.4: el ícono abre los ajustes en la PS5

Al abrir **PS5 KBM Bridge** desde el home:

1. Si el bridge no está corriendo, la mini app se lo pasa a elfldr y arranca.
2. El bridge cierra la mini app y abre el **navegador de la PS5 en `http://127.0.0.1:8099/`**, la misma página que ves desde el celular, con `sceSystemServiceLaunchWebBrowser`. Es la llamada que usa el ejemplo `samples/browser` del PS5 Payload SDK.
3. Configurás con el DualSense, o con el teclado y el mouse una vez que el mando virtual está activo, y salís con el **botón PS** (o la tecla Windows).

En la tele la página se ve más grande y muestra cómo salir. Si no querés que se abra la página, desactivá en **General** la opción "Al abrir el ícono, mostrar esta página en la PS5".

**Para actualizar:** volvé a correr `./build_wsl.sh` y copiá otra vez `out/homebrew/PPSA51258/` a `/data/homebrew/PPSA51258/`. El bridge nuevo viaja dentro de esa carpeta.

**No verificado en consola:** la apertura del navegador desde el bridge. Si no se abre, el log muestra `sceSystemServiceLaunchWebBrowser(...) rc=0x...`; mandámelo.


## v0.7.3: ícono en el home (ShadowMountPlus)

La carpeta `launcher/` es una mini app nativa de PS5 (**título `PPSA51258`, "PS5 KBM Bridge"**). Cuando la abrís desde el home:

1. Si el bridge ya está corriendo, avisa y se cierra.
2. Si no, lee `ps5-kbm-bridge.elf`, que viaja dentro de la app, y se lo pasa a tu **elfldr** en `127.0.0.1:9021`. Es lo mismo que hacés hoy desde la PC.
3. El bridge, apenas arranca, ve la app lanzadora abierta y la cierra con `sceSystemServiceKillApp`, la misma llamada que usa websrv. Volvés al home y ya podés abrir el juego.

Si elfldr no está cargado, la app lo avisa con una notificación y queda abierta; la cerrás con el botón PS.

**Compilar el ícono (en tu WSL):**

```bash
sudo apt install -y clang-18 lld-18 ninja-build python3
./build_wsl.sh            # al final arma out/homebrew/PPSA51258/
```

La primera vez, `make deps` descarga el PS5 Payload SDK fijado por las herramientas, unos 350 MB, en `launcher/.deps/`.

**Instalar:** copiá la carpeta `out/homebrew/PPSA51258/` completa a **`/data/homebrew/PPSA51258/`** por FTP (podés usar ps5upload). ShadowMountPlus la agrega al home.

**Para actualizar el bridge más adelante:** recompilá y volvé a copiar la carpeta; el ELF va adentro de la app.

Herramientas de compilación: [ps5-native-app-boilerplate](https://github.com/blackbearreloaded/ps5-native-app-boilerplate) (BlackBearReloaded), tomadas de [ScreenTester5](https://github.com/Marice/ScreenTester5). Ver `launcher/THIRD_PARTY_NOTICES.md`.

**Verificado:** `main.c` compila con el PS5 SDK sin warnings y solo importa funciones de libkernel (`socket`, `connect`, `send`, `open`, `read`, `close`, `nanosleep`, `sceKernelSendNotificationRequest`). `param.json` pasa las reglas del script de compilación y el ícono es un PNG de 512×512. El bridge importa `sceSystemServiceKillApp`. **No verificado:** la compilación completa del ícono (en este entorno no se pudo ejecutar el toolchain externo) y su funcionamiento en la PS5.


## v0.7.2

- **Tecla Windows (izquierda o derecha) = botón PS** en los tres perfiles (bit `0x00010000`, el mismo que usa Ghostcontrol para el botón Guide de Xbox). Se puede reasignar como cualquier otra acción.
- **Diagnóstico de retomado:** cada vez que el bridge toma el receptor registra `TAKE n` con el estado del driver de la PS5 después del detach y el protocolo HID real (`GET_PROTOCOL`, 0 = boot). También registra la forma de los primeros 12 reportes de teclado y los primeros 5 de mouse. Es para investigar lo que se vio en el log de v0.7: después de F10 → retomado, el mouse no mandó reportes y el teclado no mandó ningún reporte de "teclas sueltas".
- **Autoarranque solo con etaHEN:** si no existe `/data/etaHEN`, la página ya no muestra esa opción. Con elfldr + ShadowMountPlus no aplica; la carpeta `out/etaHEN/` es solo para quien usa etaHEN.


## v0.7.1: arranca solo con etaHEN (solo si usás etaHEN)

- **Autoarranque:** copiá `out/etaHEN/payloads/` a **`/data/etaHEN/payloads/`** en la PS5 por FTP (el FTP de etaHEN usa el puerto 1337). Tienen que quedar dos archivos: `ps5-kbm-bridge.elf` y `ps5-kbm-bridge.elf.auto_start`, este último vacío. etaHEN arranca cualquier ELF de esa carpeta que tenga su `.auto_start` al lado, y también lo lista en Toolbox → Plugins/Payloads para lanzarlo a mano. Desde la página web podés prenderlo o apagarlo: la casilla crea o borra el `.auto_start`.
- **Modo servicio:** por defecto ya no se apaga por inactividad. Se puede volver a activar en General ("Apagar si no se usa").
- **Una sola instancia:** si ya hay un bridge corriendo, el segundo avisa y sale sin tocar nada.
- **Se quitó la extensión de websrv.** En la PS5 daba `TypeError: Load failed`, y con el autoarranque ya no hace falta. Los ajustes siguen en `http://IP-de-la-PS5:8099`.
- **Un ícono propio en el home no se puede hacer con un ELF.** Hace falta una app nativa de PS5 (`eboot.bin`) registrada por un loader como ShadowMountPlus, que es como lo hace, por ejemplo, RommPS. Además esa app se suspende al abrir un juego, así que solo serviría para lanzar el bridge. Con el autoarranque ese paso desaparece.


## v0.7

Teclado + mouse inalámbricos (receptor Logitech `046d:c534`) en PS5 11.60 + etaHEN, traducidos a un DualSense virtual de Ghostcontrol. Incluye una app companion para ajustar sensibilidad y teclas por juego.

**EXPERIMENTAL.** Compilado con PS5 Payload SDK v0.45. Las funciones nuevas de v0.7 **no se probaron en una consola**: hay que pasar primero por el ELF de diagnóstico.

### Qué hay de nuevo

| Pedido | Cómo quedó |
|---|---|
| Arrancar sin F8 | Se activa solo cuando teclado **y** mouse mandan datos válidos. Es la misma condición de seguridad que antes, pero sin apretar nada. F8 sigue andando si desactivás el arranque automático. |
| 120 Hz / auto 60–120 | **Inyección a 250 Hz por defecto**, la misma frecuencia de un DualSense por USB y la que usa Ghostcontrol con mandos reales. El mouse se convierte por **velocidad** (cuentas por ms en una ventana fija), así que el stick da el mismo valor a 60, 120 o 250 Hz y con juegos a 30, 60 o 120 fps. Medido en los tests: 48 / 48 / 48. Por eso no hace falta detectar la frecuencia del juego. Se puede fijar en 60, 120 o 250 desde la app. |
| Teclas como la versión de PC de RE | Perfiles **RE2/RE3/RE4 Remake** y **RE Requiem** con las teclas de PC por defecto, más uno **Genérico** con todos los botones. Todo se puede reasignar. |
| App companion | **Desde el celular o la PC:** `http://IP-de-la-PS5:8099`, con todos los ajustes. La extensión de websrv que había en v0.7 se quitó en v0.7.1. |
| Detectar juego nativo | El bridge lee el juego abierto con `sceSystemServiceGetAppIdOfRunningBigApp` / `GetAppTitleId`, las mismas llamadas que usan websrv y otros payloads de PS5. Cada juego se puede marcar como **Nativo** o asignarle un perfil, y queda recordado. En un juego nativo el bridge **devuelve el receptor a la PS5** (reset USB con `USB_DEVICEENUMERATE`) y lo retoma al salir. |

Otras mejoras:
- **F12** ahora también devuelve el teclado y el mouse a la PS5. Si funciona, ya no haría falta reiniciar después de jugar; se confirma con el diagnóstico.
- El apagado por inactividad, si lo activás, **no cuenta** el tiempo que pasás en un juego nativo.
- Los ajustes se guardan en `/data/ps5-kbm/config.ini`.

#### Teclas por defecto

**RE2 / RE3 / RE4 Remake** (teclas de PC → botones del layout tipo A):

| | | | |
|---|---|---|---|
| WASD / flechas → stick izq. | Mouse → stick der. | Clic der. → L2 (apuntar) | Clic izq. → R2 (atacar) |
| F / Enter → ✕ | R → □ (recargar) | Espacio → L1 (cuchillo) | E → R1 (munición) |
| Q → giro 180° (↓ + ○) | Esc / Borrar → ○ | Tab / I → △ (inventario) | M / Ctrl → touchpad (mapa) |
| P → Options | Clic rueda / Alt → R3 | Shift → L3 (correr) | 1–4 → cruceta ↑ → ↓ ← |

**RE Requiem:** WASD, mouse, clic der. L2, clic izq. R2, Espacio L1 (parry), F ✕, R □, Esc ○, Shift L3, Ctrl R3 (agacharse), X giro 180°, Q R1 (curarse), Tab △, 1/2/3 o rueda → cruceta ← ↓ →, G cruceta ↑ (linterna), M touchpad, P Options.

Las correspondencias de PS para RE2R salen de guías de controles de PS4. Si alguna acción no coincide en tu juego, reasignala desde la app.

## v0.6

- Arreglado el retraso de entrada de v0.5: `USB_FS_COMPLETE` saca de **una sola cola por descriptor** y devuelve el índice del endpoint que terminó. v0.5 asumía una cola por endpoint y mezclaba los reportes del teclado y del mouse. Ahora se vacía la cola hasta `EBUSY` y cada reporte va a su endpoint.
- Probado en consola con RE2 Remake y RE Requiem.

## v0.5 y anteriores

- Primeras versiones: lectura del receptor Logitech como teclado y mouse de arranque (boot protocol) y traducción a un DualSense virtual de Ghostcontrol. Tenían retraso de entrada y un corte a los 10 minutos (watchdog), corregidos en v0.6 y v0.7.

