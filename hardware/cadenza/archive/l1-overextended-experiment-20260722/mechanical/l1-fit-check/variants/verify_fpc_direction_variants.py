#!/usr/bin/env python3
"""Repeatable read-only gate for generated FPC direction candidates."""

from __future__ import annotations

import json
import importlib.util
import sys
from pathlib import Path


OCP_CACHE = Path("/Users/tapir/.cache/cadenza-cad-tools-py313")
HERE = Path(__file__).resolve().parent
GENERATED = HERE / "generated"
EXPECTED_VARIANTS = {
    "fpc-plus-y-top-contact",
    "fpc-plus-y-bottom-contact",
    "fpc-minus-y-top-contact",
    "fpc-minus-y-bottom-contact",
}
EXPECTED_ORIENTATIONS = {
    "datasheet-plus-y-folded-top-contact",
    "datasheet-minus-y-folded-top-contact",
}


def load_ocp():
    if importlib.util.find_spec("OCP") is None:
        sys.path.insert(0, str(OCP_CACHE))
    from OCP.BRepCheck import BRepCheck_Analyzer
    from OCP.IFSelect import IFSelect_RetDone
    from OCP.STEPControl import STEPControl_Reader

    return BRepCheck_Analyzer, IFSelect_RetDone, STEPControl_Reader


def valid_step(path, analyzer, ret_done, reader_type):
    reader = reader_type()
    if reader.ReadFile(str(path)) != ret_done:
        return False
    reader.TransferRoots()
    shape = reader.OneShape()
    return not shape.IsNull() and analyzer(shape).IsValid()


def main() -> int:
    report = json.loads((GENERATED / "fpc-direction-variants.json").read_text(encoding="utf-8"))
    actual = set(report["variants"])
    if actual != EXPECTED_VARIANTS:
        raise RuntimeError(f"variant set mismatch: {sorted(actual)}")
    orientations = set(report.get("orientation_variants", {}))
    if orientations != EXPECTED_ORIENTATIONS:
        raise RuntimeError(f"orientation variant set mismatch: {sorted(orientations)}")

    analyzer, ret_done, reader_type = load_ocp()
    checked_steps = []
    for slug, candidate in report["variants"].items():
        collisions = candidate["hard_collision_common_volume_mm3"]
        if len(collisions) != 6 or any(abs(value) > 1e-6 for value in collisions.values()):
            raise RuntimeError(f"collision gate failed: {slug}: {collisions}")
        for kind, relative in candidate["outputs"].items():
            path = HERE / relative
            if not path.is_file() or path.stat().st_size == 0:
                raise RuntimeError(f"missing output: {path}")
            if kind.endswith("_step") and path not in checked_steps:
                if not valid_step(path, analyzer, ret_done, reader_type):
                    raise RuntimeError(f"invalid STEP: {path}")
                checked_steps.append(path)

    for slug, candidate in report["orientation_variants"].items():
        if candidate["selection_status"].startswith("selected"):
            raise RuntimeError(f"orientation unexpectedly selected: {slug}")
        if candidate["routing_status"] != "PENDING_EXACT_FOLD_ROUTE_AND_LATCH":
            raise RuntimeError(f"routing must remain pending: {slug}")
        if candidate["straight_after_tail_can_fit_connector"] is not False:
            raise RuntimeError(f"straight-tail gate unexpectedly passed: {slug}")
        if candidate["folded_connector_xy_inside_pcb"] is not True:
            raise RuntimeError(f"connector envelope leaves PCB XY: {slug}")
        if any(abs(v) > 1e-6 for v in candidate["physical_hard_collision_common_volume_mm3"].values()):
            raise RuntimeError(f"physical collision gate failed: {slug}")
        if candidate["routing_keepout_vs_connector_overlap_mm3"] <= 1e-6:
            raise RuntimeError(f"expected unresolved route overlap missing: {slug}")
        for relative in candidate["outputs"].values():
            path = HERE / relative
            if not path.is_file() or path.stat().st_size == 0:
                raise RuntimeError(f"missing orientation output: {path}")
            if path.suffix == ".step" and path not in checked_steps:
                if not valid_step(path, analyzer, ret_done, reader_type):
                    raise RuntimeError(f"invalid STEP: {path}")
                checked_steps.append(path)

    result = {
        "status": "PASS",
        "variant_count": len(actual),
        "datasheet_orientation_variant_count": len(orientations),
        "unique_valid_step_count": len(checked_steps),
        "collision_checks_per_variant": 6,
        "maximum_hard_collision_volume_mm3": max(
            abs(value)
            for candidate in report["variants"].values()
            for value in candidate["hard_collision_common_volume_mm3"].values()
        ),
        "selection_frozen": False,
        "datasheet_relative_orientation_resolved": True,
        "enclosure_rotation_frozen": False,
        "routing_frozen": False,
        "pending_count": len(report["pending_verification"]),
    }
    (GENERATED / "verification-report.json").write_text(
        json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    print(json.dumps(result, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
