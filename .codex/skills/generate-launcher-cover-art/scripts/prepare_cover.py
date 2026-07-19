#!/usr/bin/env python3
"""Prepare a smooth grayscale Cover master as deterministic 1-bit assets."""

from __future__ import annotations

import argparse
import pathlib
import shutil
import subprocess
import sys

try:
    from PIL import Image, ImageFilter, ImageOps
except ImportError as error:
    raise SystemExit("prepare_cover.py requires Pillow") from error


BAYER_4X4 = (
    (0, 8, 2, 10),
    (12, 4, 14, 6),
    (3, 11, 1, 9),
    (15, 7, 13, 5),
)


def fit_without_crop(image: Image.Image, size: tuple[int, int]) -> Image.Image:
    rgba = image.convert("RGBA")
    white = Image.new("RGBA", rgba.size, (255, 255, 255, 255))
    white.alpha_composite(rgba)
    grayscale = white.convert("L")
    contained = ImageOps.contain(grayscale, size, Image.Resampling.LANCZOS)
    result = Image.new("L", size, 255)
    result.paste(contained, ((size[0] - contained.width) // 2,
                             (size[1] - contained.height) // 2))
    return result


def antialias_edge_mask(
    image: Image.Image,
    dark_limit: int = 64,
    light_limit: int = 191,
) -> list[bool]:
    """Find and guard soft pixels around a black/white silhouette edge.

    Broad gray material planes do not normally touch both extremes inside the
    same 3x3 neighborhood, so they remain available for ordered dither. Expand
    the detected transition by one pixel so patterned gray cannot form a fuzzy
    fringe directly against a critical contour.
    """
    pixels = image.load()
    width, height = image.size
    mask = [False] * (width * height)
    for y in range(height):
        top = max(0, y - 1)
        bottom = min(height, y + 2)
        for x in range(width):
            value = int(pixels[x, y])
            if value == 0 or value == 255:
                continue
            left = max(0, x - 1)
            right = min(width, x + 2)
            has_dark = False
            has_light = False
            for neighbor_y in range(top, bottom):
                for neighbor_x in range(left, right):
                    neighbor = int(pixels[neighbor_x, neighbor_y])
                    has_dark = has_dark or neighbor <= dark_limit
                    has_light = has_light or neighbor >= light_limit
            mask[y * width + x] = has_dark and has_light
    mask_image = Image.new("L", image.size, 0)
    mask_image.putdata([255 if value else 0 for value in mask])
    guarded = mask_image.filter(ImageFilter.MaxFilter(3))
    return [value != 0 for value in guarded.getdata()]


def ordered_one_bit(
    image: Image.Image,
    levels: int,
    solid_light_region: tuple[float, float, float, float] | None = None,
    edge_cleanup: bool = False,
) -> list[int]:
    output: list[int] = []
    pixels = image.load()
    clean_edges = antialias_edge_mask(image) if edge_cleanup else None
    hinted = None
    if solid_light_region is not None:
        left, top, right, bottom = solid_light_region
        bounds = (
            round(left * image.width),
            round(top * image.height),
            round(right * image.width),
            round(bottom * image.height),
        )
        core = Image.new("L", image.size, 0)
        core_pixels = core.load()
        for y in range(bounds[1], bounds[3]):
            for x in range(bounds[0], bounds[2]):
                if pixels[x, y] >= 224:
                    core_pixels[x, y] = 255
        filter_size = 5 if image.width >= 350 else 3
        hinted = core.filter(ImageFilter.MaxFilter(filter_size)).load()
    for y in range(image.height):
        for x in range(image.width):
            index = y * image.width + x
            if ((hinted is not None and hinted[x, y] != 0) or
                    (clean_edges is not None and clean_edges[index])):
                output.append(0 if pixels[x, y] >= 128 else 1)
                continue
            blackness = 255 - int(pixels[x, y])
            band = round(blackness * (levels - 1) / 255)
            coverage = round(band * 16 / (levels - 1))
            output.append(1 if BAYER_4X4[y & 3][x & 3] < coverage else 0)
    return output


def hard_threshold_one_bit(image: Image.Image, threshold: int = 128) -> list[int]:
    """Return a texture-independent black/white recognition preview."""
    return [1 if int(value) < threshold else 0 for value in image.getdata()]


def write_pbm(path: pathlib.Path, size: tuple[int, int], pixels: list[int],
              source_name: str, levels: int, edge_cleanup: bool) -> None:
    width, height = size
    with path.open("w", encoding="ascii", newline="\n") as stream:
        stream.write("P1\n")
        cleanup = "; antialias edge cleanup" if edge_cleanup else ""
        stream.write(
            f"# No-crop derivative of {source_name}; ordered 4x4; "
            f"{levels} tonal bands{cleanup}\n{width} {height}\n"
        )
        for y in range(height):
            row = pixels[y * width:(y + 1) * width]
            stream.write(" ".join(str(pixel) for pixel in row) + "\n")


def preview(path: pathlib.Path, size: tuple[int, int], pixels: list[int],
            reflective: bool, scale: int = 1) -> None:
    ink = (50, 47, 40) if reflective else (0, 0, 0)
    paper = (177, 174, 167) if reflective else (255, 255, 255)
    image = Image.new("RGB", size)
    image.putdata([ink if pixel else paper for pixel in pixels])
    if scale > 1:
        image = image.resize(
            (size[0] * scale, size[1] * scale), Image.Resampling.NEAREST
        )
    image.save(path)


def pascal_case(value: str) -> str:
    return "".join(part.capitalize() for part in value.replace("-", "_").split("_")
                   if part)


def pack(python: str, pack_tool: pathlib.Path, pbm: pathlib.Path,
         header: pathlib.Path, symbol: str, provenance: str) -> None:
    header.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(
        [python, str(pack_tool), str(pbm), str(header), "--symbol", symbol,
         "--provenance", provenance],
        check=True,
    )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=pathlib.Path)
    parser.add_argument("output", type=pathlib.Path,
                        help="Launcher Cover asset directory")
    parser.add_argument("--name", required=True,
                        help="asset stem, for example timer or animation_gallery")
    parser.add_argument("--levels", type=int, default=5,
                        help="intentional grayscale bands before ordered dither (3-6)")
    parser.add_argument(
        "--solid-light-region", nargs=4, type=float,
        metavar=("LEFT", "TOP", "RIGHT", "BOTTOM"),
        help=("normalized region whose near-white contours use solid coverage "
              "thresholding instead of ordered dither"),
    )
    parser.add_argument(
        "--edge-cleanup", action="store_true",
        help=("harden antialiased pixels that bridge near-black and near-white "
              "contours while preserving broad gray planes for dither"),
    )
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--pack-tool", type=pathlib.Path)
    parser.add_argument("--header-dir", type=pathlib.Path)
    args = parser.parse_args()
    if not 3 <= args.levels <= 6:
        parser.error("--levels must be between 3 and 6")
    if bool(args.pack_tool) != bool(args.header_dir):
        parser.error("--pack-tool and --header-dir must be provided together")
    solid_light_region = None
    if args.solid_light_region is not None:
        solid_light_region = tuple(args.solid_light_region)
        left, top, right, bottom = solid_light_region
        if not (0.0 <= left < right <= 1.0 and
                0.0 <= top < bottom <= 1.0):
            parser.error("--solid-light-region must be normalized and ordered")

    source_dir = args.output / "source"
    review_dir = args.output / "review"
    source_path = source_dir / f"{args.name}.png"
    if not args.check:
        args.output.mkdir(parents=True, exist_ok=True)
        source_dir.mkdir(exist_ok=True)
        review_dir.mkdir(exist_ok=True)
    if not args.check and args.input.resolve() != source_path.resolve():
        shutil.copy2(args.input, source_path)

    source = Image.open(args.input if args.check else source_path)
    variants = (("", (350, 155), ""),
                ("_t_embed", (280, 124), "TEmbed"))
    base_symbol = pascal_case(args.name)
    for suffix, size, symbol_suffix in variants:
        grayscale = fit_without_crop(source, size)
        threshold_pixels = hard_threshold_one_bit(grayscale)
        pixels = ordered_one_bit(
            grayscale, args.levels, solid_light_region, args.edge_cleanup
        )
        stem = f"{args.name}{suffix}"
        pbm = args.output / f"{stem}.pbm"
        if args.check:
            if not pbm.exists():
                raise SystemExit(f"missing generated asset: {pbm}")
            with Image.open(pbm) as actual_image:
                actual = [0 if value else 1 for value in
                          actual_image.convert("1").getdata()]
                if actual_image.size != size or actual != pixels:
                    differing = (len(pixels) if actual_image.size != size else
                                 sum(a != b for a, b in zip(actual, pixels)))
                    raise SystemExit(
                        f"generated bitmap is stale: {pbm} "
                        f"({differing} differing pixels)"
                    )
            continue
        write_pbm(
            pbm, size, pixels, source_path.name, args.levels,
            args.edge_cleanup,
        )
        preview(review_dir / f"{stem}_1bit.png", size, pixels, False)
        preview(review_dir / f"{stem}_reflective.png", size, pixels, True)
        preview(
            review_dir / f"{stem}_threshold.png",
            size,
            threshold_pixels,
            False,
        )
        preview(
            review_dir / f"{stem}_threshold_4x.png",
            size,
            threshold_pixels,
            False,
            4,
        )
        preview(review_dir / f"{stem}_1bit_4x.png", size, pixels, False, 4)
        preview(
            review_dir / f"{stem}_reflective_4x.png",
            size,
            pixels,
            True,
            4,
        )
        if args.pack_tool and args.header_dir:
            header = args.header_dir / f"{stem}_cover.h"
            symbol = f"k{base_symbol}{symbol_suffix}Cover"
            provenance = (
                "Cadenza original artwork; generated by "
                ".codex/skills/generate-launcher-cover-art/scripts/"
                f"prepare_cover.py from {source_path.as_posix()}"
            )
            pack(sys.executable, args.pack_tool, pbm, header, symbol, provenance)


if __name__ == "__main__":
    main()
