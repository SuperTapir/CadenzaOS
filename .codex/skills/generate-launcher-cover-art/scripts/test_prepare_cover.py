#!/usr/bin/env python3
"""Regression tests for deterministic Launcher Cover preparation."""

from __future__ import annotations

import importlib.util
import pathlib
import tempfile
import unittest

from PIL import Image


SCRIPT = pathlib.Path(__file__).with_name("prepare_cover.py")
SPEC = importlib.util.spec_from_file_location("prepare_cover", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
prepare_cover = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(prepare_cover)


class EdgeCleanupTests(unittest.TestCase):
    def test_antialiased_black_white_boundary_uses_coverage_threshold(self) -> None:
        image = Image.new("L", (8, 8), 255)
        pixels = image.load()
        edge_values = (72, 96, 120, 136, 160, 184, 104, 152)
        for y, value in enumerate(edge_values):
            for x in range(3):
                pixels[x, y] = 0
            pixels[3, y] = value

        cleaned = prepare_cover.ordered_one_bit(
            image, 5, edge_cleanup=True
        )

        for y, value in enumerate(edge_values):
            self.assertEqual(cleaned[y * image.width + 3], int(value < 128))

    def test_broad_gray_plane_keeps_ordered_dither(self) -> None:
        image = Image.new("L", (8, 8), 128)
        plain = prepare_cover.ordered_one_bit(image, 5)
        cleaned = prepare_cover.ordered_one_bit(
            image, 5, edge_cleanup=True
        )

        self.assertEqual(cleaned, plain)
        self.assertIn(0, cleaned)
        self.assertIn(1, cleaned)

    def test_cleanup_hardens_one_pixel_guard_around_silhouette(self) -> None:
        image = Image.new("L", (8, 8), 255)
        pixels = image.load()
        for y in range(image.height):
            for x in range(3):
                pixels[x, y] = 0
            pixels[3, y] = 96
            pixels[4, y] = 192

        plain = prepare_cover.ordered_one_bit(image, 5)
        cleaned = prepare_cover.ordered_one_bit(
            image, 5, edge_cleanup=True
        )

        self.assertEqual(plain[4], 1)
        self.assertEqual(cleaned[4], 0)

    def test_binary_master_is_unchanged(self) -> None:
        image = Image.new("L", (8, 8), 255)
        pixels = image.load()
        for y in range(image.height):
            for x in range(4):
                pixels[x, y] = 0

        self.assertEqual(
            prepare_cover.ordered_one_bit(image, 5, edge_cleanup=True),
            prepare_cover.ordered_one_bit(image, 5),
        )


class ReviewArtifactTests(unittest.TestCase):
    def test_hard_threshold_preview_does_not_credit_gray_texture(self) -> None:
        image = Image.new("L", (4, 1))
        image.putdata((0, 127, 128, 255))

        self.assertEqual(
            prepare_cover.hard_threshold_one_bit(image),
            [1, 1, 0, 0],
        )

    def test_scaled_preview_uses_nearest_neighbor_pixel_blocks(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "preview.png"
            prepare_cover.preview(path, (2, 1), [1, 0], False, 4)
            with Image.open(path) as image:
                self.assertEqual(image.size, (8, 4))
                pixels = image.load()
                for y in range(4):
                    for x in range(4):
                        self.assertEqual(pixels[x, y], (0, 0, 0))
                        self.assertEqual(pixels[x + 4, y], (255, 255, 255))


if __name__ == "__main__":
    unittest.main()
