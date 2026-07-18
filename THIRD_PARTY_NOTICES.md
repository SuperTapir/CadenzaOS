# Third-party notices

The repository vendors only the files needed for deterministic host and
firmware builds. Commit pins below are authoritative; local research clones
are not build inputs.

## U8g2

- Upstream: <https://github.com/olikraus/u8g2>
- Commit: `ab9e48b2228351e9476682a70b7f3ee4909cd585`
- Code license: BSD-2-Clause; copied at `third_party/u8g2/licenses/LICENSE`
- Vendored scope: core 1-bit bitmap/box/buffer/circle/font/hvline/
  intersection/kerning/line/setup sources, the minimal U8x8 support those
  translation units reference, and the two public headers. No device driver is
  included.
- Font: `u8g2_font_6x10_tf`, whose upstream metadata declares the terminal
  emulator font public domain. Only this font array is copied from
  `u8g2_fonts.c`.

U8g2 fonts have independent provenance. No other font is approved merely by
being present upstream.

## doctest

- Upstream: <https://github.com/doctest/doctest>
- Commit: `d44d4f6e66232d716af82f00a063759e9d0e50d6`
- License: MIT; copied at `third_party/licenses/doctest-LICENSE.txt`
- Vendored file: `doctest.h`, host tests only.

## stb_image_write

- Upstream: <https://github.com/nothings/stb>
- Commit: `31c1ad37456438565541f4919958214b6e762fb4`
- License choice: MIT; copied at `third_party/licenses/stb-LICENSE.txt`
- Vendored file: `stb_image_write.h`, desktop PNG output only.

## gif-h

- Upstream: <https://github.com/charlietangora/gif-h>
- Commit: `70b645280d5e687f5217177c9cfa2889b0a2ad5f`
- License: Unlicense; copied at `third_party/licenses/gif-h-LICENSE.txt`
- Vendored file: `gif.h`, desktop convenience recording only.
