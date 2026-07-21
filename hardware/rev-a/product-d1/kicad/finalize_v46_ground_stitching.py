#!/usr/bin/env python3
"""Close every B.Cu ground island before rerouting three low-speed controls."""

from pathlib import Path
import pcbnew


ROOT = Path(__file__).resolve().parent
SOURCE = ROOT / "production-v42-priority" / "cadenza-d1-v45b-pg-rerouted.kicad_pcb"
OUTPUT = ROOT / "production-v42-priority" / "cadenza-d1-v46-controls-reroute-base.kicad_pcb"


def mm(value: float) -> int:
    return pcbnew.FromMM(value)


def point(x: float, y: float) -> pcbnew.VECTOR2I:
    return pcbnew.VECTOR2I(mm(x), mm(y))


def add_track(board, net, start, end, width=0.15, locked=True):
    item = pcbnew.PCB_TRACK(board)
    item.SetNet(net)
    item.SetLayer(pcbnew.B_Cu)
    item.SetStart(point(*start))
    item.SetEnd(point(*end))
    item.SetWidth(mm(width))
    item.SetLocked(locked)
    board.Add(item)


def add_via(board, net, at, locked=True):
    item = pcbnew.PCB_VIA(board)
    item.SetNet(net)
    item.SetPosition(point(*at))
    item.SetWidth(mm(0.50))
    item.SetDrill(mm(0.25))
    item.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
    item.SetLocked(locked)
    board.Add(item)


board = pcbnew.LoadBoard(str(SOURCE))
gnd = board.FindNet("/GND") or board.FindNet("GND")
original_tracks = list(board.GetTracks())

# Six B.Cu islands have direct, all-layer DRC-clean stitching locations.
for at in [
    (111.50, 106.50),
    (111.50, 103.00),
    (139.50, 102.50),
    (133.50, 101.00),
    (85.00, 69.50),
    (139.50, 68.50),
]:
    add_via(board, gnd, at)

# U3 pin 15 reaches the main ground copper inside the regulator footprint,
# avoiding a via through the dense POWER-layer bus crossing this location.
add_track(board, gnd, (126.60, 93.46), (127.20, 93.72))

# R7/C1 ground reaches the adjacent main B.Cu ground region.  The two old
# charger-set traces crossing this short bridge are rerouted in the next stage.
add_track(board, gnd, (136.325, 100.80), (136.30, 99.20))

# R21's small ground island has one through-board site; its only conflict is
# the low-speed B button trace, also rerouted in the next stage.
add_via(board, gnd, (129.75, 82.75))

# Remove the three low-speed routes that cross the new, intentional ground
# topology.  All other nets remain untouched and will be locked in the DSN.
reroute_names = {"/B_SW", "/ISET_SET", "/ITERM_SET"}
reroute_codes = {
    board.FindNet(name).GetNetCode()
    for name in reroute_names
    if board.FindNet(name) is not None
}
# SES imports can reintroduce branch stubs from locked nets.  Remove the four
# known free endpoints together with the reroute nets in one container pass.
dangling_ends = [
    point(113.9530, 102.8590),
    point(117.9185, 102.7310),
    point(144.7320, 104.0195),
    point(141.5065, 100.7940),
]
for item in original_tracks:
    is_target = item.GetNetCode() in reroute_codes
    is_dangling_stub = (
        isinstance(item, pcbnew.PCB_TRACK)
        and not isinstance(item, pcbnew.PCB_VIA)
        and any(
            (item.GetStart() - p).EuclideanNorm() < mm(0.01)
            or (item.GetEnd() - p).EuclideanNorm() < mm(0.01)
            for p in dangling_ends
        )
    )
    if is_target or is_dangling_stub:
        board.Remove(item)

pcbnew.ZONE_FILLER(board).Fill(board.Zones())
pcbnew.SaveBoard(str(OUTPUT), board)
print(OUTPUT)
