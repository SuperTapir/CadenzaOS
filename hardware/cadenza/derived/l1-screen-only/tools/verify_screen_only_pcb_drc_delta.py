#!/usr/bin/env python3
"""Compare L1 PCB DRC against the imported reference board.

The reference import is not a zero-DRC KiCad design.  This gate therefore
requires zero unconnected items and rejects any new violation in the actual L1
edit window, while preserving the complete raw counts for audit.
"""

from __future__ import annotations

import argparse
import json
from collections import Counter
from pathlib import Path
from typing import Any


CRITICAL_TYPES = {
    "shorting_items",
    "tracks_crossing",
    "hole_clearance",
    "track_width",
    "via_diameter",
    "drill_out_of_range",
    "solder_mask_bridge",
}
EDIT_WINDOW = (145.0, 183.0, 103.0, 134.0)
NEW_REFS = ("J_DISP1", "C_DISP1")


def load(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def item_signature(item: dict[str, Any]) -> tuple[str, float, float]:
    pos = item.get("pos", {})
    return (
        str(item.get("description", "")),
        round(float(pos.get("x", 0.0)), 3),
        round(float(pos.get("y", 0.0)), 3),
    )


def violation_signature(violation: dict[str, Any]) -> tuple[str, tuple[tuple[str, float, float], ...]]:
    return (
        str(violation.get("type", "")),
        tuple(sorted(item_signature(item) for item in violation.get("items", []))),
    )


def in_edit_window(item: dict[str, Any]) -> bool:
    pos = item.get("pos")
    if not isinstance(pos, dict):
        return False
    x0, x1, y0, y1 = EDIT_WINDOW
    return x0 <= float(pos.get("x", -1e9)) <= x1 and y0 <= float(pos.get("y", -1e9)) <= y1


def summarize(path: Path, data: dict[str, Any]) -> dict[str, Any]:
    violations = data.get("violations", [])
    return {
        "path": str(path.resolve()),
        "violations": len(violations),
        "unconnected": len(data.get("unconnected_items", [])),
        "by_type": dict(sorted(Counter(v.get("type", "") for v in violations).items())),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    here = Path(__file__).resolve().parents[1]
    parser.add_argument("--baseline", type=Path, default=here / "generated/evidence/baseline-pcb.drc.json")
    parser.add_argument("--candidate", type=Path, default=here / "generated/evidence/candidate-pcb.drc.json")
    parser.add_argument("--output", type=Path, default=here / "generated/evidence/pcb-drc-delta.json")
    args = parser.parse_args()

    baseline = load(args.baseline)
    candidate = load(args.candidate)
    baseline_signatures = {violation_signature(v) for v in baseline.get("violations", [])}
    new_violations = [
        v for v in candidate.get("violations", [])
        if violation_signature(v) not in baseline_signatures
    ]
    new_in_window = [v for v in new_violations if any(in_edit_window(i) for i in v.get("items", []))]
    new_ref_hits = [
        v for v in candidate.get("violations", [])
        if any(ref in i.get("description", "") for ref in NEW_REFS for i in v.get("items", []))
    ]
    base_counts = Counter(v.get("type", "") for v in baseline.get("violations", []))
    candidate_counts = Counter(v.get("type", "") for v in candidate.get("violations", []))
    critical_regressions = {
        kind: {"baseline": base_counts[kind], "candidate": candidate_counts[kind]}
        for kind in sorted(CRITICAL_TYPES)
        if candidate_counts[kind] > base_counts[kind]
    }

    checks = {
        "candidate_zero_unconnected": len(candidate.get("unconnected_items", [])) == 0,
        "candidate_total_not_above_reference": len(candidate.get("violations", [])) <= len(baseline.get("violations", [])),
        "no_new_violation_in_l1_edit_window": not new_in_window,
        "no_violation_names_new_screen_components": not new_ref_hits,
        "no_critical_type_count_regression": not critical_regressions,
    }
    status = "PASS" if all(checks.values()) else "FAIL"
    report = {
        "schema_version": 1,
        "status": status,
        "scope": "PCB DRC regression against imported reference; not a production-readiness waiver",
        "edit_window_mm": {"x_min": EDIT_WINDOW[0], "x_max": EDIT_WINDOW[1], "y_min": EDIT_WINDOW[2], "y_max": EDIT_WINDOW[3]},
        "baseline": summarize(args.baseline, baseline),
        "candidate": summarize(args.candidate, candidate),
        "checks": checks,
        "critical_regressions": critical_regressions,
        "new_violation_count": len(new_violations),
        "new_outside_edit_window": [violation_signature(v) for v in new_violations if v not in new_in_window],
        "new_in_edit_window": [violation_signature(v) for v in new_in_window],
        "new_screen_component_hits": [violation_signature(v) for v in new_ref_hits],
        "interpretation": (
            "PASS means the Sharp display delta has zero ratsnest items and no new local DRC finding. "
            "Remaining candidate findings are inherited/import or full-zone-refill debt and remain visible in the raw report."
        ),
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"status": status, "checks": checks, "output": str(args.output)}, ensure_ascii=False))
    return 0 if status == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
