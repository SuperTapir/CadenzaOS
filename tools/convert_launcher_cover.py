#!/usr/bin/env python3
"""Convert a Launcher cover master PNG directly to a target-size 1-bit PBM."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from PIL import Image, ImageOps


def converted_pixels(source: Path, width: int, height: int, threshold: int) -> list[int]:
    with Image.open(source) as image:
        grayscale = image.convert("L")
        fitted = ImageOps.fit(
            grayscale,
            (width, height),
            method=Image.Resampling.LANCZOS,
            centering=(0.5, 0.5),
        )
        return [
            1 if value < threshold else 0
            for value in fitted.get_flattened_data()
        ]


def read_pbm_pixels(path: Path) -> tuple[tuple[int, int], list[int]]:
    with Image.open(path) as image:
        monochrome = image.convert("1")
        return monochrome.size, [
            1 if value == 0 else 0
            for value in monochrome.get_flattened_data()
        ]


def pbm_text(
    source: Path, width: int, height: int, threshold: int, pixels: list[int]
) -> str:
    rows = []
    for y in range(height):
        start = y * width
        rows.append(" ".join(str(value) for value in pixels[start : start + width]))
    return (
        "P1\n"
        f"# Direct LANCZOS fit from {source.name}; {width}x{height}; "
        f"grayscale threshold {threshold}/255\n"
        f"{width} {height}\n"
        + "\n".join(rows)
        + "\n"
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--width", required=True, type=int)
    parser.add_argument("--height", required=True, type=int)
    parser.add_argument("--threshold", required=True, type=int)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()

    if args.width <= 0 or args.height <= 0:
        parser.error("width and height must be positive")
    if not 1 <= args.threshold <= 255:
        parser.error("threshold must be in [1, 255]")

    expected = converted_pixels(
        args.source, args.width, args.height, args.threshold
    )
    if args.check:
        if not args.output.exists():
            print(f"missing generated asset: {args.output}", file=sys.stderr)
            return 1
        actual_size, actual = read_pbm_pixels(args.output)
        expected_size = (args.width, args.height)
        if actual_size != expected_size:
            print(
                f"size mismatch for {args.output}: {actual_size} != {expected_size}",
                file=sys.stderr,
            )
            return 1
        differing = sum(left != right for left, right in zip(expected, actual))
        if differing:
            print(
                f"pixel mismatch for {args.output}: {differing} pixels differ from "
                f"direct conversion of {args.source}",
                file=sys.stderr,
            )
            return 1
        return 0

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        pbm_text(args.source, args.width, args.height, args.threshold, expected),
        encoding="ascii",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
