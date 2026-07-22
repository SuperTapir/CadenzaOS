#!/usr/bin/env python3
"""Verify that the first local routing stage is a controlled PCB delta."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path

import pcbnew


HERE = Path(__file__).resolve().parent
REPO = HERE.parents[3]
ROUTING_REPORT = HERE / "routing-report.json"
OUTPUT = HERE / "verification-report.json"
VARIANTS = {
    "rotation-0": {
        "source": REPO / "hardware/cadenza/derived/l1-pcb-candidate/Cadenza-L1-fpc-pin1-rotation-0.kicad_pcb",
        "output": HERE / "Cadenza-L1-rotation-0-local-routing.kicad_pcb",
        "source_drc": REPO / "hardware/cadenza/derived/l1-pcb-candidate/drc-rotation-0.json",
        "output_drc": HERE / "drc-local-rotation-0.json",
    },
    "rotation-180": {
        "source": REPO / "hardware/cadenza/derived/l1-pcb-candidate/Cadenza-L1-fpc-pin1-rotation-180.kicad_pcb",
        "output": HERE / "Cadenza-L1-rotation-180-local-routing.kicad_pcb",
        "source_drc": REPO / "hardware/cadenza/derived/l1-pcb-candidate/drc-rotation-180.json",
        "output_drc": HERE / "drc-local-rotation-180.json",
    },
}
ALLOWED_NEW_ROUTE_NETS = {
    "PWR_GATE", "PWR_BUTTON_NODE", "PWR_IP5306_KEY",
    "Net-(USB1-CC1)", "Net-(USB1-CC2)", "/D+", "/D-", "/VBUS", "3V3M",
    "GPIO48", "GPIO12", "GPIO14", "GPIO47", "GPIO39",
}
EXPECTED_ADDED_SEGMENTS = {"rotation-0": 73, "rotation-180": 75}
HIGH_RISK_DRC_TYPES = {
    "shorting_items", "clearance", "courtyards_overlap", "hole_clearance",
    "starved_thermal", "track_dangling", "copper_edge_clearance",
    "tracks_crossing",
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def track_signature(track) -> tuple:
    width = track.GetWidth(pcbnew.F_Cu) if isinstance(track, pcbnew.PCB_VIA) else track.GetWidth()
    return (
        track.GetNetname(), track.GetLayerName(),
        track.GetStart().x, track.GetStart().y,
        track.GetEnd().x, track.GetEnd().y,
        width, type(track).__name__,
    )


def track_map(board) -> dict[str, tuple]:
    result = {}
    for track in board.GetTracks():
        item_uuid = track.m_Uuid.AsString()
        if item_uuid in result:
            raise RuntimeError(f"duplicate track/via UUID {item_uuid}")
        result[item_uuid] = track_signature(track)
    return result


def drc_signature(item: dict) -> tuple:
    return (
        item["type"],
        tuple(sorted(part.get("description", "") for part in item.get("items", []))),
    )


def audit_variant(name: str, paths: dict[str, Path], expected: dict) -> dict:
    source = pcbnew.LoadBoard(str(paths["source"]))
    output = pcbnew.LoadBoard(str(paths["output"]))
    source_tracks = track_map(source)
    output_tracks = track_map(output)
    changed_source = sorted(
        item_uuid for item_uuid, signature in source_tracks.items()
        if output_tracks.get(item_uuid) != signature
    )
    added_uuids = sorted(set(output_tracks) - set(source_tracks))
    added_nets = sorted({output_tracks[item_uuid][0] for item_uuid in added_uuids})
    added_lengths_mm = {}
    for item_uuid in added_uuids:
        net_name = output_tracks[item_uuid][0]
        track = next(track for track in output.GetTracks() if track.m_Uuid.AsString() == item_uuid)
        added_lengths_mm[net_name] = added_lengths_mm.get(net_name, 0.0) + pcbnew.ToMM(track.GetLength())
    usb_data_skew_mm = abs(added_lengths_mm.get("/D+", 0.0) - added_lengths_mm.get("/D-", 0.0))
    source_drc = json.loads(paths["source_drc"].read_text(encoding="utf-8"))
    output_drc = json.loads(paths["output_drc"].read_text(encoding="utf-8"))
    source_high_risk = {
        drc_signature(item) for item in source_drc["violations"]
        if item["type"] in HIGH_RISK_DRC_TYPES
    }
    output_high_risk = {
        drc_signature(item) for item in output_drc["violations"]
        if item["type"] in HIGH_RISK_DRC_TYPES
    }
    new_high_risk = sorted(output_high_risk - source_high_risk, key=repr)
    added_route_high_risk = [
        drc_signature(item) for item in output_drc["violations"]
        if item["type"] in HIGH_RISK_DRC_TYPES
        and any(part.get("uuid") in added_uuids for part in item.get("items", []))
    ]
    courtyard_items = [
        sorted(part["description"] for part in item.get("items", []))
        for item in output_drc["violations"] if item["type"] == "courtyards_overlap"
    ]
    footprint_refs = {footprint.GetReference() for footprint in output.GetFootprints()}
    named_nets = {str(value) for value in output.GetNetsByName().keys() if str(value)}
    checks = {
        "source_hash_matches_routing_report": sha256(paths["source"]) == expected["source_sha256"],
        "output_hash_matches_routing_report": sha256(paths["output"]) == expected["output_sha256"],
        "footprint_count_and_refs_preserved": len(footprint_refs) == 94 and footprint_refs == {
            footprint.GetReference() for footprint in source.GetFootprints()
        },
        "named_net_set_preserved": named_nets == {
            str(value) for value in source.GetNetsByName().keys() if str(value)
        } and len(named_nets) == 97,
        "all_source_track_and_via_items_preserved": not changed_source and set(source_tracks) <= set(output_tracks),
        "track_delta_count_exact": len(added_uuids) == expected["tracks_added"] == EXPECTED_ADDED_SEGMENTS[name],
        "only_reviewed_local_route_nets_added": set(added_nets) == ALLOWED_NEW_ROUTE_NETS,
        "added_routes_have_no_high_risk_drc": not added_route_high_risk,
        "no_cross_net_shorts": not any(item["type"] == "shorting_items" for item in output_drc["violations"]),
        "only_inherited_l2_c1_courtyard_overlap": courtyard_items == [["封装 C1", "封装 L2"]],
        "usb_data_pair_added_on_both_copper_layers": {
            output_tracks[item_uuid][1]
            for item_uuid in added_uuids
            if output_tracks[item_uuid][0] in {"/D+", "/D-"}
        } == {"Top Layer", "Bottom Layer"},
        "usb_data_pair_absolute_skew_below_2_5mm": usb_data_skew_mm < 2.5,
        "usb_vbus_uses_inherited_0_508mm_width": {
            round(pcbnew.ToMM(next(track for track in output.GetTracks() if track.m_Uuid.AsString() == item_uuid).GetWidth()), 3)
            for item_uuid in added_uuids
            if output_tracks[item_uuid][0] == "/VBUS"
        } <= {0.508, 0.8},
        "unconnected_reduced_by_exactly_twenty_three": len(source_drc["unconnected_items"]) == 56
        and len(output_drc["unconnected_items"]) == 33,
    }
    return {
        "source": str(paths["source"].relative_to(REPO)),
        "output": str(paths["output"].relative_to(REPO)),
        "source_sha256": sha256(paths["source"]),
        "output_sha256": sha256(paths["output"]),
        "source_tracks": len(source_tracks),
        "output_tracks": len(output_tracks),
        "added_track_uuids": added_uuids,
        "added_route_nets": added_nets,
        "added_route_lengths_mm": {key: round(value, 4) for key, value in sorted(added_lengths_mm.items())},
        "usb_data_skew_mm": round(usb_data_skew_mm, 4),
        "changed_source_track_uuids": changed_source,
        "source_unconnected": len(source_drc["unconnected_items"]),
        "output_unconnected": len(output_drc["unconnected_items"]),
        "new_high_risk_drc_signatures": new_high_risk,
        "added_route_high_risk_drc": added_route_high_risk,
        "checks": checks,
    }


def main() -> int:
    routing_report = json.loads(ROUTING_REPORT.read_text(encoding="utf-8"))
    expected = {item["name"]: item for item in routing_report["variants"]}
    variants = {
        name: audit_variant(name, paths, expected[name])
        for name, paths in VARIANTS.items()
    }
    checks = [value for item in variants.values() for value in item["checks"].values()]
    status = "PASS_LOCAL_ROUTING_VERIFIED" if all(checks) else "FAIL"
    report = {
        "schema_version": 1,
        "status": status,
        "checks_passed": sum(bool(value) for value in checks),
        "checks_total": len(checks),
        "scope": "controlled local Power/Lock and complete USB connector route delta",
        "routing_complete": False,
        "drc_passed": False,
        "production_ready": False,
        "selection_frozen": False,
        "variants": variants,
        "limits": routing_report["pending"],
    }
    OUTPUT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"status": status, "checks_passed": report["checks_passed"], "checks_total": report["checks_total"], "report": str(OUTPUT)}))
    return 0 if status == "PASS_LOCAL_ROUTING_VERIFIED" else 1


if __name__ == "__main__":
    raise SystemExit(main())
