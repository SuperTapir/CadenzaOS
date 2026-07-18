# Bundled App framebuffer baselines

These FNV-1a hashes cover the active bytes of the canonical row-major,
MSB-first framebuffer plus its dimensions. They were approved after rendering
PBM artifacts with `cadenza_dump_headless_pbm` and visually inspecting PNG
conversions on 2026-07-18.

Capture point: a fresh `HeadlessHost`; Launcher is its initial render. The two
Launcher Gallery-selection cases apply one `turn = 3` frame and prove the
longest bundled application name stays inside its card. Each other App is
opened directly and captured on the first stable render after the 0.32-second
transition completes. No normal App update runs after completion.

| Profile | App | Hash (decimal) |
| --- | --- | ---: |
| 320×170 | Launcher | 2968281691757874956 |
| 320×170 | Clock | 2172376209712558838 |
| 320×170 | Motion | 11046562126395087774 |
| 320×170 | Settings | 16657356307816133356 |
| 320×170 | Animation Gallery | 14139291840108583961 |
| 400×240 | Launcher | 5421283046709258962 |
| 400×240 | Clock | 8667913246713477979 |
| 400×240 | Motion | 2956592992690758759 |
| 400×240 | Settings | 5690656840055152546 |
| 400×240 | Animation Gallery | 13234575752027769465 |

Launcher selection baselines:

| Profile | Launcher state | Hash (decimal) |
| --- | --- | ---: |
| 320×170 | Animation Gallery selected | 16937710498554101448 |
| 400×240 | Animation Gallery selected | 5912879823780594669 |
