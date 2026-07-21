#!/usr/bin/env python3
"""Derive the electrically corrected D1 v41 PCB from the mechanically valid v40.

This script intentionally fixes topology and placement before autorouting.  The
PACKP/VBAT current path and the BQ27441 Kelvin connections are routed here and
must not be replaced by the autorouter.
"""

from pathlib import Path

import pcbnew


ROOT = Path(__file__).resolve().parent
SOURCE = ROOT / "production-v40-final" / "cadenza-d1-v40.kicad_pcb"
OUT_DIR = ROOT / "production-v41-electrical-audit"
OUTPUT = OUT_DIR / "cadenza-d1-v41.kicad_pcb"
KICAD_FP = Path("/Users/tapir/Applications/KiCad/KiCad.app/Contents/SharedSupport/footprints")

MM = 1_000_000


def pos(x, y):
    return pcbnew.VECTOR2I(int(round(x * MM)), int(round(y * MM)))


board = pcbnew.LoadBoard(str(SOURCE))
OUT_DIR.mkdir(parents=True, exist_ok=True)
footprints = {item.GetReference(): item for item in board.GetFootprints()}


def fp(ref):
    found = footprints.get(ref)
    if found is None:
        raise RuntimeError(f"missing footprint {ref}")
    return found


def pad(ref, number):
    found = next((item for item in fp(ref).Pads() if item.GetNumber() == str(number)), None)
    if found is None:
        raise RuntimeError(f"missing pad {ref}.{number}")
    return found


def net(name):
    found = board.GetNetInfo().GetNetItem(name)
    if found is None:
        found = pcbnew.NETINFO_ITEM(board, name)
        board.Add(found)
    return found


def set_pad(ref, number, net_name):
    item = pad(ref, number)
    if net_name is None:
        item.SetNetCode(0)
    else:
        item.SetNet(net(net_name))


def remove_routes(net_names):
    doomed = [item for item in board.GetTracks() if item.GetNetname() in net_names]
    for item in doomed:
        board.Delete(item)


def clone_footprint(template_ref, ref, value, x, y, rotation=0, layer=pcbnew.B_Cu):
    item = pcbnew.FOOTPRINT.Cast(fp(template_ref).Duplicate(False))
    item.SetReference(ref)
    item.SetValue(value)
    item.SetPosition(pos(x, y))
    item.SetOrientationDegrees(rotation)
    board.Add(item)
    if item.GetLayer() != layer:
        item.Flip(item.GetPosition(), False)
    footprints[ref] = item
    return item


def library_footprint(library, name, ref, value, x, y, rotation=0, layer=pcbnew.B_Cu):
    item = pcbnew.FootprintLoad(str(KICAD_FP / f"{library}.pretty"), name)
    if item is None:
        raise RuntimeError(f"cannot load {library}:{name}")
    item.SetReference(ref)
    item.SetValue(value)
    item.SetPosition(pos(x, y))
    item.SetOrientationDegrees(rotation)
    board.Add(item)
    if item.GetLayer() != layer:
        item.Flip(item.GetPosition(), False)
    footprints[ref] = item
    return item


def assign_two_pin(ref, first, second):
    set_pad(ref, 1, first)
    set_pad(ref, 2, second)


def track(net_name, points, width, layer=pcbnew.B_Cu):
    for start, end in zip(points, points[1:]):
        item = pcbnew.PCB_TRACK(board)
        item.SetStart(pos(*start))
        item.SetEnd(pos(*end))
        item.SetWidth(int(round(width * MM)))
        item.SetLayer(layer)
        item.SetNet(net(net_name))
        board.Add(item)


# Remove routes whose endpoint ownership changes in v41.  Other v40 routing is
# retained; remaining ratlines are handled after topology migration.
remove_routes({
    "/VBAT", "/VBAT_SENSE", "/GAUGE_SRP", "/GAUGE_SRN",
    "/GAUGE_VDD", "/GAUGE_BIN", "/GAUGE_INT_N", "/I2C_SCL",
    "/I2S_BCLK", "/SD_CD", "/I2C_SDA", "/GND", "/+3V3_MAIN",
    "/VSYS", "/VBUS", "/USB_D-", "/USB_D+", "/LCD_SCS", "/SD_CMD", "/SD_CS",
    "/SD_CLK", "/BOOT_N", "/ESP_EN", "/PDM_DATA", "/CHG_N", "/PGOOD_N",
    "/BAT_TS",
})

# ESP32-S3: move product signals away from strapping GPIO45/GPIO46.
set_pad("U1", 9, "/I2S_BCLK")      # GPIO16
set_pad("U1", 16, None)             # GPIO46 strap remains unused
set_pad("U1", 17, "/SD_CD")        # GPIO9
set_pad("U1", 23, "/GAUGE_INT_N")  # GPIO21
set_pad("U1", 26, None)             # GPIO45 strap remains unused
set_pad("U1", 38, "/LED_CTRL")     # GPIO2
set_pad("U1", 39, "/PGOOD_N")      # GPIO1

# Charger and external connectors.
set_pad("U2", 6, "/VSYS")
set_pad("J3", 9, "/SD_CD")
set_pad("J4", 1, "/PACKP")

# BQ27441DRZR-G1A datasheet pin map.
gauge_map = {
    1: "/I2C_SDA", 2: "/I2C_SCL", 3: "/GND", 4: None,
    5: "/GAUGE_VDD", 6: "/PACKP", 7: "/VBAT", 8: "/PACKP",
    9: None, 10: "/GAUGE_BIN", 11: None, 12: "/GAUGE_INT_N", 13: "/GND",
}
for number, name in gauge_map.items():
    for item in fp("U7").Pads():
        if item.GetNumber() == str(number):
            item.SetNetCode(0) if name is None else item.SetNet(net(name))
fp("U7").SetPosition(pos(128.0, 106.0))

# Replace the invalid 0603 shunt with the TI-recommended 10 mOhm 2512 part.
old_r73 = footprints.pop("R73")
board.Delete(old_r73)
library_footprint(
    "Resistor_SMD", "R_2512_6332Metric", "R73", "0.01R 1% 1W 50ppm",
    128.0, 111.5,
)
# The B-side flip reverses the global left/right pad locations.  R73 is
# non-polarized, so pad 2 is PACKP (left) and pad 1 is VBAT (right).
assign_two_pin("R73", "/VBAT", "/PACKP")

# Gauge support: BAT bypass, internal-LDO bypass, BIN and GPOUT pull-up.
fp("C70").SetPosition(pos(122.5, 105.0))
fp("C70").SetOrientationDegrees(180)
assign_two_pin("C70", "/PACKP", "/GND")
clone_footprint("C70", "C71", "1uF", 122.5, 108.0, rotation=180)
assign_two_pin("C71", "/GAUGE_VDD", "/GND")
fp("R72").SetPosition(pos(132.0, 108.0))
assign_two_pin("R72", "/GAUGE_BIN", "/GND")
clone_footprint("R72", "R74", "10k", 136.0, 108.0)
assign_two_pin("R74", "/GAUGE_INT_N", "/+3V3_MAIN")

# Open-drain pull-ups and deterministic battery-start enable.
clone_footprint("R72", "R75", "100k", 147.0, 105.5)
assign_two_pin("R75", "/PGOOD_N", "/+3V3_MAIN")
clone_footprint("R72", "R76", "100k", 138.0, 87.5)
assign_two_pin("R76", "/PG_3V3_N", "/+3V3_MAIN")
clone_footprint("R72", "R77", "100k", 145.0, 107.0)
assign_two_pin("R77", "/CHG_N", "/+3V3_MAIN")
clone_footprint("R72", "R30", "100k", 124.0, 91.5)
assign_two_pin("R30", "/RUN_EN", "/VSYS")

# microSD pull-ups and local supply bypass.
for ref, signal, x in [
    ("R90", "/SD_CS", 146.0), ("R91", "/SD_CMD", 149.0),
    ("R92", "/SD_DAT0", 152.0), ("R93", "/SD_CD", 155.0),
]:
    clone_footprint("R72", ref, "10k", x, 92.0)
    assign_two_pin(ref, signal, "/+3V3_MAIN")
clone_footprint("C70", "C90", "10uF", 154.0, 94.0)
assign_two_pin("C90", "/+3V3_MAIN", "/GND")
clone_footprint("C70", "C91", "100nF", 157.0, 94.0)
assign_two_pin("C91", "/+3V3_MAIN", "/GND")

# ESP32 module local bulk and high-frequency bypass at the 3V3 pin.
clone_footprint("C70", "C81", "10uF", 139.0, 69.0)
assign_two_pin("C81", "/+3V3_MAIN", "/GND")
clone_footprint("C70", "C82", "100nF", 139.0, 71.0)
assign_two_pin("C82", "/+3V3_MAIN", "/GND")

# Manufacturing test access.  TP4 is the converter power-good signal; the
# remaining points expose serial, ground and each power-domain boundary.
test_points = [
    ("TP1", "UART_TX0", "/UART_TX0", 75.0, 65.0),
    ("TP2", "UART_RX0", "/UART_RX0", 79.0, 65.0),
    ("TP3", "GND", "/GND", 83.0, 65.0),
    ("TP4", "PG_3V3", "/PG_3V3_N", 87.0, 65.0),
    ("TP5", "PACKP", "/PACKP", 91.0, 65.0),
    ("TP6", "VBAT", "/VBAT", 95.0, 65.0),
    ("TP7", "VSYS", "/VSYS", 99.0, 65.0),
    ("TP8", "+3V3", "/+3V3_MAIN", 103.0, 65.0),
    ("TP9", "+5V", "/+5V_MEDIA", 107.0, 65.0),
]
for ref, value, name, x, y in test_points:
    library_footprint("TestPoint", "TestPoint_Pad_D1.5mm", ref, value, x, y)
    set_pad(ref, 1, name)

# Three global fiducials support paste/placement alignment.
for ref, x, y in [("FID1", 68.0, 68.0), ("FID2", 135.0, 62.0), ("FID3", 112.0, 118.0)]:
    library_footprint("Fiducial", "Fiducial_1mm_Mask2mm", ref, "Fiducial", x, y, layer=pcbnew.F_Cu)

# Locked battery current path.  These traces are intentionally wider than the
# ordinary signal rules and terminate directly at the shunt pads.
r73_p1 = pad("R73", 1).GetPosition()
r73_p2 = pad("R73", 2).GetPosition()
p1 = (r73_p1.x / MM, r73_p1.y / MM)
p2 = (r73_p2.x / MM, r73_p2.y / MM)
track("/PACKP", [
    (75.0, 106.5), (70.0, 110.0), (70.0, 112.0), (100.0, 112.0),
    (100.0, 110.0), (116.0, 110.0), (116.0, 112.0), (120.0, 112.0), p2,
], 1.5)
track("/VBAT", [p1, (136.0, 111.5), (138.5, 108.0), (138.5, 105.0)], 1.2)
track("/VBAT", [(138.5, 105.0), (140.5, 103.25), (141.5375, 103.25)], 0.2)
track("/VBAT", [(141.5375, 103.25), (141.5375, 102.75)], 0.2)

# Kelvin sensing: each gauge sense input lands at the appropriate shunt
# terminal rather than sampling an arbitrary point on the power trace.
track("/PACKP", [(126.1, 105.0), (124.0, 105.0), (124.0, 110.0), p2], 0.15)
track("/PACKP", [(129.9, 105.4), (130.5, 105.4), (130.5, 108.5), (125.0, 111.0), p2], 0.15)
track("/VBAT", [(129.9, 105.0), (132.0, 105.0), (132.0, 110.0), p1], 0.15)

# Gauge-local bypass capacitors are placed close to their pins.  Their ordinary
# signal connections are left to the router; only the Kelvin paths are locked.

board.SetTitleBlock(board.GetTitleBlock())
pcbnew.ZONE_FILLER(board).Fill(board.Zones())
# Freerouting 2.2.4 can crash while normalizing imported partial traces.  Lock
# the DRC-clean retained routing and the hand-routed current/Kelvin paths so the
# autorouter only adds missing connections.
for item in board.GetTracks():
    item.SetLocked(True)
pcbnew.SaveBoard(str(OUTPUT), board)
print(OUTPUT)
