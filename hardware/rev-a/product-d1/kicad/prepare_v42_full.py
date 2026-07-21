#!/usr/bin/env python3
"""Export the v42 critical-routed board for full autorouting."""

from __future__ import annotations

import json
from pathlib import Path

import pcbnew


ROOT = Path(__file__).parent / "production-v42-priority"
SOURCE = ROOT / "cadenza-d1-v42-priority-complete.kicad_pcb"
BOARD_OUT = ROOT / "cadenza-d1-v42-full-base.kicad_pcb"
DSN_OUT = ROOT / "cadenza-d1-v42-full.dsn"

BATTERY_MAIN = {"/PACKP", "/VBAT"}
POWER_MAIN = {
    "/+3V3_MAIN", "/+5V_AUDIO", "/+5V_LCD", "/+5V_MEDIA",
    "/BOOST_SW", "/GAUGE_VDD", "/L3V3_A", "/L3V3_B",
    "/SPK+", "/SPK-", "/VAUX_3V3", "/VBUS", "/VSYS",
}
GROUND = {"/GND"}


def q(name: str) -> str:
    return json.dumps(name, ensure_ascii=True)


def class_block(name: str, nets: set[str], width_um: int) -> str:
    members = " ".join(q(net) for net in sorted(nets))
    return f"""    (class {name} {members}
      (circuit
        (use_via \"Via[0-3]_600:300_um\")
      )
      (rule
        (width {width_um})
        (clearance 150)
      )
    )"""


def replace_default_class(dsn: str, all_nets: set[str]) -> str:
    marker = "    (class kicad_default "
    start = dsn.index(marker)
    depth = 0
    end = None
    for index in range(start, len(dsn)):
        if dsn[index] == "(":
            depth += 1
        elif dsn[index] == ")":
            depth -= 1
            if depth == 0:
                end = index + 1
                break
    if end is None:
        raise ValueError("unterminated kicad_default class")

    signal = all_nets - BATTERY_MAIN - POWER_MAIN - GROUND
    classes = "\n".join([
        class_block("BATTERY_MAIN", BATTERY_MAIN, 1200),
        class_block("POWER_MAIN", POWER_MAIN, 500),
        class_block("SIGNAL", signal, 200),
        class_block("GROUND", GROUND, 300),
    ])
    return dsn[:start] + classes + dsn[end:]


def main() -> None:
    board = pcbnew.LoadBoard(str(SOURCE))
    removed_degenerate = 0
    for item in list(board.GetTracks()):
        if item.Type() != pcbnew.PCB_VIA_T:
            a, b = item.GetStart(), item.GetEnd()
            if a == b or pcbnew.ToMM((a - b).EuclideanNorm()) < 0.01:
                board.Delete(item)
                removed_degenerate += 1
                continue
        item.SetLocked(True)
    pcbnew.SaveBoard(str(BOARD_OUT), board)
    if not pcbnew.ExportSpecctraDSN(board, str(DSN_OUT)):
        raise RuntimeError("KiCad failed to export full-routing DSN")

    all_nets = {
        net.GetNetname()
        for net in board.GetNetInfo().NetsByName().values()
        if net.GetNetname()
    }
    DSN_OUT.write_text(
        replace_default_class(DSN_OUT.read_text(encoding="utf-8"), all_nets),
        encoding="utf-8",
    )
    print(f"Removed {removed_degenerate} degenerate track segments")
    print(f"Locked {len(board.GetTracks())} critical/protected track and via items")
    print(BOARD_OUT)
    print(DSN_OUT)


if __name__ == "__main__":
    main()
