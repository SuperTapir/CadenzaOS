# Memory and animation budget

The canonical maximum framebuffer is 400×240×1 bit = 12,000 bytes. All
profiles use fixed storage so an object's cost does not change at runtime.

| Owner | Fixed framebuffer storage | Purpose |
| --- | ---: | --- |
| firmware output | 12,000 B | current frame presented to TFT |
| `AppRuntime` | 24,000 B | immutable outgoing/incoming transition captures |
| `AnimationGalleryApp` | 24,000 B | transition source/destination demo scenes |

Particle, atlas, state, Tween, graph, diagnostic, and bounded-text result tables
have fixed compile-time capacities and do not allocate in frame updates. A
`BoundedTextResult` owns at most four 96-byte line fragments and is compile-time
guarded to remain within 512 bytes. Desktop PNG/GIF/RGBA conversion may allocate
because it is outside the embedded core.

## Interaction audio budget

The audio path has no allocation after construction in the portable sample
loop. A 2026-07-18 host ABI probe reports:

| Object | Fixed size |
| --- | ---: |
| `AudioEngine` (4 voices including pending steal state) | 592 B |
| `AudioCommandQueue` (16 commands + atomics/cache separation) | 192 B |
| `InteractionSoundService` (including 16 delayed events) | 1,728 B |
| `AppRuntime` including two framebuffers and command submission | 24,712 B |
| `SystemServiceHost` including owned interaction sound service | 13,120 B |

The T-Embed output task requests a 4,096-byte FreeRTOS stack. Its 128 mono
samples plus 128 right-left frames occupy 768 bytes inside that stack. Four
128-frame stereo DMA buffers require 2,048 bytes of payload plus ESP-IDF driver
descriptors/metadata from runtime internal RAM. These runtime allocations are
not fully represented by PlatformIO's static RAM line and must be checked with
`uxTaskGetStackHighWaterMark` and heap telemetry on the physical board.

The sample loop uses float arithmetic but no per-sample `sin`, `exp`, `pow`,
`tan`, double, heap, or decoder. Sine uses a 65-entry quarter-wave table;
exponential attack/decay uses multipliers prepared once per tone. The previous
three-voice host probe is no longer treated as evidence for this four-voice
revision; firmware link/size is automated, while task runtime and stack
high-water still require the physical ESP32 measurement.

The clean system-service-foundation base (`518baf6`) and audio-enabled release
were both linked on 2026-07-18 using locked `espressif32@6.12.0` /
Arduino-ESP32 2.0.17:

| Build | RAM | Flash |
| --- | ---: | ---: |
| clean `518baf6` base | 94,680 B (28.9%) | 349,609 B (11.1%) |
| audio palette with calibrated Confirm/Back | 98,808 B (30.2%) | 358,805 B (11.4%) |
| audio delta | +4,128 B | +9,196 B |

The audio-enabled link additionally has:

- runtime audio heap in addition to the static line: 4,096-byte task stack,
  2,048-byte DMA payload, and driver/task metadata.

The delta buys the semantic queue, deterministic four-voice synth, delayed
events, SDL-shared core, full 15-cue palette, legacy I²S task and diagnostics.
A future 15-file PCM palette at 250 ms per cue would add about 323 KiB before
metadata (`44,100 × 2 × 0.25 × 15`); ADPCM should be considered only after an
actual flash/CPU trade-off, not adopted by habit.

The framebuffer ownership is intentionally explicit for correctness and
snapshot determinism. P8 must measure real FPS/slowest frame and decide whether
the two Gallery buffers should be reused with Runtime buffers. No reuse should
be introduced merely to reduce a static number without lifetime tests.
