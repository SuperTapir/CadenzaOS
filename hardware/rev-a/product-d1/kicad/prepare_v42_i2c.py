#!/usr/bin/env python3
"""Prepare an I2C-only retry after fixing the U7 GAUGE_VDD escape."""

from __future__ import annotations

import json
from pathlib import Path

import pcbnew


ROOT = Path(__file__).parent / "production-v42-priority"
SOURCE = ROOT / "cadenza-d1-v42-expanded-gauge-base.kicad_pcb"
BOARD_OUT = ROOT / "cadenza-d1-v42-i2c-base.kicad_pcb"
DSN_OUT = ROOT / "cadenza-d1-v42-i2c.dsn"
I2C = {"/I2C_SDA", "/I2C_SCL"}
GROUND = {"/GND"}


def q(name: str) -> str:
    return json.dumps(name, ensure_ascii=True)


def block(name: str, nets: set[str]) -> str:
    members = " ".join(q(net) for net in sorted(nets))
    return f"""    (class {name} {members}
      (circuit (use_via \"Via[0-3]_600:300_um\"))
      (rule (width 200) (clearance 150))
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
    deferred = all_nets - I2C - GROUND
    classes = "\n".join([block("I2C", I2C), block("DEFERRED", deferred), block("GROUND", GROUND)])
    return dsn[:start] + classes + dsn[end:]


def main():
    board = pcbnew.LoadBoard(str(SOURCE))
    for item in board.GetTracks():
        item.SetLocked(True)
    pcbnew.SaveBoard(str(BOARD_OUT), board)
    if not pcbnew.ExportSpecctraDSN(board, str(DSN_OUT)):
        raise RuntimeError("KiCad failed to export I2C DSN")
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
