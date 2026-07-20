#!/usr/bin/env python3
"""Generate the review-grade Cadenza Rev A Product D0 exchange package.

The PCB is a deterministic KiCad 5.9 exchange board because JLCEDA Pro documents
that importer explicitly. Interface footprints are deliberately simple review
footprints. They are not a substitute for manufacturer land-pattern verification.
"""

from __future__ import annotations

import csv
import json
import math
import shutil
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent
OUT = ROOT / "generated"
KICAD = OUT / "kicad"

BOARD_W = 116.0
BOARD_H = 68.0
OX = 20.0
OY = 20.0

PARTS = [
    ("U1", "ESP32-S3-WROOM-1-N16R8", 77, 30, 18, 25.5, "MCU/RF"),
    ("J_LCD", "FH34SRJ-10S-0.5SH", 58, 26, 9, 4, "DISPLAY"),
    ("U_5V", "TPS61023", 54, 36, 8, 8, "POWER"),
    ("U_CHG", "BQ24074", 91, 62, 10, 10, "POWER"),
    ("U_3V3", "AP2112K-3.3", 78, 60, 8, 8, "POWER"),
    ("U_AMP", "MAX98357A", 30, 54, 10, 10, "AUDIO"),
    ("U_MIC", "IM69D130", 27, 30, 5, 5, "AUDIO"),
    ("J_SPK", "JST-PH-2", 25, 63, 8, 5, "AUDIO"),
    ("J_BAT", "JST-PH-2", 102, 63, 8, 5, "POWER"),
    ("J_USB", "USB-C-USB2", 108, 22, 10, 8, "USB"),
    ("J_SD", "MICROSD", 101, 47, 14, 13, "STORAGE"),
    ("J_ENC", "ENCODER_DB_1x6", 108, 40, 8, 14, "CONTROL"),
    ("SW_B", "TACT_B", 32, 40, 7, 7, "CONTROL"),
    ("SW_MENU", "TACT_MENU", 32, 32, 7, 7, "CONTROL"),
]

NETS = [
    "GND", "VBUS", "VBAT", "+3V3", "+5V_LCD", "USB_D-", "USB_D+",
    "LCD_SCLK", "LCD_SI", "LCD_SCS", "LCD_EXTCOMIN", "LCD_DISP",
    "LCD_5V_EN", "ENC_A", "ENC_B", "A_SW", "B_SW", "MENU_SW",
    "MIC_CLK", "MIC_DATA", "SPK_BCLK", "SPK_LRCLK", "SPK_DIN",
    "SPK+", "SPK-", "SD_SCK", "SD_MOSI", "SD_MISO", "SD_CS",
    "I2C_SDA", "I2C_SCL"
]

PINMAP = {
    "U1": ["GND", "+3V3", "USB_D-", "USB_D+", "LCD_SCLK", "LCD_SI", "LCD_SCS", "LCD_EXTCOMIN", "LCD_DISP", "LCD_5V_EN", "ENC_A", "ENC_B", "A_SW", "B_SW", "MENU_SW", "MIC_CLK", "MIC_DATA", "SPK_BCLK", "SPK_LRCLK", "SPK_DIN", "SD_SCK", "SD_MOSI", "SD_MISO", "SD_CS", "I2C_SDA", "I2C_SCL"],
    "J_LCD": ["LCD_SCLK", "LCD_SI", "LCD_SCS", "LCD_EXTCOMIN", "LCD_DISP", "+5V_LCD", "+5V_LCD", "GND", "GND", "GND"],
    "U_5V": ["VBAT", "GND", "LCD_5V_EN", "+5V_LCD"],
    "U_CHG": ["VBUS", "GND", "VBAT", "I2C_SDA", "I2C_SCL"],
    "U_3V3": ["VBAT", "GND", "+3V3"],
    "U_AMP": ["+5V_LCD", "GND", "SPK_BCLK", "SPK_LRCLK", "SPK_DIN", "SPK+", "SPK-"],
    "U_MIC": ["+3V3", "GND", "MIC_CLK", "MIC_DATA"],
    "J_SPK": ["SPK+", "SPK-"], "J_BAT": ["VBAT", "GND"],
    "J_USB": ["VBUS", "GND", "USB_D-", "USB_D+"],
    "J_SD": ["+3V3", "GND", "SD_SCK", "SD_MOSI", "SD_MISO", "SD_CS"],
    "J_ENC": ["+3V3", "GND", "ENC_A", "ENC_B", "A_SW", "+3V3"],
    "SW_B": ["B_SW", "GND"], "SW_MENU": ["MENU_SW", "GND"]
}


def module(ref: str, value: str, x: float, y: float, w: float, h: float) -> str:
    pins = PINMAP[ref]
    rows = math.ceil(len(pins) / 2)
    pitch = min(2.0, max(0.8, (h - 2) / max(rows - 1, 1)))
    pads = []
    for i, net in enumerate(pins):
        side = -1 if i < rows else 1
        row = i if i < rows else i - rows
        px = side * w / 2
        py = (row - (rows - 1) / 2) * pitch
        shape = "rect" if i == 0 else "circle"
        pads.append(f'''    (pad {i+1} thru_hole {shape} (at {px:.3f} {py:.3f}) (size 1.4 1.4) (drill 0.7) (layers *.Cu *.Mask) (net {NETS.index(net)+1} {net}))''')
    return f'''  (module Cadenza:{ref} (layer F.Cu) (at {OX+x:.3f} {OY+y:.3f})
    (fp_text reference {ref} (at 0 {-h/2-1.5:.2f}) (layer F.SilkS))
    (fp_text value {value} (at 0 {h/2+1.5:.2f}) (layer F.Fab))
    (fp_rect (start {-w/2:.2f} {-h/2:.2f}) (end {w/2:.2f} {h/2:.2f}) (layer F.SilkS) (width 0.3))
{chr(10).join(pads)}
  )'''


def make_pcb() -> str:
    nets = "\n".join(f"  (net {i+1} {n})" for i, n in enumerate(NETS))
    mods = "\n".join(module(*p[:6]) for p in PARTS)
    x0, y0, x1, y1 = OX, OY, OX + BOARD_W, OY + BOARD_H
    holes = "\n".join(
        f'''  (module Cadenza:MountingHole (layer F.Cu) (at {x:.2f} {y:.2f})
    (fp_text reference H{i} (at 0 -3) (layer F.SilkS))
    (fp_circle (center 0 0) (end 2.25 0) (layer Edge.Cuts) (width 0.2))
    (pad 1 np_thru_hole circle (at 0 0) (size 2.7 2.7) (drill 2.7) (layers *.Cu *.Mask)))'''
        for i, (x, y) in enumerate(((x0+4,y0+4),(x1-4,y0+4),(x0+4,y1-4),(x1-4,y1-4)), 1)
    )
    zones = f'''  (gr_rect (start {x0+38} {y0+4}) (end {x0+100} {y0+44}) (layer Dwgs.User) (width 0.3))
  (gr_text "DISPLAY GLASS 62.8 x 42.82 / VIEW 58.8 x 35.28" (at {x0+69} {y0+46}) (layer Dwgs.User))
  (gr_rect (start {x0+64} {y0}) (end {x0+94} {y0+16}) (layer Dwgs.User) (width 0.4))
  (gr_text "RF KEEP-OUT ALL LAYERS" (at {x0+79} {y0+9}) (layer Dwgs.User))
  (gr_circle (center {x0+108} {y0+34}) (end {x0+121} {y0+34}) (layer Dwgs.User) (width 0.3))
  (gr_text "ENCODER DB" (at {x0+105} {y0+53}) (layer Dwgs.User))
  (gr_rect (start {x0+3} {y0+28}) (end {x0+20} {y0+62}) (layer Dwgs.User) (width 0.3))
  (gr_text "SPEAKER CAVITY" (at {x0+11.5} {y0+45} 90) (layer Dwgs.User))'''
    return f'''(kicad_pcb (version 20171130) (host pcbnew 5.1.12)
  (general (thickness 1.6))
  (page A4)
  (layers (0 F.Cu signal) (1 In1.Cu power) (2 In2.Cu power) (31 B.Cu signal) (36 B.SilkS user) (37 F.SilkS user) (44 Edge.Cuts user) (46 B.CrtYd user) (47 F.CrtYd user) (48 B.Fab user) (49 F.Fab user))
  (setup (last_trace_width 0.2) (trace_clearance 0.15) (zone_clearance 0.2) (zone_45_only no) (trace_min 0.15) (via_size 0.6) (via_drill 0.3) (via_min_size 0.45) (via_min_drill 0.2) (uvia_size 0.3) (uvia_drill 0.1) (uvias_allowed no) (edge_width 0.05))
{nets}
  (gr_line (start {x0} {y0}) (end {x1} {y0}) (layer Edge.Cuts) (width 0.15))
  (gr_line (start {x1} {y0}) (end {x1} {y1}) (layer Edge.Cuts) (width 0.15))
  (gr_line (start {x1} {y1}) (end {x0} {y1}) (layer Edge.Cuts) (width 0.15))
  (gr_line (start {x0} {y1}) (end {x0} {y0}) (layer Edge.Cuts) (width 0.15))
  (gr_text "CADENZA REV A PRODUCT D0 - DO NOT FABRICATE" (at {x0+58} {y1-2}) (layer F.SilkS))
{zones}
{mods}
{holes}
)\n'''


def make_schematic() -> str:
    blocks = [
        ("POWER", "USB-C -> BQ24074 -> LiPo; AP2112 3V3; TPS61023 5V", 1800, 1700),
        ("MCU", "ESP32-S3-WROOM-1-N16R8", 4700, 1700),
        ("DISPLAY", "LS027B7DH01 / 10-pin FPC / 5V + 3V3 logic", 7700, 1700),
        ("INPUT", "Menu, B, encoder A/B and centre A", 1800, 3800),
        ("AUDIO", "IM69D130 + MAX98357A + 8ohm speaker", 4700, 3800),
        ("STORAGE", "microSD + 16MB module flash", 7700, 3800),
    ]
    comps=[]
    for i,(name,value,x,y) in enumerate(blocks,1):
        comps.append(f'''$Comp\nL Connector_Generic:Conn_01x02 J{i}\nU 1 1 {0x60000000+i:X}\nP {x} {y}\nF 0 "J{i}" H {x+80} {y+217} 50  0000 C CNN\nF 1 "{name}: {value}" H {x+80} {y+126} 50  0000 C CNN\n\t1    {x} {y}\n\t1 0 0 -1\n$EndComp''')
    notes="\n".join(f'Text Notes {x-600} {y-450} 0    60   ~ 12\n{name}' for name,_,x,y in blocks)
    contracts="\n".join(f'Text Notes 1200 {5100+i*180} 0    45   ~ 0\n{n}' for i,n in enumerate([
        "LCD: SCLK SI SCS EXTCOMIN DISP / 5V VDD+VDDA / VSS+VSSA",
        "INPUT: ENC_A GPIO4, ENC_B GPIO5, A_SW GPIO0, B GPIO47, MENU GPIO48",
        "AUDIO: MIC GPIO39/42; AMP GPIO46/40/7; differential SPK+/-",
        "USB: native D-/D+ GPIO19/20; ESD and 5.1k CC resistors required",
        "D0 uses interface blocks; production symbols/footprints require release-gate review"
    ]))
    return f'''EESchema Schematic File Version 4\nLIBS:power\nLIBS:device\nLIBS:Connector_Generic\nEELAYER 29 0\nEELAYER END\n$Descr A4 11693 8268\nSheet 1 1\nTitle "Cadenza Rev A Product D0"\nDate "2026-07-20"\nRev "D0 - DO NOT FABRICATE"\nComp "CadenzaOS"\nComment1 "System architecture and interface contract"\n$EndDescr\n{chr(10).join(comps)}\n{notes}\n{contracts}\n$EndSCHEMATC\n'''


def make_scad() -> str:
    return '''// Cadenza Rev A D0 enclosure envelope - dimensions in mm
$fn=72;
W=122; H=74; D=19; wall=2; clearance=0.35;
screen_view=[59.2,35.7]; screen_glass=[63.2,43.2];
module rounded_box(w,h,d,r=8){ hull(){ for(x=[-w/2+r,w/2-r]) for(y=[-h/2+r,h/2-r]) translate([x,y,0]) cylinder(h=d,r=r); } }
module front_shell(){ difference(){ rounded_box(W,H,D/2,9); translate([0,0,wall]) rounded_box(W-2*wall,H-2*wall,D/2,7); translate([0,0,-1]) cube([screen_view[0],screen_view[1],D],center=true); translate([48,0,-1]) cylinder(h=D,r=14); translate([-48,10,-1]) cylinder(h=D,r=4); translate([-48,-10,-1]) cylinder(h=D,r=4); for(x=[-54:2:-40]) for(y=[-28:2:-20]) translate([x,y,-1]) cylinder(h=D,r=.55); translate([-53,21,-1]) cylinder(h=D,r=.55); } }
module rear_shell(){ difference(){ translate([0,0,D/2]) rounded_box(W,H,D/2,9); translate([0,0,D/2-0.1]) rounded_box(W-2*wall,H-2*wall,D/2,7); } translate([-35,-H/2,D/2]) rotate([0,18,0]) cube([35,4,9]); translate([18,-H/2,D/2]) rotate([0,35,0]) cube([28,4,9]); }
front_shell();
translate([0,90,0]) rear_shell();
'''


def write_csv() -> None:
    data=json.loads((ROOT/"parts.json").read_text())
    with (OUT/"BOM.csv").open("w",newline="") as f:
        w=csv.writer(f); w.writerow(["ref","mpn_or_spec","lcsc","class","gate"])
        for cls in ("fixed","conditional","external"):
            for p in data[cls]: w.writerow([p["ref"],p.get("mpn",p.get("spec","")),p.get("lcsc",""),p.get("class",cls),p.get("gate",p.get("source",""))])


def main() -> None:
    if OUT.exists(): shutil.rmtree(OUT)
    KICAD.mkdir(parents=True)
    (KICAD/"cadenza_rev_a_product_d0.kicad_pcb").write_text(make_pcb())
    (KICAD/"cadenza_rev_a_product_d0.sch").write_text(make_schematic())
    (KICAD/"cadenza_rev_a_product_d0.pro").write_text("update=0\n")
    (KICAD/"cadenza_rev_a_product_d0.kicad_pro").write_text("{}\n")
    (OUT/"enclosure.scad").write_text(make_scad())
    (OUT/"JLCEDA_IMPORT.md").write_text('''# JLCEDA Pro import check\n\n1. File > Import > KiCad; upload `cadenza-rev-a-product-d0.zip`.\n2. Confirm one schematic and one PCB are created.\n3. Confirm board outline is 116 x 68 mm and four NPTH holes exist.\n4. Confirm all named blocks, display glass, RF keepout, speaker and encoder zones exist.\n5. Confirm 31 named nets and every interface pad retains its net.\n6. Confirm D0 warning is visible. Do not order this revision.\n7. Replace conditional parts with current JLC library parts before production capture.\n''')
    shutil.copy2(ROOT/"README.md",OUT/"PRODUCT_SPEC.md")
    shutil.copy2(ROOT/"parts.json",OUT/"parts.json")
    write_csv()
    bundle=OUT/"cadenza-rev-a-product-d0.zip"
    with zipfile.ZipFile(bundle,"w",zipfile.ZIP_DEFLATED) as z:
        for path in sorted(p for p in OUT.rglob("*") if p.is_file() and p != bundle):
            info=zipfile.ZipInfo(path.relative_to(OUT).as_posix(),(2026,7,20,0,0,0)); info.compress_type=zipfile.ZIP_DEFLATED
            z.writestr(info,path.read_bytes())


if __name__ == "__main__": main()
