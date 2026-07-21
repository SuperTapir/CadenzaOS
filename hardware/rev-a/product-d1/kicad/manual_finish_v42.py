#!/usr/bin/env python3
"""Add auditable escape/bridge routes for the v42 residual ratlines."""

from pathlib import Path
import pcbnew


ROOT = Path(__file__).resolve().parent / "production-v42-priority"
SOURCE = ROOT / "cadenza-d1-v42-signal-complete.kicad_pcb"
OUTPUT = ROOT / "cadenza-d1-v42-manual-candidate.kicad_pcb"


def p(x: float, y: float) -> pcbnew.VECTOR2I:
    return pcbnew.VECTOR2I(pcbnew.FromMM(x), pcbnew.FromMM(y))


def add_track(board, net_name: str, layer: str, width: float, points: list[tuple[float, float]]) -> None:
    net = board.GetNetInfo().GetNetItem(net_name)
    layer_id = board.GetLayerID(layer)
    for start, end in zip(points, points[1:]):
        track = pcbnew.PCB_TRACK(board)
        track.SetNet(net)
        track.SetLayer(layer_id)
        track.SetWidth(pcbnew.FromMM(width))
        track.SetStart(p(*start))
        track.SetEnd(p(*end))
        board.Add(track)


def add_via(board, net_name: str, at: tuple[float, float]) -> None:
    via = pcbnew.PCB_VIA(board)
    via.SetNet(board.GetNetInfo().GetNetItem(net_name))
    via.SetPosition(p(*at))
    via.SetWidth(pcbnew.FromMM(0.6))
    via.SetDrill(pcbnew.FromMM(0.3))
    via.SetLayerPair(board.GetLayerID("F.Cu"), board.GetLayerID("B.Cu"))
    board.Add(via)


def main() -> None:
    board = pcbnew.LoadBoard(str(SOURCE))

    # SD card detect: escape U1 on B.Cu, then meet the existing POWER-layer island.
    add_track(board, "/SD_CD", "B.Cu", 0.2, [(146.555, 86.5), (147.3, 86.5)])
    add_via(board, "/SD_CD", (147.3, 86.5))
    add_track(board, "/SD_CD", "POWER", 0.2, [(147.3, 86.5), (152.0993, 91.079)])

    # Media rail: U4 pad 6 faces left, directly bridge to the existing local island.
    add_track(board, "/+5V_MEDIA", "B.Cu", 0.5,
              [(134.74, 79.5), (134.0, 79.5), (130.4, 83.0963), (128.2125, 83.0963)])

    # U3 regulator local escape routes.
    add_track(board, "/VAUX_3V3", "B.Cu", 0.5,
              [(128.25, 93.04), (128.25, 92.2), (126.0, 89.95), (122.225, 88.0)])
    add_track(board, "/L3V3_A", "B.Cu", 0.5,
              [(127.5, 94.96), (127.5, 96.1), (130.6, 96.1), (130.6, 94.0)])
    add_track(board, "/L3V3_B", "B.Cu", 0.5,
              [(128.5, 94.96), (128.5, 97.0), (133.4, 97.0), (133.4, 94.0)])

    add_track(board, "/PG_3V3_N", "B.Cu", 0.2, [(127.75, 93.04), (127.75, 92.3)])
    add_via(board, "/PG_3V3_N", (127.75, 92.3))
    add_track(board, "/PG_3V3_N", "F.Cu", 0.2, [(127.75, 92.3), (136.7248, 87.0498)])
    add_via(board, "/PG_3V3_N", (136.7248, 87.0498))

    add_track(board, "/FB_3V3", "B.Cu", 0.2, [(129.4, 93.46), (130.2, 93.46)])
    add_via(board, "/FB_3V3", (130.2, 93.46))
    add_track(board, "/FB_3V3", "F.Cu", 0.2, [(130.2, 93.46), (117.4463, 92.2362)])

    # Local battery and audio discontinuities.
    add_track(board, "/VBAT", "B.Cu", 0.5,
              [(138.225, 100.8), (138.225, 102.0), (138.5, 102.275), (138.5, 105.0)])
    add_track(board, "/+5V_AUDIO", "B.Cu", 0.5,
              [(64.75, 95.4375), (65.2, 95.4375), (68.9, 95.6), (69.225, 95.6)])

    add_track(board, "/SPK+", "B.Cu", 0.5, [(65.4375, 94.75), (66.2, 94.75)])
    add_via(board, "/SPK+", (66.2, 94.75))
    add_track(board, "/SPK+", "F.Cu", 0.5, [(66.2, 94.75), (66.2, 102.8), (61.8, 105.7)])
    add_via(board, "/SPK+", (61.8, 105.7))
    add_track(board, "/SPK+", "B.Cu", 0.5, [(61.8, 105.7), (61.0, 106.5)])

    board.BuildConnectivity()
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    pcbnew.SaveBoard(str(OUTPUT), board)
    print(f"Wrote {OUTPUT}")


if __name__ == "__main__":
    main()
