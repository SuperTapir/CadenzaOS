#!/usr/bin/env python3
"""Export the deterministic dual-profile typography review matrix."""

from __future__ import annotations

import argparse
import subprocess
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


SCENES = (
    ("launcher", "0"),
    ("timer-ready", "1"),
    ("timer-running", "1", "running"),
    ("timer-paused", "1", "paused"),
    ("timer-alert", "1", "alert"),
    ("timer-starting-mid", "1", "starting-mid"),
    ("timer-pausing-mid", "1", "pausing-mid"),
    ("timer-resuming-mid", "1", "resuming-mid"),
    ("timer-alert-mid", "1", "alert-mid"),
    ("motion", "2"),
    ("settings", "3"),
    ("gallery-easing", "4", "0", "10"),
    ("gallery-selection", "4", "9", "10"),
    ("gallery-sprite", "4", "12", "10"),
    ("system-menu", "1", "menu", "0"),
    ("system-overlay", "6"),
    ("role-long-inverse", "5"),
)
PROFILES = (("t-embed", "320"), ("sharp", "400"))


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    binary = args.binary.resolve()
    output = args.output.resolve()
    frames = output / "frames"
    frames.mkdir(parents=True, exist_ok=True)
    captures: list[tuple[str, str, Path, Image.Image]] = []
    for scene in SCENES:
        scene_name, app_arg, *scene_args = scene
        for profile_name, profile_arg in PROFILES:
            path = frames / f"{profile_name}-{scene_name}.pbm"
            subprocess.run(
                [str(binary), profile_arg, app_arg, str(path), *scene_args],
                check=True,
            )
            captures.append(
                (scene_name, profile_name, path, Image.open(path).convert("RGB"))
            )

    label_height = 18
    gutter = 12
    column_width = max(image.width for *_, image in captures) + gutter * 2
    row_height = max(image.height for *_, image in captures) + label_height + gutter
    sheet = Image.new(
        "RGB",
        (column_width * len(PROFILES), row_height * len(SCENES)),
        "#bdbdbd",
    )
    draw = ImageDraw.Draw(sheet)
    font = ImageFont.load_default()
    for scene_index, scene in enumerate(SCENES):
        for profile_index, (profile_name, _) in enumerate(PROFILES):
            capture_index = scene_index * len(PROFILES) + profile_index
            scene_name, _, _, frame = captures[capture_index]
            x = profile_index * column_width + gutter
            y = scene_index * row_height + label_height
            draw.text((x, y - 14), f"{profile_name} / {scene_name}",
                      fill="black", font=font)
            sheet.paste(frame, (x, y))
    sheet_path = output / "typography-contact-sheet.png"
    sheet.save(sheet_path, optimize=False)

    index = output / "typography-matrix-index.md"
    lines = [
        "# Typography visual matrix",
        "",
        "Columns: T-Embed 320×170, Sharp 400×240.",
        "",
    ]
    lines.extend(f"{row + 1}. `{scene[0]}`" for row, scene in enumerate(SCENES))
    lines.extend(["", f"Contact sheet: `{sheet_path.name}`", ""])
    index.write_text("\n".join(lines), encoding="utf-8")
    print(sheet_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
