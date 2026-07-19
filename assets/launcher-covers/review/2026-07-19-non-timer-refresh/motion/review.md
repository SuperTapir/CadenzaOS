# MOTION v1 visual review

- Exact title: pass (`MOTION`, once, fully inside safe margins).
- Functional hook: pass (one large hollow mass ring on one track, with visible elastic lag).
- Element budget: pass (title, ring, track, one target dot, three broad lag marks; no extra instrument or panel).
- Hard-threshold recognition: pass at 350×155 and 280×124; title, ring and travel direction remain recognizable without gray.
- Reduction: pass; counters stay open and critical strokes remain comfortably above two pixels.
- Tonal discipline: pass for candidate review; the only gray area is a broad motion/overlap plane and may be simplified further during approved ordered-dither preparation.
- Contour cleanup: pass after `prepare_cover.py --edge-cleanup`; nearest-neighbor 4× A/B removes Bayer fringe from the title, ring and baseline while retaining the broad motion-plane dither. Formal PBM hashes were regenerated from this cleaned output.
- House style: pass; contour confidence and restraint calibrate against TIMER while the bright flat typographic composition remains distinct.
- Static Cover contract: candidate is one still image with no border or Launcher chrome.
- Visual approval: approved by the user in this task on 2026-07-19 ("我觉得这个挺好的").

Locked TIMER SHA-256 before integration:

- `source/timer.png`: `e5e8455f652268f15614b8d48720e5e11304a3b4d70e30ca62af1bd938a14480`
- `timer.pbm`: `8406756202a4a9de974d133cd4e5de740819c34419d90f0ff47fdb51836a85bb`
- `timer_t_embed.pbm`: `87a00f5298e5bfc682a508608ab1b03030f8649f6677be9ef634b323cd764cb7`
