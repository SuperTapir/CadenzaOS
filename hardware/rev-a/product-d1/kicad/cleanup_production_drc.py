#!/usr/bin/env python3
"""Apply deterministic production-DRC cleanup to the D1 board."""

import argparse

import pcbnew


MM = 1_000_000
SILK_TO_FAB = {"H1", "H2", "H3", "H4", "J2", "SW1", "U3", "U4", "U6"}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("input")
    parser.add_argument("output")
    args = parser.parse_args()
    board = pcbnew.LoadBoard(args.input)

    removed_vias = 0
    for item in list(board.GetTracks()):
        if item.Type() != pcbnew.PCB_VIA_T:
            continue
        position = item.GetPosition()
        if abs(position.x / MM - 124.0456) < 0.01 and abs(position.y / MM - 102.2005) < 0.01:
            board.Remove(item)
            removed_vias += 1
    if removed_vias != 1:
        raise SystemExit(f"expected one obsolete USB_D+ via, removed {removed_vias}")

    for zone in board.Zones():
        if not zone.GetIsRuleArea() and zone.GetNetname() == "/GND":
            zone.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)

    mirrored = 0
    moved_graphics = 0
    frozen_footprints = 0
    for footprint in board.GetFootprints():
        footprint.SetFPID(pcbnew.LIB_ID())
        frozen_footprints += 1
        if footprint.GetReference() == "L1":
            footprint.Reference().SetVisible(False)
        if footprint.GetLayer() == pcbnew.B_Cu:
            for field in footprint.GetFields():
                field.SetMirrored(True)
                mirrored += 1
            for graphic in footprint.GraphicalItems():
                if isinstance(graphic, pcbnew.PCB_TEXT):
                    graphic.SetMirrored(True)
                    mirrored += 1
        if footprint.GetReference() in SILK_TO_FAB:
            for graphic in footprint.GraphicalItems():
                if graphic.GetLayer() == pcbnew.F_SilkS:
                    graphic.SetLayer(pcbnew.F_Fab)
                    moved_graphics += 1
                elif graphic.GetLayer() == pcbnew.B_SilkS:
                    graphic.SetLayer(pcbnew.B_Fab)
                    moved_graphics += 1

    pcbnew.SaveBoard(args.output, board)
    print(
        f"removed vias: {removed_vias}; mirrored text: {mirrored}; "
        f"silk graphics moved to Fab: {moved_graphics}; frozen footprints: {frozen_footprints}"
    )


if __name__ == "__main__":
    main()
