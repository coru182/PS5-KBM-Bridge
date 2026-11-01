# Custom profiles

**English** · [Español](custom-profiles.es.md)

You can add or replace profiles without rebuilding the bridge. On the settings page (F11, or `http://PS5-IP:8099`), the **Share and import profiles** section has:

- **Export**: the profile you are editing, as JSON, with its current keys and settings and the games you assigned it to. Download it or copy it.
- **Import**: paste a profile or pick a `.json` file. The bridge checks it, applies it at once and saves it as `/data/ps5-kbm/profiles/<id>.json`. It is loaded again at every start.
- **Delete**: only for imported profiles. If it replaced a built-in profile, the built-in one comes back.

The easiest way to make one: export a similar profile, change it in a text editor, import it.

## Format

```json
{
  "id": "my_game",
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

| Field | Meaning |
|---|---|
| `id` | Required. 1–23 characters: `a-z`, `0-9`, `_`. The id of a built-in profile (see [`data/PROFILES.md`](../data/PROFILES.md)) replaces it. |
| `name`, `name_en` | Name shown in Spanish / English. |
| `kind` | `game` (default), `template` or `menu`. Only changes the group on the page. |
| `title_ids` | Up to 8 Title IDs (`PPSA01234`, `CUSA01234`). The profile is used automatically in those games, unless you chose another profile for the game on the page. |
| `mouse` | `camera` (mouse → right stick, default) or `dpad` (mouse steps the D-pad, for menus). |
| `sens` … `ads_pct` | Same settings as the sliders. Missing = default. |
| `actions` | Up to 64. Each: `ps5` = buttons joined with `+`; `pc` = up to two keys; `layer` = `foot` (default), `vehicle`, `bike`, `air` or `both` (every mode). |

PS5 buttons: `CROSS CIRCLE SQUARE TRIANGLE L1 R1 L2 R2 L3 R3 OPTIONS TOUCHPAD PS DPAD_UP DPAD_DOWN DPAD_LEFT DPAD_RIGHT LS_UP LS_DOWN LS_LEFT LS_RIGHT`, plus `MODE_TOGGLE` (on foot ↔ car) and `MODE_CYCLE` (on foot → car → bike → air).

Keys: the names in the key lists on the page (`A`…`Z`, `0`…`9`, `SPACE`, `LSHIFT`, `LCTRL`, `TAB`, `ENTER`, `ESC`, `KP8`, `MOUSE1`…`MOUSE5`, `WHEEL_UP`, `WHEEL_DOWN`…). F8–F12 are reserved by the bridge.

Rules:

- One key does one thing per mode. A key used in `both` counts in every mode, so it can't also be used by a `foot`/`vehicle`/`bike`/`air` action. Stick-only actions (movement) are the exception: a mode can take over W, A, S, D.
- If no action moves the left stick, WASD + arrows movement is added (`"no_auto_move": true` turns that off). If no action presses PS, the Windows keys → PS button is added. Exported profiles already include both.
- Changes you make on the page afterwards are saved in `config.ini` on top of the imported file, like for built-in profiles. "Restore factory keys" goes back to the imported file.
