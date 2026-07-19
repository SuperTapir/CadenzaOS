#!/usr/bin/env python3
"""Regression tests for the deterministic Playdate font converter."""

from __future__ import annotations

import importlib.util
import pathlib
import tempfile
import unittest

from PIL import Image


ROOT = pathlib.Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "font_converter", ROOT / "tools/convert_playdate_fonts.py"
)
assert SPEC and SPEC.loader
font_converter = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(font_converter)


class FontConverterTests(unittest.TestCase):
    def test_pack_rows_is_msb_first_and_clears_row_padding(self) -> None:
        pixels = [
            1, 0, 1, 0, 1, 0, 1, 0, 1,
            0, 1, 0, 1, 0, 1, 0, 1, 0,
        ]
        self.assertEqual(
            font_converter.pack_rows(9, 2, pixels),
            bytes((0xAA, 0x80, 0x55, 0x00)),
        )

    def test_missing_ascii_glyph_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            (root / "Tiny.fnt").write_text("tracking=1\nspace 2\nA 2\n")
            Image.new("RGBA", (4, 2), (0, 0, 0, 0)).save(
                root / "Tiny-table-2-2.png"
            )
            with self.assertRaisesRegex(ValueError, "missing required glyph"):
                font_converter.parse_font(
                    root / "Tiny.fnt", root / "Tiny-table-2-2.png",
                    "Tiny", "kTiny",
                )

    def test_stale_output_check_fails_without_overwriting(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            output = pathlib.Path(directory) / "generated.h"
            output.write_text("old\n")
            with self.assertRaisesRegex(SystemExit, "generated font is stale"):
                font_converter.write_or_check(output, "new\n", check=True)
            self.assertEqual(output.read_text(), "old\n")

    def test_pinned_roobert_sources_generate_deterministically(self) -> None:
        first = font_converter.generate(ROOT / "assets/fonts/roobert-source")
        second = font_converter.generate(ROOT / "assets/fonts/roobert-source")
        self.assertEqual(first, second)
        self.assertEqual(font_converter.generated_sha256(first),
                         "01c7abbc61ac18312afe77cda54110d909dac76428b972606a707f404a094b1e")
        self.assertIn("kRoobert10Bold", first)
        self.assertIn("kRoobert9MonoCondensed", first)
        self.assertIn("kTypographyCompiledBytes = 32932U", first)
        self.assertIn("Total unique font bytes:", first)


if __name__ == "__main__":
    unittest.main()
