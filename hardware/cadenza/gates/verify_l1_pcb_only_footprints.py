#!/usr/bin/env python3
"""Verify the reviewed PCB-only net-tie and bare-pad testpoint footprints."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import pcbnew


KICAD_FOOTPRINTS = Path("/Applications/KiCad.app/Contents/SharedSupport/footprints")
NETTIE_NAME = "NetTie-2_SMD_Pad2.0mm"
TESTPOINT_NAME = "TestPoint_Pad_D1.5mm"


def mm(value: int) -> float:
    return pcbnew.ToMM(value)


def close(actual: float, expected: float, tolerance: float = 1e-6) -> bool:
    return abs(actual - expected) <= tolerance


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    nettie = pcbnew.FootprintLoad(str(KICAD_FOOTPRINTS / "NetTie.pretty"), NETTIE_NAME)
    testpoint = pcbnew.FootprintLoad(str(KICAD_FOOTPRINTS / "TestPoint.pretty"), TESTPOINT_NAME)
    if nettie is None or testpoint is None:
        raise RuntimeError("KiCad 10 standard footprint could not be loaded")

    nettie_pads = sorted(nettie.Pads(), key=lambda pad: pad.GetNumber())
    tp_pads = list(testpoint.Pads())
    copper_shapes = [item for item in nettie.GraphicalItems() if item.GetLayer() == pcbnew.F_Cu]
    excluded_mask = pcbnew.FP_EXCLUDE_FROM_BOM | pcbnew.FP_EXCLUDE_FROM_POS_FILES
    checks = {
        "nettie_two_numbered_pads": [pad.GetNumber() for pad in nettie_pads] == ["1", "2"],
        "nettie_pad_size_2mm": all(
            close(mm(pad.GetSize().x), 2.0) and close(mm(pad.GetSize().y), 2.0)
            for pad in nettie_pads
        ),
        "nettie_centres_plus_minus_2mm": [round(mm(pad.GetPosition().x), 6) for pad in nettie_pads] == [-2.0, 2.0],
        "nettie_single_4x2mm_top_copper_bridge": (
            len(copper_shapes) == 1
            and close(mm(copper_shapes[0].GetBoundingBox().GetWidth()), 4.0)
            and close(mm(copper_shapes[0].GetBoundingBox().GetHeight()), 2.0)
        ),
        "nettie_excluded_from_bom_and_cpl": nettie.GetAttributes() & excluded_mask == excluded_mask,
        "testpoint_one_1p5mm_pad": (
            len(tp_pads) == 1
            and tp_pads[0].GetNumber() == "1"
            and close(mm(tp_pads[0].GetSize().x), 1.5)
            and close(mm(tp_pads[0].GetSize().y), 1.5)
        ),
        "testpoint_top_copper_and_mask_no_paste": (
            tp_pads[0].GetLayerSet().Contains(pcbnew.F_Cu)
            and tp_pads[0].GetLayerSet().Contains(pcbnew.F_Mask)
            and not tp_pads[0].GetLayerSet().Contains(pcbnew.F_Paste)
        ),
        "testpoint_excluded_from_bom_and_cpl": testpoint.GetAttributes() & excluded_mask == excluded_mask,
    }

    copper_resistivity_ohm_m = 1.724e-8
    length_m = 4e-3
    width_m = 2e-3
    thickness_m = 35e-6
    current_a = 2.1
    resistance = copper_resistivity_ohm_m * length_m / (width_m * thickness_m)
    result = {
        "schema_version": 1,
        "status": "PASS_CANDIDATE" if all(checks.values()) else "FAIL",
        "production_ready": False,
        "layout_validated": False,
        "checks": checks,
        "footprints": {
            "R_PWR6": f"NetTie:{NETTIE_NAME}",
            "test_points": f"TestPoint:{TESTPOINT_NAME}",
        },
        "nominal_1oz_copper_estimate": {
            "assumptions": {
                "resistivity_ohm_m_at_20c": copper_resistivity_ohm_m,
                "length_mm": 4.0,
                "width_mm": 2.0,
                "copper_thickness_um": 35.0,
                "current_a": current_a,
            },
            "resistance_mohm": resistance * 1000,
            "voltage_drop_mv": current_a * resistance * 1000,
            "power_mw": current_a * current_a * resistance * 1000,
            "limit": "Nominal geometry estimate only; final board neck-down, thermal rise and fab copper tolerance remain to be checked.",
        },
    }
    rendered = json.dumps(result, ensure_ascii=False, indent=2) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered, encoding="utf-8")
    print(rendered, end="")
    return 0 if result["status"] == "PASS_CANDIDATE" else 1


if __name__ == "__main__":
    raise SystemExit(main())
