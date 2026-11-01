# Third-party notices (launcher)

| Component | Use | License |
|---|---|---|
| [ps5-native-app-boilerplate](https://github.com/blackbearreloaded/ps5-native-app-boilerplate) by BlackBearReloaded, as vendored in [ScreenTester5](https://github.com/Marice/ScreenTester5) by Marice | `tooling/`, `tools/`, `runtime/` (clean-room `libc.prx` manifests, generated at build time): compile, link, convert to a PS5 module, sign as development FSELF and assemble the title folder | GPL-3.0-or-later |
| [PS5 Payload SDK](https://github.com/ps5-payload-dev/sdk) | toolchain and import stubs, fetched by `make deps` | GPL-3.0-or-later (see SDK) |

`src/main.c`, `sce_sys/` and the icon are part of PS5 KBM Bridge (GPL-3.0-or-later).
