#!/usr/bin/env python3
"""Verify generated L2 candidate manifest and required deliverables."""

from __future__ import annotations

import json
from pathlib import Path


HERE = Path(__file__).resolve().parent


def main() -> int:
    manifest_path = HERE / "generated/l2-input-candidates.json"
    data = json.loads(manifest_path.read_text(encoding="utf-8"))
    assert data["status"] == "candidate only; selection_frozen=false"
    assert tuple(data["inherited_datums"]["pcb_xy_mm"]) == (0.0, 0.0, 130.0, 60.0)
    assert len(data["inherited_datums"]["mount_hole_centres_xy_mm"]) == 4
    assert len(data["variants"]) == 3
    assert len(data["physical_measurements_required_before_freeze"]) >= 8
    for name, variant in data["variants"].items():
        folder = HERE / "generated" / name
        for suffix in (
            "front-candidate.step", "front-candidate.stl",
            "button-keycap-proxies.step", "button-keycap-proxies.stl",
            "encoder-a-knob-proxy.step", "encoder-a-knob-proxy.stl",
            "control-body-keepout-proxies.step", "control-body-keepout-proxies.stl",
            "assembly.step", "assembly.stl", "plan.svg", "parameters.json",
        ):
            path = folder / f"{name}-{suffix}"
            assert path.is_file() and path.stat().st_size > 100, path
        assert all(abs(v) <= 1e-5 for v in variant["hard_collision_common_volume_mm3"].values())
        for control in ("B", "Menu", "A-knob"):
            assert variant["metrics"][control]["screen_panel_clearance_mm"] >= 3.0
            assert variant["metrics"][control]["nearest_mount_keepout_clearance_mm"] >= 2.0
    print(json.dumps({"status": "pass", "variants": len(data["variants"]), "selection_frozen": False}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
