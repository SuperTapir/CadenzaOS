#!/usr/bin/env python3
"""Independent semantic audit for the non-frozen L1 fit-check outputs."""

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
VARIANTS = HERE / "variants/generated"
OUTPUT = HERE / "L1_FITCHECK_AUDIT.json"
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
    b = api["Bnd_Box"]()
    api["BRepBndLib"].Add_s(shape, b)
    values = b.Get()
    return [round(v, 6) for v in values]


def mounting_holes(api, shape):
    holes = []
    ex = api["TopExp_Explorer"](shape, api["TopAbs_FACE"])
    while ex.More():
        face = api["TopoDS"].Face_s(ex.Current())
        surface = api["BRepAdaptor_Surface"](face)
        if str(surface.GetType()).endswith("Cylinder"):
            cylinder = surface.Cylinder()
            radius = cylinder.Radius()
            axis = cylinder.Axis()
            direction = axis.Direction()
            if abs(radius - 1.5) < 0.01 and abs(abs(direction.Z()) - 1.0) < 1e-5:
                loc = axis.Location()
                holes.append([round(loc.X(), 4), round(loc.Y(), 4), round(radius * 2, 4)])
        ex.Next()
    return sorted(holes)


def near_holes(actual):
    if len(actual) != 4:
        return False
    actual_xy = [(a[0], a[1]) for a in actual]
    return all(any(abs(x - ex) < 0.001 and abs(y - ey) < 0.001 for x, y in actual_xy) for ex, ey in EXPECTED_HOLES)


def aggregate_files(paths):
    """Stable bundle hash: relative path plus file content, in sorted order."""
    digest = hashlib.sha256()
    for path in sorted(paths):
        digest.update(path.relative_to(REPO).as_posix().encode("utf-8"))
        digest.update(b"\0")
        digest.update(hashlib.sha256(path.read_bytes()).digest())
    return digest.hexdigest()


def main() -> int:
    api = load_ocp()
    params = json.loads((GENERATED / "l1-front-fitcheck-parameters.json").read_text(encoding="utf-8"))
    variants = json.loads((VARIANTS / "fpc-direction-variants.json").read_text(encoding="utf-8"))
    verification = json.loads((VARIANTS / "verification-report.json").read_text(encoding="utf-8"))

    ref_shape = read_shape(api, REFERENCE)
    front_shape = read_shape(api, GENERATED / "l1-front-fitcheck.step")
    ref_bbox = bbox(api, ref_shape)
    front_bbox = bbox(api, front_shape)
    holes = mounting_holes(api, front_shape)

    base_stems = [
        "l1-front-fitcheck",
        "l1-front-screen-assembly",
        "l1-screen-gasket-proxy",
        "l1-screen-retainer-fitcheck",
    ]
    variant_stems = [
        "fpc-plus-y-top-contact-assembly",
        "fpc-plus-y-bottom-contact-assembly",
        "fpc-minus-y-top-contact-assembly",
        "fpc-minus-y-bottom-contact-assembly",
        "fpc-plus-y-retainer",
        "fpc-minus-y-retainer",
        "fpc-plus-y-bend-keepout",
        "fpc-minus-y-bend-keepout",
    ]
    orientation_stems = [
        f"datasheet-{direction}-folded-top-contact-{part}"
        for direction in ("plus-y", "minus-y")
        for part in ("connector-envelope", "pcb-plane-proxy", "assembly")
    ]
    missing = []
    valid_step_count = 0
    for directory, stems in ((GENERATED, base_stems), (VARIANTS, variant_stems + orientation_stems)):
        for stem in stems:
            for suffix in ("step", "stl"):
                p = directory / f"{stem}.{suffix}"
                if not p.is_file() or p.stat().st_size == 0:
                    missing.append(str(p.relative_to(REPO)))
                elif suffix == "step":
                    read_shape(api, p)
                    valid_step_count += 1

    reference_hash = hashlib.sha256(REFERENCE.read_bytes()).hexdigest()
    outer_bbox_xy_preserved = all(abs(ref_bbox[i] - front_bbox[i]) < 0.001 for i in (0, 1, 3, 4))
    base_collision_max = max(abs(v) for v in params["hard_collision_common_volume_mm3"].values())
    variant_collision_max = max(
        abs(v)
        for candidate in variants["variants"].values()
        for v in candidate["hard_collision_common_volume_mm3"].values()
    )
    orientation_collision_max = max(
        abs(v)
        for candidate in variants["orientation_variants"].values()
        for v in candidate["physical_hard_collision_common_volume_mm3"].values()
    )
    semantic = {
        "reference_sha256": reference_hash,
        "reference_bbox": ref_bbox,
        "front_bbox": front_bbox,
        "mount_holes": holes,
        "window_xy": params["new_window_xy"],
        "sharp_panel_xy": params["sharp_panel_xy"],
        "sharp_view_xy": params["sharp_view_xy"],
        "gasket_proxy_thickness": params["gasket_proxy_thickness"],
        "retainer_thickness": params["retainer_thickness"],
        "variant_ids": sorted(variants["variants"]),
        "base_collision_max_mm3": base_collision_max,
        "variant_collision_max_mm3": variant_collision_max,
        "orientation_candidate_ids": sorted(variants["orientation_variants"]),
        "orientation_collision_max_mm3": orientation_collision_max,
        "routing_keepout_overlap_min_mm3": min(
            candidate["routing_keepout_vs_connector_overlap_mm3"]
            for candidate in variants["orientation_variants"].values()
        ),
        "valid_step_count": valid_step_count,
    }
    signature = hashlib.sha256(json.dumps(semantic, sort_keys=True).encode()).hexdigest()
    checks = {
        "reference_input_hash_unchanged": reference_hash == EXPECTED_REFERENCE_SHA256,
        "outer_bbox_xy_preserved": outer_bbox_xy_preserved,
        "four_mounting_holes_preserved": near_holes(holes),
        "sharp_window_is_60_by_36_48": abs((params["new_window_xy"][2] - params["new_window_xy"][0]) - 60.0) < 1e-9 and abs((params["new_window_xy"][3] - params["new_window_xy"][1]) - 36.48) < 1e-9,
        "gasket_retainer_relief_declared": params["gasket_proxy_thickness"] == 0.4 and params["retainer_thickness"] == 1.0 and len(params["fpc_relief_xy"]) == 4,
        "base_step_stl_complete": not any("mechanical/l1-fit-check/generated" in p for p in missing),
        "four_direction_contact_candidates_present": sorted(variants["variants"]) == sorted([
            "fpc-plus-y-top-contact", "fpc-plus-y-bottom-contact", "fpc-minus-y-top-contact", "fpc-minus-y-bottom-contact"
        ]),
        "two_datasheet_orientation_candidates_present": sorted(variants["orientation_variants"]) == sorted([
            "datasheet-plus-y-folded-top-contact", "datasheet-minus-y-folded-top-contact"
        ]),
        "variant_step_stl_complete": not any("mechanical/l1-fit-check/variants/generated" in p for p in missing),
        "all_hard_collision_volumes_zero": base_collision_max <= 1e-6 and variant_collision_max <= 1e-6 and orientation_collision_max <= 1e-6,
        "datasheet_relative_orientation_resolved": verification["datasheet_relative_orientation_resolved"] is True,
        "enclosure_rotation_and_route_not_frozen": verification["enclosure_rotation_frozen"] is False and verification["routing_frozen"] is False,
        "route_overlap_explicitly_pending": all(candidate["routing_status"] == "PENDING_EXACT_FOLD_ROUTE_AND_LATCH" and candidate["routing_keepout_vs_connector_overlap_mm3"] > 1e-6 for candidate in variants["orientation_variants"].values()),
        "selection_not_frozen": verification["selection_frozen"] is False and "non-frozen" in variants["status"],
    }
    result = {
        "schema_version": 1,
        "status": "PASS" if all(checks.values()) else "FAIL",
        "checks": checks,
        "semantic_signature_sha256": signature,
        "semantic_evidence": semantic,
        "stable_artifact_hashes": {
            "stl_bundle_sha256": aggregate_files(
                [GENERATED / f"{stem}.stl" for stem in base_stems]
                + [VARIANTS / f"{stem}.stl" for stem in variant_stems + orientation_stems]
            ),
            "note": "STEP byte hashes are intentionally excluded because the exporter writes timestamps; STEP geometry is covered by semantic checks and BREP validity.",
        },
        "missing_outputs": missing,
        "limits": [
            "STEP has no parametric feature history; hole preservation is checked from exact cylindrical faces.",
            "Outer preservation means the inherited XY envelope and four PCB mounting bores are unchanged; the screen window region is intentionally modified.",
            "Datasheet-relative 6-o'clock exit, rear flat-contact face, Pin 1 side and allowed fold direction are resolved; enclosure +Y/-Y mapping, adapter mapping, connector placement, latch travel and folded route are not frozen.",
        ],
    }
    OUTPUT.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"status": result["status"], "signature": signature, "valid_steps": valid_step_count, "missing": len(missing)}))
    return 0 if result["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
