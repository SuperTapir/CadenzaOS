#!/usr/bin/env python3
"""Convert an ASCII PBM (P1) image to a packed Cadenza bitmap header."""

from __future__ import annotations

import argparse
import pathlib
import re


def tokens(path: pathlib.Path) -> list[str]:
    meaningful: list[str] = []
    for line in path.read_text(encoding="utf-8").splitlines():
        meaningful.extend(line.split("#", 1)[0].split())
    return meaningful


def read_pbm(path: pathlib.Path) -> tuple[int, int, list[int]]:
    values = tokens(path)
    if len(values) < 3 or values[0] != "P1":
        raise ValueError("input must be an ASCII PBM (P1)")
    width, height = int(values[1]), int(values[2])
    if width <= 0 or height <= 0:
        raise ValueError("bitmap dimensions must be positive")
    pixels = [int(value) for value in values[3:]]
    if len(pixels) != width * height or any(pixel not in (0, 1) for pixel in pixels):
        raise ValueError("PBM pixel count or value is invalid")
    return width, height, pixels


def pack(width: int, height: int, pixels: list[int]) -> list[int]:
    stride = (width + 7) // 8
    packed = [0] * (stride * height)
    for y in range(height):
        for x in range(width):
            if pixels[y * width + x]:
                packed[y * stride + x // 8] |= 0x80 >> (x & 7)
    return packed


def identifier(value: str) -> str:
    result = re.sub(r"[^A-Za-z0-9_]", "_", value)
    if not result or result[0].isdigit():
        result = "bitmap_" + result
    return result


def render(symbol: str, width: int, height: int, packed: list[int],
           provenance: str) -> str:
    stride = (width + 7) // 8
    rows = []
    for offset in range(0, len(packed), 12):
        rows.append("    " + ", ".join(f"0x{byte:02X}" for byte in packed[offset:offset + 12]))
    data = ",\n".join(rows)
    return f"""#pragma once

#include <cstdint>
#include "cadenza/core/mono_canvas.h"

// Provenance: {provenance}
inline constexpr std::uint8_t {symbol}_data[] = {{
{data}
}};
inline constexpr cadenza::BitmapView {symbol}{{
    {symbol}_data, {width}, {height}, {stride}
}};
"""


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=pathlib.Path)
    parser.add_argument("output", type=pathlib.Path)
    parser.add_argument("--symbol", required=True)
    parser.add_argument(
        "--provenance",
        required=True,
        help="original/public-domain/license reference recorded in the header",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="fail if output does not exactly match the generated header",
    )
    args = parser.parse_args()
    width, height, pixels = read_pbm(args.input)
    output = render(identifier(args.symbol), width, height, pack(width, height, pixels),
                    args.provenance)
    if args.check:
        if not args.output.exists() or args.output.read_text(encoding="utf-8") != output:
            raise SystemExit(f"generated bitmap is stale: {args.output}")
        return
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(output, encoding="utf-8")


if __name__ == "__main__":
    main()
