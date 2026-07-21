#!/usr/bin/env python3
"""Finish local v42 connections that require deliberate fine-pitch escape."""

from pathlib import Path

import pcbnew


ROOT = Path(__file__).parent / "production-v42-priority"
SOURCE = ROOT / "cadenza-d1-v42-expanded-critical.kicad_pcb"
OUTPUT = ROOT / "cadenza-d1-v42-expanded-gauge-base.kicad_pcb"


def point(x, y):
    return pcbnew.VECTOR2I(pcbnew.FromMM(x), pcbnew.FromMM(y))


def add_track(board, net, a, b, layer=pcbnew.B_Cu, width=0.15):
    track = pcbnew.PCB_TRACK(board)
    track.SetStart(point(*a))
    track.SetEnd(point(*b))
    track.SetWidth(pcbnew.FromMM(width))
    track.SetLayer(layer)
    track.SetNet(net)
    track.SetLocked(True)
    board.Add(track)


def add_via(board, net, xy):
    via = pcbnew.PCB_VIA(board)
    via.SetPosition(point(*xy))
    via.SetWidth(pcbnew.FromMM(0.6))
    via.SetDrill(pcbnew.FromMM(0.3))
    via.SetViaType(pcbnew.VIATYPE_THROUGH)
    via.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
    via.SetNet(net)
    via.SetLocked(True)
    board.Add(via)


def main():
    board = pcbnew.LoadBoard(str(SOURCE))
    # Free the local corridor; the I2C pair is rerouted after GAUGE_VDD is fixed.
    for item in list(board.GetTracks()):
        if item.GetNetname() in {"/I2C_SDA", "/I2C_SCL"}:
            board.Delete(item)

    net = board.FindNet("/GAUGE_VDD")
    if net is None:
        raise RuntimeError("missing /GAUGE_VDD")
    b_escape = [(126.1, 105.4), (124.8, 105.4), (124.8, 106.2)]
    for a, b in zip(b_escape, b_escape[1:]):
        add_track(board, net, a, b)
    add_via(board, net, (124.8, 106.2))
    add_track(board, net, (124.8, 106.2), (122.5, 109.5), layer=pcbnew.F_Cu)
    add_via(board, net, (122.5, 109.5))
    add_track(board, net, (122.5, 109.5), (123.275, 108.0))
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    pcbnew.SaveBoard(str(OUTPUT), board)
    print(OUTPUT)


if __name__ == "__main__":
    main()
