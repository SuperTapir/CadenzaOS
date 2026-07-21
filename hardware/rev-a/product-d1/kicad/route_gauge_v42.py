#!/usr/bin/env python3
"""Route the GAUGE_BIN net that Freerouting 1.9 skips."""

from pathlib import Path

import pcbnew

from finalize_v41_routing import route


ROOT = Path(__file__).parent / "production-v42-priority"
SOURCE = ROOT / "cadenza-d1-v42-critical-routed.kicad_pcb"
OUTPUT = ROOT / "cadenza-d1-v42-critical-complete.kicad_pcb"


def pad_xy(board, reference: str, number: str):
    footprint = board.FindFootprintByReference(reference)
    if footprint is None:
        raise RuntimeError(f"missing footprint {reference}")
    pad = footprint.FindPadByNumber(number)
    if pad is None:
        raise RuntimeError(f"missing pad {reference}.{number}")
    pos = pad.GetPosition()
    return pcbnew.ToMM(pos.x), pcbnew.ToMM(pos.y)


def main():
    board = pcbnew.LoadBoard(str(SOURCE))
    net = board.FindNet("/GAUGE_BIN")
    if net is None:
        raise RuntimeError("missing /GAUGE_BIN")
    route(board, net, pad_xy(board, "U7", "10"), pad_xy(board, "R72", "1"), layer=pcbnew.B_Cu)
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    pcbnew.SaveBoard(str(OUTPUT), board)
    print(OUTPUT)


if __name__ == "__main__":
    main()
