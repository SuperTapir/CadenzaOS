#!/usr/bin/env python3
"""Semantic/reproducibility audit for the current non-frozen L1 fit-check."""

from __future__ import annotations

import hashlib
import importlib.util
import json
import sys
from pathlib import Path


HERE = Path(__file__).resolve().parent
REPO = HERE.parents[3]
REFERENCE = REPO / "hardware/reference/oshwhub-project_jofcnupz/顶盖V7.step"
GENERATED = HERE / "generated"
JSON_OUT = HERE / "L1_FITCHECK_AUDIT.json"
MD_OUT = HERE / "FITCHECK_REPRO_AUDIT.md"
OCP_CACHE = Path("/Users/tapir/.cache/cadenza-cad-tools-py313")
EXPECTED_REFERENCE_SHA256 = "41a991f288e6c38a120f876404848cc09c7c30d14df6e56ff053dea67efe9bee"
EXPECTED_HOLES = [(5.0, 5.0), (5.0, 55.0), (125.0, 5.0), (125.0, 55.0)]


def load_ocp():
    if importlib.util.find_spec("OCP") is None:
        sys.path.insert(0, str(OCP_CACHE))
    from OCP.Bnd import Bnd_Box
    from OCP.BRepAdaptor import BRepAdaptor_Surface
    from OCP.BRepBndLib import BRepBndLib
    from OCP.BRepCheck import BRepCheck_Analyzer
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
    shape = reader.OneShape()
    if shape.IsNull() or not api["BRepCheck_Analyzer"](shape).IsValid():
        raise RuntimeError(f"invalid STEP: {path}")
    return shape


def bbox(api, shape):
    result = api["Bnd_Box"]()
    api["BRepBndLib"].Add_s(shape, result)
    return [round(v, 6) for v in result.Get()]


def mounting_holes(api, shape):
    holes = []
    ex = api["TopExp_Explorer"](shape, api["TopAbs_FACE"])
    while ex.More():
        face = api["TopoDS"].Face_s(ex.Current())
        surface = api["BRepAdaptor_Surface"](face)
        if str(surface.GetType()).endswith("Cylinder"):
            cylinder = surface.Cylinder()
            direction = cylinder.Axis().Direction()
            if abs(cylinder.Radius()-1.5) < 0.01 and abs(abs(direction.Z())-1.0) < 1e-5:
                loc = cylinder.Axis().Location()
                holes.append([round(loc.X(), 4), round(loc.Y(), 4), round(cylinder.Radius()*2, 4)])
        ex.Next()
    return sorted(holes)


def holes_match(actual):
    if len(actual) != 4:
        return False
    return all(any(abs(a[0]-x) < 0.001 and abs(a[1]-y) < 0.001 for a in actual)
               for x, y in EXPECTED_HOLES)


def bundle_hash(paths):
    digest = hashlib.sha256()
    for path in sorted(paths):
        digest.update(path.relative_to(REPO).as_posix().encode())
        digest.update(b"\0")
        digest.update(hashlib.sha256(path.read_bytes()).digest())
    return digest.hexdigest()


def main() -> int:
    api = load_ocp()
    params = json.loads((GENERATED / "l1-front-fitcheck-parameters.json").read_text())
    pcb = json.loads((GENERATED / "candidate-pcb-mechanical.json").read_text())
    reference_shape = read_shape(api, REFERENCE)
    front_shape = read_shape(api, GENERATED / "l1-front-fitcheck.step")
    ref_bbox, front_bbox = bbox(api, reference_shape), bbox(api, front_shape)
    holes = mounting_holes(api, front_shape)

    stems = [
        "l1-front-fitcheck", "l1-screen-gasket-proxy", "l1-screen-retainer-fitcheck",
        "l1-sharp-screen-current-placement", "l1-fpc-fold-bend-keepout",
        "l1-pcb-plane-proxy", "l1-fh34-courtyard-height-envelope",
        "l1-usb1-courtyard-height-envelope", "l1-pcb-front-courtyard-projection",
        "l1-front-screen-pcb-assembly", "l1-front-screen-assembly",
    ]
    missing = []
    valid_steps = 0
    for stem in stems:
        for suffix in ("step", "stl"):
            path = GENERATED / f"{stem}.{suffix}"
            if not path.is_file() or path.stat().st_size == 0:
                missing.append(str(path.relative_to(REPO)))
            elif suffix == "step":
                read_shape(api, path)
                valid_steps += 1

    reference_hash = hashlib.sha256(REFERENCE.read_bytes()).hexdigest()
    hard_max = max(abs(v) for v in params["hard_collision_common_volume_mm3"].values())
    route_min = min(params["routing_keepout_overlap_mm3"].values())
    checks = {
        "reference_input_hash_unchanged": reference_hash == EXPECTED_REFERENCE_SHA256,
        "outer_bbox_xy_preserved": all(abs(ref_bbox[i]-front_bbox[i]) < 0.001 for i in (0,1,3,4)),
        "four_cover_mounting_holes_preserved": holes_match(holes),
        "candidate_pcb_critical_placements_match": all(pcb["critical_checks"].values()),
        "screen_shift_transform_exact": params["screen_shift_from_prior_center_kicad_xy_mm"] == [1.5,-1.5]
            and params["screen_shift_from_prior_center_enclosure_xy_mm"] == [1.5,1.5],
        "sharp_window_is_60_by_36_48": abs(params["new_window_xy"][2]-params["new_window_xy"][0]-60) < 1e-9
            and abs(params["new_window_xy"][3]-params["new_window_xy"][1]-36.48) < 1e-9,
        "j_disp1_usb_gap_matches_current_pcb": abs(params["j_disp1_to_usb1_courtyard_gap_y_mm"]-0.965567) < 1e-6,
        "current_outputs_complete_and_valid": not missing and valid_steps == len(stems),
        "hard_collisions_zero_under_declared_proxy_z": hard_max <= 1e-6,
        "fpc_route_conflict_retained_as_pending": route_min > 1e-6,
        "component_projection_coverage_declared": params["courtyard_coverage"]["with_f_courtyard"] == 66
            and params["courtyard_coverage"]["footprint_count"] == 74,
        "screen_projection_overlap_not_hidden": params["pcb_front_courtyard_screen_projection_overlap_count"] == 31,
        "production_status_remains_false": params["production_ready"] is False
            and "not production frozen" in params["status"],
        "physical_gates_present": len(params["pending"]) >= 7,
    }
    semantic = {
        "reference_bbox": ref_bbox,
        "front_bbox": front_bbox,
        "cover_mounting_holes": holes,
        "window_xy": params["new_window_xy"],
        "front_fpc_pass_through_slot_xy": params["front_fpc_pass_through_slot_xy"],
        "panel_xy": params["sharp_panel_xy"],
        "j_disp1": params["j_disp1"],
        "usb1": params["usb1"],
        "j_to_usb_gap_y_mm": params["j_disp1_to_usb1_courtyard_gap_y_mm"],
        "screen_projection_overlap_count": params["pcb_front_courtyard_screen_projection_overlap_count"],
        "hard_collision_max_mm3": hard_max,
        "route_overlap_min_mm3": route_min,
        "valid_step_count": valid_steps,
    }
    signature = hashlib.sha256(json.dumps(semantic, sort_keys=True).encode()).hexdigest()
    result = {
        "schema_version": 2,
        "status": "PASS_NON_FROZEN_AUDIT" if all(checks.values()) else "FAIL",
        "production_ready": False,
        "checks": checks,
        "semantic_signature_sha256": signature,
        "semantic_evidence": semantic,
        "stable_artifact_hashes": {
            "stl_bundle_sha256": bundle_hash([GENERATED/f"{stem}.stl" for stem in stems]),
            "note": "STEP byte hashes excluded because exporter timestamps vary; STEP validity and geometry semantics are audited.",
        },
        "missing_outputs": missing,
        "limits": [
            "Zero hard collision is conditional on a declared provisional PCB Z datum, not a measured closed-shell stack.",
            "Thirty-one front-side courtyards overlap the screen panel in XY; component heights and board seating remain required.",
            "FPC bend keepout intersects FH34, USB1 and front-shell material outside the candidate slot; real fold direction, Pin 1, slot coverage and latch access remain physical gates.",
        ],
    }
    JSON_OUT.write_text(json.dumps(result, ensure_ascii=False, indent=2)+"\n", encoding="utf-8")
    MD_OUT.write_text(f"""# L1 fit-check 可重复性审计

> 结果：**{result['status']}**。该结果只证明当前非冻结 fit-check 可重复生成，不表示外壳可直接打印或 PCB 可打板。

- 当前 PCB SHA-256：`{params['candidate_pcb_sha256']}`。
- Sharp 面板/窗口相对旧居中位置在外壳坐标均移动 `+1.5 mm`；对应 KiCad 为 `X +1.5 mm / Y -1.5 mm`。
- J_DISP1 位于 KiCad `(154.625, 128.500)`；USB1 固定于 `(153.125, 136.121)`；两者 courtyard 的 Y 向净距为 `{params['j_disp1_to_usb1_courtyard_gap_y_mm']:.6f} mm`。
- 声明的代理 Z 下硬碰撞最大值为 `{hard_max:.6f} mm³`，但 FPC 折弯 keepout 与 FH34/USB1 都有交叠，最小交叠 `{route_min:.6f} mm³`。
- 74 个 footprint 中 66 个有 F.CrtYd；其中 31 个 courtyard 的 XY 投影落在 Sharp 玻璃范围内。没有真实高度与 PCB 到前壳 Z 实测，不能据此清除闭壳风险。
- 生成并校验 `{valid_steps}` 个 STEP；缺失输出 `{len(missing)}` 个。

实物门槛仍包括 FPC 出线/触点/Pin 1、180° 折弯路线、FH34 锁扣操作、USB1 真实高度、PCB 安装 Z 和通电闭壳测试。
""", encoding="utf-8")
    print(json.dumps({"status": result["status"], "checks": sum(checks.values()),
                      "total_checks": len(checks), "valid_steps": valid_steps,
                      "signature": signature}))
    return 0 if result["status"] != "FAIL" else 1


if __name__ == "__main__":
    raise SystemExit(main())
