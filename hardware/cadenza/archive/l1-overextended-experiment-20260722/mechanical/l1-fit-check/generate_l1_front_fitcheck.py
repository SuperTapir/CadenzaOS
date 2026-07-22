#!/usr/bin/env python3
"""Generate a reversible L1 front-shell fit-check from the frozen V7 STEP.

This is intentionally a mechanical prototype, not a production enclosure.
It preserves the reference outer shell and openings, closes the original TFT
window, cuts a Sharp view window, and adds separate gasket/retainer parts.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path


OCP_CACHE = Path("/Users/tapir/.cache/cadenza-cad-tools")
HERE = Path(__file__).resolve().parent
REPO = HERE.parents[3]
REFERENCE_TOP = REPO / "hardware/reference/oshwhub-project_jofcnupz/顶盖V7.step"
SCREEN_STEP = (
    REPO
    / "hardware/cadenza/mechanical/shared/generated/ls027b7dh01-envelope.step"
)

PANEL = (33.6, 8.59, 96.4, 51.41)
VIEW = (35.6, 12.36, 94.4, 47.64)
WINDOW = (35.0, 11.76, 95.0, 48.24)  # 0.60 mm outside active area
ORIGINAL_WINDOW = (27.63, 3.07, 102.37, 56.93)

TOP_ZMIN = -3.0002643070850947
TOP_ZMAX = 0.0003659072890944156
GASKET_T = 0.40
SCREEN_FRONT_Z = TOP_ZMIN - GASKET_T
SCREEN_T = 1.64
RETAINER_T = 1.00


def load_api():
    if str(OCP_CACHE) not in sys.path:
        sys.path.insert(0, str(OCP_CACHE))
    from OCP.BRep import BRep_Builder
    from OCP.BRepAlgoAPI import BRepAlgoAPI_Common, BRepAlgoAPI_Cut, BRepAlgoAPI_Fuse
    from OCP.BRepBuilderAPI import BRepBuilderAPI_Transform
    from OCP.BRepCheck import BRepCheck_Analyzer
    from OCP.BRepMesh import BRepMesh_IncrementalMesh
    from OCP.BRepPrimAPI import BRepPrimAPI_MakeBox
    from OCP.BRepGProp import BRepGProp
    from OCP.GProp import GProp_GProps
    from OCP.IFSelect import IFSelect_RetDone
    from OCP.STEPControl import STEPControl_AsIs, STEPControl_Reader, STEPControl_Writer
    from OCP.StlAPI import StlAPI_Writer
    from OCP.TopoDS import TopoDS_Compound
    from OCP.gp import gp_Pnt, gp_Trsf, gp_Vec

    return locals()


def read_step(api, path: Path):
    reader = api["STEPControl_Reader"]()
    if reader.ReadFile(str(path)) != api["IFSelect_RetDone"]:
        raise RuntimeError(f"Cannot read {path}")
    reader.TransferRoots()
    shape = reader.OneShape()
    if shape.IsNull():
        raise RuntimeError(f"Null shape from {path}")
    return shape


def box(api, x1, y1, z1, x2, y2, z2):
    return api["BRepPrimAPI_MakeBox"](
        api["gp_Pnt"](x1, y1, z1), x2 - x1, y2 - y1, z2 - z1
    ).Shape()


def cut(api, base, tool):
    operation = api["BRepAlgoAPI_Cut"](base, tool)
    operation.Build()
    if not operation.IsDone():
        raise RuntimeError("Boolean cut failed")
    return operation.Shape()


def fuse(api, a, b):
    operation = api["BRepAlgoAPI_Fuse"](a, b)
    operation.Build()
    if not operation.IsDone():
        raise RuntimeError("Boolean fuse failed")
    return operation.Shape()


def ring(api, outer, inner, z1, z2):
    return cut(api, box(api, *outer[:2], z1, *outer[2:], z2), box(api, *inner[:2], z1 - 0.1, *inner[2:], z2 + 0.1))


def compound(api, shapes):
    result = api["TopoDS_Compound"]()
    builder = api["BRep_Builder"]()
    builder.MakeCompound(result)
    for shape in shapes:
        builder.Add(result, shape)
    return result


def translate(api, shape, z):
    transform = api["gp_Trsf"]()
    transform.SetTranslation(api["gp_Vec"](0, 0, z))
    return api["BRepBuilderAPI_Transform"](shape, transform, True).Shape()


def validate(api, name, shape):
    if shape.IsNull() or not api["BRepCheck_Analyzer"](shape).IsValid():
        raise RuntimeError(f"Invalid generated shape: {name}")


def common_volume(api, a, b):
    operation = api["BRepAlgoAPI_Common"](a, b)
    operation.Build()
    if not operation.IsDone():
        raise RuntimeError("Boolean common failed")
    properties = api["GProp_GProps"]()
    api["BRepGProp"].VolumeProperties_s(operation.Shape(), properties)
    return properties.Mass()


def write_step(api, shape, path):
    writer = api["STEPControl_Writer"]()
    writer.Transfer(shape, api["STEPControl_AsIs"])
    if writer.Write(str(path)) != api["IFSelect_RetDone"]:
        raise RuntimeError(f"STEP write failed: {path}")


def write_stl(api, shape, path):
    api["BRepMesh_IncrementalMesh"](shape, 0.06, False, 0.3, True)
    if not api["StlAPI_Writer"]().Write(shape, str(path)):
        raise RuntimeError(f"STL write failed: {path}")


def main() -> int:
    api = load_api()
    output = HERE / "generated"
    output.mkdir(parents=True, exist_ok=True)

    reference_top = read_step(api, REFERENCE_TOP)
    screen = read_step(api, SCREEN_STEP)

    # Close the old opening without changing the inherited outer Z datum.
    x1, y1, x2, y2 = ORIGINAL_WINDOW
    closure = box(api, x1 - 0.20, y1 - 0.20, TOP_ZMIN, x2 + 0.20, y2 + 0.20, TOP_ZMAX)
    closed_top = fuse(api, reference_top, closure)
    wx1, wy1, wx2, wy2 = WINDOW
    window_tool = box(api, wx1, wy1, TOP_ZMIN - 1.0, wx2, wy2, TOP_ZMAX + 1.0)
    front = cut(api, closed_top, window_tool)

    # A 0.4 mm soft-gasket proxy supports only the non-view glass border.
    gasket = ring(api, PANEL, WINDOW, TOP_ZMIN - GASKET_T, TOP_ZMIN)

    # Loose fit-check retainer behind the screen.  Its FPC slot prevents the
    # rear frame from pressing on the tail.  Fastening features are deferred.
    retainer_outer = (32.8, 7.79, 97.2, 52.21)
    retainer_inner = (35.2, 12.0, 94.8, 48.0)
    screen_back_z = SCREEN_FRONT_Z - SCREEN_T
    retainer = ring(
        api,
        retainer_outer,
        retainer_inner,
        screen_back_z - RETAINER_T,
        screen_back_z,
    )
    fpc_relief = box(
        api,
        60.18,
        50.8,
        screen_back_z - RETAINER_T - 0.1,
        69.82,
        58.0,
        screen_back_z + 0.1,
    )
    retainer = cut(api, retainer, fpc_relief)
    assembled_screen = translate(api, screen, SCREEN_FRONT_Z)
    assembly = compound(api, [front, gasket, assembled_screen, retainer])

    collision_volumes = {
        "front_vs_gasket": common_volume(api, front, gasket),
        "front_vs_screen": common_volume(api, front, assembled_screen),
        "gasket_vs_screen": common_volume(api, gasket, assembled_screen),
        "screen_vs_retainer": common_volume(api, assembled_screen, retainer),
        "front_vs_retainer": common_volume(api, front, retainer),
        "gasket_vs_retainer": common_volume(api, gasket, retainer),
    }
    if any(abs(value) > 1e-6 for value in collision_volumes.values()):
        raise RuntimeError(f"Generated fit-check has hard collisions: {collision_volumes}")

    for name, shape in {
        "l1-front-fitcheck": front,
        "l1-screen-gasket-proxy": gasket,
        "l1-screen-retainer-fitcheck": retainer,
        "l1-front-screen-assembly": assembly,
    }.items():
        validate(api, name, shape)
        write_step(api, shape, output / f"{name}.step")
        write_stl(api, shape, output / f"{name}.stl")

    parameters = {
        "status": "fit-check only; not production frozen",
        "reference_top": str(REFERENCE_TOP),
        "reference_preserved": True,
        "original_window_xy": ORIGINAL_WINDOW,
        "sharp_panel_xy": PANEL,
        "sharp_view_xy": VIEW,
        "new_window_xy": WINDOW,
        "view_clearance_each_edge": 0.60,
        "gasket_proxy_thickness": GASKET_T,
        "screen_front_z": SCREEN_FRONT_Z,
        "screen_back_z": screen_back_z,
        "retainer_thickness": RETAINER_T,
        "fpc_relief_xy": [60.18, 50.8, 69.82, 58.0],
        "hard_collision_common_volume_mm3": collision_volumes,
        "pending": [
            "physical FPC side/contact/Pin-1 confirmation",
            "final connector STEP and folded FPC route",
            "retainer fastening method",
            "printed dimensional compensation",
            "real screen stress and closure test",
        ],
    }
    (output / "l1-front-fitcheck-parameters.json").write_text(
        json.dumps(parameters, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    print(json.dumps({"output": str(output), "window": WINDOW, "screen_front_z": SCREEN_FRONT_Z}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
