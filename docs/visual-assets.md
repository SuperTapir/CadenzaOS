# Visual asset manifest

Every visual resource compiled into the runtime is listed here. Reference
project screenshots, logos, texture tables, and example art are not copied.

| Resource | Location | Provenance | Redistribution |
| --- | --- | --- | --- |
| `u8g2_font_6x10_tf` | `third_party/u8g2/src/u8g2_font_6x10_tf.c` | U8g2 upstream terminal-emulator font metadata marks the font public domain; vendored at the pinned U8g2 commit | Public domain font data; U8g2 wrapper code is BSD-2-Clause |
| Roobert 24 Medium / 20 Medium / 11 Bold / 11 Medium / 10 Bold / 9 Mono Condensed source fonts | `assets/fonts/roobert-source/` and generated core descriptors | Playdate SDK 3.0.6 fonts from Panic, Inc.; pinned hashes and conversion are documented in the source manifest | CC BY 4.0 with attribution; converted glyph bitmaps remain attributed |
| ordered 8×8 threshold table | `lib/cadenza_core/src/mono_canvas.cpp` | Cadenza-authored recursive Bayer ordering | Original project code |
| Gallery four-frame 2×4 sprite | `lib/cadenza_core/src/gallery.cpp` | Cadenza-authored for deterministic state-animation QA | Original project code |
| Timer Launcher Cover | `assets/launcher-covers/source/timer.png` and generated PBM/header | AI-assisted original 3D artwork approved 2026-07-19 | Original project artwork |
| Motion Launcher Cover | `assets/launcher-covers/source/motion.png` and generated PBM/header | User-provided, AI-assisted original artwork approved 2026-07-18 | Original project artwork |
| Settings Launcher Cover | `assets/launcher-covers/source/settings.png` and generated PBM/header | User-provided, AI-assisted original artwork approved 2026-07-18 | Original project artwork |
| Animation Gallery Launcher Cover | `assets/launcher-covers/source/gallery.png` and generated PBM/header | User-provided, AI-assisted original artwork approved 2026-07-18 | Original project artwork |

`font8x8` is present only in the ignored research clone and is not a bundled
asset. New fonts, textures, converted bitmaps, or atlases must add a row here
before entering a build target.
