#!/usr/bin/env python3
"""Generate the current L1-only print/assembly risk register."""

from __future__ import annotations

import hashlib
import importlib.util
import json
import sys
from pathlib import Path


HERE = Path(__file__).resolve().parent
REPO = HERE.parents[3]
BOTTOM = REPO / "hardware/reference/oshwhub-project_jofcnupz/底盒V7.step"
OCP_CACHE = Path("/Users/tapir/.cache/cadenza-cad-tools-py313")
JSON_OUT = HERE / "L1_PRINT_ASSEMBLY_RISKS.json"
MD_OUT = HERE / "PRINT_ASSEMBLY_RISK_REGISTER.md"
PHOTO_EVIDENCE = REPO / "hardware/cadenza/validation/physical-evidence/photos/l1-screen-20260722/evidence.json"
HOLE_CENTRES = [(5.0,5.0),(5.0,55.0),(125.0,5.0),(125.0,55.0)]


def load_ocp():
    if importlib.util.find_spec("OCP") is None:
        sys.path.insert(0, str(OCP_CACHE))
    from OCP.BRepAdaptor import BRepAdaptor_Surface
    from OCP.BRepGProp import BRepGProp
    from OCP.GProp import GProp_GProps
    from OCP.IFSelect import IFSelect_RetDone
    from OCP.STEPControl import STEPControl_Reader
    from OCP.TopAbs import TopAbs_FACE
    from OCP.TopExp import TopExp_Explorer
    from OCP.TopoDS import TopoDS
    return locals()


def read_shape(api, path):
    reader = api["STEPControl_Reader"]()
    if reader.ReadFile(str(path)) != api["IFSelect_RetDone"]:
        raise RuntimeError(path)
    reader.TransferRoots()
    return reader.OneShape()


def inherited_sections(api):
    shape = read_shape(api, BOTTOM)
    cylinders, planes = [], []
    ex = api["TopExp_Explorer"](shape, api["TopAbs_FACE"])
    while ex.More():
        face = api["TopoDS"].Face_s(ex.Current())
        surface = api["BRepAdaptor_Surface"](face)
        kind = str(surface.GetType())
        if kind.endswith("Cylinder"):
            cyl = surface.Cylinder(); axis = cyl.Axis(); direction = axis.Direction()
            if abs(abs(direction.Z())-1) < 1e-6:
                loc = axis.Location()
                cylinders.append((cyl.Radius(), loc.X(), loc.Y(), abs(surface.LastVParameter()-surface.FirstVParameter())))
        elif kind.endswith("Plane"):
            plane = surface.Plane(); direction = plane.Axis().Direction()
            if abs(abs(direction.Z())-1) < 1e-6:
                props = api["GProp_GProps"](); api["BRepGProp"].SurfaceProperties_s(face, props)
                planes.append((plane.Location().Z(), props.Mass()))
        ex.Next()
    def matches(radius, x, y, span):
        return any(abs(r-radius)<0.01 and abs(cx-x)<0.001 and abs(cy-y)<0.001 and s>=span
                   for r,cx,cy,s in cylinders)
    levels = sorted({round(z,3) for z,area in planes if area > 300})
    return {
        "bottom_step_sha256": hashlib.sha256(BOTTOM.read_bytes()).hexdigest(),
        "four_m3_boss_face_pairs_found": all(matches(1.5,x,y,5.0) and matches(3.5,x,y,10.9)
                                               for x,y in HOLE_CENTRES),
        "mounting_bore_diameter_mm": 3.0,
        "boss_outer_diameter_mm": 7.0,
        "boss_radial_wall_mm": 2.0,
        "nominal_floor_section_mm": 1.6,
        "large_horizontal_plane_z_mm": levels,
        "floor_section_face_pair_found": 0.0 in levels and 1.6 in levels,
        "interpretation_limit": "Nominal STEP face sections, not global minimum-thickness or PCB seating proof.",
    }


def risk(rid, severity, confidence, finding, evidence, gate):
    return {"id":rid,"severity":severity,"status":"PENDING_REAL_VALIDATION",
            "confidence":confidence,"finding":finding,"evidence":evidence,"release_gate":gate}


def main() -> int:
    api = load_ocp()
    params = json.loads((HERE/"generated/l1-front-fitcheck-parameters.json").read_text())
    audit = json.loads((HERE/"L1_FITCHECK_AUDIT.json").read_text())
    inherited = inherited_sections(api)
    photos = json.loads(PHOTO_EVIDENCE.read_text(encoding="utf-8"))
    panel, window = params["sharp_panel_xy"], params["new_window_xy"]
    overlap = [round(window[0]-panel[0],3),round(panel[2]-window[2],3),
               round(window[1]-panel[1],3),round(panel[3]-window[3],3)]
    route = params["routing_keepout_overlap_mm3"]
    coverage = params["courtyard_coverage"]
    risks = [
        risk("MECH-FPC-01","critical","high",
             "Current FPC bend keepout intersects both the selected FH34 envelope and USB1 envelope; exact folded route and latch access are not proven.",
             f"J-to-USB courtyard gap={params['j_disp1_to_usb1_courtyard_gap_y_mm']:.6f} mm; front pass-through={params['front_fpc_pass_through_slot_xy']}; overlaps={route}",
             "Use the photo-confirmed rear contact face in a 1:1 fold mock-up, then confirm Pin 1 and pass repeated insertion and closed-shell powered test without touching USB1."),
        risk("MECH-PIN1-02","critical","high",
             "The PCB connector rotation is fixed in the candidate, but the 180-degree fold can reverse the apparent left/right pin order; physical mapping is not closed.",
             "User front/back photos confirm 6-o'clock exit and contacts on the display rear; no unambiguous physical Pin 1 mark is visible. J_DISP1 KiCad centre=(154.625,128.500), rotation=0.",
             "Photograph front/back, continuity-check all ten pins and compare a printed 1:1 pin-number overlay before ordering."),
        risk("MECH-ZSTACK-03","critical","high",
             "The imported PCB does not provide a trustworthy board-to-front-cover Z datum or complete verified component heights.",
             f"{params['pcb_front_courtyard_screen_projection_overlap_count']} front courtyards project into the screen panel; coverage {coverage['with_f_courtyard']}/{coverage['footprint_count']}.",
             "Measure PCB seating Z and critical component heights, model all screen-overlap parts, then pass a closed-shell section/collision audit."),
        risk("MECH-USB-04","high","high",
             "USB1 XY is authoritative but its body height is still a conservative proxy, so FPC-to-USB clearance is not verified.",
             f"USB1={params['usb1']['courtyard_xy_enclosure_mm']}; height proxy={params['z_stack_proxy']['usb1_height_mm']} mm.",
             "Identify exact USB MPN or caliper-measure the mounted connector including solder protrusion and repeat the bend audit."),
        risk("MECH-SCREEN-05","high","high",
             "The 0.40 mm gasket is only geometry; compression, adhesive creep and glass stress are untested.",
             f"window-to-panel overlap L/R/B/T={overlap} mm",
             "Select actual gasket/adhesive, print a sacrificial front, and inspect powered pixels after repeated close/open cycles."),
        risk("MECH-RETAINER-06","high","high",
             "The loose 1.00 mm rear frame has no fastening or preload feature and is not a production retainer.",
             "current fit-check exports a separated frame at the cover interior datum",
             "Choose screws/snaps/adhesive and verify retention, glass load and service sequence in a real print."),
        risk("MECH-SCREW-07","high","high",
             "Inherited cover Ø3.0 mm bores and PCB Ø2.0 mm holes do not select a screw or printed pilot-hole compensation.",
             "four inherited boss face pairs found; current PCB mounting centres unchanged",
             "Print a hole/boss coupon, select screw or inserts, and inspect after three assembly cycles."),
        risk("MECH-WALL-08","medium","medium",
             "Nominal inherited floor/boss sections do not prove local minimum wall or print anisotropy.",
             "bottom STEP nominal floor 1.60 mm, boss radial wall 2.00 mm",
             "Run slicer thin-wall inspection and a material-specific dimensional coupon."),
        risk("MECH-PRINT-09","medium","medium",
             "Shrinkage, supports, warping and surface finish remain printer/material/orientation dependent.",
             "no printer/material/coupon result recorded",
             "Print one sacrificial fit shell and record XY hole and Z-stack deviations before release."),
    ]
    checks = {
        "fitcheck_audit_pass_non_frozen": audit["status"] == "PASS_NON_FROZEN_AUDIT",
        "production_false": params["production_ready"] is False,
        "inherited_boss_faces_found": inherited["four_m3_boss_face_pairs_found"],
        "inherited_floor_face_pair_found": inherited["floor_section_face_pair_found"],
        "route_conflicts_explicit": all(v>1e-6 for v in route.values()),
        "photo_exit_and_contact_face_confirmed": photos["confirmed"]["fpc_exits_at_display_six_oclock"]
            and photos["confirmed"]["gold_contacts_are_on_display_rear_side"],
        "all_risks_pending_real_validation": all(r["status"]=="PENDING_REAL_VALIDATION" for r in risks),
    }
    result = {
        "schema_version":2,
        "status":"PASS_NON_FROZEN_AUDIT" if all(checks.values()) else "FAIL",
        "production_ready":False,
        "selection_frozen":False,
        "checks":checks,
        "geometry_evidence":{
            "inherited_bottom_sections":inherited,
            "screen_window_overlap_left_right_bottom_top_mm":overlap,
            "j_disp1_to_usb1_courtyard_gap_y_mm":params["j_disp1_to_usb1_courtyard_gap_y_mm"],
            "fpc_route_keepout_overlap_mm3":route,
            "screen_projection_overlap_count":params["pcb_front_courtyard_screen_projection_overlap_count"],
            "courtyard_coverage":coverage,
            "physical_photo_evidence":photos,
        },
        "risks":risks,
        "risk_counts":{s:sum(r["severity"]==s for r in risks) for s in ("critical","high","medium","low")},
        "limits":[
            "No real assembly evidence is created by this audit.",
            "The visual PCB Z datum and USB height are explicit proxies.",
            "FPC exit and contact face are photo-confirmed; folded Pin 1 mapping and latch operation remain physical gates.",
        ],
    }
    JSON_OUT.write_text(json.dumps(result,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    rows="\n".join(f"| {r['id']} | {r['severity']} | {r['confidence']} | {r['finding']} | {r['release_gate']} |" for r in risks)
    MD_OUT.write_text(f"""# L1 打印与装配风险清单

> 结果：**{result['status']}**。当前 CAD 已对齐 screen-only PCB，但仍不是可直接打印/打板证明。

## 当前可证明的几何

- Sharp 窗口和玻璃相对旧居中位置在外壳坐标均移动 `+1.5 mm X / +1.5 mm Y`。
- J_DISP1/USB1 来自当前 PCB；courtyard Y 净距 `{params['j_disp1_to_usb1_courtyard_gap_y_mm']:.6f} mm`。
- FPC 弯折 keepout 与 FH34/USB1 的代理公共体积为 `{route}`，所以折线路径明确保持未完成。
- 前壳已增加与屏幕窗口连通的 FPC 过线槽候选 `{params['front_fpc_pass_through_slot_xy']}`；槽口遮蔽与真实排线折弯仍需实体样。
- 74 个 footprint 中 66 个有 F.CrtYd；31 个 XY 投影落入屏幕玻璃。缺少高度和 PCB 安装 Z，不能清除闭壳风险。
- 屏幕窗口对玻璃覆盖 L/R/B/T 为 `{overlap} mm`；软垫 `0.40 mm`、压框 `1.00 mm` 均为代理。

## 风险登记

| ID | 等级 | 信心 | 判断 | 解除条件 |
|---|---|---|---|---|
{rows}

## 边界

- 不因代理 Z 下硬碰撞为零就宣称可生产。
- FPC 出线和金手指面已由用户正反照片确认；仍必须确认折后 Pin 1、USB 高度、PCB 到前壳 Z，并做 1:1 折弯样和通电闭壳测试。
- 打印前还需选材料/打印机，完成孔、薄壁、玻璃受力试片。
""",encoding="utf-8")
    print(json.dumps({"status":result["status"],"checks":sum(checks.values()),"risk_counts":result["risk_counts"]}))
    return 0 if result["status"] != "FAIL" else 1


if __name__ == "__main__":
    raise SystemExit(main())
