#!/usr/bin/env python3
"""Validate the declarative L1 schematic delta contract against the frozen netlist.

This does not edit or validate a candidate KiCad schematic.  It proves that the
contract itself is internally consistent and still partitions the cited frozen
reference baseline exactly.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path
from typing import Any


SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parents[2]
DEFAULT_CONTRACT = SCRIPT_DIR / "l1-schematic-delta-contract.json"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def resolve_repo_path(value: str) -> Path:
    path = Path(value)
    return path if path.is_absolute() else REPO_ROOT / path


def normalize_value(value: str) -> str:
    return re.sub(r"[^a-z0-9.+-]", "", value.replace("Ω", "ohm").lower())


def duplicates(values: list[Any]) -> list[Any]:
    seen: set[Any] = set()
    repeated: set[Any] = set()
    for value in values:
        if value in seen:
            repeated.add(value)
        seen.add(value)
    return sorted(repeated)


def load_reference_netlist(path: Path) -> tuple[dict[str, str], set[str]]:
    root = ET.parse(path).getroot()
    components = {
        node.attrib["ref"]: node.findtext("value", default="")
        for node in root.findall("./components/comp")
    }
    nets = {node.attrib["name"] for node in root.findall("./nets/net")}
    return components, nets


def validate(contract: dict[str, Any], contract_path: Path) -> dict[str, Any]:
    checks: dict[str, bool] = {}
    details: dict[str, Any] = {}

    baseline = contract["reference_baseline"]
    schematic_path = resolve_repo_path(baseline["schematic"])
    netlist_path = resolve_repo_path(baseline["netlist"])
    reference_components, reference_nets = load_reference_netlist(netlist_path)

    checks["baseline_files_exist"] = schematic_path.is_file() and netlist_path.is_file()
    checks["baseline_hashes_match"] = (
        sha256(schematic_path) == baseline["schematic_sha256"]
        and sha256(netlist_path) == baseline["netlist_sha256"]
    )
    checks["baseline_counts_match"] = (
        len(reference_components) == baseline["component_count"] == 77
        and len(reference_nets) == baseline["net_count"] == 102
    )

    component_policy = contract["reference_components"]
    deleted_rows = component_policy["delete"]
    modified_rows = component_policy["modify"]
    deleted_refs = [row["reference"] for row in deleted_rows]
    modified_refs = [row["reference"] for row in modified_rows]
    retained_refs = component_policy["retain"]
    deleted_set = set(deleted_refs)
    modified_set = set(modified_refs)
    retained_set = set(retained_refs)
    reference_ref_set = set(reference_components)
    checks["reference_component_lists_unique"] = (
        not duplicates(deleted_refs)
        and not duplicates(modified_refs)
        and not duplicates(retained_refs)
    )
    checks["delete_modify_retain_components_mutually_exclusive"] = (
        deleted_set.isdisjoint(modified_set)
        and deleted_set.isdisjoint(retained_set)
        and modified_set.isdisjoint(retained_set)
    )
    checks["reference_components_partition_exactly"] = (
        deleted_set | modified_set | retained_set == reference_ref_set
    )
    value_mismatches = [
        {
            "reference": row["reference"],
            "contract": row["value"],
            "baseline": reference_components.get(row["reference"]),
        }
        for row in deleted_rows
        if row["reference"] not in reference_components
        or normalize_value(row["value"]) != normalize_value(reference_components[row["reference"]])
    ]
    checks["deleted_component_values_match_baseline"] = not value_mismatches
    details["deleted_component_value_mismatches"] = value_mismatches
    modified_value_mismatches = [
        {
            "reference": row["reference"],
            "contract": row["value"],
            "baseline": reference_components.get(row["reference"]),
        }
        for row in modified_rows
        if row["reference"] not in reference_components
        or normalize_value(row["value"]) != normalize_value(reference_components[row["reference"]])
    ]
    checks["modified_component_values_match_baseline"] = not modified_value_mismatches
    details["modified_component_value_mismatches"] = modified_value_mismatches

    subsystem_refs = {
        ref
        for refs in component_policy["must_preserve_subsystems"].values()
        for ref in refs
    }
    checks["preserved_subsystems_only_use_retained_refs"] = subsystem_refs <= retained_set
    required_subsystem_names = {
        "battery_charge_boost_except_SW1_break",
        "3v3_regulator",
        "esp32_minimum_system",
        "l1_usb_uart_download",
        "audio",
        "mic_reference_only",
        "microsd",
        "mechanical_datums",
    }
    checks["mandatory_preserved_subsystems_declared"] = required_subsystem_names <= set(
        component_policy["must_preserve_subsystems"]
    )

    new_components = contract["new_components"]
    new_refs = [row["reference"] for row in new_components]
    new_ref_set = set(new_refs)
    checks["new_component_references_unique"] = not duplicates(new_refs)
    checks["new_components_do_not_reuse_reference_designators"] = new_ref_set.isdisjoint(reference_ref_set)

    net_policy = contract["reference_nets"]
    deleted_nets = net_policy["delete"]
    modified_from = [row["from"] for row in net_policy["modify"]]
    retained_nets = net_policy["retain"]
    deleted_net_set = set(deleted_nets)
    modified_from_set = set(modified_from)
    retained_net_set = set(retained_nets)
    checks["reference_net_action_lists_unique"] = (
        not duplicates(deleted_nets)
        and not duplicates(modified_from)
        and not duplicates(retained_nets)
    )
    checks["reference_net_actions_mutually_exclusive"] = (
        deleted_net_set.isdisjoint(modified_from_set)
        and deleted_net_set.isdisjoint(retained_net_set)
        and modified_from_set.isdisjoint(retained_net_set)
    )
    checks["reference_nets_partition_exactly"] = (
        deleted_net_set | modified_from_set | retained_net_set == reference_nets
    )
    details["reference_net_partition_missing"] = sorted(
        reference_nets - deleted_net_set - modified_from_set - retained_net_set
    )
    details["reference_net_partition_extra"] = sorted(
        (deleted_net_set | modified_from_set | retained_net_set) - reference_nets
    )

    target_domains = contract["target_net_domains"]
    target_net_names = [row["net"] for row in target_domains]
    target_net_set = set(target_net_names)
    checks["target_net_names_unique"] = not duplicates(target_net_names)
    checks["target_net_domains_nonempty"] = all(row.get("domain") for row in target_domains)

    gpio_rows = contract["gpio_assignments"]
    gpios = [row["gpio"] for row in gpio_rows]
    gpio_nets = [row["net"] for row in gpio_rows]
    gpio_functions = [row["function"] for row in gpio_rows]
    constraints = contract["gpio_constraints"]
    prohibited_gpios = {
        gpio
        for key, values in constraints.items()
        if key != "rgb_reference_reserved"
        for gpio in values
    } | set(constraints["rgb_reference_reserved"])
    checks["gpio_numbers_unique"] = not duplicates(gpios)
    checks["gpio_functions_unique"] = not duplicates(gpio_functions)
    checks["gpio_nets_unique"] = not duplicates(gpio_nets)
    checks["gpio_assignments_avoid_reserved_and_unavailable"] = set(gpios).isdisjoint(prohibited_gpios)
    checks["gpio_nets_are_declared_target_nets"] = set(gpio_nets) <= target_net_set
    release_errors = []
    for row in gpio_rows:
        release_reference = row.get("release_reference")
        if not release_reference:
            continue
        release_action = row.get("release_reference_action")
        valid = (
            (release_action == "delete" and release_reference in deleted_set)
            or (release_action == "modify" and release_reference in modified_set)
        )
        if not valid:
            release_errors.append(
                {
                    "function": row["function"],
                    "release_reference": release_reference,
                    "release_reference_action": release_action,
                }
            )
    checks["gpio_reference_conflicts_have_delete_actions"] = not release_errors
    details["gpio_release_errors"] = release_errors
    expected_gpio_map = {
        "DISPLAY_SCLK": 48,
        "DISPLAY_SI": 12,
        "DISPLAY_SCS": 14,
        "DISPLAY_EXTCOMIN": 47,
        "DISPLAY_DISP": 39,
        "PWR_KEY_SENSE": 18,
        "PWR_FORCE_OFF": 6,
    }
    checks["gpio_function_map_matches_candidate_allocation"] = {
        row["function"]: row["gpio"] for row in gpio_rows
    } == expected_gpio_map

    display = contract["display_connector"]
    display_rows = display["pin_to_net"]
    display_pins = [row["pin"] for row in display_rows]
    display_names = [row["name"] for row in display_rows]
    checks["sharp_connector_has_exactly_one_definition_per_pin"] = (
        sorted(display_pins) == list(range(1, 11)) and not duplicates(display_pins)
    )
    checks["sharp_signal_names_unique"] = not duplicates(display_names)
    expected_sharp_map = {
        1: ("SCLK", "GPIO48"),
        2: ("SI", "GPIO12"),
        3: ("SCS", "GPIO14"),
        4: ("EXTCOMIN", "GPIO47"),
        5: ("DISP", "GPIO39"),
        6: ("VDDA", "+5V"),
        7: ("VDD", "+5V"),
        8: ("EXTMODE", "+5V"),
        9: ("VSS", "GND"),
        10: ("VSSA", "GND"),
    }
    checks["sharp_pin_to_net_map_exact"] = {
        row["pin"]: (row["name"], row["net"]) for row in display_rows
    } == expected_sharp_map
    checks["fpc_footprint_and_direction_remain_unfrozen"] = (
        contract["decisions"]["fpc_footprint_selected"] is False
        and display["footprint_policy"]["selected"] is False
        and next(row for row in new_components if row["reference"] == "J_DISP1")["footprint"] is None
    )

    voltage = contract["voltage_domains"]
    actual_display_power = {
        str(row["pin"]): row["net"] for row in display_rows if row["pin"] >= 6
    }
    actual_display_logic = {
        str(row["pin"]): row["net"] for row in display_rows if row["pin"] <= 5
    }
    checks["display_5v_and_ground_pins_match_domain_rule"] = actual_display_power == voltage["display_power_pins"]
    checks["display_logic_pins_match_domain_rule"] = actual_display_logic == voltage["display_logic_pins"]
    checks["no_gpio_is_assigned_to_a_power_rail"] = set(gpio_nets).isdisjoint({"+5V", "3V3M", "GND", "/VOUT"})
    checks["no_display_supply_pin_uses_3v3"] = all(
        row["net"] != "3V3M" for row in display_rows if row["pin"] in {6, 7, 8}
    )

    decoupling = contract["decoupling"]
    decoupling_refs = {row["reference"] for row in decoupling}
    checks["all_three_required_display_capacitors_declared"] = decoupling_refs == {
        "C_DISP1", "C_DISP2", "C_DISP3"
    }
    checks["display_capacitors_have_required_values"] = (
        next(row for row in decoupling if row["reference"] == "C_DISP1").get("value_uf") == 0.1
        and next(row for row in decoupling if row["reference"] == "C_DISP2").get("minimum_value_uf") == 0.1
        and next(row for row in decoupling if row["reference"] == "C_DISP3").get("minimum_value_uf") == 1.0
    )

    test_points = contract["test_points"]
    test_point_refs = [row["reference"] for row in test_points]
    test_point_nets = {row["net"] for row in test_points}
    checks["test_point_references_unique_and_declared"] = (
        not duplicates(test_point_refs) and set(test_point_refs) <= new_ref_set
    )
    checks["test_point_nets_are_declared"] = test_point_nets <= target_net_set
    required_test_nets = {
        "+5V", "GND", "GPIO48", "GPIO12", "GPIO14", "GPIO47", "GPIO39",
        "PWR_IP5306_KEY", "PWR_BUTTON_NODE", "PWR_KEY_SENSE", "PWR_FORCE_OFF", "/VOUT",
    }
    checks["required_display_and_power_test_nets_covered"] = required_test_nets <= test_point_nets

    power = contract["power_lock"]
    power_connections = power["required_connections"]
    power_pins = [row["component_pin"] for row in power_connections]
    power_nets = {row["net"] for row in power_connections}
    checks["power_lock_component_pins_have_unique_net_assignments"] = not duplicates(power_pins)
    checks["power_lock_nets_are_declared"] = power_nets <= target_net_set
    expected_power_gpio_connections = {
        "U4.GPIO18": "PWR_KEY_SENSE",
        "U4.GPIO6": "PWR_FORCE_OFF",
    }
    checks["power_lock_gpio_connections_exact"] = {
        row["component_pin"]: row["net"]
        for row in power_connections
        if row["component_pin"].startswith("U4.GPIO")
    } == expected_power_gpio_connections
    checks["power_lock_isolation_prevents_direct_key_to_gpio"] = not any(
        row["net"] in {"PWR_IP5306_KEY", "PWR_BUTTON_NODE"}
        for row in power_connections
        if row["component_pin"].startswith("U4.GPIO")
    )
    checks["power_lock_rescue_bypass_is_dnp"] = power["default_population"] == {
        "D_PWR1": "POPULATE", "R_PWR5": "DNP"
    }

    usb_preserve_refs = {"USB1", "U3", "Q1", "R1", "R2"}
    usb_preserve_nets = {"/D+", "/D-", "/DTR", "/RTS", "/U0RXD", "/U0TXD"}
    checks["l1_ch340c_components_retained"] = usb_preserve_refs <= retained_set
    checks["l1_ch340c_nets_retained"] = usb_preserve_nets <= retained_net_set
    checks["native_usb_deferred_not_implemented"] = (
        contract["decisions"]["l1_usb"] == "retain_reference_CH340C_route"
        and contract["decisions"]["native_usb"] == "deferred_to_L2"
    )
    sw2 = next((row for row in modified_rows if row["reference"] == "SW2"), None)
    checks["sw2_partial_l1_change_preserves_gpio7_19_20"] = (
        sw2 is not None
        and sw2["must_keep"].get("pad_1") == "/GPIO7"
        and sw2["must_keep"].get("pad_2") == "/GPIO19"
        and sw2["must_keep"].get("pad_3") == "/GPIO20"
        and {"/GPIO7", "/GPIO19", "/GPIO20"} <= retained_net_set
        and sw2["must_change"].get("pad_5") == "no_connect_or_explicitly_isolated_from_/GPIO6"
    )

    boundary_text = set(contract["forbidden_boundaries"])
    checks["forbidden_boundaries_cover_source_fpc_usb_and_openspec"] = {
        "do_not_edit_golden_import_or_working_base",
        "do_not_parallel_CH340C_USB_PHY_with_ESP32S3_native_USB_PHY",
        "do_not_assign_FPC_footprint_contact_face_or_pin1_before_physical_gate",
        "do_not_mark_OpenSpec_tasks_complete_from_this_contract",
    } <= boundary_text

    failed = sorted(name for name, passed in checks.items() if not passed)
    return {
        "schema_version": 1,
        "contract": str(contract_path.resolve()),
        "baseline_netlist": str(netlist_path.resolve()),
        "summary": {
            "checks": len(checks),
            "passed": len(checks) - len(failed),
            "failed": len(failed),
            "reference_components": len(reference_components),
            "delete_components": len(deleted_set),
            "modify_components": len(modified_set),
            "retain_components": len(retained_set),
            "new_components": len(new_ref_set),
            "reference_nets": len(reference_nets),
            "delete_nets": len(deleted_net_set),
            "modify_nets": len(modified_from_set),
            "retain_nets": len(retained_net_set),
        },
        "checks": checks,
        "failed_checks": failed,
        "details": details,
        "verdict": "PASS" if not failed else "FAIL",
        "scope_note": "PASS validates the contract and frozen-baseline partition only; it does not validate a modified KiCad schematic, FPC direction, hardware behavior, PCB layout, or fabrication readiness.",
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--contract", type=Path, default=DEFAULT_CONTRACT)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    contract = json.loads(args.contract.read_text(encoding="utf-8"))
    result = validate(contract, args.contract)
    rendered = json.dumps(result, ensure_ascii=False, indent=2) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered, encoding="utf-8")
    sys.stdout.write(rendered)
    return 0 if result["verdict"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
