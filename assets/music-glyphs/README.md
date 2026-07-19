# SIGHT music glyph subset

`sight_glyphs_t_embed.pbm` and `sight_glyphs_sharp.pbm` contain only SMuFL
`gClef`, `fClef`, `accidentalSharp`, and `accidentalFlat`. They are offline
1-bit raster derivatives of `redist/otf/Bravura.otf` at Bravura commit
`02e8ed29a29115df35007d1178cebaeee26c20e1` (source SHA-256
`dca2d90c88437a701b1c2e71fa54e76f9fa41d7deee935d74dc871ea66ecfdd2`).

The source font is licensed under SIL OFL 1.1 with Reserved Font Name
“Bravura”; see `third_party/licenses/Bravura-OFL-1.1.txt`. The complete OTF is
not stored in or loaded by CadenzaOS. Regenerate with Pillow 11.3.0:

```sh
python tools/generate_sight_glyphs.py /path/to/Bravura.otf assets/music-glyphs
```

Each atlas has four equal-width cells in this order: treble clef, bass clef,
sharp, flat. Runtime uses native-size cells and never scales them.
