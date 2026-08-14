# Open Engine Simulator — agent guide

- Build with CMake presets; do not add platform-specific project build files.
  `engine-sim-core` and scripting must remain free of SDL, GPU, and OS-device
  types. SDL3 belongs only in the desktop host/adapters.
- Keep runtime assets self-contained under `assets/`. Piranha standard scripts
  live in `assets/es`; authored meshes are exported from `art/assets.blend` to
  `assets/authored_meshes.obj` with `make export-meshes`.
- Preserve the audio boundary: simulation/synthesis produces PCM without
  owning SDL or an audio device. Never allocate, log, perform I/O, or take a
  blocking lock in real-time audio code.
- Validate proportional to the change: `make PLATFORM=<target> portable-test`
  for core changes; rebuild the desktop target for host/render/audio changes.
  Run the script compile and SDL dummy-audio tests when touching their paths.
- For web-visible UI, layout, input, or rendering changes, run `make web`,
  serve that freshly built `build/web/web` output, and inspect the changed
  interaction with Browser Use before reporting success. A compile alone is
  not visual validation; confirm the local server is serving the rebuilt
  artifact rather than a stale development process.
- Keep public wording accurate: this is an independent, community-driven fork
  of AngeTheGreat's Engine Simulator. Preserve upstream attribution and MIT
  notices.

## UI overlays

- Dashboard clusters own persistent panel content and trigger buttons only.
  App-wide dialogs must use `UiManager`'s `OverlayHost`, which owns modal
  lifecycle, z-order, viewport layout, backdrop rendering, and exclusive input
  capture.
- Keep one active modal unless a task explicitly requires a modal stack. Do not
  implement dialogs by changing a dashboard cluster's render layer, mouse
  bounds, or visibility state.
