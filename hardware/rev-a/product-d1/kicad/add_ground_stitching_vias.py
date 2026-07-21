#!/usr/bin/env python3
"""Anchor every connectable F.Cu/B.Cu GND fill polygon to the inner plane."""

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

    for layer in (pcbnew.F_Cu, pcbnew.In1_Cu, pcbnew.B_Cu):
        if layer not in zones:
            raise SystemExit(f"missing filled GND zone on layer {layer}")

    # A through via must clear copper on every layer.  Blocking complete
    # footprint courtyards was far too conservative and left nearly every
    # useful island unstitched; use only actual different-net copper items.
    blocked_pads = []
    for footprint in board.GetFootprints():
        for pad in footprint.Pads():
            if pad.GetNetCode() == gnd.GetNetCode():
                continue
            blocked_pads.append(pad)
    blocked_tracks = [
        item for item in board.GetTracks()
        if item.GetNetCode() != gnd.GetNetCode()
    ]

    def point_segment_distance(point, start, end):
        dx = end.x - start.x
        dy = end.y - start.y
        if dx == 0 and dy == 0:
            return math.hypot(point.x - start.x, point.y - start.y)
        ratio = max(
            0.0,
            min(
                1.0,
                ((point.x - start.x) * dx + (point.y - start.y) * dy)
                / (dx * dx + dy * dy),
            ),
        )
        return math.hypot(
            point.x - (start.x + ratio * dx),
            point.y - (start.y + ratio * dy),
        )

    def via_is_clear(point):
        # 0.30 mm via radius plus the 0.20 mm production clearance.
        margin = mm(0.50)
        if any(pad.HitTest(point, margin) for pad in blocked_pads):
            return False
        for item in blocked_tracks:
            if item.Type() == pcbnew.PCB_VIA_T:
                required = margin + item.GetWidth(pcbnew.F_Cu) / 2
                if math.hypot(
                    point.x - item.GetPosition().x,
                    point.y - item.GetPosition().y,
                ) < required:
                    return False
            elif item.Type() == pcbnew.PCB_TRACE_T:
                required = margin + item.GetWidth() / 2
                if point_segment_distance(point, item.GetStart(), item.GetEnd()) < required:
                    return False
            else:
                box = item.GetBoundingBox()
                box.Inflate(margin)
                if box.Contains(point):
                    return False
        return True

    ring_radius = mm(0.35)
    ring = [
        pcbnew.VECTOR2I(
            round(ring_radius * math.cos(index * math.tau / 16)),
            round(ring_radius * math.sin(index * math.tau / 16)),
        )
        for index in range(16)
    ]
    anchors: list[pcbnew.VECTOR2I] = []
    added: list[tuple[int, pcbnew.VECTOR2I]] = []
    skipped: list[tuple[int, int]] = []

    for surface_layer in (pcbnew.F_Cu, pcbnew.B_Cu):
        polys = zones[surface_layer].GetFilledPolysList(surface_layer)
        for outline_index in range(polys.OutlineCount()):
            if any(polys.Contains(point, outline_index) for point in anchors):
                continue

            box = polys.COutline(outline_index).BBox()
            chosen = None
            y = box.GetY() + ring_radius
            while y <= box.GetBottom() - ring_radius and chosen is None:
                x = box.GetX() + ring_radius
                while x <= box.GetRight() - ring_radius:
                    point = pcbnew.VECTOR2I(x, y)
                    samples = [point] + [point + offset for offset in ring]
                    if (
                        # Preserve the provisional ESP32 antenna keepout side;
                        # its exact rule area will be frozen with the module
                        # placement before release.
                        pcbnew.ToMM(x) < 140.0
                        and via_is_clear(point)
                        and all(polys.Contains(sample, outline_index) for sample in samples)
                        and all(
                            zones[pcbnew.In1_Cu].HitTestFilledArea(pcbnew.In1_Cu, sample)
                            for sample in samples
                        )
                    ):
                        chosen = point
                        break
                    x += mm(0.25)
                y += mm(0.25)

            if chosen is None:
                skipped.append((surface_layer, outline_index))
                continue

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
            anchors.append(chosen)
            added.append((surface_layer, chosen))

    args.output.parent.mkdir(parents=True, exist_ok=True)
    pcbnew.SaveBoard(str(args.output), board)

    for layer, point in added:
        print(
            f"{board.GetLayerName(layer)} anchor: "
            f"{pcbnew.ToMM(point.x):.3f}, {pcbnew.ToMM(point.y):.3f} mm"
        )
    print(f"added {len(added)} GND stitching vias; skipped {len(skipped)} tiny/blocked polygons")


if __name__ == "__main__":
    main()
