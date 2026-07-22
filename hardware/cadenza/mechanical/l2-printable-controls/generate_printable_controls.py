#!/usr/bin/env python3
"""Generate isolated, non-frozen L2 printable control candidates.

The geometry deliberately keeps all switch/shaft fits parameterised.  It
reuses the A/B/C centres and already-cut candidate front shells, but never
edits those inputs or any formal KiCad/OpenSpec artifact.
"""

from __future__ import annotations

import hashlib
import json
import math
import shutil
import sys
from pathlib import Path


OCP_CACHE = Path("/Users/tapir/.cache/cadenza-cad-tools")
HERE = Path(__file__).resolve().parent
REPO = HERE.parents[3]
LAYOUT_ROOT = REPO / "hardware/cadenza/mechanical/l2-input-candidates"
LAYOUT_MANIFEST = LAYOUT_ROOT / "generated/l2-input-candidates.json"
OUTPUT = HERE / "generated"

SHELL_Z_MIN = -3.0002643070850947
SCREEN_PANEL = (33.6, 8.59, 96.4, 51.41)
MOUNT_HOLES = ((5.0, 5.0), (5.0, 55.0), (125.0, 5.0), (125.0, 55.0))
MOUNT_KEEPOUT_R = 5.0

# These are deliberately test-print assumptions, never selected component data.
PARAMETERS = {
    "status": "candidate-only; dimensions-not-frozen",
    "selection_frozen": False,
    "production_ready": False,
    "units": "mm",
    "source": {
        "layout_manifest": str(LAYOUT_MANIFEST.relative_to(REPO)),
        "coordinate_system": "PCB XY from existing L2 candidates; front-shell exterior is negative Z",
    },
    "print_process_assumption": {
        "process": "FDM test coupon / fit-check only",
        "nominal_nozzle_diameter": 0.4,
        "nominal_layer_height": 0.2,
        "xy_fit_clearance_per_side": 0.10,
        "recommended_orientation": "outer labelled face on bed; stem/hub upward; verify first-layer label quality",
        "material": "not selected",
    },
    "buttons": {
        "B": {
            "cap_d": 10.5,
            "cap_h": 2.8,
            "stem_od": 7.7,
            "stem_h": 2.2,
            "shell_hole_d_inherited": 8.4,
            "actuator_nominal_d_assumption": 3.5,
            "actuator_socket_d": 3.75,
            "actuator_socket_depth": 1.7,
            "unpressed_shell_gap": 0.65,
            "press_travel_proxy": 0.35,
        },
        "Menu": {
            "cap_d": 8.0,
            "cap_h": 2.2,
            "stem_od": 6.1,
            "stem_h": 2.0,
            "shell_hole_d_inherited": 6.8,
            "actuator_nominal_d_assumption": 3.5,
            "actuator_socket_d": 3.75,
            "actuator_socket_depth": 1.5,
            "unpressed_shell_gap": 0.65,
            "press_travel_proxy": 0.35,
        },
    },
    "knob": {
        "shell_hole_d_inherited": 7.8,
        "hub_od": 7.1,
        "hub_h": 2.8,
        "unpressed_shell_gap": 0.8,
        "press_travel_proxy": 0.5,
        "top_solid_thickness": 1.25,
        "a_mark_recess_depth": 0.45,
        "press_interface": "the entire A-marked encoder knob moves axially with the EC12 push switch",
    },
    "shaft_interface_candidates": {
        "round": {
            "intent": "friction-fit coupon for an assumed round shaft",
            "shaft_nominal_d_assumption": 6.0,
            "bore_d": 6.25,
        },
        "d-shaft": {
            "intent": "friction-fit coupon for an assumed D shaft; flat angle remains replaceable",
            "shaft_nominal_d_assumption": 6.0,
            "bore_d": 6.25,
            "flat_depth_from_circle_assumption": 0.8,
            "flat_angle_deg": 0.0,
        },
        "knurled-adapter": {
            "intent": "knob accepts a separately printable keyed insert; only the insert changes after shaft measurement",
            "socket_d": 8.4,
            "insert_od": 8.15,
            "key_width": 2.0,
            "socket_key_extension": 1.0,
            "insert_key_extension": 0.8,
            "insert_length": 3.5,
            "knurled_proxy_polygon_sides": 12,
            "knurled_proxy_across_flats_assumption": 5.75,
        },
    },
    "mandatory_measurements_before_freeze": [
        "EC12 shaft type: round, D-flat or knurled",
        "shaft maximum/minimum diameter at several heights",
        "D-flat depth and angular index, if present",
        "knurl major/minor diameter and tooth count, if present",
        "usable shaft length above bushing and bushing diameter/height",
        "EC12 axial push travel and knob clearance at full press",
        "B/Menu tactile-switch actuator diameter, height and travel",
        "assembled PCB-to-front-shell distance",
        "printer/material hole shrink and first-layer compensation",
    ],
}


def load_api():
    if str(OCP_CACHE) not in sys.path:
        sys.path.insert(0, str(OCP_CACHE))
    from OCP.BRep import BRep_Builder
    from OCP.BRepAlgoAPI import BRepAlgoAPI_Common, BRepAlgoAPI_Cut, BRepAlgoAPI_Fuse
    from OCP.BRepBuilderAPI import (
        BRepBuilderAPI_MakeFace,
        BRepBuilderAPI_MakePolygon,
        BRepBuilderAPI_Transform,
    )
    from OCP.BRepCheck import BRepCheck_Analyzer
    from OCP.BRepMesh import BRepMesh_IncrementalMesh
    from OCP.BRepPrimAPI import BRepPrimAPI_MakeBox, BRepPrimAPI_MakeCylinder, BRepPrimAPI_MakePrism
    from OCP.BRepGProp import BRepGProp
    from OCP.GProp import GProp_GProps
    from OCP.IFSelect import IFSelect_RetDone
    from OCP.STEPControl import STEPControl_AsIs, STEPControl_Reader, STEPControl_Writer
    from OCP.StlAPI import StlAPI_Writer
    from OCP.TopoDS import TopoDS_Compound
    from OCP.gp import gp_Ax1, gp_Ax2, gp_Dir, gp_Pnt, gp_Trsf, gp_Vec
    return locals()


def cylinder(api, x, y, z, radius, height):
    axis = api["gp_Ax2"](api["gp_Pnt"](x, y, z), api["gp_Dir"](0, 0, 1))
    return api["BRepPrimAPI_MakeCylinder"](axis, radius, height).Shape()


def box(api, x1, y1, z1, x2, y2, z2):
    return api["BRepPrimAPI_MakeBox"](
        api["gp_Pnt"](x1, y1, z1), x2 - x1, y2 - y1, z2 - z1
    ).Shape()


def polygon_prism(api, points, z, height):
    wire = api["BRepBuilderAPI_MakePolygon"]()
    for x, y in points:
        wire.Add(api["gp_Pnt"](x, y, z))
    wire.Close()
    face = api["BRepBuilderAPI_MakeFace"](wire.Wire()).Face()
    return api["BRepPrimAPI_MakePrism"](face, api["gp_Vec"](0, 0, height)).Shape()


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


def common(api, a, b):
    op = api["BRepAlgoAPI_Common"](a, b)
    op.Build()
    if not op.IsDone():
        raise RuntimeError("Boolean common failed")
    return op.Shape()


def volume(api, shape):
    props = api["GProp_GProps"]()
    api["BRepGProp"].VolumeProperties_s(shape, props)
    return props.Mass()


def common_volume(api, a, b):
    return volume(api, common(api, a, b))


def compound(api, shapes):
    result = api["TopoDS_Compound"]()
    builder = api["BRep_Builder"]()
    builder.MakeCompound(result)
    for shape in shapes:
        builder.Add(result, shape)
    return result


def transform(api, shape, *, dx=0.0, dy=0.0, dz=0.0, rotate_x_180=False):
    current = shape
    if rotate_x_180:
        rot = api["gp_Trsf"]()
        rot.SetRotation(api["gp_Ax1"](api["gp_Pnt"](0, 0, 0), api["gp_Dir"](1, 0, 0)), math.pi)
        current = api["BRepBuilderAPI_Transform"](current, rot, True).Shape()
    tr = api["gp_Trsf"]()
    tr.SetTranslation(api["gp_Vec"](dx, dy, dz))
    return api["BRepBuilderAPI_Transform"](current, tr, True).Shape()


def read_step(api, path):
    reader = api["STEPControl_Reader"]()
    if reader.ReadFile(str(path)) != api["IFSelect_RetDone"]:
        raise RuntimeError(f"Cannot read STEP: {path}")
    reader.TransferRoots()
    shape = reader.OneShape()
    if shape.IsNull():
        raise RuntimeError(f"Null STEP: {path}")
    return shape


def validate(api, name, shape):
    if shape.IsNull() or not api["BRepCheck_Analyzer"](shape).IsValid():
        raise RuntimeError(f"Invalid BRep: {name}")


def write_pair(api, shape, stem):
    validate(api, stem.name, shape)
    stem.parent.mkdir(parents=True, exist_ok=True)
    step = stem.with_suffix(".step")
    stl = stem.with_suffix(".stl")
    writer = api["STEPControl_Writer"]()
    writer.Transfer(shape, api["STEPControl_AsIs"])
    if writer.Write(str(step)) != api["IFSelect_RetDone"]:
        raise RuntimeError(f"STEP write failed: {step}")
    api["BRepMesh_IncrementalMesh"](shape, 0.06, False, 0.30, True)
    if not api["StlAPI_Writer"]().Write(shape, str(stl)):
        raise RuntimeError(f"STL write failed: {stl}")
    return {"step": str(step.relative_to(HERE)), "stl": str(stl.relative_to(HERE))}


def groove_box(api, x1, y1, x2, y2, z, depth):
    return box(api, x1, y1, z, x2, y2, z + depth)


def button_cap(api, name):
    p = PARAMETERS["buttons"][name]
    cap = cylinder(api, 0, 0, 0, p["cap_d"] / 2, p["cap_h"])
    stem = cylinder(api, 0, 0, -p["stem_h"], p["stem_od"] / 2, p["stem_h"] + 0.25)
    result = fuse(api, cap, stem)
    socket = cylinder(
        api,
        0,
        0,
        -p["stem_h"] - 0.1,
        p["actuator_socket_d"] / 2,
        p["actuator_socket_depth"] + 0.1,
    )
    result = cut(api, result, socket)
    groove_z = p["cap_h"] - 0.42
    if name == "B":
        cutters = [
            groove_box(api, -1.55, -1.9, -0.85, 1.9, groove_z, 0.5),
            cylinder(api, -0.35, 0.95, groove_z, 0.62, 0.5),
            cylinder(api, -0.35, -0.95, groove_z, 0.62, 0.5),
        ]
    else:
        cutters = [groove_box(api, -2.0, y - 0.22, 2.0, y + 0.22, groove_z, 0.5) for y in (-0.9, 0, 0.9)]
    for tool in cutters:
        result = cut(api, result, tool)
    return result


def a_mark_cutters(api, top_z, depth):
    # A font-independent recessed mark: two diagonal slots and one crossbar.
    cutters = []
    for sign in (-1, 1):
        leg = groove_box(api, -0.34, -2.2, 0.34, 2.2, top_z - depth, depth + 0.12)
        rot = api["gp_Trsf"]()
        rot.SetRotation(api["gp_Ax1"](api["gp_Pnt"](0, 0, 0), api["gp_Dir"](0, 0, 1)), sign * math.radians(19))
        leg = api["BRepBuilderAPI_Transform"](leg, rot, True).Shape()
        leg = transform(api, leg, dx=sign * 0.72)
        cutters.append(leg)
    cutters.append(groove_box(api, -1.1, -0.15, 1.1, 0.35, top_z - depth, depth + 0.12))
    return cutters


def keyed_profile(api, radius, key_extension, key_width, z, height):
    body = cylinder(api, 0, 0, z, radius, height)
    tab = box(api, radius - 0.2, -key_width / 2, z, radius + key_extension, key_width / 2, z + height)
    return fuse(api, body, tab)


def shaft_cutter(api, interface, z, height):
    p = PARAMETERS["shaft_interface_candidates"][interface]
    if interface == "round":
        return cylinder(api, 0, 0, z, p["bore_d"] / 2, height)
    if interface == "d-shaft":
        radius = p["bore_d"] / 2
        circular = cylinder(api, 0, 0, z, radius, height)
        flat_x = radius - p["flat_depth_from_circle_assumption"]
        clip = box(api, -radius - 1, -radius - 1, z - 0.1, flat_x, radius + 1, z + height + 0.1)
        return common(api, circular, clip)
    raise ValueError(interface)


def knob(api, diameter, height, interface):
    kp = PARAMETERS["knob"]
    body = cylinder(api, 0, 0, 0, diameter / 2, height)
    hub = cylinder(api, 0, 0, -kp["hub_h"], kp["hub_od"] / 2, kp["hub_h"] + 0.4)
    result = fuse(api, body, hub)
    if interface in ("round", "d-shaft"):
        bore = shaft_cutter(api, interface, -kp["hub_h"] - 0.2, kp["hub_h"] + height - kp["top_solid_thickness"] + 0.2)
        result = cut(api, result, bore)
    else:
        p = PARAMETERS["shaft_interface_candidates"][interface]
        # The wide removable insert lives only in the knob body, outside the
        # front shell.  A smaller access bore continues through the hub; this
        # keeps the moving insert inside the inherited Ø7.8 mm shell opening.
        socket = keyed_profile(
            api,
            p["socket_d"] / 2,
            p["socket_key_extension"],
            p["key_width"] + 0.25,
            -0.05,
            height - kp["top_solid_thickness"] + 0.1,
        )
        shaft_access = cylinder(api, 0, 0, -kp["hub_h"] - 0.2, 3.25, kp["hub_h"] + 0.3)
        result = cut(api, cut(api, result, socket), shaft_access)
    for cutter in a_mark_cutters(api, height, kp["a_mark_recess_depth"]):
        result = cut(api, result, cutter)
    return result


def adapter_insert(api):
    p = PARAMETERS["shaft_interface_candidates"]["knurled-adapter"]
    outer = keyed_profile(api, p["insert_od"] / 2, p["insert_key_extension"], p["key_width"], 0, p["insert_length"])
    sides = p["knurled_proxy_polygon_sides"]
    apothem = p["knurled_proxy_across_flats_assumption"] / 2
    radius = apothem / math.cos(math.pi / sides)
    points = [(radius * math.cos(2 * math.pi * i / sides), radius * math.sin(2 * math.pi * i / sides)) for i in range(sides)]
    inner = polygon_prism(api, points, -0.1, p["insert_length"] + 0.2)
    return cut(api, outer, inner)


def circle_rect_clearance(rect, centre, radius):
    x1, y1, x2, y2 = rect
    x, y = centre
    dx = max(x1 - x, 0.0, x - x2)
    dy = max(y1 - y, 0.0, y - y2)
    if dx or dy:
        return math.hypot(dx, dy) - radius
    return -min(x - x1, x2 - x, y - y1, y2 - y) - radius


def main():
    api = load_api()
    source = json.loads(LAYOUT_MANIFEST.read_text(encoding="utf-8"))
    if source["status"] != "candidate only; selection_frozen=false":
        raise RuntimeError("Refusing to consume a source with unexpected freeze state")
    if OUTPUT.exists():
        shutil.rmtree(OUTPUT)
    OUTPUT.mkdir(parents=True)

    b_raw = button_cap(api, "B")
    menu_raw = button_cap(api, "Menu")
    adapter_raw = adapter_insert(api)
    # Print orientation: labelled face at Z=0, stem/hub upward.
    b_print = transform(api, b_raw, dz=PARAMETERS["buttons"]["B"]["cap_h"], rotate_x_180=True)
    menu_print = transform(api, menu_raw, dz=PARAMETERS["buttons"]["Menu"]["cap_h"], rotate_x_180=True)
    common_dir = OUTPUT / "common"
    files = {
        "B_keycap": write_pair(api, b_print, common_dir / "cadenza-b-keycap-candidate"),
        "Menu_keycap": write_pair(api, menu_print, common_dir / "cadenza-menu-keycap-candidate"),
        "knurled_adapter_insert": write_pair(api, adapter_raw, common_dir / "cadenza-knurled-adapter-insert-candidate"),
    }

    report = {
        **PARAMETERS,
        "source_sha256": hashlib.sha256(LAYOUT_MANIFEST.read_bytes()).hexdigest(),
        "files": files,
        "layouts": {},
    }
    mount_keepout = compound(api, [cylinder(api, x, y, -12, MOUNT_KEEPOUT_R, 24) for x, y in MOUNT_HOLES])

    for layout_name, layout in source["variants"].items():
        layout_dir = OUTPUT / layout_name
        shell_path = LAYOUT_ROOT / "generated" / layout_name / f"{layout_name}-front-candidate.step"
        shell = read_step(api, shell_path)
        b_xy, menu_xy, enc_xy = layout["B"], layout["Menu"], layout["encoder"]
        b_mount_z = SHELL_Z_MIN - PARAMETERS["buttons"]["B"]["unpressed_shell_gap"]
        menu_mount_z = SHELL_Z_MIN - PARAMETERS["buttons"]["Menu"]["unpressed_shell_gap"]
        knob_mount_z = SHELL_Z_MIN - PARAMETERS["knob"]["unpressed_shell_gap"]
        b_asm = transform(api, b_raw, dx=b_xy[0], dy=b_xy[1], dz=b_mount_z, rotate_x_180=True)
        menu_asm = transform(api, menu_raw, dx=menu_xy[0], dy=menu_xy[1], dz=menu_mount_z, rotate_x_180=True)
        external_controls = [b_asm, menu_asm]
        layout_record = {
            "centres_xy_mm": {"B": b_xy, "Menu": menu_xy, "encoder_A": enc_xy},
            "knob_d": layout["knob_d"],
            "knob_h": layout["knob_h"],
            "interface_candidates": {},
        }
        for interface in ("round", "d-shaft", "knurled-adapter"):
            knob_raw = knob(api, layout["knob_d"], layout["knob_h"], interface)
            knob_print = transform(api, knob_raw, dz=layout["knob_h"], rotate_x_180=True)
            knob_files = write_pair(api, knob_print, layout_dir / f"cadenza-a-knob-{interface}-candidate")
            knob_asm = transform(api, knob_raw, dx=enc_xy[0], dy=enc_xy[1], dz=knob_mount_z, rotate_x_180=True)
            parts = [shell, b_asm, menu_asm, knob_asm]
            insert_asm = None
            adapter_collision = 0.0
            if interface == "knurled-adapter":
                # Insert is seated in the external knob body, above the hub.
                insert_local = transform(api, adapter_raw, dz=0.10)
                adapter_collision = common_volume(api, knob_raw, insert_local)
                insert_asm = transform(api, insert_local, dx=enc_xy[0], dy=enc_xy[1], dz=knob_mount_z, rotate_x_180=True)
                parts.append(insert_asm)
            assembly = compound(api, parts)
            assembly_files = write_pair(api, assembly, layout_dir / f"{layout_name}-{interface}-control-assembly")
            all_controls = compound(api, parts[1:])
            pressed_parts = [
                transform(api, b_asm, dz=PARAMETERS["buttons"]["B"]["press_travel_proxy"]),
                transform(api, menu_asm, dz=PARAMETERS["buttons"]["Menu"]["press_travel_proxy"]),
                transform(api, knob_asm, dz=PARAMETERS["knob"]["press_travel_proxy"]),
            ]
            if insert_asm is not None:
                pressed_parts.append(transform(api, insert_asm, dz=PARAMETERS["knob"]["press_travel_proxy"]))
            pressed_controls = compound(api, pressed_parts)
            collision = {
                "shell_vs_unpressed_controls_mm3": common_volume(api, shell, all_controls),
                "shell_vs_pressed_controls_mm3": common_volume(api, shell, pressed_controls),
                "mount_keepouts_vs_controls_mm3": common_volume(api, mount_keepout, all_controls),
                "B_vs_Menu_mm3": common_volume(api, b_asm, menu_asm),
                "B_vs_A_knob_mm3": common_volume(api, b_asm, knob_asm),
                "Menu_vs_A_knob_mm3": common_volume(api, menu_asm, knob_asm),
                "adapter_insert_vs_knob_mm3": adapter_collision,
            }
            if any(abs(v) > 1e-5 for v in collision.values()):
                raise RuntimeError(f"{layout_name}/{interface} collision: {collision}")
            control_radii = {
                "B": PARAMETERS["buttons"]["B"]["cap_d"] / 2,
                "Menu": PARAMETERS["buttons"]["Menu"]["cap_d"] / 2,
                "encoder_A": layout["knob_d"] / 2,
            }
            clearances = {
                name: round(circle_rect_clearance(SCREEN_PANEL, centre, control_radii[name]), 3)
                for name, centre in {"B": b_xy, "Menu": menu_xy, "encoder_A": enc_xy}.items()
            }
            layout_record["interface_candidates"][interface] = {
                "part_files": knob_files,
                "assembly_files": assembly_files,
                "hard_collision_common_volume_mm3": collision,
                "screen_panel_clearance_mm": clearances,
                "press_travel_checked_mm": {
                    "B": PARAMETERS["buttons"]["B"]["press_travel_proxy"],
                    "Menu": PARAMETERS["buttons"]["Menu"]["press_travel_proxy"],
                    "encoder_A": PARAMETERS["knob"]["press_travel_proxy"],
                },
            }
        report["layouts"][layout_name] = layout_record

    (HERE / "parameters.json").write_text(json.dumps(PARAMETERS, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    (OUTPUT / "printable-controls-manifest.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"status": "generated", "layouts": len(report["layouts"]), "interfaces_per_layout": 3, "selection_frozen": False}))


if __name__ == "__main__":
    main()
