#!/usr/bin/env python3
"""Prepare the second priority batch after v42 critical routing."""

from __future__ import annotations

import json
from pathlib import Path

import pcbnew


ROOT = Path(__file__).parent / "production-v42-priority"
SOURCE = ROOT / "cadenza-d1-v42-critical-complete.kicad_pcb"
BOARD_OUT = ROOT / "cadenza-d1-v42-stage2-base.kicad_pcb"
DSN_OUT = ROOT / "cadenza-d1-v42-stage2.dsn"

NEXT_POWER = {"/+3V3_MAIN", "/VBUS", "/PACKP", "/GAUGE_VDD"}
NEXT_SIGNAL = {
    "/RUN_EN", "/ITERM_SET", "/CHG_N", "/I2C_SDA", "/GAUGE_INT_N",
    "/I2S_LRCLK", "/PGOOD_N", "/FB_3V3", "/ILIM_SET", "/TMR_SET",
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


def patch_classes(dsn: str, all_nets: set[str]) -> str:
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
    deferred = all_nets - NEXT_POWER - NEXT_SIGNAL - GROUND
    classes = "\n".join([
        class_block("NEXT_POWER", NEXT_POWER, 500),
        class_block("NEXT_SIGNAL", NEXT_SIGNAL, 200),
        class_block("DEFERRED", deferred, 200),
        class_block("GROUND", GROUND, 300),
    ])
    return dsn[:start] + classes + dsn[end:]


def main():
    board = pcbnew.LoadBoard(str(SOURCE))
    for item in board.GetTracks():
        item.SetLocked(True)
    pcbnew.SaveBoard(str(BOARD_OUT), board)
    if not pcbnew.ExportSpecctraDSN(board, str(DSN_OUT)):
        raise RuntimeError("KiCad failed to export stage-2 DSN")
    all_nets = {
        net.GetNetname()
        for net in board.GetNetInfo().NetsByName().values()
        if net.GetNetname()
    }
    DSN_OUT.write_text(patch_classes(DSN_OUT.read_text(encoding="utf-8"), all_nets), encoding="utf-8")
    print(BOARD_OUT)
    print(DSN_OUT)


if __name__ == "__main__":
    main()
