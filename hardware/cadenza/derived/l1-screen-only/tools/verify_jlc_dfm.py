#!/usr/bin/env python3
"""Manufacturing-oriented L1 PCB gate using the selected JLC 2-layer limits.

This does not waive the imported KiCad rule debt.  It separately proves that
the actual copper/drill geometry clears the manufacturing minima used for the
prototype order and that no electrical shorts or ratsnest items remain.
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

import pcbnew


ACTUAL_RE = re.compile(r"(?:actual|实际)\s*([0-9.]+)\s*mm", re.IGNORECASE)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--board", type=Path, required=True)
    parser.add_argument("--drc", type=Path, required=True)
    parser.add_argument("--bundle-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    board = pcbnew.LoadBoard(str(args.board))
    drc = json.loads(args.drc.read_text(encoding="utf-8"))
    bundle_audit = json.loads(
        (args.bundle_dir / "manufacturing-bom-cpl-audit.json").read_text(encoding="utf-8")
    )

    tracks = [item for item in board.GetTracks() if item.Type() == pcbnew.PCB_TRACE_T]
    vias = [item for item in board.GetTracks() if item.Type() == pcbnew.PCB_VIA_T]
    min_track = min(pcbnew.ToMM(item.GetWidth()) for item in tracks)
    min_via_drill = min(pcbnew.ToMM(item.GetDrillValue()) for item in vias)
    min_via_diameter = min(pcbnew.ToMM(item.GetWidth(pcbnew.F_Cu)) for item in vias)

    clearance_actuals = []
    edge_actuals = []
    violation_types = []
    for violation in drc.get("violations", []):
        violation_types.append(violation.get("type", ""))
        match = ACTUAL_RE.search(violation.get("description", ""))
        if not match:
            continue
        value = float(match.group(1))
        if violation.get("type") == "clearance":
            clearance_actuals.append(value)
        elif violation.get("type") == "copper_edge_clearance":
            edge_actuals.append(value)

    mounting_holes = {}
    for reference in ("SCREW1", "SCREW2", "SCREW3", "SCREW4"):
        footprint = board.FindFootprintByReference(reference)
        pads = list(footprint.Pads()) if footprint else []
        if len(pads) == 1:
            pad = pads[0]
            mounting_holes[reference] = {
                "attribute": int(pad.GetAttribute()),
                "drill_mm": pcbnew.ToMM(pad.GetDrillSize().x),
                "x_mm": pcbnew.ToMM(pad.GetPosition().x),
                "y_mm": pcbnew.ToMM(pad.GetPosition().y),
            }

    bbox = board.GetBoardEdgesBoundingBox()
    required_extensions = {".gtl", ".gbl", ".gts", ".gbs", ".gto", ".gbo", ".gm1", ".drl"}
    present_extensions = {path.suffix.lower() for path in (args.bundle_dir / "gerbers").iterdir() if path.is_file()}
    expected_holes = {
        "SCREW1": (93.125, 84.725),
        "SCREW2": (213.125, 84.725),
        "SCREW3": (93.125, 134.725),
        "SCREW4": (213.125, 134.725),
    }
    holes_ok = set(mounting_holes) == set(expected_holes)
    if holes_ok:
        for reference, (x, y) in expected_holes.items():
            hole = mounting_holes[reference]
            holes_ok &= (
                hole["attribute"] == int(pcbnew.PAD_ATTRIB_NPTH)
                and abs(hole["drill_mm"] - 2.0) < 1e-6
                and abs(hole["x_mm"] - x) < 1e-6
                and abs(hole["y_mm"] - y) < 1e-6
            )

    limits = {
        "min_trace_spacing_mm": 0.127,
        "min_trace_width_mm": 0.127,
        "min_via_drill_mm": 0.20,
        "min_via_diameter_mm": 0.45,
        "nominal_copper_to_edge_mm": 0.30,
        "edge_numeric_tolerance_mm": 0.001,
    }
    checks = {
        "two_copper_layers": board.GetCopperLayerCount() == 2,
        "board_bbox_matches_130x60_outline_with_stroke": abs(pcbnew.ToMM(bbox.GetWidth()) - 130.254) < 0.01
            and abs(pcbnew.ToMM(bbox.GetHeight()) - 60.254) < 0.01,
        "no_unconnected_items": len(drc.get("unconnected_items", [])) == 0,
        "no_shorting_items": "shorting_items" not in violation_types,
        "actual_clearances_meet_jlc_minimum": not clearance_actuals or min(clearance_actuals) >= limits["min_trace_spacing_mm"],
        "actual_track_widths_meet_jlc_minimum": min_track >= limits["min_trace_width_mm"],
        "actual_vias_meet_jlc_minimum": min_via_drill >= limits["min_via_drill_mm"]
            and min_via_diameter >= limits["min_via_diameter_mm"],
        "copper_edge_meets_nominal_with_numeric_tolerance": not edge_actuals or min(edge_actuals) >= (
            limits["nominal_copper_to_edge_mm"] - limits["edge_numeric_tolerance_mm"]
        ),
        "four_mounting_holes_are_2mm_npth_at_frozen_coordinates": holes_ok,
        "required_gerber_and_drill_extensions_present": required_extensions <= present_extensions,
        "bom_cpl_designator_gate_passes": bundle_audit.get("status") == "PASS",
    }
    report = {
        "schema_version": 1,
        "status": "PASS_PROVISIONAL_DFM" if all(checks.values()) else "FAIL",
        "production_ready": False,
        "scope": "JLC prototype geometry/file gate; placement preview, stock and physical FPC confirmation remain pending",
        "limits": limits,
        "measurements": {
            "copper_layers": board.GetCopperLayerCount(),
            "board_edges_bbox_mm": [
                pcbnew.ToMM(bbox.GetWidth()), pcbnew.ToMM(bbox.GetHeight())
            ],
            "min_track_width_mm": min_track,
            "min_reported_clearance_mm": min(clearance_actuals) if clearance_actuals else None,
            "min_via_drill_mm": min_via_drill,
            "min_via_diameter_mm": min_via_diameter,
            "min_reported_copper_edge_mm": min(edge_actuals) if edge_actuals else None,
            "mounting_holes": mounting_holes,
            "raw_drc_violations": len(drc.get("violations", [])),
            "raw_drc_unconnected": len(drc.get("unconnected_items", [])),
        },
        "checks": checks,
        "remaining_manual_gates": [
            "JLC Gerber viewer board outline/drill preview",
            "JLC BOM/CPL component match and rotation preview",
            "C324723 stock/extended-part confirmation at upload time",
            "physical Sharp FPC Pin 1/contact face/fold/latch/closed-shell confirmation",
        ],
    }
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, ensure_ascii=False))
    return 0 if report["status"] != "FAIL" else 1


if __name__ == "__main__":
    raise SystemExit(main())
