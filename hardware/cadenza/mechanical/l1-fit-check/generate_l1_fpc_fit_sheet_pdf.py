#!/usr/bin/env python3
"""Generate a visually verified A4 1:1 Sharp/FPC/connector fit sheet."""

from __future__ import annotations

import json
from pathlib import Path

from reportlab.lib import colors
from reportlab.lib.pagesizes import A4, landscape
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.pdfgen import canvas


HERE = Path(__file__).resolve().parent
REPO = HERE.parents[3]
PARAMS = HERE / "generated/l1-front-fitcheck-parameters.json"
PCB = HERE / "generated/candidate-pcb-mechanical.json"
OUTPUT = REPO / "output/pdf/cadenza-l1-fpc-physical-fit-sheet-a4.pdf"
MM = 72.0 / 25.4


def mm(value: float) -> float:
    return value * MM


def rect(c, origin, xy, *, stroke=colors.black, fill=None, width=0.25, dash=None):
    x1, y1, x2, y2 = xy
    c.setStrokeColor(stroke)
    c.setLineWidth(mm(width))
    c.setDash(dash or [])
    if fill is not None:
        c.setFillColor(fill)
    c.rect(mm(origin[0] + x1), mm(origin[1] + y1), mm(x2 - x1), mm(y2 - y1),
           stroke=1, fill=1 if fill is not None else 0)
    c.setDash([])


def line(c, x1, y1, x2, y2, *, color=colors.black, width=0.25, dash=None):
    c.setStrokeColor(color)
    c.setLineWidth(mm(width))
    c.setDash(dash or [])
    c.line(mm(x1), mm(y1), mm(x2), mm(y2))
    c.setDash([])


def text(c, x, y, value, size=8, color=colors.black, font="STSong-Light"):
    c.setFont(font, size)
    c.setFillColor(color)
    c.drawString(mm(x), mm(y), value)


def multiline(c, x, y, lines, size=8, leading=4.3, color=colors.black):
    for index, value in enumerate(lines):
        text(c, x, y - index * leading, value, size=size, color=color)


def main() -> int:
    pdfmetrics.registerFont(TTFont("STSong-Light", "/System/Library/Fonts/STHeiti Medium.ttc", subfontIndex=0))
    params = json.loads(PARAMS.read_text(encoding="utf-8"))
    pcb = json.loads(PCB.read_text(encoding="utf-8"))
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)

    page_w, page_h = landscape(A4)
    c = canvas.Canvas(str(OUTPUT), pagesize=(page_w, page_h), pageCompression=1)
    c.setTitle("Cadenza L1 Sharp FPC physical fit sheet - A4 1:1")

    text(c, 14, 198, "Cadenza L1 Sharp 屏幕 / FPC 实物方向检查纸", size=16)
    text(c, 14, 191, "A4 横向，必须按 100% / Actual Size 打印；左侧几何区域为 1:1。禁止自动适应页面。", size=9,
         color=colors.HexColor("#B3261E"))

    origin = (15.0, 82.0)
    board = (0.0, 0.0, 130.0, 60.0)
    panel = tuple(params["sharp_panel_xy"])
    window = tuple(params["new_window_xy"])
    fpc = tuple(params["fpc_flat_xy"])
    bend = tuple(params["fpc_bend_keepout_xy"])
    slot = tuple(params["front_fpc_pass_through_slot_xy"])
    j = tuple(pcb["critical_footprints"]["J_DISP1"]["courtyard_xy_enclosure_mm"])
    usb = tuple(pcb["critical_footprints"]["USB1"]["courtyard_xy_enclosure_mm"])

    rect(c, origin, board, stroke=colors.HexColor("#37474F"), fill=colors.HexColor("#FAFAFA"), width=0.35)
    rect(c, origin, panel, stroke=colors.HexColor("#1B5E20"), fill=colors.Color(0.3, 0.75, 0.42, alpha=0.16), width=0.35)
    rect(c, origin, window, stroke=colors.HexColor("#1565C0"), width=0.35)
    rect(c, origin, fpc, stroke=colors.HexColor("#EF6C00"), fill=colors.Color(1.0, 0.65, 0.15, alpha=0.20), width=0.3)
    rect(c, origin, bend, stroke=colors.HexColor("#EF6C00"), width=0.3, dash=[mm(1.2), mm(0.8)])
    rect(c, origin, slot, stroke=colors.HexColor("#00838F"), width=0.3, dash=[mm(0.8), mm(0.6)])
    rect(c, origin, j, stroke=colors.HexColor("#5E35B1"), fill=colors.Color(0.45, 0.3, 0.8, alpha=0.12), width=0.35)
    rect(c, origin, usb, stroke=colors.HexColor("#C62828"), fill=colors.Color(0.9, 0.2, 0.2, alpha=0.10), width=0.35)

    # Four frozen mounting holes.
    for x, y in ((5, 5), (125, 5), (5, 55), (125, 55)):
        c.setStrokeColor(colors.black)
        c.setLineWidth(mm(0.25))
        c.circle(mm(origin[0] + x), mm(origin[1] + y), mm(1.0), stroke=1, fill=0)

    # Ten FPC contact bars at 0.5 mm pitch.  With the display face up, Pin 1
    # is on +X/right according to the Sharp mechanical drawing.
    contact_y1 = origin[1] + fpc[1]
    for pin in range(1, 11):
        x = origin[0] + 66.5 + (5.5 - pin) * 0.5
        c.setFillColor(colors.HexColor("#D18B00"))
        c.rect(mm(x - 0.16), mm(contact_y1), mm(0.32), mm(1.2), stroke=0, fill=1)
    text(c, origin[0] + 68.9, contact_y1 - 2.3, "P1", size=6, color=colors.HexColor("#B3261E"))
    text(c, origin[0] + 63.7, contact_y1 - 2.3, "P10", size=6)

    # J_DISP1 pin row proxy, also Pin 1 on +X/right for rotation 0 degrees.
    j_center_y = origin[1] + pcb["critical_footprints"]["J_DISP1"]["position_enclosure_mm"][1]
    for pin in range(1, 11):
        x = origin[0] + 66.5 + (5.5 - pin) * 0.5
        c.setFillColor(colors.HexColor("#5E35B1"))
        c.circle(mm(x), mm(j_center_y), mm(0.13), stroke=0, fill=1)
    text(c, origin[0] + 69.2, j_center_y + 0.8, "J P1", size=6, color=colors.HexColor("#5E35B1"))

    text(c, origin[0] + 1.5, origin[1] + 61.5, "PCB 外框 130 x 60 mm", size=7)
    text(c, origin[0] + panel[0] + 1, origin[1] + panel[3] - 3, "Sharp 玻璃 62.8 x 42.82", size=7,
         color=colors.HexColor("#1B5E20"))
    text(c, origin[0] + window[0] + 1, origin[1] + window[1] + 1.2, "可视窗口", size=6,
         color=colors.HexColor("#1565C0"))
    text(c, origin[0] + usb[0], origin[1] + usb[1] - 3.2, "USB1", size=6, color=colors.HexColor("#C62828"))
    text(c, origin[0] + j[2] + 1, origin[1] + j[1] + 2.0, "J_DISP1", size=6,
         color=colors.HexColor("#5E35B1"))
    text(c, origin[0] + bend[2] + 1, origin[1] + bend[1] + 1.0, "允许折弯范围 R>=0.45", size=6,
         color=colors.HexColor("#EF6C00"))

    # Exact scale checks.
    line(c, 15, 70, 65, 70, width=0.5)
    line(c, 15, 68.5, 15, 71.5, width=0.4)
    line(c, 65, 68.5, 65, 71.5, width=0.4)
    text(c, 28, 65.5, "50.00 mm 校准线", size=8)
    c.setStrokeColor(colors.black)
    c.rect(mm(75), mm(65), mm(10), mm(10), stroke=1, fill=0)
    text(c, 88, 68.2, "10 x 10 mm", size=8)

    # Right-hand beginner instructions and record fields.
    x = 158
    text(c, x, 180, "只做这 6 步（全程不通电）", size=12)
    multiline(c, x, 172, [
        "1. 打印选择 100% / Actual Size，关闭 Fit to Page。",
        "2. 用卡尺量左下校准线，应为 50.00 mm。",
        "3. 屏幕显示面朝上，玻璃对齐绿色外框。",
        "4. FPC 自然朝下，不扭转；金手指应在屏幕后面。",
        "5. 只向屏幕后面做 180° 弯折，内半径不小于 0.45 mm。",
        "6. 检查 P1 是否落在 J P1 同一侧，并确认不压 USB。",
    ], size=8.2, leading=6.0)

    text(c, x, 128, "必须拍的照片", size=11)
    multiline(c, x, 121, [
        "□ 显示面朝上的整体照（屏幕与本页同时入镜）",
        "□ 屏幕背面整体照",
        "□ FPC 金手指正反面近照",
        "□ 连接器锁扣打开 / 合上的侧面照",
        "□ 折弯后 Pin 1 与 USB 间隙照",
    ], size=8.2, leading=5.4)

    text(c, x, 90, "记录", size=11)
    multiline(c, x, 83, [
        "校准线实测：________ mm（应为 50.00）",
        "屏幕 P1 与 J P1：□同侧  □不同侧  □看不清",
        "折弯时是否扭转 FPC：□否  □是",
        "是否碰 USB：□否  □是  最小间隙：______ mm",
        "锁扣能否打开并重新合上：□能  □不能",
    ], size=8.2, leading=6.0)

    text(c, x, 48, "屏幕引脚（不要按转接板丝印猜）", size=10)
    multiline(c, x, 41, [
        "1 SCLK   2 SI   3 SCS   4 EXTCOMIN   5 DISP",
        "6 VDDA   7 VDD   8 EXTMODE   9 VSS   10 VSSA",
        "若任一方向不清楚：停止，不通电，不下单。",
    ], size=7.7, leading=5.2, color=colors.HexColor("#37474F"))

    text(c, 14, 10, "来源：Sharp LS027B7DH01 机械图；PCB 候选 J_DISP1=(154.625,128.500), rot 0°；USB-J courtyard 净距 0.966 mm。", size=6.5,
         color=colors.HexColor("#455A64"))
    text(c, 265, 10, "Rev 2026-07-22", size=6.5, color=colors.HexColor("#455A64"))

    c.showPage()
    c.save()
    print(OUTPUT)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
