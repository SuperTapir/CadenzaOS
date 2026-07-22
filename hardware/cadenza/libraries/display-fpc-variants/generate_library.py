#!/usr/bin/env python3
"""Generate the isolated display FPC candidate footprint library and proxy STEP files."""

from __future__ import annotations

import json
import sys
from pathlib import Path


HERE = Path(__file__).resolve().parent
SPECS = HERE / "footprint-specs.json"
PRETTY = HERE / "Cadenza_Display_FPC_Variants.pretty"
MODELS = HERE / "3dmodels"
OCP_CACHE = Path("/Users/tapir/.cache/cadenza-cad-tools")


def q(value: str) -> str:
    return value.replace("\\", "\\\\").replace('"', '\\"')


def footprint_text(c: dict) -> str:
    body = c["body"]
    sig = c["signal_pad"]
    name = c["footprint"]
    lines = [
        f'(footprint "{q(name)}"',
        '  (version 20240108)',
        '  (generator "cadenza-display-fpc-generator")',
        '  (layer "F.Cu")',
        f'  (descr "CANDIDATE ONLY; {q(c["mpn"])}; {q(c["contact_side"])}; verify physical FPC before selection")',
        f'  (tags "FPC 10P 0.5mm {q(c["contact_side"])} candidate not-frozen")',
        '  (property "Reference" "J**" (at 0 -3.6 0) (layer "F.SilkS")',
        '    (effects (font (size 1 1) (thickness 0.15))))',
        f'  (property "Value" "{q(name)}" (at 0 {body["ymax"] + 1.2:.3f} 0) (layer "F.Fab")',
        '    (effects (font (size 0.8 0.8) (thickness 0.12))))',
        f'  (property "MPN" "{q(c["mpn"])}" (at 0 0 0) (layer "F.Fab") hide',
        '    (effects (font (size 1 1) (thickness 0.15))))',
        f'  (property "LCSC" "{q(c["lcsc"])}" (at 0 0 0) (layer "F.Fab") hide',
        '    (effects (font (size 1 1) (thickness 0.15))))',
        f'  (property "ContactSide" "{q(c["contact_side"])}" (at 0 0 0) (layer "F.Fab") hide',
        '    (effects (font (size 1 1) (thickness 0.15))))',
        '  (attr smd)',
        f'  (fp_rect (start {body["xmin"]:.3f} {body["ymin"]:.3f}) (end {body["xmax"]:.3f} {body["ymax"]:.3f})',
        '    (stroke (width 0.10) (type solid)) (fill none) (layer "F.Fab"))',
        f'  (fp_rect (start {body["xmin"] - 0.5:.3f} {body["ymin"] - 0.5:.3f}) (end {body["xmax"] + 0.5:.3f} {body["ymax"] + 0.5:.3f})',
        '    (stroke (width 0.05) (type solid)) (fill none) (layer "F.CrtYd"))',
        f'  (fp_line (start {body["xmin"]:.3f} {body["ymin"]:.3f}) (end {body["xmax"]:.3f} {body["ymin"]:.3f})',
        '    (stroke (width 0.12) (type solid)) (layer "F.SilkS"))',
        f'  (fp_line (start {body["xmin"]:.3f} {body["ymax"]:.3f}) (end {body["xmax"]:.3f} {body["ymax"]:.3f})',
        '    (stroke (width 0.12) (type solid)) (layer "F.SilkS"))',
    ]
    # Pin 1 triangle is deliberately placed on the actual manufacturer-defined side.
    p1x = c["pin1_x"]
    mark_y = sig["y"] - sig["height"] / 2 - 0.55
    s = -1 if p1x < 0 else 1
    lines += [
        f'  (fp_poly (pts (xy {p1x:.3f} {mark_y:.3f}) (xy {p1x - 0.35*s:.3f} {mark_y - 0.55:.3f}) (xy {p1x + 0.35*s:.3f} {mark_y - 0.55:.3f}))',
        '    (stroke (width 0.10) (type solid)) (fill solid) (layer "F.SilkS"))',
        f'  (fp_text user "{q(c["contact_side"].upper())}" (at 0 {body["ymax"] - 0.45:.3f}) (layer "F.Fab")',
        '    (effects (font (size 0.65 0.65) (thickness 0.10))))',
    ]
    for pin in range(1, 11):
        x = c["pin1_x"] + (pin - 1) * c["pad_pitch"]
        shape = "roundrect" if pin == 1 else "rect"
        extra = ' (roundrect_rratio 0.20)' if pin == 1 else ''
        lines.append(
            f'  (pad "{pin}" smd {shape} (at {x:.3f} {sig["y"]:.3f}) '
            f'(size {sig["width"]:.3f} {sig["height"]:.3f}) '
            f'(layers "F.Cu" "F.Paste" "F.Mask"){extra})'
        )
    for mp in c["mount_pads"]:
        lines.append(
            f'  (pad "MP" smd rect (at {mp["x"]:.3f} {mp["y"]:.3f}) '
            f'(size {mp["width"]:.3f} {mp["height"]:.3f}) '
            '(layers "F.Cu" "F.Paste" "F.Mask"))'
        )
    lines += [
        f'  (model "{q(c["model"]["path"])}"',
        '    (offset (xyz 0 0 0)) (scale (xyz 1 1 1)) (rotate (xyz 0 0 0)))',
        ')',
        '',
    ]
    return "\n".join(lines)


def load_ocp():
    if str(OCP_CACHE) not in sys.path:
        sys.path.insert(0, str(OCP_CACHE))
    from OCP.BRepPrimAPI import BRepPrimAPI_MakeBox
    from OCP.IFSelect import IFSelect_RetDone
    from OCP.STEPControl import STEPControl_AsIs, STEPControl_Writer
    from OCP.gp import gp_Pnt
    return locals()


def write_proxy_step(c: dict) -> None:
    if "PROXY.step" not in c["model"]["path"]:
        return
    api = load_ocp()
    b = c["body"]
    shape = api["BRepPrimAPI_MakeBox"](
        api["gp_Pnt"](b["xmin"], b["ymin"], 0),
        b["xmax"] - b["xmin"], b["ymax"] - b["ymin"], b["height"]
    ).Shape()
    writer = api["STEPControl_Writer"]()
    writer.Transfer(shape, api["STEPControl_AsIs"])
    filename = c["model"]["path"].split("/")[-1]
    status = writer.Write(str(MODELS / filename))
    if status != api["IFSelect_RetDone"]:
        raise RuntimeError(f"STEP write failed: {filename}")


def main() -> int:
    data = json.loads(SPECS.read_text(encoding="utf-8"))
    PRETTY.mkdir(parents=True, exist_ok=True)
    MODELS.mkdir(parents=True, exist_ok=True)
    for c in data["candidates"]:
        (PRETTY / f'{c["footprint"]}.kicad_mod').write_text(footprint_text(c), encoding="utf-8")
        write_proxy_step(c)
    print(json.dumps({"footprints": len(data["candidates"]), "proxy_steps": 2, "status": data["library_status"]}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
