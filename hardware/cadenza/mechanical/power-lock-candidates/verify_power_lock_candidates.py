#!/usr/bin/env python3
"""Verify non-frozen Power/Lock candidate deliverables and collision gates."""

from __future__ import annotations

import json
from pathlib import Path

HERE = Path(__file__).resolve().parent


def main():
    data=json.loads((HERE/"generated/power-lock-candidates.json").read_text(encoding="utf-8"))
    assert data["status"] == "candidate only; selection_frozen=false; production_ready=false"
    assert tuple(data["inherited_datums"]["pcb_xy_mm"]) == (0.0,0.0,130.0,60.0)
    assert len(data["variants"]) == 2
    assert len(data["verification_limits"]) >= 6
    for name,v in data["variants"].items():
        folder=HERE/"generated"/name
        for part in ("enclosure-interface","button-cap-proxy","guide-proxy","guarded-recess-proxy","switch-keepout-proxy","actuation-sweep-proxy","assembly"):
            for ext in ("step","stl"):
                p=folder/f"{name}-{part}.{ext}"; assert p.is_file() and p.stat().st_size>100,p
        for suffix in ("assembly-preview.png","assembly-iso.png","parameters.json"):
            p=folder/f"{name}-{suffix}"; assert p.is_file() and p.stat().st_size>100,p
        assert all(abs(x)<=1e-5 for x in v["hard_collision_common_volume_mm3"].values())
        assert v["metrics"]["screen_panel_xy_clearance_mm"] >= 3.0
        assert v["metrics"]["fpc_relief_xy_clearance_mm"] >= 3.0
        assert v["metrics"]["guard_protrudes_beyond_cap_mm"] >= 0.5
    print(json.dumps({"status":"pass","variants":2,"selection_frozen":False,"production_ready":False}))
    return 0


if __name__ == "__main__": raise SystemExit(main())
