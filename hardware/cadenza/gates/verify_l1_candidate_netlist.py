#!/usr/bin/env python3
"""Validate an L1 candidate schematic/netlist against the frozen delta contract.

The validator is deliberately read-only.  A ``.kicad_sch`` input is exported to
a temporary KiCad XML netlist; an XML input is parsed directly.  Passing this
gate proves only structural conformance to the L1 schematic delta contract.  It
does not select the FPC footprint and does not imply fabrication readiness.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
from dataclasses import dataclass
from pathlib import Path
from typing import Any


HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]
DEFAULT_CONTRACT = HERE / "l1-schematic-delta-contract.json"


@dataclass(frozen=True)
class Component:
    value: str
    footprint: str
    lib_id: str


@dataclass
class Netlist:
    components: dict[str, Component]
    component_count: int
    duplicate_component_refs: list[str]
    pins: dict[str, dict[str, str]]
    pinfunctions: dict[str, dict[str, str]]
    nets: dict[str, set[tuple[str, str]]]


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def repo_path(value: str) -> Path:
    path = Path(value)
    return path if path.is_absolute() else REPO / path


def parse_xml(path: Path) -> Netlist:
    root = ET.parse(path).getroot()
    components: dict[str, Component] = {}
    component_count = 0
    duplicate_component_refs: set[str] = set()
    pins: dict[str, dict[str, str]] = {}
    pinfunctions: dict[str, dict[str, str]] = {}
    nets: dict[str, set[tuple[str, str]]] = {}
    for node in root.findall("./components/comp"):
        component_count += 1
        ref = node.attrib["ref"]
        if ref in components:
            duplicate_component_refs.add(ref)
        components[ref] = Component(
            value=node.findtext("value", default=""),
            footprint=node.findtext("footprint", default="").strip(),
            lib_id=(
                f"{node.find('libsource').attrib.get('lib', '')}:{node.find('libsource').attrib.get('part', '')}"
                if node.find("libsource") is not None else ""
            ),
        )
        pins[ref] = {}
        pinfunctions[ref] = {}
    for net in root.findall("./nets/net"):
        name = net.attrib["name"]
        endpoints: set[tuple[str, str]] = set()
        for node in net.findall("node"):
            ref, pin = node.attrib["ref"], node.attrib["pin"]
            if pin in pins.setdefault(ref, {}):
                raise ValueError(f"endpoint {ref}.{pin} appears on more than one net")
            endpoints.add((ref, pin))
            pins[ref][pin] = name
            pinfunctions.setdefault(ref, {})[pin] = node.attrib.get("pinfunction", "")
        nets[name] = endpoints
    return Netlist(components, component_count, sorted(duplicate_component_refs), pins, pinfunctions, nets)


def find_kicad_cli(explicit: str | None) -> str:
    candidates = [
        explicit,
        shutil.which("kicad-cli"),
        "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli",
        "/Applications/KiCad.app/Contents/MacOS/kicad-cli",
    ]
    for candidate in candidates:
        if candidate and Path(candidate).is_file():
            return candidate
    raise RuntimeError("kicad-cli not found; pass --kicad-cli or provide an XML netlist")


def candidate_xml(candidate: Path, kicad_cli: str | None) -> tuple[Path, tempfile.TemporaryDirectory[str] | None, dict[str, Any]]:
    if candidate.suffix.lower() in {".xml", ".net"}:
        return candidate, None, {"input_kind": "kicad_xml_netlist", "exported_by_kicad": False}
    if candidate.suffix.lower() != ".kicad_sch":
        raise ValueError("candidate must be a .kicad_sch, .xml, or .net file")
    temp = tempfile.TemporaryDirectory(prefix="cadenza-l1-netlist-")
    output = Path(temp.name) / "candidate.netlist.xml"
    cli = find_kicad_cli(kicad_cli)
    proc = subprocess.run(
        [cli, "sch", "export", "netlist", "--format", "kicadxml", "--output", str(output), str(candidate)],
        text=True,
        capture_output=True,
        check=False,
        timeout=120,
        env={**os.environ, "PYTHONDONTWRITEBYTECODE": "1"},
    )
    if proc.returncode != 0 or not output.is_file():
        temp.cleanup()
        raise RuntimeError(f"KiCad netlist export failed ({proc.returncode}): {(proc.stderr or proc.stdout).strip()}")
    return output, temp, {
        "input_kind": "kicad_schematic",
        "exported_by_kicad": True,
        "kicad_cli": cli,
        "export_stderr": proc.stderr.strip(),
    }


def norm_value(value: str) -> str:
    return re.sub(r"[^a-z0-9.+-]", "", value.replace("Ω", "ohm").lower())


def capacitance_uf(value: str) -> float | None:
    match = re.search(r"([0-9]+(?:\.[0-9]+)?)\s*(pf|nf|uf|µf|mf|f)(?=$|[^a-z])", value.lower())
    if not match:
        return None
    number = float(match.group(1))
    factors = {"pf": 1e-6, "nf": 1e-3, "uf": 1.0, "µf": 1.0, "mf": 1e3, "f": 1e6}
    return number * factors[match.group(2)]


def resistance_ohms(value: str) -> float | None:
    """Parse common KiCad resistor forms, ignoring documented suffix notes."""
    text = value.strip().lower().replace("ω", "ohm").replace("Ω", "ohm")
    match = re.match(r"^([0-9]+(?:\.[0-9]+)?)\s*(meg|[kmg])?\s*(?:r|ohms?)?(?=$|[^a-z])", text)
    if not match:
        return None
    factors = {None: 1.0, "k": 1e3, "m": 1e6, "meg": 1e6, "g": 1e9}
    return float(match.group(1)) * factors[match.group(2)]


def equivalent_new_value(reference: str, expected: str, actual: str) -> bool:
    if reference.startswith("C_DISP"):
        # Exact/minimum capacitor semantics are checked by the dedicated
        # decoupling check, including 100 nF == 0.1 uF.
        return capacitance_uf(actual) is not None
    if reference.startswith("R_PWR"):
        expected_ohms = resistance_ohms(expected)
        actual_ohms = resistance_ohms(actual)
        return expected_ohms is not None and actual_ohms is not None and abs(expected_ohms - actual_ohms) < 1e-12
    return norm_value(actual) == norm_value(expected)


def canonical_pinfunction(function: str) -> str | None:
    """Normalize explicit KiCad pin labels without accepting unknown/empty data."""
    tokens = [token for token in re.split(r"[^a-z0-9]+", function.strip().lower()) if token]
    symbolic = [token for token in tokens if not token.isdigit()]
    if not symbolic:
        return None
    joined = "".join(symbolic)
    aliases = {
        "a1": "A1", "anode1": "A1",
        "a2": "A2", "anode2": "A2",
        "k": "K_common", "kcommon": "K_common", "commonk": "K_common",
        "commoncathode": "K_common", "cathodecommon": "K_common", "cathode": "K_common",
        "g": "G", "gate": "G",
        "s": "S", "source": "S",
        "d": "D", "drain": "D",
    }
    return aliases.get(joined) or (aliases.get(symbolic[0]) if len(symbolic) == 1 else None)


def is_isolated(net: str | None, ref: str, pin: str) -> bool:
    if net is None:
        return True
    low = net.lower()
    return "unconnected" in low and ref.lower() in low and (f"pad{pin}" in low or f"pin{pin}" in low)


def resolve_pin(spec: str, candidate: Netlist, baseline: Netlist) -> tuple[str, str]:
    ref, label = spec.split(".", 1)
    numeric = re.match(r"(\d+)", label)
    if numeric:
        return ref, numeric.group(1)
    gpio = re.fullmatch(r"GPIO(\d+)", label)
    if gpio:
        number = gpio.group(1)
        functions = baseline.pinfunctions.get(ref, {}) | candidate.pinfunctions.get(ref, {})
        matches = [pin for pin, function in functions.items() if re.search(rf"(?:GPIO|IO){number}(?:_|$)", function, re.I)]
        if len(matches) == 1:
            return ref, matches[0]
    raise ValueError(f"cannot resolve symbolic pin {spec}")


def add_check(checks: list[dict[str, Any]], check_id: str, passed: bool, expected: Any, actual: Any) -> None:
    checks.append({"id": check_id, "status": "PASS" if passed else "FAIL", "expected": expected, "actual": actual})


def validate(candidate_path: Path, contract_path: Path, kicad_cli: str | None = None) -> dict[str, Any]:
    contract = json.loads(contract_path.read_text(encoding="utf-8"))
    baseline_meta = contract["reference_baseline"]
    baseline_sch = repo_path(baseline_meta["schematic"])
    baseline_xml = repo_path(baseline_meta["netlist"])
    checks: list[dict[str, Any]] = []

    hashes = {
        "schematic": sha256(baseline_sch),
        "netlist": sha256(baseline_xml),
    }
    add_check(
        checks, "baseline_hashes_match_contract",
        hashes["schematic"] == baseline_meta["schematic_sha256"] and hashes["netlist"] == baseline_meta["netlist_sha256"],
        {"schematic": baseline_meta["schematic_sha256"], "netlist": baseline_meta["netlist_sha256"]}, hashes,
    )

    xml_path, temp, input_meta = candidate_xml(candidate_path, kicad_cli)
    try:
        baseline = parse_xml(baseline_xml)
        candidate = parse_xml(xml_path)
    finally:
        if temp is not None:
            temp.cleanup()

    delete = {row["reference"] for row in contract["reference_components"]["delete"]}
    modify = {row["reference"] for row in contract["reference_components"]["modify"]}
    retain = set(contract["reference_components"]["retain"])
    new = {row["reference"] for row in contract["new_components"]}
    metadata_repairs = {
        row["reference"]: row
        for row in contract["reference_components"].get("allowed_metadata_repairs", [])
    }
    expected_refs = (set(baseline.components) - delete) | new
    actual_refs = set(candidate.components)
    add_check(checks, "candidate_real_component_count", candidate.component_count == 94, 94, candidate.component_count)
    add_check(checks, "candidate_component_designators_unique", not candidate.duplicate_component_refs, "no duplicates", candidate.duplicate_component_refs)
    add_check(checks, "candidate_designator_set_exact", actual_refs == expected_refs, sorted(expected_refs), sorted(actual_refs))
    add_check(checks, "nine_deleted_designators_absent", delete.isdisjoint(actual_refs) and len(delete) == 9, sorted(delete), sorted(delete & actual_refs))
    add_check(checks, "one_modified_designator_present", modify <= actual_refs and len(modify) == 1, sorted(modify), sorted(modify & actual_refs))
    add_check(checks, "sixty_seven_retained_designators_present", retain <= actual_refs and len(retain) == 67, 67, len(retain & actual_refs))
    add_check(checks, "twenty_six_new_designators_present", new <= actual_refs and len(new) == 26, 26, len(new & actual_refs))

    property_drift = []
    for ref in sorted(retain | modify):
        before, after = baseline.components.get(ref), candidate.components.get(ref)
        repair = metadata_repairs.get(ref)
        if repair:
            repair_ok = (
                repair["field"] == "Footprint"
                and before is not None and after is not None
                and before.footprint == repair["from"]
                and after.footprint == repair["to"]
                and before.value == after.value
                and before.lib_id == after.lib_id
            )
        else:
            repair_ok = before == after
        if not repair_ok:
            property_drift.append({"reference": ref, "baseline": before.__dict__ if before else None, "candidate": after.__dict__ if after else None})
    add_check(
        checks,
        "retained_and_modified_value_footprint_preserved",
        not property_drift,
        "no drift except exact contract-listed footprint metadata recovery",
        property_drift,
    )

    jdisp = candidate.components.get("J_DISP1")
    add_check(checks, "j_disp1_footprint_empty", bool(jdisp) and not jdisp.footprint, "", None if jdisp is None else jdisp.footprint)

    pcb_only_footprint_failures = []
    for row in contract["new_components"]:
        expected_footprint = row.get("footprint")
        if not expected_footprint:
            continue
        actual_component = candidate.components.get(row["reference"])
        actual_footprint = actual_component.footprint if actual_component else None
        if actual_footprint != expected_footprint:
            pcb_only_footprint_failures.append({
                "reference": row["reference"],
                "expected": expected_footprint,
                "actual": actual_footprint,
            })
    add_check(
        checks,
        "declared_pcb_only_footprints_exact",
        not pcb_only_footprint_failures,
        "R_PWR6 net-tie and 13 bare-pad testpoint footprints",
        pcb_only_footprint_failures,
    )

    new_value_failures = []
    for row in contract["new_components"]:
        ref, expected = row["reference"], row["value"]
        actual_component = candidate.components.get(ref)
        actual = actual_component.value if actual_component else None
        if actual is None or not equivalent_new_value(ref, expected, actual):
            new_value_failures.append({"reference": ref, "expected": expected, "actual": actual})
    add_check(checks, "new_component_values_match_contract", not new_value_failures, "26 contract-equivalent values", new_value_failures)

    modified_net_map = {row["from"]: row["to"] for row in contract["reference_nets"]["modify"]}
    deleted_nets = set(contract["reference_nets"]["delete"])
    endpoint_drift = []
    for ref in sorted(retain):
        unexpected_pins = sorted(set(candidate.pins.get(ref, {})) - set(baseline.pins.get(ref, {})))
        for pin in unexpected_pins:
            endpoint_drift.append({
                "endpoint": f"{ref}.{pin}", "baseline": None,
                "expected": "no additional protected-component pin endpoint",
                "actual": candidate.pins[ref][pin],
            })
        for pin, old_net in sorted(baseline.pins.get(ref, {}).items()):
            actual = candidate.pins.get(ref, {}).get(pin)
            if old_net in deleted_nets:
                ok = is_isolated(actual, ref, pin)
                expected = "isolated_after_deleted_net"
            else:
                expected = modified_net_map.get(old_net, old_net)
                ok = actual == expected
            if not ok:
                endpoint_drift.append({"endpoint": f"{ref}.{pin}", "baseline": old_net, "expected": expected, "actual": actual})
    add_check(checks, "sixty_seven_retain_endpoints_preserved", not endpoint_drift, "mapped baseline endpoints", endpoint_drift)

    sw2_drift = []
    for pin in sorted(set(candidate.pins.get("SW2", {})) - set(baseline.pins["SW2"])):
        sw2_drift.append({"pin": pin, "expected": "no additional SW2 pin endpoint", "actual": candidate.pins["SW2"][pin]})
    for pin, old_net in sorted(baseline.pins["SW2"].items()):
        actual = candidate.pins.get("SW2", {}).get(pin)
        if pin == "5":
            ok, expected = is_isolated(actual, "SW2", "5"), "isolated/no-connect"
        else:
            ok, expected = actual == old_net, old_net
        if not ok:
            sw2_drift.append({"pin": pin, "expected": expected, "actual": actual})
    add_check(checks, "sw2_only_pad5_changes", not sw2_drift, "pad 5 isolated; all other pads unchanged", sw2_drift)

    sharp_expected = {str(row["pin"]): row["net"] for row in contract["display_connector"]["pin_to_net"]}
    sharp_actual = candidate.pins.get("J_DISP1", {})
    add_check(checks, "sharp_pin_1_to_10_exact", sharp_actual == sharp_expected, sharp_expected, sharp_actual)

    cap_failures = []
    for row in contract["decoupling"]:
        ref = row["reference"]
        nets = sorted(candidate.pins.get(ref, {}).values())
        expected_nets = sorted(row["between"])
        actual_uf = capacitance_uf(candidate.components.get(ref, Component("", "", "")).value)
        minimum = row.get("minimum_value_uf", row.get("value_uf"))
        exact = row.get("value_uf")
        value_ok = actual_uf is not None and actual_uf + 1e-12 >= minimum and (exact is None or abs(actual_uf - exact) < 1e-9)
        if nets != expected_nets or not value_ok:
            cap_failures.append({"reference": ref, "expected_nets": expected_nets, "actual_nets": nets, "minimum_uf": minimum, "actual_uf": actual_uf})
    add_check(checks, "display_capacitors_connections_and_values", not cap_failures, "contract decoupling", cap_failures)

    power_failures = []
    for row in contract["power_lock"]["required_connections"]:
        ref, pin = resolve_pin(row["component_pin"], candidate, baseline)
        actual = candidate.pins.get(ref, {}).get(pin)
        if actual != row["net"]:
            power_failures.append({"component_pin": row["component_pin"], "resolved": f"{ref}.{pin}", "expected": row["net"], "actual": actual})
    add_check(checks, "power_lock_endpoints_exact", not power_failures, "contract required_connections", power_failures)

    pinfunction_failures = []
    pinfunction_exemptions = []
    invariant_refs = {
        "D_PWR1_BAT54C": "D_PWR1",
        "Q_PWR1_AO3400A": "Q_PWR1",
    }
    for invariant_name, pin_map in contract["power_lock"]["pinout_invariants"].items():
        ref = invariant_refs.get(invariant_name)
        if ref is None:
            pinfunction_failures.append({"invariant": invariant_name, "error": "unsupported invariant reference mapping"})
            continue
        component = candidate.components.get(ref)
        lib_id = component.lib_id if component else ""
        if ref == "D_PWR1" and lib_id != "Diode:BAT54C":
            pinfunction_failures.append({
                "reference": ref, "expected_lib_id": "Diode:BAT54C", "actual_lib_id": lib_id,
            })
        if ref == "Q_PWR1" and lib_id != "Transistor_FET:AO3400A":
            pinfunction_failures.append({
                "reference": ref, "expected_lib_id": "Transistor_FET:AO3400A", "actual_lib_id": lib_id,
            })
        raw_functions = {pin: candidate.pinfunctions.get(ref, {}).get(pin, "") for pin in pin_map}
        if ref == "D_PWR1" and lib_id == "Diode:BAT54C" and all(not raw.strip() for raw in raw_functions.values()):
            pinfunction_exemptions.append({
                "reference": "D_PWR1",
                "lib_id": "Diode:BAT54C",
                "pins": {"1": "A1", "2": "A2", "3": "K_common"},
                "basis": "KiCad 10 standard Diode:BAT54C has blank XML pinfunction names; standard-library symbol geometry and BAT54C datasheet pinout were reviewed outside this parser",
            })
            continue
        for pin, expected in pin_map.items():
            raw = raw_functions[pin]
            actual = canonical_pinfunction(raw)
            if actual != expected:
                pinfunction_failures.append({
                    "endpoint": f"{ref}.{pin}", "expected": expected,
                    "actual": actual, "raw_pinfunction": raw,
                })
    add_check(checks, "power_lock_pinfunction_invariants", not pinfunction_failures, contract["power_lock"]["pinout_invariants"], pinfunction_failures)

    tp_failures = []
    for row in contract["test_points"]:
        actual_nets = set(candidate.pins.get(row["reference"], {}).values())
        if actual_nets != {row["net"]}:
            tp_failures.append({"reference": row["reference"], "expected": row["net"], "actual": sorted(actual_nets)})
    add_check(checks, "test_point_endpoints_exact", not tp_failures, "one declared net per test point", tp_failures)

    forbidden = []
    baseline_gpio_pins = {
        pin for pin, function in baseline.pinfunctions.get("U4", {}).items()
        if re.search(r"(?:GPIO|IO)\d+", function, re.I)
    }
    for pin, net in candidate.pins.get("U4", {}).items():
        function = candidate.pinfunctions.get("U4", {}).get(pin) or baseline.pinfunctions.get("U4", {}).get(pin, "")
        if net == "+5V" and pin in baseline_gpio_pins:
            forbidden.append({"rule": "+5V_to_U4_GPIO", "endpoint": f"U4.{pin}", "pinfunction": function})
        if net in {"PWR_IP5306_KEY", "PWR_BUTTON_NODE"} and pin in baseline_gpio_pins:
            forbidden.append({"rule": "unisolated_power_key_node_to_U4_GPIO", "endpoint": f"U4.{pin}", "net": net})
    for pin in ("6", "7", "8"):
        if candidate.pins.get("J_DISP1", {}).get(pin) == "3V3M":
            forbidden.append({"rule": "3V3M_to_Sharp_power_pin", "endpoint": f"J_DISP1.{pin}"})
    add_check(checks, "forbidden_voltage_domain_connections_absent", not forbidden, "none", forbidden)

    failures = [row for row in checks if row["status"] == "FAIL"]
    return {
        "schema_version": 1,
        "gate": "cadenza_l1_candidate_netlist_contract",
        "status": "PASS_CANDIDATE" if not failures else "FAIL",
        "candidate": str(candidate_path.resolve()),
        "candidate_sha256": sha256(candidate_path),
        "input": input_meta,
        "contract": str(contract_path.resolve()),
        "evidence_scope": "schematic_structure_only",
        "production_ready": False,
        "fpc_selection_frozen": False,
        "fixture_pass_is_not_real_candidate_evidence": True,
        "summary": {"total": len(checks), "passed": len(checks) - len(failures), "failed": len(failures)},
        "details": {
            "pinfunction_exemptions": pinfunction_exemptions,
            "allowed_metadata_repairs": list(metadata_repairs.values()),
        },
        "checks": checks,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("candidate", type=Path, help="candidate .kicad_sch or KiCad XML netlist")
    parser.add_argument("--contract", type=Path, default=DEFAULT_CONTRACT)
    parser.add_argument("--kicad-cli")
    parser.add_argument("--output", type=Path, help="also write JSON to this path")
    args = parser.parse_args()
    try:
        result = validate(args.candidate, args.contract, args.kicad_cli)
    except Exception as exc:
        result = {
            "schema_version": 1,
            "gate": "cadenza_l1_candidate_netlist_contract",
            "status": "ERROR",
            "candidate": str(args.candidate.resolve()),
            "production_ready": False,
            "error": f"{type(exc).__name__}: {exc}",
        }
    rendered = json.dumps(result, ensure_ascii=False, indent=2) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered, encoding="utf-8")
    sys.stdout.write(rendered)
    return 0 if result["status"] == "PASS_CANDIDATE" else 1


if __name__ == "__main__":
    raise SystemExit(main())
