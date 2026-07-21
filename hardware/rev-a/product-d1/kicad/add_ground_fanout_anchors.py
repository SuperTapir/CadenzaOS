#!/usr/bin/env python3
"""Anchor surface GND pours with short pad-to-via fanouts."""

from __future__ import annotations

import argparse
from pathlib import Path

import pcbnew


def mm(value: float) -> int:
    return pcbnew.FromMM(value)


def find_pad(board: pcbnew.BOARD, reference: str, number: str) -> pcbnew.PAD:
    footprint = next(
        (item for item in board.GetFootprints() if item.GetReference() == reference),
        None,
    )
    if footprint is None:
        raise SystemExit(f"missing footprint {reference}")
    pad = next((item for item in footprint.Pads() if item.GetNumber() == number), None)
    if pad is None:
        raise SystemExit(f"missing pad {reference}.{number}")
    return pad


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    if args.input.resolve() == args.output.resolve():
        raise SystemExit("refusing to overwrite the input board")

    board = pcbnew.LoadBoard(str(args.input))
    gnd = board.FindNet("/GND")
    if gnd is None:
        raise SystemExit("/GND net is missing")

    zones = {
        zone.GetLayer(): zone
        for zone in board.Zones()
        if zone.GetNetCode() == gnd.GetNetCode()
        and zone.GetLayer() in (pcbnew.F_Cu, pcbnew.In1_Cu, pcbnew.B_Cu)
    }
    for layer in (pcbnew.F_Cu, pcbnew.In1_Cu, pcbnew.B_Cu):
        if layer not in zones or not zones[layer].HasFilledPolysForLayer(layer):
            raise SystemExit(f"missing filled /GND zone on layer {layer}")

    # Route J6 /A_SW first so the adjacent GND pin can escape upward.  Pass 1
    # routed this low-speed signal immediately above the connector, trapping
    # J6.2 between /A_SW and /ENC_A.  Preserve the exact far endpoint while
    # moving only the three connector-side F.Cu segments upward.
    a_sw = board.FindNet("/A_SW")
    if a_sw is None:
        raise SystemExit("/A_SW net is missing")
    old_a_sw = [
        item for item in board.GetTracks()
        if item.Type() == pcbnew.PCB_TRACE_T
        and item.GetLayer() == pcbnew.F_Cu
        and item.GetNetCode() == a_sw.GetNetCode()
    ]
    if len(old_a_sw) != 3:
        raise SystemExit(f"expected 3 connector-side /A_SW tracks, found {len(old_a_sw)}")
    far = min(
        (point for item in old_a_sw for point in (item.GetStart(), item.GetEnd())),
        key=lambda point: point.x,
    )
    for item in old_a_sw:
        board.Remove(item)
    a_sw_points = [
        find_pad(board, "J6", "5").GetPosition(),
        pcbnew.VECTOR2I(mm(161.500), mm(79.530)),
        pcbnew.VECTOR2I(mm(160.889), far.y),
        pcbnew.VECTOR2I(mm(160.500), far.y),
        pcbnew.VECTOR2I(mm(160.500), mm(76.500)),
        pcbnew.VECTOR2I(mm(156.500), mm(76.500)),
        pcbnew.VECTOR2I(mm(156.500), far.y),
        far,
    ]
    for start, end in zip(a_sw_points, a_sw_points[1:]):
        track = pcbnew.PCB_TRACK(board)
        track.SetStart(start)
        track.SetEnd(end)
        track.SetWidth(mm(0.20))
        track.SetLayer(pcbnew.F_Cu)
        track.SetNet(a_sw)
        board.Add(track)

    # The through GND via added below also crosses B.Cu.  Move only the four
    # U1-side I2S_LRCLK segments through the narrow but manufacturable channel
    # centered between the AMP_EN and new GND vias.  A 0.15 mm local neck-down
    # preserves slightly more than 0.20 mm clearance to both via barrels.
    lrclk = board.FindNet("/I2S_LRCLK")
    if lrclk is None:
        raise SystemExit("/I2S_LRCLK net is missing")
    old_lrclk = [
        item for item in board.GetTracks()
        if item.Type() == pcbnew.PCB_TRACE_T
        and item.GetLayer() == pcbnew.B_Cu
        and item.GetNetCode() == lrclk.GetNetCode()
        and min(item.GetStart().x, item.GetEnd().x) > mm(157.000)
        and max(item.GetStart().y, item.GetEnd().y) < mm(85.000)
    ]
    if len(old_lrclk) != 4:
        raise SystemExit(f"expected 4 U1-side /I2S_LRCLK tracks, found {len(old_lrclk)}")
    lrclk_start = max(
        (point for item in old_lrclk for point in (item.GetStart(), item.GetEnd())),
        key=lambda point: point.x,
    )
    lrclk_end = max(
        (point for item in old_lrclk for point in (item.GetStart(), item.GetEnd())),
        key=lambda point: point.y,
    )
    for item in old_lrclk:
        board.Remove(item)
    lrclk_points = [
        lrclk_start,
        pcbnew.VECTOR2I(mm(157.913), lrclk_start.y),
        pcbnew.VECTOR2I(mm(157.913), mm(83.615)),
        lrclk_end,
    ]
    for start, end in zip(lrclk_points, lrclk_points[1:]):
        track = pcbnew.PCB_TRACK(board)
        track.SetStart(start)
        track.SetEnd(end)
        track.SetWidth(mm(0.15))
        track.SetLayer(pcbnew.B_Cu)
        track.SetNet(lrclk)
        board.Add(track)

    # These destinations are frozen against the D1 Pass 1 placement/routing.
    # Each point is in both the surface pour and In1 GND plane, and has at
    # least 0.20 mm clearance from copper on every layer for a 0.60/0.30 via.
    anchors = (
        ("U6", "4", pcbnew.F_Cu, ((59.850, 58.850),)),
        (
            "J6",
            "2",
            pcbnew.F_Cu,
            ((158.500, 78.500),),
        ),
        ("U7", "4", pcbnew.B_Cu, ((118.350, 103.800),)),
        ("C2", "2", pcbnew.B_Cu, ((140.511, 97.854),)),
        ("C13", "2", pcbnew.B_Cu, ((120.775, 93.600),)),
        ("U4", "4", pcbnew.B_Cu, ((134.740, 81.000),)),
        ("C22", "2", pcbnew.B_Cu, ((129.775, 78.800),)),
    )
    for reference, number, surface_layer, waypoints in anchors:
        pad = find_pad(board, reference, number)
        if pad.GetNetCode() != gnd.GetNetCode():
            raise SystemExit(f"{reference}.{number} is not /GND")

        start = pad.GetPosition()
        points = [pcbnew.VECTOR2I(mm(x), mm(y)) for x, y in waypoints]
        chosen = points[-1]
        if not (
            zones[surface_layer].HitTestFilledArea(surface_layer, chosen)
            and zones[pcbnew.In1_Cu].HitTestFilledArea(pcbnew.In1_Cu, chosen)
        ):
            raise SystemExit(f"frozen fanout point is outside GND fill for {reference}.{number}")

        previous = start
        for point in points:
            track = pcbnew.PCB_TRACK(board)
            track.SetStart(previous)
            track.SetEnd(point)
            track.SetWidth(mm(0.20))
            track.SetLayer(surface_layer)
            track.SetNet(gnd)
            board.Add(track)
            previous = point

        via = pcbnew.PCB_VIA(board)
        via.SetPosition(chosen)
        via.SetWidth(mm(0.60))
        via.SetDrill(mm(0.30))
        via.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
        via.SetViaType(pcbnew.VIATYPE_THROUGH)
        via.SetNet(gnd)
        for layer in (pcbnew.F_Cu, pcbnew.In1_Cu, pcbnew.B_Cu):
            via.SetZoneLayerOverride(layer, pcbnew.ZLO_FORCE_FLASHED)
        board.Add(via)

        print(
            f"{reference}.{number} {board.GetLayerName(surface_layer)} fanout: "
            f"{pcbnew.ToMM(start.x):.3f},{pcbnew.ToMM(start.y):.3f} -> "
            f"{pcbnew.ToMM(chosen.x):.3f},{pcbnew.ToMM(chosen.y):.3f} mm"
        )

    args.output.parent.mkdir(parents=True, exist_ok=True)
    pcbnew.SaveBoard(str(args.output), board)


if __name__ == "__main__":
    main()
