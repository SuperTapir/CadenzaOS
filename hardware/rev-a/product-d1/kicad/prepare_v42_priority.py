#!/usr/bin/env python3
"""Build a clean v42 board/DSN for critical-net-first autorouting."""

from __future__ import annotations

import json
from pathlib import Path

import pcbnew


ROOT = Path(__file__).parent / "production-v42-priority"
SOURCE = Path(__file__).parent / "production-v41-electrical-audit" / "cadenza-d1-v41.kicad_pcb"
BOARD_OUT = ROOT / "cadenza-d1-v42-priority-base.kicad_pcb"
DSN_OUT = ROOT / "cadenza-d1-v42-priority.dsn"

CRITICAL_CONTROL = {
    "/LCD_SCS", "/LCD_SCLK", "/LCD_SI", "/LCD_DISP", "/LCD_EXTCOMIN", "/LCD_EXTMODE",
    "/I2S_BCLK", "/I2S_LRCLK", "/I2S_DIN", "/AMP_EN",
    "/BAT_TS", "/ITERM_SET", "/CHG_N", "/PGOOD_N", "/ILIM_SET", "/ISET_SET",
    "/TMR_SET", "/RUN_EN",
    "/GAUGE_BIN", "/GAUGE_INT_N", "/I2C_SDA", "/I2C_SCL",
}
CRITICAL_POWER = {"/VSYS", "/VBUS", "/GAUGE_VDD", "/+3V3_MAIN", "/PACKP"}
GROUND = {"/GND"}

# These routes were intentionally designed before autorouting and must survive.
PROTECTED = {
    "/PACKP", "/VBAT",
    "/USB_D+", "/USB_D-", "/USB_D+_CONN", "/USB_D-_CONN",
}


def q(name: str) -> str:
    return json.dumps(name, ensure_ascii=True)


def class_block(name: str, nets: set[str], width_um: int = 200) -> str:
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

    deferred = all_nets - CRITICAL_CONTROL - CRITICAL_POWER - GROUND
    classes = "\n".join([
        class_block("CRITICAL_CONTROL", CRITICAL_CONTROL),
        class_block("CRITICAL_POWER", CRITICAL_POWER, width_um=500),
        class_block("DEFERRED", deferred),
        class_block("GROUND", GROUND),
    ])
    return dsn[:start] + classes + dsn[end:]


def main() -> None:
    ROOT.mkdir(parents=True, exist_ok=True)
    board = pcbnew.LoadBoard(str(SOURCE))

    # R72 was trapped between U7/R73 and the PACKP current path in v41.
    # Move only this low-current BIN configuration resistor into the open area
    # left of the gauge; preserve U7, R73, and all Kelvin/current geometry.
    r72 = board.FindFootprintByReference("R72")
    if r72 is None:
        raise RuntimeError("R72 footprint not found")
    r72.SetPosition(pcbnew.VECTOR2I(pcbnew.FromMM(118.0), pcbnew.FromMM(108.0)))

    # Shift only the narrow PACKP Kelvin branch that sealed U7 pin 10's
    # right-side escape. The 1.5 mm PACKP main-current path is untouched.
    old_kelvin = {
        tuple(sorted(((129.9, 105.4), (130.5, 105.4)))),
        tuple(sorted(((130.5, 105.4), (130.5, 108.5)))),
        tuple(sorted(((130.5, 108.5), (125.0, 111.0)))),
    }
    removed_kelvin = 0
    for item in list(board.GetTracks()):
        if item.GetNetname() != "/PACKP" or item.Type() == pcbnew.PCB_VIA_T:
            continue
        a, b = item.GetStart(), item.GetEnd()
        key = tuple(sorted(((round(pcbnew.ToMM(a.x), 1), round(pcbnew.ToMM(a.y), 1)),
                            (round(pcbnew.ToMM(b.x), 1), round(pcbnew.ToMM(b.y), 1)))))
        if key in old_kelvin:
            board.Delete(item)
            removed_kelvin += 1
    if removed_kelvin != 3:
        raise RuntimeError(f"expected 3 PACKP Kelvin segments, removed {removed_kelvin}")

    packp = board.FindNet("/PACKP")
    for a, b in [
        ((129.9, 105.4), (131.5, 105.4)),
        ((131.5, 105.4), (131.5, 108.5)),
        ((131.5, 108.5), (125.0, 111.0)),
    ]:
        track = pcbnew.PCB_TRACK(board)
        track.SetStart(pcbnew.VECTOR2I(pcbnew.FromMM(a[0]), pcbnew.FromMM(a[1])))
        track.SetEnd(pcbnew.VECTOR2I(pcbnew.FromMM(b[0]), pcbnew.FromMM(b[1])))
        track.SetWidth(pcbnew.FromMM(0.15))
        track.SetLayer(pcbnew.B_Cu)
        track.SetNet(packp)
        track.SetLocked(True)
        board.Add(track)

    removed = 0
    for item in list(board.GetTracks()):
        if item.GetNetname() not in PROTECTED:
            board.Delete(item)
            removed += 1
        else:
            item.SetLocked(True)

    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    pcbnew.SaveBoard(str(BOARD_OUT), board)
    if not pcbnew.ExportSpecctraDSN(board, str(DSN_OUT)):
        raise RuntimeError("KiCad failed to export priority DSN")

    all_nets = {
        net.GetNetname()
        for net in board.GetNetInfo().NetsByName().values()
        if net.GetNetname()
    }
    DSN_OUT.write_text(
        replace_default_class(DSN_OUT.read_text(encoding="utf-8"), all_nets),
        encoding="utf-8",
    )
    print(f"Removed {removed} non-critical track/via items")
    print(BOARD_OUT)
    print(DSN_OUT)


if __name__ == "__main__":
    main()
