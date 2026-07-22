#!/usr/bin/env python3
"""Run reproducible ngspice tolerance corners for the L2 input RC candidate."""

from __future__ import annotations

import json
import hashlib
import math
import re
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent
TEMPLATE = ROOT / "input_filter_tb.cir"
GENERATED = ROOT / "generated"
RESULTS = ROOT / "results.json"

CORNERS = {
    "fast": {"RPU": 9_900.0, "RSER": 950.0, "CDEB": 35.1e-9},
    "nominal": {"RPU": 10_000.0, "RSER": 1_000.0, "CDEB": 39.0e-9},
    "slow": {"RPU": 10_100.0, "RSER": 1_050.0, "CDEB": 42.9e-9},
}

MEASURE_RE = re.compile(r"^([a-z0-9_]+)\s*=\s*([-+0-9.eE]+)", re.MULTILINE)


def rendered_netlist(template: str, values: dict[str, float]) -> str:
    text = template
    text = re.sub(r"\.param RPU=.*", f".param RPU={values['RPU']:.9g}", text)
    text = re.sub(r"\.param RSER=.*", f".param RSER={values['RSER']:.9g}", text)
    text = re.sub(r"\.param CDEB=.*", f".param CDEB={values['CDEB']:.9g}", text)
    return text


def main() -> int:
    ngspice = shutil.which("ngspice")
    if not ngspice:
        raise SystemExit("ngspice not found; install it and rerun this script")
    GENERATED.mkdir(parents=True, exist_ok=True)
    template = TEMPLATE.read_text(encoding="utf-8")
    corner_results = {}
    overall_pass = True

    for name, values in CORNERS.items():
        netlist = GENERATED / f"input_filter_{name}.cir"
        log = GENERATED / f"input_filter_{name}.log"
        netlist.write_text(rendered_netlist(template, values), encoding="utf-8")
        proc = subprocess.run(
            [ngspice, "-b", "-o", str(log), str(netlist)],
            text=True,
            capture_output=True,
            check=False,
        )
        log_text = log.read_text(encoding="utf-8", errors="replace") if log.exists() else ""
        measures = {key: float(value) for key, value in MEASURE_RE.findall(log_text)}
        expected = {
            "press_tau_s": (values["RPU"] * values["RSER"] / (values["RPU"] + values["RSER"])) * values["CDEB"],
            "release_tau_s": values["RPU"] * values["CDEB"],
            "steady_pressed_v": 3.3 * values["RSER"] / (values["RPU"] + values["RSER"]),
        }
        checks = {
            "ngspice_exit_zero": proc.returncode == 0,
            "20us_glitch_filtered_above_vil": measures.get("vmin_20us", -1.0) > 0.825,
            "75us_pulse_retained_below_vil": measures.get("vmin_75us", 99.0) < 0.825,
            "100us_pulse_retained_below_vil": measures.get("vmin_100us", 99.0) < 0.825,
            "200us_pulse_retained_below_vil": measures.get("vmin_200us", 99.0) < 0.825,
            "stable_press_is_low": measures.get("bounce_v_after_stable", 99.0) < 0.4,
        }
        passed = all(checks.values())
        overall_pass &= passed
        corner_results[name] = {
            "status": "pass" if passed else "fail",
            "components": values,
            "expected": expected,
            "measurements": measures,
            "checks": checks,
            "netlist": str(netlist.relative_to(ROOT)),
            "log": str(log.relative_to(ROOT)),
        }

    version_text = subprocess.run([ngspice, "--version"], text=True, capture_output=True, check=False).stdout
    version_match = re.search(r"ngspice-[0-9.]+", version_text, re.IGNORECASE)
    output = {
        "schema_version": "1.0",
        "status": "pass" if overall_pass else "fail",
        "simulator": "ngspice",
        "simulator_path": ngspice,
        "simulator_version": version_match.group(0) if version_match else "unknown",
        "testbench": TEMPLATE.name,
        "testbench_sha256": hashlib.sha256(TEMPLATE.read_bytes()).hexdigest(),
        "model_scope": "ideal_passives_ideal_switch_10pF_assumed_GPIO_input",
        "thresholds_v": {"vil_max_assumption": 0.825, "vih_min_assumption": 2.475},
        "acceptance": {
            "20us_glitch": "must_remain_above_VIL",
            "75us_and_longer_pulses": "must_cross_below_VIL",
            "stable_press": "must_settle_below_0.4V",
        },
        "corners": corner_results,
        "limitations": [
            "does_not_model_real_EC12_or_button_contact_waveforms",
            "does_not_prove_pulses_per_detent_or_minimum_real_pulse_width",
            "does_not_replace_software_gray_code_decode_or_debounce",
            "component_values_are_candidates_not_frozen",
        ],
    }
    RESULTS.write_text(json.dumps(output, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(json.dumps({"status": output["status"], "results": str(RESULTS), "corners": {k: v["status"] for k, v in corner_results.items()}}, ensure_ascii=False))
    return 0 if overall_pass else 1


if __name__ == "__main__":
    raise SystemExit(main())
