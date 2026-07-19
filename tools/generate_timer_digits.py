#!/usr/bin/env python3
"""Generate Cadenza's native 1-bit industrial Timer numeral atlases."""

from __future__ import annotations

import argparse
from pathlib import Path


PROFILES = {"t_embed": (50, 28, 84), "sharp": (68, 36, 120)}
SEGMENTS = {
    "0": "abcdef", "1": "bc", "2": "abdeg", "3": "abcdg",
    "4": "bcfg", "5": "acdfg", "6": "acdefg", "7": "abc",
    "8": "abcdefg", "9": "abcdfg",
}

Point = tuple[float, float]
Polygon = list[Point]


def digit_polygons(left: float, cell: float, height: float, value: str) -> list[Polygon]:
    t = height * 0.105
    bevel = t * 0.48
    inset = cell * 0.17
    right, left_ink = left + cell - inset, left + inset
    middle, top, bottom = height * 0.50, height * 0.035, height * 0.965
    upper_top, upper_bottom = top + t * 0.58, middle - t * 0.35
    lower_top, lower_bottom = middle + t * 0.35, bottom - t * 0.58
    horizontal = {
        "a": (left_ink, right, top), "g": (left_ink, right, middle - t / 2),
        "d": (left_ink, right, bottom - t),
    }
    vertical = {
        "f": (left_ink - t * 0.55, upper_top, upper_bottom),
        "b": (right - t * 0.45, upper_top, upper_bottom),
        "e": (left_ink - t * 0.55, lower_top, lower_bottom),
        "c": (right - t * 0.45, lower_top, lower_bottom),
    }
    result: list[Polygon] = []
    for segment in SEGMENTS[value]:
        if segment in horizontal:
            x1, x2, y = horizontal[segment]
            result.append([(x1 + bevel, y), (x2 - bevel, y),
                           (x2, y + t / 2), (x2 - bevel, y + t),
                           (x1 + bevel, y + t), (x1, y + t / 2)])
        else:
            x, y1, y2 = vertical[segment]
            result.append([(x + t / 2, y1), (x + t, y1 + bevel),
                           (x + t, y2 - bevel), (x + t / 2, y2),
                           (x, y2 - bevel), (x, y1 + bevel)])
    return result


def colon_polygons(left: float, width: float, height: float) -> list[Polygon]:
    center_x, size = left + width / 2, height * 0.115
    bevel = size * 0.22
    result = []
    for center_y in (height * 0.36, height * 0.64):
        x1, x2 = center_x - size / 2, center_x + size / 2
        y1, y2 = center_y - size / 2, center_y + size / 2
        result.append([(x1 + bevel, y1), (x2 - bevel, y1),
                       (x2, y1 + bevel), (x2, y2 - bevel),
                       (x2 - bevel, y2), (x1 + bevel, y2),
                       (x1, y2 - bevel), (x1, y1 + bevel)])
    return result


def atlas_polygons(cell: int, colon: int, height: int) -> list[Polygon]:
    result: list[Polygon] = []
    for value in range(10):
        result.extend(digit_polygons(value * cell, cell, height, str(value)))
    result.extend(colon_polygons(cell * 10, colon, height))
    return result


def contains(polygon: Polygon, x: float, y: float) -> bool:
    inside = False
    previous = polygon[-1]
    for current in polygon:
        if ((current[1] > y) != (previous[1] > y)):
            crossing = ((previous[0] - current[0]) * (y - current[1]) /
                        (previous[1] - current[1]) + current[0])
            if x < crossing:
                inside = not inside
        previous = current
    return inside


def raster(cell: int, colon: int, height: int, samples: int = 4) -> list[int]:
    width = cell * 10 + colon
    polygons = atlas_polygons(cell, colon, height)
    values: list[int] = []
    for y in range(height):
        for x in range(width):
            hits = 0
            for sy in range(samples):
                for sx in range(samples):
                    px = x + (sx + 0.5) / samples
                    py = y + (sy + 0.5) / samples
                    if any(contains(polygon, px, py) for polygon in polygons):
                        hits += 1
            values.append(round(hits * 255 / (samples * samples)))
    return values


def pbm_text(width: int, height: int, values: list[int], profile: str) -> str:
    rows = [" ".join("1" if values[y * width + x] >= 128 else "0"
                     for x in range(width)) for y in range(height)]
    return ("P1\n# Cadenza-authored Timer industrial numeral atlas; "
            f"native {profile}; deterministic 4x supersampling then threshold\n"
            f"{width} {height}\n" + "\n".join(rows) + "\n")


def svg_text(cell: int, colon: int, height: int) -> str:
    width = cell * 10 + colon
    paths = []
    for polygon in atlas_polygons(cell, colon, height):
        points = " ".join(f"{x:.3f},{y:.3f}" for x, y in polygon)
        paths.append(f'  <polygon points="{points}"/>')
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {height}" '
            f'width="{width * 4}" height="{height * 4}">\n'
            '  <rect width="100%" height="100%" fill="white"/>\n'
            '  <g fill="black">\n' + "\n".join(paths) +
            '\n  </g>\n</svg>\n')


def write_or_check(path: Path, output: str, check: bool) -> None:
    if check:
        if not path.exists() or path.read_text(encoding="utf-8") != output:
            raise SystemExit(f"stale Timer digit asset: {path}")
    else:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(output, encoding="utf-8")


def generate(root: Path, check: bool) -> None:
    assets = root / "assets" / "timer-digits"
    for profile, (cell, colon, height) in PROFILES.items():
        width = cell * 10 + colon
        values = raster(cell, colon, height)
        write_or_check(assets / f"timer_digits_{profile}.pbm",
                       pbm_text(width, height, values, profile), check)
        if profile == "sharp":
            write_or_check(assets / "source" / "timer_digits_master.svg",
                           svg_text(cell, colon, height), check)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    generate(Path(__file__).resolve().parents[1], args.check)


if __name__ == "__main__":
    main()
