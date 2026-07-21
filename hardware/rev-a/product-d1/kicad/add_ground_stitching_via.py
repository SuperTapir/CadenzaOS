#!/usr/bin/env python3
"""Add one safe through-board GND anchor to already-filled surface zones."""

from __future__ import annotations

import argparse
import math
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
    gnd = board.FindNet("/GND")
    if gnd is None:
        raise SystemExit("/GND net is missing")

    zones = {}
    for zone in board.Zones():
        if zone.GetNetCode() == gnd.GetNetCode() and zone.GetLayer() in (
            pcbnew.F_Cu,
            pcbnew.In1_Cu,
            pcbnew.B_Cu,
        ):
            if not zone.HasFilledPolysForLayer(zone.GetLayer()):
                raise SystemExit("all GND zones must be filled before stitching")
            zones[zone.GetLayer()] = zone

    required_layers = (pcbnew.F_Cu, pcbnew.In1_Cu, pcbnew.B_Cu)
    if any(layer not in zones for layer in required_layers):
        raise SystemExit("F.Cu, In1.Cu and B.Cu GND zones are required")

    blocked_boxes = []
    for footprint in board.GetFootprints():
        box = footprint.GetBoundingBox()
        box.Inflate(mm(1.0))
        blocked_boxes.append(box)

    bounds = board.GetBoardEdgesBoundingBox()
    margin = mm(1.5)
    ring_radius = mm(0.7)
    ring = [
        pcbnew.VECTOR2I(
            round(ring_radius * math.cos(index * math.tau / 16)),
            round(ring_radius * math.sin(index * math.tau / 16)),
        )
        for index in range(16)
    ]

    chosen = None
    y = bounds.GetY() + margin
    while y <= bounds.GetBottom() - margin and chosen is None:
        x = bounds.GetX() + margin
        while x <= bounds.GetRight() - margin:
            point = pcbnew.VECTOR2I(x, y)
            # Keep stitching away from the ESP32 antenna end of the board.
            if pcbnew.ToMM(x) < 140.0 and not any(box.Contains(point) for box in blocked_boxes):
                samples = [point] + [point + offset for offset in ring]
                if all(
                    zones[layer].HitTestFilledArea(layer, sample)
                    for layer in required_layers
                    for sample in samples
                ):
                    chosen = point
                    break
            x += mm(1.0)
        y += mm(1.0)

    if chosen is None:
        raise SystemExit("no safe common GND-fill point found")

    via = pcbnew.PCB_VIA(board)
    via.SetPosition(chosen)
    via.SetWidth(mm(0.60))
    via.SetDrill(mm(0.30))
    via.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
    via.SetViaType(pcbnew.VIATYPE_THROUGH)
    via.SetNet(gnd)
    board.Add(via)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    pcbnew.SaveBoard(str(args.output), board)
    print(f"GND stitching via: {pcbnew.ToMM(chosen.x):.3f}, {pcbnew.ToMM(chosen.y):.3f} mm")


if __name__ == "__main__":
    main()
