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
| `AudioEngine` (3 voices including pending steal state) | 304 B |
| `AudioCommandQueue` (16 commands + atomics/cache separation) | 192 B |
| `InteractionSoundService` total | 512 B |
| `AppRuntime` including two framebuffers and sound service | 24,640 B |

The T-Embed output task requests a 4,096-byte FreeRTOS stack. Its 128 mono
samples plus 128 right-left frames occupy 768 bytes inside that stack. Four
128-frame stereo DMA buffers require 2,048 bytes of payload plus ESP-IDF driver
descriptors/metadata from runtime internal RAM. These runtime allocations are
not fully represented by PlatformIO's static RAM line and must be checked with
`uxTaskGetStackHighWaterMark` and heap telemetry on the physical board.

The sample loop uses float arithmetic but no `sin`, `pow`, `tan`, double, heap,
or decoder. A local optimized host stress probe rendered 400 seconds of
three-voice audio in 0.922 seconds (433.8× realtime). This proves ample host
headroom only; it does not replace an ESP32 task-runtime measurement.

The pre-audio clean T-Embed release link on 2026-07-18 reported:

- RAM: 82,616 / 327,680 B (25.2%);
- Flash: 319,433 / 3,145,728 B (10.2%);
- pre-Gallery baseline: RAM 55,824 B, Flash 302,717 B;
- current delta from the pre-Gallery baseline: +26,792 B RAM and +16,716 B
  Flash (Gallery, final repeat/Timeline/transition audit fixes, and bounded
  text layout).

The audio-enabled release link using locked `espressif32@6.12.0` /
Arduino-ESP32 2.0.17 reports:

- RAM: 83,736 / 327,680 B (25.6%), +1,120 B from the last recorded clean
  pre-audio link;
- Flash: 339,669 / 3,145,728 B (10.8%), +20,236 B from that link;
- runtime audio heap in addition to the static line: 4,096-byte task stack,
  2,048-byte DMA payload, and driver/task metadata.

The delta buys the semantic queue, deterministic three-voice synth, SDL-shared
core, full cue palette, legacy I²S task and diagnostics. A future seven-file
PCM palette at the current maximum 250 ms would add at most about 154 KiB before
metadata (`44,100 × 2 × 0.25 × 7`); ADPCM should be considered only after an
actual flash/CPU trade-off, not adopted by habit.

The framebuffer ownership is intentionally explicit for correctness and
snapshot determinism. P8 must measure real FPS/slowest frame and decide whether
the two Gallery buffers should be reused with Runtime buffers. No reuse should
be introduced merely to reduce a static number without lifetime tests.
