#!/usr/bin/env python3
"""Verify that the L1 PCB candidate stays inside the screen-only change boundary.

Exit codes are intentionally gate-friendly:
  0 = PASS
  1 = FAIL
  2 = PENDING (the candidate PCB does not exist yet)

Run this script with KiCad's bundled Python so that ``pcbnew`` is available.
"""

from __future__ import annotations

import argparse
import json
import sys
from dataclasses import dataclass, asdict
from pathlib import Path
from typing import Any

try:
    import pcbnew
except ImportError as exc:  # pragma: no cover - depends on the host installation
    print(
        "ERROR: pcbnew is unavailable. Run with KiCad's bundled Python:\n"
        "/Applications/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3",
        file=sys.stderr,
    )
    raise SystemExit(1) from exc


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_BASELINE = (
    PROJECT_ROOT / "Cadenza-reference-ESP32-S3-game-console-original-v2-20260722.kicad_pcb"
)
DEFAULT_CANDIDATE = (
    PROJECT_ROOT / "generated/candidate/Cadenza-L1-Screen-Only.kicad_pcb"
)

DELETED_REFS = {"FPC1", "U6", "Q2", "R13", "R14"}
ADDED_REFS = {"J_DISP1", "C_DISP1"}
MOVABLE_REUSED_REFS = {"C20", "C21"}
MOUNTING_HOLE_REFS = {"SCREW1", "SCREW2", "SCREW3", "SCREW4"}
CRITICAL_FIXED_REFS = MOUNTING_HOLE_REFS | {"U4", "USB1"}

J_DISP_FOOTPRINT = (
    "Cadenza_Display_FPC_Variants:"
    "Hirose_FH34SRJ-10S-0.5SH_50_1x10_P0.50mm_DualContact"
)
J_DISP_VALUE = "FH34SRJ-10S-0.5SH(50)"
J_DISP_PIN_MAP = {
    "1": "GPIO39",
    "2": "GPIO48",
    "3": "GPIO47",
    "4": "GPIO14",
    "5": "GPIO12",
    "6": "+5V",
    "7": "+5V",
    "8": "+5V",
    "9": "GND",
    "10": "GND",
}
C_DISP_FOOTPRINT = "Cadenza-re-easyedapro:C0805"
C_DISP_VALUE = "100nF"
C_DISP_PIN_MAP = {"1": "GPIO12", "2": "GND"}
C20_PIN_MAP = {"1": "GND", "2": "+5V"}
C21_PIN_MAP = {"1": "+5V", "2": "GND"}

# These nets existed only to support the removed TFT/backlight branch.  The
# three unconnected FPC pads had no PCB net objects, so there are four PCB-level
# names to forbid.
LEGACY_DISPLAY_NETS = {"3V3LCD", "GPIO3", "$1N34531", "$1N34538"}


@dataclass
class Check:
    name: str
    passed: bool
    expected: Any
    actual: Any
    evidence: str


def point_tuple(point: Any) -> tuple[int, int]:
    return (int(point.x), int(point.y))


def orientation_degrees(item: Any) -> float:
    if hasattr(item, "GetOrientationDegrees"):
        return round(float(item.GetOrientationDegrees()) % 360.0, 6)
    return round(float(item.GetOrientation().AsDegrees()) % 360.0, 6)


def layer_set_tuple(item: Any) -> tuple[int, ...]:
    layer_set = item.GetLayerSet()
    return tuple(
        layer
        for layer in range(int(pcbnew.PCB_LAYER_ID_COUNT))
        if layer_set.Contains(layer)
    )


def pad_shape_signature(pad: Any, *, include_net: bool) -> dict[str, Any]:
    signature: dict[str, Any] = {
        "number": str(pad.GetNumber()),
        "relative_position_iu": point_tuple(pad.GetFPRelativePosition()),
        "relative_orientation_deg": round(
            float(pad.GetFPRelativeOrientation().AsDegrees()) % 360.0, 6
        ),
        "size_iu": point_tuple(pad.GetSize()),
        "drill_iu": point_tuple(pad.GetDrillSize()),
        "shape": int(pad.GetShape()),
        "attribute": int(pad.GetAttribute()),
        "layers": layer_set_tuple(pad),
    }
    if include_net:
        signature["net"] = str(pad.GetNetname())
    return signature


def footprint_signature(footprint: Any) -> dict[str, Any]:
    return {
        "position_iu": point_tuple(footprint.GetPosition()),
        "rotation_deg": orientation_degrees(footprint),
        "side": "B" if footprint.IsFlipped() else "F",
        "footprint": str(footprint.GetFPID().GetUniStringLibId()),
        "value": str(footprint.GetValue()),
    }


def footprint_physical_signature(footprint: Any) -> dict[str, Any]:
    result = footprint_signature(footprint)
    result["pads"] = sorted(
        (pad_shape_signature(pad, include_net=False) for pad in footprint.Pads()),
        key=lambda item: (
            item["number"],
            item["relative_position_iu"],
            item["layers"],
        ),
    )
    return result


def ref_map(board: Any) -> tuple[dict[str, Any], list[str]]:
    footprints: dict[str, Any] = {}
    duplicates: list[str] = []
    for footprint in board.GetFootprints():
        ref = str(footprint.GetReference())
        if ref in footprints:
            duplicates.append(ref)
        footprints[ref] = footprint
    return footprints, sorted(duplicates)


def pad_net_map(footprint: Any, *, include_mechanical: bool = False) -> dict[str, str]:
    result: dict[str, str] = {}
    duplicates: set[str] = set()
    for pad in footprint.Pads():
        number = str(pad.GetNumber())
        if not include_mechanical and number not in {str(i) for i in range(1, 11)}:
            continue
        if number in result:
            duplicates.add(number)
        result[number] = str(pad.GetNetname())
    for number in duplicates:
        result[number] = f"<duplicate pad number: {number}>"
    return result


def edge_signature(board: Any) -> list[dict[str, Any]]:
    shapes: list[dict[str, Any]] = []
    for drawing in board.GetDrawings():
        if drawing.GetLayer() != pcbnew.Edge_Cuts:
            continue
        shape = {
            "type": type(drawing).__name__,
            "shape": int(drawing.GetShape()),
            "start_iu": point_tuple(drawing.GetStart()),
            "end_iu": point_tuple(drawing.GetEnd()),
            "center_iu": point_tuple(drawing.GetCenter()),
            "width_iu": int(drawing.GetWidth()),
        }
        if int(drawing.GetShape()) == int(pcbnew.SHAPE_T_ARC):
            shape["arc_mid_iu"] = point_tuple(drawing.GetArcMid())
        shapes.append(shape)
    return sorted(shapes, key=lambda item: json.dumps(item, sort_keys=True))


def connectivity_metrics(board: Any) -> dict[str, int]:
    board.BuildConnectivity()
    connectivity = board.GetConnectivity()
    tracks = list(board.GetTracks())
    return {
        "footprints": len(list(board.GetFootprints())),
        "pads": sum(len(list(fp.Pads())) for fp in board.GetFootprints()),
        "net_count": int(connectivity.GetNetCount()),
        "node_count": int(connectivity.GetNodeCount()),
        "connectivity_pad_count": int(connectivity.GetPadCount()),
        "track_and_via_count": len(tracks),
        "track_count": sum(not isinstance(item, pcbnew.PCB_VIA) for item in tracks),
        "via_count": sum(isinstance(item, pcbnew.PCB_VIA) for item in tracks),
        "zone_count": len(list(board.Zones())),
        "unconnected_count": int(connectivity.GetUnconnectedCount(False)),
    }


def board_net_names(board: Any) -> set[str]:
    return {
        str(net.GetNetname())
        for _, net in board.GetNetInfo().NetsByNetcode().items()
        if str(net.GetNetname())
    }


def legacy_net_evidence(board: Any) -> dict[str, list[str]]:
    evidence: dict[str, list[str]] = {name: [] for name in sorted(LEGACY_DISPLAY_NETS)}
    for footprint in board.GetFootprints():
        ref = str(footprint.GetReference())
        for pad in footprint.Pads():
            net = str(pad.GetNetname())
            if net in evidence:
                evidence[net].append(f"pad:{ref}.{pad.GetNumber()}")
    for item in board.GetTracks():
        net = str(item.GetNetname())
        if net in evidence:
            kind = "via" if isinstance(item, pcbnew.PCB_VIA) else "track"
            evidence[net].append(f"{kind}@{point_tuple(item.GetPosition())}")
    for index, zone in enumerate(board.Zones()):
        net = str(zone.GetNetname())
        if net in evidence:
            evidence[net].append(f"zone:{index}")
    for net in sorted(LEGACY_DISPLAY_NETS & board_net_names(board)):
        evidence[net].append("netinfo")
    return {name: items for name, items in evidence.items() if items}


def add_check(
    checks: list[Check], name: str, expected: Any, actual: Any, evidence: str
) -> None:
    checks.append(Check(name, expected == actual, expected, actual, evidence))


def verify(baseline: Any, candidate: Any) -> tuple[list[Check], dict[str, Any]]:
    checks: list[Check] = []
    base_refs, base_duplicates = ref_map(baseline)
    candidate_refs, candidate_duplicates = ref_map(candidate)
    base_set = set(base_refs)
    candidate_set = set(candidate_refs)

    add_check(checks, "baseline_reference_uniqueness", [], base_duplicates, "baseline footprints")
    add_check(checks, "candidate_reference_uniqueness", [], candidate_duplicates, "candidate footprints")
    add_check(
        checks,
        "deleted_references_exact",
        sorted(DELETED_REFS),
        sorted(base_set - candidate_set),
        "baseline refs minus candidate refs",
    )
    add_check(
        checks,
        "added_references_exact",
        sorted(ADDED_REFS),
        sorted(candidate_set - base_set),
        "candidate refs minus baseline refs",
    )
    add_check(checks, "candidate_footprint_count", 74, len(candidate_refs), "candidate footprint inventory")

    retained = base_set - DELETED_REFS
    frozen = retained - MOVABLE_REUSED_REFS
    add_check(checks, "frozen_reference_count", 70, len(frozen), "77 baseline - 5 deleted - 2 movable caps")
    base_frozen = {ref: footprint_signature(base_refs[ref]) for ref in sorted(frozen)}
    candidate_frozen = {
        ref: footprint_signature(candidate_refs[ref]) if ref in candidate_refs else None
        for ref in sorted(frozen)
    }
    add_check(
        checks,
        "other_70_footprint_placement_identity",
        base_frozen,
        candidate_frozen,
        "position, rotation, side, footprint ID and value",
    )

    for ref, expected_pin_map in (("C20", C20_PIN_MAP), ("C21", C21_PIN_MAP)):
        base_fp = base_refs.get(ref)
        candidate_fp = candidate_refs.get(ref)
        if base_fp is None or candidate_fp is None:
            add_check(checks, f"{ref}_retained", True, False, "footprint existence")
            continue
        base_identity = footprint_signature(base_fp)
        candidate_identity = footprint_signature(candidate_fp)
        for allowed in ("position_iu", "rotation_deg"):
            base_identity.pop(allowed)
            candidate_identity.pop(allowed)
        add_check(
            checks,
            f"{ref}_identity_except_position_rotation",
            base_identity,
            candidate_identity,
            "side, footprint ID and value remain frozen",
        )
        add_check(
            checks,
            f"{ref}_pad_geometry",
            sorted(
                (pad_shape_signature(pad, include_net=False) for pad in base_fp.Pads()),
                key=lambda item: item["number"],
            ),
            sorted(
                (pad_shape_signature(pad, include_net=False) for pad in candidate_fp.Pads()),
                key=lambda item: item["number"],
            ),
            "footprint-local pad geometry",
        )
        add_check(
            checks,
            f"{ref}_display_power_net_change_only",
            expected_pin_map,
            pad_net_map(candidate_fp),
            "allowed display net reassignment",
        )

    add_check(
        checks,
        "board_outline_exact",
        edge_signature(baseline),
        edge_signature(candidate),
        "all Edge.Cuts line/arc coordinates and widths in KiCad internal units",
    )

    add_check(
        checks,
        "critical_fixed_refs_present",
        sorted(CRITICAL_FIXED_REFS),
        sorted(CRITICAL_FIXED_REFS & candidate_set),
        "mounting holes, ESP32-S3 module and USB connector",
    )
    if CRITICAL_FIXED_REFS <= base_set and CRITICAL_FIXED_REFS <= candidate_set:
        add_check(
            checks,
            "critical_fixed_physical_geometry",
            {ref: footprint_physical_signature(base_refs[ref]) for ref in sorted(CRITICAL_FIXED_REFS)},
            {
                ref: footprint_physical_signature(candidate_refs[ref])
                for ref in sorted(CRITICAL_FIXED_REFS)
            },
            "full footprint and pad geometry; pad nets intentionally excluded",
        )
        add_check(
            checks,
            "USB1_pin_nets_unchanged",
            pad_net_map(base_refs["USB1"], include_mechanical=True),
            pad_net_map(candidate_refs["USB1"], include_mechanical=True),
            "USB connector endpoints are frozen even if nearby copper is rerouted",
        )

    j = candidate_refs.get("J_DISP1")
    if j is None:
        add_check(checks, "J_DISP1_present", True, False, "candidate footprint inventory")
    else:
        add_check(checks, "J_DISP1_footprint", J_DISP_FOOTPRINT, footprint_signature(j)["footprint"], "FPID")
        add_check(checks, "J_DISP1_value", J_DISP_VALUE, str(j.GetValue()), "footprint value")
        add_check(checks, "J_DISP1_pin_map", J_DISP_PIN_MAP, pad_net_map(j), "pads 1 through 10")

    c_disp = candidate_refs.get("C_DISP1")
    if c_disp is None:
        add_check(checks, "C_DISP1_present", True, False, "candidate footprint inventory")
    else:
        add_check(
            checks,
            "C_DISP1_footprint",
            C_DISP_FOOTPRINT,
            footprint_signature(c_disp)["footprint"],
            "FPID",
        )
        add_check(checks, "C_DISP1_value", C_DISP_VALUE, str(c_disp.GetValue()), "footprint value")
        add_check(checks, "C_DISP1_pin_map", C_DISP_PIN_MAP, pad_net_map(c_disp), "DISP capacitor endpoints")

    old_net_hits = legacy_net_evidence(candidate)
    add_check(
        checks,
        "legacy_display_nets_and_copper_absent",
        {},
        old_net_hits,
        "netinfo, pads, tracks, vias and zones",
    )

    base_metrics = connectivity_metrics(baseline)
    candidate_metrics = connectivity_metrics(candidate)
    add_check(
        checks,
        "candidate_zero_unconnected",
        0,
        candidate_metrics["unconnected_count"],
        "pcbnew connectivity rebuilt before reading count",
    )
    return checks, {"baseline": base_metrics, "candidate": candidate_metrics}


def concise(value: Any, limit: int = 500) -> str:
    rendered = json.dumps(value, ensure_ascii=False, sort_keys=True)
    if len(rendered) <= limit:
        return rendered
    return rendered[: limit - 3] + "..."


def emit_human(status: str, checks: list[Check], metrics: dict[str, Any], paths: dict[str, str]) -> None:
    print(f"STATUS: {status}")
    print(f"baseline: {paths['baseline']}")
    print(f"candidate: {paths['candidate']}")
    if status == "PENDING":
        print("Candidate PCB does not exist yet; no design checks were reported as PASS.")
        return
    passed = sum(check.passed for check in checks)
    print(f"checks: {passed}/{len(checks)} passed")
    for check in checks:
        marker = "PASS" if check.passed else "FAIL"
        print(f"[{marker}] {check.name}")
        if not check.passed:
            print(f"  expected: {concise(check.expected)}")
            print(f"  actual:   {concise(check.actual)}")
            print(f"  evidence: {check.evidence}")
    print("connectivity metrics:")
    print(json.dumps(metrics, ensure_ascii=False, indent=2, sort_keys=True))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", type=Path, default=DEFAULT_BASELINE)
    parser.add_argument("--candidate", type=Path, default=DEFAULT_CANDIDATE)
    parser.add_argument("--json", action="store_true", help="emit one machine-readable JSON object")
    args = parser.parse_args()

    baseline_path = args.baseline.expanduser().resolve()
    candidate_path = args.candidate.expanduser().resolve()
    paths = {"baseline": str(baseline_path), "candidate": str(candidate_path)}

    if not baseline_path.is_file():
        payload = {"status": "FAIL", "paths": paths, "error": "baseline PCB is missing"}
        if args.json:
            print(json.dumps(payload, ensure_ascii=False, indent=2))
        else:
            print(f"STATUS: FAIL\nbaseline PCB is missing: {baseline_path}")
        return 1

    if not candidate_path.is_file():
        payload = {
            "status": "PENDING",
            "paths": paths,
            "checks": [],
            "reason": "candidate PCB does not exist yet; no design checks were executed",
        }
        if args.json:
            print(json.dumps(payload, ensure_ascii=False, indent=2))
        else:
            emit_human("PENDING", [], {}, paths)
        return 2

    try:
        baseline = pcbnew.LoadBoard(str(baseline_path))
        candidate = pcbnew.LoadBoard(str(candidate_path))
        checks, metrics = verify(baseline, candidate)
    except Exception as exc:
        payload = {"status": "FAIL", "paths": paths, "error": f"{type(exc).__name__}: {exc}"}
        if args.json:
            print(json.dumps(payload, ensure_ascii=False, indent=2))
        else:
            print(f"STATUS: FAIL\n{payload['error']}")
        return 1

    status = "PASS" if all(check.passed for check in checks) else "FAIL"
    payload = {
        "status": status,
        "paths": paths,
        "checks": [asdict(check) for check in checks],
        "connectivity_metrics": metrics,
    }
    if args.json:
        print(json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True))
    else:
        emit_human(status, checks, metrics, paths)
    return 0 if status == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
