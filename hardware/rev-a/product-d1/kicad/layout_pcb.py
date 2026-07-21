#!/usr/bin/env python3
"""Apply deterministic D1 stack-up, outline and component placement.

Input must be the KiCad-created board after Update PCB from Schematic.  The
script preserves all unknown KiCad 10 fields and rewrites only top-level layer,
thickness, footprint placement/layer and Cadenza-owned mechanical primitives.
"""

from __future__ import annotations

import re
import uuid
from pathlib import Path

BOARD = Path(__file__).with_name("cadenza-d1.kicad_pcb")

# Absolute positions in millimetres. B means the complete footprint is flipped
# to the back. Electromechanical parts that touch the enclosure remain on F.
PLACEMENT = {
    "U1": (151.0, 74.0, 0, "B"),
    "U2": (143.0, 103.0, 0, "B"),
    "U3": (128.0, 94.0, 0, "B"),
    "U4": (134.0, 80.0, 0, "B"),
    "U5": (64.0, 94.0, 0, "B"),
    "U6": (58.0, 58.0, 0, "F"),
    "U7": (121.0, 104.0, 0, "B"),
    "U8": (116.0, 101.0, 0, "B"),
    "J1": (108.0, 111.0, 0, "F"),
    "J2": (108.0, 54.0, 0, "F"),
    "J3": (151.0, 104.0, 0, "F"),
    "J4": (76.0, 106.0, 0, "B"),
    "J5": (62.0, 106.0, 0, "B"),
    "J6": (160.0, 82.0, 0, "F"),
    "SW1": (54.0, 92.0, 0, "F"),
    "SW2": (60.0, 76.0, 0, "F"),
    "SW3": (60.0, 66.0, 0, "F"),
    "SW4": (86.0, 106.0, 0, "B"),
    "SW5": (94.0, 106.0, 0, "B"),
    "TP1": (97.0, 109.0, 0, "B"),
    "TP2": (99.54, 109.0, 0, "B"),
    "TP3": (102.08, 109.0, 0, "B"),
    "TP4": (104.62, 109.0, 0, "B"),
    "TP5": (82.0, 109.0, 0, "B"),
    "TP6": (84.54, 109.0, 0, "B"),
    "TP7": (132.0, 109.0, 0, "B"),
    "TP8": (134.54, 109.0, 0, "B"),
    "TP9": (137.08, 109.0, 0, "B"),
    "L1": (132.0, 94.0, 0, "B"),
    "L2": (134.0, 90.0, 0, "B"),
    "FB1": (132.0, 84.0, 0, "B"),
    "FB2": (129.0, 84.0, 0, "B"),
}

# Local support parts are grouped around the IC they serve. The large display
# is above this region; all entries below are backside unless listed above.
CLUSTERS = {
    "charger": (["R3", "R4", "R5", "R6", "R7", "R75", "R77", "C1", "C2", "C3"], 132.0, 98.0),
    "main3v3": (["R10", "R11", "R30", "R76", "C10", "C11", "C12", "C13", "C14"], 116.0, 88.0),
    "media5v": (["R20", "R21", "C20", "C21", "C22"], 125.0, 76.0),
    "usb": (["R1", "R2"], 100.0, 100.0),
    "lcd": (["C30", "C31"], 114.0, 57.0),
    "mic": (["R40", "C40"], 64.0, 58.0),
    "amp": (["R41", "C50", "C51"], 70.0, 90.0),
    "gauge": (["R70", "R71", "R72", "R73", "R74", "C70", "C71"], 103.0, 98.0),
    "mcu_power": (["C81", "C82"], 138.0, 69.0),
    "sd": (["R90", "R91", "R92", "R93", "C90", "C91"], 145.0, 90.0),
    "reset": (["R80", "C80"], 88.0, 98.0),
}


def matching_close(text: str, start: int) -> int:
    depth = 0
    quoted = False
    escaped = False
    for i in range(start, len(text)):
        c = text[i]
        if quoted:
            if escaped:
                escaped = False
            elif c == "\\":
                escaped = True
            elif c == '"':
                quoted = False
            continue
        if c == '"':
            quoted = True
        elif c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                return i + 1
    raise ValueError(f"unterminated expression at {start}")


def footprint_spans(text: str):
    spans = []
    for m in re.finditer(r"(?m)^\t\(footprint ", text):
        end = matching_close(text, m.start() + 1)
        block = text[m.start() + 1:end]
        ref_match = re.search(r'\(property "Reference" "([^"]+)"', block)
        if ref_match:
            spans.append((m.start() + 1, end, ref_match.group(1), block))
    return spans


def remove_owned_additions(text: str) -> str:
    seeds = [f"edge-line-{i}" for i in range(4)] + [f"edge-arc-{i}" for i in range(4)]
    seeds += ["H1", "H2", "H3", "H4", "MIC_PORT", "LCD1", "zone-gnd"]
    owned = {uid(seed) for seed in seeds}
    spans = []
    for match in re.finditer(r"(?m)^\t\((?:footprint|gr_line|gr_arc|zone)\b", text):
        start = match.start()
        end = matching_close(text, start + 1)
        block = text[start + 1:end]
        if any(value in block for value in owned):
            spans.append((start, end))
    for start, end in reversed(spans):
        if end < len(text) and text[end] == "\n":
            end += 1
        text = text[:start] + text[end:]
    return text


def replace_position(block: str, x: float, y: float, angle: int, side: str) -> str:
    at = f"\t\t(at {x:.3f} {y:.3f}" + (f" {angle}" if angle else "") + ")"
    block, n = re.subn(r"(?m)^\t\t\(at [^\n]+\)$", at, block, count=1)
    if n != 1:
        raise ValueError("missing top-level footprint at")
    if side == "B" and '\t\t(layer "F.Cu")' in block:
        swaps = {
            '"F.Cu"': '"B.Cu"', '"F.Paste"': '"B.Paste"', '"F.Mask"': '"B.Mask"',
            '"F.SilkS"': '"B.SilkS"', '"F.Fab"': '"B.Fab"', '"F.CrtYd"': '"B.CrtYd"',
        }
        for old, new in swaps.items():
            block = block.replace(old, new)
    return block


def assign_cluster_positions():
    for refs, x0, y0 in CLUSTERS.values():
        for i, ref in enumerate(refs):
            col, row = divmod(i, 3)
            PLACEMENT[ref] = (x0 + col * 3.5, y0 + row * 2.8, 0, "B")


def uid(seed: str) -> str:
    return str(uuid.uuid5(uuid.UUID("43dc9525-68f5-4f7c-b924-4dfdb731be2d"), seed))


def edge_primitives() -> str:
    # 116 x 64 mm, R6 corners, origin at (50, 50).
    lines = [
        (56, 50, 160, 50), (166, 56, 166, 108),
        (160, 114, 56, 114), (50, 108, 50, 56),
    ]
    arcs = [
        (56, 50, 51.757359, 51.757359, 50, 56),
        (166, 56, 164.242641, 51.757359, 160, 50),
        (160, 114, 164.242641, 112.242641, 166, 108),
        (50, 108, 51.757359, 112.242641, 56, 114),
    ]
    out = []
    for i, (x1, y1, x2, y2) in enumerate(lines):
        out.append(f'\t(gr_line (start {x1} {y1}) (end {x2} {y2}) (stroke (width 0.1) (type solid)) (layer "Edge.Cuts") (uuid "{uid("edge-line-"+str(i))}"))')
    for i, (sx, sy, mx, my, ex, ey) in enumerate(arcs):
        out.append(f'\t(gr_arc (start {sx} {sy}) (mid {mx} {my}) (end {ex} {ey}) (stroke (width 0.1) (type solid)) (layer "Edge.Cuts") (uuid "{uid("edge-arc-"+str(i))}"))')
    return "\n".join(out)


def npth(ref: str, x: float, y: float, drill: float) -> str:
    return f'''\t(footprint "Cadenza_D1:{ref}"
\t\t(layer "F.Cu")
\t\t(uuid "{uid(ref)}")
\t\t(at {x} {y})
\t\t(property "Reference" "{ref}" (at 0 -2 0) (layer "F.SilkS") (effects (font (size 0.8 0.8) (thickness 0.12))))
\t\t(property "Value" "NPTH {drill}mm" (at 0 2 0) (layer "F.Fab") (effects (font (size 0.8 0.8) (thickness 0.12))))
\t\t(fp_circle (center 0 0) (end {drill/2+0.5:.2f} 0) (stroke (width 0.15) (type solid)) (fill none) (layer "F.SilkS"))
\t\t(pad "" np_thru_hole circle (at 0 0) (size {drill} {drill}) (drill {drill}) (layers "*.Cu" "*.Mask"))
\t)'''


def lcd_envelope() -> str:
    return f'''\t(footprint "Cadenza_D1:Sharp_LS027B7DH01_Mechanical"
\t\t(layer "F.Cu")
\t\t(uuid "{uid('LCD1')}")
\t\t(at 108 82)
\t\t(property "Reference" "LCD1" (at 0 -23 0) (layer "F.SilkS") (effects (font (size 1 1) (thickness 0.15))))
\t\t(property "Value" "LS027B7DH01 62.8x42.82" (at 0 23 0) (layer "F.Fab") (effects (font (size 1 1) (thickness 0.15))))
\t\t(fp_rect (start -31.4 -21.41) (end 31.4 21.41) (stroke (width 0.1) (type solid)) (fill none) (layer "F.Fab"))
\t\t(fp_rect (start -29.4 -17.64) (end 29.4 17.64) (stroke (width 0.1) (type solid)) (fill none) (layer "Dwgs.User"))
\t)'''


def zone(name: str, layer: str, seed: str) -> str:
    return f'''\t(zone
\t\t(net_name "{name}")
\t\t(layer "{layer}")
\t\t(uuid "{uid(seed)}")
\t\t(hatch edge 0.5)
\t\t(connect_pads (clearance 0.25))
\t\t(min_thickness 0.25)
\t\t(fill yes (thermal_gap 0.3) (thermal_bridge_width 0.3))
\t\t(polygon (pts (xy 51 56) (xy 56 51) (xy 160 51) (xy 165 56) (xy 165 108) (xy 160 113) (xy 56 113) (xy 51 108)))
\t)'''


def main():
    text = BOARD.read_text(encoding="utf-8")
    text = remove_owned_additions(text)
    assign_cluster_positions()
    text = text.replace("\t\t(thickness 1.2)", "\t\t(thickness 1.6)", 1)
    two_layers = '\t\t(0 "F.Cu" signal)\n\t\t(2 "B.Cu" signal)'
    legacy_four_layers = '\t\t(0 "F.Cu" signal)\n\t\t(2 "In1.Cu" power "GND")\n\t\t(4 "In2.Cu" power "POWER")\n\t\t(31 "B.Cu" signal)'
    canonical_four_layers = '\t\t(0 "F.Cu" signal)\n\t\t(4 "In1.Cu" power "GND")\n\t\t(6 "In2.Cu" signal "POWER")\n\t\t(2 "B.Cu" signal)'
    canonical_power_plane = '\t\t(0 "F.Cu" signal)\n\t\t(4 "In1.Cu" power "GND")\n\t\t(6 "In2.Cu" power "POWER")\n\t\t(2 "B.Cu" signal)'
    if two_layers in text:
        text = text.replace(two_layers, canonical_four_layers, 1)
    elif legacy_four_layers in text:
        text = text.replace(legacy_four_layers, canonical_four_layers, 1)
    elif canonical_power_plane in text:
        text = text.replace(canonical_power_plane, canonical_four_layers, 1)
    elif canonical_four_layers not in text:
        raise ValueError("expected two- or four-layer header not found")

    spans = footprint_spans(text)
    found = set()
    for start, end, ref, block in reversed(spans):
        if ref in PLACEMENT:
            found.add(ref)
            block = replace_position(block, *PLACEMENT[ref])
            text = text[:start] + block + text[end:]
    missing = sorted(set(PLACEMENT) - found)
    if missing:
        raise ValueError(f"placement references absent from PCB: {missing}")

    net_names = set(re.findall(r'\(net "([^"]+)"\)', text))
    if "/GND" not in net_names:
        raise ValueError("GND net not found")

    additions = [edge_primitives(), lcd_envelope()]
    for ref, x, y in (("H1", 55, 55), ("H2", 120, 49), ("H3", 55, 109), ("H4", 161, 109)):
        additions.append(npth(ref, x, y, 2.2))
    additions.append(zone("/GND", "In1.Cu", "zone-gnd"))
    text = text.rstrip()
    if not text.endswith(")"):
        raise ValueError("invalid board root")
    text = text[:-1] + "\n" + "\n".join(additions) + "\n)\n"
    BOARD.write_text(text, encoding="utf-8")


if __name__ == "__main__":
    main()
