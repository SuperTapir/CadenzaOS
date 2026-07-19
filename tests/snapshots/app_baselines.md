# Bundled App framebuffer baselines

These FNV-1a hashes cover the active bytes of the canonical row-major,
MSB-first framebuffer plus its dimensions. They were approved after rendering
PBM artifacts with `cadenza_dump_headless_pbm` and visually inspecting PNG
conversions on 2026-07-19.

Capture point: a fresh `HeadlessHost`; Launcher is its initial render. The two
Launcher Gallery-selection cases apply one `turn = 3` frame and prove the
longest bundled application name stays inside its card. Each other App is
opened directly and captured on the first stable render after the 0.32-second
transition completes. No normal App update runs after completion.

| Profile | App | Hash (decimal) |
| --- | --- | ---: |
| 320×170 | Launcher | 4407872121496200056 |
| 320×170 | Timer Ready 10:00 | 12921254497184521768 |
| 320×170 | Motion | 1593956483296057867 |
| 320×170 | Settings | 10255963290226060893 |
| 320×170 | Animation Gallery | 14139291840108583961 |
| 400×240 | Launcher | 14360345327951497471 |
| 400×240 | Timer Ready 10:00 | 4523606151491982558 |
| 400×240 | Motion | 2802791382376082090 |
| 400×240 | Settings | 5598788588305196167 |
| 400×240 | Animation Gallery | 13234575752027769465 |

Settings hashes were re-approved on 2026-07-19 after adding selection-follow
scroll for overflow on 320×170 (seven rows no longer force a clipped static
stack). 400×240 still fits without scrolling; both profile PNGs were inspected.

Timer hashes were re-approved on 2026-07-19 after adding native industrial
numerals, centering the enlarged time body, correcting `HOLD: MENU`, and applying
the approved 2D-title/3D-dial Launcher Cover. Both profile PNGs were visually
inspected.

Launcher selection baselines:

| Profile | Launcher state | Hash (decimal) |
| --- | --- | ---: |
| 320×170 | Animation Gallery selected | 15533885305252050834 |
| 400×240 | Animation Gallery selected | 6567266582574601646 |

Launcher and handoff hashes were re-approved on 2026-07-19 after replacing the
legacy Clock artwork with the approved `TIMER` cover. The horizontal/vertical
track endpoints, launch/return transition frames, and warped Menu closing frame
were visually inspected at both profiles before updating their test constants.
The Menu opening keyframe was re-approved again after the 24 FPS reference
comparison added a 22% bottom-edge lead-in delay; its first visible frame now
keeps the top edge settled while the bottom remains at the right edge.
