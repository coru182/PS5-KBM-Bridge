# Perfiles propios

[English](custom-profiles.md) · **Español**

Podés agregar o reemplazar perfiles sin recompilar el bridge. En la página de ajustes (F11, o `http://IP-DE-LA-PS5:8099`), la sección **Compartir e importar perfiles** tiene:

- **Exportar**: el perfil que estás editando, en JSON, con sus teclas y ajustes actuales y los juegos a los que se lo asignaste. Descargalo o copialo.
- **Importar**: pegá un perfil o elegí un archivo `.json`. El bridge lo valida, lo aplica al instante y lo guarda como `/data/ps5-kbm/profiles/<id>.json`. Se vuelve a cargar en cada arranque.
- **Borrar**: solo para perfiles importados. Si reemplazaba uno de fábrica, vuelve el de fábrica.

Lo más fácil para hacer uno: exportá un perfil parecido, cambialo en un editor de texto e importalo.

## Formato

```json
{
  "id": "mi_juego",
  "name": "Mi juego",
  "name_en": "My game",
  "kind": "game",
  "title_ids": ["PPSA01234", "PPSA05678"],
  "mouse": "camera",
  "sens": 8, "yratio": 100, "smooth_ms": 16, "deadzone": 0, "curve": 0, "invert_y": 0, "ads_pct": 100,
  "tips": {"es": "Ajustes del juego…", "en": "In-game settings…"},
  "actions": [
    {"id": "jump",  "label": "Saltar",   "label_en": "Jump",  "ps5": "CROSS",           "pc": ["SPACE", ""]},
    {"id": "aim",   "label": "Apuntar",  "label_en": "Aim",   "ps5": "L2",              "pc": ["MOUSE2", ""]},
    {"id": "drive", "label": "Acelerar", "label_en": "Accelerate", "ps5": "R2",         "pc": ["W", ""], "layer": "vehicle"},
    {"id": "enter", "label": "Subir / bajar", "label_en": "Get in / out", "ps5": "TRIANGLE+MODE_TOGGLE", "pc": ["F", ""], "layer": "both"}
  ]
}
```

| Campo | Qué es |
|---|---|
| `id` | Obligatorio. De 1 a 23 caracteres: `a-z`, `0-9`, `_`. El id de un perfil de fábrica (ver [`data/PROFILES.es.md`](../data/PROFILES.es.md)) lo reemplaza. |
| `name`, `name_en` | Nombre en español / inglés. |
| `kind` | `game` (por defecto), `template` o `menu`. Solo cambia el grupo en la página. |
| `title_ids` | Hasta 8 Title IDs (`PPSA01234`, `CUSA01234`). El perfil se usa solo en esos juegos, salvo que en la página hayas elegido otro perfil para el juego. |
| `mouse` | `camera` (mouse → stick derecho, por defecto) o `dpad` (el mouse mueve la cruceta, para menús). |
| `sens` … `ads_pct` | Los mismos ajustes que los deslizadores. Si falta, valor por defecto. |
| `actions` | Hasta 64. Cada una: `ps5` = botones unidos con `+`; `pc` = hasta dos teclas; `layer` = `foot` (por defecto), `vehicle`, `bike`, `air` o `both` (todos los modos). |

Botones de PS5: `CROSS CIRCLE SQUARE TRIANGLE L1 R1 L2 R2 L3 R3 OPTIONS TOUCHPAD PS DPAD_UP DPAD_DOWN DPAD_LEFT DPAD_RIGHT LS_UP LS_DOWN LS_LEFT LS_RIGHT`, más `MODE_TOGGLE` (a pie ↔ auto) y `MODE_CYCLE` (a pie → auto → bici → aire).

Teclas: los nombres de las listas de teclas de la página (`A`…`Z`, `0`…`9`, `SPACE`, `LSHIFT`, `LCTRL`, `TAB`, `ENTER`, `ESC`, `KP8`, `MOUSE1`…`MOUSE5`, `WHEEL_UP`, `WHEEL_DOWN`…). F8–F12 están reservadas por el bridge.

Reglas:

- Una tecla hace una sola cosa por modo. Una tecla usada en `both` cuenta en todos los modos, así que no puede usarla también una acción `foot`/`vehicle`/`bike`/`air`. La excepción son las acciones que solo mueven el stick (movimiento): un modo puede tomar W, A, S, D.
- Si ninguna acción mueve el stick izquierdo, se agrega el movimiento con WASD + flechas (`"no_auto_move": true` lo desactiva). Si ninguna aprieta PS, se agregan las teclas Windows → botón PS. Los perfiles exportados ya traen las dos cosas.
- Lo que cambies después en la página se guarda en `config.ini` encima del archivo importado, igual que con los perfiles de fábrica. "Restaurar teclas de fábrica" vuelve al archivo importado.
