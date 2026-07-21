#!/usr/bin/env python3
"""Produce the electrically complete Cadenza D1 Rev-A PCB candidate."""

from pathlib import Path
import pcbnew


ROOT = Path(__file__).resolve().parent
SOURCE = ROOT / "production-v42-priority" / "cadenza-d1-v46-controls-rerouted.kicad_pcb"
OUTPUT = ROOT / "production-v42-priority" / "cadenza-d1-rev-a-production.kicad_pcb"


def mm(value: float) -> int:
    return pcbnew.FromMM(value)


def point(x: float, y: float) -> pcbnew.VECTOR2I:
    return pcbnew.VECTOR2I(mm(x), mm(y))


board = pcbnew.LoadBoard(str(SOURCE))
original_tracks = list(board.GetTracks())
gnd = board.FindNet("/GND") or board.FindNet("GND")

# Final C2 ground-island stitch, selected by contour search and full DRC sweep.
via = pcbnew.PCB_VIA(board)
via.SetNet(gnd)
via.SetPosition(point(139.25, 98.50))
via.SetWidth(mm(0.50))
via.SetDrill(mm(0.25))
via.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
board.Add(via)

# Remove the three branch stubs reintroduced with locked SES geometry.
dangling_ends = [
    point(113.9530, 102.8590),
    point(117.9185, 102.7310),
    point(144.7320, 104.0195),
]
for item in original_tracks:
    if not isinstance(item, pcbnew.PCB_TRACK) or isinstance(item, pcbnew.PCB_VIA):
        continue
    if any(
        (item.GetStart() - p).EuclideanNorm() < mm(0.01)
        or (item.GetEnd() - p).EuclideanNorm() < mm(0.01)
        for p in dangling_ends
    ):
        board.Remove(item)

pcbnew.ZONE_FILLER(board).Fill(board.Zones())
pcbnew.SaveBoard(str(OUTPUT), board)
print(OUTPUT)
