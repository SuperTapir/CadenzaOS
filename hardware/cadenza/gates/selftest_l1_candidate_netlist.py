#!/usr/bin/env python3
"""Build synthetic XML fixtures and self-test the L1 candidate gate."""

from __future__ import annotations

import copy
import json
import sys
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path
from typing import Any


HERE = Path(__file__).resolve().parent
FIXTURES = HERE / "l1-candidate-netlist-fixtures"
sys.path.insert(0, str(HERE))

from verify_l1_candidate_netlist import (  # noqa: E402
    DEFAULT_CONTRACT,
    parse_xml,
    repo_path,
    resolve_pin,
    validate,
)


def set_comp(root: ET.Element, ref: str, value: str, footprint: str = "", lib_id: str = "") -> None:
    components = root.find("./components")
    assert components is not None
    comp = ET.SubElement(components, "comp", {"ref": ref})
    ET.SubElement(comp, "value").text = value
    ET.SubElement(comp, "footprint").text = footprint
    if lib_id:
        lib, part = lib_id.split(":", 1)
        ET.SubElement(comp, "libsource", {"lib": lib, "part": part, "description": ""})


def endpoints_from_root(root: ET.Element) -> dict[tuple[str, str], dict[str, str]]:
    result: dict[tuple[str, str], dict[str, str]] = {}
    for net in root.findall("./nets/net"):
        for node in net.findall("node"):
            attrs = dict(node.attrib)
            attrs["net"] = net.attrib["name"]
            result[(attrs["ref"], attrs["pin"])] = attrs
    return result


def write_endpoints(root: ET.Element, endpoints: dict[tuple[str, str], dict[str, str]]) -> None:
    old = root.find("./nets")
    assert old is not None
    root.remove(old)
    nets = ET.SubElement(root, "nets")
    grouped: dict[str, list[dict[str, str]]] = {}
    for attrs in endpoints.values():
        grouped.setdefault(attrs["net"], []).append(attrs)
    for code, name in enumerate(sorted(grouped), start=1):
        net = ET.SubElement(nets, "net", {"code": str(code), "name": name})
        for attrs in sorted(grouped[name], key=lambda row: (row["ref"], row["pin"])):
            ET.SubElement(net, "node", {key: value for key, value in attrs.items() if key != "net"})


def add_pin(endpoints: dict[tuple[str, str], dict[str, str]], ref: str, pin: str, net: str, function: str | None = None) -> None:
    previous = endpoints.get((ref, pin), {})
    endpoints[(ref, pin)] = {
        "ref": ref,
        "pin": pin,
        "pinfunction": function if function is not None else (previous.get("pinfunction") or f"{pin}_{pin}"),
        "pintype": previous.get("pintype", "passive"),
        "net": net,
    }


def build_valid(contract: dict[str, Any]) -> ET.ElementTree:
    baseline_path = repo_path(contract["reference_baseline"]["netlist"])
    baseline_tree = ET.parse(baseline_path)
    root = copy.deepcopy(baseline_tree.getroot())
    baseline = parse_xml(baseline_path)

    deleted = {row["reference"] for row in contract["reference_components"]["delete"]}
    components = root.find("./components")
    assert components is not None
    for comp in list(components.findall("comp")):
        if comp.attrib["ref"] in deleted:
            components.remove(comp)

    by_ref = {comp.attrib["ref"]: comp for comp in components.findall("comp")}
    for repair in contract["reference_components"].get("allowed_metadata_repairs", []):
        footprint = by_ref[repair["reference"]].find("footprint")
        if footprint is None:
            footprint = ET.SubElement(by_ref[repair["reference"]], "footprint")
        footprint.text = repair["to"]

    deleted_nets = set(contract["reference_nets"]["delete"])
    net_map = {row["from"]: row["to"] for row in contract["reference_nets"]["modify"]}
    endpoints = endpoints_from_root(root)
    for key, attrs in list(endpoints.items()):
        ref, pin = key
        if ref in deleted or attrs["net"] in deleted_nets or (ref == "SW2" and pin == "5"):
            del endpoints[key]
            continue
        attrs["net"] = net_map.get(attrs["net"], attrs["net"])

    for row in contract["new_components"]:
        lib_ids = {
            "D_PWR1": "Diode:BAT54C",
            "Q_PWR1": "Transistor_FET:AO3400A",
        }
        set_comp(
            root,
            row["reference"],
            row["value"],
            row.get("footprint") or "",
            lib_ids.get(row["reference"], ""),
        )

    for row in contract["display_connector"]["pin_to_net"]:
        add_pin(endpoints, "J_DISP1", str(row["pin"]), row["net"], row["name"])

    for row in contract["decoupling"]:
        add_pin(endpoints, row["reference"], "1", row["between"][0])
        add_pin(endpoints, row["reference"], "2", row["between"][1])

    # A temporary candidate shell is sufficient to resolve U4.GPIO18/GPIO6
    # because all original U4 pinfunction metadata remains in endpoints.
    write_endpoints(root, endpoints)
    with tempfile.TemporaryDirectory(prefix="cadenza-l1-fixture-resolve-") as temp:
        partial = Path(temp) / "partial.xml"
        ET.ElementTree(root).write(partial, encoding="utf-8", xml_declaration=True)
        candidate_partial = parse_xml(partial)
        for row in contract["power_lock"]["required_connections"]:
            ref, pin = resolve_pin(row["component_pin"], candidate_partial, baseline)
            invariant_functions = {
                # KiCad 10's standard Diode:BAT54C exports blank pinfunction
                # labels; exercise the documented exact-libsource exemption.
                "D_PWR1": {"1": "", "2": "", "3": ""},
                "Q_PWR1": contract["power_lock"]["pinout_invariants"]["Q_PWR1_AO3400A"],
            }
            add_pin(endpoints, ref, pin, row["net"], invariant_functions.get(ref, {}).get(pin))

    for row in contract["test_points"]:
        add_pin(endpoints, row["reference"], "1", row["net"])
    write_endpoints(root, endpoints)
    return ET.ElementTree(root)


def mutate(tree: ET.ElementTree, mutation: dict[str, Any] | None) -> None:
    if not mutation:
        return
    root = tree.getroot()
    kind = mutation["kind"]
    if kind == "set_footprint":
        comp = next(node for node in root.findall("./components/comp") if node.attrib["ref"] == mutation["reference"])
        footprint = comp.find("footprint")
        if footprint is None:
            footprint = ET.SubElement(comp, "footprint")
        footprint.text = mutation["value"]
        return
    if kind == "set_value":
        comp = next(node for node in root.findall("./components/comp") if node.attrib["ref"] == mutation["reference"])
        value = comp.find("value")
        assert value is not None
        value.text = mutation["value"]
        return
    if kind == "set_values":
        by_ref = {node.attrib["ref"]: node for node in root.findall("./components/comp")}
        for ref, replacement in mutation["values"].items():
            value = by_ref[ref].find("value")
            assert value is not None
            value.text = replacement
        return
    if kind == "set_libsource":
        comp = next(node for node in root.findall("./components/comp") if node.attrib["ref"] == mutation["reference"])
        source = comp.find("libsource")
        if source is None:
            source = ET.SubElement(comp, "libsource")
        lib, part = mutation["value"].split(":", 1)
        source.attrib.update({"lib": lib, "part": part, "description": ""})
        return
    endpoints = endpoints_from_root(root)
    if kind == "swap_pins":
        ref = mutation["reference"]
        first, second = (str(pin) for pin in mutation["pins"])
        endpoints[(ref, first)]["net"], endpoints[(ref, second)]["net"] = (
            endpoints[(ref, second)]["net"], endpoints[(ref, first)]["net"]
        )
    elif kind == "set_pin_net":
        endpoints[(mutation["reference"], str(mutation["pin"]))]["net"] = mutation["net"]
    elif kind == "set_pinfunction":
        endpoints[(mutation["reference"], str(mutation["pin"]))]["pinfunction"] = mutation["value"]
    else:
        raise ValueError(f"unknown fixture mutation: {kind}")
    write_endpoints(root, endpoints)


def main() -> int:
    contract = json.loads(DEFAULT_CONTRACT.read_text(encoding="utf-8"))
    fixture_paths = sorted(FIXTURES.glob("*.json"))
    results = []
    with tempfile.TemporaryDirectory(prefix="cadenza-l1-gate-selftest-") as temp:
        for fixture_path in fixture_paths:
            fixture = json.loads(fixture_path.read_text(encoding="utf-8"))
            tree = build_valid(contract)
            mutate(tree, fixture.get("mutation"))
            xml = Path(temp) / f"{fixture['fixture']}.xml"
            tree.write(xml, encoding="utf-8", xml_declaration=True)
            result = validate(xml, DEFAULT_CONTRACT)
            expected = fixture["expected_status"]
            expected_failed_check = {
                "sharp-pin-swap": "sharp_pin_1_to_10_exact",
                "j-disp-footprint-selected": "j_disp1_footprint_empty",
                "protected-endpoint-drift": "sixty_seven_retain_endpoints_preserved",
                "r-pwr3-wrong-value": "new_component_values_match_contract",
                "bat54c-pinfunction-swap": "power_lock_pinfunction_invariants",
                "bat54c-wrong-libsource": "power_lock_pinfunction_invariants",
                "q-pwr1-wrong-libsource": "power_lock_pinfunction_invariants",
                "retained-footprint-drift": "retained_and_modified_value_footprint_preserved",
                "testpoint-footprint-drift": "declared_pcb_only_footprints_exact",
            }.get(fixture["fixture"])
            failed_ids = {row["id"] for row in result["checks"] if row["status"] == "FAIL"}
            exemption_required = fixture["fixture"] in {"valid", "equivalent-values"}
            exemption_present = any(
                row.get("reference") == "D_PWR1" and row.get("lib_id") == "Diode:BAT54C"
                for row in result.get("details", {}).get("pinfunction_exemptions", [])
            )
            passed = (
                result["status"] == expected
                and (expected_failed_check is None or expected_failed_check in failed_ids)
                and (not exemption_required or exemption_present)
            )
            results.append({
                "fixture": fixture["fixture"],
                "expected": expected,
                "actual": result["status"],
                "required_failed_check": expected_failed_check,
                "failed_checks": sorted(failed_ids),
                "required_bat54c_blank_pinfunction_exemption_present": exemption_required,
                "bat54c_blank_pinfunction_exemption_present": exemption_present,
                "status": "PASS" if passed else "FAIL",
            })
    failures = [row for row in results if row["status"] == "FAIL"]
    output = {
        "schema_version": 1,
        "suite": "cadenza_l1_candidate_netlist_gate_selftest",
        "status": "PASS" if not failures else "FAIL",
        "fixture_evidence_only": True,
        "real_candidate_validated": False,
        "summary": {"total": len(results), "passed": len(results) - len(failures), "failed": len(failures)},
        "results": results,
    }
    print(json.dumps(output, ensure_ascii=False, indent=2))
    return 0 if not failures else 1


if __name__ == "__main__":
    raise SystemExit(main())
