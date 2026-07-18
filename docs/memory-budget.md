# Memory and animation budget

The canonical maximum framebuffer is 400×240×1 bit = 12,000 bytes. All
profiles use fixed storage so an object's cost does not change at runtime.

| Owner | Fixed framebuffer storage | Purpose |
| --- | ---: | --- |
| firmware output | 12,000 B | current frame presented to TFT |
| `AppRuntime` | 24,000 B | immutable outgoing/incoming transition captures |
| `AnimationGalleryApp` | 24,000 B | transition source/destination demo scenes |

Particle, atlas, state, Tween, graph, and diagnostic tables have fixed compile-
time capacities and do not allocate in frame updates. Desktop PNG/GIF/RGBA
conversion may allocate because it is outside the embedded core.

The clean T-Embed release link on 2026-07-18 reported:

- RAM: 82,608 / 327,680 B (25.2%);
- Flash: 316,193 / 3,145,728 B (10.1%);
- pre-Gallery baseline: RAM 55,824 B, Flash 302,717 B;
- current delta from the pre-Gallery baseline: +26,784 B RAM and +13,476 B
  Flash (Gallery plus final repeat/Timeline/transition audit fixes).

The framebuffer ownership is intentionally explicit for correctness and
snapshot determinism. P8 must measure real FPS/slowest frame and decide whether
the two Gallery buffers should be reused with Runtime buffers. No reuse should
be introduced merely to reduce a static number without lifetime tests.
