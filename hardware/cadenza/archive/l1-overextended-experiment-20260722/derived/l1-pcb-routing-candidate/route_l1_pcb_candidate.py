#!/usr/bin/env python3
"""Add the reviewed local L1 routes to both PCB direction candidates.

This stage deliberately routes only topology-reviewed nets.  Long display, LED
and MCU-control routes remain unrouted until obstacle checks are added.  Outputs
are engineering candidates, never order data.
"""

from __future__ import annotations

import hashlib
import importlib.util
import json
import uuid
from pathlib import Path

import pcbnew


HERE = Path(__file__).resolve().parent
REPO = HERE.parents[3]
SOURCE_DIR = REPO / "hardware/cadenza/derived/l1-pcb-candidate"
REPORT = HERE / "routing-report.json"
UUID_NAMESPACE = uuid.UUID("5a9016e8-3126-5b31-bb3f-3db9a131cdea")
VARIANTS = {
    "rotation-0": SOURCE_DIR / "Cadenza-L1-fpc-pin1-rotation-0.kicad_pcb",
    "rotation-180": SOURCE_DIR / "Cadenza-L1-fpc-pin1-rotation-180.kicad_pcb",
}


def load_sync_normalizer():
    path = SOURCE_DIR / "generate_l1_pcb_candidate.py"
    spec = importlib.util.spec_from_file_location("cadenza_l1_sync_generator", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load synchronization normalizer from {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.normalize_generated_board


NORMALIZE_BOARD = load_sync_normalizer()


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def pad_point(board, ref: str, number: str) -> pcbnew.VECTOR2I:
    footprint = board.FindFootprintByReference(ref)
    if footprint is None:
        raise RuntimeError(f"missing footprint {ref}")
    pads = [pad for pad in footprint.Pads() if pad.GetNumber() == number]
    if len(pads) != 1:
        raise RuntimeError(f"expected one pad {ref}.{number}, got {len(pads)}")
    return pads[0].GetPosition()


def mechanical_point(board, x_mm: float, y_mm: float) -> pcbnew.VECTOR2I:
    edge = board.GetBoardEdgesBoundingBox()
    return pcbnew.VECTOR2I(
        edge.GetX() + pcbnew.FromMM(x_mm),
        edge.GetBottom() - pcbnew.FromMM(y_mm),
    )


def resolve_point(board, value) -> pcbnew.VECTOR2I:
    if value[0] == "pad":
        return pad_point(board, value[1], value[2])
    if value[0] == "xy":
        return mechanical_point(board, value[1], value[2])
    if value[0] == "abs":
        return pcbnew.VECTOR2I(pcbnew.FromMM(value[1]), pcbnew.FromMM(value[2]))
    raise ValueError(value)


def add_polyline(board, route_id: str, net_name: str, width_mm: float, points, uuid_replacements, layer=pcbnew.F_Cu) -> int:
    net = board.FindNet(net_name)
    if net is None:
        raise RuntimeError(f"missing net {net_name}")
    resolved = [resolve_point(board, point) for point in points]
    count = 0
    for index, (start, end) in enumerate(zip(resolved, resolved[1:])):
        if start == end:
            continue
        track = pcbnew.PCB_TRACK(board)
        track.SetStart(start)
        track.SetEnd(end)
        track.SetWidth(pcbnew.FromMM(width_mm))
        track.SetLayer(layer)
        track.SetNet(net)
        old_uuid = track.m_Uuid.AsString()
        stable_uuid = str(uuid.uuid5(UUID_NAMESPACE, f"{route_id}:{index}"))
        uuid_replacements[old_uuid] = stable_uuid
        board.Add(track)
        count += 1
    return count


def add_via(
    board, route_id: str, net_name: str, point, uuid_replacements,
    width_mm: float = 0.6, drill_mm: float = 0.3,
) -> int:
    net = board.FindNet(net_name)
    if net is None:
        raise RuntimeError(f"missing net {net_name}")
    via = pcbnew.PCB_VIA(board)
    via.SetPosition(resolve_point(board, point))
    via.SetWidth(pcbnew.FromMM(width_mm))
    via.SetDrill(pcbnew.FromMM(drill_mm))
    via.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
    via.SetNet(net)
    uuid_replacements[via.m_Uuid.AsString()] = str(uuid.uuid5(UUID_NAMESPACE, route_id))
    board.Add(via)
    return 1


def add_usb_cc2_route(board, uuid_replacements) -> int:
    net = "Net-(USB1-CC2)"
    input_via = ("xy", 82.827, 2.0)
    output_via = ("xy", 92.0, 3.736)
    count = add_polyline(
        board, "usb_cc2_top_in", net, 0.2,
        [("pad", "USB1", "B5"), input_via], uuid_replacements, pcbnew.F_Cu,
    )
    count += add_via(board, "usb_cc2_via_in", net, input_via, uuid_replacements)
    count += add_polyline(
        board, "usb_cc2_bottom", net, 0.2,
        [input_via, ("xy", 82.827, 3.736), output_via], uuid_replacements, pcbnew.B_Cu,
    )
    count += add_via(board, "usb_cc2_via_out", net, output_via, uuid_replacements)
    count += add_polyline(
        board, "usb_cc2_top_out", net, 0.2,
        [output_via, ("pad", "R2", "2")], uuid_replacements, pcbnew.F_Cu,
    )
    return count


def add_usb_cc1_route(board, uuid_replacements) -> int:
    net = "Net-(USB1-CC1)"
    input_via = ("xy", 79.827, 9.0)
    output_via = ("xy", 92.0, 9.0)
    count = add_polyline(
        board, "usb_cc1_top_in", net, 0.2,
        [("pad", "USB1", "A5"), input_via], uuid_replacements, pcbnew.F_Cu,
    )
    count += add_via(board, "usb_cc1_via_in", net, input_via, uuid_replacements)
    count += add_polyline(
        board, "usb_cc1_bottom", net, 0.2,
        [input_via, output_via], uuid_replacements, pcbnew.B_Cu,
    )
    count += add_via(board, "usb_cc1_via_out", net, output_via, uuid_replacements)
    count += add_polyline(
        board, "usb_cc1_top_out", net, 0.2,
        [output_via, ("pad", "R1", "1")], uuid_replacements, pcbnew.F_Cu,
    )
    return count


def add_usb_data_routes(board, uuid_replacements) -> int:
    """Route the short CH340C USB-FS pair and the reversible Type-C branches.

    USB1 uses plated through-hole signal pads.  The crossed A/B-side mapping is
    therefore resolved without extra vias: D+ joins A6/B6 on B.Cu while D-
    joins A7/B7 on F.Cu.  Both main legs leave the A row on F.Cu and run as a
    compact pair to U3.  Exact impedance remains a later stack-up calculation.
    """
    count = add_polyline(
        board, "usb_dp_main", "/D+", 0.25,
        [
            ("pad", "U3", "5"), ("xy", 85.2, 13.9415),
            ("xy", 84.2, 12.9415), ("xy", 84.2, 12.373),
            ("xy", 80.827, 9.0),
            ("pad", "USB1", "A6"),
        ],
        uuid_replacements, pcbnew.F_Cu,
    )
    count += add_polyline(
        board, "usb_dm_main", "/D-", 0.25,
        [
            ("pad", "U3", "6"), ("xy", 85.75, 12.6715),
            ("xy", 84.75, 11.6715), ("xy", 84.75, 11.423),
            ("xy", 81.827, 8.5),
            ("pad", "USB1", "A7"),
        ],
        uuid_replacements, pcbnew.F_Cu,
    )
    count += add_polyline(
        board, "usb_dp_reversible_branch", "/D+", 0.25,
        [("pad", "USB1", "A6"), ("pad", "USB1", "B6")],
        uuid_replacements, pcbnew.B_Cu,
    )
    count += add_polyline(
        board, "usb_dm_reversible_branch", "/D-", 0.25,
        [("pad", "USB1", "A7"), ("pad", "USB1", "B7")],
        uuid_replacements, pcbnew.F_Cu,
    )
    return count


def add_usb_vbus_route(board, uuid_replacements) -> int:
    """Translate the reference board's proven VBUS topology to shifted USB1.

    The reference used 0.508 mm copper but its old lower-edge trunk no longer
    clears the shifted connector, the new CC2 route and the Power/Lock testpoint.
    The four reversible-connector pads therefore converge on B.Cu above USB1,
    stay below the inherited horizontal signal corridor, and change to F.Cu in
    the clear lane between the Power/Lock testpoints before reaching U1/C2/R4.
    """
    net = "/VBUS"
    common_via = ("xy", 83.827, 7.852)
    layer_via = ("xy", 102.002, 7.852)
    count = add_polyline(
        board, "usb_vbus_right_pair", net, 0.508,
        [("pad", "USB1", "A9"), ("pad", "USB1", "B4")],
        uuid_replacements, pcbnew.F_Cu,
    )
    count += add_polyline(
        board, "usb_vbus_right_rise", net, 0.508,
        [("pad", "USB1", "A9"), common_via],
        uuid_replacements, pcbnew.F_Cu,
    )
    count += add_polyline(
        board, "usb_vbus_left_pair", net, 0.508,
        [("pad", "USB1", "A4"), ("pad", "USB1", "B9")],
        uuid_replacements, pcbnew.B_Cu,
    )
    count += add_polyline(
        board, "usb_vbus_left_rise", net, 0.508,
        [
            ("pad", "USB1", "A4"), ("xy", 78.827, 7.852),
            common_via,
        ],
        uuid_replacements, pcbnew.B_Cu,
    )
    count += add_via(
        board, "usb_vbus_common_via", net, common_via, uuid_replacements,
        width_mm=0.8, drill_mm=0.4,
    )
    count += add_polyline(
        board, "usb_vbus_bottom_trunk", net, 0.508,
        [common_via, layer_via],
        uuid_replacements, pcbnew.B_Cu,
    )
    count += add_via(
        board, "usb_vbus_layer_via", net, layer_via, uuid_replacements,
        width_mm=0.8, drill_mm=0.4,
    )
    count += add_polyline(
        board, "usb_vbus_top_trunk", net, 0.508,
        [
            layer_via, ("xy", 102.002, 27.852), ("pad", "R4", "2"),
            ("pad", "C2", "2"), ("pad", "U1", "1"),
        ],
        uuid_replacements, pcbnew.F_Cu,
    )
    return count


def add_power_sense_3v3_drop(board, uuid_replacements) -> int:
    """Connect R_PWR2.1 to the inherited 3V3M bottom-layer trunk below it."""
    via_point = ("xy", 47.0875, 41.9665)
    count = add_polyline(
        board, "pwr_sense_3v3_top", "3V3M", 0.508,
        [("pad", "R_PWR2", "1"), via_point], uuid_replacements, pcbnew.F_Cu,
    )
    count += add_via(
        board, "pwr_sense_3v3_via", "3V3M", via_point, uuid_replacements,
    )
    return count


DISPLAY_ROUTES = {
    "rotation-0": {
        "GPIO48": {
            "pad": "1", "via": (158.0, 128.5), "lane": (158.0, 108.0), "u4": "25",
            "bottom": [("pad", "J_DISP1", "1"), ("abs", 150.0, 133.5), ("abs", 150.0, 128.5), ("abs", 158.0, 128.5)],
        },
        "GPIO12": {"pad": "2", "via": (152.0, 132.5), "lane": (152.0, 108.0), "u4": "20"},
        "GPIO14": {"pad": "3", "via": (154.0, 131.5), "lane": (154.0, 108.0), "u4": "22"},
        "GPIO47": {"pad": "4", "via": (156.0, 130.5), "lane": (156.0, 108.0), "u4": "24"},
        "GPIO39": {"pad": "5", "via": (160.0, 129.5), "lane": (160.0, 100.0), "u4": "32"},
    },
    "rotation-180": {
        "GPIO48": {"pad": "1", "via": (158.0, 132.0), "lane": (158.0, 108.0), "u4": "25"},
        "GPIO12": {
            "pad": "2", "via": (152.0, 129.0), "lane": (152.0, 108.0), "u4": "20",
            "bottom": [("pad", "J_DISP1", "2"), ("abs", 157.2, 133.5), ("abs", 157.2, 129.0), ("abs", 152.0, 129.0)],
        },
        "GPIO14": {"pad": "3", "via": (154.0, 132.0), "lane": (154.0, 108.0), "u4": "22"},
        "GPIO47": {"pad": "4", "via": (156.0, 131.0), "lane": (156.0, 108.0), "u4": "24"},
        "GPIO39": {
            "pad": "5", "via": (160.0, 127.5), "lane": (160.0, 100.0), "u4": "32",
            "bottom": [("pad", "J_DISP1", "5"), ("abs", 152.5, 133.5), ("abs", 152.5, 127.5), ("abs", 160.0, 127.5)],
        },
    },
}


def add_display_signal_routes(board, variant_name: str, uuid_replacements) -> int:
    """Route the five Sharp digital signals; TP/cap branches remain separate."""
    count = 0
    for net_name, config in DISPLAY_ROUTES[variant_name].items():
        via_abs = ("abs", *config["via"])
        bottom = config.get(
            "bottom", [("pad", "J_DISP1", config["pad"]), via_abs],
        )
        count += add_polyline(
            board, f"display_{variant_name}_{net_name}_escape", net_name, 0.2,
            bottom, uuid_replacements, pcbnew.B_Cu,
        )
        count += add_via(
            board, f"display_{variant_name}_{net_name}_via", net_name, via_abs,
            uuid_replacements, width_mm=0.61, drill_mm=0.305,
        )
        count += add_polyline(
            board, f"display_{variant_name}_{net_name}_main", net_name, 0.254,
            [via_abs, ("abs", *config["lane"]), ("pad", "U4", config["u4"])],
            uuid_replacements, pcbnew.F_Cu,
        )
    return count


ROUTES = [
    # MOSFET gate network: force-off resistor to Q gate and gate pulldown.
    ("pwr_gate_main", "PWR_GATE", 0.25, [
        ("pad", "R_PWR3", "2"), ("xy", 116.0, 29.0),
        ("xy", 116.0, 30.0), ("pad", "Q_PWR1", "1"),
    ]),
    # Gate pulldown joins the reviewed gate trunk from the left side of R_PWR4;
    # the path stays away from R_PWR4.2 so its GND thermal remains accessible.
    ("pwr_gate_pulldown", "PWR_GATE", 0.25, [
        ("pad", "R_PWR4", "1"), ("xy", 113.0875, 31.0),
        ("xy", 116.0, 31.0), ("xy", 116.0, 30.0),
    ]),
    # Button node local branch between diode and bias resistor.
    ("pwr_button_local", "PWR_BUTTON_NODE", 0.25, [
        ("pad", "R_PWR5", "2"), ("pad", "D_PWR1", "3"),
    ]),
    # IP5306 KEY local branch across the bias resistor, diode and MOSFET drain.
    ("pwr_key_local_a", "PWR_IP5306_KEY", 0.25, [
        ("pad", "R_PWR5", "1"), ("xy", 113.5, 26.0),
        ("xy", 113.5, 23.0), ("xy", 122.5, 23.0),
        ("xy", 122.5, 26.95), ("pad", "D_PWR1", "1"),
    ]),
    ("pwr_key_local_b", "PWR_IP5306_KEY", 0.25, [
        ("pad", "D_PWR1", "1"), ("xy", 120.0, 28.0), ("pad", "Q_PWR1", "3"),
    ]),
]


def build_variant(name: str, source: Path) -> dict[str, object]:
    board = pcbnew.LoadBoard(str(source))
    before = len(board.GetTracks())
    added = 0
    uuid_replacements = {}
    for route in ROUTES:
        added += add_polyline(board, *route, uuid_replacements)
    added += add_usb_cc1_route(board, uuid_replacements)
    added += add_usb_cc2_route(board, uuid_replacements)
    added += add_usb_data_routes(board, uuid_replacements)
    added += add_usb_vbus_route(board, uuid_replacements)
    added += add_power_sense_3v3_drop(board, uuid_replacements)
    added += add_display_signal_routes(board, name, uuid_replacements)
    regular_zones = pcbnew.ZONES()
    for zone in board.Zones():
        if not zone.IsTeardropArea():
            regular_zones.push_back(zone)
    pcbnew.ZONE_FILLER(board).Fill(regular_zones)
    output = HERE / f"Cadenza-L1-{name}-local-routing.kicad_pcb"
    pcbnew.SaveBoard(str(output), board)
    text = output.read_text(encoding="utf-8")
    for old_uuid, stable_uuid in uuid_replacements.items():
        old = f'(uuid "{old_uuid}")'
        if text.count(old) != 1:
            raise RuntimeError(f"expected one generated route UUID {old_uuid}")
        text = text.replace(old, f'(uuid "{stable_uuid}")')
    output.write_text(text, encoding="utf-8")
    NORMALIZE_BOARD(output)
    reloaded = pcbnew.LoadBoard(str(output))
    return {
        "name": name,
        "source": str(source.relative_to(REPO)),
        "source_sha256": sha256(source),
        "output": str(output.relative_to(REPO)),
        "output_sha256": sha256(output),
        "tracks_before": before,
        "tracks_added": added,
        "tracks_after": len(reloaded.GetTracks()),
        "regular_zones_refilled": regular_zones.size(),
    }


def main() -> int:
    HERE.mkdir(parents=True, exist_ok=True)
    variants = [build_variant(name, source) for name, source in VARIANTS.items()]
    report = {
        "schema_version": 1,
        "status": "PASS_LOCAL_ROUTING_GENERATED",
        "scope": "reviewed local Power/Lock, complete USB and five Sharp digital signal routes",
        "selection_frozen": False,
        "routing_complete": False,
        "drc_passed": False,
        "production_ready": False,
        "route_count": len(ROUTES) + 13,
        "variants": variants,
        "pending": [
            "DRC-check every added segment and revise any collision",
            "calculate and document USB D+/D- impedance after the JLC stack-up is selected",
            "review the inherited 0.508 mm USB VBUS width against the final copper stack-up and charge-current limit",
            "route Sharp +5V/GND and display capacitor/testpoint branches",
            "route remaining MCU Power/Lock control, LED and all testpoints",
            "route the high-current U1 VOUT to R_PWR6 bridge with explicit copper-width and obstacle review",
            "refill zones, verify connectivity, then run EMC/DFM",
        ],
    }
    REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"status": report["status"], "variants": len(variants), "report": str(REPORT)}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
