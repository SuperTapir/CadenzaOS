#!/usr/bin/env python3
"""Normalize two EasyEDA-imported compound ground pads in a derived PCB copy.

The source board is never modified in place.  By default this is a dry-run audit;
pass --output to write an explicit derived copy using KiCad's pcbnew module.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any

import pcbnew


EXPECTED_GROUND_PADS = {("U4", "41"), ("U7", "3")}


def mm(value: int) -> float:
    return round(value / 1_000_000.0, 6)


def audit_and_normalize(board: Any, apply_changes: bool) -> dict[str, Any]:
    footprints = {str(fp.GetReference()): fp for fp in board.GetFootprints()}
    gnd = board.FindNet("GND")
    if gnd is None:
        raise RuntimeError("GND net not found")

    rows: list[dict[str, Any]] = []
    for reference, pad_number in sorted(EXPECTED_GROUND_PADS):
        footprint = footprints.get(reference)
        if footprint is None:
            raise RuntimeError(f"Expected footprint missing: {reference}")

        pads = [pad for pad in footprint.Pads() if str(pad.GetNumber()) == pad_number]
        if not pads:
            raise RuntimeError(f"Expected pad missing: {reference}.{pad_number}")

        nets_before = sorted({str(pad.GetNetname()) for pad in pads})
        if "GND" not in nets_before:
            raise RuntimeError(
                f"Refusing normalization: {reference}.{pad_number} has no GND member; "
                f"nets={nets_before}"
            )
        unexpected = [name for name in nets_before if name not in {"", "GND"}]
        if unexpected:
            raise RuntimeError(
                f"Refusing normalization: {reference}.{pad_number} also carries {unexpected}"
            )

        changed = []
        for pad in pads:
            if not str(pad.GetNetname()):
                changed.append(
                    {
                        "x_mm": mm(pad.GetPosition().x),
                        "y_mm": mm(pad.GetPosition().y),
                    }
                )
                if apply_changes:
                    pad.SetNet(gnd)

        rows.append(
            {
                "reference": reference,
                "pad_number": pad_number,
                "compound_pad_members": len(pads),
                "nets_before": nets_before,
                "empty_members_to_set_gnd": changed,
            }
        )

    return {
        "schema_version": 1,
        "operation": "set only empty members of explicitly whitelisted compound pads to GND",
        "apply_changes": apply_changes,
        "targets": rows,
        "changed_member_count": sum(
            len(row["empty_members_to_set_gnd"]) for row in rows
        ),
        "scope_note": (
            "This repairs KiCad net semantics for imported compound pads; it does not "
            "change placement, pad geometry, tracks, zones, board outline, or holes."
        ),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("pcb", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--manifest", type=Path)
    args = parser.parse_args()

    source = args.pcb.resolve()
    if args.output is not None and args.output.resolve() == source:
        parser.error("--output must not overwrite the source PCB")

    board = pcbnew.LoadBoard(str(source))
    result = audit_and_normalize(board, apply_changes=args.output is not None)
    result["source"] = str(source)
    result["output"] = str(args.output.resolve()) if args.output else None

    if args.output is not None:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        pcbnew.SaveBoard(str(args.output), board)

    rendered = json.dumps(result, ensure_ascii=False, indent=2) + "\n"
    if args.manifest is not None:
        args.manifest.parent.mkdir(parents=True, exist_ok=True)
        args.manifest.write_text(rendered, encoding="utf-8")
    print(rendered, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
