#!/usr/bin/env python3
"""Convert pinned Playdate .fnt/image tables to Cadenza bitmap fonts."""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import re
from typing import NamedTuple

from PIL import Image


FIRST_GLYPH = 0x20
LAST_GLYPH = 0x7E
REQUIRED_GLYPHS = "".join(chr(code) for code in range(FIRST_GLYPH, LAST_GLYPH + 1))

PINNED_FONTS = (
    ("Roobert-24-Medium", "kRoobert24Medium"),
    ("Roobert-20-Medium", "kRoobert20Medium"),
    ("Roobert-11-Bold", "kRoobert11Bold"),
    ("Roobert-11-Medium", "kRoobert11Medium"),
    ("Roobert-10-Bold", "kRoobert10Bold"),
    ("Roobert-9-Mono-Condensed", "kRoobert9MonoCondensed"),
)

PINNED_SHA256 = {
    "Roobert-24-Medium.fnt": "e6689163e1fed6df3377c08168871781f3c05430714dc52acdeda6f5576918d6",
    "Roobert-24-Medium-table-36-36.png": "aba8dac6721b2866b9f81d73dc432e36989f681f385e2e2ed8ffa59b1dfaef65",
    "Roobert-20-Medium.fnt": "8a53198879ccabad77567c7f50da13cd83b435ca4cda47b4c97356c204f18fb8",
    "Roobert-20-Medium-table-32-32.png": "7e3c348656cfd6230ac2f2cea8a694f979943f1609456379e678d78bf8fecf39",
    "Roobert-11-Bold.fnt": "48c53b2d4b8c17e25037eca3920121c791340d89795ee0e21a5cb311d0c94df7",
    "Roobert-11-Bold-table-22-22.png": "d85b0d464dd812f0beaf8d2996b401a1236938cd380330f38ac237c8efa972b4",
    "Roobert-11-Medium.fnt": "e1f6ba01ae24e26579b47dc569fc0e053ab3dbf3556073f61293d7a255604b88",
    "Roobert-11-Medium-table-22-22.png": "9c9dbfe59813a795fc500e07d5db8ffcf1de8e86e81cabe9cd3c05cc43466a1b",
    "Roobert-10-Bold.fnt": "4a58387aa5898d3920abc7300ffb0dbd5fe19ba708568b191f6c51831e236fed",
    "Roobert-10-Bold-table-12-14.png": "721a06c9dbf321f91438e589e2c129d87177e28dbeaf66722a6b73657a051874",
    "Roobert-9-Mono-Condensed.fnt": "50ecae700834ddcbb1ca2e18224203f764a84276a97a6ecd5fffcb34a0d3861c",
    "Roobert-9-Mono-Condensed-table-8-14.png": "afde90d6b06a73ceeb8ead0ca7db2a5118349b638bf8e15d424e19b8248fdcec",
}


class Glyph(NamedTuple):
    offset: int
    width: int
    stride: int


class PackedFont(NamedTuple):
    stem: str
    symbol: str
    height: int
    tracking: int
    data: bytes
    glyphs: tuple[Glyph, ...]
    kerning: tuple[tuple[int, int], ...]


def digest(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def pack_rows(width: int, height: int, pixels: list[int]) -> bytes:
    if width <= 0 or height <= 0 or len(pixels) != width * height:
        raise ValueError("invalid glyph geometry")
    stride = (width + 7) // 8
    packed = bytearray(stride * height)
    for y in range(height):
        for x in range(width):
            if pixels[y * width + x]:
                packed[y * stride + x // 8] |= 0x80 >> (x & 7)
    return bytes(packed)


def source_glyphs(lines: list[str]) -> tuple[list[tuple[str, int]], int, dict[str, int]]:
    glyphs: list[tuple[str, int]] = []
    tracking = 0
    kerning: dict[str, int] = {}
    for line in lines:
        if line.startswith("--metrics="):
            metrics = json.loads(line.removeprefix("--metrics="))
            kerning = {
                pair: int(values[0])
                for pair, values in metrics.get("pairs", {}).items()
                if len(pair) == 2 and values and int(values[0]) != 0
            }
            break
    reading_glyphs = True
    for raw in lines:
        line = raw.strip()
        if not line or line.startswith("--"):
            continue
        if line.startswith("tracking="):
            tracking = int(line.split("=", 1)[1])
            continue
        parts = line.split()
        if len(parts) != 2:
            continue
        token, width_text = parts
        if token != "space" and len(token) != 1:
            reading_glyphs = False
        if not reading_glyphs:
            if len(token) == 2:
                adjustment = int(width_text)
                if adjustment != 0:
                    kerning[token] = adjustment
            continue
        glyphs.append((" " if token == "space" else token, int(width_text)))
    return glyphs, tracking, kerning


def parse_font(fnt_path: pathlib.Path, png_path: pathlib.Path,
               stem: str, symbol: str) -> PackedFont:
    match = re.search(r"-table-(\d+)-(\d+)\.png$", png_path.name)
    if not match:
        raise ValueError(f"font image table has no cell dimensions: {png_path}")
    cell_width, cell_height = (int(value) for value in match.groups())
    if cell_width <= 0 or cell_height <= 0:
        raise ValueError("font cell dimensions must be positive")

    entries, tracking, source_kerning = source_glyphs(
        fnt_path.read_text(encoding="utf-8").splitlines()
    )
    by_char = {char: (index, width) for index, (char, width) in enumerate(entries)}
    missing = [char for char in REQUIRED_GLYPHS if char not in by_char]
    if missing:
        display = ", ".join(repr(char) for char in missing[:8])
        raise ValueError(f"missing required glyph(s): {display}")

    table = Image.open(png_path).convert("RGBA")
    if table.width % cell_width != 0 or table.height % cell_height != 0:
        raise ValueError("font image table is not an integer grid")
    columns = table.width // cell_width
    capacity = columns * (table.height // cell_height)
    if len(entries) > capacity:
        raise ValueError("font metrics contain more glyphs than the image table")

    font_data = bytearray()
    glyphs: list[Glyph] = []
    for char in REQUIRED_GLYPHS:
        glyph_index, width = by_char[char]
        if width <= 0 or width > cell_width:
            raise ValueError(f"glyph {char!r} width is outside its cell")
        left = (glyph_index % columns) * cell_width
        top = (glyph_index // columns) * cell_height
        cell = table.crop((left, top, left + cell_width, top + cell_height))
        rgba = cell.load()
        pixels = []
        for y in range(cell_height):
            for x in range(width):
                red, green, blue, alpha = rgba[x, y]
                pixels.append(int(alpha > 127 and red + green + blue < 384))
        packed = pack_rows(width, cell_height, pixels)
        glyphs.append(Glyph(len(font_data), width, (width + 7) // 8))
        font_data.extend(packed)

    kerning = []
    for pair, adjustment in source_kerning.items():
        first, second = ord(pair[0]), ord(pair[1])
        if FIRST_GLYPH <= first <= LAST_GLYPH and FIRST_GLYPH <= second <= LAST_GLYPH:
            if not -128 <= adjustment <= 127:
                raise ValueError(f"kerning adjustment outside int8: {pair!r}")
            kerning.append(((first << 8) | second, adjustment))
    kerning.sort()
    return PackedFont(stem, symbol, cell_height, tracking, bytes(font_data),
                      tuple(glyphs), tuple(kerning))


def byte_rows(values: bytes, indent: str = "    ") -> str:
    if not values:
        return indent
    return ",\n".join(
        indent + ", ".join(f"0x{value:02X}" for value in values[offset:offset + 12])
        for offset in range(0, len(values), 12)
    )


def render_font(font: PackedFont) -> tuple[str, int]:
    glyph_rows = ",\n".join(
        f"    {{{glyph.offset}U, {glyph.width}U, {glyph.stride}U}}"
        for glyph in font.glyphs
    )
    kerning_rows = ",\n".join(
        f"    {{0x{key:04X}U, {adjustment}}}" for key, adjustment in font.kerning
    )
    if not kerning_rows:
        kerning_rows = "    {0U, 0}"
    estimated_bytes = len(font.data) + len(font.glyphs) * 8 + len(font.kerning) * 4
    text = f"""
// Source: Playdate SDK 3.0.6 Resources/Fonts/Roobert/{font.stem}
// License: CC BY 4.0; Playdate fonts from Panic, Inc.; converted by CadenzaOS.
inline constexpr std::uint8_t {font.symbol}Data[] = {{
{byte_rows(font.data)}
}};

inline constexpr BitmapGlyph {font.symbol}Glyphs[] = {{
{glyph_rows}
}};

inline constexpr FontKerningPair {font.symbol}Kerning[] = {{
{kerning_rows}
}};

inline constexpr BitmapFont {font.symbol}{{
    {font.symbol}Data,
    sizeof({font.symbol}Data),
    {font.symbol}Glyphs,
    {len(font.glyphs)}U,
    {font.symbol}Kerning,
    {len(font.kerning)}U,
    {FIRST_GLYPH}U,
    {font.height}U,
    {font.height},
    0,
    {font.tracking}
}};
inline constexpr std::size_t {font.symbol}CompiledBytes = {estimated_bytes}U;
"""
    return text, estimated_bytes


def verify_pins(source_dir: pathlib.Path) -> None:
    for filename, expected in PINNED_SHA256.items():
        path = source_dir / filename
        if not path.exists():
            raise ValueError(f"missing pinned font source: {path}")
        actual = digest(path)
        if actual != expected:
            raise ValueError(
                f"pinned font hash mismatch for {filename}: {actual} != {expected}"
            )


def generate(source_dir: pathlib.Path) -> str:
    verify_pins(source_dir)
    fonts = []
    for stem, symbol in PINNED_FONTS:
        png_matches = sorted(source_dir.glob(f"{stem}-table-*.png"))
        if len(png_matches) != 1:
            raise ValueError(f"expected one image table for {stem}")
        fonts.append(parse_font(source_dir / f"{stem}.fnt", png_matches[0],
                                stem, symbol))
    rendered = []
    total = 0
    for font in fonts:
        text, size = render_font(font)
        rendered.append(text)
        total += size
    return f"""#pragma once

#include <cstddef>
#include <cstdint>

#include "cadenza/core/bitmap_font.h"

// Generated by tools/convert_playdate_fonts.py. Do not edit.
// Glyph manifest: printable ASCII U+0020..U+007E.
// Total unique font bytes: {total}

namespace cadenza::fonts {{
{''.join(rendered)}
inline constexpr std::size_t kTypographyCompiledBytes = {total}U;
inline constexpr std::size_t kTypographyFlashBudget = 40000U;
static_assert(kTypographyCompiledBytes <= kTypographyFlashBudget,
              "typography font resources exceed flash budget");

}}  // namespace cadenza::fonts
"""


def generated_sha256(output: str) -> str:
    return hashlib.sha256(output.encode("utf-8")).hexdigest()


def write_or_check(output: pathlib.Path, generated: str, check: bool) -> None:
    if check:
        if not output.exists() or output.read_text(encoding="utf-8") != generated:
            raise SystemExit(f"generated font is stale: {output}")
        return
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(generated, encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source_dir", type=pathlib.Path)
    parser.add_argument("output", type=pathlib.Path)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    generated = generate(args.source_dir)
    write_or_check(args.output, generated, args.check)
    if not args.check:
        print(f"generated {args.output} sha256={generated_sha256(generated)}")


if __name__ == "__main__":
    main()
