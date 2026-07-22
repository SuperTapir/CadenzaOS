#!/usr/bin/env python3
"""Deterministically check candidate footprints against footprint-specs.json."""

from __future__ import annotations

import hashlib
import json
import re
import sys
from pathlib import Path


HERE = Path(__file__).resolve().parent
SPECS = HERE / "footprint-specs.json"
PRETTY = HERE / "Cadenza_Display_FPC_Variants.pretty"
OUTPUT = HERE / "verification.json"
PAD_RE = re.compile(
    r'\(pad "(?P<num>[^"]+)" smd (?P<shape>\w+) \(at (?P<x>-?[0-9.]+) (?P<y>-?[0-9.]+)\) '
    r'\(size (?P<w>[0-9.]+) (?P<h>[0-9.]+)\)'
)
MODEL_RE = re.compile(r'\(model "([^"]+)"')


def close(a: float, b: float, tol: float = 1e-6) -> bool:
    return abs(a - b) <= tol


def check_one(c: dict) -> dict:
    path = PRETTY / f'{c["footprint"]}.kicad_mod'
    errors: list[str] = []
    if not path.exists():
        return {"footprint": c["footprint"], "status": "FAIL", "errors": ["file_missing"]}
    text = path.read_text(encoding="utf-8")
    pads = [m.groupdict() for m in PAD_RE.finditer(text)]
    signal = {int(p["num"]): p for p in pads if p["num"].isdigit()}
    mounts = [p for p in pads if p["num"] == "MP"]
    if sorted(signal) != list(range(1, 11)):
        errors.append(f"signal_pad_numbers={sorted(signal)}")
    sig = c["signal_pad"]
    for pin in range(1, 11):
        if pin not in signal:
            continue
        p = signal[pin]
        expected_x = c["pin1_x"] + (pin - 1) * c["pad_pitch"]
        for key, got, expected in [
            ("x", float(p["x"]), expected_x),
            ("y", float(p["y"]), sig["y"]),
            ("width", float(p["w"]), sig["width"]),
            ("height", float(p["h"]), sig["height"]),
        ]:
            if not close(got, expected):
                errors.append(f"pin{pin}_{key}={got}_expected={expected}")
    if len(mounts) != len(c["mount_pads"]):
        errors.append(f"mount_pad_count={len(mounts)}")
    else:
        got_mounts = sorted((float(p["x"]), float(p["y"]), float(p["w"]), float(p["h"])) for p in mounts)
        exp_mounts = sorted((p["x"], p["y"], p["width"], p["height"]) for p in c["mount_pads"])
        if got_mounts != exp_mounts:
            errors.append(f"mount_pads={got_mounts}_expected={exp_mounts}")
    models = MODEL_RE.findall(text)
    if models != [c["model"]["path"]]:
        errors.append(f"model={models}_expected={[c['model']['path']]}")
    return {
        "footprint": c["footprint"],
        "mpn": c["mpn"],
        "contact_side": c["contact_side"],
        "pin1_x": c["pin1_x"],
        "pad_numbering": [signal.get(i, {}).get("x") for i in range(1, 11)],
        "model_kind": c["model"]["kind"],
        "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "status": "PASS" if not errors else "FAIL",
        "errors": errors,
    }


def main() -> int:
    data = json.loads(SPECS.read_text(encoding="utf-8"))
    checks = [check_one(c) for c in data["candidates"]]
    proxy_paths = sorted(MODEL_RE.findall(p.read_text(encoding="utf-8"))[0] for p in PRETTY.glob("*.kicad_mod"))
    result = {
        "schema_version": 1,
        "status": "PASS" if all(c["status"] == "PASS" for c in checks) else "FAIL",
        "candidate_not_frozen": data["library_status"] == "candidate_not_frozen" and not data["orientation_frozen"],
        "footprint_count": len(checks),
        "checks": checks,
        "model_paths": proxy_paths,
        "note": "Geometry check only; does not replace physical FPC direction and 1:1 print verification."
    }
    OUTPUT.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"status": result["status"], "footprints": len(checks), "candidate_not_frozen": result["candidate_not_frozen"]}))
    return 0 if result["status"] == "PASS" and result["candidate_not_frozen"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
