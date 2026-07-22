#!/usr/bin/env python3
"""Generate non-frozen L1 FPC direction/contact-side candidates.

This script is deliberately isolated from the main L1 fit-check generator.  It
does not choose a final display orientation or invent a connector model.  Four
assemblies make the two independent physical questions visible:

* flat FPC leaves the glass toward enclosure +Y or -Y;
* exposed contact face points toward +Z (front/top) or -Z (rear/bottom).
"""

from __future__ import annotations

import json
import sys
from pathlib import Path


OCP_CACHE = Path("/Users/tapir/.cache/cadenza-cad-tools-py313")
HERE = Path(__file__).resolve().parent
FITCHECK = HERE.parent
SHARED = FITCHECK.parent / "shared/generated"

FRONT_STEP = FITCHECK / "generated/l1-front-fitcheck.step"
GASKET_STEP = FITCHECK / "generated/l1-screen-gasket-proxy.step"
GLASS_STEP = SHARED / "ls027b7dh01-glass.step"

PANEL = (33.6, 8.59, 96.4, 51.41)
FPC_X = (60.68, 69.32)
FPC_FLAT_LENGTH = 6.06
FPC_T = 0.30
FPC_CONTACT_LENGTH = 2.50
CONTACT_WITNESS_T = 0.02
BEND_START = 0.8
BEND_END = 6.0
MIN_BEND_R = 0.45

SCREEN_FRONT_Z = -3.4002643070850946
SCREEN_T = 1.64
SCREEN_BACK_Z = SCREEN_FRONT_Z - SCREEN_T
RETAINER_T = 1.0
RETAINER_OUTER = (32.8, 7.79, 97.2, 52.21)
RETAINER_INNER = (35.2, 12.0, 94.8, 48.0)
RELIEF_X = (60.18, 69.82)
RELIEF_DEPTH = 7.20
PCB_XY = (0.0, 0.0, 130.0, 60.0)
PCB_T_PROXY = 1.60
# Conservative envelope of the direction-specific top-contact candidate
# FH12A-10S-0.5SH(55).  It is a fit envelope, not a production STEP.
CONNECTOR_X = 9.10
CONNECTOR_Y = 6.20
CONNECTOR_Z = 2.00


def load_api():
    if str(OCP_CACHE) not in sys.path:
        sys.path.insert(0, str(OCP_CACHE))
    from OCP.Bnd import Bnd_Box
    from OCP.BRep import BRep_Builder
    from OCP.BRepAlgoAPI import BRepAlgoAPI_Common, BRepAlgoAPI_Cut
    from OCP.BRepBndLib import BRepBndLib
    from OCP.BRepBuilderAPI import BRepBuilderAPI_Transform
    from OCP.BRepCheck import BRepCheck_Analyzer
    from OCP.BRepGProp import BRepGProp
    from OCP.BRepMesh import BRepMesh_IncrementalMesh
    from OCP.BRepPrimAPI import BRepPrimAPI_MakeBox
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


def ring(api, outer, inner, z1, z2):
    outer_box = box(api, outer[0], outer[1], z1, outer[2], outer[3], z2)
    inner_box = box(api, inner[0], inner[1], z1 - 0.1, inner[2], inner[3], z2 + 0.1)
    return cut(api, outer_box, inner_box)


def compound(api, shapes):
    result = api["TopoDS_Compound"]()
    builder = api["BRep_Builder"]()
    builder.MakeCompound(result)
    for shape in shapes:
        builder.Add(result, shape)
    return result


def translate_z(api, shape, z):
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


def bbox(api, shape):
    bounds = api["Bnd_Box"]()
    api["BRepBndLib"].Add_s(shape, bounds)
    xmin, ymin, zmin, xmax, ymax, zmax = bounds.Get()
    return {
        "xmin": xmin,
        "ymin": ymin,
        "zmin": zmin,
        "xmax": xmax,
        "ymax": ymax,
        "zmax": zmax,
        "width": xmax - xmin,
        "height": ymax - ymin,
        "depth": zmax - zmin,
    }


def write_step(api, shape, path):
    writer = api["STEPControl_Writer"]()
    writer.Transfer(shape, api["STEPControl_AsIs"])
    if writer.Write(str(path)) != api["IFSelect_RetDone"]:
        raise RuntimeError(f"STEP write failed: {path}")


def write_stl(api, shape, path):
    api["BRepMesh_IncrementalMesh"](shape, 0.06, False, 0.3, True)
    if not api["StlAPI_Writer"]().Write(shape, str(path)):
        raise RuntimeError(f"STL write failed: {path}")


def direction_geometry(api, direction: str):
    x1, x2 = FPC_X
    if direction == "plus-y":
        y1, y2 = PANEL[3], PANEL[3] + FPC_FLAT_LENGTH
        relief_y = (PANEL[3] - 0.61, PANEL[3] + RELIEF_DEPTH)
        bend_y = (PANEL[3] + BEND_START, PANEL[3] + BEND_END)
        distal_contact_y = (y2 - FPC_CONTACT_LENGTH, y2)
    elif direction == "minus-y":
        y1, y2 = PANEL[1] - FPC_FLAT_LENGTH, PANEL[1]
        relief_y = (PANEL[1] - RELIEF_DEPTH, PANEL[1] + 0.61)
        bend_y = (PANEL[1] - BEND_END, PANEL[1] - BEND_START)
        distal_contact_y = (y1, y1 + FPC_CONTACT_LENGTH)
    else:
        raise ValueError(direction)

    fpc = box(api, x1, y1, SCREEN_BACK_Z, x2, y2, SCREEN_BACK_Z + FPC_T)
    bend_keepout = box(
        api,
        x1,
        bend_y[0],
        SCREEN_BACK_Z - 2 * MIN_BEND_R,
        x2,
        bend_y[1],
        SCREEN_BACK_Z + FPC_T + 2 * MIN_BEND_R,
    )
    retainer = ring(
        api,
        RETAINER_OUTER,
        RETAINER_INNER,
        SCREEN_BACK_Z - RETAINER_T,
        SCREEN_BACK_Z,
    )
    relief = box(
        api,
        RELIEF_X[0],
        relief_y[0],
        SCREEN_BACK_Z - RETAINER_T - 0.1,
        RELIEF_X[1],
        relief_y[1],
        SCREEN_BACK_Z + 0.1,
    )
    retainer = cut(api, retainer, relief)
    return fpc, bend_keepout, retainer, distal_contact_y, relief_y


def contact_witness(api, contact_side: str, contact_y):
    if contact_side == "top-contact":
        z1 = SCREEN_BACK_Z + FPC_T
        z2 = z1 + CONTACT_WITNESS_T
        physical_side = "+Z / toward display front and front cover"
    elif contact_side == "bottom-contact":
        z2 = SCREEN_BACK_Z
        z1 = z2 - CONTACT_WITNESS_T
        physical_side = "-Z / toward enclosure interior and PCB"
    else:
        raise ValueError(contact_side)
    witness = box(api, FPC_X[0], contact_y[0], z1, FPC_X[1], contact_y[1], z2)
    return witness, physical_side


def connector_pcb_proxy(api, direction: str):
    """Place a conservative folded/top-contact envelope in the edge corridor."""
    x1 = 65.0 - CONNECTOR_X / 2
    x2 = 65.0 + CONNECTOR_X / 2
    if direction == "plus-y":
        y1, y2 = PCB_XY[3] - CONNECTOR_Y, PCB_XY[3]
    else:
        y1, y2 = PCB_XY[1], PCB_XY[1] + CONNECTOR_Y
    connector_bottom_z = SCREEN_BACK_Z - CONNECTOR_Z
    connector = box(api, x1, y1, connector_bottom_z, x2, y2, SCREEN_BACK_Z)
    pcb = box(
        api,
        PCB_XY[0], PCB_XY[1], connector_bottom_z - PCB_T_PROXY,
        PCB_XY[2], PCB_XY[3], connector_bottom_z,
    )
    return connector, pcb, [x1, y1, connector_bottom_z, x2, y2, SCREEN_BACK_Z]


def main() -> int:
    api = load_api()
    output = HERE / "generated"
    output.mkdir(parents=True, exist_ok=True)

    front = read_step(api, FRONT_STEP)
    gasket = read_step(api, GASKET_STEP)
    glass = translate_z(api, read_step(api, GLASS_STEP), SCREEN_FRONT_Z)
    report = {
        "schema_version": 1,
        "status": "non-frozen fit-check candidates; datasheet-relative orientation resolved, enclosure rotation and physical adapter mapping not selected",
        "units": "mm",
        "coordinate_system": {
            "x": "inherited PCB left to right",
            "y": "enclosure lower edge to upper edge",
            "z": "+Z points toward display front/front cover; -Z points toward enclosure interior/PCB",
        },
        "shared_inputs": {
            "front_step": str(FRONT_STEP),
            "gasket_step": str(GASKET_STEP),
            "glass_step": str(GLASS_STEP),
        },
        "datasheet_geometry": {
            "panel_xy": PANEL,
            "panel_thickness": SCREEN_T,
            "flat_fpc_width": FPC_X[1] - FPC_X[0],
            "flat_fpc_length_outside_glass": FPC_FLAT_LENGTH,
            "flat_fpc_nominal_thickness": FPC_T,
            "bend_start_distance_from_glass": BEND_START,
            "bend_end_distance_from_glass": BEND_END,
            "minimum_inner_bend_radius": MIN_BEND_R,
            "source_pdf": "hardware/reference/oshwhub-project_jofcnupz/datasheets/LS027B7DH01_Sharp_LCY-1210401A.pdf",
            "source_pdf_sha256": "997ecbcb7f020fbe077632dc90f6bf82d9e0e49ccf2211484093dec348965d98",
            "evidence_pdf_page": 26,
            "evidence_spec_page": 23,
            "screen_local_facts": {
                "natural_exit": "6 o'clock/bottom edge with display side up",
                "flat_contact_face": "rear/back side (-Z when +Z is display front)",
                "pin1": "viewer-right with display side up; viewer-left with back side up",
                "allowed_fold": "toward panel rear/back side only; never toward front polarizer",
                "flat_connector": "bottom-side contact",
                "folded_connector": "top-side contact",
            },
        },
        "modeling_limits": [
            "The contact witness is only a 0.02 mm face marker, not copper thickness.",
            "The connector is a conservative FH12A body envelope and the PCB is a datum proxy; latch motion, pads and exact folded route are not modeled.",
            "Datasheet Pin 1 is resolved relative to the display, but its global X side changes when the display is rotated 180 degrees in the enclosure.",
            "The bend keepout is a conservative box, not a prediction of the natural bend shape.",
        ],
        "variants": {},
        "orientation_variants": {},
        "pending_verification": [
            "choose whether datasheet 6 o'clock maps to enclosure +Y or -Y",
            "physical screen/adapter photo confirmation of the datasheet-relative contact face and Pin 1",
            "adapter board pin numbering and 1:1 continuity for all ten pins",
            "final connector MPN, body height, latch travel and placement",
            "folded FPC route, bend radius, strain, repeated-bend count and enclosure closing clearance",
        ],
    }

    for direction in ("plus-y", "minus-y"):
        fpc, bend_keepout, retainer, contact_y, relief_y = direction_geometry(api, direction)
        direction_slug = f"fpc-{direction}"
        validate(api, direction_slug, fpc)
        validate(api, f"{direction_slug}-retainer", retainer)
        validate(api, f"{direction_slug}-bend-keepout", bend_keepout)
        write_step(api, retainer, output / f"{direction_slug}-retainer.step")
        write_stl(api, retainer, output / f"{direction_slug}-retainer.stl")
        write_step(api, bend_keepout, output / f"{direction_slug}-bend-keepout.step")
        write_stl(api, bend_keepout, output / f"{direction_slug}-bend-keepout.stl")

        connector, pcb_proxy, connector_bbox = connector_pcb_proxy(api, direction)
        evidence_slug = f"datasheet-{direction}-folded-top-contact"
        evidence_assembly = compound(api, [front, gasket, glass, fpc, retainer, connector, pcb_proxy])
        for label, shape in {
            f"{evidence_slug}-connector-envelope": connector,
            f"{evidence_slug}-pcb-plane-proxy": pcb_proxy,
            f"{evidence_slug}-assembly": evidence_assembly,
        }.items():
            validate(api, label, shape)
            write_step(api, shape, output / f"{label}.step")
            write_stl(api, shape, output / f"{label}.stl")

        physical_collisions = {
            "connector_vs_front": common_volume(api, connector, front),
            "connector_vs_gasket": common_volume(api, connector, gasket),
            "connector_vs_glass": common_volume(api, connector, glass),
            "connector_vs_retainer": common_volume(api, connector, retainer),
            "connector_vs_flat_fpc": common_volume(api, connector, fpc),
            "connector_vs_pcb_proxy": common_volume(api, connector, pcb_proxy),
            "pcb_proxy_vs_front": common_volume(api, pcb_proxy, front),
            "pcb_proxy_vs_glass": common_volume(api, pcb_proxy, glass),
            "pcb_proxy_vs_retainer": common_volume(api, pcb_proxy, retainer),
        }
        if any(abs(value) > 1e-6 for value in physical_collisions.values()):
            raise RuntimeError(f"{evidence_slug} has physical hard collisions: {physical_collisions}")
        routing_keepout_overlap = common_volume(api, connector, bend_keepout)
        pin1_global_x = "+X" if direction == "plus-y" else "-X"
        edge_corridor = PANEL[3] if direction == "plus-y" else PANEL[1]
        edge_corridor = PCB_XY[3] - edge_corridor if direction == "plus-y" else edge_corridor - PCB_XY[1]
        report["orientation_variants"][evidence_slug] = {
            "selection_status": "candidate; enclosure rotation and physical adapter mapping pending",
            "datasheet_6_oclock_maps_to": "+Y" if direction == "plus-y" else "-Y",
            "datasheet_pin1_global_x_for_this_rotation": pin1_global_x,
            "flat_fpc_contact_face": "-Z / panel rear / bottom-contact connector",
            "folded_to_back_contact_face": "+Z / top-contact connector",
            "connector_envelope_xyz_mm": [CONNECTOR_X, CONNECTOR_Y, CONNECTOR_Z],
            "connector_bbox": connector_bbox,
            "edge_corridor_depth_mm": edge_corridor,
            "flat_tail_length_mm": FPC_FLAT_LENGTH,
            "straight_after_tail_remaining_mm": edge_corridor - FPC_FLAT_LENGTH,
            "straight_after_tail_can_fit_connector": edge_corridor - FPC_FLAT_LENGTH >= CONNECTOR_Y,
            "folded_connector_xy_inside_pcb": connector_bbox[0] >= PCB_XY[0] and connector_bbox[1] >= PCB_XY[1] and connector_bbox[3] <= PCB_XY[2] and connector_bbox[4] <= PCB_XY[3],
            "relief_width_margin_each_side_mm": ((RELIEF_X[1] - RELIEF_X[0]) - CONNECTOR_X) / 2,
            "required_screen_back_to_pcb_surface_mm": CONNECTOR_Z,
            "physical_hard_collision_common_volume_mm3": physical_collisions,
            "routing_keepout_vs_connector_overlap_mm3": routing_keepout_overlap,
            "routing_status": "PENDING_EXACT_FOLD_ROUTE_AND_LATCH" if routing_keepout_overlap > 1e-6 else "CLEAR",
            "outputs": {
                "assembly_step": f"generated/{evidence_slug}-assembly.step",
                "assembly_stl": f"generated/{evidence_slug}-assembly.stl",
                "connector_step": f"generated/{evidence_slug}-connector-envelope.step",
                "connector_stl": f"generated/{evidence_slug}-connector-envelope.stl",
                "pcb_proxy_step": f"generated/{evidence_slug}-pcb-plane-proxy.step",
                "pcb_proxy_stl": f"generated/{evidence_slug}-pcb-plane-proxy.stl",
            },
        }

        for contact_side in ("top-contact", "bottom-contact"):
            witness, physical_side = contact_witness(api, contact_side, contact_y)
            slug = f"{direction_slug}-{contact_side}"
            assembly = compound(api, [front, gasket, glass, fpc, witness, retainer])
            validate(api, slug, witness)
            validate(api, f"{slug}-assembly", assembly)

            collisions = {
                "front_vs_glass": common_volume(api, front, glass),
                "front_vs_flat_fpc": common_volume(api, front, fpc),
                "front_vs_retainer": common_volume(api, front, retainer),
                "gasket_vs_glass": common_volume(api, gasket, glass),
                "glass_vs_retainer": common_volume(api, glass, retainer),
                "flat_fpc_vs_retainer": common_volume(api, fpc, retainer),
            }
            if any(abs(value) > 1e-6 for value in collisions.values()):
                raise RuntimeError(f"{slug} has hard collisions: {collisions}")

            write_step(api, assembly, output / f"{slug}-assembly.step")
            write_stl(api, assembly, output / f"{slug}-assembly.stl")
            report["variants"][slug] = {
                "selection_status": "candidate; pending physical confirmation",
                "flat_fpc_direction": "+Y" if direction == "plus-y" else "-Y",
                "contact_face": physical_side,
                "flat_fpc_bbox": bbox(api, fpc),
                "contact_witness_bbox": bbox(api, witness),
                "retainer_fpc_relief_xy": [RELIEF_X[0], relief_y[0], RELIEF_X[1], relief_y[1]],
                "bend_keepout_bbox": bbox(api, bend_keepout),
                "assembly_bbox": bbox(api, assembly),
                "hard_collision_common_volume_mm3": collisions,
                "outputs": {
                    "assembly_step": f"generated/{slug}-assembly.step",
                    "assembly_stl": f"generated/{slug}-assembly.stl",
                    "direction_retainer_step": f"generated/{direction_slug}-retainer.step",
                    "direction_retainer_stl": f"generated/{direction_slug}-retainer.stl",
                    "direction_bend_keepout_step": f"generated/{direction_slug}-bend-keepout.step",
                    "direction_bend_keepout_stl": f"generated/{direction_slug}-bend-keepout.stl",
                },
            }

    report_path = output / "fpc-direction-variants.json"
    report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    report_lines = [
        "# L1 FPC 候选尺寸与碰撞报告",
        "",
        "> 状态：未冻结。数据手册已解决屏幕局部方向，但外壳中 0°/180° 旋转、转接板映射和连接器位置仍未选择。",
        "",
        "| 候选 | 出线 | 触点面 | FPC XY 包络 (mm) | 压框避让 XY (mm) | 六组硬碰撞最大值 (mm³) |",
        "|---|---:|---|---|---|---:|",
    ]
    for slug, candidate in report["variants"].items():
        fpc_box = candidate["flat_fpc_bbox"]
        relief = candidate["retainer_fpc_relief_xy"]
        max_collision = max(abs(v) for v in candidate["hard_collision_common_volume_mm3"].values())
        report_lines.append(
            f"| `{slug}` | {candidate['flat_fpc_direction']} | {candidate['contact_face']} | "
            f"X {fpc_box['xmin']:.2f}–{fpc_box['xmax']:.2f}, Y {fpc_box['ymin']:.2f}–{fpc_box['ymax']:.2f} | "
            f"X {relief[0]:.2f}–{relief[2]:.2f}, Y {relief[1]:.2f}–{relief[3]:.2f} | {max_collision:.6f} |"
        )
    report_lines.extend(
        [
            "",
            "共同名义尺寸：屏幕玻璃 62.80 × 42.82 × 1.64 mm；FPC 平直包络宽 8.64 mm、伸出玻璃 6.06 mm、厚 0.30 mm。弯折提示区从玻璃边 0.80 mm 延伸到 6.00 mm，最小内弯半径按 R0.45 mm 记录。",
            "",
            "碰撞检查覆盖前壳/玻璃、前壳/FPC、前壳/压框、软垫/玻璃、玻璃/压框和 FPC/压框。四个候选的这些实体硬碰撞均为 0。触点面标记与 FPC 本体有意贴合，不作为独立结构件参与碰撞判定。",
            "",
            "待验证：真实出线方向、裸露触点面、Pin 1、转接板 1:1 连续性、连接器型号与高度、卡扣空间、自然折叠路线和合壳余量。",
            "",
            "## 数据手册约束后的两套外壳旋转候选",
            "",
            "Sharp 原厂 LCY-1210401A 规格书 PDF 第 26 页（规格页 23）明确：显示面朝上时 FPC 从 6 点钟方向伸出；平直尾部触点在背面，正面视图 Pin 1 在右侧；只能向背面弯折。平直接法推荐 bottom-contact，按 Figure 8-2 向背面折叠后推荐 top-contact。",
            "",
            "| 候选 | 6点钟映射 | Pin 1 全局侧 | 边缘走廊 | 平直尾后剩余 | FH12A 包络 | 实体硬碰撞 | 路径状态 |",
            "|---|---:|---:|---:|---:|---:|---:|---|",
        ]
    )
    for slug, candidate in report["orientation_variants"].items():
        max_physical = max(abs(v) for v in candidate["physical_hard_collision_common_volume_mm3"].values())
        report_lines.append(
            f"| `{slug}` | {candidate['datasheet_6_oclock_maps_to']} | {candidate['datasheet_pin1_global_x_for_this_rotation']} | "
            f"{candidate['edge_corridor_depth_mm']:.2f} mm | {candidate['straight_after_tail_remaining_mm']:.2f} mm | "
            f"{CONNECTOR_X:.1f}×{CONNECTOR_Y:.1f}×{CONNECTOR_Z:.1f} mm | {max_physical:.6f} mm³ | {candidate['routing_status']} |"
        )
    report_lines.extend([
        "",
        "两套候选的连接器与前壳、软垫、玻璃、对应开槽压框、FPC 和 PCB 代理实体硬碰撞均为 0，且连接器 XY 包络位于 130×60 mm PCB 内。可是 8.59 mm 边缘走廊减去 6.06 mm 平直尾只剩 2.53 mm，不能把 6.2 mm 深连接器简单接在尾端之后；连接器包络还与保守弯折 keepout 相交。因此精确折线路径、锁扣开合和真实 PCB Z 高度仍必须待验证，不能因实体 0 碰撞冻结。",
        "",
    ])
    (output / "DIMENSION_COLLISION_REPORT.md").write_text(
        "\n".join(report_lines), encoding="utf-8"
    )
    print(json.dumps({"output": str(output), "variants": list(report["variants"])}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
