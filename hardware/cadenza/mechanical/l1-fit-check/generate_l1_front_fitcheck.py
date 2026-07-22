#!/usr/bin/env python3
"""Generate the current L1 Sharp-screen enclosure/PCB fit-check.

The screen/window position and PCB critical placements are tied to the current
screen-only KiCad candidate.  Z placement of the PCB remains an explicit
visualisation proxy because the imported reference does not encode a reliable
PCB-to-cover seating datum.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path


OCP_CACHE = Path("/Users/tapir/.cache/cadenza-cad-tools-py313")
HERE = Path(__file__).resolve().parent
REPO = HERE.parents[3]
REFERENCE_TOP = REPO / "hardware/reference/oshwhub-project_jofcnupz/顶盖V7.step"
PCB_EVIDENCE = HERE / "generated/candidate-pcb-mechanical.json"

# Enclosure XY.  The current PCB candidate moved the formerly-centred Sharp
# panel +1.5 mm X and -1.5 mm KiCad Y.  Since enclosure Y=139.725-KiCad-Y,
# that is +1.5 mm in both enclosure X and enclosure Y.
PANEL = (35.10, 10.09, 97.90, 52.91)
VIEW = (37.10, 13.86, 95.90, 49.14)
WINDOW = (36.50, 13.26, 96.50, 49.74)
ORIGINAL_WINDOW = (27.63, 3.07, 102.37, 56.93)

TOP_ZMIN = -3.0002643070850947
TOP_ZMAX = 0.0003659072890944156
GASKET_T = 0.40
SCREEN_T = 1.64
# Negative Z faces the viewer.  The screen rests outside the cover on the
# gasket: rear surface at -3.400..., front polarizer at -5.040....
SCREEN_REAR_Z = TOP_ZMIN - GASKET_T
SCREEN_FRONT_Z = SCREEN_REAR_Z - SCREEN_T
RETAINER_T = 1.00

FPC_W = 8.64
FPC_FLAT_LENGTH = 6.06
FPC_T = 0.30
BEND_START = 0.80
BEND_END = 6.00
MIN_BEND_R = 0.45
FPC_X = (66.50 - FPC_W / 2, 66.50 + FPC_W / 2)
FPC_FLAT_Y = (PANEL[1] - FPC_FLAT_LENGTH, PANEL[1])
FPC_BEND_Y = (PANEL[1] - BEND_END, PANEL[1] - BEND_START)
FPC_RELIEF = (FPC_X[0] - 0.50, FPC_FLAT_Y[0] - 0.50, FPC_X[1] + 0.50, PANEL[1] + 0.61)
# Candidate pass-through from the front-mounted display into the enclosure.
# It reaches 0.95 mm beyond the glass edge so the earliest permitted R0.45
# rear fold is not pinched.  The exposed lip still needs a real-print check.
FRONT_FPC_SLOT = (FPC_X[0] - 0.50, PANEL[1] - 0.95, FPC_X[1] + 0.50, WINDOW[1] + 0.10)

# This is only a common CAD viewing datum.  It is deliberately not a release
# dimension.  Component envelopes project from the F.Cu face toward -Z.
PCB_COMPONENT_FACE_Z_PROXY = 2.40
PCB_T = 1.60
FH34_BODY_HEIGHT = 1.00  # public Hirose drawing
USB_HEIGHT_PROXY = 4.00  # conservative; imported footprint lacks verified MPN height


def load_api():
    if str(OCP_CACHE) not in sys.path:
        sys.path.insert(0, str(OCP_CACHE))
    from OCP.BRep import BRep_Builder
    from OCP.BRepAlgoAPI import BRepAlgoAPI_Common, BRepAlgoAPI_Cut, BRepAlgoAPI_Fuse
    from OCP.BRepCheck import BRepCheck_Analyzer
    from OCP.BRepMesh import BRepMesh_IncrementalMesh
    from OCP.BRepPrimAPI import BRepPrimAPI_MakeBox, BRepPrimAPI_MakeCylinder
    from OCP.BRepGProp import BRepGProp
    from OCP.GProp import GProp_GProps
    from OCP.IFSelect import IFSelect_RetDone
    from OCP.STEPControl import STEPControl_AsIs, STEPControl_Reader, STEPControl_Writer
    from OCP.StlAPI import StlAPI_Writer
    from OCP.TopoDS import TopoDS_Compound
    from OCP.gp import gp_Ax2, gp_Dir, gp_Pnt
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
    return api["BRepPrimAPI_MakeBox"](api["gp_Pnt"](x1, y1, z1), x2-x1, y2-y1, z2-z1).Shape()


def cylinder(api, x, y, z, radius, height):
    axis = api["gp_Ax2"](api["gp_Pnt"](x, y, z), api["gp_Dir"](0, 0, 1))
    return api["BRepPrimAPI_MakeCylinder"](axis, radius, height).Shape()


def boolean(api, kind, a, b):
    op = (api["BRepAlgoAPI_Cut"] if kind == "cut" else api["BRepAlgoAPI_Fuse"])(a, b)
    op.Build()
    if not op.IsDone():
        raise RuntimeError(f"Boolean {kind} failed")
    return op.Shape()


def ring(api, outer, inner, z1, z2):
    return boolean(api, "cut", box(api, outer[0], outer[1], z1, outer[2], outer[3], z2),
                   box(api, inner[0], inner[1], z1-0.1, inner[2], inner[3], z2+0.1))


def compound(api, shapes):
    result = api["TopoDS_Compound"]()
    builder = api["BRep_Builder"]()
    builder.MakeCompound(result)
    for shape in shapes:
        builder.Add(result, shape)
    return result


def common_volume(api, a, b):
    op = api["BRepAlgoAPI_Common"](a, b)
    op.Build()
    props = api["GProp_GProps"]()
    api["BRepGProp"].VolumeProperties_s(op.Shape(), props)
    return props.Mass()


def validate(api, name, shape):
    if shape.IsNull() or not api["BRepCheck_Analyzer"](shape).IsValid():
        raise RuntimeError(f"Invalid generated shape: {name}")


def write_pair(api, shape, output: Path, stem: str):
    validate(api, stem, shape)
    sw = api["STEPControl_Writer"]()
    sw.Transfer(shape, api["STEPControl_AsIs"])
    if sw.Write(str(output / f"{stem}.step")) != api["IFSelect_RetDone"]:
        raise RuntimeError(f"STEP write failed: {stem}")
    api["BRepMesh_IncrementalMesh"](shape, 0.06, False, 0.3, True)
    if not api["StlAPI_Writer"]().Write(shape, str(output / f"{stem}.stl")):
        raise RuntimeError(f"STL write failed: {stem}")


def xy_box(api, rect, z1, z2):
    return box(api, rect[0], rect[1], z1, rect[2], rect[3], z2)


def main() -> int:
    api = load_api()
    output = HERE / "generated"
    output.mkdir(parents=True, exist_ok=True)
    pcb = json.loads(PCB_EVIDENCE.read_text(encoding="utf-8"))

    reference_top = read_step(api, REFERENCE_TOP)
    closure = xy_box(api, (ORIGINAL_WINDOW[0]-0.2, ORIGINAL_WINDOW[1]-0.2,
                           ORIGINAL_WINDOW[2]+0.2, ORIGINAL_WINDOW[3]+0.2), TOP_ZMIN, TOP_ZMAX)
    front = boolean(api, "cut", boolean(api, "fuse", reference_top, closure),
                    xy_box(api, WINDOW, TOP_ZMIN-1.0, TOP_ZMAX+1.0))
    front = boolean(api, "cut", front,
                    xy_box(api, FRONT_FPC_SLOT, TOP_ZMIN-1.0, TOP_ZMAX+1.0))
    gasket = ring(api, PANEL, WINDOW, SCREEN_REAR_Z, TOP_ZMIN)

    glass = xy_box(api, PANEL, SCREEN_FRONT_Z, SCREEN_REAR_Z)
    flat_fpc = xy_box(api, (*FPC_X[:1], FPC_FLAT_Y[0], *FPC_X[1:], FPC_FLAT_Y[1]),
                      SCREEN_REAR_Z, SCREEN_REAR_Z + FPC_T)
    bend_keepout = xy_box(api, (FPC_X[0], FPC_BEND_Y[0], FPC_X[1], FPC_BEND_Y[1]),
                          SCREEN_REAR_Z - 0.10,
                          PCB_COMPONENT_FACE_Z_PROXY - FH34_BODY_HEIGHT + 2*MIN_BEND_R)
    screen = compound(api, [glass, flat_fpc])

    # Interior loose-fit frame only.  It intentionally does not claim a
    # fastening or compression mechanism.
    retainer_outer = (34.30, 9.29, 98.70, 53.71)
    retainer_inner = (36.70, 13.50, 96.30, 49.50)
    retainer = ring(api, retainer_outer, retainer_inner, TOP_ZMAX, TOP_ZMAX + RETAINER_T)
    retainer = boolean(api, "cut", retainer,
                       xy_box(api, FPC_RELIEF, TOP_ZMAX-0.1, TOP_ZMAX+RETAINER_T+0.1))

    pcb_plane = box(api, 0, 0, PCB_COMPONENT_FACE_Z_PROXY, 130, 60,
                    PCB_COMPONENT_FACE_Z_PROXY + PCB_T)
    for hole in pcb["mounting_holes"]:
        x, y = hole["centre_enclosure_mm"]
        pcb_plane = boolean(api, "cut", pcb_plane,
                            cylinder(api, x, y, PCB_COMPONENT_FACE_Z_PROXY-0.1,
                                     hole["pcb_drill_diameter_mm"]/2,
                                     PCB_T+0.2))

    j_rect = pcb["critical_footprints"]["J_DISP1"]["courtyard_xy_enclosure_mm"]
    usb_rect = pcb["critical_footprints"]["USB1"]["courtyard_xy_enclosure_mm"]
    fh34 = xy_box(api, j_rect, PCB_COMPONENT_FACE_Z_PROXY-FH34_BODY_HEIGHT,
                  PCB_COMPONENT_FACE_Z_PROXY)
    usb = xy_box(api, usb_rect, PCB_COMPONENT_FACE_Z_PROXY-USB_HEIGHT_PROXY,
                 PCB_COMPONENT_FACE_Z_PROXY)

    courtyard_projection = compound(api, [
        xy_box(api, rect, PCB_COMPONENT_FACE_Z_PROXY+PCB_T+0.05,
               PCB_COMPONENT_FACE_Z_PROXY+PCB_T+0.10)
        for rect in pcb["front_courtyard_envelopes"].values()
    ])
    assembly = compound(api, [front, gasket, screen, retainer, pcb_plane, fh34, usb, bend_keepout])

    hard = {
        "front_vs_gasket": common_volume(api, front, gasket),
        "front_vs_screen": common_volume(api, front, screen),
        "gasket_vs_screen": common_volume(api, gasket, screen),
        "screen_vs_retainer": common_volume(api, screen, retainer),
        "front_vs_retainer": common_volume(api, front, retainer),
        "fh34_vs_usb1": common_volume(api, fh34, usb),
        "screen_vs_fh34_at_provisional_z": common_volume(api, screen, fh34),
        "screen_vs_usb1_at_provisional_z": common_volume(api, screen, usb),
    }
    route_overlaps = {
        "fpc_bend_keepout_vs_fh34_mm3": common_volume(api, bend_keepout, fh34),
        "fpc_bend_keepout_vs_usb1_mm3": common_volume(api, bend_keepout, usb),
        "fpc_bend_keepout_vs_front_outside_candidate_slot_mm3": common_volume(api, bend_keepout, front),
    }
    if any(abs(v) > 1e-6 for v in hard.values()):
        raise RuntimeError(f"fit-check hard collision under declared proxy Z: {hard}")
    if not all(v > 1e-6 for v in route_overlaps.values()):
        raise RuntimeError(f"expected unresolved FPC route overlap missing: {route_overlaps}")

    outputs = {
        "l1-front-fitcheck": front,
        "l1-screen-gasket-proxy": gasket,
        "l1-screen-retainer-fitcheck": retainer,
        "l1-sharp-screen-current-placement": screen,
        "l1-fpc-fold-bend-keepout": bend_keepout,
        "l1-pcb-plane-proxy": pcb_plane,
        "l1-fh34-courtyard-height-envelope": fh34,
        "l1-usb1-courtyard-height-envelope": usb,
        "l1-pcb-front-courtyard-projection": courtyard_projection,
        "l1-front-screen-pcb-assembly": assembly,
        # Preserve the established filename for downstream fit-check viewers.
        "l1-front-screen-assembly": assembly,
    }
    for stem, shape in outputs.items():
        write_pair(api, shape, output, stem)

    params = {
        "schema_version": 2,
        "status": "current L1 screen-only fit-check; not production frozen",
        "production_ready": False,
        "reference_top": str(REFERENCE_TOP),
        "reference_preserved": True,
        "candidate_pcb_evidence": str(PCB_EVIDENCE),
        "candidate_pcb_sha256": pcb["source_pcb_sha256"],
        "coordinate_mapping": pcb["coordinate_transform"],
        "screen_shift_from_prior_center_enclosure_xy_mm": [1.5, 1.5],
        "screen_shift_from_prior_center_kicad_xy_mm": [1.5, -1.5],
        "original_window_xy": ORIGINAL_WINDOW,
        "sharp_panel_xy": PANEL,
        "sharp_view_xy": VIEW,
        "new_window_xy": WINDOW,
        "view_clearance_each_edge": 0.60,
        "gasket_proxy_thickness": GASKET_T,
        "screen_front_z": SCREEN_FRONT_Z,
        "screen_rear_z": SCREEN_REAR_Z,
        "retainer_thickness": RETAINER_T,
        "fpc_flat_xy": [FPC_X[0], FPC_FLAT_Y[0], FPC_X[1], FPC_FLAT_Y[1]],
        "fpc_relief_xy": FPC_RELIEF,
        "front_fpc_pass_through_slot_xy": FRONT_FPC_SLOT,
        "front_fpc_pass_through_slot_status": "candidate; real fold and exposed-lip appearance pending",
        "fpc_bend_keepout_xy": [FPC_X[0], FPC_BEND_Y[0], FPC_X[1], FPC_BEND_Y[1]],
        "j_disp1": pcb["critical_footprints"]["J_DISP1"],
        "usb1": pcb["critical_footprints"]["USB1"],
        "j_disp1_to_usb1_courtyard_gap_y_mm": pcb["j_disp1_to_usb1_courtyard_gap_y_mm"],
        "pcb_front_courtyard_screen_projection_overlap_count": pcb["front_courtyard_screen_projection_overlap_count"],
        "pcb_front_courtyard_screen_projection_overlaps": pcb["front_courtyard_screen_projection_overlaps"],
        "courtyard_coverage": pcb["courtyard_coverage"],
        "z_stack_proxy": {
            "status": "visualisation only; PCB-to-cover seating datum not verified",
            "pcb_component_face_z_mm": PCB_COMPONENT_FACE_Z_PROXY,
            "pcb_thickness_mm": PCB_T,
            "fh34_height_mm": FH34_BODY_HEIGHT,
            "usb1_height_mm": USB_HEIGHT_PROXY,
            "usb1_height_status": "conservative unverified proxy",
        },
        "hard_collision_common_volume_mm3": hard,
        "routing_keepout_overlap_mm3": route_overlaps,
        "pending": [
            "physical screen FPC exit/contact/Pin-1 confirmation",
            "1:1 paper or real-part confirmation of folded Pin-1 mapping into J_DISP1",
            "exact FPC fold route and latch-access test around USB1",
            "measured PCB component face to front-cover Z datum",
            "verified USB1 body height and full 74-footprint component heights",
            "retainer fastening/compression method",
            "printed dimensional compensation and powered closed-shell test",
        ],
    }
    (output / "l1-front-fitcheck-parameters.json").write_text(
        json.dumps(params, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({
        "status": "PASS_NON_FROZEN",
        "window": WINDOW,
        "j_to_usb_gap_y_mm": pcb["j_disp1_to_usb1_courtyard_gap_y_mm"],
        "screen_projection_overlaps": pcb["front_courtyard_screen_projection_overlap_count"],
        "hard_collision_max_mm3": max(abs(v) for v in hard.values()),
        "route_overlap_mm3": route_overlaps,
    }))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
