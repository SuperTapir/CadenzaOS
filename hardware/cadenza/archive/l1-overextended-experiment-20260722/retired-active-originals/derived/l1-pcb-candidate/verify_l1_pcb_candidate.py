#!/usr/bin/env python3
"""Independently verify the two unrouted L1 PCB synchronization candidates.

This verifier intentionally does not import the generator.  PASS means the
candidate PCB objects match the reviewed netlist and preserved mechanical
baseline; it never means routed, DRC-clean, or production-ready.
"""

from __future__ import annotations

import hashlib
import json
import re
import xml.etree.ElementTree as ET
from collections import Counter, defaultdict
from pathlib import Path

import pcbnew


HERE = Path(__file__).resolve().parent
REPO = HERE.parents[3]
SOURCE = REPO / "hardware/cadenza/working-base/Cadenza-reference-ESP32-S3-game-console-original-v2-20260722.kicad_pcb"
NETLIST = REPO / "hardware/cadenza/derived/l1-candidate/candidate.netlist.xml"
PLACEMENT = REPO / "hardware/cadenza/derived/l1-pcb-placement/placement-feasibility.json"
GENERATION = HERE / "generation-report.json"
OUTPUT = HERE / "verification-report.json"
VARIANTS = {
    "rotation_0": HERE / "Cadenza-L1-fpc-pin1-rotation-0.kicad_pcb",
    "rotation_180": HERE / "Cadenza-L1-fpc-pin1-rotation-180.kicad_pcb",
}
DELETED_REFS = {"C20", "C21", "FPC1", "KEY1", "Q2", "R13", "R14", "SW1", "U6"}
MOVED_REFS = {"USB1", "R1", "R2"}
REROUTED_NETS = {
    "/D+", "/D-", "/VBUS", "Net-(LED1-+)", "Net-(USB1-CC1)", "Net-(USB1-CC2)",
    "GPIO12", "GPIO14", "GPIO39", "GPIO47", "GPIO48",
    "PWR_FORCE_OFF", "PWR_KEY_SENSE",
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def mm(value: int) -> float:
    return round(pcbnew.ToMM(value), 6)


def vec(value) -> tuple[float, float]:
    return (mm(value.x), mm(value.y))


def parse_netlist():
    root = ET.parse(NETLIST).getroot()
    refs = {item.attrib["ref"] for item in root.findall("./components/comp")}
    endpoint_to_net: dict[tuple[str, str], str] = {}
    net_names: set[str] = set()
    for net in root.findall("./nets/net"):
        name = net.attrib["name"]
        net_names.add(name)
        for node in net.findall("node"):
            endpoint = (node.attrib["ref"], node.attrib["pin"])
            if endpoint in endpoint_to_net:
                raise RuntimeError(f"duplicate logical endpoint {endpoint}")
            endpoint_to_net[endpoint] = name
    return refs, endpoint_to_net, net_names


def footprint_map(board) -> dict[str, object]:
    result = {}
    for footprint in board.GetFootprints():
        ref = footprint.GetReference()
        if ref in result:
            raise RuntimeError(f"duplicate footprint reference {ref}")
        result[ref] = footprint
    return result


def footprint_pose(footprint) -> tuple:
    return (*vec(footprint.GetPosition()), round(footprint.GetOrientationDegrees(), 6), footprint.GetLayerName())


def edge_signature(board) -> list[tuple]:
    result = []
    for item in board.GetDrawings():
        if item.GetLayer() != pcbnew.Edge_Cuts:
            continue
        shape = item.GetShapeStr()
        signature = [shape, vec(item.GetStart()), vec(item.GetEnd()), round(pcbnew.ToMM(item.GetWidth()), 6)]
        if shape == "Arc":
            signature.append(vec(item.GetCenter()))
        result.append(tuple(signature))
    return sorted(result, key=repr)


def mounting_signature(by_ref: dict[str, object]) -> dict[str, list[tuple]]:
    result = {}
    for ref in ("SCREW1", "SCREW2", "SCREW3", "SCREW4"):
        footprint = by_ref[ref]
        result[ref] = sorted(
            (pad.GetNumber(), *vec(pad.GetPosition()), *vec(pad.GetDrillSize()))
            for pad in footprint.Pads()
        )
    return result


def uuid_audit(board) -> dict[str, object]:
    values = []
    for footprint in board.GetFootprints():
        values.append(footprint.m_Uuid.AsString())
        values.extend(pad.m_Uuid.AsString() for pad in footprint.Pads())
    duplicates = sorted(value for value, count in Counter(values).items() if count > 1)
    return {"scope": "footprints_and_pads", "items": len(values), "duplicate_uuids": duplicates}


def direct_child_blocks(text: str) -> list[tuple[int, int, str, str]]:
    blocks = []
    depth = 0
    start = None
    in_string = False
    escaped = False
    for index, char in enumerate(text):
        if in_string:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                in_string = False
            continue
        if char == '"':
            in_string = True
        elif char == "(":
            depth += 1
            if depth == 2:
                start = index
        elif char == ")":
            if depth == 2 and start is not None:
                block = text[start:index + 1]
                match = re.match(r"\(\s*([^\s()]+)", block)
                blocks.append((start, index + 1, match.group(1) if match else "", block))
                start = None
            depth -= 1
    return blocks


def without_j_disp1(path: Path) -> str:
    text = path.read_text(encoding="utf-8")
    for start, end, head, block in direct_child_blocks(text):
        if head != "footprint":
            continue
        match = re.search(r'\(property\s+"Reference"\s+"([^"]+)"', block)
        if match and match.group(1) == "J_DISP1":
            return text[:start] + "(J_DISP1_VARIANT_REMOVED)" + text[end:]
    raise RuntimeError(f"J_DISP1 not found in {path}")


def strip_named_blocks(text: str, head: str) -> str:
    """Remove every S-expression with the requested head, preserving the rest."""
    spans = []
    index = 0
    needle = f"({head}"
    while index < len(text):
        start = text.find(needle, index)
        if start < 0:
            break
        boundary = start + len(needle)
        if boundary < len(text) and text[boundary] not in " \t\r\n)":
            index = start + 1
            continue
        index = start
        depth = 0
        in_string = False
        escaped = False
        while index < len(text):
            char = text[index]
            if in_string:
                if escaped:
                    escaped = False
                elif char == "\\":
                    escaped = True
                elif char == '"':
                    in_string = False
            elif char == '"':
                in_string = True
            elif char == "(":
                depth += 1
            elif char == ")":
                depth -= 1
                if depth == 0:
                    spans.append((start, index + 1))
                    index += 1
                    break
            index += 1
        else:
            raise RuntimeError(f"unterminated {head} block")
    pieces = []
    cursor = 0
    for start, end in spans:
        pieces.append(text[cursor:start])
        cursor = end
    pieces.append(text[cursor:])
    return "".join(pieces)


def variant_audit(name, path, source_board, source_refs, logical_refs, endpoint_to_net, net_names, placement):
    board = pcbnew.LoadBoard(str(path))
    by_ref = footprint_map(board)
    actual_refs = set(by_ref)
    actual_nets = {str(value) for value in board.GetNetsByName().keys() if str(value)}

    physical = defaultdict(list)
    for ref, footprint in by_ref.items():
        for pad in footprint.Pads():
            if pad.GetNumber():
                physical[(ref, pad.GetNumber())].append(pad.GetNetname())
    missing_endpoints = sorted(endpoint for endpoint in endpoint_to_net if endpoint not in physical)
    wrong_endpoint_nets = []
    for endpoint, expected in endpoint_to_net.items():
        for actual in physical.get(endpoint, []):
            if actual != expected:
                wrong_endpoint_nets.append({"endpoint": list(endpoint), "expected": expected, "actual": actual})
    extra_physical = []
    for endpoint, actual_values in physical.items():
        if endpoint not in endpoint_to_net:
            extra_physical.extend({"endpoint": list(endpoint), "actual": actual} for actual in actual_values)

    preserved_pose_mismatches = []
    source_by_ref = footprint_map(source_board)
    for ref in sorted(source_refs & actual_refs - MOVED_REFS):
        if footprint_pose(source_by_ref[ref]) != footprint_pose(by_ref[ref]):
            preserved_pose_mismatches.append({
                "reference": ref,
                "source": footprint_pose(source_by_ref[ref]),
                "candidate": footprint_pose(by_ref[ref]),
            })

    edge_box = source_board.GetBoardEdgesBoundingBox()
    xmin = pcbnew.ToMM(edge_box.GetX())
    ymax = pcbnew.ToMM(edge_box.GetBottom())
    usb_xy = placement["candidate"]["usb1_move"]["new_position_xy_mm"]
    expected_positions = {
        "USB1": (round(xmin + usb_xy[0], 6), round(ymax - usb_xy[1], 6)),
        "R1": (round(xmin + placement["candidate"]["usb_cc_resistor_moves"]["target_x_mm"], 6), mm(source_by_ref["R1"].GetPosition().y)),
        "R2": (round(xmin + placement["candidate"]["usb_cc_resistor_moves"]["target_x_mm"], 6), mm(source_by_ref["R2"].GetPosition().y)),
    }
    actual_positions = {ref: vec(by_ref[ref].GetPosition()) for ref in expected_positions}

    tracks_by_net = Counter(track.GetNetname() for track in board.GetTracks())
    rerouted_net_copper = {net: tracks_by_net[net] for net in sorted(REROUTED_NETS) if tracks_by_net[net]}
    uuids = uuid_audit(board)
    checks = {
        "footprint_count_94": len(actual_refs) == 94,
        "reference_set_exactly_netlist": actual_refs == logical_refs,
        "deleted_references_absent": not (actual_refs & DELETED_REFS),
        "net_names_exactly_netlist": actual_nets == net_names,
        "all_logical_endpoints_present": not missing_endpoints,
        "all_physical_endpoint_nets_match": not wrong_endpoint_nets,
        "only_two_j_disp1_mp_extra_pads": extra_physical == [
            {"endpoint": ["J_DISP1", "MP"], "actual": ""},
            {"endpoint": ["J_DISP1", "MP"], "actual": ""},
        ],
        "footprint_and_pad_uuids_unique": not uuids["duplicate_uuids"],
        "edge_cuts_exactly_preserved": edge_signature(board) == edge_signature(source_board),
        "four_mounting_holes_exactly_preserved": mounting_signature(by_ref) == mounting_signature(source_by_ref),
        "preserved_reference_poses_exact": not preserved_pose_mismatches,
        "usb_r1_r2_positions_match_plan": actual_positions == expected_positions,
        "rerouted_target_nets_have_no_old_tracks_or_vias": not rerouted_net_copper,
    }
    return {
        "path": str(path.relative_to(REPO)),
        "sha256": sha256(path),
        "footprints": len(actual_refs),
        "named_nets": len(actual_nets),
        "missing_endpoints": [list(value) for value in missing_endpoints],
        "wrong_endpoint_nets": wrong_endpoint_nets,
        "extra_physical_pads": extra_physical,
        "uuid_audit": uuids,
        "preserved_pose_mismatches": preserved_pose_mismatches,
        "expected_moved_positions_board_xy_mm": expected_positions,
        "actual_moved_positions_board_xy_mm": actual_positions,
        "rerouted_net_copper_items": rerouted_net_copper,
        "checks": checks,
    }


def main() -> int:
    logical_refs, endpoint_to_net, net_names = parse_netlist()
    placement = json.loads(PLACEMENT.read_text(encoding="utf-8"))
    generation = json.loads(GENERATION.read_text(encoding="utf-8"))
    source_board = pcbnew.LoadBoard(str(SOURCE))
    source_refs = set(footprint_map(source_board))
    variants = {
        name: variant_audit(name, path, source_board, source_refs, logical_refs, endpoint_to_net, net_names, placement)
        for name, path in VARIANTS.items()
    }
    generated_hashes = {Path(item["output"]).name: item["sha256"] for item in generation["variants"]}
    report_hashes = {path.name: item["sha256"] for path, item in zip(VARIANTS.values(), variants.values())}
    cross_checks = {
        "source_hash_matches_placement_evidence": sha256(SOURCE) == placement["sources"]["reference_board_sha256"],
        "source_hash_matches_generation_evidence": sha256(SOURCE) == generation["source"]["board_sha256"],
        "netlist_hash_matches_generation_evidence": sha256(NETLIST) == generation["source"]["netlist_sha256"],
        "placement_hash_matches_generation_evidence": sha256(PLACEMENT) == generation["source"]["placement_sha256"],
        "board_hashes_match_generation_report": report_hashes == generated_hashes,
        "variants_static_board_identical_outside_j_disp1": strip_named_blocks(
            without_j_disp1(VARIANTS["rotation_0"]), "filled_polygon"
        ) == strip_named_blocks(
            without_j_disp1(VARIANTS["rotation_180"]), "filled_polygon"
        ),
        "j_disp1_pose_differs_by_180_degrees": False,
    }
    boards = {name: pcbnew.LoadBoard(str(path)) for name, path in VARIANTS.items()}
    j0 = footprint_map(boards["rotation_0"])["J_DISP1"]
    j180 = footprint_map(boards["rotation_180"])["J_DISP1"]
    angle_delta = (j180.GetOrientationDegrees() - j0.GetOrientationDegrees()) % 360
    cross_checks["j_disp1_pose_differs_by_180_degrees"] = (
        vec(j0.GetPosition()) == vec(j180.GetPosition()) and angle_delta == 180.0 and j0.GetLayerName() == j180.GetLayerName() == "Bottom Layer"
    )
    all_checks = [value for item in variants.values() for value in item["checks"].values()] + list(cross_checks.values())
    passed = sum(bool(value) for value in all_checks)
    status = "PASS_SYNC_VERIFIED" if passed == len(all_checks) else "FAIL"
    report = {
        "schema_version": 1,
        "status": status,
        "checks_passed": passed,
        "checks_total": len(all_checks),
        "selection_frozen": False,
        "routed": False,
        "drc_passed": False,
        "production_ready": False,
        "sources": {
            "reference_board": str(SOURCE.relative_to(REPO)),
            "reference_board_sha256": sha256(SOURCE),
            "netlist": str(NETLIST.relative_to(REPO)),
            "netlist_sha256": sha256(NETLIST),
            "placement": str(PLACEMENT.relative_to(REPO)),
            "placement_sha256": sha256(PLACEMENT),
            "generation_report": str(GENERATION.relative_to(REPO)),
            "generation_report_sha256": sha256(GENERATION),
        },
        "cross_variant_checks": cross_checks,
        "variants": variants,
        "limits": [
            "This verifies synchronization and preserved mechanics, not routing quality.",
            "DRC remains failing with 49 unconnected items per variant.",
            "FPC contact face, Pin 1, latch access and natural cable exit remain physically unverified.",
        ],
    }
    OUTPUT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"status": status, "checks_passed": passed, "checks_total": len(all_checks), "report": str(OUTPUT)}))
    return 0 if status == "PASS_SYNC_VERIFIED" else 1


if __name__ == "__main__":
    raise SystemExit(main())
