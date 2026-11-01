# PS5 KBM Bridge launcher — native ShadowMountPlus title (PPSA51258).
# Build tooling: BlackBearReloaded's ps5-native-app-boilerplate, as vendored by
# ScreenTester5 (Marice). GPL-3.0-or-later. See THIRD_PARTY_NOTICES.md.
#
#   make deps   # once: fetches the public PS5 payload SDK pinned by the tooling (.deps/)
#   make        # dist/PPSA51258/  (copy that folder to /data/homebrew/ on the PS5)
#
# Needs: clang-18, lld-18, ninja (pip install ninja), python3, sha256sum.
SHELL := /bin/bash
.DEFAULT_GOAL := app
export PATH := $(HOME)/.local/bin:$(PATH)

TITLE_ID ?= PPSA51258
APP_NAME ?= PS5 KBM Bridge
APP_CATEGORY ?= media
APP_DEFINITIONS ?=
APP_INCLUDE_PATHS ?=
APP_STATIC_ARCHIVES ?=
PACBREW_PACKAGES ?=
PACBREW_INCLUDE_PATHS ?=
PACBREW_STATIC_ARCHIVES ?=
APP_RUNTIME_MODULES ?=
APP_WRAP_SYMBOLS ?=
APP_SOURCE_DIR ?=
APP_PARAM ?=
APP_SCE_SYS ?=
APP_ASSETS ?=
# the bridge payload rides inside the app: /app0/ps5-kbm-bridge.elf
APP_ROOT_FILES ?= ps5-kbm-bridge.elf
CONTENT_SUFFIX ?=
BUILD_JOBS ?= $(shell nproc 2>/dev/null || echo 2)
USE_CCACHE ?= 0
export BUILD_JOBS USE_CCACHE
export APP_DEFINITIONS APP_INCLUDE_PATHS APP_STATIC_ARCHIVES APP_RUNTIME_MODULES APP_WRAP_SYMBOLS
export APP_SOURCE_DIR APP_PARAM APP_SCE_SYS APP_ASSETS APP_ROOT_FILES
export PACBREW_PACKAGES PACBREW_INCLUDE_PATHS PACBREW_STATIC_ARCHIVES
export TITLE_ID APP_NAME APP_CATEGORY CONTENT_SUFFIX

RUNTIME := runtime/libc.prx
RUNTIME_INPUTS := tools/rebuild-libc.sh tools/build-host-tools.sh tools/ninja-build.sh \
	$(wildcard tooling/native/*.cpp tooling/native/*.hpp) \
	$(wildcard tooling/native/runtime/*.txt)

.PHONY: all app deps clean
all: app

deps:
	@bash tools/setup-native-dependencies.sh

$(RUNTIME): $(RUNTIME_INPUTS)
	@bash tools/rebuild-libc.sh

app: $(RUNTIME)
	@test -f ps5-kbm-bridge.elf || { echo "Falta ps5-kbm-bridge.elf: copia aqui out/ps5-kbm-bridge-v0XX.elf con ese nombre" >&2; exit 2; }
	@bash tools/build.sh Folder
	@echo "Listo: dist/$(TITLE_ID)/  -> copiar a /data/homebrew/$(TITLE_ID)/ en la PS5"

clean:
	rm -rf build dist
