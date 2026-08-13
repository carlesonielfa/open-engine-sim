# Portable CMake-preset wrappers for Open Engine Simulator.
#
# Select one of: macos-arm64, linux-x86_64, windows-x86_64.
PLATFORM ?= macos-arm64
CMAKE_ARGS ?=

CORE_PRESET := $(PLATFORM)
DESKTOP_PRESET := $(PLATFORM)-desktop
CI_PRESET := $(PLATFORM)-ci

ifeq ($(PLATFORM),windows-x86_64)
DESKTOP_EXECUTABLE := build/$(DESKTOP_PRESET)/engine-sim-desktop.exe
else
DESKTOP_EXECUTABLE := build/$(DESKTOP_PRESET)/engine-sim-desktop
endif

.DEFAULT_GOAL := help
.PHONY: help portable-configure portable-build portable-run portable-test portable-validate export-meshes web web-serve web-serve-lan

help:
	@echo "make PLATFORM=<target> portable-build  Configure and build the desktop host"
	@echo "make PLATFORM=<target> portable-run    Configure, build, and run the desktop host"
	@echo "make PLATFORM=<target> portable-test   Configure, build, and test the portable core"
	@echo "make PLATFORM=<target> portable-validate Run the CI-equivalent core and SDL audio tests"
	@echo "make export-meshes                     Export authored Blender meshes"
	@echo "make web                               Build the Emscripten/WebGL 2 browser host"
	@echo "make web-serve                         Build and serve it at http://localhost:8080"
	@echo "make web-serve-lan                     Build and serve it on the local network"

portable-configure:
	cmake --preset $(DESKTOP_PRESET) $(CMAKE_ARGS)

portable-build: portable-configure
	cmake --build --preset $(DESKTOP_PRESET)

portable-run: portable-build
	$(DESKTOP_EXECUTABLE)

portable-test:
	cmake --preset $(CORE_PRESET) $(CMAKE_ARGS)
	cmake --build --preset $(CORE_PRESET)
	ctest --preset $(CORE_PRESET)

portable-validate:
	cmake --preset $(CI_PRESET) $(CMAKE_ARGS)
	cmake --build --preset $(CI_PRESET)
	ctest --preset $(CI_PRESET) --exclude-regex SdlAudioOutput
	ctest --preset $(CI_PRESET) --tests-regex SdlAudioOutput

export-meshes:
	uv run --with bpy --python 3.13 tools/export_blender_meshes.py

web:
	@command -v emcmake >/dev/null || { echo "Emscripten is required. Source emsdk_env.sh first."; exit 1; }
	emcmake cmake --preset web $(CMAKE_ARGS)
	cmake -E rm -rf build/web/web
	cmake --build --preset web

web-serve: web
	python3 tools/web_server.py --directory build/web/web --port 8080

web-serve-lan: web
	python3 tools/web_server.py --directory build/web/web --host 0.0.0.0 --port 8080
