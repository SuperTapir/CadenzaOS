#!/usr/bin/env python3
"""Manufacturing cleanup for the routed Cadenza D1 Rev-A v43 board."""

from pathlib import Path
import pcbnew


ROOT = Path(__file__).resolve().parent
SOURCE = ROOT / "production-v42-priority" / "cadenza-d1-v43b-local-candidate.kicad_pcb"
OUTPUT = ROOT / "production-v42-priority" / "cadenza-d1-v43-cleanup-wip.kicad_pcb"


def mm(value: float) -> int:
    return pcbnew.FromMM(value)


def point(x: float, y: float) -> pcbnew.VECTOR2I:
    return pcbnew.VECTOR2I(mm(x), mm(y))


def add_track(board, net, start, end, width=0.30, layer=pcbnew.B_Cu):
    track = pcbnew.PCB_TRACK(board)
    track.SetNet(net)
    track.SetLayer(layer)
    track.SetStart(point(*start))
    track.SetEnd(point(*end))
    track.SetWidth(mm(width))
    board.Add(track)


def add_via(board, net, at, diameter=0.60, drill=0.30):
    via = pcbnew.PCB_VIA(board)
    via.SetNet(net)
    via.SetPosition(point(*at))
    via.SetWidth(mm(diameter))
    via.SetDrill(mm(drill))
    via.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
    board.Add(via)


board = pcbnew.LoadBoard(str(SOURCE))
footprints = {fp.GetReference(): fp for fp in board.GetFootprints()}

# The C165948 manufacturer land pattern places B12 0.1847 mm from its NPTH.
# JLCPCB requires 0.20 mm. Narrowing this shield-side GND tail from 0.60 to
# 0.55 mm yields about 0.2097 mm while retaining ample copper for the contact.
j1 = footprints["J1"]
for pad in j1.Pads():
    if pad.GetNumber() == "B12":
        size = pad.GetSize()
        pad.SetSize(pcbnew.VECTOR2I(mm(0.55), size.y))

# Remove autorouter tail fragments reported as dangling. These are branches,
# not required links; connectivity is checked after the cleanup.
dangling_ends = [
    point(113.9530, 102.8590),
    point(144.7320, 104.0195),
    point(141.5065, 100.7940),
    point(122.5190, 111.7500),
]
for track in list(board.GetTracks()):
    if not isinstance(track, pcbnew.PCB_TRACK) or isinstance(track, pcbnew.PCB_VIA):
        continue
    if any(
        (track.GetStart() - p).EuclideanNorm() < mm(0.01)
        or (track.GetEnd() - p).EuclideanNorm() < mm(0.01)
        for p in dangling_ends
    ):
        board.Remove(track)

gnd = board.FindNet("/GND") or board.FindNet("GND")
if gnd is None:
    raise RuntimeError("GND net not found")

# U7 pad 3 can reach the component's own grounded exposed pad without leaving
# the footprint.  This is shorter and safer than adding another close-pitch via.
add_track(board, gnd, (126.10, 106.20), (127.025, 106.00), width=0.15)

# U4 pad 4 remains intentionally unresolved in this WIP.  BOOST_SW and
# +5V_MEDIA currently block every DRC-clean via escape and must be locally
# rerouted together before this board can be promoted to production.

# U7 exposed/PTH ground pad is intentionally tied solidly to the ground plane;
# its thermal performance is more important than hand-soldering convenience.
for pad in footprints["U7"].Pads():
    if pad.GetNumber() == "13":
        pad.SetLocalZoneConnection(pcbnew.ZONE_CONNECTION_FULL)

# Keep assembly references on fabrication drawings, while preventing the PCB
# silkscreen from being clipped over fine-pitch pads and adjacent components.
for fp in board.GetFootprints():
    fp.Reference().SetVisible(False)
    fp.Value().SetVisible(False)

filler = pcbnew.ZONE_FILLER(board)
filler.Fill(board.Zones())
pcbnew.SaveBoard(str(OUTPUT), board)
print(OUTPUT)
