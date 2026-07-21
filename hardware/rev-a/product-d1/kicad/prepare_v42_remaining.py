#!/usr/bin/env python3
"""Prepare a locked-board DSN that routes only the remaining non-GND nets."""

from pathlib import Path
import os
import pcbnew


ROOT = Path(__file__).resolve().parent / "production-v42-priority"
SOURCE = Path(os.environ.get("CADENZA_SOURCE", ROOT / "cadenza-d1-v42-full-routed-final.kicad_pcb"))
BOARD_OUT = Path(os.environ.get("CADENZA_BOARD_OUT", ROOT / "cadenza-d1-v42-remaining-base.kicad_pcb"))
DSN_OUT = Path(os.environ.get("CADENZA_DSN_OUT", ROOT / "cadenza-d1-v42-remaining.dsn"))

TARGET_SIGNAL = {
    "/SD_CD",
    "/USB_D+",
    "/PG_3V3_N",
    "/FB_3V3",
}

TARGET_POWER = {
    "/+5V_MEDIA",
    "/VAUX_3V3",
    "/L3V3_A",
    "/L3V3_B",
    "/VBAT",
    "/BOOST_SW",
    "/+5V_AUDIO",
    "/SPK+",
}

if os.environ.get("CADENZA_TARGET_NETS"):
    selected = {net.strip() for net in os.environ["CADENZA_TARGET_NETS"].split(",") if net.strip()}
    TARGET_SIGNAL &= selected
    TARGET_POWER &= selected


def quoted(nets: set[str]) -> str:
    return " ".join(f'"{net}"' for net in sorted(nets))


def replace_classes(dsn: str, all_nets: set[str]) -> str:
    deferred = all_nets - TARGET_SIGNAL - TARGET_POWER - {"/GND"}
    classes = f'''  (class TARGET_SIGNAL {quoted(TARGET_SIGNAL)}
      (circuit (use_via "Via[0-3]_600:300_um"))
      (rule (width 200) (clearance 150))
    )
    (class TARGET_POWER {quoted(TARGET_POWER)}
      (circuit (use_via "Via[0-3]_600:300_um"))
      (rule (width 500) (clearance 150))
    )
    (class DEFERRED {quoted(deferred)}
      (circuit (use_via "Via[0-3]_600:300_um"))
      (rule (width 200) (clearance 150))
    )
    (class GROUND "/GND"
      (circuit (use_via "Via[0-3]_600:300_um"))
      (rule (width 300) (clearance 150))
    )
'''
    start = dsn.index("  (class ")
    end = dsn.index("  )\n  (wiring", start)
    return dsn[:start] + classes + dsn[end:]


def main() -> None:
    board = pcbnew.LoadBoard(str(SOURCE))
    targets = TARGET_SIGNAL | TARGET_POWER
    removed = 0

    # The imported full-route tracks are unlocked. Remove only incomplete target
    # fragments while preserving the locked, hand-audited priority routing.
    tracks = list(board.GetTracks())
    for item in tracks:
        degenerate = not isinstance(item, pcbnew.PCB_VIA) and pcbnew.ToMM(item.GetLength()) < 0.01
        if degenerate or (item.GetNetname() in targets and not item.IsLocked()):
            board.Remove(item)
            removed += 1
        else:
            item.SetLocked(True)

    pcbnew.SaveBoard(str(BOARD_OUT), board)
    if not pcbnew.ExportSpecctraDSN(board, str(DSN_OUT)):
        raise RuntimeError("Failed to export Specctra DSN")

    all_nets = {net.GetNetname() for net in board.GetNetInfo().NetsByName().values()}
    DSN_OUT.write_text(replace_classes(DSN_OUT.read_text(), all_nets))
    print(f"Removed {removed} incomplete target tracks/vias")
    print(f"Wrote {BOARD_OUT}")
    print(f"Wrote {DSN_OUT}")


if __name__ == "__main__":
    main()
