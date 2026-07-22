#!/usr/bin/env python3
"""Generate a conservative LS027B7DH01 mechanical datum model.

The model is positioned in the inherited 130 x 60 mm PCB coordinate system:
X grows to the right, Y grows toward the FPC edge, and Z=0 is the display's
front surface.  It intentionally does not guess the final board connector or
the folded FPC route; those require the physical display/adapter inspection.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path


OCP_CACHE = Path("/Users/tapir/.cache/cadenza-cad-tools")
HERE = Path(__file__).resolve().parent

PANEL_W = 62.8
PANEL_H = 42.82
PANEL_T = 1.64
VIEW_W = 58.8
VIEW_H = 35.28
FPC_W = 8.64
FPC_FLAT_LENGTH = 6.06
FPC_T = 0.30
BEND_START = 0.8
BEND_END = 6.0
MIN_BEND_R = 0.45

PCB_CENTER_X = 65.0
PCB_CENTER_Y = 30.0


def box(api, x, y, z, dx, dy, dz):
    return api["BRepPrimAPI_MakeBox"](api["gp_Pnt"](x, y, z), dx, dy, dz).Shape()


def compound(api, shapes):
    result = api["TopoDS_Compound"]()
    builder = api["BRep_Builder"]()
    builder.MakeCompound(result)
    for shape in shapes:
        builder.Add(result, shape)
    return result


def write_step(api, shape, path: Path):
    writer = api["STEPControl_Writer"]()
    writer.Transfer(shape, api["STEPControl_AsIs"])
    status = writer.Write(str(path))
    if status != api["IFSelect_RetDone"]:
        raise RuntimeError(f"STEP write failed for {path}: {status}")


def write_stl(api, shape, path: Path):
    api["BRepMesh_IncrementalMesh"](shape, 0.05, False, 0.25, True)
    writer = api["StlAPI_Writer"]()
    if not writer.Write(shape, str(path)):
        raise RuntimeError(f"STL write failed for {path}")


def load_api():
    if str(OCP_CACHE) not in sys.path:
        sys.path.insert(0, str(OCP_CACHE))
    from OCP.BRep import BRep_Builder
    from OCP.BRepMesh import BRepMesh_IncrementalMesh
    from OCP.BRepPrimAPI import BRepPrimAPI_MakeBox
    from OCP.IFSelect import IFSelect_RetDone
    from OCP.STEPControl import STEPControl_AsIs, STEPControl_Writer
    from OCP.StlAPI import StlAPI_Writer
    from OCP.TopoDS import TopoDS_Compound
    from OCP.gp import gp_Pnt

    return locals()


def main() -> int:
    api = load_api()
    output = HERE / "generated"
    output.mkdir(parents=True, exist_ok=True)

    left = PCB_CENTER_X - PANEL_W / 2
    top = PCB_CENTER_Y - PANEL_H / 2
    right = left + PANEL_W
    bottom = top + PANEL_H
    view_left = PCB_CENTER_X - VIEW_W / 2
    view_top = PCB_CENTER_Y - VIEW_H / 2
    fpc_left = PCB_CENTER_X - FPC_W / 2

    glass = box(api, left, top, -PANEL_T, PANEL_W, PANEL_H, PANEL_T)
    flat_fpc = box(
        api,
        fpc_left,
        bottom,
        -PANEL_T,
        FPC_W,
        FPC_FLAT_LENGTH,
        FPC_T,
    )
    bend_keepout = box(
        api,
        fpc_left,
        bottom + BEND_START,
        -PANEL_T - 2 * MIN_BEND_R,
        FPC_W,
        BEND_END - BEND_START,
        FPC_T + 2 * MIN_BEND_R,
    )
    envelope = compound(api, [glass, flat_fpc])

    write_step(api, glass, output / "ls027b7dh01-glass.step")
    write_step(api, flat_fpc, output / "ls027b7dh01-flat-fpc.step")
    write_step(api, bend_keepout, output / "ls027b7dh01-bend-keepout.step")
    write_step(api, envelope, output / "ls027b7dh01-envelope.step")
    write_stl(api, envelope, output / "ls027b7dh01-envelope.stl")

    data = {
        "coordinate_system": {
            "units": "mm",
            "origin": "inherited PCB top-left (0, 0); display centered at (65, 30)",
            "x_positive": "right",
            "y_positive": "toward the display FPC edge",
            "z_zero": "front polarizer outer surface",
            "z_positive": "toward viewer/front enclosure",
        },
        "panel": {
            "bbox": {"xmin": left, "xmax": right, "ymin": top, "ymax": bottom, "zmin": -PANEL_T, "zmax": 0.0},
            "size": [PANEL_W, PANEL_H, PANEL_T],
            "datasheet_tolerance_xy": 0.2,
        },
        "view_area": {
            "bbox_xy": {
                "xmin": view_left,
                "xmax": view_left + VIEW_W,
                "ymin": view_top,
                "ymax": view_top + VIEW_H,
            },
            "size": [VIEW_W, VIEW_H],
        },
        "fpc_flat_datasheet_envelope": {
            "bbox": {
                "xmin": fpc_left,
                "xmax": fpc_left + FPC_W,
                "ymin": bottom,
                "ymax": bottom + FPC_FLAT_LENGTH,
                "zmin": -PANEL_T,
                "zmax": -PANEL_T + FPC_T,
            },
            "width": FPC_W,
            "pitch": 0.5,
            "contact_count": 10,
            "nominal_thickness_at_stiffener": FPC_T,
        },
        "bend_constraints": {
            "recommended_distance_from_glass_edge": [BEND_START, BEND_END],
            "minimum_inner_radius": MIN_BEND_R,
            "reverse_bend_toward_polarizer": "forbidden",
            "maximum_repeat_bends": 3,
        },
        "connector": {
            "status": "placeholder_not_modeled",
            "reason": "Final top/bottom contact orientation and purchasable MPN require physical FPC inspection",
            "datasheet_examples": [
                "SMK CFP-4610-0150F bottom contact",
                "Molex 51441-1093 bottom contact",
                "SMK CFP-4510-0150F top contact",
            ],
        },
        "evidence": {
            "datasheet": "LS027B7DH01 Sharp LCY-1210401A",
            "outline": "PDF page 23, Figure 8-1",
            "bend": "PDF page 23, Figure 8-2",
            "status": "datasheet geometry; orientation relative to real device pending physical confirmation",
        },
    }
    (output / "ls027b7dh01-datums.json").write_text(
        json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    print(json.dumps({"output": str(output), "panel_bbox": data["panel"]["bbox"]}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
