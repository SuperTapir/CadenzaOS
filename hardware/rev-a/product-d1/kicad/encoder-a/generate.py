#!/usr/bin/env python3
"""Generate the Cadenza D1 encoder+A daughterboard KiCad sources."""

from __future__ import annotations

import csv
import json
import uuid
from pathlib import Path

ROOT = Path(__file__).resolve().parent

NETS = {
    "GND": 1,
    "+3V3": 2,
    "ENC_A": 3,
    "ENC_B": 4,
    "A_SW": 5,
    "LED_CTRL": 6,
    "LED_A": 7,
    "LED_K": 8,
}


def uid() -> str:
    return f'"{uuid.uuid4()}"'


def effects(layer: str, hide: bool = False) -> str:
    hidden = " (hide yes)" if hide else ""
    justify = " (justify mirror)" if layer.startswith("B.") else ""
    return (
        f'(layer "{layer}"){hidden} '
        f'(effects (font (size 0.8 0.8) (thickness 0.12)){justify})'
    )


def property_text(name: str, value: str, x: float, y: float, layer: str, hide: bool = False) -> str:
    return f'(property "{name}" "{value}" (at {x:.3f} {y:.3f} 0) {effects(layer, hide)})'


def pad(
    number: str,
    kind: str,
    shape: str,
    x: float,
    y: float,
    sx: float,
    sy: float,
    layers: str,
    net: str | None = None,
    drill: str | None = None,
    rr: float | None = None,
) -> str:
    bits = [f'(pad "{number}" {kind} {shape}', f'(at {x:.3f} {y:.3f})', f'(size {sx:.3f} {sy:.3f})']
    if drill:
        bits.append(f'(drill {drill})')
    bits.append(f'(layers {layers})')
    if rr is not None:
        bits.append(f'(roundrect_rratio {rr})')
    if net:
        bits.append(f'(net {NETS[net]} "{net}")')
    bits.append(f'(uuid {uid()})')
    return " ".join(bits) + ")"


def fp_start(name: str, ref: str, value: str, x: float, y: float, layer: str, attr: str) -> list[str]:
    return [
        f'(footprint "Cadenza_Encoder:{name}"',
        f'  (layer "{layer}")',
        f'  (uuid {uid()})',
        f'  (at {x:.3f} {y:.3f})',
        "  " + property_text("Reference", ref, 0, -2.3, f'{layer[0]}.SilkS'),
        "  " + property_text("Value", value, 0, 2.3, f'{layer[0]}.Fab', True),
        f'  (attr {attr})',
    ]


def encoder() -> str:
    lines = fp_start("ALPS_EC12D1524403", "SW1", "EC12D1524403", 17, 17, "F.Cu", "through_hole")
    lines += [
        '  (property "LCSC Part" "C112349" (at 0 0 0) (layer "F.Fab") (hide yes) (effects (font (size 1 1))))',
        '  (fp_rect (start -6.41 -4.50) (end 6.09 7.20) (stroke (width 0.15) (type solid)) (fill none) (layer "F.SilkS"))',
        '  (fp_circle (center -0.16 1.35) (end 2.84 1.35) (stroke (width 0.15) (type solid)) (fill none) (layer "F.SilkS"))',
        '  (fp_rect (start -6.66 -4.75) (end 6.34 7.45) (stroke (width 0.05) (type solid)) (fill none) (layer "F.CrtYd"))',
        "  " + pad("A", "thru_hole", "circle", -5.71, -2.80, 1.70, 1.70, '"*.Cu" "*.Mask"', "ENC_A", "1.05"),
        "  " + pad("B", "thru_hole", "circle", 5.39, -2.80, 1.70, 1.70, '"*.Cu" "*.Mask"', "ENC_B", "1.05"),
        "  " + pad("C", "thru_hole", "circle", -5.71, 5.50, 1.70, 1.70, '"*.Cu" "*.Mask"', "GND", "1.05"),
        "  " + pad("D", "thru_hole", "circle", 5.79, 4.80, 1.70, 1.70, '"*.Cu" "*.Mask"', "A_SW", "1.05"),
        "  " + pad("E", "thru_hole", "circle", -0.16, -5.50, 1.70, 1.70, '"*.Cu" "*.Mask"', "GND", "1.05"),
        "  " + pad("6", "thru_hole", "oval", -5.79, 1.35, 2.90, 3.20, '"*.Cu" "*.Mask"', "GND", "oval 2.05 2.30"),
        "  " + pad("7", "thru_hole", "oval", 5.46, 1.35, 2.90, 3.20, '"*.Cu" "*.Mask"', "GND", "oval 2.05 2.30"),
        '  (model "${KIPRJMOD}/encoders.3dshapes/SW-TH_EC12D1524403-L12.5-W11.7-H17.5-P11.1-LS14.1.step" (offset (xyz 0 0 0)) (scale (xyz 1 1 1)) (rotate (xyz 0 0 0)))',
        ")",
    ]
    return "\n".join(lines)


def smd2(ref: str, value: str, x: float, y: float, net1: str, net2: str, kind: str = "R", vertical: bool = False) -> str:
    lines = fp_start("0603", ref, value, x, y, "B.Cu", "smd")
    p1 = (0, -0.85) if vertical else (-0.85, 0)
    p2 = (0, 0.85) if vertical else (0.85, 0)
    lines += [
        '  (fp_rect (start -1.55 -0.85) (end 1.55 0.85) (stroke (width 0.05) (type solid)) (fill none) (layer "B.CrtYd"))',
        "  " + pad("1", "smd", "roundrect", p1[0], p1[1], 0.90, 0.95, '"B.Cu" "B.Paste" "B.Mask"', net1, rr=0.2),
        "  " + pad("2", "smd", "roundrect", p2[0], p2[1], 0.90, 0.95, '"B.Cu" "B.Paste" "B.Mask"', net2, rr=0.2),
        ")",
    ]
    return "\n".join(lines)


def jst() -> str:
    lines = fp_start("JST_SH_SM06B-SRSS-TB", "J1", "JST-SH-6", 17, 33.5, "B.Cu", "smd")
    lines += [
        '  (property "LCSC Part" "C160404" (at 0 0 0) (layer "B.Fab") (hide yes) (effects (font (size 1 1)) (justify mirror)))',
        '  (fp_rect (start -4.9 -3.28) (end 4.9 3.28) (stroke (width 0.05) (type solid)) (fill none) (layer "B.CrtYd"))',
        '  (fp_line (start -4.1 -1.78) (end 4.1 -1.78) (stroke (width 0.12) (type solid)) (layer "B.SilkS"))',
    ]
    nets = ["+3V3", "GND", "ENC_A", "ENC_B", "A_SW", "LED_CTRL"]
    for index, net in enumerate(nets, 1):
        x = 2.5 - (index - 1)
        lines.append("  " + pad(str(index), "smd", "roundrect", x, -2, 0.6, 1.55, '"B.Cu" "B.Paste" "B.Mask"', net, rr=0.25))
    lines += [
        "  " + pad("MP", "smd", "roundrect", -3.8, 1.875, 1.2, 1.8, '"B.Cu" "B.Paste" "B.Mask"', rr=0.2),
        "  " + pad("MP", "smd", "roundrect", 3.8, 1.875, 1.2, 1.8, '"B.Cu" "B.Paste" "B.Mask"', rr=0.2),
        ")",
    ]
    return "\n".join(lines)


def led() -> str:
    lines = fp_start("LED_D3.0mm", "D1", "AMBER", 17, 4, "F.Cu", "through_hole")
    lines += [
        '  (fp_circle (center 0 0) (end 1.8 0) (stroke (width 0.15) (type solid)) (fill none) (layer "F.SilkS"))',
        "  " + pad("1", "thru_hole", "rect", -1.27, 0, 1.8, 1.8, '"*.Cu" "*.Mask"', "LED_K", "0.9"),
        "  " + pad("2", "thru_hole", "circle", 1.27, 0, 1.8, 1.8, '"*.Cu" "*.Mask"', "LED_A", "0.9"),
        ")",
    ]
    return "\n".join(lines)


def sot23() -> str:
    lines = fp_start("SOT-23", "Q1", "2N7002", 27, 20, "B.Cu", "smd")
    lines += [
        '  (fp_rect (start -1.55 -1.45) (end 1.55 1.45) (stroke (width 0.05) (type solid)) (fill none) (layer "B.CrtYd"))',
        "  " + pad("1", "smd", "roundrect", -0.95, 0.95, 1.0, 1.2, '"B.Cu" "B.Paste" "B.Mask"', "LED_CTRL", rr=0.2),
        "  " + pad("2", "smd", "roundrect", 0.95, 0.95, 1.0, 1.2, '"B.Cu" "B.Paste" "B.Mask"', "GND", rr=0.2),
        "  " + pad("3", "smd", "roundrect", 0, -0.95, 1.0, 1.2, '"B.Cu" "B.Paste" "B.Mask"', "LED_K", rr=0.2),
        ")",
    ]
    return "\n".join(lines)


def mounting_hole(ref: str, x: float, y: float) -> str:
    return "\n".join([
        f'(footprint "Cadenza_Encoder:MountingHole_2.2mm" (layer "F.Cu") (uuid {uid()}) (at {x} {y})',
        "  " + property_text("Reference", ref, 0, -2, "F.SilkS", True),
        "  " + property_text("Value", "M2", 0, 2, "F.Fab", True),
        "  (attr exclude_from_pos_files exclude_from_bom)",
        "  " + pad("", "np_thru_hole", "circle", 0, 0, 2.2, 2.2, '"*.Cu" "*.Mask"', drill="2.2"),
        ")",
    ])


def segment(net: str, points: list[tuple[float, float]], layer: str = "B.Cu", width: float = 0.25) -> list[str]:
    return [
        f'(segment (start {a[0]:.3f} {a[1]:.3f}) (end {b[0]:.3f} {b[1]:.3f}) (width {width}) (layer "{layer}") (net {NETS[net]}) (uuid {uid()}))'
        for a, b in zip(points, points[1:])
    ]


def via(net: str, x: float, y: float) -> str:
    return f'(via (at {x:.3f} {y:.3f}) (size 0.8) (drill 0.4) (layers "F.Cu" "B.Cu") (net {NETS[net]}) (uuid {uid()}))'


def board() -> str:
    footprints = [
        encoder(), jst(),
        smd2("R1", "10k", 5, 25.5, "+3V3", "ENC_A", vertical=True),
        smd2("C1", "10n", 5, 29.4, "ENC_A", "GND", "C", vertical=True),
        smd2("R2", "10k", 29, 25.5, "+3V3", "ENC_B", vertical=True),
        smd2("C2", "10n", 29, 29.4, "ENC_B", "GND", "C", vertical=True),
        smd2("R3", "10k", 22, 25.5, "+3V3", "A_SW", vertical=True),
        smd2("C3", "10n", 22, 27.8, "A_SW", "GND", "C", vertical=True),
        mounting_hole("H1", 3, 3), mounting_hole("H2", 31, 3),
        mounting_hole("H3", 3, 35), mounting_hole("H4", 31, 35),
    ]

    routes: list[str] = []
    vias: list[str] = []
    # Back-side local debounce connections.
    routes += segment("ENC_A", [(5,26.35),(5,28.55)])
    routes += segment("ENC_B", [(29,26.35),(29,28.55)])
    routes += segment("A_SW", [(22,26.35),(22,26.95)])
    # Input paths use the front layer so the three channels never cross the
    # connector fan-out or the power/ground rails on the back.
    for net, point in (("ENC_A",(5,27.4)),("ENC_A",(17.5,30.8)),
                       ("ENC_B",(29,27.4)),("ENC_B",(16.5,29.5)),
                       ("A_SW",(22,26.65)),("A_SW",(15.5,29))):
        vias.append(via(net, *point))
    routes += segment("ENC_A", [(11.29,14.2),(3,14.2),(3,27.4),(5,27.4)], "F.Cu")
    routes += segment("ENC_A", [(5,27.4),(3,27.4),(3,33.5),(17.5,33.5),(17.5,30.8)], "F.Cu")
    routes += segment("ENC_A", [(17.5,30.8),(17.5,31.5)])
    routes += segment("ENC_B", [(22.39,14.2),(31,14.2),(31,27.4),(29,27.4)], "F.Cu")
    routes += segment("ENC_B", [(29,27.4),(31,27.4),(31,29.5),(16.5,29.5)], "F.Cu")
    routes += segment("ENC_B", [(16.5,29.5),(16.5,31.5)])
    routes += segment("A_SW", [(22.79,21.8),(22,24),(22,26.65)], "F.Cu")
    routes += segment("A_SW", [(22,26.65),(19,26.65),(15.5,29)], "F.Cu")
    routes += segment("A_SW", [(15.5,29),(15.5,31.5)])
    # Power rail on back: J1 pin 1 is x=19.5 after back-side mirroring.
    routes += segment("+3V3", [(19.5,31.5),(20,31.5),(20,23.5),(32,23.5),(29,24.65)])
    routes += segment("+3V3", [(20,23.5),(22,23.5),(22,24.65)])
    routes += segment("+3V3", [(20,23.5),(20,24.65),(5,24.65)])
    # Ground rail and returns, kept below the signal RC nodes.
    routes += segment("GND", [(18.5,31.5),(18.5,33.2),(5,33.2),(5,30.25)])
    routes += segment("GND", [(18.5,33.2),(22,33.2),(22,28.65)])
    routes += segment("GND", [(22,33.2),(29,33.2),(29,30.25)])
    routes += segment("GND", [(16.84,11.5),(2,11.5),(2,23),(11.29,23),(11.29,22.5)])
    routes += segment("GND", [(11.21,18.35),(2,18.35)])
    routes += segment("GND", [(22.46,18.35),(22.46,16),(16.84,16),(16.84,11.5)])
    routes += segment("GND", [(2,23),(2,32),(5,33.2)])

    layers = """(layers
    (0 \"F.Cu\" signal)
    (2 \"B.Cu\" signal)
    (9 \"F.Adhes\" user \"F.Adhesive\")
    (11 \"B.Adhes\" user \"B.Adhesive\")
    (13 \"F.Paste\" user)
    (15 \"B.Paste\" user)
    (5 \"F.SilkS\" user \"F.Silkscreen\")
    (7 \"B.SilkS\" user \"B.Silkscreen\")
    (1 \"F.Mask\" user)
    (3 \"B.Mask\" user)
    (17 \"Dwgs.User\" user \"User.Drawings\")
    (19 \"Cmts.User\" user \"User.Comments\")
    (21 \"Eco1.User\" user \"User.Eco1\")
    (23 \"Eco2.User\" user \"User.Eco2\")
    (25 \"Edge.Cuts\" user)
    (27 \"Margin\" user)
    (31 \"F.CrtYd\" user \"F.Courtyard\")
    (29 \"B.CrtYd\" user \"B.Courtyard\")
    (35 \"F.Fab\" user)
    (33 \"B.Fab\" user)
  )"""
    edge = [
        f'(gr_rect (start 0 0) (end 34 38) (stroke (width 0.25) (type solid)) (fill none) (layer "Edge.Cuts") (uuid {uid()}))',
        f'(gr_circle (center 17 17) (end 31 17) (stroke (width 0.15) (type dash)) (fill none) (layer "Dwgs.User") (uuid {uid()}))',
        f'(gr_text "KNOB OD 28 MAX" (at 17 1.5) (layer "Dwgs.User") (uuid {uid()}) (effects (font (size 0.8 0.8) (thickness 0.12))))',
    ]
    return "\n".join([
        '(kicad_pcb (version 20260206) (generator "pcbnew") (generator_version "10.0")',
        '  (general (thickness 1.6))',
        '  (paper "A4")',
        "  " + layers.replace("\n", "\n  "),
        '  (setup (pad_to_mask_clearance 0))',
        '  (net 0 "")',
        *[f'  (net {number} "{name}")' for name, number in NETS.items()],
        *["  " + fp.replace("\n", "\n  ") for fp in footprints],
        *["  " + item for item in routes],
        *["  " + item for item in vias],
        *["  " + item for item in edge],
        ')',
        '',
    ])


def legacy_schematic() -> str:
    return """EESchema Schematic File Version 4
LIBS:cadenza-encoder-a
EELAYER 29 0
EELAYER END
$Descr A4 11693 8268
Sheet 1 1
Title "Cadenza D1 Encoder + A Daughterboard"
Comment1 "Six-wire replaceable control module"
Comment2 "EC12D1524403 + RC debounce"
Comment3 "D1 WIP"
Comment4 "CadenzaOS"
$EndDescr
Text Notes 900 850 0 100 ~ 20
J1: 1=+3V3 2=GND 3=ENC_A 4=ENC_B 5=A_SW 6=LED_CTRL
Text Notes 900 1100 0 80 ~ 16
SW1 axial push is the front-panel A button. Encoder common and switch common return to GND.
Text Notes 900 1350 0 80 ~ 16
R1/C1, R2/C2, R3/C3: 10k pull-up and 10nF debounce. Firmware still performs state-machine debounce.
Text Notes 900 1600 0 80 ~ 16
LED_CTRL is reserved and intentionally no-connect on the D1 daughterboard.
$Comp
L Device:R R1
U 1 1 1010
P 4700 2600
F 0 "R1" H 4770 2646 50 0000 L CNN
F 1 "10k" H 4770 2555 50 0000 L CNN
F 2 "Resistor_SMD:R_0603_1608Metric" V 4630 2600 50 0001 C CNN
\t1    4700 2600
\t1 0 0 -1
$EndComp
$Comp
L Device:C C1
U 1 1 1011
P 4700 3200
F 0 "C1" H 4815 3246 50 0000 L CNN
F 1 "10n" H 4815 3155 50 0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 4738 3050 50 0001 C CNN
\t1    4700 3200
\t1 0 0 -1
$EndComp
Text Label 4700 2350 1 50 ~ 0
+3V3
Text Label 4700 2900 1 50 ~ 0
ENC_A
Text Label 4700 3450 3 50 ~ 0
GND
Wire Wire Line
\t4700 2350 4700 2450
Wire Wire Line
\t4700 2750 4700 3050
Wire Wire Line
\t4700 3350 4700 3450
$Comp
L Device:R R2
U 1 1 1020
P 5500 2600
F 0 "R2" H 5570 2646 50 0000 L CNN
F 1 "10k" H 5570 2555 50 0000 L CNN
F 2 "Resistor_SMD:R_0603_1608Metric" V 5430 2600 50 0001 C CNN
\t1    5500 2600
\t1 0 0 -1
$EndComp
$Comp
L Device:C C2
U 1 1 1021
P 5500 3200
F 0 "C2" H 5615 3246 50 0000 L CNN
F 1 "10n" H 5615 3155 50 0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 5538 3050 50 0001 C CNN
\t1    5500 3200
\t1 0 0 -1
$EndComp
Text Label 5500 2350 1 50 ~ 0
+3V3
Text Label 5500 2900 1 50 ~ 0
ENC_B
Text Label 5500 3450 3 50 ~ 0
GND
Wire Wire Line
\t5500 2350 5500 2450
Wire Wire Line
\t5500 2750 5500 3050
Wire Wire Line
\t5500 3350 5500 3450
$Comp
L Device:R R3
U 1 1 1030
P 6300 2600
F 0 "R3" H 6370 2646 50 0000 L CNN
F 1 "10k" H 6370 2555 50 0000 L CNN
F 2 "Resistor_SMD:R_0603_1608Metric" V 6230 2600 50 0001 C CNN
\t1    6300 2600
\t1 0 0 -1
$EndComp
$Comp
L Device:C C3
U 1 1 1031
P 6300 3200
F 0 "C3" H 6415 3246 50 0000 L CNN
F 1 "10n" H 6415 3155 50 0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 6338 3050 50 0001 C CNN
\t1    6300 3200
\t1 0 0 -1
$EndComp
Text Label 6300 2350 1 50 ~ 0
+3V3
Text Label 6300 2900 1 50 ~ 0
A_SW
Text Label 6300 3450 3 50 ~ 0
GND
Wire Wire Line
\t6300 2350 6300 2450
Wire Wire Line
\t6300 2750 6300 3050
Wire Wire Line
\t6300 3350 6300 3450
$Comp
L Cadenza_Encoder:EC12D1524403 SW1
U 1 1 1001
P 3500 3300
F 0 "SW1" H 3500 3900 50  0000 C CNN
F 1 "EC12D1524403" H 3500 3800 50 0000 C CNN
F 2 "Cadenza_Encoder:ALPS_EC12D1524403" H 3500 3300 50 0001 C CNN
	1    3500 3300
	1 0 0 -1
$EndComp
$Comp
L Connector_Generic:Conn_01x06 J1
U 1 1 1002
P 7600 3300
F 0 "J1" H 7680 3292 50 0000 L CNN
F 1 "JST-SH-6" H 7680 3201 50 0000 L CNN
F 2 "Cadenza_Encoder:JST_SH_SM06B-SRSS-TB" H 7600 3300 50 0001 C CNN
	1    7600 3300
	1 0 0 -1
$EndComp
Text Label 3000 3100 2 50 ~ 0
ENC_A
Text Label 3000 3300 2 50 ~ 0
ENC_B
Text Label 3000 3500 2 50 ~ 0
GND
Text Label 4000 3200 0 50 ~ 0
A_SW
Text Label 4000 3400 0 50 ~ 0
GND
Wire Wire Line
	3000 3100 3200 3100
Wire Wire Line
	3000 3300 3200 3300
Wire Wire Line
	3000 3500 3200 3500
Wire Wire Line
	3800 3200 4000 3200
Wire Wire Line
	3800 3400 4000 3400
Text Label 7100 3100 2 50 ~ 0
+3V3
Text Label 7100 3200 2 50 ~ 0
GND
Text Label 7100 3300 2 50 ~ 0
ENC_A
Text Label 7100 3400 2 50 ~ 0
ENC_B
Text Label 7100 3500 2 50 ~ 0
A_SW
Text Label 7100 3600 2 50 ~ 0
LED_CTRL
Wire Wire Line
	7100 3100 7400 3100
Wire Wire Line
	7100 3200 7400 3200
Wire Wire Line
	7100 3300 7400 3300
Wire Wire Line
	7100 3400 7400 3400
Wire Wire Line
	7100 3500 7400 3500
Wire Wire Line
	7100 3600 7400 3600
$EndSCHEMATC
"""


def symbol_library() -> str:
    return """EESchema-LIBRARY Version 2.4
#encoding utf-8
#
# EC12D1524403
#
DEF EC12D1524403 SW 0 40 Y Y 1 F N
F0 "SW" 0 600 50 H V C CNN
F1 "EC12D1524403" 0 500 50 H V C CNN
F2 "Cadenza_Encoder:ALPS_EC12D1524403" 0 -600 50 H I C CNN
DRAW
S -300 400 300 -400 0 1 12 f
X ENC_A A -500 200 200 R 40 40 1 1 P
X ENC_B B -500 0 200 R 40 40 1 1 P
X ENC_COM C -500 -200 200 R 40 40 1 1 P
X A_SW D 500 100 200 L 40 40 1 1 P
X SW_COM E 500 -100 200 L 40 40 1 1 P
ENDDRAW
ENDDEF
#End Library
"""


def project_file() -> str:
    return json.dumps({
        "board": {},
        "boards": [],
        "cvpcb": {},
        "erc": {},
        "libraries": {},
        "meta": {"filename": "cadenza-encoder-a.kicad_pro", "version": 1},
        "net_settings": {"classes": [], "meta": {"version": 3}},
        "pcbnew": {},
        "schematic": {},
        "text_variables": {},
    }, indent=2) + "\n"


def write_bom() -> None:
    rows = [
        ["Reference", "Qty", "Value", "MPN", "LCSC", "Assembly"],
        ["SW1", "1", "30 detent / 15 pulse encoder with push", "EC12D1524403", "C112349", "Hand/THT"],
        ["J1", "1", "JST-SH 6 pin horizontal", "SM06B-SRSS-TB", "C160404", "JLC SMT"],
        ["R1,R2,R3", "3", "10k 1% 0603", "generic", "TBD basic", "JLC SMT"],
        ["C1,C2,C3", "3", "10nF 16V X7R 0603", "generic", "TBD basic", "JLC SMT"],
    ]
    with (ROOT / "bom.csv").open("w", newline="", encoding="utf-8") as handle:
        csv.writer(handle).writerows(rows)


def main() -> None:
    (ROOT / "cadenza-encoder-a.kicad_pcb").write_text(board(), encoding="utf-8")
    (ROOT / "cadenza-encoder-a.sch").write_text(legacy_schematic(), encoding="utf-8")
    (ROOT / "cadenza-encoder-a.lib").write_text(symbol_library(), encoding="utf-8")
    (ROOT / "cadenza-encoder-a.kicad_pro").write_text(project_file(), encoding="utf-8")
    (ROOT / "sym-lib-table").write_text('(sym_lib_table\n  (lib (name "Cadenza_Encoder")(type "Legacy")(uri "${KIPRJMOD}/cadenza-encoder-a.lib")(options "")(descr ""))\n)\n', encoding="utf-8")
    (ROOT / "fp-lib-table").write_text('(fp_lib_table\n  (lib (name "Cadenza_Encoder")(type "KiCad")(uri "${KIPRJMOD}/encoders.pretty")(options "")(descr ""))\n)\n', encoding="utf-8")
    write_bom()


if __name__ == "__main__":
    main()
