#!/usr/bin/env python3
"""Complete the two residual v43b signal islands without disturbing routing."""

from pathlib import Path
import pcbnew


ROOT = Path(__file__).resolve().parent / "production-v42-priority"
SOURCE = ROOT / "cadenza-d1-v43b-routed.kicad_pcb"
OUTPUT = ROOT / "cadenza-d1-v43b-local-candidate.kicad_pcb"


def pt(x: float, y: float) -> pcbnew.VECTOR2I:
    return pcbnew.VECTOR2I(pcbnew.FromMM(x), pcbnew.FromMM(y))


def track(board, net, layer: str, width: float, points: list[tuple[float, float]]) -> None:
    for a, b in zip(points, points[1:]):
        item = pcbnew.PCB_TRACK(board)
        item.SetNet(net)
        item.SetLayer(board.GetLayerID(layer))
        item.SetWidth(pcbnew.FromMM(width))
        item.SetStart(pt(*a))
        item.SetEnd(pt(*b))
        board.Add(item)


def via(board, net, at: tuple[float, float]) -> None:
    item = pcbnew.PCB_VIA(board)
    item.SetNet(net)
    item.SetPosition(pt(*at))
    item.SetWidth(pcbnew.FromMM(0.6))
    item.SetDrill(pcbnew.FromMM(0.3))
    item.SetLayerPair(board.GetLayerID("F.Cu"), board.GetLayerID("B.Cu"))
    board.Add(item)


def main() -> None:
    board = pcbnew.LoadBoard(str(SOURCE))
    vaux_net = board.GetNetInfo().GetNetItem("/VAUX_3V3")
    vbat_net = board.GetNetInfo().GetNetItem("/VBAT")
    bat_ts_net = board.GetNetInfo().GetNetItem("/BAT_TS")

    old_pg = pt(127.7286, 92.2830)
    for item in list(board.GetTracks()):
        if (item.GetNetname() == "/PG_3V3_N"
                and isinstance(item, pcbnew.PCB_VIA)
                and item.GetPosition() == old_pg):
            item.SetWidth(pcbnew.FromMM(0.5))
            item.SetDrill(pcbnew.FromMM(0.25))
        elif item.GetNetname() == "/BAT_TS" and not isinstance(item, pcbnew.PCB_VIA):
            a, b = item.GetStart(), item.GetEnd()
            left, right = pt(136.125, 102.25), pt(141.5375, 102.25)
            if (a == left and b == right) or (a == right and b == left):
                board.Remove(item)

    track(board, vaux_net, "B.Cu", 0.2,
          [(127.225, 90.0), (128.25, 91.025), (128.25, 93.04)])

    track(board, vbat_net, "B.Cu", 0.5,
          [(138.225, 100.8), (137.2, 100.8), (138.5, 105.0)])

    track(board, bat_ts_net, "B.Cu", 0.2, [(141.5375, 102.25), (140.8, 102.25)])
    via(board, bat_ts_net, (140.8, 102.25))
    track(board, bat_ts_net, "F.Cu", 0.2,
          [(140.8, 102.25), (140.8, 103.5), (135.0, 103.5), (135.0, 102.25)])
    via(board, bat_ts_net, (135.0, 102.25))
    track(board, bat_ts_net, "B.Cu", 0.2, [(135.0, 102.25), (136.125, 102.25)])

    board.BuildConnectivity()
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    pcbnew.SaveBoard(str(OUTPUT), board)
    print(f"Wrote {OUTPUT}")


if __name__ == "__main__":
    main()
