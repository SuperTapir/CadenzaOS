#!/usr/bin/env python3
"""Prove that the L1 schematic candidate adds no new KiCad ERC errors.

The imported reference contains known ERC debt.  This gate compares stable
violation type + item UUID signatures instead of requiring an artificial zero.
Warnings are reported but do not decide this narrow regression gate.
"""

from __future__ import annotations

import argparse
import json
from collections import Counter
from pathlib import Path


def violations(path: Path) -> list[dict]:
    data = json.loads(path.read_text(encoding="utf-8"))
    return [item for sheet in data["sheets"] for item in sheet["violations"]]


def signature(item: dict) -> tuple[str, tuple[str, ...]]:
    return item["type"], tuple(sorted(part["uuid"] for part in item.get("items", [])))


def summary(items: list[dict]) -> dict:
    return {
        "total": len(items),
        "by_severity": dict(Counter(item["severity"] for item in items)),
        "by_type": dict(Counter(item["type"] for item in items)),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", type=Path, required=True)
    parser.add_argument("--candidate", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    baseline = violations(args.baseline)
    candidate = violations(args.candidate)
    baseline_errors = {signature(item): item for item in baseline if item["severity"] == "error"}
    candidate_errors = {signature(item): item for item in candidate if item["severity"] == "error"}
    new_signatures = sorted(set(candidate_errors) - set(baseline_errors))
    removed_signatures = sorted(set(baseline_errors) - set(candidate_errors))
    result = {
        "schema_version": 1,
        "gate": "cadenza_l1_erc_error_regression",
        "status": "PASS_NO_NEW_ERRORS" if not new_signatures else "FAIL",
        "scope": "KiCad ERC error-level regression only; warnings and electrical validity still require review",
        "production_ready": False,
        "baseline": {"path": str(args.baseline.resolve()), **summary(baseline)},
        "candidate": {"path": str(args.candidate.resolve()), **summary(candidate)},
        "error_delta": {
            "baseline": len(baseline_errors),
            "candidate": len(candidate_errors),
            "new": len(new_signatures),
            "removed": len(removed_signatures),
            "new_signatures": [list(value) for value in new_signatures],
            "removed_signatures": [list(value) for value in removed_signatures],
        },
        "interpretation": (
            "No new error-level ERC signature was introduced by the L1 delta. "
            "Remaining errors are inherited reference/import debt, not waived production findings."
            if not new_signatures else
            "The L1 candidate introduced at least one new error-level ERC signature."
        ),
    }
    rendered = json.dumps(result, ensure_ascii=False, indent=2) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered, encoding="utf-8")
    print(rendered, end="")
    return 0 if result["status"] == "PASS_NO_NEW_ERRORS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
