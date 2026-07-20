#!/usr/bin/env python3
"""Generate the D1 pin-correct legacy schematic and BOM for KiCad upgrade.

KiCad's legacy interchange is used only as a deterministic authoring format;
the checked deliverable is upgraded to .kicad_sch by kicad-cli 10.
"""

from __future__ import annotations

import csv
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parent


@dataclass(frozen=True)
class Symbol:
    name: str
    pins: tuple[tuple[str, str], ...]


@dataclass(frozen=True)
class Part:
    ref: str
    value: str
    symbol: str
    footprint: str
    mpn: str
    lcsc: str
    x: int
    y: int
    nets: dict[str, str]
    note: str = ""


ESP_PINS = (
    ("GND", "1"), ("3V3", "2"), ("EN", "3"), ("GPIO4", "4"),
    ("GPIO5", "5"), ("GPIO6", "6"), ("GPIO7", "7"), ("GPIO15", "8"),
    ("GPIO16", "9"), ("GPIO17", "10"), ("GPIO18", "11"), ("GPIO8", "12"),
    ("GPIO19", "13"), ("GPIO20", "14"), ("GPIO3", "15"), ("GPIO46", "16"),
    ("GPIO9", "17"), ("GPIO10", "18"), ("GPIO11", "19"), ("GPIO12", "20"),
    ("GPIO13", "21"), ("GPIO14", "22"), ("GPIO21", "23"), ("GPIO47", "24"),
    ("GPIO48", "25"), ("GPIO45", "26"), ("GPIO0", "27"), ("GPIO35", "28"),
    ("GPIO36", "29"), ("GPIO37", "30"), ("GPIO38", "31"), ("GPIO39", "32"),
    ("GPIO40", "33"), ("GPIO41", "34"), ("GPIO42", "35"), ("RXD0", "36"),
    ("TXD0", "37"), ("GPIO2", "38"), ("GPIO1", "39"), ("GND", "40"),
    ("EPAD", "41"),
)

SYMBOLS = {
    "ESP32_S3_WROOM_1": Symbol("ESP32_S3_WROOM_1", ESP_PINS),
    "BQ24074": Symbol("BQ24074", (("TS", "1"), ("BAT", "2"), ("BAT", "3"), ("CE", "4"),
        ("EN2", "5"), ("EN1", "6"), ("PGOOD", "7"), ("VSS", "8"), ("CHG", "9"),
        ("OUT", "10"), ("OUT", "11"), ("ILIM", "12"), ("IN", "13"), ("TMR", "14"),
        ("ITERM", "15"), ("ISET", "16"), ("EP", "17"))),
    "TPS63070": Symbol("TPS63070", (("PS_SYNC", "1"), ("PG", "2"), ("VAUX", "3"), ("GND", "4"),
        ("FB", "5"), ("FB2", "6"), ("VOUT", "7"), ("VOUT", "8"), ("L2", "9"),
        ("PGND", "10"), ("L1", "11"), ("VIN", "12"), ("VIN", "13"), ("EN", "14"), ("VSEL", "15"))),
    "TPS61023": Symbol("TPS61023", (("FB", "1"), ("EN", "2"), ("VIN", "3"), ("GND", "4"), ("SW", "5"), ("VOUT", "6"))),
    "MAX98357A": Symbol("MAX98357A", (("DIN", "1"), ("GAIN_SLOT", "2"), ("GND", "3"), ("SD_MODE", "4"),
        ("NC", "5"), ("NC", "6"), ("VDD", "7"), ("VDD", "8"), ("OUTP", "9"), ("OUTN", "10"),
        ("GND", "11"), ("NC", "12"), ("NC", "13"), ("LRCLK", "14"), ("GND", "15"), ("BCLK", "16"), ("EP", "17"))),
    "PDM_MIC": Symbol("PDM_MIC", (("DATA", "1"), ("VDD", "2"), ("CLOCK", "3"), ("LR", "4"), ("GND", "5"))),
    "BQ27441": Symbol("BQ27441", (("SDA", "1"), ("VDD", "2"), ("NC", "3"), ("VSS", "4"),
        ("SCL", "5"), ("BAT", "6"), ("SRN", "7"), ("NC", "8"), ("BIN", "9"),
        ("NC", "10"), ("SRP", "11"), ("GPOUT", "12"), ("EP", "13"))),
    "USBLC6_2": Symbol("USBLC6_2", (("IO1", "1"), ("GND", "2"), ("IO2", "3"), ("IO2", "4"), ("VBUS", "5"), ("IO1", "6"))),
    "USB_C_16P": Symbol("USB_C_16P", (("GND", "A1"), ("VBUS", "A4"), ("CC1", "A5"), ("D+", "A6"), ("D-", "A7"),
        ("SBU1", "A8"), ("VBUS", "A9"), ("GND", "A12"), ("GND", "B1"), ("VBUS", "B4"), ("CC2", "B5"),
        ("D+", "B6"), ("D-", "B7"), ("SBU2", "B8"), ("VBUS", "B9"), ("GND", "B12"), ("SHIELD", "S1"))),
    "LCD_FPC_10": Symbol("LCD_FPC_10", (("SCLK", "1"), ("SI", "2"), ("SCS", "3"), ("EXTCOMIN", "4"),
        ("DISP", "5"), ("VDDA", "6"), ("VDD", "7"), ("EXTMODE", "8"), ("VSS", "9"), ("VSSA", "10"))),
    "MICROSD_10": Symbol("MICROSD_10", (("DAT2", "1"), ("CD_DAT3", "2"), ("CMD", "3"), ("VDD", "4"),
        ("CLK", "5"), ("VSS", "6"), ("DAT0", "7"), ("DAT1", "8"), ("CD", "9"), ("SHIELD", "10"))),
    "CONN_6": Symbol("CONN_6", tuple((f"P{i}", str(i)) for i in range(1, 7))),
    "CONN_2": Symbol("CONN_2", (("P1", "1"), ("P2", "2"))),
    "SWITCH": Symbol("SWITCH", (("A", "1"), ("B", "2"))),
    "PASSIVE": Symbol("PASSIVE", (("1", "1"), ("2", "2"))),
}


def esp_nets():
    n = {"1": "GND", "2": "+3V3_MAIN", "3": "ESP_EN", "40": "GND", "41": "GND"}
    gpio = {
        4: "ENC_A", 5: "ENC_B", 6: "A_SW", 7: "I2S_DIN", 8: "I2C_SDA",
        10: "LCD_SCS", 11: "LCD_SI", 12: "LCD_SCLK", 13: "LCD_DISP", 14: "LCD_EXTCOMIN",
        15: "LCD_EXTMODE", 17: "CHG_N", 18: "I2C_SCL", 19: "USB_D-", 20: "USB_D+",
        35: "SD_CMD", 36: "SD_CLK", 37: "SD_DAT0", 38: "SD_CS", 39: "PDM_CLK", 40: "I2S_LRCLK",
        41: "AMP_EN", 42: "PDM_DATA", 45: "SD_CD", 46: "I2S_BCLK", 47: "B_SW", 48: "MENU_SW", 0: "BOOT_N",
    }
    pin_for_gpio = {int(name.removeprefix("GPIO")): num for name, num in ESP_PINS if name.startswith("GPIO")}
    for gpio_num, net in gpio.items():
        n[pin_for_gpio[gpio_num]] = net
    n["36"] = "UART_RX0"
    n["37"] = "UART_TX0"
    return n


PARTS = [
    Part("U1", "ESP32-S3-WROOM-1-N16R8", "ESP32_S3_WROOM_1", "RF_Module:ESP32-S3-WROOM-1", "ESP32-S3-WROOM-1-N16R8", "C2913202", 2100, 2800, esp_nets()),
    Part("U2", "BQ24074RGTR", "BQ24074", "Package_DFN_QFN:VQFN-16-1EP_3x3mm_P0.5mm_EP1.68x1.68mm_ThermalVias", "BQ24074RGTR", "C54313", 4700, 1700,
         {"1":"BAT_TS", "2":"VBAT", "3":"VBAT", "4":"GND", "5":"GND", "6":"+3V3_MAIN", "7":"PGOOD_N", "8":"GND", "9":"CHG_N", "10":"VSYS", "11":"VSYS", "12":"ILIM_SET", "13":"VBUS", "14":"TMR_SET", "15":"ITERM_SET", "16":"ISET_SET", "17":"GND"}),
    Part("U3", "TPS63070RNMR", "TPS63070", "Cadenza_D1:TI_RNM0015A_VQFN-HR-15_2.5x3mm", "TPS63070RNMR", "C109322", 7200, 1700,
         {"1":"+3V3_MAIN", "2":"PG_3V3_N", "3":"VAUX_3V3", "4":"GND", "5":"FB_3V3", "6":"GND", "7":"+3V3_MAIN", "8":"+3V3_MAIN", "9":"L3V3_B", "10":"GND", "11":"L3V3_A", "12":"VSYS", "13":"VSYS", "14":"RUN_EN", "15":"GND"}),
    Part("U4", "TPS61023DRLR", "TPS61023", "Cadenza_D1:TI_DRL0006A_SOT-563", "TPS61023DRLR", "C919459", 9700, 1700,
         {"1":"FB_5V", "2":"RUN_EN", "3":"VSYS", "4":"GND", "5":"BOOST_SW", "6":"+5V_MEDIA"}),
    Part("J1", "USB-C USB2 Sink", "USB_C_16P", "Connector_USB:USB_C_Receptacle_HCTL_HC-TYPE-C-16P-01A", "HC-TYPE-C-16P-01A-G", "C2997435", 4700, 4200,
         {"A1":"GND", "A4":"VBUS", "A5":"CC1", "A6":"USB_D+_CONN", "A7":"USB_D-_CONN", "A8":"NC", "A9":"VBUS", "A12":"GND", "B1":"GND", "B4":"VBUS", "B5":"CC2", "B6":"USB_D+_CONN", "B7":"USB_D-_CONN", "B8":"NC", "B9":"VBUS", "B12":"GND", "S1":"GND"}),
    Part("U8", "USBLC6-2SC6", "USBLC6_2", "Package_TO_SOT_SMD:SOT-23-6", "USBLC6-2SC6", "C7519", 7200, 4200,
         {"1":"USB_D-_CONN", "2":"GND", "3":"USB_D+_CONN", "4":"USB_D+", "5":"VBUS", "6":"USB_D-"}),
    Part("J2", "LS027B7DH01 FPC", "LCD_FPC_10", "Cadenza_D1:Hirose_FH34SRJ-10S-0.5SH", "FH34SRJ-10S-0.5SH(99)", "C324723", 9700, 4200,
         {"1":"LCD_SCLK", "2":"LCD_SI", "3":"LCD_SCS", "4":"LCD_EXTCOMIN", "5":"LCD_DISP", "6":"+5V_LCD", "7":"+5V_LCD", "8":"LCD_EXTMODE", "9":"GND", "10":"GND"}),
    Part("U5", "MAX98357AETE+T", "MAX98357A", "Package_DFN_QFN:TQFN-16-1EP_3x3mm_P0.5mm_EP1.23x1.23mm_ThermalVias", "MAX98357AETE+T", "C910544", 2100, 6200,
         {"1":"I2S_DIN", "2":"GND", "3":"GND", "4":"AMP_EN", "7":"+5V_AUDIO", "8":"+5V_AUDIO", "9":"SPK+", "10":"SPK-", "11":"GND", "14":"I2S_LRCLK", "15":"GND", "16":"I2S_BCLK", "17":"GND"}),
    Part("U6", "IM73D122V01", "PDM_MIC", "Cadenza_D1:Infineon_IM73D122V01_PG-LLGA-5-3", "IM73D122V01", "C5563886", 4700, 6200,
         {"1":"PDM_DATA_RAW", "2":"+3V3_MAIN", "3":"PDM_CLK", "4":"GND", "5":"GND"}, "DNP IM69 variant when fitted"),
    Part("U7", "BQ27441DRZR-G1A", "BQ27441", "Cadenza_D1:TI_DRZ0012A_VSON-12_2.5x4mm", "BQ27441DRZR-G1A", "C473397", 7200, 6200,
         {"1":"I2C_SDA", "2":"GAUGE_VDD", "4":"GND", "5":"I2C_SCL", "6":"VBAT_SENSE", "7":"GAUGE_SRN", "9":"GAUGE_BIN", "11":"GAUGE_SRP", "12":"GAUGE_INT_N", "13":"GND"}),
    Part("J3", "microSD push-push", "MICROSD_10", "Connector_Card:microSD_HC_Hirose_DM3AT-SF-PEJM5", "DM3AT-SF-PEJM5", "C114218", 9700, 6200,
         {"1":"NC", "2":"SD_CS", "3":"SD_CMD", "4":"+3V3_MAIN", "5":"SD_CLK", "6":"GND", "7":"SD_DAT0", "8":"NC", "9":"NC", "10":"GND"}, "No mechanical card detect; sample-check shell aperture"),
    Part("J6", "Encoder daughterboard", "CONN_6", "Connector_JST:JST_SH_SM06B-SRSS-TB_1x06-1MP_P1.00mm_Horizontal", "SM06B-SRSS-TB", "TBD", 9700, 7600,
         {"1":"+3V3_MAIN", "2":"GND", "3":"ENC_A", "4":"ENC_B", "5":"A_SW", "6":"LED_CTRL"}),
    Part("J4", "LiPo 1S", "CONN_2", "Connector_JST:JST_PH_B2B-PH-SM4-TB_1x02-1MP_P2.00mm_Vertical", "B2B-PH-SM4-TB", "TBD", 4700, 7600, {"1":"VBAT", "2":"GND"}),
    Part("J5", "Speaker 8R 1-2W", "CONN_2", "Connector_JST:JST_PH_B2B-PH-SM4-TB_1x02-1MP_P2.00mm_Vertical", "B2B-PH-SM4-TB", "TBD", 7200, 7600, {"1":"SPK+", "2":"SPK-"}),
    Part("SW1", "POWER RUN/OFF", "SWITCH", "Button_Switch_SMD:SW_SPDT_CK_JS102011SAQN", "JS102011SAQN", "TBD", 2100, 9000, {"1":"RUN_EN", "2":"GND"}, "Maintained side switch; charger remains active"),
    Part("SW2", "B", "SWITCH", "Button_Switch_SMD:SW_Push_1P1T_NO_CK_KMR2", "KMR2", "TBD", 4700, 9000, {"1":"B_SW", "2":"GND"}),
    Part("SW3", "MENU", "SWITCH", "Button_Switch_SMD:SW_Push_1P1T_NO_CK_KMR2", "KMR2", "TBD", 7200, 9000, {"1":"MENU_SW", "2":"GND"}),
    Part("SW4", "RESET", "SWITCH", "Button_Switch_SMD:SW_Push_1P1T_NO_CK_KMR2", "KMR2", "TBD", 9700, 9000, {"1":"ESP_EN", "2":"GND"}, "Hidden service switch"),
    Part("SW5", "BOOT", "SWITCH", "Button_Switch_SMD:SW_Push_1P1T_NO_CK_KMR2", "KMR2", "TBD", 2100, 10200, {"1":"BOOT_N", "2":"GND"}, "Hidden service switch"),
]


PASSIVES = [
    ("R1","5.1k","CC1","GND"), ("R2","5.1k","CC2","GND"),
    ("R3","1.5k","ILIM_SET","GND"), ("R4","1.18k","ISET_SET","GND"), ("R5","1.5k","ITERM_SET","GND"),
    ("R6","47k","TMR_SET","GND"), ("R7","10k","BAT_TS","GND"),
    ("R10","470k","+3V3_MAIN","FB_3V3"), ("R11","150k","FB_3V3","GND"),
    ("R20","732k","+5V_MEDIA","FB_5V"), ("R21","100k","FB_5V","GND"),
    ("R40","100R","PDM_DATA_RAW","PDM_DATA"), ("R41","100k","AMP_EN","+3V3_MAIN"),
    ("R70","4.7k","I2C_SDA","+3V3_MAIN"), ("R71","4.7k","I2C_SCL","+3V3_MAIN"), ("R72","10k","GAUGE_BIN","GND"),
    ("R73","0.01R 1%","GAUGE_SRP","GAUGE_SRN"),
    ("R80","10k","+3V3_MAIN","ESP_EN"),
    ("C1","10uF","VBUS","GND"), ("C2","10uF","VSYS","GND"), ("C3","10uF","VBAT","GND"),
    ("C10","10uF","VSYS","GND"), ("C11","10uF","VSYS","GND"), ("C12","22uF","+3V3_MAIN","GND"), ("C13","22uF","+3V3_MAIN","GND"), ("C14","100nF","VAUX_3V3","GND"),
    ("C20","10uF","VSYS","GND"), ("C21","22uF","+5V_MEDIA","GND"), ("C22","22uF","+5V_MEDIA","GND"),
    ("C30","1uF","+5V_LCD","GND"), ("C31","100nF","+5V_LCD","GND"),
    ("C40","100nF","+3V3_MAIN","GND"), ("C50","10uF","+5V_AUDIO","GND"), ("C51","100nF","+5V_AUDIO","GND"),
    ("C70","1uF","VBAT_SENSE","GND"),
    ("C80","1uF","ESP_EN","GND"),
    ("L1","1.2uH Isat>=4.5A","L3V3_A","L3V3_B"), ("L2","1uH Isat>=4.5A","VSYS","BOOST_SW"),
    ("FB1","600R@100MHz","+5V_MEDIA","+5V_LCD"), ("FB2","600R@100MHz","+5V_MEDIA","+5V_AUDIO"),
]


def lib_text() -> str:
    out = ["EESchema-LIBRARY Version 2.4", "#encoding utf-8"]
    for sym in SYMBOLS.values():
        count = len(sym.pins)
        half = (count + 1) // 2
        height = max(600, half * 100 + 200)
        out += ["#", f"# {sym.name}", "#", f"DEF {sym.name} U 0 40 Y Y 1 F N",
                f'F0 "U" 0 {height//2 + 150} 50 H V C CNN', f'F1 "{sym.name}" 0 {-height//2 - 150} 50 H V C CNN', "DRAW",
                f"S -500 {height//2} 500 {-height//2} 0 1 10 f"]
        for idx, (pin_name, pin_num) in enumerate(sym.pins):
            if idx < half:
                py = height // 2 - 100 - idx * 100
                out.append(f"X {pin_name} {pin_num} -700 {py} 200 R 40 40 1 1 P")
            else:
                py = height // 2 - 100 - (idx - half) * 100
                out.append(f"X {pin_name} {pin_num} 700 {py} 200 L 40 40 1 1 P")
        out += ["ENDDRAW", "ENDDEF"]
    out += ["#", "#End Library", ""]
    return "\n".join(out)


def component_block(part: Part, stamp: int) -> list[str]:
    return [
        "$Comp", f"L Cadenza_D1:{part.symbol} {part.ref}", f"U 1 1 {stamp:08X}", f"P {part.x} {part.y}",
        f'F 0 "{part.ref}" H {part.x} {part.y + 100} 50  0000 C CNN',
        f'F 1 "{part.value}" H {part.x} {part.y - 100} 50  0000 C CNN',
        f'F 2 "{part.footprint}" H {part.x} {part.y} 50  0001 C CNN',
        f'F 3 "{part.mpn}" H {part.x} {part.y} 50  0001 C CNN',
        f"\t1    {part.x} {part.y}", "\t1    0    0    -1", "$EndComp",
    ]


def pin_position(part: Part, pin_num: str):
    pins = SYMBOLS[part.symbol].pins
    idx = next(i for i, (_, n) in enumerate(pins) if n == pin_num)
    half = (len(pins) + 1) // 2
    height = max(600, half * 100 + 200)
    if idx < half:
        return part.x - 700, part.y - (height // 2 - 100 - idx * 100), -1
    return part.x + 700, part.y - (height // 2 - 100 - (idx - half) * 100), 1


def schematic(parts: list[Part]) -> str:
    out = [
        "EESchema Schematic File Version 4", "LIBS:Cadenza_D1", "EELAYER 29 0", "EELAYER END",
        "$Descr A2 23386 16535", "Sheet 1 1", 'Title "Cadenza Rev A D1 - production electrical contract"',
        'Date "2026-07-20"', 'Rev "D1"', 'Comp "CadenzaOS"',
        'Comment1 "Pin-correct; sample-gated electromechanics remain DNP until verified"', "$EndDescr",
    ]
    for stamp, part in enumerate(parts, 0xD1000001):
        out += component_block(part, stamp)
        for _, pin_num in SYMBOLS[part.symbol].pins:
            x, y, side = pin_position(part, pin_num)
            net = part.nets.get(pin_num)
            if not net or net == "NC":
                out.append(f"NoConn ~ {x} {y}")
                continue
            x2 = x + side * 250
            out += [f"Wire Wire Line", f"\t{x} {y} {x2} {y}", f"Text Label {x2} {y} {0 if side < 0 else 2}    35   ~ 0", net]
    out += ["Text Notes 900 650 0    100  ~ 20", "D1 FUNCTIONAL SCHEMATIC - labels are global electrical nets",
            "Text Notes 900 850 0    60   ~ 0", "See component-freeze.md and circuit.json before substitution or manufacture.",
            "$EndSCHEMATC", ""]
    return "\n".join(out)


def passive_parts() -> list[Part]:
    parts = []
    x0, y0 = 12300, 900
    for i, (ref, value, n1, n2) in enumerate(PASSIVES):
        col, row = divmod(i, 13)
        footprint = "Resistor_SMD:R_0603_1608Metric" if ref.startswith("R") else "Capacitor_SMD:C_0603_1608Metric"
        if ref.startswith("L"):
            footprint = "Inductor_SMD:L_1210_3225Metric"
        if ref.startswith("FB"):
            footprint = "Inductor_SMD:L_0603_1608Metric"
        # Keep at least 400 legacy units between opposing label stubs.  A
        # smaller pitch silently joins adjacent passive rows after conversion.
        parts.append(Part(ref, value, "PASSIVE", footprint, "VALUE_TO_FREEZE", "TBD", x0 + col * 2300, y0 + row * 900, {"1": n1, "2": n2}))
    return parts


def write_bom(parts: list[Part]):
    with (ROOT / "bom-d1.csv").open("w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["Reference", "Value", "MPN", "LCSC", "Footprint", "Status/Note"])
        for p in parts:
            w.writerow([p.ref, p.value, p.mpn, p.lcsc, p.footprint, p.note])


def main():
    parts = PARTS + passive_parts()
    (ROOT / "Cadenza_D1.lib").write_text(lib_text(), encoding="utf-8")
    (ROOT / "sym-lib-table").write_text('(sym_lib_table\n  (version 7)\n  (lib (name "Cadenza_D1")(type "Legacy")(uri "${KIPRJMOD}/Cadenza_D1.lib")(options "")(descr "Cadenza D1 audited symbols"))\n)\n', encoding="ascii")
    (ROOT / "fp-lib-table").write_text('(fp_lib_table\n  (version 7)\n  (lib (name "Cadenza_D1")(type "KiCad")(uri "${KIPRJMOD}/Cadenza_D1.pretty")(options "")(descr "Cadenza D1 audited footprints"))\n)\n', encoding="ascii")
    (ROOT / "cadenza-d1.sch").write_text(schematic(parts), encoding="utf-8")
    write_bom(parts)


if __name__ == "__main__":
    main()
