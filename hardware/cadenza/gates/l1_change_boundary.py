#!/usr/bin/env python3
"""Snapshot and verify the reference PCB area locked during the L1 display edit.

Run with KiCad's bundled Python because the script uses pcbnew:

  /Applications/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/bin/python3.9 \
      l1_change_boundary.py snapshot BASE.kicad_pcb -o l1-change-boundary.json

  /Applications/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/bin/python3.9 \
      l1_change_boundary.py verify CANDIDATE.kicad_pcb \
      --baseline l1-change-boundary.json -o verification.json
"""

from __future__ import annotations

import argparse
import json
from collections import defaultdict
from pathlib import Path
from typing import Any, Dict, Iterable, List, Set, Tuple

import pcbnew


ALLOWED_EXISTING_REFS: Set[str] = {
    "FPC1", "U6", "Q2", "R13", "R14", "C20", "C21"
}
ALLOWED_DISPLAY_NETS: Set[str] = {
    "GND", "+5V", "GPIO48", "GPIO12", "GPIO14", "GPIO47", "GPIO39"
}
LEGACY_REMOVABLE_NETS: Set[str] = {
    "3V3LCD", "$1N34538", "$1N34531", "GPIO3"
}
MOUNTING_REFS = ("SCREW1", "SCREW2", "SCREW3", "SCREW4")


def mm(value: int) -> float:
    return round(value / 1_000_000.0, 6)


def pad_rows(footprint: Any) -> List[Dict[str, Any]]:
    rows = []
    for pad in footprint.Pads():
        rows.append(
            {
                "number": str(pad.GetNumber()),
                "net": str(pad.GetNetname()),
                "x_mm": mm(pad.GetPosition().x),
                "y_mm": mm(pad.GetPosition().y),
            }
        )
    return sorted(rows, key=lambda row: (row["number"], row["x_mm"], row["y_mm"]))


def footprint_row(footprint: Any) -> Dict[str, Any]:
    footprint_id = footprint.GetFPID()
    library = str(footprint_id.GetLibNickname())
    item = str(footprint_id.GetLibItemName())
    return {
        "reference": str(footprint.GetReference()),
        "footprint": f"{library}:{item}" if library else item,
        "x_mm": mm(footprint.GetPosition().x),
        "y_mm": mm(footprint.GetPosition().y),
        "rotation_deg": round(float(footprint.GetOrientationDegrees()), 6),
        "side": str(footprint.GetLayerName()),
        "pads": pad_rows(footprint),
    }


def board_snapshot(path: Path) -> Dict[str, Any]:
    board = pcbnew.LoadBoard(str(path))
    footprints = {
        str(fp.GetReference()): footprint_row(fp) for fp in board.GetFootprints()
    }
    bbox = board.GetBoardEdgesBoundingBox()

    net_endpoints: Dict[str, Set[Tuple[str, str]]] = defaultdict(set)
    for reference, row in footprints.items():
        for pad in row["pads"]:
            if pad["net"]:
                net_endpoints[pad["net"]].add((reference, pad["number"]))

    return {
        "schema_version": 1,
        "source": str(path.resolve()),
        "policy": {
            "allowed_existing_refs": sorted(ALLOWED_EXISTING_REFS),
            "allowed_display_nets": sorted(ALLOWED_DISPLAY_NETS),
            "legacy_removable_nets": sorted(LEGACY_REMOVABLE_NETS),
            "mounting_refs": list(MOUNTING_REFS),
            "meaning": "Only the old display subsystem may be deleted or repurposed during L1; all other reference footprints stay locked.",
        },
        "board_edge_bbox_mm": {
            "x": mm(bbox.GetX()),
            "y": mm(bbox.GetY()),
            "width": mm(bbox.GetWidth()),
            "height": mm(bbox.GetHeight()),
        },
        "footprints": footprints,
        "net_endpoints": {
            name: [list(endpoint) for endpoint in sorted(endpoints)]
            for name, endpoints in sorted(net_endpoints.items())
        },
    }


def comparable_locked_footprint(row: Dict[str, Any]) -> Dict[str, Any]:
    return {
        key: row[key]
        for key in ("reference", "footprint", "x_mm", "y_mm", "rotation_deg", "side", "pads")
    }


def locked_endpoints(snapshot: Dict[str, Any], net: str, locked_refs: Set[str]) -> Set[Tuple[str, str]]:
    return {
        (str(ref), str(pad))
        for ref, pad in snapshot.get("net_endpoints", {}).get(net, [])
        if str(ref) in locked_refs
    }


def verify(candidate_path: Path, baseline: Dict[str, Any]) -> Dict[str, Any]:
    candidate = board_snapshot(candidate_path)
    base_fps = baseline["footprints"]
    cand_fps = candidate["footprints"]
    locked_refs = set(base_fps) - ALLOWED_EXISTING_REFS

    missing_locked = sorted(locked_refs - set(cand_fps))
    changed_locked = []
    for ref in sorted(locked_refs & set(cand_fps)):
        if comparable_locked_footprint(base_fps[ref]) != comparable_locked_footprint(cand_fps[ref]):
            changed_locked.append(ref)

    base_bbox = baseline["board_edge_bbox_mm"]
    candidate_bbox = candidate["board_edge_bbox_mm"]
    mounting_changes = []
    for ref in MOUNTING_REFS:
        if ref not in base_fps or ref not in cand_fps:
            mounting_changes.append(ref)
            continue
        base_mount = comparable_locked_footprint(base_fps[ref])
        cand_mount = comparable_locked_footprint(cand_fps[ref])
        if base_mount != cand_mount:
            mounting_changes.append(ref)

    all_nets = set(baseline.get("net_endpoints", {})) | set(candidate.get("net_endpoints", {}))
    locked_net_changes = []
    for net in sorted(all_nets):
        before = locked_endpoints(baseline, net, locked_refs)
        after = locked_endpoints(candidate, net, locked_refs)
        if before != after:
            locked_net_changes.append(
                {"net": net, "baseline_locked_endpoints": sorted(before), "candidate_locked_endpoints": sorted(after)}
            )

    new_refs = sorted(set(cand_fps) - set(base_fps))
    unexpected_new_connections = []
    for ref in new_refs:
        for pad in cand_fps[ref]["pads"]:
            net = pad["net"]
            if net and net not in ALLOWED_DISPLAY_NETS:
                unexpected_new_connections.append({"reference": ref, "pad": pad["number"], "net": net})

    repurposed_refs = sorted(ALLOWED_EXISTING_REFS & set(cand_fps))
    unexpected_repurposed_connections = []
    for ref in repurposed_refs:
        if ref in base_fps and comparable_locked_footprint(cand_fps[ref]) == comparable_locked_footprint(base_fps[ref]):
            continue
        for pad in cand_fps[ref]["pads"]:
            net = pad["net"]
            if net and net not in ALLOWED_DISPLAY_NETS and net not in LEGACY_REMOVABLE_NETS:
                unexpected_repurposed_connections.append({"reference": ref, "pad": pad["number"], "net": net})

    checks = {
        "board_edge_bbox_unchanged": base_bbox == candidate_bbox,
        "mounting_holes_unchanged": not mounting_changes,
        "all_locked_footprints_present": not missing_locked,
        "locked_footprints_placement_footprint_and_pad_nets_unchanged": not changed_locked,
        "locked_net_endpoints_unchanged": not locked_net_changes,
        "new_components_connect_only_to_display_allowed_nets": not unexpected_new_connections,
        "repurposed_display_refs_connect_only_to_allowed_or_legacy_nets": not unexpected_repurposed_connections,
    }
    return {
        "schema_version": 1,
        "baseline": baseline["source"],
        "candidate": str(candidate_path.resolve()),
        "checks": checks,
        "details": {
            "missing_locked_refs": missing_locked,
            "changed_locked_refs": changed_locked,
            "mounting_changes": mounting_changes,
            "locked_net_changes": locked_net_changes,
            "new_refs": new_refs,
            "unexpected_new_connections": unexpected_new_connections,
            "unexpected_repurposed_connections": unexpected_repurposed_connections,
        },
        "verdict": "PASS" if all(checks.values()) else "FAIL",
        "scope_note": "This gate protects the inherited PCB boundary; it does not prove the new display circuit, routing, EMC, DFM, or physical FPC direction.",
    }


def write_json(path: Path, data: Dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)

    snapshot_parser = subparsers.add_parser("snapshot")
    snapshot_parser.add_argument("pcb", type=Path)
    snapshot_parser.add_argument("-o", "--output", type=Path, required=True)

    verify_parser = subparsers.add_parser("verify")
    verify_parser.add_argument("pcb", type=Path)
    verify_parser.add_argument("--baseline", type=Path, required=True)
    verify_parser.add_argument("-o", "--output", type=Path, required=True)

    args = parser.parse_args()
    if args.command == "snapshot":
        result = board_snapshot(args.pcb)
    else:
        baseline = json.loads(args.baseline.read_text(encoding="utf-8"))
        result = verify(args.pcb, baseline)

    write_json(args.output, result)
    print(json.dumps({"verdict": result.get("verdict", "SNAPSHOT"), "output": str(args.output)}))
    return 0 if result.get("verdict", "PASS") == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
