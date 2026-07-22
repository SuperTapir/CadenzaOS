#!/usr/bin/env python3
"""Extract authoritative L1 PCB XY/mechanical evidence with KiCad pcbnew.

Run this with KiCad's bundled Python.  The CAD generator consumes the JSON and
never guesses the J_DISP1/USB1 placement from an old enclosure experiment.
"""

from __future__ import annotations

import hashlib
import json
from pathlib import Path

import pcbnew


HERE = Path(__file__).resolve().parent
REPO = HERE.parents[3]
PCB = REPO / "hardware/cadenza/derived/l1-screen-only/generated/candidate/Cadenza-L1-Screen-Only.kicad_pcb"
OUTPUT = HERE / "generated/candidate-pcb-mechanical.json"

# The nominal 130 x 60 mm board edges are unchanged from the reference.  KiCad
# uses top-left (88.125, 79.725); the enclosure uses lower-left (0, 0).
PCB_KICAD_LEFT = 88.125
PCB_KICAD_BOTTOM = 139.725
SCREEN_PANEL_XY = (35.10, 10.09, 97.90, 52.91)
EXPECTED = {
    "J_DISP1": {"position_kicad_mm": [154.625, 128.500], "rotation_deg": 0.0},
    "USB1": {"position_kicad_mm": [153.125, 136.121], "rotation_deg": 0.0},
}


def mm(value: int) -> float:
    return pcbnew.ToMM(value)


def mech_xy(x_kicad: float, y_kicad: float) -> tuple[float, float]:
    return x_kicad - PCB_KICAD_LEFT, PCB_KICAD_BOTTOM - y_kicad


def courtyard_bbox(fp) -> list[float] | None:
    boxes = []
    for item in fp.GraphicalItems():
        if item.GetLayer() != pcbnew.F_CrtYd:
            continue
        bounds = item.GetBoundingBox()
        left, lower = mech_xy(mm(bounds.GetLeft()), mm(bounds.GetBottom()))
        right, upper = mech_xy(mm(bounds.GetRight()), mm(bounds.GetTop()))
        boxes.append((left, lower, right, upper))
    if not boxes:
        return None
    return [
        round(min(v[0] for v in boxes), 6),
        round(min(v[1] for v in boxes), 6),
        round(max(v[2] for v in boxes), 6),
        round(max(v[3] for v in boxes), 6),
    ]


def intersection_area(a, b) -> float:
    return max(0.0, min(a[2], b[2]) - max(a[0], b[0])) * max(
        0.0, min(a[3], b[3]) - max(a[1], b[1])
    )


def main() -> int:
    if not PCB.is_file():
        raise FileNotFoundError(PCB)
    board = pcbnew.LoadBoard(str(PCB))
    footprints = list(board.GetFootprints())
    records = {}
    no_courtyard = []
    screen_overlaps = []
    for fp in footprints:
        ref = fp.GetReference()
        pos = fp.GetPosition()
        x_kicad, y_kicad = mm(pos.x), mm(pos.y)
        bbox = courtyard_bbox(fp)
        record = {
            "reference": ref,
            "value": fp.GetValue(),
            "footprint": str(fp.GetFPID().GetLibItemName()),
            "side": "front" if fp.GetLayer() == pcbnew.F_Cu else "back",
            "position_kicad_mm": [round(x_kicad, 6), round(y_kicad, 6)],
            "position_enclosure_mm": [round(v, 6) for v in mech_xy(x_kicad, y_kicad)],
            "rotation_deg": round(fp.GetOrientationDegrees(), 6),
            "courtyard_xy_enclosure_mm": bbox,
        }
        records[ref] = record
        if bbox is None:
            no_courtyard.append(ref)
        else:
            area = intersection_area(bbox, SCREEN_PANEL_XY)
            if area > 1e-9:
                screen_overlaps.append({"reference": ref, "overlap_area_mm2": round(area, 6)})

    edge = board.GetBoardEdgesBoundingBox()
    edge_kicad = [mm(edge.GetLeft()), mm(edge.GetTop()), mm(edge.GetRight()), mm(edge.GetBottom())]
    critical_checks = {}
    for ref, expected in EXPECTED.items():
        actual = records.get(ref)
        critical_checks[ref] = bool(
            actual
            and all(abs(a - b) < 1e-6 for a, b in zip(actual["position_kicad_mm"], expected["position_kicad_mm"]))
            and abs(actual["rotation_deg"] - expected["rotation_deg"]) < 1e-6
            and actual["courtyard_xy_enclosure_mm"] is not None
        )

    mounting = []
    for ref in ("SCREW1", "SCREW2", "SCREW3", "SCREW4"):
        fp = board.FindFootprintByReference(ref)
        if fp is None:
            continue
        pos = fp.GetPosition()
        pads = list(fp.Pads())
        mounting.append({
            "reference": ref,
            "centre_enclosure_mm": [round(v, 6) for v in mech_xy(mm(pos.x), mm(pos.y))],
            "pcb_drill_diameter_mm": round(max((mm(p.GetDrillSizeX()) for p in pads), default=0.0), 6),
        })

    j_box = records["J_DISP1"]["courtyard_xy_enclosure_mm"]
    usb_box = records["USB1"]["courtyard_xy_enclosure_mm"]
    vertical_gap = j_box[1] - usb_box[3]
    result = {
        "schema_version": 1,
        "source_pcb": str(PCB),
        "source_pcb_sha256": hashlib.sha256(PCB.read_bytes()).hexdigest(),
        "coordinate_transform": {
            "x_enclosure": "x_kicad - 88.125",
            "y_enclosure": "139.725 - y_kicad",
            "note": "KiCad -Y (up) maps to enclosure +Y.",
        },
        "board_edge_bbox_kicad_mm": [round(v, 6) for v in edge_kicad],
        "nominal_board_xy_enclosure_mm": [0.0, 0.0, 130.0, 60.0],
        "mounting_holes": sorted(mounting, key=lambda v: v["reference"]),
        "critical_footprints": {ref: records[ref] for ref in EXPECTED},
        "front_courtyard_envelopes": {
            ref: record["courtyard_xy_enclosure_mm"]
            for ref, record in sorted(records.items())
            if record["side"] == "front" and record["courtyard_xy_enclosure_mm"] is not None
        },
        "critical_checks": critical_checks,
        "j_disp1_to_usb1_courtyard_gap_y_mm": round(vertical_gap, 6),
        "screen_panel_xy_enclosure_mm": list(SCREEN_PANEL_XY),
        "front_courtyard_screen_projection_overlaps": sorted(screen_overlaps, key=lambda v: v["reference"]),
        "front_courtyard_screen_projection_overlap_count": len(screen_overlaps),
        "courtyard_coverage": {
            "with_f_courtyard": len(footprints) - len(no_courtyard),
            "footprint_count": len(footprints),
            "missing_references": sorted(no_courtyard),
        },
        "limits": [
            "Courtyard overlap is a two-dimensional projection, not a three-dimensional hard collision.",
            "Footprint courtyard is an assembly allowance and may exceed the physical body.",
            "Component heights and PCB-to-front-cover Z datum are not reliably encoded in the imported project.",
        ],
    }
    if not all(critical_checks.values()):
        raise RuntimeError(f"candidate PCB critical placement drift: {critical_checks}")
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({
        "status": "PASS",
        "pcb_sha256": result["source_pcb_sha256"],
        "footprints": len(footprints),
        "screen_projection_overlaps": len(screen_overlaps),
        "j_to_usb_gap_y_mm": result["j_disp1_to_usb1_courtyard_gap_y_mm"],
    }))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
