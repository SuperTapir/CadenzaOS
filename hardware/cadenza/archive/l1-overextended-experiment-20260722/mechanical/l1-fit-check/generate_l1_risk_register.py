#!/usr/bin/env python3
"""Build a machine-readable, non-frozen L1 print/assembly risk register.

The register only consumes existing mechanical candidates and the inherited
bottom enclosure.  It does not select an FPC direction or modify CAD.
"""

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
HOLE_CENTRES = [(5.0, 5.0), (5.0, 55.0), (125.0, 5.0), (125.0, 55.0)]


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


def read_shape(api, path: Path):
    reader = api["STEPControl_Reader"]()
    if reader.ReadFile(str(path)) != api["IFSelect_RetDone"]:
        raise RuntimeError(f"cannot read STEP: {path}")
    reader.TransferRoots()
    return reader.OneShape()


def inherited_sections(api):
    shape = read_shape(api, BOTTOM)
    cylinders = []
    planes = []
    ex = api["TopExp_Explorer"](shape, api["TopAbs_FACE"])
    while ex.More():
        face = api["TopoDS"].Face_s(ex.Current())
        surface = api["BRepAdaptor_Surface"](face)
        kind = str(surface.GetType())
        if kind.endswith("Cylinder"):
            cylinder = surface.Cylinder()
            axis = cylinder.Axis()
            direction = axis.Direction()
            if abs(abs(direction.Z()) - 1.0) < 1e-6:
                location = axis.Location()
                cylinders.append({
                    "radius": cylinder.Radius(),
                    "x": location.X(),
                    "y": location.Y(),
                    "span": abs(surface.LastVParameter() - surface.FirstVParameter()),
                })
        elif kind.endswith("Plane"):
            plane = surface.Plane()
            direction = plane.Axis().Direction()
            if abs(abs(direction.Z()) - 1.0) < 1e-6:
                props = api["GProp_GProps"]()
                api["BRepGProp"].SurfaceProperties_s(face, props)
                planes.append({"z": plane.Location().Z(), "area": props.Mass()})
        ex.Next()

    def matches(radius, x, y, min_span):
        return any(
            abs(c["radius"] - radius) < 0.01
            and abs(c["x"] - x) < 0.001
            and abs(c["y"] - y) < 0.001
            and c["span"] >= min_span
            for c in cylinders
        )

    boss_faces_ok = all(
        matches(1.5, x, y, 5.0) and matches(3.5, x, y, 10.9)
        for x, y in HOLE_CENTRES
    )
    large_plane_z = sorted({round(p["z"], 3) for p in planes if p["area"] > 300})
    floor_pair_ok = 0.0 in large_plane_z and 1.6 in large_plane_z
    return {
        "bottom_step_sha256": hashlib.sha256(BOTTOM.read_bytes()).hexdigest(),
        "four_m3_boss_face_pairs_found": boss_faces_ok,
        "mounting_bore_diameter_mm": 3.0,
        "boss_outer_diameter_mm": 7.0,
        "boss_radial_wall_mm": 2.0,
        "boss_cylindrical_span_mm": 11.0,
        "nominal_floor_section_mm": 1.6,
        "large_horizontal_plane_z_mm": large_plane_z,
        "floor_section_face_pair_found": floor_pair_ok,
        "interpretation_limit": "These are nominal STEP face sections, not a global minimum-thickness proof.",
    }


def risk(rid, severity, confidence, finding, evidence, release_gate):
    return {
        "id": rid,
        "severity": severity,
        "status": "PENDING_REAL_VALIDATION",
        "confidence": confidence,
        "finding": finding,
        "evidence": evidence,
        "release_gate": release_gate,
    }


def main() -> int:
    api = load_ocp()
    params = json.loads((HERE / "generated/l1-front-fitcheck-parameters.json").read_text())
    audit = json.loads((HERE / "L1_FITCHECK_AUDIT.json").read_text())
    fpc = json.loads((HERE / "variants/generated/verification-report.json").read_text())
    l2 = json.loads((HERE.parent / "l2-input-candidates/generated/l2-input-candidates.json").read_text())
    power = json.loads((HERE.parent / "power-lock-candidates/generated/power-lock-candidates.json").read_text())
    inherited = inherited_sections(api)

    panel = params["sharp_panel_xy"]
    window = params["new_window_xy"]
    retainer_outer = [32.8, 7.79, 97.2, 52.21]
    retainer_inner = [35.2, 12.0, 94.8, 48.0]
    screen_overlap = [
        round(window[0] - panel[0], 3), round(panel[2] - window[2], 3),
        round(window[1] - panel[1], 3), round(panel[3] - window[3], 3),
    ]
    retainer_overlap = [
        round(retainer_inner[0] - panel[0], 3), round(panel[2] - retainer_inner[2], 3),
        round(retainer_inner[1] - panel[1], 3), round(panel[3] - retainer_inner[3], 3),
    ]
    l2_variants = l2["variants"]
    l2_mins = {
        "screen_panel_clearance_mm": min(v["metrics"][k]["screen_panel_clearance_mm"] for v in l2_variants.values() for k in ("B", "Menu", "A-knob")),
        "pcb_edge_clearance_mm": min(v["metrics"][k]["pcb_edge_clearance_mm"] for v in l2_variants.values() for k in ("B", "Menu", "A-knob")),
        "mount_keepout_clearance_mm": min(v["metrics"][k]["nearest_mount_keepout_clearance_mm"] for v in l2_variants.values() for k in ("B", "Menu", "A-knob")),
        "b_menu_edge_clearance_mm": min(v["metrics"]["B_to_Menu_edge_clearance_mm"] for v in l2_variants.values()),
    }
    power_mins = {
        "screen_clearance_mm": min(v["metrics"]["screen_panel_xy_clearance_mm"] for v in power["variants"].values()),
        "fpc_relief_clearance_mm": min(v["metrics"]["fpc_relief_xy_clearance_mm"] for v in power["variants"].values()),
        "guard_overhang_mm": min(v["metrics"]["guard_protrudes_beyond_cap_mm"] for v in power["variants"].values()),
    }

    risks = [
        risk("MECH-FPC-01", "critical", "high", "Sharp p.23 resolves the screen-local 6-o'clock exit, rear flat-contact face, Pin 1 side and allowed rear fold; enclosure +Y/-Y rotation, adapter mapping, exact connector latch and folded route remain unknown.", "two datasheet-backed enclosure rotations present; selection_frozen=false; routing_frozen=false", "Photograph the real display/adapter, continuity-check all ten adapter pins, choose enclosure rotation, then validate connector latch and folded route in a closed enclosure."),
        risk("MECH-SCREEN-02", "high", "high", "The 0.40 mm gasket is a geometric proxy; compression, adhesive creep and glass stress are untested.", f"window-to-panel overlap L/R/B/T={screen_overlap} mm", "Choose actual gasket material from supplier data and pass repeated close/open plus powered display inspection without pressure artefacts or glass damage."),
        risk("MECH-RETAINER-03", "high", "high", "The 1.00 mm retainer has no fastener or snap geometry and is thinner than the inherited 1.60 mm nominal floor section.", "generated retainer thickness=1.00 mm; inherited floor section=1.60 mm", "Select material/process, add fastening, print and pass flex/retention testing."),
        risk("MECH-SCREW-04", "high", "high", "Inherited exact Ø3.0 mm bores may print undersize; screw type and pilot-hole rule are not selected.", "four Ø3.0 bores and Ø7.0 bosses found in bottom STEP", "Print a hole/boss coupon, select screw/inserts and record final compensated bore; inspect bosses after three assembly cycles."),
        risk("MECH-WALL-05", "medium", "medium", "Nominal inherited sections are 1.60 mm floor and 2.00 mm boss radial wall, but local global minimum and print anisotropy are not proven.", "direct STEP plane/cylinder face audit", "Run slicer thin-wall preview for the chosen nozzle/resin and physically inspect all walls/bosses; thicken only derived enclosure if needed."),
        risk("MECH-INPUT-06", "high", "high", "Button bodies, keycaps, EC12 body/shaft and press travel remain proxies.", f"candidate minima={l2_mins}", "Measure the purchased EC12 and switches with calipers; validate axial press at full travel and rotation in a 1:1 print."),
        risk("MECH-POWER-07", "high", "high", "Power/Lock placements clear current proxies but switch body, cap travel and accidental-press force are unverified.", f"candidate minima={power_mins}", "Select the real switch and pass reach, guard, full-travel and closed-shell tests."),
        risk("MECH-ZSTACK-08", "critical", "high", "PCB components, solder joints, battery, speaker, connector latch and wiring are not included in a closed-shell Z-stack.", "current assembly contains front, gasket, screen and retainer only", "Build a complete component-envelope assembly and pass closed-shell collision/clearance audit."),
        risk("MECH-PRINT-09", "medium", "medium", "Shrinkage, hole compensation, support scars and warping depend on the chosen printer/material/orientation.", "no printer/material/coupon result recorded", "Print a dimensional coupon and one sacrificial fit shell; record measured compensation before final STL export."),
    ]

    checks = {
        "base_fitcheck_audit_pass": audit["status"] == "PASS",
        "fpc_candidate_gate_pass": fpc["status"] == "PASS",
        "fpc_selection_not_frozen": fpc["selection_frozen"] is False,
        "inherited_boss_faces_found": inherited["four_m3_boss_face_pairs_found"],
        "inherited_floor_face_pair_found": inherited["floor_section_face_pair_found"],
        "all_risks_keep_real_validation_pending": all(r["status"] == "PENDING_REAL_VALIDATION" for r in risks),
    }
    result = {
        "schema_version": 1,
        "status": "PASS_NON_FROZEN_AUDIT" if all(checks.values()) else "FAIL",
        "production_ready": False,
        "selection_frozen": False,
        "checks": checks,
        "geometry_evidence": {
            "inherited_bottom_sections": inherited,
            "screen_window_overlap_left_right_bottom_top_mm": screen_overlap,
            "retainer_capture_overlap_left_right_bottom_top_mm": retainer_overlap,
            "retainer_thickness_mm": params["retainer_thickness"],
            "gasket_proxy_thickness_mm": params["gasket_proxy_thickness"],
            "l2_control_candidate_minima": l2_mins,
            "power_lock_candidate_minima": power_mins,
        },
        "provisional_print_guidance_not_design_facts": [
            "Do not scale the whole model to compensate shrinkage; calibrate XY holes and Z separately with the actual process.",
            "Treat the glass as a no-press-fit part; use a compliant gasket and process-specific clearance coupon.",
            "For FDM, confirm every retained wall has enough slicer perimeters; the 1.00 mm retainer is specifically unresolved.",
            "Do not derive a screw pilot diameter from the nominal M3 label; use the selected screw/insert supplier rule plus a printed coupon.",
        ],
        "risks": risks,
        "risk_counts": {s: sum(r["severity"] == s for r in risks) for s in ("critical", "high", "medium", "low")},
        "reproducibility": {
            "command": "PYTHONPATH=/Users/tapir/.cache/cadenza-cad-tools-py313 /opt/homebrew/bin/python3.13 hardware/cadenza/mechanical/l1-fit-check/generate_l1_risk_register.py",
            "inputs": [
                "generated/l1-front-fitcheck-parameters.json", "L1_FITCHECK_AUDIT.json",
                "variants/generated/verification-report.json", "../l2-input-candidates/generated/l2-input-candidates.json",
                "../power-lock-candidates/generated/power-lock-candidates.json", str(BOTTOM.relative_to(REPO)),
            ],
        },
        "limits": [
            "No real assembly evidence is created by this audit.",
            "Nominal STEP sections are not a proof of global minimum wall thickness.",
            "No FPC direction/contact side/Pin 1, connector rotation, input placement or power-button placement is selected.",
        ],
    }
    JSON_OUT.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    rows = "\n".join(
        f"| {r['id']} | {r['severity']} | {r['confidence']} | {r['finding']} | {r['release_gate']} |"
        for r in risks
    )
    md = f"""# L1 打印与装配风险清单

> 结果：**PASS_NON_FROZEN_AUDIT**。这只表示候选几何和风险记录可重复复查，不表示外壳可生产。FPC 方向、接点面和 Pin 1 仍未冻结。

## 可直接从 STEP/候选 JSON 证明的尺寸

- 底盒四个孔为名义 `Ø3.0 mm`，同心螺柱外径 `Ø7.0 mm`，径向壁厚 `2.0 mm`，圆柱面跨度约 `11.0 mm`。
- 底盒大面水平面显示名义底厚 `1.60 mm`。这些是局部名义截面，**不是全局最小壁厚证明**。
- Sharp 窗口对玻璃边缘的覆盖 L/R/B/T 为 `{screen_overlap} mm`；后压框捕捉量为 `{retainer_overlap} mm`。
- 后压框厚 `1.00 mm`，软垫仅是 `0.40 mm` 代理。两者都没有真实材料/压缩证据。
- L2 三套控件代理的最小屏幕/外缘/孔位 keepout 间隙为 `{l2_mins['screen_panel_clearance_mm']:.2f} / {l2_mins['pcb_edge_clearance_mm']:.2f} / {l2_mins['mount_keepout_clearance_mm']:.2f} mm`；B/Menu 最小边缘间隙 `{l2_mins['b_menu_edge_clearance_mm']:.2f} mm`。这只适用于代理体。

## 风险登记

| ID | 等级 | 信心 | 判断 | 解除条件 |
|---|---|---|---|---|
{rows}

## 容差策略（非冻结）

- 不对整件等比缩放来“补偿收缩”；用真实打印机/材料做孔、轴、薄壁和 Z 高度试片，分项记录补偿。
- 玻璃不做刚性压入配合；间隙和软垫压缩必须由材料数据与实物合壳确定。
- 名义 Ø3.0 mm 孔不直接当作可装 M3 的证据；先选螺丝/热熔螺母，再按厂商建议与试片定孔。
- 切片时必须检查薄壁是否生成足够周长线；`1.00 mm` 后压框当前明确为待重设计/实测项。

机器可读结果：`L1_PRINT_ASSEMBLY_RISKS.json`。
"""
    MD_OUT.write_text(md, encoding="utf-8")
    print(json.dumps({"status": result["status"], "checks": sum(checks.values()), "risk_counts": result["risk_counts"]}, ensure_ascii=False))
    return 0 if result["status"] != "FAIL" else 1


if __name__ == "__main__":
    raise SystemExit(main())
