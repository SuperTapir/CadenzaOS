# Bundled App framebuffer baselines

These FNV-1a hashes cover the active bytes of the canonical row-major,
MSB-first framebuffer plus its dimensions. They were approved after rendering
PBM artifacts with `cadenza_dump_headless_pbm` and visually inspecting PNG
conversions on 2026-07-18.

Capture point: a fresh `HeadlessHost`; Launcher is its initial render. Each
other App is opened directly and captured on the first stable render after the
0.32-second transition completes. No normal App update runs after completion.

| Profile | App | Hash (decimal) |
| --- | --- | ---: |
| 320×170 | Launcher | 15479386324999666307 |
| 320×170 | Clock | 2172376209712558838 |
| 320×170 | Motion | 11046562126395087774 |
| 320×170 | Settings | 16015422347803220551 |
| 400×240 | Launcher | 5367135371095058079 |
| 400×240 | Clock | 8667913246713477979 |
| 400×240 | Motion | 2956592992690758759 |
| 400×240 | Settings | 16871251489666615369 |
