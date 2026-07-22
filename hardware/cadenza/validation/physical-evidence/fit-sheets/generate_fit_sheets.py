#!/usr/bin/env python3
"""Generate 1:1 A4 SVG/PDF physical-fit sheets from KiCad footprints.

The three connector drawings are parsed from the candidate .kicad_mod files.
No connector orientation is selected or frozen by this generator.
"""

from __future__ import annotations

import hashlib
import json
import re
from dataclasses import dataclass
from datetime import datetime, timezone
from html import escape
from pathlib import Path
from typing import Iterable

PAGE_W_MM = 210.0
PAGE_H_MM = 297.0
PT_PER_MM = 72.0 / 25.4
ACTUAL_CENTER = (105.0, 103.0)
DETAIL_CENTER = (105.0, 183.0)
DETAIL_SCALE = 4.0


def repo_root() -> Path:
    here = Path(__file__).resolve()
    for parent in here.parents:
        if (parent / ".git").exists():
            return parent
    raise RuntimeError("repository root not found")


ROOT = repo_root()
FIT_DIR = Path(__file__).resolve().parent
LIB_DIR = ROOT / "hardware/cadenza/libraries/display-fpc-variants"
MOD_DIR = LIB_DIR / "Cadenza_Display_FPC_Variants.pretty"
PDF_PATH = ROOT / "output/pdf/cadenza-physical-fit-sheets-a4.pdf"


@dataclass(frozen=True)
class Pad:
    number: str
    x: float
    y: float
    width: float
    height: float


@dataclass(frozen=True)
class Footprint:
    source: Path
    name: str
    mpn: str
    lcsc: str
    contact_side: str
    body: tuple[float, float, float, float]
    pads: tuple[Pad, ...]

    @property
    def signal_pads(self) -> tuple[Pad, ...]:
        return tuple(p for p in self.pads if p.number.isdigit())

    @property
    def mount_pads(self) -> tuple[Pad, ...]:
        return tuple(p for p in self.pads if p.number == "MP")

    @property
    def identifier(self) -> str:
        if self.contact_side == "bottom":
            return "fh12_bottom"
        if self.contact_side == "top":
            return "fh12a_top"
        if self.contact_side == "top_and_bottom":
            return "fh34srj_dual"
        raise ValueError(f"unknown contact side: {self.contact_side}")


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def _property(text: str, key: str) -> str:
    match = re.search(rf'\(property\s+"{re.escape(key)}"\s+"([^"]*)"', text)
    if not match:
        raise ValueError(f"missing property {key}")
    return match.group(1)


def parse_footprint(path: Path) -> Footprint:
    text = path.read_text(encoding="utf-8")
    name_match = re.search(r'^\(footprint\s+"([^"]+)"', text)
    if not name_match:
        raise ValueError(f"missing footprint name: {path}")

    body_match = re.search(
        r'\(fp_rect\s+\(start\s+([-+\d.]+)\s+([-+\d.]+)\)\s+'
        r'\(end\s+([-+\d.]+)\s+([-+\d.]+)\).*?\(layer\s+"F\.Fab"\)\)',
        text,
        re.S,
    )
    if not body_match:
        raise ValueError(f"missing F.Fab body rectangle: {path}")
    body = tuple(float(v) for v in body_match.groups())

    pad_pattern = re.compile(
        r'\(pad\s+"([^"]+)"\s+smd\s+[^\n]*?\(at\s+([-+\d.]+)\s+([-+\d.]+)'
        r'(?:\s+[-+\d.]+)?\)\s+\(size\s+([-+\d.]+)\s+([-+\d.]+)\)',
    )
    pads = tuple(
        Pad(number=m.group(1), x=float(m.group(2)), y=float(m.group(3)),
            width=float(m.group(4)), height=float(m.group(5)))
        for m in pad_pattern.finditer(text)
    )
    signal = [p for p in pads if p.number.isdigit()]
    if sorted(int(p.number) for p in signal) != list(range(1, 11)):
        raise ValueError(f"expected exactly signal pads 1..10: {path}")
    if len([p for p in pads if p.number == "MP"]) != 2:
        raise ValueError(f"expected exactly two MP pads: {path}")

    return Footprint(
        source=path,
        name=name_match.group(1),
        mpn=_property(text, "MPN"),
        lcsc=_property(text, "LCSC"),
        contact_side=_property(text, "ContactSide"),
        body=body,  # type: ignore[arg-type]
        pads=pads,
    )


def load_footprints() -> list[Footprint]:
    footprints = [parse_footprint(path) for path in sorted(MOD_DIR.glob("*.kicad_mod"))]
    if len(footprints) != 3:
        raise RuntimeError(f"expected 3 footprint files, found {len(footprints)}")
    return footprints


class Scene:
    def __init__(self) -> None:
        self.ops: list[dict] = []

    def line(self, x1: float, y1: float, x2: float, y2: float, *,
             stroke: str = "#111111", width: float = 0.25, dash: str | None = None,
             element_id: str | None = None) -> None:
        self.ops.append({"type": "line", "x1": x1, "y1": y1, "x2": x2, "y2": y2,
                         "stroke": stroke, "width": width, "dash": dash, "id": element_id})

    def rect(self, x: float, y: float, width: float, height: float, *,
             stroke: str = "#111111", fill: str = "none", line_width: float = 0.25,
             dash: str | None = None, element_id: str | None = None,
             data: dict[str, str | float] | None = None) -> None:
        self.ops.append({"type": "rect", "x": x, "y": y, "w": width, "h": height,
                         "stroke": stroke, "fill": fill, "width": line_width,
                         "dash": dash, "id": element_id, "data": data or {}})

    def text(self, x: float, y: float, value: str, *, size: float = 3.2,
             weight: str = "normal", fill: str = "#111111", anchor: str = "start",
             element_id: str | None = None) -> None:
        self.ops.append({"type": "text", "x": x, "y": y, "value": value,
                         "size": size, "weight": weight, "fill": fill,
                         "anchor": anchor, "id": element_id})

    def circle(self, cx: float, cy: float, r: float, *, stroke: str = "#111111",
               fill: str = "none", line_width: float = 0.25,
               element_id: str | None = None) -> None:
        self.ops.append({"type": "circle", "cx": cx, "cy": cy, "r": r,
                         "stroke": stroke, "fill": fill, "width": line_width,
                         "id": element_id})


def page_frame(scene: Scene, page_title: str, page_number: int) -> None:
    scene.rect(8, 8, 194, 281, stroke="#606060", line_width=0.25)
    scene.text(12, 16, "CADENZA PHYSICAL FIT GATE", size=5.0, weight="bold")
    scene.text(198, 16, f"A4 / Page {page_number} of 4", size=2.7, anchor="end")
    scene.text(12, 24, page_title, size=4.3, weight="bold")
    scene.rect(12, 28, 186, 16, stroke="#b00020", fill="#fff2f4", line_width=0.35)
    scene.text(15, 33.5, "PRINT AT 100% / ACTUAL SIZE. DISABLE FIT TO PAGE AND SHRINK.",
               size=3.4, weight="bold", fill="#b00020")
    scene.text(15, 39.2, "Measure the 10 mm and 50 mm rulers first. If either is wrong, STOP.",
               size=3.0, weight="bold", fill="#b00020")
    scene.line(12, 282, 198, 282, stroke="#808080", width=0.2)
    scene.text(12, 287, "Printed geometry cannot replace powered-off Pin 1 continuity; orientation is NOT FROZEN.",
               size=2.45, weight="bold", fill="#b00020")


def calibration(scene: Scene) -> None:
    y = 54.0
    scene.text(12, 49, "PRINT SCALE CALIBRATION", size=3.2, weight="bold")
    scene.line(20, y, 70, y, width=0.5, element_id="calibration-50mm")
    scene.line(20, y - 2, 20, y + 2, width=0.5)
    scene.line(70, y - 2, 70, y + 2, width=0.5)
    for x in range(25, 70, 5):
        scene.line(x, y - 1, x, y + 1, width=0.25)
    scene.text(45, y + 6, "50.0 mm", size=2.8, weight="bold", anchor="middle")
    scene.line(91, y, 101, y, width=0.5, element_id="calibration-10mm")
    scene.line(91, y - 2, 91, y + 2, width=0.5)
    scene.line(101, y - 2, 101, y + 2, width=0.5)
    scene.text(96, y + 6, "10.0 mm", size=2.8, weight="bold", anchor="middle")
    scene.rect(127, 49, 50, 10, stroke="#111111", fill="none", line_width=0.35,
               element_id="calibration-box-50x10")
    scene.text(152, 63, "50.0 x 10.0 mm box", size=2.6, anchor="middle")


def add_footprint(scene: Scene, fp: Footprint, center: tuple[float, float], scale: float,
                  prefix: str) -> None:
    cx, cy = center
    x1, y1, x2, y2 = fp.body
    scene.rect(cx + x1 * scale, cy + y1 * scale,
               (x2 - x1) * scale, (y2 - y1) * scale,
               stroke="#1c4c76", fill="#eaf4fb", line_width=0.2 if scale == 1 else 0.35,
               element_id=f"{prefix}-body",
               data={"local-xmin": x1, "local-ymin": y1,
                     "local-xmax": x2, "local-ymax": y2,
                     "center-x": cx, "center-y": cy, "scale": scale})
    for index, pad in enumerate(fp.pads):
        is_pin1 = pad.number == "1"
        fill = "#d51f45" if is_pin1 else ("#b87a16" if pad.number == "MP" else "#d8aa35")
        element_id = f"{prefix}-pad-{pad.number}-{index}" if pad.number == "MP" else f"{prefix}-pad-{pad.number}"
        scene.rect(cx + (pad.x - pad.width / 2) * scale,
                   cy + (pad.y - pad.height / 2) * scale,
                   pad.width * scale, pad.height * scale,
                   stroke="#5c3b00", fill=fill, line_width=0.12 if scale == 1 else 0.25,
                   element_id=element_id,
                   data={"number": pad.number, "local-x": pad.x, "local-y": pad.y,
                         "local-width": pad.width, "local-height": pad.height,
                         "center-x": cx, "center-y": cy, "scale": scale})
        if is_pin1:
            label_y = cy + (pad.y - pad.height / 2) * scale - 1.5
            scene.text(cx + pad.x * scale, label_y,
                       "PAD 1", size=2.3 if scale == 1 else 2.8,
                       weight="bold", fill="#b00020", anchor="middle")
            scene.line(cx + pad.x * scale, label_y + 0.8,
                       cx + pad.x * scale,
                       cy + (pad.y - pad.height / 2) * scale - 0.2,
                       stroke="#b00020", width=0.18 if scale == 1 else 0.3)


def connector_scene(fp: Footprint, page_number: int) -> Scene:
    scene = Scene()
    page_frame(scene, f"1:1 CONNECTOR OVERLAY - {fp.mpn}", page_number)
    calibration(scene)
    scene.text(12, 72, "1:1 TOP VIEW - place the loose connector here only after ruler check",
               size=3.2, weight="bold")
    scene.text(12, 77, "View direction: looking at the PCB front/copper side. Red = terminal Pad 1.", size=2.8)
    add_footprint(scene, fp, ACTUAL_CENTER, 1.0, "actual")

    x1, y1, x2, y2 = fp.body
    body_w, body_h = x2 - x1, y2 - y1
    signals = sorted(fp.signal_pads, key=lambda p: int(p.number))
    pitch = abs(signals[1].x - signals[0].x)
    pin1 = next(p for p in signals if p.number == "1")
    mp = fp.mount_pads[0]
    scene.text(12, 122, f"Source footprint: {fp.name}", size=2.5)
    scene.text(12, 127, f"MPN: {fp.mpn}    LCSC candidate: {fp.lcsc}", size=2.5)
    scene.text(12, 132, f"Contact face: {fp.contact_side.replace('_', ' ')}", size=2.5, weight="bold")
    scene.text(12, 137, f"Body F.Fab envelope: {body_w:.2f} x {body_h:.2f} mm", size=2.5)
    scene.text(12, 142, f"Signal pads: 10 x {signals[0].width:.2f} x {signals[0].height:.2f} mm; pitch {pitch:.2f} mm", size=2.5)
    scene.text(12, 147, f"Pad 1 local center: X {pin1.x:+.2f}, Y {pin1.y:+.2f} mm", size=2.5)
    scene.text(12, 152, f"Mount pads: 2 x {mp.width:.2f} x {mp.height:.2f} mm", size=2.5)

    scene.rect(42, 160, 126, 62, stroke="#b00020", fill="none", line_width=0.35, dash="2,1")
    scene.text(105, 166, "4X DETAIL - NOT FOR FIT OR MEASUREMENT", size=3.2,
               weight="bold", fill="#b00020", anchor="middle")
    add_footprint(scene, fp, DETAIL_CENTER, DETAIL_SCALE, "detail")
    scene.text(12, 232, "PHYSICAL CHECK (write by hand)", size=3.2, weight="bold")
    prompts = [
        "[ ] 50 mm ruler = ______ mm     [ ] 10 mm ruler = ______ mm",
        "[ ] Loose part body aligns with printed body: YES / NO / UNCLEAR",
        "[ ] Signal-pad row and two mount pads align: YES / NO / UNCLEAR",
        "Observed Pin 1 mark on real part: __________________________________________",
        "Contact metal seen at insertion opening: TOP / BOTTOM / BOTH / UNCLEAR",
        "Photo filename / note: ____________________________________________________",
    ]
    for i, prompt in enumerate(prompts):
        scene.text(16, 240 + i * 6.2, prompt, size=2.7)
    scene.text(12, 278, "This overlay cannot prove LCD-FPC Pin 1. Use the powered-off continuity test.",
               size=2.8, weight="bold", fill="#b00020")
    return scene


def record_scene() -> Scene:
    scene = Scene()
    page_frame(scene, "CALIPER RECORD - EC12 / 6x6 SWITCH / FPC ADAPTER", 4)
    calibration(scene)
    scene.text(12, 70, "Session", size=3.2, weight="bold")
    scene.text(16, 76, "Date: __________  Caliper ID: __________  Zero checked: YES / NO", size=2.8)
    scene.text(16, 82, "Operator: __________________________  Units: mm  Photos folder: __________________", size=2.8)

    scene.text(12, 92, "EC12 ROTARY ENCODER - measure twice", size=3.2, weight="bold")
    ec12_rows = [
        "Metal body width", "Metal body length", "Body height from PCB plane",
        "Threaded sleeve OD / height", "Shaft OD / exposed length",
        "Shaft type + D-flat depth", "Mechanical-tab center spacing",
        "A-B-C electrical pin pitch", "Push travel / detents per turn",
    ]
    table(scene, 12, 97, ["Item", "Read 1", "Read 2", "Notes"], ec12_rows,
          [75, 25, 25, 61], 5.7)

    scene.text(12, 157, "6x6 TACT SWITCH - three samples", size=3.2, weight="bold")
    tact_rows = [
        "Plastic body L x W", "PCB plane to body top", "PCB plane to actuator top",
        "Actuator diameter / width", "Same-side pin pitch", "Row-to-row pin spacing",
    ]
    table(scene, 12, 162, ["Item", "Sample 1", "Sample 2", "Sample 3"], tact_rows,
          [75, 37, 37, 37], 5.7)

    scene.text(12, 207, "10P FPC-TO-2.54 ADAPTER - do not infer Pin 1 from silkscreen", size=3.2, weight="bold")
    adapter_rows = [
        "PCB length x width x thickness", "Connector body L x W x H",
        "Header pin pitch / row offset", "Connector offset from PCB edges",
        "Opening contact metal", "Board Pin 1 mark + location",
        "Locked FPC natural exit direction", "LCD front-up copper face",
    ]
    table(scene, 12, 212, ["Item", "Measured / observed", "Photo / notes"], adapter_rows,
          [75, 55, 56], 5.7)
    scene.text(12, 271, "DECISION: leave blank until continuity evidence is attached.",
               size=2.8, weight="bold", fill="#b00020")
    scene.text(16, 277, "Chosen connector / PCB rotation: __________________________________  NOT FROZEN", size=2.7)
    return scene


def table(scene: Scene, x: float, y: float, headers: list[str], rows: list[str],
          widths: list[float], row_h: float) -> None:
    total_w = sum(widths)
    total_h = row_h * (len(rows) + 1)
    scene.rect(x, y, total_w, total_h, stroke="#444444", line_width=0.2)
    pos = x
    for width in widths[:-1]:
        pos += width
        scene.line(pos, y, pos, y + total_h, stroke="#777777", width=0.18)
    for row in range(1, len(rows) + 1):
        scene.line(x, y + row * row_h, x + total_w, y + row * row_h,
                   stroke="#777777", width=0.18)
    pos = x
    for header, width in zip(headers, widths):
        scene.text(pos + 1.5, y + 3.8, header, size=2.35, weight="bold")
        pos += width
    for row_index, label in enumerate(rows):
        scene.text(x + 1.5, y + (row_index + 1) * row_h + 3.8, label, size=2.2)


def svg_for(scene: Scene, metadata: dict[str, str]) -> str:
    out = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{PAGE_W_MM:g}mm" height="{PAGE_H_MM:g}mm" '
        f'viewBox="0 0 {PAGE_W_MM:g} {PAGE_H_MM:g}" data-print-scale="1.0">',
        "  <title>Cadenza physical fit gate</title>",
        "  <metadata>" + escape(json.dumps(metadata, sort_keys=True)) + "</metadata>",
        '  <rect x="0" y="0" width="210" height="297" fill="#ffffff"/>',
    ]
    for op in scene.ops:
        element_id = f' id="{escape(op["id"])}"' if op.get("id") else ""
        if op["type"] == "line":
            dash = f' stroke-dasharray="{op["dash"]}"' if op.get("dash") else ""
            out.append(f'  <line{element_id} x1="{op["x1"]:.4f}" y1="{op["y1"]:.4f}" '
                       f'x2="{op["x2"]:.4f}" y2="{op["y2"]:.4f}" '
                       f'stroke="{op["stroke"]}" stroke-width="{op["width"]:.4f}"{dash}/>' )
        elif op["type"] == "rect":
            dash = f' stroke-dasharray="{op["dash"]}"' if op.get("dash") else ""
            data = "".join(f' data-{escape(k)}="{escape(str(v))}"' for k, v in op.get("data", {}).items())
            out.append(f'  <rect{element_id}{data} x="{op["x"]:.4f}" y="{op["y"]:.4f}" '
                       f'width="{op["w"]:.4f}" height="{op["h"]:.4f}" '
                       f'stroke="{op["stroke"]}" fill="{op["fill"]}" '
                       f'stroke-width="{op["width"]:.4f}"{dash}/>' )
        elif op["type"] == "circle":
            out.append(f'  <circle{element_id} cx="{op["cx"]:.4f}" cy="{op["cy"]:.4f}" '
                       f'r="{op["r"]:.4f}" stroke="{op["stroke"]}" fill="{op["fill"]}" '
                       f'stroke-width="{op["width"]:.4f}"/>')
        elif op["type"] == "text":
            out.append(f'  <text{element_id} x="{op["x"]:.4f}" y="{op["y"]:.4f}" '
                       f'font-family="Helvetica,Arial,sans-serif" font-size="{op["size"]:.4f}" '
                       f'font-weight="{op["weight"]}" fill="{op["fill"]}" '
                       f'text-anchor="{op["anchor"]}">{escape(op["value"])}</text>')
    out.append("</svg>")
    return "\n".join(out) + "\n"


def render_pdf(scenes: Iterable[Scene], path: Path) -> None:
    from reportlab.pdfgen.canvas import Canvas

    path.parent.mkdir(parents=True, exist_ok=True)
    canvas = Canvas(str(path), pagesize=(PAGE_W_MM * PT_PER_MM, PAGE_H_MM * PT_PER_MM),
                    pageCompression=0, invariant=1)
    for scene in scenes:
        for op in scene.ops:
            if op["type"] == "line":
                canvas.setStrokeColor(op["stroke"])
                canvas.setLineWidth(op["width"] * PT_PER_MM)
                canvas.setDash([float(v) * PT_PER_MM for v in op["dash"].split(",")]
                               if op.get("dash") else [])
                canvas.line(op["x1"] * PT_PER_MM, (PAGE_H_MM - op["y1"]) * PT_PER_MM,
                            op["x2"] * PT_PER_MM, (PAGE_H_MM - op["y2"]) * PT_PER_MM)
            elif op["type"] == "rect":
                canvas.setStrokeColor(op["stroke"])
                canvas.setFillColor(op["fill"] if op["fill"] != "none" else "#ffffff")
                canvas.setLineWidth(op["width"] * PT_PER_MM)
                canvas.setDash([float(v) * PT_PER_MM for v in op["dash"].split(",")]
                               if op.get("dash") else [])
                canvas.rect(op["x"] * PT_PER_MM,
                            (PAGE_H_MM - op["y"] - op["h"]) * PT_PER_MM,
                            op["w"] * PT_PER_MM, op["h"] * PT_PER_MM,
                            stroke=1, fill=0 if op["fill"] == "none" else 1)
            elif op["type"] == "circle":
                canvas.setStrokeColor(op["stroke"])
                canvas.setFillColor(op["fill"] if op["fill"] != "none" else "#ffffff")
                canvas.setLineWidth(op["width"] * PT_PER_MM)
                canvas.setDash([])
                canvas.circle(op["cx"] * PT_PER_MM, (PAGE_H_MM - op["cy"]) * PT_PER_MM,
                              op["r"] * PT_PER_MM, stroke=1,
                              fill=0 if op["fill"] == "none" else 1)
            elif op["type"] == "text":
                canvas.setFillColor(op["fill"])
                canvas.setFont("Helvetica-Bold" if op["weight"] == "bold" else "Helvetica",
                               op["size"] * PT_PER_MM)
                x = op["x"] * PT_PER_MM
                y = (PAGE_H_MM - op["y"]) * PT_PER_MM
                if op["anchor"] == "middle":
                    canvas.drawCentredString(x, y, op["value"])
                elif op["anchor"] == "end":
                    canvas.drawRightString(x, y, op["value"])
                else:
                    canvas.drawString(x, y, op["value"])
        canvas.showPage()
    canvas.save()


def main() -> None:
    footprints = load_footprints()
    scenes: list[Scene] = []
    outputs: list[Path] = []
    candidates = []
    for page_number, fp in enumerate(footprints, start=1):
        scene = connector_scene(fp, page_number)
        scenes.append(scene)
        svg_path = FIT_DIR / f"fit-sheet-{fp.identifier}-a4.svg"
        source_rel = fp.source.relative_to(ROOT).as_posix()
        svg_path.write_text(svg_for(scene, {
            "candidate": fp.identifier,
            "source": source_rel,
            "source_sha256": sha256(fp.source),
            "scale": "1:1",
            "orientation_frozen": "false",
        }), encoding="utf-8")
        outputs.append(svg_path)
        candidates.append({
            "id": fp.identifier,
            "mpn": fp.mpn,
            "lcsc": fp.lcsc,
            "contact_side": fp.contact_side,
            "orientation_frozen": False,
            "source": source_rel,
            "source_sha256": sha256(fp.source),
            "body": {"xmin": fp.body[0], "ymin": fp.body[1],
                     "xmax": fp.body[2], "ymax": fp.body[3]},
            "pads": [pad.__dict__ for pad in fp.pads],
            "svg": svg_path.relative_to(ROOT).as_posix(),
        })

    records = record_scene()
    scenes.append(records)
    record_svg = FIT_DIR / "fit-sheet-caliper-record-a4.svg"
    record_svg.write_text(svg_for(records, {
        "purpose": "EC12, 6x6 tact switch, and FPC adapter caliper record",
        "scale": "1:1",
        "orientation_frozen": "false",
    }), encoding="utf-8")
    outputs.append(record_svg)

    render_pdf(scenes, PDF_PATH)
    outputs.append(PDF_PATH)
    manifest = {
        "schema_version": 1,
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "generator": Path(__file__).relative_to(ROOT).as_posix(),
        "page": {"format": "A4", "width_mm": PAGE_W_MM, "height_mm": PAGE_H_MM,
                 "pdf_pages": 4, "print_scale": 1.0},
        "orientation_frozen": False,
        "warnings": [
            "Print at 100% / Actual Size; disable Fit to Page and Shrink.",
            "Measure the 10 mm and 50 mm rulers before using any overlay.",
            "Printed geometry cannot replace the powered-off Pin 1 continuity test.",
        ],
        "candidates": candidates,
        "record_svg": record_svg.relative_to(ROOT).as_posix(),
        "pdf": PDF_PATH.relative_to(ROOT).as_posix(),
        "outputs": [],
    }
    for output in outputs:
        manifest["outputs"].append({
            "path": output.relative_to(ROOT).as_posix(),
            "sha256": sha256(output),
            "bytes": output.stat().st_size,
        })
    manifest_path = FIT_DIR / "fit-sheets-manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"Generated {len(outputs)} artifacts; PDF: {PDF_PATH.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
