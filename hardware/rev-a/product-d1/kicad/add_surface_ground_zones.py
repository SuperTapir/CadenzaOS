#!/usr/bin/env python3
"""Add F.Cu and B.Cu GND zones to a routed Cadenza D1 candidate board.

Run this script with KiCad's bundled Python interpreter so the `pcbnew` module
matches the board file version. The input board is never modified in place.
"""

from __future__ import annotations

import argparse
from pathlib import Path

import pcbnew


def mm(value: float) -> int:
    return pcbnew.FromMM(value)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    if args.input.resolve() == args.output.resolve():
        raise SystemExit("refusing to overwrite the input board")

    board = pcbnew.LoadBoard(str(args.input))
    if board.GetCopperLayerCount() != 4:
        raise SystemExit("expected a four-layer board")

    gnd = board.FindNet("/GND")
    if gnd is None:
        raise SystemExit("/GND net is missing")

    reference = next(
        (
            zone
            for zone in board.Zones()
            if zone.GetLayer() == pcbnew.In1_Cu and zone.GetNetCode() == gnd.GetNetCode()
        ),
        None,
    )
    if reference is None:
        raise SystemExit("In1.Cu /GND reference zone is missing")

    outline = pcbnew.SHAPE_POLY_SET()
    if not board.GetBoardPolygonOutlines(outline, False):
        raise SystemExit("failed to derive a closed board outline")

    existing_layers = {
        zone.GetLayer()
        for zone in board.Zones()
        if zone.GetNetCode() == gnd.GetNetCode()
    }
    for layer in (pcbnew.F_Cu, pcbnew.B_Cu):
        if layer in existing_layers:
            raise SystemExit(f"GND zone already exists on layer {layer}")

        zone = pcbnew.ZONE(board)
        zone.SetLayer(layer)
        zone.SetNet(gnd)
        zone.SetOutline(outline)
        zone.SetLocalClearance(reference.GetLocalClearance())
        zone.SetMinThickness(reference.GetMinThickness())
        zone.SetPadConnection(pcbnew.ZONE_CONNECTION_FULL)
        zone.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)
        zone.SetAssignedPriority(reference.GetAssignedPriority())
        if hasattr(zone, "SetThermalReliefGap"):
            zone.SetThermalReliefGap(reference.GetThermalReliefGap())
        if hasattr(zone, "SetThermalReliefSpokeWidth"):
            zone.SetThermalReliefSpokeWidth(reference.GetThermalReliefSpokeWidth())
        board.Add(zone)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    pcbnew.SaveBoard(str(args.output), board)


if __name__ == "__main__":
    main()
