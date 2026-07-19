#!/usr/bin/env python3
"""Rasterize the four SIGHT SMuFL glyphs into fixed 1-bit atlases.

The input must be Bravura.otf from commit 02e8ed29a29115df35007d1178cebaeee26c20e1.
Pillow is an offline build dependency only; firmware consumes packed PBM data.
"""

from __future__ import annotations

import argparse
import hashlib
import pathlib

from PIL import Image, ImageDraw, ImageFont


EXPECTED_SHA256 = "dca2d90c88437a701b1c2e71fa54e76f9fa41d7deee935d74dc871ea66ecfdd2"
GLYPHS = ("\ue050", "\ue062", "\ue262", "\ue260")  # G/F clef, sharp, flat.


def render_atlas(font_path: pathlib.Path, cell_width: int, height: int,
                 font_size: int) -> Image.Image:
    font = ImageFont.truetype(str(font_path), font_size)
    atlas = Image.new("1", (cell_width * len(GLYPHS), height), 0)
    for index, glyph in enumerate(GLYPHS):
        scratch = Image.new("L", (font_size * 2, font_size * 2), 0)
        draw = ImageDraw.Draw(scratch)
        bounds = draw.textbbox((0, 0), glyph, font=font)
        draw.text((-bounds[0], -bounds[1]), glyph, font=font, fill=255)
        crop_bounds = scratch.getbbox()
        if crop_bounds is None:
            raise ValueError(f"glyph U+{ord(glyph):04X} has no ink")
        glyph_image = scratch.crop(crop_bounds).point(lambda value: 255 if value >= 128 else 0, "1")
        if glyph_image.width > cell_width - 2 or glyph_image.height > height - 2:
            raise ValueError(f"glyph U+{ord(glyph):04X} does not fit atlas cell")
        x = index * cell_width + (cell_width - glyph_image.width) // 2
        y = (height - glyph_image.height) // 2
        atlas.paste(glyph_image, (x, y))
    return atlas


def pbm_text(image: Image.Image) -> str:
    width, height = image.size
    rows = ["P1", "# Bravura 02e8ed2 SMuFL subset: gClef fClef accidentalSharp accidentalFlat", f"{width} {height}"]
    pixels = image.load()
    for y in range(height):
        rows.append(" ".join("1" if pixels[x, y] else "0" for x in range(width)))
    return "\n".join(rows) + "\n"


def write_or_check(path: pathlib.Path, content: str, check: bool) -> None:
    if check:
        if not path.exists() or path.read_text(encoding="ascii") != content:
            raise SystemExit(f"generated glyph atlas is stale: {path}")
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="ascii")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("font", type=pathlib.Path)
    parser.add_argument("output", type=pathlib.Path)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    digest = hashlib.sha256(args.font.read_bytes()).hexdigest()
    if digest != EXPECTED_SHA256:
        raise SystemExit(f"unexpected Bravura source SHA-256: {digest}")
    atlases = (("sight_glyphs_t_embed.pbm", 24, 48, 25),
               ("sight_glyphs_sharp.pbm", 32, 64, 34))
    for name, cell_width, height, font_size in atlases:
        image = render_atlas(args.font, cell_width, height, font_size)
        write_or_check(args.output / name, pbm_text(image), args.check)


if __name__ == "__main__":
    main()
