#!/usr/bin/env python3
"""Generate the audited project-private KiCad footprints for Cadenza D1.

Dimensions come from the package drawings named in component-freeze.md.  This
script intentionally owns only packages that cannot safely be represented by a
stock KiCad footprint.  Generic passives and the ESP32/USB/JST footprints remain
references to the KiCad 10 standard libraries.
"""

from pathlib import Path

OUT = Path(__file__).with_name("Cadenza_D1.pretty")


def header(name: str, descr: str) -> list[str]:
    return [
        f'(footprint "{name}"',
        '  (version 20240108)',
        '  (generator "cadenza_d1_footprint_generator")',
        '  (layer "F.Cu")',
        f'  (descr "{descr}")',
        '  (attr smd)',
        '  (fp_text reference "REF**" (at 0 -3) (layer "F.SilkS")',
        '    (effects (font (size 1 1) (thickness 0.15))))',
        '  (fp_text value "VALUE" (at 0 3) (layer "F.Fab")',
        '    (effects (font (size 1 1) (thickness 0.15))))',
    ]


def line(x1, y1, x2, y2, layer="F.SilkS", width=0.12):
    return f'  (fp_line (start {x1} {y1}) (end {x2} {y2}) (stroke (width {width}) (type solid)) (layer "{layer}"))'


def smd_pad(num, x, y, w, h, shape="roundrect", layers='"F.Cu" "F.Paste" "F.Mask"'):
    rr = ' (roundrect_rratio 0.2)' if shape == "roundrect" else ""
    return f'  (pad "{num}" smd {shape} (at {x} {y}) (size {w} {h}) (layers {layers}){rr})'


def hole(x, y, diameter, plated=False):
    kind = "thru_hole" if plated else "np_thru_hole"
    layers = '"*.Cu" "*.Mask"' if plated else '"*.Cu" "*.Mask"'
    return f'  (pad "" {kind} circle (at {x} {y}) (size {diameter} {diameter}) (drill {diameter}) (layers {layers}))'


def write(name: str, body: list[str]):
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / f"{name}.kicad_mod").write_text("\n".join(body + [")", ""]), encoding="ascii")


def tps63070():
    name = "TI_RNM0015A_VQFN-HR-15_2.5x3mm"
    b = header(name, "TPS63070 RNM0015A; TI official land pattern 4222000/B")
    b += [line(-1.4, -1.65, 1.4, -1.65), line(1.4, -1.65, 1.4, 1.65),
          line(1.4, 1.65, -1.4, 1.65), line(-1.4, 1.65, -1.4, -1.65),
          line(-1.55, -1.8, -1.15, -1.8)]
    # RNM is an asymmetric HotRod package. Coordinates follow the TI land view.
    pads = [
        (1, -1.1, -1.25, .25, .6), (2, -.6, -1.25, .25, .6),
        (3, -.1, -1.25, .25, .6), (4, .4, -1.25, .25, .6),
        (5, 1.1, -.75, .25, .6), (6, 1.1, 0, .25, .6),
        (7, 1.1, .75, .25, .6), (8, .65, 1.25, .25, .6),
        (9, 0, 1.0, .6, 1.15), (10, -.65, 1.25, .25, .6),
        (11, -1.0, .35, .775, .6), (12, -.525, -.15, .25, .6),
        (13, 0, -.15, .25, .6), (14, .525, -.15, .25, .6),
        (15, -1.0, -.55, .775, .6),
    ]
    b += [smd_pad(*p) for p in pads]
    write(name, b)


def tps61023():
    name = "TI_DRL0006A_SOT-563"
    b = header(name, "TPS61023 DRL0006A; TI official land pattern 4223266/F")
    b += [line(-1.0, -0.85, 1.0, -0.85), line(1.0, -0.85, 1.0, .85),
          line(1.0, .85, -1.0, .85), line(-1.0, .85, -1.0, -.85),
          line(-1.1, -.95, -.7, -.95)]
    for i, y in enumerate((-0.5, 0, 0.5), 1):
        b.append(smd_pad(i, -0.74, y, .67, .3))
    for i, y in zip((6, 5, 4), (-0.5, 0, 0.5)):
        b.append(smd_pad(i, .74, y, .67, .3))
    write(name, b)


def bq27441():
    name = "TI_DRZ0012A_VSON-12_2.5x4mm"
    b = header(name, "BQ27441 DRZ0012A; TI official land pattern 4218895/B")
    b += [line(-1.4, -2.15, 1.4, -2.15), line(1.4, -2.15, 1.4, 2.15),
          line(1.4, 2.15, -1.4, 2.15), line(-1.4, 2.15, -1.4, -2.15),
          line(-1.55, -2.3, -1.1, -2.3)]
    for i, y in enumerate((-1, -.6, -.2, .2, .6, 1), 1):
        b.append(smd_pad(i, -1.225, y, .6, .2))
    for i, y in zip(range(12, 6, -1), (-1, -.6, -.2, .2, .6, 1)):
        b.append(smd_pad(i, 1.225, y, .6, .2))
    b.append(smd_pad(13, 0, 0, 1.95, 2.9, shape="rect"))
    write(name, b)


def microphone(name: str, package: str):
    b = header(name, f"Infineon {package} bottom-port PDM microphone; dedicated variant")
    b += [line(-2.1, -1.6, 2.1, -1.6), line(2.1, -1.6, 2.1, 1.6),
          line(2.1, 1.6, -2.1, 1.6), line(-2.1, 1.6, -2.1, -1.6),
          line(-2.2, -1.75, -1.7, -1.75)]
    # Five SMD lands surround a copper-free 0.8 mm acoustic via.
    for p in [(1, -1.35, -.85, .85, .7), (2, 0, -.85, .85, .7),
              (3, 1.35, -.85, .85, .7), (4, 1.35, .85, .85, .7),
              (5, -1.35, .85, .85, .7)]:
        b.append(smd_pad(*p))
    b.append(hole(0, .7, .8, plated=False))
    b.append('  (fp_circle (center 0 0.7) (end 0.65 0.7) (stroke (width 0.1) (type solid)) (fill none) (layer "F.CrtYd"))')
    write(name, b)


def sharp_lcd():
    name = "Sharp_LS027B7DH01_Mechanical"
    b = header(name, "Sharp LS027B7DH01 62.8x42.82x1.64 mm mechanical envelope")
    x, y = 31.4, 21.41
    b += [line(-x, -y, x, -y, "F.Fab", .1), line(x, -y, x, y, "F.Fab", .1),
          line(x, y, -x, y, "F.Fab", .1), line(-x, y, -x, -y, "F.Fab", .1),
          line(-29.4, -17.64, 29.4, -17.64, "Dwgs.User", .1),
          line(29.4, -17.64, 29.4, 17.64, "Dwgs.User", .1),
          line(29.4, 17.64, -29.4, 17.64, "Dwgs.User", .1),
          line(-29.4, 17.64, -29.4, -17.64, "Dwgs.User", .1)]
    write(name, b)


def fpc_fh34_10():
    name = "Hirose_FH34SRJ-10S-0.5SH"
    b = header(name, "Hirose FH34SRJ 10 pin 0.5 mm pitch; verify contact face with physical LCD")
    b += [line(-3.6, -1.6, 3.6, -1.6), line(3.6, -1.6, 3.6, 1.6),
          line(3.6, 1.6, -3.6, 1.6), line(-3.6, 1.6, -3.6, -1.6)]
    for i in range(10):
        x = (i - 4.5) * .5
        b.append(smd_pad(i + 1, x, -1.25, .3, 1.0))
    b.append(smd_pad("MP", -3.3, .65, .8, 1.3, shape="rect"))
    b.append(smd_pad("MP", 3.3, .65, .8, 1.3, shape="rect"))
    write(name, b)


def main():
    tps63070()
    tps61023()
    bq27441()
    microphone("Infineon_IM73D122V01_PG-LLGA-5-3", "PG-LLGA-5-3")
    microphone("Infineon_IM69D130_PG-LLGA-5-1", "PG-LLGA-5-1")
    sharp_lcd()
    fpc_fh34_10()


if __name__ == "__main__":
    main()
