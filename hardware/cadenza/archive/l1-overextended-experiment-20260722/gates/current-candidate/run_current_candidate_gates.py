#!/usr/bin/env python3
"""Run the current Cadenza hardware candidate gates without mutating inputs.

Validators that normally write result files are executed in a temporary mirror.
Only ``latest.json`` and ``latest.md`` beside this script are persisted.

On this Mac, invoke ``run_current_candidate_gates.sh`` so the known-bad
python.org runtime is avoided before dyld starts the Python process.  The
bootstrap below remains a fallback for Python runtimes that can launch.
"""

from __future__ import annotations

# Bootstrap before importing the wider standard library.  The locally installed
# python.org 3.13 runtime on this Mac can start successfully but macOS rejects
# some of its extension modules (notably ``_struct``) at import time.  Prefer a
# project/runtime-isolated interpreter when the entry point is invoked through
# ``python3`` or its ``/usr/bin/env`` shebang.
import os as _bootstrap_os
import sys as _bootstrap_sys


def _bootstrap_isolated_python() -> None:
    if _bootstrap_os.environ.get("CADENZA_GATE_BOOTSTRAPPED") == "1":
        return

    script_dir = _bootstrap_os.path.dirname(_bootstrap_os.path.abspath(__file__))
    repository = _bootstrap_os.path.abspath(
        _bootstrap_os.path.join(script_dir, "..", "..", "..", "..")
    )
    candidates = [
        _bootstrap_os.environ.get("CADENZA_GATE_PYTHON"),
        _bootstrap_os.path.join(repository, ".venv", "bin", "python3"),
        _bootstrap_os.path.expanduser(
            "~/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/bin/python3"
        ),
    ]
    current = _bootstrap_os.path.realpath(_bootstrap_sys.executable)
    for candidate in candidates:
        if not candidate or not _bootstrap_os.path.isfile(candidate):
            continue
        if _bootstrap_os.path.realpath(candidate) == current:
            return
        environment = _bootstrap_os.environ.copy()
        environment["CADENZA_GATE_BOOTSTRAPPED"] = "1"
        try:
            _bootstrap_os.execve(
                candidate,
                [candidate, _bootstrap_os.path.abspath(__file__), *_bootstrap_sys.argv[1:]],
                environment,
            )
        except OSError:
            continue


_bootstrap_isolated_python()

import hashlib
import json
import os
import shlex
import shutil
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


HERE = Path(__file__).resolve().parent
REPO = HERE.parents[3]
HARDWARE = REPO / "hardware/cadenza"
CHANGE = REPO / "openspec/changes/adapt-oshwhub-handheld-hardware"
OUTPUT_JSON = HERE / "latest.json"
OUTPUT_MD = HERE / "latest.md"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def tree_manifest(paths: list[Path]) -> dict[str, str]:
    result: dict[str, str] = {}
    for root in paths:
        if root.is_file():
            result[str(root.relative_to(REPO))] = sha256(root)
            continue
        for path in sorted(p for p in root.rglob("*") if p.is_file()):
            result[str(path.relative_to(REPO))] = sha256(path)
    return result


def probe(python: str, module: str, pythonpath: str | None = None) -> tuple[bool, str]:
    env = os.environ.copy()
    env["PYTHONDONTWRITEBYTECODE"] = "1"
    if pythonpath:
        env["PYTHONPATH"] = pythonpath
    proc = subprocess.run(
        [python, "-c", f"import {module}; print(getattr({module}, '__file__', 'builtin'))"],
        text=True,
        capture_output=True,
        check=False,
        env=env,
        timeout=20,
    )
    detail = (proc.stdout or proc.stderr).strip()
    return proc.returncode == 0, detail


def unique_existing(values: list[str | None]) -> list[str]:
    seen: set[str] = set()
    result: list[str] = []
    for value in values:
        if not value:
            continue
        resolved = shutil.which(value) or (value if Path(value).is_file() else None)
        if resolved and resolved not in seen:
            result.append(resolved)
            seen.add(resolved)
    return result


def choose_python_runtime() -> dict[str, Any]:
    candidates = unique_existing([
        sys.executable,
        "/Library/Frameworks/Python.framework/Versions/3.13/bin/python3",
        "/opt/homebrew/bin/python3",
        "python3",
    ])
    attempts = []
    for python in candidates:
        proc = subprocess.run(
            [python, "-c", "import sys; print(f'{sys.version_info.major}.{sys.version_info.minor}')"],
            text=True,
            capture_output=True,
            check=False,
            timeout=20,
        )
        attempts.append({"python": python, "ok": proc.returncode == 0, "version": proc.stdout.strip()})
        if proc.returncode == 0:
            return {"python": python, "version": proc.stdout.strip(), "attempts": attempts}
    raise RuntimeError("No usable Python interpreter found")


def choose_cad_runtime(
    python_candidates: list[str], cache_candidates: list[str] | None = None
) -> dict[str, Any]:
    cache_candidates = cache_candidates or [
        str(Path.home() / ".cache/cadenza-cad-tools-py313"),
        str(Path.home() / ".cache/cadenza-cad-tools"),
    ]
    attempts = []
    for python in python_candidates:
        for pythonpath in cache_candidates:
            if not Path(pythonpath).is_dir():
                continue
            ok, detail = probe(python, "OCP", pythonpath)
            attempts.append({"python": python, "pythonpath": pythonpath, "ok": ok, "detail": detail})
            if ok:
                return {"python": python, "pythonpath": pythonpath, "module": detail, "attempts": attempts}
    raise RuntimeError("No compatible Python/OCP runtime pair found")


def choose_kicad_runtime(python_candidates: list[str]) -> dict[str, Any]:
    bundled_python = "/Applications/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3"
    site_paths = [
        "/Applications/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/lib/python3.9/site-packages",
        "/Applications/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/lib/python3.9/site-packages",
    ]
    candidates = unique_existing([bundled_python, *python_candidates])
    attempts = []
    for python in candidates:
        for pythonpath in [None, *site_paths]:
            if pythonpath and not Path(pythonpath).is_dir():
                continue
            ok, detail = probe(python, "pcbnew", pythonpath)
            attempts.append({"python": python, "pythonpath": pythonpath, "ok": ok, "detail": detail})
            if ok:
                version_proc = subprocess.run(
                    [python, "-c", "import pcbnew; print(pcbnew.GetBuildVersion())"],
                    text=True,
                    capture_output=True,
                    check=False,
                    env={**os.environ, **({"PYTHONPATH": pythonpath} if pythonpath else {})},
                    timeout=20,
                )
                return {
                    "python": python,
                    "pythonpath": pythonpath,
                    "module": detail,
                    "kicad_version": version_proc.stdout.strip(),
                    "attempts": attempts,
                }
    raise RuntimeError("No compatible Python/pcbnew runtime pair found")


def choose_pdf_runtime(python_candidates: list[str]) -> dict[str, Any]:
    bundled_python = (
        "/Users/tapir/.cache/codex-runtimes/codex-primary-runtime/"
        "dependencies/python/bin/python3"
    )
    candidates = unique_existing([bundled_python, *python_candidates])
    attempts = []
    for python in candidates:
        proc = subprocess.run(
            [
                python,
                "-c",
                "import pypdf, reportlab; "
                "print(pypdf.__version__); print(reportlab.Version); print(pypdf.__file__)",
            ],
            text=True,
            capture_output=True,
            check=False,
            env={**os.environ, "PYTHONDONTWRITEBYTECODE": "1"},
            timeout=20,
        )
        lines = proc.stdout.strip().splitlines()
        attempts.append({"python": python, "ok": proc.returncode == 0, "detail": (proc.stdout or proc.stderr).strip()})
        if proc.returncode == 0 and len(lines) >= 3:
            return {
                "python": python,
                "pypdf_version": lines[0],
                "reportlab_version": lines[1],
                "pypdf_module": lines[2],
                "attempts": attempts,
            }
    raise RuntimeError("No bundled Python runtime with pypdf/reportlab found")


def run_check(
    check_id: str,
    title: str,
    command: list[str],
    *,
    cwd: Path,
    env_extra: dict[str, str] | None = None,
) -> dict[str, Any]:
    env = os.environ.copy()
    env["PYTHONDONTWRITEBYTECODE"] = "1"
    if env_extra:
        env.update(env_extra)
    proc = subprocess.run(
        command, cwd=cwd, env=env, text=True, capture_output=True, check=False, timeout=180
    )
    return {
        "id": check_id,
        "title": title,
        "status": "PASS" if proc.returncode == 0 else "FAIL",
        "exit_code": proc.returncode,
        "command": shlex.join(command),
        "cwd": str(cwd),
        "stdout": proc.stdout.strip(),
        "stderr": proc.stderr.strip(),
    }


def run_host_c_check(
    compiler: str,
    firmware_dir: Path,
    output_binary: Path,
) -> dict[str, Any]:
    output_binary.parent.mkdir(parents=True, exist_ok=True)
    compile_command = [
        compiler,
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-I",
        str(firmware_dir / "main"),
        str(firmware_dir / "tests/test_input_logic.c"),
        str(firmware_dir / "main/input_logic.c"),
        "-o",
        str(output_binary),
    ]
    compile_proc = subprocess.run(
        compile_command,
        cwd=firmware_dir,
        text=True,
        capture_output=True,
        check=False,
        timeout=60,
    )
    test_proc = None
    if compile_proc.returncode == 0:
        test_proc = subprocess.run(
            [str(output_binary)],
            cwd=firmware_dir,
            text=True,
            capture_output=True,
            check=False,
            timeout=60,
        )
    passed = compile_proc.returncode == 0 and test_proc is not None and test_proc.returncode == 0
    stdout = compile_proc.stdout
    stderr = compile_proc.stderr
    if test_proc is not None:
        stdout += test_proc.stdout
        stderr += test_proc.stderr
    return {
        "id": "l2_input_host_c",
        "title": "L2 input logic host C tests with warnings as errors",
        "status": "PASS" if passed else "FAIL",
        "exit_code": 0 if passed else (compile_proc.returncode or test_proc.returncode),
        "compile_exit_code": compile_proc.returncode,
        "test_exit_code": test_proc.returncode if test_proc is not None else None,
        "command": f"{shlex.join(compile_command)} && {shlex.join([str(output_binary)])}",
        "cwd": str(firmware_dir),
        "stdout": stdout.strip(),
        "stderr": stderr.strip(),
    }


def copy_dir(source: Path, temporary_repo: Path) -> None:
    destination = temporary_repo / source.relative_to(REPO)
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copytree(source, destination)


def copy_file(source: Path, temporary_repo: Path) -> Path:
    destination = temporary_repo / source.relative_to(REPO)
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)
    return destination


def json_read(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def render_markdown(result: dict[str, Any]) -> str:
    rows = "\n".join(
        f"| {item['status']} | `{item['id']}` | {item['title']} |"
        for item in result["checks"]
    )
    pending = "\n".join(f"- {item}" for item in result["pending_before_freeze"])
    runtime = result["runtimes"]
    connectivity = result["evidence"]["reference_connectivity"]
    return f"""# Current Hardware Candidate Gate

Verdict: **{result['verdict']}**

Release class: **candidate only / not production ready**

This gate reruns the current candidate evidence. It does not certify a JLCPCB order,
freeze an FPC direction, or replace physical fit and bring-up tests.

| Result | Check | Scope |
|---|---|---|
{rows}

## Key evidence

- Reference connectivity: `{connectivity['exact_topology_matches']}/{connectivity['pcb_nets']}` PCB nets exactly match schematic endpoint topology.
- L1 FPC direction variants: `{result['evidence']['l1_fpc_variants']['variant_count']}`; selection frozen: `{str(result['evidence']['l1_fpc_variants']['selection_frozen']).lower()}`.
- L1 maximum hard collision volume: `{result['evidence']['l1_fit']['maximum_hard_collision_volume_mm3']}` mm³.
- L2 input SPICE corners: `{', '.join(result['evidence']['l2_input_spice']['corners'])}`.
- L2 mechanical selection frozen: `{str(result['evidence']['l2_mechanical']['selection_frozen']).lower()}`.
- L1 schematic delta contract: `{result['evidence']['l1_schematic_delta_contract']['passed']}/{result['evidence']['l1_schematic_delta_contract']['checks']}` checks; FPC frozen: `{str(result['evidence']['l1_schematic_delta_contract']['fpc_selection_frozen']).lower()}`.
- L1 pre-edit dry-run baseline: `{result['evidence']['l1_pre_edit_baseline']['passed']}/{result['evidence']['l1_pre_edit_baseline']['checks']}` checks and `{result['evidence']['l1_pre_edit_baseline']['source_manifest_entries']}` manifest files; L1 implemented: `{str(result['evidence']['l1_pre_edit_baseline']['l1_implemented']).lower()}`.
- L1 candidate-netlist validator self-test: `{result['evidence']['l1_candidate_netlist_selftest']['passed']}/{result['evidence']['l1_candidate_netlist_selftest']['fixtures']}` synthetic fixtures; real candidate validated: `{str(result['evidence']['l1_candidate_netlist_selftest']['real_candidate_validated']).lower()}`.
- L1 real hierarchical schematic candidate: `{result['evidence']['l1_real_candidate_netlist']['passed']}/{result['evidence']['l1_real_candidate_netlist']['checks']}` contract checks; real candidate validated: `{str(result['evidence']['l1_real_candidate_netlist']['real_candidate_validated']).lower()}`; PCB synced: `{str(result['evidence']['l1_real_candidate_netlist']['pcb_synced']).lower()}`.
- L1 ERC error regression: baseline `{result['evidence']['l1_erc_delta']['baseline_errors']}` → candidate `{result['evidence']['l1_erc_delta']['candidate_errors']}`; new errors: `{result['evidence']['l1_erc_delta']['new_errors']}`.
- L1 PCB sync preflight: `{result['evidence']['l1_pcb_sync_readiness']['new_with_footprints']}/26` new components have footprints; physical footprint blockers: `{result['evidence']['l1_pcb_sync_readiness']['physical_blockers']}`; PCB synced: `false`.
- L1 PCB-only footprints: `{result['evidence']['l1_pcb_only_footprints']['status']}`; nominal 2.1 A net-tie drop: `{result['evidence']['l1_pcb_only_footprints']['nominal_drop_mv']:.3f}` mV; layout validated: `false`.
- L1 placement feasibility: `{result['evidence']['l1_pcb_placement']['status']}`; FPC-to-shifted-USB gap: `{result['evidence']['l1_pcb_placement']['fpc_usb_gap_mm']}` mm; USB-to-LED1 gap: `{result['evidence']['l1_pcb_placement']['usb_led_gap_mm']}` mm; PCB created: `{str(result['evidence']['l1_pcb_placement']['pcb_created']).lower()}`.
- L1 PCB synchronization candidate: `{result['evidence']['l1_pcb_sync_candidate']['status']}` with `{result['evidence']['l1_pcb_sync_candidate']['passed']}/{result['evidence']['l1_pcb_sync_candidate']['checks']}` checks; footprints: `{result['evidence']['l1_pcb_sync_candidate']['footprints']}`; named nets: `{result['evidence']['l1_pcb_sync_candidate']['named_nets']}`; routed/DRC/production ready: `false/false/false`.
- L1 local routing candidate: `{result['evidence']['l1_local_routing_candidate']['status']}` with `{result['evidence']['l1_local_routing_candidate']['passed']}/{result['evidence']['l1_local_routing_candidate']['checks']}` checks; added track segments: `{result['evidence']['l1_local_routing_candidate']['tracks_added']}`; unconnected: `{result['evidence']['l1_local_routing_candidate']['source_unconnected']}` → `{result['evidence']['l1_local_routing_candidate']['output_unconnected']}`; routing complete/production ready: `false/false`.
- L1 print/assembly risk register: `{result['evidence']['l1_risk_register']['status']}` with `{result['evidence']['l1_risk_register']['risk_count']}` pending risks; selection frozen: `{str(result['evidence']['l1_risk_register']['selection_frozen']).lower()}`.
- L2 printable controls: `{result['evidence']['l2_printable_controls']['step_files_checked']}` STEP + `{result['evidence']['l2_printable_controls']['stl_files_checked']}` STL, `{result['evidence']['l2_printable_controls']['layouts']}×{result['evidence']['l2_printable_controls']['shaft_interfaces_per_layout']}` candidates; selection frozen: `{str(result['evidence']['l2_printable_controls']['selection_frozen']).lower()}`; production ready: `{str(result['evidence']['l2_printable_controls']['production_ready']).lower()}`.
- Physical fit sheets: `{result['evidence']['physical_fit_sheets']['pdf_pages_checked']}` PDF pages and `{result['evidence']['physical_fit_sheets']['source_footprints_checked']}` connector footprints; orientation frozen: `{str(result['evidence']['physical_fit_sheets']['orientation_frozen']).lower()}`.
- L2 input host C: `{result['evidence']['l2_input_firmware']['host_c_status']}` with `-Wall -Wextra -Werror`; ESP-IDF full build: `{result['evidence']['l2_input_firmware']['esp_idf_full_build']['status']}` (recorded only, daily gate: `{str(result['evidence']['l2_input_firmware']['esp_idf_full_build']['included_in_daily_gate']).lower()}`).
- S1 Power/Lock footprint: `{result['evidence']['s1_power_lock_footprint']['status']}`; axes verified: `{str(result['evidence']['s1_power_lock_footprint']['axis_semantics_verified']).lower()}`; selection frozen: `false`.
- S1 Power/Lock fit: `{result['evidence']['s1_power_lock_fit']['status']}` with `{result['evidence']['s1_power_lock_fit']['valid_step_count']}` valid STEP files; selection frozen: `false`.
- Power/Lock production ready: `{str(result['evidence']['power_lock_mechanical']['production_ready']).lower()}`.
- Protected KiCad/reference/OpenSpec inputs unchanged during the run: `{str(result['protected_inputs']['unchanged']).lower()}`.

## Selected runtimes

- Python: `{runtime['python']['python']}` ({runtime['python']['version']})
- CAD/OCP: `{runtime['cad']['python']}` with `PYTHONPATH={runtime['cad']['pythonpath']}`
- Printable-controls CAD/OCP: `{runtime['printable_controls_cad']['python']}` with `PYTHONPATH={runtime['printable_controls_cad']['pythonpath']}`
- Bundled PDF Python: `{runtime['pdf']['python']}` (pypdf `{runtime['pdf']['pypdf_version']}`, reportlab `{runtime['pdf']['reportlab_version']}`)
- Host C compiler: `{runtime['cc']['path']}`
- KiCad pcbnew: `{runtime['kicad']['python']}` ({runtime['kicad']['kicad_version']})
- ngspice: `{runtime['ngspice']}`
- OpenSpec: `{runtime['openspec']}`

## Pending before any freeze/order

{pending}

## Safety boundary

- Validators that normally write local results ran in a `mktemp` mirror.
- `golden-import/`, `working-base/`, both reference enclosure STEP files, OpenSpec change, the 39-file pre-edit source manifest/dry-run, the real L1 schematic, PCB synchronization and controlled local-routing candidates, R_PWR6 sourcing evidence, S1 Power/Lock datasheet/footprint/fit candidates, netlist validator/self-test/11 fixtures, delta contract, printable controls, fit sheets/PDF, and L2 firmware sources were hash-checked before and after.
- A PASS here means the named candidate checks are reproducible now. It is **not** a production-ready claim.
"""


def main() -> int:
    protected_paths = [
        HARDWARE / "golden-import",
        HARDWARE / "working-base",
        REPO / "hardware/reference/oshwhub-project_jofcnupz/顶盖V7.step",
        REPO / "hardware/reference/oshwhub-project_jofcnupz/底盒V7.step",
        CHANGE,
        HARDWARE / "gates/l1-schematic-delta-contract.json",
        HARDWARE / "gates/verify_l1_schematic_delta_contract.py",
        HARDWARE / "gates/verify_l1_candidate_netlist.py",
        HARDWARE / "gates/verify_l1_erc_delta.py",
        HARDWARE / "gates/analyze_l1_pcb_sync_readiness.py",
        HARDWARE / "gates/verify_l1_pcb_only_footprints.py",
        HARDWARE / "gates/selftest_l1_candidate_netlist.py",
        HARDWARE / "gates/l1-candidate-netlist-fixtures",
        HARDWARE / "mechanical/l2-printable-controls",
        HARDWARE / "mechanical/power-lock-s1-fit",
        HARDWARE / "libraries/power-lock-switch-candidate",
        HARDWARE / "derived/l1-schematic-dryrun",
        HARDWARE / "derived/l1-candidate",
        HARDWARE / "derived/l1-pcb-placement",
        HARDWARE / "derived/l1-pcb-candidate",
        HARDWARE / "derived/l1-pcb-routing-candidate",
        HARDWARE / "sourcing/power-bridge",
        HARDWARE / "sourcing/power-lock-switch",
        HARDWARE / "validation/physical-evidence/fit-sheets",
        HARDWARE / "validation/l2-input-firmware",
        REPO / "output/pdf/cadenza-physical-fit-sheets-a4.pdf",
    ]
    before = tree_manifest(protected_paths)

    python_runtime = choose_python_runtime()
    python_candidates = unique_existing([
        python_runtime["python"],
        "/Users/tapir/.local/bin/python3.12",
        "python3.12",
        "python3",
    ])
    cad_runtime = choose_cad_runtime(python_candidates)
    printable_controls_cad_runtime = choose_cad_runtime(
        python_candidates,
        [str(Path.home() / ".cache/cadenza-cad-tools")],
    )
    kicad_runtime = choose_kicad_runtime(python_candidates)
    pdf_runtime = choose_pdf_runtime(python_candidates)
    ngspice = shutil.which("ngspice")
    openspec = shutil.which("openspec")
    cc = shutil.which("cc")
    if not ngspice:
        raise RuntimeError("ngspice not found")
    if not openspec:
        raise RuntimeError("openspec not found")
    if not cc:
        raise RuntimeError("C compiler not found")

    mktemp = subprocess.run(
        ["mktemp", "-d", "-t", "cadenza-current-candidate-gate.XXXXXX"],
        text=True,
        capture_output=True,
        check=True,
    )
    temporary_repo = Path(mktemp.stdout.strip())
    checks: list[dict[str, Any]] = []
    evidence: dict[str, Any] = {}
    try:
        copied_dirs = [
            HARDWARE / "electrical/display",
            HARDWARE / "electrical/input",
            HARDWARE / "libraries/display-fpc-variants",
            HARDWARE / "mechanical/l1-fit-check",
            HARDWARE / "mechanical/l2-input-candidates",
            HARDWARE / "mechanical/l2-printable-controls",
            HARDWARE / "mechanical/power-lock-candidates",
            HARDWARE / "mechanical/power-lock-s1-fit",
            HARDWARE / "libraries/power-lock-switch-candidate",
            HARDWARE / "sourcing/power-lock-switch",
            HARDWARE / "derived/l1-schematic-dryrun",
            HARDWARE / "derived/l1-candidate",
            HARDWARE / "derived/l1-pcb-placement",
            HARDWARE / "derived/l1-pcb-candidate",
            HARDWARE / "derived/l1-pcb-routing-candidate",
            HARDWARE / "validation/physical-evidence/fit-sheets",
            HARDWARE / "validation/l2-input-firmware",
        ]
        for source in copied_dirs:
            copy_dir(source, temporary_repo)
        reference = REPO / "hardware/reference/oshwhub-project_jofcnupz/顶盖V7.step"
        reference_copy = temporary_repo / reference.relative_to(REPO)
        reference_copy.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(reference, reference_copy)
        bottom_reference = REPO / "hardware/reference/oshwhub-project_jofcnupz/底盒V7.step"
        bottom_reference_copy = temporary_repo / bottom_reference.relative_to(REPO)
        bottom_reference_copy.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(bottom_reference, bottom_reference_copy)
        delta_validator = copy_file(HARDWARE / "gates/verify_l1_schematic_delta_contract.py", temporary_repo)
        delta_contract = copy_file(HARDWARE / "gates/l1-schematic-delta-contract.json", temporary_repo)
        candidate_netlist_validator = copy_file(
            HARDWARE / "gates/verify_l1_candidate_netlist.py", temporary_repo
        )
        candidate_netlist_selftest = copy_file(
            HARDWARE / "gates/selftest_l1_candidate_netlist.py", temporary_repo
        )
        erc_delta_validator = copy_file(HARDWARE / "gates/verify_l1_erc_delta.py", temporary_repo)
        pcb_sync_analyzer = copy_file(HARDWARE / "gates/analyze_l1_pcb_sync_readiness.py", temporary_repo)
        pcb_only_footprint_validator = copy_file(
            HARDWARE / "gates/verify_l1_pcb_only_footprints.py", temporary_repo
        )
        copy_dir(HARDWARE / "gates/l1-candidate-netlist-fixtures", temporary_repo)
        copy_file(
            HARDWARE / "working-base/Cadenza-reference-ESP32-S3-game-console-original-v2-20260722.kicad_sch",
            temporary_repo,
        )
        reference_board_copy = copy_file(
            HARDWARE / "working-base/Cadenza-reference-ESP32-S3-game-console-original-v2-20260722.kicad_pcb",
            temporary_repo,
        )
        copy_file(HARDWARE / "working-base/connectivity.netlist.xml", temporary_repo)
        copy_file(REPO / "output/pdf/cadenza-physical-fit-sheets-a4.pdf", temporary_repo)
        (temporary_repo / ".git").mkdir(exist_ok=True)

        temp_hw = temporary_repo / "hardware/cadenza"
        base_env = {"PYTHONPATH": cad_runtime["pythonpath"]}

        checks.append(run_check(
            "display_baseline", "L1 display mapping/domain/GPIO candidate",
            [python_runtime["python"], str(temp_hw / "electrical/display/verify_l1_display_baseline.py")],
            cwd=temporary_repo,
        ))
        checks.append(run_check(
            "fpc_library", "10-pin FPC candidate footprint geometry and Pin 1",
            [python_runtime["python"], str(temp_hw / "libraries/display-fpc-variants/verify_library.py")],
            cwd=temporary_repo,
        ))
        checks.append(run_check(
            "l1_fpc_variants", "L1 four-direction/contact FPC fit variants",
            [cad_runtime["python"], str(temp_hw / "mechanical/l1-fit-check/variants/verify_fpc_direction_variants.py")],
            cwd=temporary_repo, env_extra=base_env,
        ))
        checks.append(run_check(
            "l1_fit_audit", "L1 reference-envelope/window/holes/collision audit",
            [cad_runtime["python"], str(temp_hw / "mechanical/l1-fit-check/audit_l1_fitcheck.py")],
            cwd=temporary_repo, env_extra=base_env,
        ))
        checks.append(run_check(
            "l2_input_spice", "L2 button/encoder RC tolerance-corner SPICE",
            [python_runtime["python"], str(temp_hw / "electrical/input/spice/run_spice.py")],
            cwd=temporary_repo,
        ))
        checks.append(run_check(
            "l2_mechanical", "L2 B/Menu/encoder+A mechanical candidates",
            [python_runtime["python"], str(temp_hw / "mechanical/l2-input-candidates/verify_l2_input_candidates.py")],
            cwd=temporary_repo,
        ))
        delta_result_path = temporary_repo / "results/l1-schematic-delta-contract.json"
        checks.append(run_check(
            "l1_schematic_delta_contract", "L1 schematic delta contract against frozen baseline",
            [
                python_runtime["python"], str(delta_validator),
                "--contract", str(delta_contract), "--output", str(delta_result_path),
            ],
            cwd=temporary_repo,
        ))
        checks.append(run_check(
            "l1_pre_edit_baseline", "L1 pre-edit dry-run baseline proof (not implementation)",
            [
                python_runtime["python"],
                str(temp_hw / "derived/l1-schematic-dryrun/baseline-proof/verify_pre_edit_baseline.py"),
            ],
            cwd=temporary_repo,
        ))
        checks.append(run_check(
            "l1_candidate_netlist_selftest",
            "L1 candidate-netlist validator synthetic self-test (not a real candidate)",
            [python_runtime["python"], str(candidate_netlist_selftest)],
            cwd=temporary_repo,
        ))
        real_candidate_result_path = temporary_repo / "results/l1-real-candidate-netlist.json"
        checks.append(run_check(
            "l1_real_candidate_netlist",
            "L1 real two-page KiCad schematic candidate netlist contract",
            [
                python_runtime["python"], str(candidate_netlist_validator),
                str(temp_hw / "derived/l1-candidate/Cadenza-L1.kicad_sch"),
                "--output", str(real_candidate_result_path),
            ],
            cwd=temporary_repo,
        ))
        erc_delta_result_path = temporary_repo / "results/l1-erc-delta.json"
        checks.append(run_check(
            "l1_erc_delta",
            "L1 candidate adds no new error-level ERC signatures",
            [
                python_runtime["python"], str(erc_delta_validator),
                "--baseline", str(temp_hw / "derived/l1-schematic-dryrun/erc-after-semantic-repair.json"),
                "--candidate", str(temp_hw / "derived/l1-candidate/candidate.erc.json"),
                "--output", str(erc_delta_result_path),
            ],
            cwd=temporary_repo,
        ))
        pcb_sync_result_path = temporary_repo / "results/l1-pcb-sync-readiness.json"
        checks.append(run_check(
            "l1_pcb_sync_readiness",
            "L1 inherited-board to real-schematic footprint sync preflight",
            [
                kicad_runtime["python"], str(pcb_sync_analyzer),
                "--board", str(reference_board_copy),
                "--netlist", str(temp_hw / "derived/l1-candidate/candidate.netlist.xml"),
                "--contract", str(delta_contract),
                "--json-output", str(pcb_sync_result_path),
            ],
            cwd=temporary_repo,
            env_extra={"PYTHONPATH": kicad_runtime["pythonpath"] or ""},
        ))
        pcb_only_footprint_result_path = temporary_repo / "results/l1-pcb-only-footprints.json"
        checks.append(run_check(
            "l1_pcb_only_footprints",
            "L1 net-tie and bare-pad testpoint footprint geometry",
            [
                kicad_runtime["python"], str(pcb_only_footprint_validator),
                "--output", str(pcb_only_footprint_result_path),
            ],
            cwd=temporary_repo,
            env_extra={"PYTHONPATH": kicad_runtime["pythonpath"] or ""},
        ))
        checks.append(run_check(
            "l1_pcb_placement", "L1 FPC/USB/Power planar placement feasibility",
            [
                kicad_runtime["python"],
                str(temp_hw / "derived/l1-pcb-placement/analyze_l1_placement.py"),
            ],
            cwd=temporary_repo,
            env_extra={"PYTHONPATH": kicad_runtime["pythonpath"] or ""},
        ))
        checks.append(run_check(
            "l1_pcb_sync_candidate",
            "L1 two-variant PCB netlist/mechanical synchronization audit",
            [
                kicad_runtime["python"],
                str(temp_hw / "derived/l1-pcb-candidate/verify_l1_pcb_candidate.py"),
            ],
            cwd=temporary_repo,
            env_extra={"PYTHONPATH": kicad_runtime["pythonpath"] or ""},
        ))
        checks.append(run_check(
            "l1_local_routing_candidate",
            "L1 controlled local Power/Lock routing delta audit",
            [
                kicad_runtime["python"],
                str(temp_hw / "derived/l1-pcb-routing-candidate/verify_local_routing_candidate.py"),
            ],
            cwd=temporary_repo,
            env_extra={"PYTHONPATH": kicad_runtime["pythonpath"] or ""},
        ))
        checks.append(run_check(
            "l1_risk_register",
            "L1 printable enclosure and assembly non-frozen risk register",
            [
                cad_runtime["python"],
                str(temp_hw / "mechanical/l1-fit-check/generate_l1_risk_register.py"),
            ],
            cwd=temporary_repo,
            env_extra=base_env,
        ))
        checks.append(run_check(
            "l2_printable_controls", "L2 printable keycaps/knobs and 3×3 assemblies",
            [
                printable_controls_cad_runtime["python"],
                str(temp_hw / "mechanical/l2-printable-controls/verify_printable_controls.py"),
            ],
            cwd=temporary_repo,
            env_extra={"PYTHONPATH": printable_controls_cad_runtime["pythonpath"]},
        ))
        checks.append(run_check(
            "power_lock_mechanical", "Power/Lock mechanical candidates",
            [python_runtime["python"], str(temp_hw / "mechanical/power-lock-candidates/verify_power_lock_candidates.py")],
            cwd=temporary_repo,
        ))
        checks.append(run_check(
            "s1_power_lock_footprint", "S1 datasheet-dimensioned footprint and axis semantics",
            [cad_runtime["python"], str(temp_hw / "libraries/power-lock-switch-candidate/verify_candidate.py")],
            cwd=temporary_repo,
            env_extra=base_env,
        ))
        checks.append(run_check(
            "s1_power_lock_fit", "S1 real-dimension A/B enclosure fit candidates",
            [cad_runtime["python"], str(temp_hw / "mechanical/power-lock-s1-fit/verify_power_lock_s1_fit.py")],
            cwd=temporary_repo,
            env_extra=base_env,
        ))
        checks.append(run_check(
            "physical_fit_sheets", "A4 1:1 connector/caliper fit sheets",
            [
                pdf_runtime["python"],
                str(temp_hw / "validation/physical-evidence/fit-sheets/verify_fit_sheets.py"),
            ],
            cwd=temporary_repo,
        ))
        checks.append(run_host_c_check(
            cc,
            temp_hw / "validation/l2-input-firmware",
            temporary_repo / "results/cadenza-l2-input-host-test",
        ))

        connectivity_json = temporary_repo / "results/reference-connectivity.json"
        connectivity_md = temporary_repo / "results/reference-connectivity.md"
        checks.append(run_check(
            "reference_connectivity", "Reference schematic-to-PCB endpoint topology",
            [
                kicad_runtime["python"], str(HARDWARE / "verify_import_connectivity.py"),
                "--pcb", str(HARDWARE / "working-base/Cadenza-reference-ESP32-S3-game-console-original-v2-20260722.kicad_pcb"),
                "--netlist", str(HARDWARE / "working-base/connectivity.netlist.xml"),
                "--golden-pcb", str(HARDWARE / "golden-import/Cadenza-reference-ESP32-S3-game-console-original-v2-20260722.kicad_pcb"),
                "--json", str(connectivity_json), "--markdown", str(connectivity_md),
            ],
            cwd=REPO,
            env_extra={"PYTHONPATH": kicad_runtime["pythonpath"] or ""},
        ))
        checks.append(run_check(
            "openspec_strict", "OpenSpec change strict validation",
            [openspec, "validate", "adapt-oshwhub-handheld-hardware", "--type", "change", "--strict", "--json"],
            cwd=REPO,
        ))

        display = json_read(temp_hw / "electrical/display/l1-display-electrical-baseline.json")
        fpc = json_read(temp_hw / "libraries/display-fpc-variants/verification.json")
        l1_variants = json_read(temp_hw / "mechanical/l1-fit-check/variants/generated/verification-report.json")
        l1_fit = json_read(temp_hw / "mechanical/l1-fit-check/L1_FITCHECK_AUDIT.json")
        spice = json_read(temp_hw / "electrical/input/spice/results.json")
        l2 = json_read(temp_hw / "mechanical/l2-input-candidates/generated/l2-input-candidates.json")
        power_lock = json_read(temp_hw / "mechanical/power-lock-candidates/generated/power-lock-candidates.json")
        s1_power_lock_footprint = json_read(
            temp_hw / "libraries/power-lock-switch-candidate/verification.json"
        )
        s1_power_lock_fit = json_read(
            temp_hw / "mechanical/power-lock-s1-fit/verification.json"
        )
        delta_contract_data = json_read(delta_contract)
        delta_result = json_read(delta_result_path)
        pre_edit_baseline = json_read(
            temp_hw / "derived/l1-schematic-dryrun/baseline-proof/verification.json"
        )
        candidate_netlist_selftest_result = json.loads(
            next(c["stdout"] for c in checks if c["id"] == "l1_candidate_netlist_selftest")
        )
        real_candidate_result = json_read(real_candidate_result_path)
        erc_delta_result = json_read(erc_delta_result_path)
        pcb_sync_result = json_read(pcb_sync_result_path)
        pcb_only_footprint_result = json_read(pcb_only_footprint_result_path)
        pcb_placement_result = json_read(
            temp_hw / "derived/l1-pcb-placement/placement-feasibility.json"
        )
        pcb_sync_candidate = json_read(
            temp_hw / "derived/l1-pcb-candidate/verification-report.json"
        )
        local_routing_candidate = json_read(
            temp_hw / "derived/l1-pcb-routing-candidate/verification-report.json"
        )
        l1_risk_register = json_read(
            temp_hw / "mechanical/l1-fit-check/L1_PRINT_ASSEMBLY_RISKS.json"
        )
        printable_controls = json_read(temp_hw / "mechanical/l2-printable-controls/MESH_AND_ASSEMBLY_REPORT.json")
        physical_fit_sheets = json_read(temp_hw / "validation/physical-evidence/fit-sheets/verification.json")
        connectivity = json_read(connectivity_json)
        openspec_result = json.loads(next(c["stdout"] for c in checks if c["id"] == "openspec_strict"))

        evidence = {
            "display": {
                "status": "PASS" if checks[0]["status"] == "PASS" else "FAIL",
                "physical_direction_selection_frozen": display["physical_direction"]["selection_frozen"],
            },
            "fpc_library": {
                "status": fpc["status"], "footprint_count": fpc["footprint_count"],
                "candidate_not_frozen": fpc["candidate_not_frozen"],
            },
            "l1_fpc_variants": {
                "status": l1_variants["status"], "variant_count": l1_variants["variant_count"],
                "selection_frozen": l1_variants["selection_frozen"],
                "pending_count": l1_variants["pending_count"],
            },
            "l1_fit": {
                "status": l1_fit["status"],
                "maximum_hard_collision_volume_mm3": max(
                    l1_fit["semantic_evidence"]["base_collision_max_mm3"],
                    l1_fit["semantic_evidence"]["variant_collision_max_mm3"],
                ),
                "valid_step_count": l1_fit["semantic_evidence"]["valid_step_count"],
            },
            "l2_input_spice": {
                "status": spice["status"],
                "corners": [name for name, value in spice["corners"].items() if value["status"] == "pass"],
                "limitations": spice["limitations"],
            },
            "l2_mechanical": {
                "status": checks[5]["status"], "variant_count": len(l2["variants"]),
                "selection_frozen": False,
            },
            "l1_schematic_delta_contract": {
                "status": delta_result["verdict"],
                "checks": delta_result["summary"]["checks"],
                "passed": delta_result["summary"]["passed"],
                "failed": delta_result["summary"]["failed"],
                "fpc_selection_frozen": delta_contract_data["decisions"]["fpc_footprint_selected"],
                "scope_note": delta_result["scope_note"],
            },
            "l1_pre_edit_baseline": {
                "status": pre_edit_baseline["status"],
                "checks": len(pre_edit_baseline["checks"]),
                "passed": sum(item["pass"] for item in pre_edit_baseline["checks"]),
                "source_manifest_entries": next(
                    item["actual"]
                    for item in pre_edit_baseline["checks"]
                    if item["name"] == "manifest_entry_count"
                ),
                "l1_implemented": False,
                "scope": "immutable pre-edit copy and baseline evidence only",
                "note": "PASS does not prove that the L1 schematic delta has been applied.",
            },
            "l1_candidate_netlist_selftest": {
                "status": candidate_netlist_selftest_result["status"],
                "fixtures": candidate_netlist_selftest_result["summary"]["total"],
                "passed": candidate_netlist_selftest_result["summary"]["passed"],
                "failed": candidate_netlist_selftest_result["summary"]["failed"],
                "fixture_evidence_only": candidate_netlist_selftest_result["fixture_evidence_only"],
                "real_candidate_validated": candidate_netlist_selftest_result["real_candidate_validated"],
                "validator": str(candidate_netlist_validator.relative_to(temporary_repo)),
                "note": "Synthetic PASS tests the validator; it is not evidence of an implemented L1 candidate.",
            },
            "l1_real_candidate_netlist": {
                "status": real_candidate_result["status"],
                "checks": real_candidate_result["summary"]["total"],
                "passed": real_candidate_result["summary"]["passed"],
                "failed": real_candidate_result["summary"]["failed"],
                "real_candidate_validated": real_candidate_result["status"] == "PASS_CANDIDATE",
                "l1_schematic_implemented": True,
                "pcb_synced": pcb_sync_candidate["status"] == "PASS_SYNC_VERIFIED",
                "production_ready": False,
                "fpc_selection_frozen": real_candidate_result["fpc_selection_frozen"],
                "scope": "real KiCad hierarchical schematic structure and netlist only",
            },
            "l1_erc_delta": {
                "status": erc_delta_result["status"],
                "baseline_errors": erc_delta_result["error_delta"]["baseline"],
                "candidate_errors": erc_delta_result["error_delta"]["candidate"],
                "new_errors": erc_delta_result["error_delta"]["new"],
                "removed_errors": erc_delta_result["error_delta"]["removed"],
                "production_ready": False,
                "scope": erc_delta_result["scope"],
            },
            "l1_pcb_sync_readiness": {
                "status": pcb_sync_result["status"],
                "new_with_footprints": pcb_sync_result["counts"]["new_components_with_footprints"],
                "new_without_footprints": pcb_sync_result["counts"]["new_components_without_footprints"],
                "physical_blockers": pcb_sync_result["blockers"]["physical_selection"],
                "metadata_repairs": pcb_sync_result["completed_metadata_repairs"],
                "pcb_sync_ready": pcb_sync_result["pcb_sync_ready"],
                "production_ready": pcb_sync_result["production_ready"],
            },
            "l1_pcb_only_footprints": {
                "status": pcb_only_footprint_result["status"],
                "nominal_drop_mv": pcb_only_footprint_result["nominal_1oz_copper_estimate"]["voltage_drop_mv"],
                "nominal_power_mw": pcb_only_footprint_result["nominal_1oz_copper_estimate"]["power_mw"],
                "layout_validated": pcb_only_footprint_result["layout_validated"],
                "production_ready": pcb_only_footprint_result["production_ready"],
            },
            "l1_pcb_placement": {
                "status": pcb_placement_result["status"],
                "checks": len(pcb_placement_result["checks"]),
                "fpc_usb_gap_mm": pcb_placement_result["findings"]["clearances_mm"]["fpc_minus_y_to_shifted_usb_x"],
                "usb_led_gap_mm": pcb_placement_result["findings"]["clearances_mm"]["led1_to_shifted_usb_x"],
                "usb_shift_x_mm": pcb_placement_result["candidate"]["usb1_move"]["delta_xy_mm"][0],
                "power_lock_preference": pcb_placement_result["candidate"]["power_lock_preference"],
                "pcb_created": pcb_sync_candidate["status"] == "PASS_SYNC_VERIFIED",
                "selection_frozen": pcb_placement_result["selection_frozen"],
                "production_ready": pcb_placement_result["production_ready"],
            },
            "l1_pcb_sync_candidate": {
                "status": pcb_sync_candidate["status"],
                "checks": pcb_sync_candidate["checks_total"],
                "passed": pcb_sync_candidate["checks_passed"],
                "variants": len(pcb_sync_candidate["variants"]),
                "footprints": pcb_sync_candidate["variants"]["rotation_0"]["footprints"],
                "named_nets": pcb_sync_candidate["variants"]["rotation_0"]["named_nets"],
                "routed": pcb_sync_candidate["routed"],
                "drc_passed": pcb_sync_candidate["drc_passed"],
                "production_ready": pcb_sync_candidate["production_ready"],
                "selection_frozen": pcb_sync_candidate["selection_frozen"],
            },
            "l1_local_routing_candidate": {
                "status": local_routing_candidate["status"],
                "checks": local_routing_candidate["checks_total"],
                "passed": local_routing_candidate["checks_passed"],
                "tracks_added": len(local_routing_candidate["variants"]["rotation-0"]["added_track_uuids"]),
                "source_unconnected": local_routing_candidate["variants"]["rotation-0"]["source_unconnected"],
                "output_unconnected": local_routing_candidate["variants"]["rotation-0"]["output_unconnected"],
                "routing_complete": local_routing_candidate["routing_complete"],
                "drc_passed": local_routing_candidate["drc_passed"],
                "production_ready": local_routing_candidate["production_ready"],
                "selection_frozen": local_routing_candidate["selection_frozen"],
            },
            "l1_risk_register": {
                "status": l1_risk_register["status"],
                "risk_count": len(l1_risk_register["risks"]),
                "risk_counts": l1_risk_register["risk_counts"],
                "selection_frozen": l1_risk_register["selection_frozen"],
                "production_ready": l1_risk_register["production_ready"],
            },
            "l2_printable_controls": {
                "status": printable_controls["status"],
                "layouts": printable_controls["layouts"],
                "shaft_interfaces_per_layout": printable_controls["shaft_interfaces_per_layout"],
                "step_files_checked": printable_controls["step_files_checked"],
                "stl_files_checked": printable_controls["stl_files_checked"],
                "selection_frozen": printable_controls["selection_frozen"],
                "production_ready": printable_controls["production_ready"],
                "scope": printable_controls["scope"],
            },
            "physical_fit_sheets": {
                "status": physical_fit_sheets["status"],
                "orientation_frozen": physical_fit_sheets["orientation_frozen"],
                "source_footprints_checked": physical_fit_sheets["source_footprints_checked"],
                "svg_count": len(physical_fit_sheets["svg"]),
                "pdf_pages_checked": physical_fit_sheets["pdf"]["pages_checked"],
                "failures": physical_fit_sheets["failures"],
            },
            "l2_input_firmware": {
                "host_c_status": next(c["status"] for c in checks if c["id"] == "l2_input_host_c"),
                "compiler_flags": ["-std=c11", "-Wall", "-Wextra", "-Werror"],
                "host_test_output": next(c["stdout"] for c in checks if c["id"] == "l2_input_host_c"),
                "esp_idf_full_build": {
                    "status": "PASS_RECORDED_NOT_RERUN",
                    "included_in_daily_gate": False,
                    "evidence_source": "hardware/cadenza/validation/l2-input-firmware/README.md",
                    "esp_idf_version": "v5.5",
                    "esp_idf_commit": "8c750b088c7cd857d079c0eeb495da199b359461",
                    "target": "esp32s3",
                    "application_image_bytes": 183136,
                    "application_image_sha256": "b5729794e867276939669c053dbcf58ba694661c1e253343412a692f70795b6a",
                    "flash_performed": False,
                },
            },
            "power_lock_mechanical": {
                "status": next(c["status"] for c in checks if c["id"] == "power_lock_mechanical"),
                "variant_count": len(power_lock["variants"]),
                "selection_frozen": False, "production_ready": False,
            },
            "s1_power_lock_footprint": {
                "status": next(c["status"] for c in checks if c["id"] == "s1_power_lock_footprint"),
                "axis_semantics_verified": s1_power_lock_footprint["checks"]["axis_semantics_exact"]
                and s1_power_lock_footprint["checks"]["H_not_Z"],
                "electrical_pad_count": s1_power_lock_footprint["pad_count_electrical"],
                "npth_count": s1_power_lock_footprint["npth_count"],
                "selection_frozen": False,
                "production_ready": False,
            },
            "s1_power_lock_fit": {
                "status": next(c["status"] for c in checks if c["id"] == "s1_power_lock_fit"),
                "valid_step_count": s1_power_lock_fit["valid_step_count"],
                "axis_semantics_verified": s1_power_lock_fit["checks"]["axis_semantics"]
                and s1_power_lock_fit["checks"]["H_is_Y_not_Z"],
                "collisions_zero": s1_power_lock_fit["checks"]["collisions_zero"],
                "selection_frozen": False,
                "production_ready": False,
            },
            "reference_connectivity": {"verdict": connectivity["verdict"], **connectivity["counts"]},
            "openspec": {
                "valid": openspec_result["summary"]["totals"]["failed"] == 0,
                "passed": openspec_result["summary"]["totals"]["passed"],
            },
        }
    finally:
        shutil.rmtree(temporary_repo, ignore_errors=True)

    after = tree_manifest(protected_paths)
    unchanged = before == after
    changed_files = sorted(set(before) | set(after))
    changed_files = [path for path in changed_files if before.get(path) != after.get(path)]
    checks.append({
        "id": "protected_inputs_unchanged",
        "title": "Protected KiCad/reference/OpenSpec/candidate inputs unchanged",
        "status": "PASS" if unchanged else "FAIL",
        "exit_code": 0 if unchanged else 1,
        "command": "SHA-256 manifest before/after",
        "cwd": str(REPO),
        "stdout": f"protected files: {len(before)}; changed: {len(changed_files)}",
        "stderr": "",
    })

    passed = all(check["status"] == "PASS" for check in checks)
    result = {
        "schema_version": 1,
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "verdict": "PASS_CANDIDATE" if passed else "FAIL",
        "release_class": "candidate_only_not_production_ready",
        "production_ready": False,
        "selection_frozen": False,
        "repository": str(REPO),
        "host": {
            "platform": f"{os.uname().sysname}-{os.uname().release}",
            "machine": os.uname().machine,
        },
        "runtimes": {
            "python": python_runtime,
            "cad": cad_runtime,
            "printable_controls_cad": printable_controls_cad_runtime,
            "pdf": pdf_runtime,
            "kicad": kicad_runtime,
            "cc": {
                "path": cc,
                "version": subprocess.run(
                    [cc, "--version"], text=True, capture_output=True, check=False
                ).stdout.splitlines()[0],
            },
            "ngspice": ngspice,
            "openspec": openspec,
        },
        "checks": checks,
        "evidence": evidence,
        "protected_inputs": {
            "paths": [str(p.relative_to(REPO)) for p in protected_paths],
            "file_count": len(before),
            "unchanged": unchanged,
            "changed_files": changed_files,
        },
        "pending_before_freeze": [
            "用真实 10-pin FPC/转接板确认触点面、Pin 1 与自然出线方向。",
            "用真实 LS027B7DH01 做无应力装配、窗口无遮挡、FPC 不受拉力和完整闭壳测试。",
            "实测 EC12/按键尺寸、每格脉冲和接点抖动；当前 SPICE 只证明候选 RC 模型。",
            "冻结 Power/Lock 开关料号、封装、按帽配合，并验证短按锁屏/长按硬关机行为。",
            "完成已同步 L1 PCB 候选的布局清理和 49 个未连接项布线，再运行 DRC、交叉分析、EMC、DFM、BOM/CPL/Gerber 门禁。",
            "打印 L2 输入候选并完成手感、可达性、旋钮尺寸及完整外壳闭合验证。",
        ],
    }
    HERE.mkdir(parents=True, exist_ok=True)
    OUTPUT_JSON.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    OUTPUT_MD.write_text(render_markdown(result), encoding="utf-8")
    print(json.dumps({
        "verdict": result["verdict"],
        "checks_passed": sum(c["status"] == "PASS" for c in checks),
        "checks_total": len(checks),
        "production_ready": False,
        "selection_frozen": False,
        "json": str(OUTPUT_JSON),
        "markdown": str(OUTPUT_MD),
    }, ensure_ascii=False))
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
