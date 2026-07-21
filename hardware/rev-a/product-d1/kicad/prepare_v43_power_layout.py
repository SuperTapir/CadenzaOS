#!/usr/bin/env python3
"""Create the v43 power-placement candidate and full-routing DSN."""

from pathlib import Path
import pcbnew


ROOT = Path(__file__).resolve().parent / "production-v42-priority"
SOURCE = ROOT / "cadenza-d1-v42-priority-complete.kicad_pcb"
BOARD_OUT = ROOT / "cadenza-d1-v43-power-layout-base.kicad_pcb"
DSN_OUT = ROOT / "cadenza-d1-v43-power-layout.dsn"

PLACEMENT = {
    # TPS63070 local loop and feedback network.
    "L1": (128.0, 98.0),
    "C14": (128.0, 90.0),
    "C10": (124.0, 95.0),
    "C11": (124.0, 97.0),
    "C12": (123.0, 100.0),
    "C13": (120.0, 102.0),
    "R10": (132.0, 91.0),
    "R11": (132.0, 92.5),
    # TPS61023 input, switch loop, output, and feedback network.
    "L2": (136.0, 83.0),
    "C20": (129.0, 86.0),
    "C21": (136.0, 87.0),
    "C22": (139.0, 87.0),
    "R20": (129.0, 81.0),
    "R21": (129.0, 82.5),
    "R76": (142.0, 89.5),
    # Keep the filtered audio rail and its bulk capacitance at the amplifier.
    "FB2": (72.0, 98.0),
}

REROUTE_NETS = {
    "/+3V3_MAIN", "/+5V_AUDIO", "/+5V_MEDIA",
    "/BOOST_SW", "/FB_3V3", "/FB_5V", "/GND", "/L3V3_A",
    "/L3V3_B", "/PG_3V3_N", "/RUN_EN", "/VAUX_3V3", "/VSYS",
    "/I2C_SCL", "/I2C_SDA", "/AMP_EN",
}

BATTERY_NETS = {"/PACKP", "/VBAT"}
POWER_NETS = {
    "/+3V3_MAIN", "/+5V_AUDIO", "/+5V_LCD", "/+5V_MEDIA",
    "/BOOST_SW", "/GAUGE_VDD", "/L3V3_A", "/L3V3_B", "/SPK+",
    "/SPK-", "/VAUX_3V3", "/VBUS", "/VSYS",
}


def point(x: float, y: float) -> pcbnew.VECTOR2I:
    return pcbnew.VECTOR2I(pcbnew.FromMM(x), pcbnew.FromMM(y))


def quoted(nets: set[str]) -> str:
    return " ".join(f'"{net}"' for net in sorted(nets))


def replace_classes(dsn: str, all_nets: set[str]) -> str:
    signals = all_nets - BATTERY_NETS - POWER_NETS - {"/GND"}
    classes = f'''  (class BATTERY_MAIN {quoted(BATTERY_NETS)}
      (circuit (use_via "Via[0-3]_600:300_um"))
      (rule (width 1200) (clearance 150))
    )
    (class POWER_MAIN {quoted(POWER_NETS)}
      (circuit (use_via "Via[0-3]_600:300_um"))
      (rule (width 500) (clearance 150))
    )
    (class SIGNAL {quoted(signals)}
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
    all_nets = {net.GetNetname() for net in board.GetNetInfo().NetsByName().values()}

    for ref, (x, y) in PLACEMENT.items():
        footprint = board.FindFootprintByReference(ref)
        if footprint is None:
            raise RuntimeError(f"Missing footprint {ref}")
        footprint.SetPosition(point(x, y))

    removed = 0
    for item in list(board.GetTracks()):
        if item.GetNetname() in REROUTE_NETS:
            board.Remove(item)
            removed += 1
        else:
            item.SetLocked(True)

    board.BuildConnectivity()
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    pcbnew.SaveBoard(str(BOARD_OUT), board)
    if not pcbnew.ExportSpecctraDSN(board, str(DSN_OUT)):
        raise RuntimeError("Failed to export Specctra DSN")

    DSN_OUT.write_text(replace_classes(DSN_OUT.read_text(), all_nets))
    print(f"Moved {len(PLACEMENT)} power-related footprints")
    print(f"Removed {removed} unlocked affected tracks/vias")
    print(f"Wrote {BOARD_OUT}")
    print(f"Wrote {DSN_OUT}")


if __name__ == "__main__":
    main()
