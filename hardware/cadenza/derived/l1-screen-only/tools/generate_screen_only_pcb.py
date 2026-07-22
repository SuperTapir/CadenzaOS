#!/usr/bin/env python3
"""Generate the narrow L1 Sharp-screen PCB candidate from the frozen baseline.

The reference board's outline, holes, ESP32-S3, USB connector, power, audio and
controls stay fixed.  Only the legacy display branch is replaced.  The Sharp
glass/window is shifted +1.5 mm in X and -1.5 mm in KiCad Y so its short FPC can
fold into J_DISP1 without moving USB1.
"""

from __future__ import annotations

import argparse
from pathlib import Path

import pcbnew


DELETED_REFS = {"FPC1", "U6", "Q2", "R13", "R14"}
DISPLAY_NETS = {"GPIO12", "GPIO14", "GPIO47", "GPIO48", "GPIO39"}
REMOVED_NETS = {"3V3LCD", "GPIO3", "$1N34531", "$1N34538"}
J_FOOTPRINT = "Hirose_FH34SRJ-10S-0.5SH_50_1x10_P0.50mm_DualContact"
J_POSITION = (154.625, 128.500)
C_DISP_POSITION = (169.500, 109.500)
CAP_POSITIONS = {
    "C20": ((165.000, 119.500), 0.0),
    "C21": ((169.500, 119.500), 0.0),
}


def point(x: float, y: float) -> pcbnew.VECTOR2I:
    return pcbnew.VECTOR2I(pcbnew.FromMM(x), pcbnew.FromMM(y))


def xy(value: pcbnew.VECTOR2I) -> tuple[float, float]:
    return (round(pcbnew.ToMM(value.x), 4), round(pcbnew.ToMM(value.y), 4))


def net(board: pcbnew.BOARD, name: str) -> pcbnew.NETINFO_ITEM:
    result = board.FindNet(name)
    if result is None:
        raise RuntimeError(f"missing baseline net {name}")
    return result


def add_track(
    board: pcbnew.BOARD,
    net_name: str,
    a: tuple[float, float],
    b: tuple[float, float],
    width_mm: float = 0.254,
    layer: int = pcbnew.F_Cu,
) -> None:
    if a == b:
        return
    track = pcbnew.PCB_TRACK(board)
    track.SetStart(point(*a))
    track.SetEnd(point(*b))
    track.SetWidth(pcbnew.FromMM(width_mm))
    track.SetLayer(layer)
    track.SetNet(net(board, net_name))
    board.Add(track)


def add_path(
    board: pcbnew.BOARD,
    net_name: str,
    points: list[tuple[float, float]],
    width_mm: float = 0.254,
    layer: int = pcbnew.F_Cu,
) -> None:
    for a, b in zip(points, points[1:]):
        add_track(board, net_name, a, b, width_mm, layer)


def add_via(
    board: pcbnew.BOARD,
    net_name: str,
    at: tuple[float, float],
    diameter_mm: float = 0.61,
    drill_mm: float = 0.30,
) -> None:
    via = pcbnew.PCB_VIA(board)
    via.SetPosition(point(*at))
    via.SetWidth(pcbnew.FromMM(diameter_mm))
    via.SetDrill(pcbnew.FromMM(drill_mm))
    via.SetViaType(pcbnew.VIATYPE_THROUGH)
    via.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
    via.SetNet(net(board, net_name))
    board.Add(via)


def item_points(item: pcbnew.BOARD_ITEM) -> list[tuple[float, float]]:
    if isinstance(item, pcbnew.PCB_VIA):
        return [xy(item.GetPosition())]
    return [xy(item.GetStart()), xy(item.GetEnd())]


def collect_target_zones(board: pcbnew.BOARD) -> list[pcbnew.ZONE]:
    """Collect before mutation; KiCad 9 invalidates later SWIG getters."""
    def should_remove(zone: pcbnew.ZONE) -> bool:
        name = str(zone.GetNetname())
        bbox = zone.GetBoundingBox()
        x0 = pcbnew.ToMM(bbox.GetX())
        y0 = pcbnew.ToMM(bbox.GetY())
        x1 = x0 + pcbnew.ToMM(bbox.GetWidth())
        y1 = y0 + pcbnew.ToMM(bbox.GetHeight())
        old_display_area = x1 >= 166.5 and x0 <= 182.5 and y1 >= 103.0 and y0 <= 116.5
        local_teardrop = (x1 - x0) <= 5.0 and (y1 - y0) <= 5.0
        return (
            name in REMOVED_NETS
            or name in {"D+", "D-", "$1N34554"}
            or (old_display_area and local_teardrop and name in DISPLAY_NETS | {"GND"})
        )

    return [zone for zone in board.Zones() if should_remove(zone)]


def collect_tracks_to_remove(board: pcbnew.BOARD) -> list[pcbnew.BOARD_ITEM]:
    """Collect legacy display and locally displaced USB copper."""
    old_terminal_vias = {
        "GPIO12": (174.125, 107.4),
        "GPIO14": (175.175, 108.975),
        "GPIO47": (177.825, 109.575),
        "GPIO48": (177.825, 108.175),
        "GPIO39": (167.487, 111.525),
        "GND": (177.875, 111.0),
    }
    old_fanout_x_min = 166.8

    targets: list[pcbnew.BOARD_ITEM] = []
    for item in board.GetTracks():
        name = str(item.GetNetname())
        points = item_points(item)
        remove = name in REMOVED_NETS or name in {"D+", "D-", "$1N34554"}
        if name in DISPLAY_NETS:
            if item.GetLayer() == pcbnew.B_Cu:
                remove = True
            if any(x >= old_fanout_x_min for x, _ in points):
                remove = True
            if isinstance(item, pcbnew.PCB_VIA) and points[0] == old_terminal_vias[name]:
                remove = True
        if name == "GND" and any(x >= 174.0 and 106.0 <= y <= 113.2 for x, y in points):
            remove = True
        # Preserve the trunk-to-C22 path through y=105.7075; remove only the
        # continuation from C22 toward deleted regulator U6.
        if (
            name == "3V3M"
            and all(168.30 <= x <= 168.45 for x, _ in points)
            and min(y for _, y in points) >= 105.70
            and max(y for _, y in points) <= 107.70
        ):
            remove = True
        if remove:
            targets.append(item)
    return targets


def reassign_and_move_reused_caps(
    board: pcbnew.BOARD, footprints: dict[str, pcbnew.FOOTPRINT]
) -> None:
    for reference, power_pad in (("C20", "2"), ("C21", "1")):
        footprint = footprints[reference]
        position, rotation = CAP_POSITIONS[reference]
        footprint.SetPosition(point(*position))
        footprint.SetOrientationDegrees(rotation)
        for pad in footprint.Pads():
            pad.SetNet(net(board, "+5V" if str(pad.GetNumber()) == power_pad else "GND"))


def add_new_footprints(board: pcbnew.BOARD, library_root: Path, baseline_root: Path) -> None:
    connector = pcbnew.FootprintLoad(
        str(library_root / "Cadenza_Display_FPC_Variants.pretty"), J_FOOTPRINT
    )
    if connector is None:
        raise RuntimeError("unable to load selected FH34SRJ footprint")
    connector.SetReference("J_DISP1")
    connector.SetValue("FH34SRJ-10S-0.5SH(50)")
    connector.SetFPID(pcbnew.LIB_ID("Cadenza_Display_FPC_Variants", J_FOOTPRINT))
    connector.SetPosition(point(*J_POSITION))
    connector.SetOrientationDegrees(0.0)
    board.Add(connector)

    pin_map = {
        "1": "GPIO39", "2": "GPIO48", "3": "GPIO47", "4": "GPIO14",
        "5": "GPIO12", "6": "+5V", "7": "+5V", "8": "+5V",
        "9": "GND", "10": "GND",
    }
    seen: set[str] = set()
    for pad in connector.Pads():
        number = str(pad.GetNumber())
        if number in pin_map:
            pad.SetNet(net(board, pin_map[number]))
            seen.add(number)
        elif number == "MP":
            pad.SetNet(net(board, "GND"))
    if seen != set(pin_map):
        raise RuntimeError(f"connector pad set mismatch: {sorted(seen)}")

    capacitor = pcbnew.FootprintLoad(str(baseline_root / "Cadenza-re-easyedapro.pretty"), "C0805")
    if capacitor is None:
        raise RuntimeError("unable to load reference C0805 footprint")
    capacitor.SetReference("C_DISP1")
    capacitor.SetValue("100nF")
    capacitor.SetFPID(pcbnew.LIB_ID("Cadenza-re-easyedapro", "C0805"))
    capacitor.SetPosition(point(*C_DISP_POSITION))
    capacitor.SetOrientationDegrees(0.0)
    board.Add(capacitor)
    for pad in capacitor.Pads():
        pad.SetNet(net(board, "GPIO12" if str(pad.GetNumber()) == "1" else "GND"))


def add_display_signal_routes(board: pcbnew.BOARD) -> None:
    # Existing U4-to-via fanout is retained.  The five old bottom trunks are
    # replaced with ordered lanes so no two Sharp signals cross.
    routes = {
        "GPIO12": ((152.500, 107.400), (155.000, 111.200), 154.875),
        "GPIO14": ((155.025, 108.975), (156.000, 111.200), 155.375),
        "GPIO47": ((157.575, 109.575), (157.000, 111.200), 155.875),
        "GPIO48": ((158.850, 108.175), (158.000, 111.200), 156.375),
        "GPIO39": ((163.100, 111.525), (159.000, 111.200), 156.875),
    }
    for name, (source, lane_via, pin_x) in routes.items():
        add_path(board, name, [source, lane_via], width_mm=0.20, layer=pcbnew.B_Cu)
        add_via(board, name, lane_via)
        add_path(
            board,
            name,
            [
                lane_via,
                (lane_via[0], 116.000),
                (pin_x, 126.000),
                (pin_x, 128.500),
            ],
            width_mm=0.20,
            layer=pcbnew.F_Cu,
        )

    # DISP is quasi-static.  Its 100 nF capacitor branches from the retained
    # source via through the vacated display corridor, avoiding a via-in-pad at
    # the 0.5 mm connector.
    add_path(board, "GPIO12", [(152.500, 107.400), (167.000, 107.400)], width_mm=0.20, layer=pcbnew.B_Cu)
    add_via(board, "GPIO12", (167.000, 107.400), 0.61, 0.30)
    add_path(board, "GPIO12", [(167.000, 107.400), (168.500, 109.000), (168.500, 109.500)], width_mm=0.20)


def add_display_power_and_ground(board: pcbnew.BOARD) -> None:
    # Continue from the existing C10 +5V branch.  Two layer changes pass the
    # inherited 3V3M/GPIO0 corridor through a gap without crossing it.
    add_path(board, "+5V", [(180.006, 95.167), (178.200, 95.500)], width_mm=0.254)
    add_via(board, "+5V", (178.200, 95.500), 0.61, 0.30)
    add_path(board, "+5V", [(178.200, 95.500), (178.200, 104.000)], width_mm=0.254, layer=pcbnew.B_Cu)
    add_via(board, "+5V", (178.200, 104.000), 0.61, 0.30)
    add_path(
        board,
        "+5V",
        [(178.200, 104.000), (178.200, 117.000), (168.500, 117.000), (168.500, 119.500), (166.000, 119.500)],
        width_mm=0.508,
        layer=pcbnew.F_Cu,
    )
    add_path(
        board,
        "+5V",
        [(167.250, 119.500), (167.250, 121.000), (163.000, 122.500), (163.000, 130.500), (156.000, 130.500)],
        width_mm=0.508,
        layer=pcbnew.F_Cu,
    )
    add_path(board, "+5V", [(156.000, 130.500), (155.500, 130.000), (153.375, 130.000)], width_mm=0.20)
    for pin_x in (153.375, 153.875, 154.375):
        add_path(board, "+5V", [(pin_x, 130.000), (pin_x, 128.500)], width_mm=0.20)

    # Local ground returns for the connector, three display capacitors, and
    # the two mechanical hold-down pads through the existing ground pours.
    add_path(board, "GND", [(152.375, 128.500), (152.375, 130.900), (150.500, 130.900)], width_mm=0.20)
    add_path(board, "GND", [(152.875, 128.500), (152.875, 130.900)], width_mm=0.20)
    add_path(board, "GND", [(152.875, 130.900), (152.375, 130.900)], width_mm=0.20)
    add_via(board, "GND", (150.500, 130.900), 0.61, 0.30)

    add_path(board, "GND", [(170.500, 109.500), (171.300, 109.500)], width_mm=0.254)
    add_via(board, "GND", (171.300, 109.500), 0.61, 0.30)
    add_path(board, "GND", [(164.000, 119.500), (163.200, 119.500)], width_mm=0.254)
    add_via(board, "GND", (163.200, 119.500), 0.61, 0.30)
    add_path(board, "GND", [(170.500, 119.500), (171.300, 119.500)], width_mm=0.254)
    add_via(board, "GND", (171.300, 119.500), 0.61, 0.30)


def add_usb_reroute(board: pcbnew.BOARD) -> None:
    # USB-FS pair detours below/right of J_DISP1.  Pair spacing is nominally
    # 0.6 mm; exact impedance remains a stack-up/field-solver manufacturing gate.
    add_path(board, "D+", [(153.625, 136.941), (152.625, 135.291)], width_mm=0.254, layer=pcbnew.B_Cu)
    add_path(board, "D+", [(152.625, 135.291), (152.625, 133.800), (153.500, 132.200), (159.400, 131.800)], width_mm=0.254, layer=pcbnew.B_Cu)
    add_via(board, "D+", (159.400, 131.800), 0.61, 0.30)
    add_path(
        board,
        "D+",
        [(159.400, 131.800), (171.500, 131.800), (172.400, 130.900), (172.400, 125.9105), (175.4375, 125.9105)],
        width_mm=0.254,
    )

    add_path(board, "D-", [(152.625, 136.941), (153.625, 135.291)], width_mm=0.254)
    add_path(board, "D-", [(153.625, 135.291), (153.625, 134.400), (154.500, 132.800), (160.100, 132.500)], width_mm=0.254, layer=pcbnew.B_Cu)
    add_via(board, "D-", (160.100, 132.500), 0.61, 0.30)
    add_path(
        board,
        "D-",
        [(160.100, 132.500), (172.200, 132.500), (173.200, 131.500), (173.200, 127.1805), (175.4375, 127.1805)],
        width_mm=0.254,
    )

    # CC1 no longer rises through the connector courtyard; it stays alongside
    # the fixed USB body and reaches the unchanged R1 endpoint.
    add_path(board, "$1N34554", [(151.625, 135.291), (151.625, 133.800), (150.500, 132.200), (147.000, 132.200)], width_mm=0.254, layer=pcbnew.B_Cu)
    add_via(board, "$1N34554", (147.000, 132.200), 0.61, 0.30)
    add_path(board, "$1N34554", [(147.000, 132.200), (145.604, 134.000), (145.604, 134.893)], width_mm=0.254)


def build(baseline: Path, output: Path, library_root: Path) -> None:
    board = pcbnew.LoadBoard(str(baseline))
    # Collect all proxies first.  On macOS, KiCad 9's SWIG collection getters
    # can become invalid after the first removal from any board collection.
    zone_targets = collect_target_zones(board)
    track_targets = collect_tracks_to_remove(board)
    deleted_footprints = []
    for reference in sorted(DELETED_REFS):
        footprint = board.FindFootprintByReference(reference)
        if footprint is None:
            raise RuntimeError(f"missing baseline footprint {reference}")
        deleted_footprints.append(footprint)
    retained_caps = {}
    for reference in ("C20", "C21"):
        footprint = board.FindFootprintByReference(reference)
        if footprint is None:
            raise RuntimeError(f"missing retained {reference}")
        retained_caps[reference] = footprint
    u4 = board.FindFootprintByReference("U4")
    if u4 is None:
        raise RuntimeError("missing frozen ESP32-S3 module U4")
    u7 = board.FindFootprintByReference("U7")
    if u7 is None:
        raise RuntimeError("missing frozen microphone U7")

    for zone in reversed(zone_targets):
        board.Remove(zone)
    for item in reversed(track_targets):
        board.Remove(item)
    for footprint in deleted_footprints:
        board.Remove(footprint)

    # GPIO3 served only the deleted TFT branch.  Leave the physical U4 pad in
    # place but clear its PCB net, matching the screen-only schematic NC pin.
    gpio3_pads = [pad for pad in u4.Pads() if str(pad.GetNumber()) == "15"]
    if len(gpio3_pads) != 1 or str(gpio3_pads[0].GetNetname()) != "GPIO3":
        raise RuntimeError("unexpected U4 GPIO3 pad identity")
    gpio3_pads[0].SetNetCode(0)

    # EasyEDA exported U4's exposed pad as nine repeated pad-41 shapes but
    # assigned GND to only one.  They are one physical thermal pad, so make
    # their KiCad net semantics agree before the ground pour is refilled.
    u4_thermal = [pad for pad in u4.Pads() if str(pad.GetNumber()) == "41"]
    if len(u4_thermal) != 9 or not any(str(pad.GetNetname()) == "GND" for pad in u4_thermal):
        raise RuntimeError("unexpected split thermal pad U4.41")
    for pad in u4_thermal:
        pad.SetNet(net(board, "GND"))

    # EasyEDA also exported U7 pad 3 as several overlapping custom-pad
    # fragments while assigning GND to only one of them.  Since they all carry
    # the same physical pin number, leaving the other fragments at <no net>
    # creates false copper shorts in KiCad and ambiguous Gerber semantics.
    # Normalize every pad-3 fragment to GND without moving or reshaping it.
    u7_ground_fragments = [pad for pad in u7.Pads() if str(pad.GetNumber()) == "3"]
    if len(u7_ground_fragments) < 2 or not any(
        str(pad.GetNetname()) == "GND" for pad in u7_ground_fragments
    ):
        raise RuntimeError("unexpected split microphone pad U7.3")
    for pad in u7_ground_fragments:
        pad.SetNet(net(board, "GND"))
        pad.SetLocalZoneConnection(pcbnew.ZONE_CONNECTION_FULL)

    # The four M2 mounting holes arrived from EasyEDA as plated pads whose
    # copper diameter equals the drill diameter.  They are mechanical,
    # unconnected holes, so normalize them to NPTH; this preserves the exact
    # 2.0 mm drill and coordinates while avoiding accidental barrel plating.
    for reference in ("SCREW1", "SCREW2", "SCREW3", "SCREW4"):
        footprint = board.FindFootprintByReference(reference)
        pads = list(footprint.Pads()) if footprint is not None else []
        if len(pads) != 1 or pads[0].GetNetCode() != 0:
            raise RuntimeError(f"unexpected mechanical mounting hole {reference}")
        if abs(pcbnew.ToMM(pads[0].GetDrillSize().x) - 2.0) > 1e-6:
            raise RuntimeError(f"unexpected mounting drill diameter {reference}")
        pads[0].SetAttribute(pcbnew.PAD_ATTRIB_NPTH)

    reassign_and_move_reused_caps(board, retained_caps)
    add_new_footprints(board, library_root, baseline.parent)
    add_display_signal_routes(board)
    add_display_power_and_ground(board)
    add_usb_reroute(board)
    output.parent.mkdir(parents=True, exist_ok=True)
    pcbnew.SaveBoard(str(output), board)
    # Refill the inherited ground pours around the newly generated copper.
    # A fresh load avoids the macOS SWIG invalidation described above.
    filled = pcbnew.LoadBoard(str(output))
    if not pcbnew.ZONE_FILLER(filled).Fill(filled.Zones()):
        raise RuntimeError("zone refill failed")
    pcbnew.SaveBoard(str(output), filled)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--library-root", type=Path, required=True)
    args = parser.parse_args()
    build(args.baseline.resolve(), args.output.resolve(), args.library_root.resolve())
    print(args.output.resolve())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
