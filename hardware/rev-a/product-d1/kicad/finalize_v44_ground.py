#!/usr/bin/env python3
"""Finalize the D1 v44 local power reroute and explicit ground escapes."""

from pathlib import Path
import pcbnew


ROOT = Path(__file__).resolve().parent
SOURCE = ROOT / "production-v42-priority" / "cadenza-d1-v44-u4-rerouted.kicad_pcb"
OUTPUT = ROOT / "production-v42-priority" / "cadenza-d1-v45-pg-reroute-base.kicad_pcb"


def mm(value: float) -> int:
    return pcbnew.FromMM(value)


def point(x: float, y: float) -> pcbnew.VECTOR2I:
    return pcbnew.VECTOR2I(mm(x), mm(y))


board = pcbnew.LoadBoard(str(SOURCE))
gnd = board.FindNet("/GND") or board.FindNet("GND")
if gnd is None:
    raise RuntimeError("GND net not found")

# U4 pin 4 ground escape.  The location was selected with a 0.10 mm search
# grid and validated using the board's 0.15 mm copper and 0.25 mm hole rules.
track = pcbnew.PCB_TRACK(board)
track.SetNet(gnd)
track.SetLayer(pcbnew.B_Cu)
track.SetStart(point(134.74, 80.50))
track.SetEnd(point(135.65, 80.65))
track.SetWidth(mm(0.20))
board.Add(track)

via = pcbnew.PCB_VIA(board)
via.SetNet(gnd)
via.SetPosition(point(135.65, 80.65))
via.SetWidth(mm(0.50))
via.SetDrill(mm(0.20))
via.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
board.Add(via)

# The C165948 outer ground contacts sit diagonally beside its two 0.60 mm
# positioning holes.  Reduce only these four mechanical-side ground lands to
# 0.55 x 1.15 mm so NPTH-to-copper clearance exceeds JLCPCB's 0.20 mm rule.
j1 = next(fp for fp in board.GetFootprints() if fp.GetReference() == "J1")
for pad in j1.Pads():
    if pad.GetNumber() in {"A1", "A12", "B1", "B12"}:
        pad.SetSize(pcbnew.VECTOR2I(mm(0.55), mm(1.15)))

# Freerouting 1.9 left the C22 output branch open.  Route the two 5 V capacitor
# pads above the alternating ground pad, keeping the output loop compact.
media_5v = board.FindNet("/+5V_MEDIA")
for start, end in [
    ((135.225, 87.00), (135.225, 85.90)),
    ((135.225, 85.90), (138.225, 85.90)),
    ((138.225, 85.90), (138.225, 87.00)),
]:
    item = pcbnew.PCB_TRACK(board)
    item.SetNet(media_5v)
    item.SetLayer(pcbnew.B_Cu)
    item.SetStart(point(*start))
    item.SetEnd(point(*end))
    item.SetWidth(mm(0.50))
    board.Add(item)

# Mirror the capacitor connection below the parts for their ground terminals,
# then stitch that short return directly into the ground plane.
for start, end in [
    ((136.775, 87.00), (136.775, 88.50)),
    ((136.775, 88.50), (139.775, 88.50)),
    ((139.775, 88.50), (139.775, 87.00)),
]:
    item = pcbnew.PCB_TRACK(board)
    item.SetNet(gnd)
    item.SetLayer(pcbnew.B_Cu)
    item.SetStart(point(*start))
    item.SetEnd(point(*end))
    item.SetWidth(mm(0.50))
    board.Add(item)

cap_gnd_via = pcbnew.PCB_VIA(board)
cap_gnd_via.SetNet(gnd)
cap_gnd_via.SetPosition(point(138.00, 88.50))
cap_gnd_via.SetWidth(mm(0.50))
cap_gnd_via.SetDrill(mm(0.20))
cap_gnd_via.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
board.Add(cap_gnd_via)

# Remove autorouter branches whose free endpoints are reported as dangling.
dangling_ends = [
    point(113.9530, 102.8590),  # USB_D+_CONN degenerate segment
    point(117.9185, 102.7310),  # USB_D+ branch stub
    point(144.7320, 104.0195),  # CHG_N branch stub
    point(141.5065, 100.7940),  # ISET_SET branch stub
]
for item in list(board.GetTracks()):
    if not isinstance(item, pcbnew.PCB_TRACK) or isinstance(item, pcbnew.PCB_VIA):
        continue
    if any(
        (item.GetStart() - p).EuclideanNorm() < mm(0.01)
        or (item.GetEnd() - p).EuclideanNorm() < mm(0.01)
        for p in dangling_ends
    ):
        board.Remove(item)

# The new compact capacitor return crosses the old low-speed PG route.  Remove
# PG here; the next deterministic stage routes only this net with all others
# locked.
pg = board.FindNet("/PG_3V3_N")
for item in list(board.GetTracks()):
    if item.GetNetCode() == pg.GetNetCode():
        board.Remove(item)

# Give U3's bottom-side PG pad a DRC-clean, locked escape to a through via.
# Freerouting then sees this via as the endpoint instead of failing to change
# layers at the fine-pitch pad.
pg_escape = pcbnew.PCB_TRACK(board)
pg_escape.SetNet(pg)
pg_escape.SetLayer(pcbnew.B_Cu)
pg_escape.SetStart(point(127.75, 93.04))
pg_escape.SetEnd(point(127.60, 92.30))
pg_escape.SetWidth(mm(0.20))
pg_escape.SetLocked(True)
board.Add(pg_escape)

pg_escape_via = pcbnew.PCB_VIA(board)
pg_escape_via.SetNet(pg)
pg_escape_via.SetPosition(point(127.60, 92.30))
pg_escape_via.SetWidth(mm(0.50))
pg_escape_via.SetDrill(mm(0.25))
pg_escape_via.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
pg_escape_via.SetLocked(True)
board.Add(pg_escape_via)

pcbnew.ZONE_FILLER(board).Fill(board.Zones())
pcbnew.SaveBoard(str(OUTPUT), board)
print(OUTPUT)
