#!/usr/bin/env python3
"""Generate two unrouted, non-frozen L1 PCB synchronization candidates.

The protected working-base board is never modified.  Both outputs contain the
real 94-component schematic endpoint set, a provisional dual-contact FPC
connector with opposite Pin-1 rotations, the reviewed Power/Lock switch, and
the localized USB/CC placement move.  New/changed nets remain deliberately
unrouted; this script does not claim DRC or production readiness.
"""

from __future__ import annotations

import hashlib
import json
import re
import tempfile
import uuid
import xml.etree.ElementTree as ET
from collections import defaultdict
from pathlib import Path

import pcbnew


HERE = Path(__file__).resolve().parent
REPO = HERE.parents[3]
SOURCE_BOARD = REPO / "hardware/cadenza/working-base/Cadenza-reference-ESP32-S3-game-console-original-v2-20260722.kicad_pcb"
NETLIST = REPO / "hardware/cadenza/derived/l1-candidate/candidate.netlist.xml"
PLACEMENT = REPO / "hardware/cadenza/derived/l1-pcb-placement/placement-feasibility.json"
REPORT = HERE / "generation-report.json"
UUID_NAMESPACE = uuid.UUID("9b50b648-9c34-5df5-9d27-f8c94299740c")

KICAD_FOOTPRINTS = Path("/Applications/KiCad.app/Contents/SharedSupport/footprints")
DISPLAY_LIBRARY = REPO / "hardware/cadenza/libraries/display-fpc-variants/Cadenza_Display_FPC_Variants.pretty"
POWER_LIBRARY = REPO / "hardware/cadenza/libraries/power-lock-switch-candidate/GT_TC020A_H035_L1.pretty"

DELETED_REFS = {"FPC1", "U6", "C20", "C21", "Q2", "R13", "R14", "SW1", "KEY1"}
DISPLAY_REFS = {
    "J_DISP1", "C_DISP1", "C_DISP2", "C_DISP3", "TP_DISP_5V", "TP_DISP_GND",
    "TP_DISP_SCLK", "TP_DISP_SI", "TP_DISP_SCS", "TP_DISP_EXTCOMIN", "TP_DISP_ENABLE",
}
POWER_REFS = {
    "D_PWR1", "SW_PWR1", "R_PWR1", "R_PWR2", "Q_PWR1", "R_PWR3", "R_PWR4", "R_PWR5",
    "R_PWR6", "TP_PWR_KEY", "TP_PWR_BUTTON", "TP_PWR_SENSE", "TP_PWR_FORCE", "TP_PWR_VOUT",
    "TP_PWR_5V",
}
NEW_REFS = DISPLAY_REFS | POWER_REFS

# Old imported net names whose copper cannot be trusted after the functional
# change or USB move.  All tracks/vias on these old nets are removed and must
# be rerouted in the next stage.
REROUTE_OLD_NETS = {
    "$1N34554", "$1N34556", "$1N34595", "D+", "D-", "VBUS", "GPIO6", "GPIO12", "GPIO14",
    "GPIO18", "GPIO39", "GPIO47", "GPIO48",
}

PLACEMENTS = {
    # mechanical lower-left board coordinates, F.Cu unless overridden
    "C_DISP1": (59.0, 18.0, 0.0, "F.Cu"),
    "C_DISP2": (58.0, 12.0, 0.0, "F.Cu"),
    "C_DISP3": (62.0, 12.0, 0.0, "F.Cu"),
    "TP_DISP_5V": (28.0, 28.0, 0.0, "F.Cu"),
    "TP_DISP_GND": (32.0, 28.0, 0.0, "F.Cu"),
    "TP_DISP_SCLK": (36.0, 28.0, 0.0, "F.Cu"),
    "TP_DISP_SI": (38.0, 32.0, 0.0, "F.Cu"),
    "TP_DISP_SCS": (44.0, 28.0, 0.0, "F.Cu"),
    "TP_DISP_EXTCOMIN": (48.0, 28.0, 0.0, "F.Cu"),
    "TP_DISP_ENABLE": (24.0, 28.0, 0.0, "F.Cu"),
    "D_PWR1": (120.0, 26.0, 0.0, "F.Cu"),
    "SW_PWR1": (128.85, 18.0, 90.0, "F.Cu"),
    "R_PWR1": (52.0, 39.5, 0.0, "F.Cu"),
    "R_PWR2": (48.0, 40.0, 0.0, "F.Cu"),
    "Q_PWR1": (120.0, 30.0, 0.0, "F.Cu"),
    "R_PWR3": (114.0, 29.0, 0.0, "F.Cu"),
    "R_PWR4": (114.0, 32.5, 0.0, "F.Cu"),
    "R_PWR5": (116.0, 26.0, 0.0, "F.Cu"),
    "R_PWR6": (114.0, 40.0, 0.0, "F.Cu"),
    "TP_PWR_KEY": (100.0, 10.0, 0.0, "F.Cu"),
    "TP_PWR_BUTTON": (104.0, 10.0, 0.0, "F.Cu"),
    "TP_PWR_SENSE": (108.0, 10.0, 0.0, "F.Cu"),
    "TP_PWR_FORCE": (112.0, 13.0, 0.0, "F.Cu"),
    "TP_PWR_VOUT": (116.0, 10.0, 0.0, "F.Cu"),
    "TP_PWR_5V": (120.0, 10.0, 0.0, "F.Cu"),
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def parse_netlist() -> tuple[dict[str, dict[str, str | None]], dict[tuple[str, str], str], dict[str, list[tuple[str, str]]]]:
    root = ET.parse(NETLIST).getroot()
    components: dict[str, dict[str, str | None]] = {}
    for item in root.findall("./components/comp"):
        components[item.attrib["ref"]] = {
            "value": item.findtext("value") or "",
            "footprint": item.findtext("footprint"),
        }
    pad_to_net: dict[tuple[str, str], str] = {}
    net_to_pads: dict[str, list[tuple[str, str]]] = {}
    for net in root.findall("./nets/net"):
        name = net.attrib["name"]
        nodes = [(node.attrib["ref"], node.attrib["pin"]) for node in net.findall("node")]
        net_to_pads[name] = nodes
        for node in nodes:
            if node in pad_to_net:
                raise ValueError(f"duplicate netlist endpoint {node}")
            pad_to_net[node] = name
    return components, pad_to_net, net_to_pads


def footprint_source(ref: str, declared: str | None) -> tuple[Path, str]:
    if ref == "J_DISP1":
        return DISPLAY_LIBRARY, "Hirose_FH34SRJ-10S-0.5SH_50_1x10_P0.50mm_DualContact"
    if ref == "SW_PWR1":
        return POWER_LIBRARY, "GT-TC020A-H035-L1"
    if not declared or ":" not in declared:
        raise ValueError(f"missing footprint for {ref}: {declared}")
    library, name = declared.split(":", 1)
    return KICAD_FOOTPRINTS / f"{library}.pretty", name


def mechanical_to_board(x_mm: float, y_mm: float, xmin: float, ymax: float) -> pcbnew.VECTOR2I:
    return pcbnew.VECTOR2I(pcbnew.FromMM(xmin + x_mm), pcbnew.FromMM(ymax - y_mm))


def top_level_blocks(text: str) -> list[tuple[int, int, str, str]]:
    """Return direct-child S-expression spans without rewriting other bytes."""
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
            continue
        if char == "(":
            if depth == 1:
                start = index
            depth += 1
        elif char == ")":
            if depth == 2 and start is not None:
                block = text[start:index + 1]
                match = re.match(r"\(\s*([^\s()]+)", block)
                blocks.append((start, index + 1, match.group(1) if match else "", block))
                start = None
            depth -= 1
            if depth < 0:
                raise ValueError("unbalanced board S-expression")
    if depth != 0 or in_string:
        raise ValueError("unterminated board S-expression")
    return blocks


def build_pruned_source(remove_old_net_names: set[str], destination: Path) -> dict[str, int]:
    text = SOURCE_BOARD.read_text(encoding="utf-8")
    spans = []
    removed_footprints = 0
    removed_copper = 0
    removed_teardrop_zones = 0
    for start, end, head, block in top_level_blocks(text):
        remove = False
        if head == "footprint":
            match = re.search(r'\(property\s+"Reference"\s+"([^"]+)"', block)
            if match and match.group(1) in DELETED_REFS:
                remove = True
                removed_footprints += 1
        elif head in {"segment", "via", "arc"}:
            match = re.search(r'\(net\s+"([^"]*)"\)', block)
            if match and match.group(1) in remove_old_net_names:
                remove = True
                removed_copper += 1
        elif head == "zone" and "(teardrop" in block:
            match = re.search(r'\(net\s+"([^"]*)"\)', block)
            if match and match.group(1) in remove_old_net_names:
                remove = True
                removed_teardrop_zones += 1
        if remove:
            spans.append((start, end))
    if removed_footprints != 9:
        raise RuntimeError(f"text prune expected 9 footprints, got {removed_footprints}")
    for start, end in reversed(spans):
        text = text[:start] + text[end:]
    destination.write_text(text, encoding="utf-8")
    return {
        "removed_footprints": removed_footprints,
        "removed_track_via_arc_items": removed_copper,
        "removed_teardrop_zones": removed_teardrop_zones,
        "removed_old_net_count": len(remove_old_net_names),
    }


def normalize_generated_board(path: Path) -> None:
    """Stabilize new-item UUIDs and footprint order without touching geometry."""
    text = path.read_text(encoding="utf-8")
    blocks = top_level_blocks(text)
    footprints = []
    for start, end, head, block in blocks:
        if head != "footprint":
            continue
        match = re.search(r'\(property\s+"Reference"\s+"([^"]+)"', block)
        if not match:
            raise RuntimeError("footprint without Reference property")
        ref = match.group(1)
        if ref in NEW_REFS:
            index = 0

            def stable_uuid(_match):
                nonlocal index
                value = uuid.uuid5(UUID_NAMESPACE, f"{ref}:{index}")
                index += 1
                return f'(uuid "{value}")'

            block = re.sub(r'\(uuid\s+"[0-9a-fA-F-]+"\)', stable_uuid, block)
        footprints.append((ref, start, end, block))
    if len(footprints) != 94:
        raise RuntimeError(f"normalization expected 94 footprints, got {len(footprints)}")
    first = min(item[1] for item in footprints)
    last = max(item[2] for item in footprints)
    footprint_spans = {(item[1], item[2]) for item in footprints}
    non_footprint_content = []
    cursor = first
    for _, start, end, _ in sorted(footprints, key=lambda item: item[1]):
        non_footprint_content.append(text[cursor:start])
        cursor = end
    non_footprint_content.append(text[cursor:last])
    if any(chunk.strip() for chunk in non_footprint_content):
        raise RuntimeError("footprints are not a contiguous top-level board section")
    ordered = "\n\t".join(item[3] for item in sorted(footprints, key=lambda item: item[0]))
    text = text[:first] + ordered + text[last:]

    # Track/via containers are also iterated nondeterministically after a
    # derived routing stage adds new objects.  Sort the one contiguous copper
    # item section by stable inherited/generated UUID.
    blocks = top_level_blocks(text)
    copper_items = []
    for start, end, head, block in blocks:
        if head not in {"segment", "via", "arc"}:
            continue
        match = re.search(r'\(uuid\s+"([0-9a-fA-F-]+)"\)', block)
        if not match:
            raise RuntimeError(f"{head} without UUID")
        copper_items.append((match.group(1), start, end, block))
    if copper_items:
        first = min(item[1] for item in copper_items)
        last = max(item[2] for item in copper_items)
        gaps = []
        cursor = first
        for _, start, end, _ in sorted(copper_items, key=lambda item: item[1]):
            gaps.append(text[cursor:start])
            cursor = end
        gaps.append(text[cursor:last])
        if any(chunk.strip() for chunk in gaps):
            raise RuntimeError("track/via/arc items are not one contiguous top-level section")
        ordered = "\n\t".join(item[3] for item in sorted(copper_items, key=lambda item: item[0]))
        text = text[:first] + ordered + text[last:]

    # pcbnew serializes the imported teardrop zones in container iteration
    # order, which is not stable between processes.  KiCad 10's text format
    # does not give these generated teardrops UUIDs, so use a digest of each
    # complete block as the stable ordering key.  This changes ordering only;
    # zone geometry and net assignments remain byte-for-byte intact.
    blocks = top_level_blocks(text)
    zones = []
    for start, end, head, block in blocks:
        if head != "zone":
            continue
        # Filled polygons legitimately differ between the two FPC rotations
        # and can vary in serialization order.  The zone definition before
        # the first fill block is the stable identity (net/layer/outline).
        stable_definition = block.split("\n\t\t(filled_polygon", 1)[0]
        digest = hashlib.sha256(stable_definition.encode("utf-8")).hexdigest()
        zones.append((digest, start, end, block))
    if zones:
        first = min(item[1] for item in zones)
        last = max(item[2] for item in zones)
        gaps = []
        cursor = first
        for _, start, end, _ in sorted(zones, key=lambda item: item[1]):
            gaps.append(text[cursor:start])
            cursor = end
        gaps.append(text[cursor:last])
        if any(chunk.strip() for chunk in gaps):
            raise RuntimeError("zones are not a contiguous top-level board section")
        ordered = "\n\t".join(item[3] for item in sorted(zones, key=lambda item: item[0]))
        text = text[:first] + ordered + text[last:]
    path.write_text(text, encoding="utf-8")


def create_target_nets(board: pcbnew.BOARD, names: list[str]) -> dict[str, pcbnew.NETINFO_ITEM]:
    existing = {str(name): net for name, net in board.GetNetsByName().items() if str(name)}
    result: dict[str, pcbnew.NETINFO_ITEM] = {}
    for name in names:
        if name in existing:
            result[name] = existing[name]
        else:
            net = pcbnew.NETINFO_ITEM(board, name)
            board.Add(net)
            result[name] = net
    return result


def build_variant(name: str, j_rotation_deg: float, components, pad_to_net, net_to_pads, placement_data, pruned_source: Path, removed_copper_count: int) -> dict[str, object]:
    board = pcbnew.LoadBoard(str(pruned_source))
    edge = board.GetBoardEdgesBoundingBox()
    xmin = pcbnew.ToMM(edge.GetX())
    ymax = pcbnew.ToMM(edge.GetBottom())

    old_targets: dict[str, set[str]] = defaultdict(set)
    for footprint in board.GetFootprints():
        if footprint.GetReference() in DELETED_REFS:
            continue
        for pad in footprint.Pads():
            target = pad_to_net.get((footprint.GetReference(), pad.GetNumber()))
            if pad.GetNetname() and target:
                old_targets[pad.GetNetname()].add(target)

    usb_move = placement_data["candidate"]["usb1_move"]
    cc_move = placement_data["candidate"]["usb_cc_resistor_moves"]
    by_ref = {footprint.GetReference(): footprint for footprint in board.GetFootprints()}
    by_ref["USB1"].SetPosition(mechanical_to_board(*usb_move["new_position_xy_mm"], xmin, ymax))
    for ref in ("R1", "R2"):
        old_y = placement_data["preserved"].get("unused", None)
        # Preserve each resistor's inherited Y and rotation, changing X only.
        current = by_ref[ref].GetPosition()
        by_ref[ref].SetPosition(pcbnew.VECTOR2I(pcbnew.FromMM(xmin + cc_move["target_x_mm"]), current.y))

    for ref in sorted(NEW_REFS):
        library, footprint_name = footprint_source(ref, components[ref]["footprint"])
        footprint = pcbnew.FootprintLoad(str(library), footprint_name)
        if footprint is None:
            raise RuntimeError(f"cannot load footprint {library}:{footprint_name}")
        footprint.SetReference(ref)
        footprint.SetValue(str(components[ref]["value"]))
        if ref == "J_DISP1":
            x_mm, y_mm, rotation, layer = (65.0, 5.3, j_rotation_deg, "B.Cu")
        else:
            x_mm, y_mm, rotation, layer = PLACEMENTS[ref]
        footprint.SetPosition(mechanical_to_board(x_mm, y_mm, xmin, ymax))
        footprint.SetOrientationDegrees(rotation)
        board.Add(footprint)
        if layer == "B.Cu":
            footprint.SetLayerAndFlip(pcbnew.B_Cu)
        if ref.startswith("TP_") or ref == "R_PWR6":
            footprint.SetAttributes(
                footprint.GetAttributes() | pcbnew.FP_EXCLUDE_FROM_BOM | pcbnew.FP_EXCLUDE_FROM_POS_FILES
            )

    target_nets = create_target_nets(board, sorted(net_to_pads))

    remapped_tracks = 0
    for track in list(board.GetTracks()):
        old_name = track.GetNetname()
        targets = old_targets.get(old_name, set())
        if len(targets) != 1:
            raise RuntimeError(f"pruned copper retained ambiguous/orphan net {old_name}: {targets}")
        track.SetNet(target_nets[next(iter(targets))])
        remapped_tracks += 1

    remapped_zones = 0
    for zone in board.Zones():
        old_name = zone.GetNetname()
        targets = old_targets.get(old_name, set())
        if len(targets) == 1:
            zone.SetNet(target_nets[next(iter(targets))])
            remapped_zones += 1
        else:
            zone.SetNetCode(0)

    missing_board_pads = []
    extra_board_pads = []
    board_endpoints = set()
    for footprint in board.GetFootprints():
        ref = footprint.GetReference()
        for pad in footprint.Pads():
            endpoint = (ref, pad.GetNumber())
            target = pad_to_net.get(endpoint)
            if target:
                pad.SetNet(target_nets[target])
                board_endpoints.add(endpoint)
            else:
                pad.SetNetCode(0)
                if pad.GetNumber():
                    extra_board_pads.append(endpoint)
    for endpoint in sorted(pad_to_net):
        if endpoint not in board_endpoints:
            missing_board_pads.append(endpoint)

    board.BuildListOfNets()
    board.SynchronizeNetsAndNetClasses(True)

    # Imported fills still describe the reference component geometry.  Refill
    # after the L1 pads and net assignments are final so pours create real
    # clearances instead of preserving obsolete accidental connectivity.
    regular_zones = pcbnew.ZONES()
    for zone in board.Zones():
        if not zone.IsTeardropArea():
            regular_zones.push_back(zone)
    zone_filler = pcbnew.ZONE_FILLER(board)
    zone_filler.Fill(regular_zones)

    output = HERE / f"Cadenza-L1-{name}.kicad_pcb"
    pcbnew.SaveBoard(str(output), board)
    normalize_generated_board(output)
    reloaded = pcbnew.LoadBoard(str(output))
    actual_nets = {str(net) for net in reloaded.GetNetsByName().keys() if str(net)}
    return {
        "name": name,
        "j_disp1_rotation_deg_before_bottom_flip": j_rotation_deg,
        "output": str(output.relative_to(REPO)),
        "sha256": sha256(output),
        "footprints": len(board.GetFootprints()),
        "nets": len(actual_nets),
        "expected_nets": len(net_to_pads),
        "net_names_exact": actual_nets == set(net_to_pads),
        "tracks_remaining": len(reloaded.GetTracks()),
        "tracks_removed_for_changed_or_orphan_nets": removed_copper_count,
        "tracks_remapped": remapped_tracks,
        "zones_remapped": remapped_zones,
        "zones_refilled_after_l1_sync": True,
        "regular_zones_refilled": regular_zones.size(),
        "missing_netlist_endpoints": [list(item) for item in missing_board_pads],
        "board_pads_without_netlist_endpoint": [list(item) for item in sorted(extra_board_pads)],
        "deterministic_uuid_and_footprint_order": True,
    }


def main() -> int:
    components, pad_to_net, net_to_pads = parse_netlist()
    placement_data = json.loads(PLACEMENT.read_text(encoding="utf-8"))
    if placement_data["status"] != "PASS_PLACEMENT_CANDIDATE":
        raise RuntimeError("placement feasibility gate is not passing")
    if set(components) != ({ref for ref in components}):
        raise RuntimeError("component reference set malformed")
    if len(components) != 94 or set(NEW_REFS) - set(components):
        raise RuntimeError("unexpected candidate component set")

    original = pcbnew.LoadBoard(str(SOURCE_BOARD))
    old_targets: dict[str, set[str]] = defaultdict(set)
    for footprint in original.GetFootprints():
        if footprint.GetReference() in DELETED_REFS:
            continue
        for pad in footprint.Pads():
            target = pad_to_net.get((footprint.GetReference(), pad.GetNumber()))
            if pad.GetNetname() and target:
                old_targets[pad.GetNetname()].add(target)
    remove_old_net_names = set(REROUTE_OLD_NETS) | {
        str(name) for name in original.GetNetsByName().keys()
        if str(name) and len(old_targets.get(str(name), set())) != 1
    }
    with tempfile.TemporaryDirectory(prefix="cadenza-l1-pcb-prune-") as temporary:
        pruned_source = Path(temporary) / "pruned-reference.kicad_pcb"
        prune_report = build_pruned_source(remove_old_net_names, pruned_source)
        variants = [
            build_variant("fpc-pin1-rotation-0", 0.0, components, pad_to_net, net_to_pads, placement_data, pruned_source, prune_report["removed_track_via_arc_items"]),
            build_variant("fpc-pin1-rotation-180", 180.0, components, pad_to_net, net_to_pads, placement_data, pruned_source, prune_report["removed_track_via_arc_items"]),
        ]
    status = "PASS_SYNC_CANDIDATE" if all(
        item["footprints"] == 94 and item["net_names_exact"] and not item["missing_netlist_endpoints"]
        for item in variants
    ) else "FAIL"
    report = {
        "schema_version": 1,
        "status": status,
        "selection_frozen": False,
        "production_ready": False,
        "routed": False,
        "drc_passed": False,
        "source": {
            "board": str(SOURCE_BOARD.relative_to(REPO)),
            "board_sha256": sha256(SOURCE_BOARD),
            "netlist": str(NETLIST.relative_to(REPO)),
            "netlist_sha256": sha256(NETLIST),
            "placement": str(PLACEMENT.relative_to(REPO)),
            "placement_sha256": sha256(PLACEMENT),
        },
        "policy": {
            "reference_board_modified_in_place": False,
            "deleted_refs": sorted(DELETED_REFS),
            "new_refs": sorted(NEW_REFS),
            "moved_reference_refs": ["USB1", "R1", "R2"],
            "j_disp1_footprint": "Hirose FH34SRJ dual-contact provisional",
            "j_disp1_pin1_frozen": False,
            "sw_pwr1_footprint": "GT-TC020A-H035-L1 candidate",
            "sw_pwr1_frozen": False,
        },
        "text_prune": prune_report,
        "variants": variants,
        "next_required": [
            "independently verify board endpoint topology against the real candidate netlist",
            "inspect footprint XY/Z and courtyard collisions",
            "route all changed display, Power/Lock and relocated USB nets",
            "remove shared-power copper stubs left by deleted display components",
            "run DRC/EMC/DFM and combined PCB-enclosure STEP checks",
            "freeze FPC Pin 1/contact/rotation and Power/Lock physical fit before fabrication",
        ],
    }
    REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"status": status, "variants": len(variants), "report": str(REPORT)}))
    return 0 if status == "PASS_SYNC_CANDIDATE" else 1


if __name__ == "__main__":
    raise SystemExit(main())
