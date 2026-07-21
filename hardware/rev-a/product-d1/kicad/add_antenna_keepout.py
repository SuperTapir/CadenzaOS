#!/usr/bin/env python3
"""Add the frozen four-layer ESP32-S3 antenna copper/routing keepout."""

import argparse

import pcbnew


MM = 1_000_000
RECT = ((141.75, 49.75), (166.00, 49.75), (166.00, 67.25), (141.75, 67.25))
CUTOUT = ((141.75, 45.00), (170.00, 45.00), (170.00, 67.25), (141.75, 67.25))


def polygon(points):
    shape = pcbnew.SHAPE_POLY_SET()
    shape.NewOutline()
    for x, y in points:
        shape.Append(round(x * MM), round(y * MM))
    return shape


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("input")
    parser.add_argument("output")
    args = parser.parse_args()
    board = pcbnew.LoadBoard(args.input)
    outline = polygon(RECT)
    area = pcbnew.ZONE(board)
    area.SetIsRuleArea(True)
    area.SetDoNotAllowTracks(True)
    area.SetDoNotAllowVias(True)
    area.SetDoNotAllowZoneFills(True)
    area.SetLayerSet(pcbnew.LSET.AllCuMask())
    area.SetOutline(outline)
    board.Add(area)

    # A rule area prevents future copper, but KiCad can retain a disconnected
    # filled-zone partition at a board edge. Subtract the RF region from every
    # GND zone outline as well, extending the subtraction beyond the board edge.
    cutout = polygon(CUTOUT)
    cut_zones = 0
    for zone in board.Zones():
        if zone.GetIsRuleArea() or zone.GetNetname() != "/GND":
            continue
        zone_outline = zone.Outline()
        zone_outline.BooleanSubtract(cutout)
        zone.SetOutline(zone_outline)
        cut_zones += 1
    pcbnew.SaveBoard(args.output, board)
    print(f"four-layer antenna keepout added; RF cutout applied to {cut_zones} GND zones")


if __name__ == "__main__":
    main()
