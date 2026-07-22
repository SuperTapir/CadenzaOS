#!/usr/bin/env python3
"""Generate non-frozen L2 input-layout fit-check candidates.

The candidates inherit the L1 Sharp front shell, 130 x 60 mm PCB datum and
four mounting-hole centres.  They intentionally use generic EC12/button
envelopes: the user's exact shaft, body, locating tabs and actuator heights
must be measured before any candidate can be frozen.
"""

from __future__ import annotations

import json
import math
import sys
from pathlib import Path


OCP_CACHE = Path("/Users/tapir/.cache/cadenza-cad-tools")
HERE = Path(__file__).resolve().parent
REPO = HERE.parents[3]
L1_FRONT = REPO / "hardware/cadenza/mechanical/l1-fit-check/generated/l1-front-fitcheck.step"
SCREEN_STEP = REPO / "hardware/cadenza/mechanical/shared/generated/ls027b7dh01-envelope.step"

PCB = (0.0, 0.0, 130.0, 60.0)
MOUNT_HOLES = ((5.0, 5.0), (5.0, 55.0), (125.0, 5.0), (125.0, 55.0))
MOUNT_KEEPOUT_R = 5.0
SCREEN_PANEL = (33.6, 8.59, 96.4, 51.41)
SCREEN_VIEW = (35.6, 12.36, 94.4, 47.64)
FPC_RELIEF = (60.18, 50.8, 69.82, 58.0)
TOP_ZMIN = -3.0002643070850947
TOP_ZMAX = 0.0003659072890944156
SCREEN_FRONT_Z = -3.4002643070850946

# Openings inherited from the game-console top and closed in every L2 candidate.
OLD_ROUND_OPENINGS = (
    (10.375, 36.05, 7.41),
    (20.875, 29.5, 7.41),
    (13.2, 56.0, 7.41),
    (22.4, 56.0, 7.41),
    (107.6, 56.0, 7.41),
    (116.8, 56.0, 7.41),
)
OLD_RECT_OPENING = (109.0, 24.0, 122.5, 36.0)

# Dimensions below are conservative proxies, not selected component dimensions.
BUTTON_PROXY = {
    "B": {"shell_hole_d": 8.4, "cap_d": 10.5, "cap_h": 2.8, "body_xy": [6.2, 6.2], "body_h": 5.0},
    "Menu": {"shell_hole_d": 6.8, "cap_d": 8.0, "cap_h": 2.2, "body_xy": [6.2, 6.2], "body_h": 5.0},
}
ENCODER_PROXY = {
    "shell_hole_d": 7.8,
    "body_xy": [12.5, 13.5],
    "body_h": 7.0,
    "bushing_d": 7.5,
    "shaft_envelope_d": 6.2,
    "shaft_external_h": 5.0,
}

VARIANTS = {
    "a-balanced": {
        "intent": "对称、间距宽松；作为首选纸面/打印手感基线",
        "B": [19.0, 29.0],
        "Menu": [18.0, 43.0],
        "encoder": [112.0, 30.0],
        "knob_d": 18.0,
        "knob_h": 5.5,
    },
    "b-compact": {
        "intent": "左键更紧凑、右旋钮更小；为屏幕和外缘保留更多空白",
        "B": [20.0, 27.0],
        "Menu": [20.0, 39.0],
        "encoder": [111.0, 30.0],
        "knob_d": 16.0,
        "knob_h": 5.0,
    },
    "c-thumb-arc": {
        "intent": "Menu 向屏幕侧上移，两个左键沿假设拇指弧排列",
        "B": [18.0, 31.0],
        "Menu": [23.0, 44.0],
        "encoder": [112.0, 32.0],
        "knob_d": 18.0,
        "knob_h": 5.5,
    },
}

LEFT_THUMB_PIVOT = (7.0, 30.0)
RIGHT_THUMB_PIVOT = (123.0, 30.0)


def load_api():
    if str(OCP_CACHE) not in sys.path:
        sys.path.insert(0, str(OCP_CACHE))
    from OCP.BRep import BRep_Builder
    from OCP.BRepAlgoAPI import BRepAlgoAPI_Common, BRepAlgoAPI_Cut, BRepAlgoAPI_Fuse
    from OCP.BRepBuilderAPI import BRepBuilderAPI_Transform
    from OCP.BRepCheck import BRepCheck_Analyzer
    from OCP.BRepMesh import BRepMesh_IncrementalMesh
    from OCP.BRepPrimAPI import BRepPrimAPI_MakeBox, BRepPrimAPI_MakeCylinder
    from OCP.BRepGProp import BRepGProp
    from OCP.GProp import GProp_GProps
    from OCP.IFSelect import IFSelect_RetDone
    from OCP.STEPControl import STEPControl_AsIs, STEPControl_Reader, STEPControl_Writer
    from OCP.StlAPI import StlAPI_Writer
    from OCP.TopoDS import TopoDS_Compound
    from OCP.gp import gp_Ax2, gp_Dir, gp_Pnt, gp_Trsf, gp_Vec

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


def cylinder(api, x, y, z1, radius, height):
    axis = api["gp_Ax2"](api["gp_Pnt"](x, y, z1), api["gp_Dir"](0, 0, 1))
    return api["BRepPrimAPI_MakeCylinder"](axis, radius, height).Shape()


def cut(api, base, tool):
    op = api["BRepAlgoAPI_Cut"](base, tool)
    op.Build()
    if not op.IsDone():
        raise RuntimeError("Boolean cut failed")
    return op.Shape()


def fuse(api, a, b):
    op = api["BRepAlgoAPI_Fuse"](a, b)
    op.Build()
    if not op.IsDone():
        raise RuntimeError("Boolean fuse failed")
    return op.Shape()


def compound(api, shapes):
    result = api["TopoDS_Compound"]()
    builder = api["BRep_Builder"]()
    builder.MakeCompound(result)
    for shape in shapes:
        builder.Add(result, shape)
    return result


def translate_z(api, shape, offset):
    transform = api["gp_Trsf"]()
    transform.SetTranslation(api["gp_Vec"](0, 0, offset))
    return api["BRepBuilderAPI_Transform"](shape, transform, True).Shape()


def common_volume(api, a, b):
    op = api["BRepAlgoAPI_Common"](a, b)
    op.Build()
    if not op.IsDone():
        raise RuntimeError("Boolean common failed")
    props = api["GProp_GProps"]()
    api["BRepGProp"].VolumeProperties_s(op.Shape(), props)
    return props.Mass()


def validate(api, name, shape):
    if shape.IsNull() or not api["BRepCheck_Analyzer"](shape).IsValid():
        raise RuntimeError(f"Invalid shape: {name}")


def write_step(api, shape, path):
    writer = api["STEPControl_Writer"]()
    writer.Transfer(shape, api["STEPControl_AsIs"])
    if writer.Write(str(path)) != api["IFSelect_RetDone"]:
        raise RuntimeError(f"STEP write failed: {path}")


def write_stl(api, shape, path):
    api["BRepMesh_IncrementalMesh"](shape, 0.08, False, 0.35, True)
    if not api["StlAPI_Writer"]().Write(shape, str(path)):
        raise RuntimeError(f"STL write failed: {path}")


def distance(a, b):
    return math.hypot(a[0] - b[0], a[1] - b[1])


def rect_circle_clearance(rect, centre, radius):
    x1, y1, x2, y2 = rect
    x, y = centre
    dx = max(x1 - x, 0.0, x - x2)
    dy = max(y1 - y, 0.0, y - y2)
    if dx or dy:
        return math.hypot(dx, dy) - radius
    return -min(x - x1, x2 - x, y - y1, y2 - y) - radius


def close_old_openings(api, front):
    result = front
    for x, y, diameter in OLD_ROUND_OPENINGS:
        result = fuse(api, result, cylinder(api, x, y, TOP_ZMIN, diameter / 2 + 0.25, TOP_ZMAX - TOP_ZMIN))
    x1, y1, x2, y2 = OLD_RECT_OPENING
    result = fuse(api, result, box(api, x1 - 0.25, y1 - 0.25, TOP_ZMIN, x2 + 0.25, y2 + 0.25, TOP_ZMAX))
    return result


def control_shapes(api, variant):
    through = {}
    caps = {}
    bodies = {}
    for name in ("B", "Menu"):
        x, y = variant[name]
        p = BUTTON_PROXY[name]
        through[name] = cylinder(api, x, y, TOP_ZMIN - 0.5, p["shell_hole_d"] / 2, TOP_ZMAX - TOP_ZMIN + 1.0)
        caps[name] = cylinder(api, x, y, TOP_ZMIN - 0.65 - p["cap_h"], p["cap_d"] / 2, p["cap_h"])
        bx, by = p["body_xy"]
        bodies[name] = box(api, x - bx / 2, y - by / 2, TOP_ZMAX + 0.15, x + bx / 2, y + by / 2, TOP_ZMAX + 0.15 + p["body_h"])

    ex, ey = variant["encoder"]
    through["encoder"] = cylinder(api, ex, ey, TOP_ZMIN - 0.5, ENCODER_PROXY["shell_hole_d"] / 2, TOP_ZMAX - TOP_ZMIN + 1.0)
    ebx, eby = ENCODER_PROXY["body_xy"]
    bodies["encoder"] = box(api, ex - ebx / 2, ey - eby / 2, TOP_ZMAX + 0.15, ex + ebx / 2, ey + eby / 2, TOP_ZMAX + 0.15 + ENCODER_PROXY["body_h"])
    shaft = cylinder(api, ex, ey, TOP_ZMIN - ENCODER_PROXY["shaft_external_h"], ENCODER_PROXY["shaft_envelope_d"] / 2, ENCODER_PROXY["shaft_external_h"] + 0.5)
    knob = cylinder(api, ex, ey, TOP_ZMIN - 0.8 - variant["knob_h"], variant["knob_d"] / 2, variant["knob_h"])
    caps["encoder-shaft"] = shaft
    caps["A-knob-envelope"] = knob
    return through, caps, bodies


def circle_metrics(variant):
    controls = {
        "B": (variant["B"], BUTTON_PROXY["B"]["cap_d"] / 2),
        "Menu": (variant["Menu"], BUTTON_PROXY["Menu"]["cap_d"] / 2),
        "A-knob": (variant["encoder"], variant["knob_d"] / 2),
    }
    result = {}
    for name, (centre, radius) in controls.items():
        x, y = centre
        result[name] = {
            "centre_xy_mm": centre,
            "visible_proxy_radius_mm": radius,
            "screen_panel_clearance_mm": round(rect_circle_clearance(SCREEN_PANEL, centre, radius), 3),
            "pcb_edge_clearance_mm": round(min(x - radius, 130 - x - radius, y - radius, 60 - y - radius), 3),
            "nearest_mount_keepout_clearance_mm": round(min(distance(centre, h) - radius - MOUNT_KEEPOUT_R for h in MOUNT_HOLES), 3),
            "nominal_thumb_pivot_distance_mm": round(distance(centre, LEFT_THUMB_PIVOT if name != "A-knob" else RIGHT_THUMB_PIVOT), 3),
        }
    result["B_to_Menu_edge_clearance_mm"] = round(
        distance(variant["B"], variant["Menu"])
        - BUTTON_PROXY["B"]["cap_d"] / 2
        - BUTTON_PROXY["Menu"]["cap_d"] / 2,
        3,
    )
    return result


def svg_plan(variant_name, variant, metrics):
    scale = 6
    def sx(x): return x * scale
    def sy(y): return (60 - y) * scale
    colours = {"B": "#d34a4a", "Menu": "#e3a43b", "A-knob": "#3276b1"}
    circles = []
    for name, centre_key in (("B", "B"), ("Menu", "Menu"), ("A-knob", "encoder")):
        cx, cy = variant[centre_key]
        radius = metrics[name]["visible_proxy_radius_mm"]
        circles.append(f'<circle cx="{sx(cx):.1f}" cy="{sy(cy):.1f}" r="{sx(radius):.1f}" fill="{colours[name]}" fill-opacity="0.55" stroke="{colours[name]}" stroke-width="2"/>')
        circles.append(f'<text x="{sx(cx):.1f}" y="{sy(cy)+4:.1f}" font-size="12" text-anchor="middle" font-family="sans-serif">{name}</text>')
    mounts = "".join(f'<circle cx="{sx(x):.1f}" cy="{sy(y):.1f}" r="{sx(MOUNT_KEEPOUT_R):.1f}" fill="none" stroke="#777" stroke-dasharray="4 3"/>' for x,y in MOUNT_HOLES)
    x1,y1,x2,y2 = SCREEN_PANEL
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="780" height="360" viewBox="0 0 780 360">
<rect x="0" y="0" width="780" height="360" fill="#f7f7f5" stroke="#111" stroke-width="2"/>
<rect x="{sx(x1)}" y="{sy(y2)}" width="{sx(x2-x1)}" height="{sx(y2-y1)}" fill="#d9e6d2" stroke="#314b2d" stroke-width="2"/>
<text x="390" y="180" text-anchor="middle" font-family="sans-serif" font-size="15">Sharp panel</text>
{mounts}
<ellipse cx="{sx(LEFT_THUMB_PIVOT[0])}" cy="{sy(LEFT_THUMB_PIVOT[1])}" rx="{sx(24)}" ry="{sx(22)}" fill="none" stroke="#777" stroke-dasharray="7 4"/>
<ellipse cx="{sx(RIGHT_THUMB_PIVOT[0])}" cy="{sy(RIGHT_THUMB_PIVOT[1])}" rx="{sx(24)}" ry="{sx(22)}" fill="none" stroke="#777" stroke-dasharray="7 4"/>
{''.join(circles)}
<text x="10" y="22" font-family="sans-serif" font-size="14">{variant_name}: candidate only / NOT FROZEN</text>
</svg>\n'''


def main() -> int:
    api = load_api()
    output = HERE / "generated"
    output.mkdir(parents=True, exist_ok=True)
    base = close_old_openings(api, read_step(api, L1_FRONT))
    screen = translate_z(api, read_step(api, SCREEN_STEP), SCREEN_FRONT_Z)

    report = {
        "status": "candidate only; selection_frozen=false",
        "coordinate_system": "PCB XY: x=0..130 left-to-right, y=0..60 bottom-to-top; top shell z=-3..0",
        "inherited_datums": {"pcb_xy_mm": PCB, "mount_hole_centres_xy_mm": MOUNT_HOLES, "screen_panel_xy_mm": SCREEN_PANEL, "screen_view_xy_mm": SCREEN_VIEW, "screen_front_z_mm": SCREEN_FRONT_Z, "fpc_relief_xy_mm": FPC_RELIEF},
        "proxy_dimensions_not_component_selection": {"buttons": BUTTON_PROXY, "encoder": ENCODER_PROXY},
        "variants": {},
        "physical_measurements_required_before_freeze": [
            "EC12 body length/width/height including metal tabs",
            "EC12 PCB locating-tab count, position, width and insertion depth",
            "EC12 electrical pin count, pitch and footprint orientation",
            "EC12 shaft diameter, round/D-flat/knurled shape, flat depth and angular index",
            "EC12 shaft length above bushing and threaded-bushing diameter/height",
            "EC12 push travel, push force and knob-to-shell bottom clearance",
            "B/Menu switch body, actuator diameter/height, travel and cap attachment geometry",
            "assembled PCB-to-front-shell inner-face distance",
            "printed hole/shaft compensation for the chosen material and printer",
        ],
    }

    mount_keepouts = compound(api, [cylinder(api, x, y, -10, MOUNT_KEEPOUT_R, 20) for x, y in MOUNT_HOLES])
    for name, variant in VARIANTS.items():
        variant_dir = output / name
        variant_dir.mkdir(parents=True, exist_ok=True)
        through, caps, bodies = control_shapes(api, variant)
        shell = base
        for cutter in through.values():
            shell = cut(api, shell, cutter)
        cap_proxy = compound(api, list(caps.values()))
        body_proxy = compound(api, list(bodies.values()))
        control_proxy = compound(api, [cap_proxy, body_proxy])
        assembly = compound(api, [shell, screen, control_proxy])

        for part_name, shape in {
            "front-candidate": shell,
            "button-keycap-proxies": compound(api, [caps["B"], caps["Menu"]]),
            "encoder-a-knob-proxy": compound(api, [caps["encoder-shaft"], caps["A-knob-envelope"]]),
            "control-body-keepout-proxies": body_proxy,
            "assembly": assembly,
        }.items():
            validate(api, f"{name}/{part_name}", shape)
            write_step(api, shape, variant_dir / f"{name}-{part_name}.step")
            write_stl(api, shape, variant_dir / f"{name}-{part_name}.stl")

        collision = {
            "screen_vs_control_proxy_mm3": common_volume(api, screen, control_proxy),
            "mount_keepouts_vs_control_proxy_mm3": common_volume(api, mount_keepouts, control_proxy),
            "shell_vs_external_caps_and_knob_mm3": common_volume(api, shell, cap_proxy),
            "shell_vs_interior_body_proxies_mm3": common_volume(api, shell, body_proxy),
        }
        metrics = circle_metrics(variant)
        if any(abs(value) > 1e-5 for value in collision.values()):
            raise RuntimeError(f"{name} has hard collision: {collision}")
        if min(metrics[c]["screen_panel_clearance_mm"] for c in ("B", "Menu", "A-knob")) < 3.0:
            raise RuntimeError(f"{name} violates 3 mm screen-panel clearance")
        if min(metrics[c]["nearest_mount_keepout_clearance_mm"] for c in ("B", "Menu", "A-knob")) < 2.0:
            raise RuntimeError(f"{name} violates 2 mm mount-keepout margin")

        variant_record = {**variant, "metrics": metrics, "hard_collision_common_volume_mm3": collision}
        report["variants"][name] = variant_record
        (variant_dir / f"{name}-plan.svg").write_text(svg_plan(name, variant, metrics), encoding="utf-8")
        (variant_dir / f"{name}-parameters.json").write_text(json.dumps(variant_record, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    (output / "l2-input-candidates.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"output": str(output), "variants": list(VARIANTS), "selection_frozen": False}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
