#!/usr/bin/env bash
# PS5 KBM Bridge v0.10.3 — build on WSL Ubuntu / Linux.
#   out/ps5-kbm-diag-v0103.elf             read+measure only, NEVER creates a virtual pad (5 min, F10 = release test)
#   out/ps5-kbm-bridge-v0103.elf           the bridge (auto-activates, settings on :8099)
#   out/etaHEN/payloads/ps5-kbm-bridge.elf (+ .auto_start)  copy to /data/etaHEN/payloads/ = starts with etaHEN
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SDK="${PS5_PAYLOAD_SDK:-/opt/ps5-payload-sdk}"
UPSTREAM="$ROOT/Ghostcontrol-PS5-USB-Controller-Patcher"
OUT="$ROOT/out"

printf '\n[1/7] Comprobando herramientas y SDK...\n'
for cmd in git python3 make cc; do
  command -v "$cmd" >/dev/null || { echo "Falta $cmd (sudo apt install git python3 make gcc)" >&2; exit 1; }
done
if [[ ! -f "$SDK/toolchain/prospero.mk" ]]; then
  echo "No encuentro SDK en $SDK. Instalalo primero o exporta PS5_PAYLOAD_SDK." >&2
  exit 1
fi
export PS5_PAYLOAD_SDK="$SDK"

printf '\n[2/7] Generando perfiles de juegos, base de titulos y pagina web...\n'
python3 "$ROOT/tools/gen_builtin.py"
python3 "$ROOT/tools/embed_web.py"

printf '\n[3/7] Pruebas locales (motor, config, USB_FS, HTTP/API)...\n'
make -C "$ROOT/tests" clean test
make -C "$ROOT/tests" host_server
python3 "$ROOT/tests/test_http.py"
python3 "$ROOT/tools/test_patch.py"
if command -v node >/dev/null && node -e "require('playwright')" 2>/dev/null; then
  node "$ROOT/tests/test_ui.js" "$ROOT/tests"
else
  echo "(sin node+playwright: salteo la prueba de la pagina en navegador)"
fi

printf '\n[4/7] Preparando copia limpia de Ghostcontrol...\n'
if [[ -f "$UPSTREAM/payload/kbm_bridge.h" ]] && ! grep -q kbm_bridge_set_vda "$UPSTREAM/payload/gc_main.c"; then
  echo "La carpeta Ghostcontrol tiene un parche KBM VIEJO (v0.6.x o anterior)." >&2
  echo "Borrala ($UPSTREAM) y volve a correr este script." >&2
  exit 1
fi
if [[ ! -d "$UPSTREAM/payload" ]]; then
  git clone --depth 1 https://github.com/srbraboo/Ghostcontrol-PS5-USB-Controller-Patcher.git "$UPSTREAM"
fi
if [[ -f "$UPSTREAM/payload/kbm_bridge.h" ]]; then
  echo "Ghostcontrol ya tiene un parche anterior: lo deshago y aplico el de v0.10.3."
  python3 "$ROOT/tools/apply_to_ghostcontrol.py" --reapply "$UPSTREAM"
else
  python3 "$ROOT/tools/apply_to_ghostcontrol.py" "$UPSTREAM"
fi

mkdir -p "$OUT"
printf '\n[5/7] Compilando ELF de DIAGNOSTICO y ELF BRIDGE...\n'
make -C "$UPSTREAM/payload" clean all TARGET=ps5-kbm-diag-v0103.elf KBM_CFLAGS=-DKBM_DIAG_ONLY=1
cp "$UPSTREAM/payload/ps5-kbm-diag-v0103.elf" "$OUT/"
make -C "$UPSTREAM/payload" clean all TARGET=ps5-kbm-bridge-v0103.elf KBM_CFLAGS=-DKBM_DIAG_ONLY=0
cp "$UPSTREAM/payload/ps5-kbm-bridge-v0103.elf" "$OUT/"

printf '\n[6/7] Armando carpeta para el autoarranque de etaHEN...\n'
rm -rf "$OUT/homebrew" "$OUT/etaHEN"
mkdir -p "$OUT/etaHEN/payloads"
cp "$OUT/ps5-kbm-bridge-v0103.elf" "$OUT/etaHEN/payloads/ps5-kbm-bridge.elf"
: > "$OUT/etaHEN/payloads/ps5-kbm-bridge.elf.auto_start"

printf '\n[7/7] Icono para ShadowMountPlus (app lanzadora PPSA51258)...\n'
if [[ "${KBM_SKIP_LAUNCHER:-0}" == "1" ]]; then
  echo "(KBM_SKIP_LAUNCHER=1: salteado)"
elif command -v clang-18 >/dev/null && { command -v ld.lld-18 >/dev/null || command -v ld.lld >/dev/null; }; then
  export PATH="$HOME/.local/bin:$PATH"
  command -v ninja >/dev/null || { echo "Falta ninja: pip install --user ninja   (o sudo apt install ninja-build)" >&2; exit 1; }
  cp "$OUT/ps5-kbm-bridge-v0103.elf" "$ROOT/launcher/ps5-kbm-bridge.elf"
  # Shared download cache: every new version reuses it (no copying .deps by hand).
  CACHE="${KBM_LAUNCHER_DEPS:-$HOME/.cache/ps5-kbm/launcher-deps}"
  mkdir -p "$CACHE"
  if [[ ! -L "$ROOT/launcher/.deps" ]]; then rm -rf "$ROOT/launcher/.deps"; ln -s "$CACHE" "$ROOT/launcher/.deps"; fi
  # Complete = SDK linker + import stubs present; otherwise wipe the SDK and fetch it again.
  if ! compgen -G "$CACHE/native/ps5-payload-sdk/target/lib/*.so" >/dev/null || [[ ! -x "$CACHE/native/ps5-payload-sdk/bin/prospero-lld" ]]; then
    echo "Descargando dependencias del icono (una sola vez, ~350 MB) en $CACHE ..."
    rm -rf "$CACHE/native/ps5-payload-sdk" "$CACHE/native/ps5-payload-sdk.zip"
    make -C "$ROOT/launcher" deps
  fi
  rm -rf "$ROOT/launcher/build"
  make -C "$ROOT/launcher" app
  rm -rf "$OUT/homebrew"; mkdir -p "$OUT/homebrew"
  cp -r "$ROOT/launcher/dist/PPSA51258" "$OUT/homebrew/"
  echo "Icono listo: out/homebrew/PPSA51258/  -> copiar a /data/homebrew/ en la PS5"
else
  echo "(sin clang-18/lld-18: salteo el icono. sudo apt install clang-18 lld-18 ninja-build)"
fi

echo
ls -l "$OUT"/*.elf
(cd "$OUT" && sha256sum *.elf etaHEN/payloads/ps5-kbm-bridge.elf | tee SHA256SUMS.txt)
printf '\nCopiar a Windows:\n  mkdir -p /mnt/c/Users/Public/PS5 && cp -r "%s"/* /mnt/c/Users/Public/PS5/\n' "$OUT"
