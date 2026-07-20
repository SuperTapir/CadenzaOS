#!/usr/bin/env python3
"""Generate deterministic Rev A interoperability smoke-test artifacts.

The .yaml inputs intentionally use JSON syntax, which is valid YAML 1.2 and can
be parsed with Python's standard library. This keeps the bootstrap dependency
free while preserving a future migration path to richer YAML.
"""

from __future__ import annotations

import csv
import hashlib
import json
import math
import shutil
import struct
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DESIGN_PATH = ROOT / "design.yaml"
BOM_PATH = ROOT / "bom.yaml"
OUT = ROOT / "generated" / "import-smoke-test"
PROJECT = "cadenza_rev_a_smoke"


def load_json_yaml(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def write_text(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content.rstrip() + "\n", encoding="utf-8")


def kicad_pcb(width: float, height: float) -> str:
    x0, y0 = 100.0, 100.0
    x1, y1 = x0 + width, y0 + height
    return f'''(kicad_pcb (version 20171130) (host pcbnew "(5.1.12)")

  (general (thickness 1.6) (drawings 7) (tracks 1) (zones 1) (modules 4) (nets 3))
  (page A4)
  (layers
    (0 F.Cu signal)
    (1 In1.Cu power)
    (2 In2.Cu power)
    (31 B.Cu signal)
    (36 B.SilkS user "B.Silkscreen")
    (37 F.SilkS user "F.Silkscreen")
    (44 Edge.Cuts user)
  )
  (setup
    (last_trace_width 0.25)
    (trace_clearance 0.2)
    (zone_clearance 0.3)
    (zone_45_only no)
    (trace_min 0.15)
    (via_size 0.8)
    (via_drill 0.4)
    (via_min_size 0.6)
    (via_min_drill 0.3)
    (uvia_size 0.3)
    (uvia_drill 0.1)
    (uvias_allowed no)
    (edge_width 0.05)
    (segment_width 0.2)
    (pcb_text_width 0.3)
    (pcb_text_size 1.5 1.5)
    (mod_edge_width 0.12)
    (mod_text_size 1 1)
    (mod_text_width 0.15)
    (pad_size 1.5 1.5)
    (pad_drill 0.8)
    (pad_to_mask_clearance 0)
    (aux_axis_origin {x0:g} {y0:g})
    (visible_elements FFFFFF7F)
  )

  (net 0 "")
  (net 1 GND)
  (net 2 TEST_SIG)

  (module Conn_01x02 (layer F.Cu) (tedit 5F000001) (tstamp 5F000101)
    (at 110 115)
    (fp_text reference J1 (at -2.4 1.27 90) (layer F.SilkS))
    (fp_text value SMOKE_IN (at 2.4 1.27 90) (layer F.Fab) hide)
    (fp_line (start -1.4 -1.2) (end 1.4 -1.2) (layer F.SilkS) (width 0.15))
    (fp_line (start 1.4 -1.2) (end 1.4 3.74) (layer F.SilkS) (width 0.15))
    (fp_line (start 1.4 3.74) (end -1.4 3.74) (layer F.SilkS) (width 0.15))
    (fp_line (start -1.4 3.74) (end -1.4 -1.2) (layer F.SilkS) (width 0.15))
    (pad 1 thru_hole rect (at 0 0) (size 1.8 1.8) (drill 1) (layers *.Cu *.Mask) (net 2 TEST_SIG))
    (pad 2 thru_hole circle (at 0 2.54) (size 1.8 1.8) (drill 1) (layers *.Cu *.Mask) (net 1 GND))
  )
  (module Conn_01x02 (layer F.Cu) (tedit 5F000001) (tstamp 5F000102)
    (at 130 115)
    (fp_text reference J2 (at -2.4 1.27 90) (layer F.SilkS))
    (fp_text value SMOKE_OUT (at 2.4 1.27 90) (layer F.Fab) hide)
    (fp_line (start -1.4 -1.2) (end 1.4 -1.2) (layer F.SilkS) (width 0.15))
    (fp_line (start 1.4 -1.2) (end 1.4 3.74) (layer F.SilkS) (width 0.15))
    (fp_line (start 1.4 3.74) (end -1.4 3.74) (layer F.SilkS) (width 0.15))
    (fp_line (start -1.4 3.74) (end -1.4 -1.2) (layer F.SilkS) (width 0.15))
    (pad 1 thru_hole rect (at 0 0) (size 1.8 1.8) (drill 1) (layers *.Cu *.Mask) (net 2 TEST_SIG))
    (pad 2 thru_hole circle (at 0 2.54) (size 1.8 1.8) (drill 1) (layers *.Cu *.Mask) (net 1 GND))
  )
  (module MountingHole_3.2mm (layer F.Cu) (tedit 5F000002) (tstamp 5F000103)
    (at 105 105)
    (fp_text reference H1 (at 0 -3) (layer F.SilkS) hide)
    (fp_text value MOUNTING_HOLE (at 0 3) (layer F.Fab) hide)
    (pad "" np_thru_hole circle (at 0 0) (size 3.2 3.2) (drill 3.2) (layers *.Cu *.Mask))
  )
  (module MountingHole_3.2mm (layer F.Cu) (tedit 5F000002) (tstamp 5F000104)
    (at 135 125)
    (fp_text reference H2 (at 0 -3) (layer F.SilkS) hide)
    (fp_text value MOUNTING_HOLE (at 0 3) (layer F.Fab) hide)
    (pad "" np_thru_hole circle (at 0 0) (size 3.2 3.2) (drill 3.2) (layers *.Cu *.Mask))
  )

  (gr_text "CADENZA REV A IMPORT SMOKE" (at 120 128) (layer F.SilkS)
    (effects (font (size 1.2 1.2) (thickness 0.2))))
  (gr_line (start {x0:g} {y0:g}) (end {x1:g} {y0:g}) (layer Edge.Cuts) (width 0.05))
  (gr_line (start {x1:g} {y0:g}) (end {x1:g} {y1:g}) (layer Edge.Cuts) (width 0.05))
  (gr_line (start {x1:g} {y1:g}) (end {x0:g} {y1:g}) (layer Edge.Cuts) (width 0.05))
  (gr_line (start {x0:g} {y1:g}) (end {x0:g} {y0:g}) (layer Edge.Cuts) (width 0.05))

  (segment (start 110 115) (end 130 115) (width 0.4) (layer F.Cu) (net 2))
  (segment (start 110 117.54) (end 130 117.54) (width 0.4) (layer B.Cu) (net 1))

  (zone (net 1) (net_name GND) (layer B.Cu) (tstamp 5F000201) (hatch edge 0.508)
    (connect_pads (clearance 0.3))
    (min_thickness 0.25)
    (fill yes (thermal_gap 0.3) (thermal_bridge_width 0.3))
    (polygon (pts
      (xy {x0 + 1:g} {y0 + 1:g}) (xy {x1 - 1:g} {y0 + 1:g})
      (xy {x1 - 1:g} {y1 - 1:g}) (xy {x0 + 1:g} {y1 - 1:g})
    ))
  )
)
'''


def legacy_library() -> str:
    return '''EESchema-LIBRARY Version 2.4
#encoding utf-8
#
# Smoke_Connector
#
DEF Smoke_Connector J 0 40 Y Y 1 F N
F0 "J" 0 150 50 H V C CNN
F1 "Smoke_Connector" 0 -150 50 H V C CNN
DRAW
S -100 100 100 -100 0 1 10 f
X 1 1 -250 50 150 R 40 40 1 1 P
X 2 2 -250 -50 150 R 40 40 1 1 P
ENDDRAW
ENDDEF
#
#End Library
'''


def legacy_schematic() -> str:
    return '''EESchema Schematic File Version 4
LIBS:cadenza_rev_a_smoke-cache
EELAYER 29 0
EELAYER END
$Descr A4 11693 8268
Sheet 1 1
Title "Cadenza Rev A JLCEDA Import Smoke Test"
Date "2026-07-20"
Rev "D0"
Comp "CadenzaOS"
Comment1 "Two nets, two connectors, self-contained legacy library"
$EndDescr
$Comp
L Smoke_Connector J1
U 1 1 5F000301
P 3500 3300
F 0 "J1" H 3580 3292 50  0000 L CNN
F 1 "SMOKE_IN" H 3580 3201 50 0000 L CNN
F 2 "Smoke:Conn_01x02" H 3500 3300 50 0001 C CNN
\t1    3500 3300
\t1 0 0 -1
$EndComp
$Comp
L Smoke_Connector J2
U 1 1 5F000302
P 6500 3300
F 0 "J2" H 6580 3292 50 0000 L CNN
F 1 "SMOKE_OUT" H 6580 3201 50 0000 L CNN
F 2 "Smoke:Conn_01x02" H 6500 3300 50 0001 C CNN
\t1    6500 3300
\t-1 0 0 -1
$EndComp
Wire Wire Line
\t3250 3250 3000 3250
Text Label 3000 3250 2 50 ~ 0
TEST_SIG
Wire Wire Line
\t3250 3350 3000 3350
Text Label 3000 3350 2 50 ~ 0
GND
Wire Wire Line
\t6750 3250 7000 3250
Text Label 7000 3250 0 50 ~ 0
TEST_SIG
Wire Wire Line
\t6750 3350 7000 3350
Text Label 7000 3350 0 50 ~ 0
GND
$EndSCHEMATC
'''


def legacy_project() -> str:
    return '''update=0
version=1
last_client=eeschema
[general]
version=1
[pcbnew]
version=1
PageLayoutDescrFile=
LastNetListRead=
UseCmpFile=1
PadDrill=0.600000000000
PadDrillOvalY=0.600000000000
PadSizeH=1.500000000000
PadSizeV=1.500000000000
PcbTextSizeV=1.500000000000
PcbTextSizeH=1.500000000000
PcbTextThickness=0.300000000000
ModuleTextSizeV=1.000000000000
ModuleTextSizeH=1.000000000000
ModuleTextSizeThickness=0.150000000000
SolderMaskClearance=0.000000000000
SolderMaskMinWidth=0.000000000000
DrawSegmentWidth=0.200000000000
BoardOutlineThickness=0.050000000000
ModuleOutlineThickness=0.150000000000
[eeschema]
version=1
LibDir=
[eeschema/libraries]
LibName1=cadenza_rev_a_smoke
'''


def dxf_rect(width: float, height: float) -> str:
    lines = [
        "0", "SECTION", "2", "HEADER", "9", "$ACADVER", "1", "AC1009", "0", "ENDSEC",
        "0", "SECTION", "2", "ENTITIES",
    ]
    for (x1, y1), (x2, y2) in [((0, 0), (width, 0)), ((width, 0), (width, height)), ((width, height), (0, height)), ((0, height), (0, 0))]:
        lines += ["0", "LINE", "8", "PCB_OUTLINE", "10", f"{x1:g}", "20", f"{y1:g}", "30", "0", "11", f"{x2:g}", "21", f"{y2:g}", "31", "0"]
    lines += ["0", "ENDSEC", "0", "EOF"]
    return "\n".join(lines) + "\n"


def box_triangles(width: float, height: float, depth: float):
    v = [(0,0,0),(width,0,0),(width,height,0),(0,height,0),(0,0,depth),(width,0,depth),(width,height,depth),(0,height,depth)]
    faces = [(0,2,1),(0,3,2),(4,5,6),(4,6,7),(0,1,5),(0,5,4),(1,2,6),(1,6,5),(2,3,7),(2,7,6),(3,0,4),(3,4,7)]
    return [(v[a], v[b], v[c]) for a,b,c in faces]


def ascii_stl(name: str, triangles) -> str:
    out = [f"solid {name}"]
    for a, b, c in triangles:
        ux, uy, uz = (b[i] - a[i] for i in range(3))
        vx, vy, vz = (c[i] - a[i] for i in range(3))
        nx, ny, nz = uy*vz-uz*vy, uz*vx-ux*vz, ux*vy-uy*vx
        length = math.sqrt(nx*nx + ny*ny + nz*nz) or 1.0
        out += [f"  facet normal {nx/length:g} {ny/length:g} {nz/length:g}", "    outer loop"]
        out += [f"      vertex {p[0]:g} {p[1]:g} {p[2]:g}" for p in (a,b,c)]
        out += ["    endloop", "  endfacet"]
    out.append(f"endsolid {name}")
    return "\n".join(out) + "\n"


def gpio_header(gpio: dict) -> str:
    rows = ["#pragma once", "", "// Generated from hardware/rev-a/design.yaml. Do not edit."]
    rows += [f"#define CADENZA_REV_A_{name} {pin}" for name, pin in gpio.items()]
    return "\n".join(rows) + "\n"


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def deterministic_zip(path: Path, members: list[Path]) -> None:
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as archive:
        for member in sorted(members, key=lambda p: p.name):
            info = zipfile.ZipInfo(member.name, date_time=(2026, 7, 20, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            archive.writestr(info, member.read_bytes())


def validate_inputs(design: dict, bom: dict) -> None:
    assert design["units"] == "mm"
    assert design["pcb"]["layers"] == 4
    assert len(design["pcb"]["mounting_holes"]) == 4
    assert design["display"]["fpc"]["pins"] == 10
    assert design["controls"]["encoder_a"]["product_center"] == [107.0, 37.0]
    assert any(item["mpn"] == "ESP32-S3-WROOM-1-N16R8" for item in bom["items"])


def main() -> None:
    design = load_json_yaml(DESIGN_PATH)
    bom = load_json_yaml(BOM_PATH)
    validate_inputs(design, bom)
    if OUT.exists():
        shutil.rmtree(OUT)
    project_dir = OUT / "kicad"
    mech_dir = OUT / "mechanical"
    report_dir = OUT / "reports"
    project_dir.mkdir(parents=True)
    mech_dir.mkdir(parents=True)
    report_dir.mkdir(parents=True)

    smoke = design["smoke_test"]
    board = smoke["board"]
    write_text(project_dir / f"{PROJECT}.kicad_pcb", kicad_pcb(board["width"], board["height"]))
    write_text(project_dir / f"{PROJECT}.sch", legacy_schematic())
    write_text(project_dir / f"{PROJECT}.lib", legacy_library())
    write_text(project_dir / f"{PROJECT}-cache.lib", legacy_library())
    write_text(project_dir / f"{PROJECT}.pro", legacy_project())
    write_text(mech_dir / "smoke-board-outline.dxf", dxf_rect(board["width"], board["height"]))
    envelope = design["product"]["envelope"]
    write_text(mech_dir / "rev-a-enclosure-envelope.stl", ascii_stl("cadenza_rev_a_envelope", box_triangles(envelope["width"], envelope["height"], envelope["depth"])))
    write_text(report_dir / "gpio_rev_a.h", gpio_header(design["gpio"]))

    with (report_dir / "bom.csv").open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=["ref", "function", "mpn", "manufacturer", "lcsc", "status", "assembly"])
        writer.writeheader()
        writer.writerows(bom["items"])

    checklist = f'''# JLCEDA Pro import smoke-test checklist

Import `{PROJECT}-jlc-import.zip` as a KiCad project.

Expected authoritative values:

- Board: {board['width']} × {board['height']} mm rectangle
- Copper layers: 4 (`F.Cu`, `In1.Cu`, `In2.Cu`, `B.Cu`)
- Nets: `GND`, `TEST_SIG`
- Connectors: 2 (`J1`, `J2`), each 2-pin through-hole
- NPTH mounting holes: 2, diameter 3.2 mm
- Routed segments: 2: `TEST_SIG` on `F.Cu` and `GND` on `B.Cu`
- Copper zones: 1 `GND` zone on `B.Cu`; JLCEDA is expected to rebuild it
- Board text: `CADENZA REV A IMPORT SMOKE`

Record import version, warnings, observed values, copper rebuild result, and whether an exported `.epro` can be reopened.
'''
    write_text(report_dir / "import-checklist.md", checklist)

    zip_members = list(project_dir.iterdir())
    zip_path = OUT / f"{PROJECT}-jlc-import.zip"
    deterministic_zip(zip_path, zip_members)

    manifest = {
        "schema": "cadenza-import-smoke-manifest/v0.1",
        "generator": "hardware/rev-a/scripts/generate.py",
        "compatibility_target": smoke["kicad_compatibility_target"],
        "expected": smoke["expected"],
        "board": board,
        "artifacts": {}
    }
    for path in sorted(p for p in OUT.rglob("*") if p.is_file() and p.name != "manifest.json"):
        manifest["artifacts"][str(path.relative_to(OUT))] = {"bytes": path.stat().st_size, "sha256": sha256(path)}
    write_text(OUT / "manifest.json", json.dumps(manifest, ensure_ascii=False, indent=2))
    print(json.dumps({"output": str(OUT), "zip": str(zip_path), "artifacts": len(manifest["artifacts"])}, ensure_ascii=False))


if __name__ == "__main__":
    main()
