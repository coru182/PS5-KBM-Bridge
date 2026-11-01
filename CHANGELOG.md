# Changelog

**English** · [Español](CHANGELOG.es.md)

## v0.10: closer to native in every game

Games handle the right stick differently: some ignore small deflections (dead zone) and speed the camera up gradually (acceleration). That's why RE2 Remake felt native and RE Requiem or GTA V felt heavy with the same bridge settings.

- **Dead-zone finder (F10, in game):** the camera turns slowly to the right; click as soon as it starts moving. The value (minus ~240 ms of reaction time) is saved as the active profile's dead zone. Esc cancels. F10 is now a reserved key.
- **Radial dead-zone compensation:** small mouse movements jump straight over the game's dead zone while keeping their direction (before, each axis was compensated separately, which could add vertical drift).
- **Small-movement boost** (0–100 %, per profile): raises small stick values for games with a smooth/exponential response curve.
- **In-game settings tips** on the page: RE Requiem (right stick dead zone 5–7, camera acceleration max, Linear curve) and GTA V (aim/look dead zone minimum, aim/look acceleration maximum), plus a general guide for every profile.
- Profiles with dead zone 0 and boost 0 (the default, e.g. RE2 Remake) behave exactly as before.

## v0.9.2: PS5 menu profile

- New **"PS5 menu" profile**, used automatically while no game is running (home screen, library, store, settings):
  - WASD or arrows = D-pad; Enter, F, E or left click = ✕; Esc, Backspace or right click = ○; Space = Options; Tab = △; Q = □; Z/X or mouse side buttons = L1/R1; C/V = L2/R2; mouse wheel = up/down; Windows key = PS button.
  - **Moving the mouse moves the selection one step at a time** (D-pad), instead of the camera. The speed is the "Cursor and menu mouse speed" slider under General.
- It can be edited like any other profile ("Edit the menu profile" on the page when no game is open).
- When a game is open (even if you go back to the home screen with the game in the background), the game's profile stays active.

## v0.9.1: GTA Trilogy and vehicle mode

- **New profile: GTA: The Trilogy – Definitive Edition** (GTA III, Vice City and San Andreas, PS5 and PS4 versions). PS5 buttons follow the Modern scheme. PC keys are the original PC defaults plus the Definitive Edition changes found in sources (Tab = weapon wheel); no complete PC key list for the Definitive Edition was found, so some keys may differ.
- **Vehicle mode:** a profile can now bind keys differently on foot and in a vehicle. In the GTA profile, W accelerates (R2) in a car and walks on foot, Space is handbrake instead of sprint, and so on. **F** (enter/exit vehicle) switches mode; **B** switches it by hand if it gets out of sync (for example, if you press F with no car nearby). The page shows the current mode and groups the keys as Always / On foot / In vehicle.
- Bikes and aircraft use other buttons in these games and are not covered by the profile.
- Rebinding a key only removes it from actions that are active at the same time (same mode).

## v0.9: English and Spanish

- The settings page and the PS5 notifications are in **English and Spanish**. By default they follow the PS5 language (read with `sceSystemServiceParamGetInt`; the result is logged). If it can't be read, the page uses the browser language and notifications are in English.
- New **Idioma / Language** selector under General: Automatic, Español or English. Saved in `config.ini` (`lang=`).
- All 520 profile actions have English text (`data/i18n/en.json`). Key names change too: in English they are shown for a US keyboard.
- The repo is in English by default, with a Spanish version of each document.

## v0.8: recognises the game and applies its profile

- **Built-in title database:** about 24,600 PS5 and PS4 Title IDs (andshrew/PlayStation-Titles, MIT license). When you open a game, the bridge reads its Title ID, looks up its name and applies the matching profile. The notification shows the game name and the chosen profile.
- **Per-game profiles**, researched with sources: each maps the **PC version's default keys** to the DualSense button that does the same thing on PS5. The list with confidence level and sources is in [`data/PROFILES.md`](data/PROFILES.md).
- **Genre templates** (first-person shooter, third-person shooter, action/souls-like, racing) for about 280 more games that don't have their own profile yet.
- **Priority:** what you chose for that game → the game's profile → its genre template → the default profile ★ (for games it doesn't recognise).
- **Native keyboard/mouse:** if research says the PS5 version supports keyboard and mouse (Fortnite, Call of Duty, Battlefield 6, FFXIV…), the page **suggests** it; it doesn't switch on its own unless you tick the option under General.
- **Settings page:** "Current game" shows the game name and buttons: Automatic, Native, templates and "Other game…" (A–Z list). The profile editor is now a grouped list (In use / Templates / Games).
- **Config:** `config.ini` only stores what you changed. A v0.7 config keeps working: key changes for RE2, Requiem and Generic are kept.
- **Note:** if you set Requiem as the default profile in v0.7, games the bridge doesn't recognise will use Requiem. To go back to Generic: Profile → Edit another profile… → Generic → "Use as default profile".

**42 games with their own profile.** Six have incomplete research and are **not** included: AC Shadows, Kingdom Come Deliverance II, Far Cry 6, Marvel Rivals, Ratchet & Clank and Sniper Elite 5 (they're in `data/unverified/`). Stellar Blade, Monster Hunter Wilds and Star Wars Jedi: Survivor were not researched. Those 9 use their genre template.

## v0.7.7: choose the game's profile from the PS5

- Under "Current game", the dropdown was replaced by **buttons**. The dropdown didn't work in the PS5 browser: the page refreshed every second and replaced the dropdown while it was open.
- Interactive parts of the page are only redrawn when they change, and the page doesn't refresh while a dropdown is open.
- The "Profile" tabs now say they only choose which profile you **edit**. Next to them there's a **"Use this profile in <game>"** button.

**v0.7.6.1 (build only):** the icon's dependencies are stored once in `~/.cache/ps5-kbm/launcher-deps` and every version reuses them. If the download was incomplete, the script detects it and downloads it again.

## v0.7.6: settings mid-game, no phone

- **F11** opens the settings page in the PS5 browser from anywhere, even with a game open, and turns on pointer mode. Go back with the **PS button** (or the Windows key), which also returns the mouse to the camera. Changes apply right away.
- **Don't open the icon with a game running:** the PS5 would ask you to close the game. Use F11 in game.
- F11 is reserved, like F8, F9 and F12.

## v0.7.5

- **Pointer mode for the PS5 browser:** the mouse moves the cursor (left stick), left click = ✕, right click = ○ and the wheel scrolls (right stick). The keyboard works as usual. It turns on when the bridge opens the settings page from the icon, and off with the **PS button** (or Windows key), when a game opens, or with **F9**.
- **Aiming (ADS) sensitivity:** a per-profile setting from 25 to 400 %, applied while any key that sends L2 is held (right click by default). RE Requiem starts at 150 %, the others at 100 %.

## v0.7.4: the icon opens the settings on the PS5

Opening **PS5 KBM Bridge** from the home screen starts the bridge through elfldr if needed, closes the launcher and opens the **PS5 browser at `http://127.0.0.1:8099/`** with `sceSystemServiceLaunchWebBrowser` (the call used by the PS5 Payload SDK's `samples/browser`). Exit with the **PS button** (or the Windows key). Can be turned off under General.

## v0.7.3: home screen icon (ShadowMountPlus)

`launcher/` is a small native PS5 app (**title `PPSA51258`, "PS5 KBM Bridge"**). When opened, it sends the bundled `ps5-kbm-bridge.elf` to **elfldr** on `127.0.0.1:9021`; the bridge then closes it with `sceSystemServiceKillApp`. If elfldr isn't loaded, it shows a notification. Install by copying `out/homebrew/PPSA51258/` to `/data/homebrew/PPSA51258/`; ShadowMountPlus adds it to the home screen. Build tooling: [ps5-native-app-boilerplate](https://github.com/blackbearreloaded/ps5-native-app-boilerplate) (BlackBearReloaded), via [ScreenTester5](https://github.com/Marice/ScreenTester5).

## v0.7.2

- **Windows key (left or right) = PS button** in every profile (bit `0x00010000`, the same Ghostcontrol uses for the Xbox Guide button).
- **Retake diagnostics:** each time the bridge takes the receiver it logs `TAKE n` with the PS5 driver state after detach and the real HID protocol (`GET_PROTOCOL`, 0 = boot), plus the shape of the first keyboard and mouse reports.
- **etaHEN autostart only shown when `/data/etaHEN` exists.**

## v0.7.1: automatic start with etaHEN (etaHEN only)

- **Autostart:** copy `out/etaHEN/payloads/` to `/data/etaHEN/payloads/`. etaHEN starts any ELF there that has a `.auto_start` file next to it. The settings page can turn it on or off.
- **Service mode:** no longer turns off when idle by default (can be re-enabled under General).
- **Single instance:** if a bridge is already running, a second one says so and exits.
- Removed the websrv extension (it failed with `TypeError: Load failed` on the PS5).

## v0.7

Wireless keyboard + mouse (Logitech receiver `046d:c534`) on PS5 11.60, translated to a Ghostcontrol virtual DualSense, with a companion page to adjust sensitivity and keys per game.

- **Starts without F8:** activates once both keyboard **and** mouse send valid data.
- **250 Hz injection by default.** The mouse is converted by **speed** (counts per ms over a fixed window), so the stick gets the same value at 60, 120 or 250 Hz and in games at 30, 60 or 120 fps (measured in tests: 48 / 48 / 48).
- **PC-style keys for Resident Evil:** RE2/RE3/RE4 Remake and RE Requiem profiles with the PC default keys, plus a **Generic** profile with every button. Everything can be rebound.
- **Companion page** at `http://PS5-IP:8099`.
- **Native game detection:** reads the running game with `sceSystemServiceGetAppIdOfRunningBigApp` / `GetAppTitleId`. A game can be marked **Native** or given a profile, and it's remembered. In a native game the bridge **hands the receiver back to the PS5** (USB reset with `USB_DEVICEENUMERATE`) and takes it back on exit.
- **F12** also hands the keyboard and mouse back to the PS5. Settings saved in `/data/ps5-kbm/config.ini`.

## v0.6

- Fixed v0.5's input lag: `USB_FS_COMPLETE` pops from **a single queue per descriptor** and returns the index of the endpoint that completed. v0.5 assumed one queue per endpoint and mixed up keyboard and mouse reports. The queue is now drained until `EBUSY` and each report goes to its endpoint.
- Tested on console with RE2 Remake and RE Requiem.

## v0.5 and earlier

- First versions: reading the Logitech receiver as a boot-protocol keyboard and mouse and translating it to a Ghostcontrol virtual DualSense. They had input lag and a 10-minute cutoff (watchdog), fixed in v0.6 and v0.7.
