#!/usr/bin/env python3
"""Add deterministic ground-stitching vias in common filled copper areas."""

import argparse
import math

import pcbnew


MM = 1_000_000
VIA_DIAMETER = 0.60
VIA_DRILL = 0.30
OBSTACLE_MARGIN = 0.55
RF_BOTTOM_STITCHING = ((150.5, 71.5), (145.5, 71.0))


def distance_to_segment(px, py, ax, ay, bx, by):
    dx, dy = bx - ax, by - ay
    if dx == 0 and dy == 0:
        return math.hypot(px - ax, py - ay)
    t = max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy)))
    return math.hypot(px - (ax + t * dx), py - (ay + t * dy))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("input")
    parser.add_argument("output")
    args = parser.parse_args()
    board = pcbnew.LoadBoard(args.input)
    gnd = board.FindNet("/GND")
    zones = [z for z in board.Zones() if not z.GetIsRuleArea() and z.GetNetname() == "/GND"]

    pads = []
    for footprint in board.GetFootprints():
        for pad in footprint.Pads():
            pos, size = pad.GetPosition(), pad.GetSize()
            pads.append((pos.x / MM, pos.y / MM, math.hypot(size.x, size.y) / (2 * MM)))

    tracks = []
    for item in board.GetTracks():
        start, end = item.GetStart(), item.GetEnd()
        width = item.GetWidth(pcbnew.F_Cu) if item.Type() == pcbnew.PCB_VIA_T else item.GetWidth()
        tracks.append((start.x / MM, start.y / MM, end.x / MM, end.y / MM, width / (2 * MM)))

    inner_zones = []
    surface_zones = []
    for zone in zones:
        layer = list(zone.GetLayerSet().Seq())[0]
        if layer in (pcbnew.F_Cu, pcbnew.B_Cu):
            surface_zones.append((zone, layer))
        else:
            inner_zones.append((zone, layer))
    if not inner_zones:
        raise SystemExit("no inner GND plane found")

    existing_vias = []
    for item in board.GetTracks():
        if item.Type() == pcbnew.PCB_VIA_T and item.GetNetCode() == gnd.GetNetCode():
            existing_vias.append(item.GetPosition())

    selected = []
    skipped = []
    for zone, layer in surface_zones:
        filled = zone.GetFilledPolysList(layer)
        for island in range(filled.OutlineCount()):
            if any(filled.Contains(position, island) for position in existing_vias):
                continue
            bbox = filled.COutline(island).BBox()
            best = None
            x0 = math.ceil(bbox.GetX() / MM * 2)
            x1 = math.floor((bbox.GetX() + bbox.GetWidth()) / MM * 2)
            y0 = math.ceil(bbox.GetY() / MM * 2)
            y1 = math.floor((bbox.GetY() + bbox.GetHeight()) / MM * 2)
            for hy in range(y0, y1 + 1):
                for hx in range(x0, x1 + 1):
                    x, y = hx / 2, hy / 2
                    if x >= 141.75 and y <= 67.25:
                        continue
                    point = pcbnew.VECTOR2I(round(x * MM), round(y * MM))
                    if not filled.Contains(point, island):
                        continue
                    if not any(inner.HitTestFilledArea(inner_layer, point) for inner, inner_layer in inner_zones):
                        continue
                    pad_clearance = min(math.hypot(x - px, y - py) - radius for px, py, radius in pads)
                    track_clearance = min(
                        distance_to_segment(x, y, ax, ay, bx, by) - radius
                        for ax, ay, bx, by, radius in tracks
                    )
                    clearance = min(pad_clearance, track_clearance)
                    if clearance >= OBSTACLE_MARGIN + VIA_DIAMETER / 2 and (best is None or clearance > best[0]):
                        best = (clearance, x, y)
            if best is None:
                skipped.append((board.GetLayerName(layer), island))
            else:
                selected.append(best)
                existing_vias.append(pcbnew.VECTOR2I(round(best[1] * MM), round(best[2] * MM)))

    selected.sort(key=lambda item: (item[2], item[1]))
    selected.extend((0.0, x, y) for x, y in RF_BOTTOM_STITCHING)
    for _, x, y in selected:
        via = pcbnew.PCB_VIA(board)
        via.SetPosition(pcbnew.VECTOR2I(round(x * MM), round(y * MM)))
        via.SetWidth(round(VIA_DIAMETER * MM))
        via.SetDrill(round(VIA_DRILL * MM))
        via.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
        via.SetNet(gnd)
        board.Add(via)

    for zone in board.Zones():
        if not zone.GetIsRuleArea():
            zone.UnFill()
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    pcbnew.SaveBoard(args.output, board)
    print(f"added {len(selected)} island GND stitching vias: {[(x, y) for _, x, y in selected]}")
    print(f"islands without a safe via site: {skipped}")


if __name__ == "__main__":
    main()
