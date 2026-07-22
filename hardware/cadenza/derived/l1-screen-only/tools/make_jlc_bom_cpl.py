#!/usr/bin/env python3
"""Convert KiCad BOM/POS CSV files to a conservative JLCPCB upload pair.

The imported reference represents its four mounting holes as SCREW footprints.
They have no LCSC number and are intentionally omitted from both output files.
Every remaining BOM designator must have exactly one placement row and a valid
LCSC C-number; otherwise this tool stops instead of producing a partial package.
"""

from __future__ import annotations

import argparse
import csv
import json
import math
import re
from pathlib import Path


LCSC_RE = re.compile(r"^C[0-9]+$")


def read_csv(path: Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8-sig") as handle:
        return list(csv.DictReader(handle))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--bom-raw", type=Path, required=True)
    parser.add_argument("--pos-raw", type=Path, required=True)
    parser.add_argument("--bom-output", type=Path, required=True)
    parser.add_argument("--cpl-output", type=Path, required=True)
    parser.add_argument("--audit-output", type=Path, required=True)
    args = parser.parse_args()

    bom_rows = read_csv(args.bom_raw)
    pos_rows = read_csv(args.pos_raw)
    bom_by_ref = {row["Designator"]: row for row in bom_rows}
    pos_by_ref = {row["Ref"]: row for row in pos_rows}

    duplicate_bom = len(bom_by_ref) != len(bom_rows)
    duplicate_pos = len(pos_by_ref) != len(pos_rows)
    omitted = sorted(ref for ref, row in bom_by_ref.items() if not row["LCSC Part #"].strip())
    assembled = sorted(ref for ref in bom_by_ref if ref not in omitted)
    missing_positions = sorted(set(assembled) - set(pos_by_ref))
    extra_positions = sorted(set(pos_by_ref) - set(assembled) - set(omitted))
    invalid_lcsc = sorted(
        ref for ref in assembled if not LCSC_RE.fullmatch(bom_by_ref[ref]["LCSC Part #"].strip())
    )
    minimum_centroid_spacing = math.inf
    minimum_centroid_pair: tuple[str, str] | None = None
    for index, left_ref in enumerate(assembled):
        left = pos_by_ref.get(left_ref)
        if left is None:
            continue
        for right_ref in assembled[index + 1:]:
            right = pos_by_ref.get(right_ref)
            if right is None:
                continue
            distance = math.hypot(
                float(left["PosX"]) - float(right["PosX"]),
                float(left["PosY"]) - float(right["PosY"]),
            )
            if distance < minimum_centroid_spacing:
                minimum_centroid_spacing = distance
                minimum_centroid_pair = (left_ref, right_ref)

    checks = {
        "unique_bom_designators": not duplicate_bom,
        "unique_cpl_designators": not duplicate_pos,
        "all_assembled_parts_have_positions": not missing_positions,
        "no_unexpected_position_only_parts": not extra_positions,
        "all_assembled_parts_have_valid_lcsc_numbers": not invalid_lcsc,
        "only_mounting_screws_are_omitted": omitted == ["SCREW1", "SCREW2", "SCREW3", "SCREW4"],
        "minimum_centroid_spacing_exceeds_jlc_0_2mm": minimum_centroid_spacing > 0.2,
    }

    audit = {
        "schema_version": 1,
        "status": "PASS" if all(checks.values()) else "FAIL",
        "scope": "format/designator gate only; JLC placement preview and stock checks remain mandatory",
        "checks": checks,
        "counts": {
            "schematic_rows": len(bom_rows),
            "pcb_position_rows": len(pos_rows),
            "assembled_rows": len(assembled),
            "omitted_mechanical_rows": len(omitted),
        },
        "omitted_mechanical_designators": omitted,
        "missing_positions": missing_positions,
        "extra_positions": extra_positions,
        "invalid_lcsc_designators": invalid_lcsc,
        "minimum_centroid_spacing_mm": minimum_centroid_spacing,
        "minimum_centroid_spacing_pair": minimum_centroid_pair,
    }
    args.audit_output.parent.mkdir(parents=True, exist_ok=True)
    args.audit_output.write_text(json.dumps(audit, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    if audit["status"] != "PASS":
        print(json.dumps(audit, ensure_ascii=False))
        return 1

    args.bom_output.parent.mkdir(parents=True, exist_ok=True)
    with args.bom_output.open("w", newline="", encoding="utf-8-sig") as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=["Comment", "Designator", "Footprint", "LCSC Part #"],
        )
        writer.writeheader()
        for ref in assembled:
            row = bom_by_ref[ref]
            pcb_package = pos_by_ref[ref]["Package"]
            writer.writerow(
                {
                    "Designator": ref,
                    "Comment": row["Comment"],
                    "Footprint": pcb_package,
                    "LCSC Part #": row["LCSC Part #"],
                }
            )

    with args.cpl_output.open("w", newline="", encoding="utf-8-sig") as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=["Designator", "Mid X", "Mid Y", "Rotation", "Layer"],
        )
        writer.writeheader()
        for ref in assembled:
            row = pos_by_ref[ref]
            writer.writerow(
                {
                    "Designator": ref,
                    "Mid X": f'{float(row["PosX"]):.6f}mm',
                    "Mid Y": f'{float(row["PosY"]):.6f}mm',
                    "Layer": "Top" if row["Side"].lower() == "top" else "Bottom",
                    "Rotation": f'{float(row["Rot"]) % 360:.6f}',
                }
            )

    print(json.dumps(audit, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
