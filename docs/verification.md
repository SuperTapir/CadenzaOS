# P0–P7 verification record

Verified on macOS on 2026-07-18. This file records authoritative gates, not a
claim that physical-device P8 work has happened.

| Phase | Evidence |
| --- | --- |
| P0 decisions | `project-vision.md`, `reference-research.md`, and `engine-adoption-decision.md`; SDL3/U8g2/doctest/stb/gif-h adopted at narrow seams, LVGL/Tweeny full-runtime adoption rejected by measured spikes |
| P1 decoupling | `cadenza_portable_core_compile_probe` and `cadenza_shared_source_audit`; T-Embed and desktop adapters feed shared `InputReducer`, App, Runtime, canvas, and animation code |
| P2 simulator MVP | `cadenza_desktop_smoke_tests` visits every registered App including Gallery through desktop input mapping; SDL dummy launch succeeds for the real executable |
| P3 debugging | simulation controller, PNG, recording, desktop model, device-preview, and diagnostic tests cover pause/step/speed/fixed-real modes, capture, GIF finalization, HUD data, and invariance |
| P4 animation core | easing/Tween/composition/Spring suites cover endpoints, interior vectors, delay, finite/infinite repeat, yoyo/reverse, callback, seek, pause, large delta, capacity, and diagnostics |
| P5 presentation | transition/effects/particle/frame/state tests plus Gallery pages and reduced-motion propagation |
| P6 graphics | guarded framebuffer, U8g2 raster/font golden tests, bitmap composition/flip/XOR/clipping, atlas, dither phase, and off-screen immutability |
| P7 regression | 32/32 normal, 32/32 warnings-as-errors, and 32/32 ASan+UBSan tests; app and representative Gallery snapshots at both profiles emit PNG artifacts on mismatch |

Fresh completion gates:

- host Debug tests: 32/32 pass;
- strict AppleClang build (`-Wall -Wextra -Wpedantic -Werror`): 32/32 pass;
- ASan+UBSan host build: 32/32 pass;
- SDL3 3.4.12 dummy-driver executable smoke: pass at 320×170;
- PlatformIO T-Embed clean release compile: pass;
- firmware size: 82,608 B RAM, 316,193 B Flash;
- `git diff --check`, source-manifest audit, platform-coupling audit, and
  third-party pin/license audit: pass.

## Deliberately unexecuted P8

The following require the physical original T-Embed and remain open: encoder
direction/debounce/high-speed behavior, long-press feel, real FPS and slowest
frame, TFT tearing, simulator-to-TFT photographed pixel comparison, animation
performance budget under real transfer cost, and repeated on-device regression
after effect groups. Sharp hardware driving, audio, persistence, and new Apps
remain after this main line.
