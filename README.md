# PS5 KBM Bridge

**English** · [Español](README.es.md)

Keyboard and mouse in any PS5 game, with no adapter and no PC. A payload for jailbroken PS5 consoles that reads a USB keyboard and mouse and turns them into a virtual DualSense, with profiles that mirror each game's PC keybindings.

> [!WARNING]
> **Experimental.** Requires a jailbroken PS5. Use at your own risk.
> - It only translates input: it does not modify games or inject cheats.
> - Some online games ban or penalise keyboard/mouse adapters (Marvel Rivals, Rainbow Six Siege, among others). Meant for offline play.

## What it does

- **Virtual DualSense** from the keyboard and mouse wireless receiver, built on Ghostcontrol's virtual DualSense. The mouse drives the right stick by speed, so it feels the same at 30, 60 or 120 fps. Injects at 250 Hz.
- **Recognises the game** you open by its Title ID. Ships a database of about 24,600 PS5 and PS4 titles, so it knows the game's name.
- **Per-game profiles:** 43 games (counting the three GTA Trilogy games as one profile) use their PC version's default keys, mapped to the button that does the same thing on PS5. About 290 more games use a genre template (shooter, third-person, souls-like, racing). Full list with sources: [`data/PROFILES.md`](data/PROFILES.md).
- **Settings page** at `http://PS5-IP:8099`, from your phone, a PC or the PS5's own browser:
  - sensitivity, aiming (ADS) sensitivity and key binding with capture;
  - which profile each game uses, and marking a game as "native".
- **From the PS5, no phone needed:**
  - a home screen icon (through ShadowMountPlus) opens the settings;
  - **F11** opens them in the middle of a game.
  - On that page the mouse drives the cursor.
- **PS5 menu profile:** with no game open, WASD/arrows and mouse movement move the selection, Enter/F/E/left click select, Esc/right click go back and Space opens Options.
- **On foot and in vehicles:** a profile can use different keys on foot and in a vehicle (GTA V and GTA Trilogy: W walks, or accelerates in a car, like on PC). F switches mode when you get in or out; B switches it by hand.
- **English or Spanish:** the settings page and notifications follow your PS5's language. You can set it by hand under General.
- **Games with their own keyboard/mouse support** (Fortnite, Call of Duty, Battlefield 6…): it suggests handing the receiver back to the PS5 while you play.

### Reserved keys

| Key | What it does |
|---|---|
| Windows | PS button |
| F8 | Start the controller by hand (if you turned off automatic start) |
| F9 | Mouse as cursor / as camera |
| F10 | In game: measure the game's right-stick dead zone (diagnostics ELF: receiver release test) |
| F11 | Open the settings in the PS5 browser |
| F12 | Stop the bridge |

## Requirements

- **Jailbroken PS5.** Tested on firmware **11.60** with elfldr, kstuff, ShadowMountPlus and backpork. With etaHEN it can also start automatically.
- **Logitech wireless receiver `046d:c534`** (keyboard and mouse on the same receiver). It's the only one tested, and the code looks for that VID:PID. Other receivers that speak the HID boot protocol might work by changing `TARGET_VID`/`TARGET_PID` in `src/kbm_bridge.c`, but that is untested.
- **To build:** Linux or WSL (Ubuntu) with `git`, `python3`, `make`, `gcc` and the [PS5 Payload SDK](https://github.com/ps5-payload-dev/sdk) in `/opt/ps5-payload-sdk`. Node and Playwright for the page tests (optional).

## Build

```bash
git clone https://github.com/coru182/ps5-kbm-bridge.git
cd ps5-kbm-bridge
export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
./build_wsl.sh
```

The script does everything:

1. Generates the profiles, the title database and the embedded page.
2. Runs the tests.
3. Clones Ghostcontrol and applies the patch.
4. Builds both ELFs.
5. Builds the icon app. The first time it downloads its dependencies to `~/.cache/ps5-kbm/`.

Output in `out/`:

| File | Purpose |
|---|---|
| `ps5-kbm-bridge-v0103.elf` | The bridge |
| `ps5-kbm-diag-v0103.elf` | Diagnostics: only reads and measures, **never** creates the virtual controller, exits after 5 min |
| `homebrew/PPSA51258/` | Home screen icon (ShadowMountPlus) |
| `etaHEN/payloads/` | Automatic start, only if you use etaHEN |

`KBM_SKIP_LAUNCHER=1 ./build_wsl.sh` builds without the icon.

## Install and use

1. Copy `out/homebrew/PPSA51258/` to `/data/homebrew/PPSA51258/` on the PS5 over FTP (for example with ps5upload). Reboot and load your payloads; ShadowMountPlus adds the **PS5 KBM Bridge** icon to the home screen.
   - Without ShadowMountPlus: send `ps5-kbm-bridge-v0103.elf` to elfldr (port 9021).
2. Plug in the receiver and open the icon. The settings page opens. Move the mouse and press a key: the virtual DualSense appears.
3. Open a game. The notification says which game it recognised and which profile it applied. To change something mid-game: **F11**. Go back with the PS button (or the Windows key).

Settings are saved in `/data/ps5-kbm/config.ini` (only what you changed). The diagnostics log is `/data/ghostpad/gc_status.log`.

**First time on a new console:** try the diagnostics ELF first.
1. Move the mouse and press keys.
2. Open a game and go back home: the log should show `GAME PPSAxxxxx`.
3. Press F10: the receiver goes back to the PS5 for 20 s and is then taken back.

### How it picks the profile

For each game you open, in this order:

1. what you chose for that game on the page;
2. the game's own profile;
3. its genre template;
4. the default profile ★ (for games it doesn't recognise).

With no game open it uses the "PS5 menu" profile.

If the database says the game supports keyboard and mouse on PS5, it suggests "native". It only switches to native without asking if you turn that option on.

## Add or fix a profile

1. Create or edit `data/profiles/<id>.json` with the PC default keys and the PS5 button for each action. Use the existing ones as examples.
   - Don't include movement or camera: WASD, arrows, mouse and the Windows key (PS button) are added automatically.
   - For games with vehicles, add `"layer": "vehicle"` to the in-vehicle actions and `"layer": "both"` to the ones that apply everywhere; give the enter/exit action `"ps5": "TRIANGLE+MODE_TOGGLE"` (see `data/profiles/gta_trilogy_de.json`).
2. Add the game's name to `data/games.json`. It's a regular expression on the normalised name: lowercase, no accents, no ™.
3. Add the English text of each new action to `data/i18n/en.json` (action labels in the profiles are written in Spanish).
4. Run `./build_wsl.sh`. `tools/gen_builtin.py` checks every key, button and translation, and stops if anything is invalid. It also regenerates `data/PROFILES.md` and `data/PROFILES.es.md`.

Profiles from other players are welcome (issues or pull requests), ideally with sources. Profiles with incomplete research are in `data/unverified/` and are not built in.

## Layout

| Folder | Contents |
|---|---|
| `src/` | The bridge in C: USB reading (`kbm_usbfs`), keyboard/mouse → controller engine (`kbm_engine`), profiles and title database (`kbm_config`), HTTP server and API (`kbm_http`, `kbm_api`), threads and lifecycle (`kbm_bridge`) |
| `web/` | The settings page, embedded in the ELF by `tools/embed_web.py` |
| `data/` | Profiles, templates, name patterns, games with native keyboard/mouse, translations and the title database |
| `launcher/` | The icon app (PPSA51258): sends the ELF to elfldr and opens the settings |
| `tools/` | Generators and the Ghostcontrol patch |
| `tests/` | PC tests: engine, configuration, simulated USB, the real HTTP API and the page in Chromium |

## What's tested

| | Status |
|---|---|
| USB reading, engine, rate-independent mouse | Tested on console (v0.6–v0.7.7) with RE2 Remake and RE Requiem |
| Icon, settings page on the PS5, F11 mid-game, PS button on the Windows key | Tested on console (v0.7.x) |
| Game detection by Title ID | Tested on console (v0.7) |
| Automatic profile choice and title database (v0.8) | PC tests. **Not tested on console** |
| PS5 system language (v0.9) | Read with `sceSystemServiceParamGetInt` and logged (`system language: ...`). **Not tested on console**; if it fails, the page uses the browser language and notifications are in English. Can be set under General |
| Per-game profile keys | Checked against their sources, **not tested in play**. "Medium" confidence profiles have inferred parts |
| Handing the receiver to the PS5 and taking it back ("native" games) | Handing it back works. **Taking it back once left the mouse unresponsive: under investigation** |
| Automatic start with etaHEN | Not tested |

**Security:** the page on port 8099 has no password. Anyone on your local network can change settings or stop the bridge. Fine for a home network, not for a public one.

## Credits and license

GPL-3.0-or-later (see [`LICENSE`](LICENSE)).

- [**Ghostcontrol**](https://github.com/srbraboo/Ghostcontrol-PS5-USB-Controller-Patcher) by StonedModder (srbraboo, GPL-3.0-or-later): this project is built on its virtual DualSense. Its code is not in this repo: the build downloads it from the original.
- [**ps5-native-app-boilerplate**](https://github.com/blackbearreloaded/ps5-native-app-boilerplate) by BlackBearReloaded (via [ScreenTester5](https://github.com/Marice/ScreenTester5)): icon app tooling.
- [**PS5 Payload SDK**](https://github.com/ps5-payload-dev/sdk) by ps5-payload-dev.
- [**PlayStation-Titles**](https://github.com/andshrew/PlayStation-Titles) by andshrew (MIT): title database.

Full details, with references and the sources of each profile, are in [`CREDITS.md`](CREDITS.md). Version history in [`CHANGELOG.md`](CHANGELOG.md).
