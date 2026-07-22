#!/usr/bin/env python3
"""Self-check the screen-only L1 contract against its frozen reference netlist.

PASS proves only that the declarative contract is internally consistent and
partitions the cited baseline.  This script intentionally does not inspect or
approve a candidate schematic or PCB.
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
DEFAULT_CONTRACT = SCRIPT_DIR / "l1-screen-only-delta-contract.json"


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


def load_reference_netlist(path: Path) -> tuple[dict[str, str], dict[str, set[str]]]:
    root = ET.parse(path).getroot()
    components = {
        node.attrib["ref"]: node.findtext("value", default="")
        for node in root.findall("./components/comp")
    }
    nets = {
        node.attrib["name"]: {
            f"{endpoint.attrib['ref']}.{endpoint.attrib['pin']}"
            for endpoint in node.findall("node")
        }
        for node in root.findall("./nets/net")
    }
    return components, nets


def validate(contract: dict[str, Any], contract_path: Path) -> dict[str, Any]:
    checks: dict[str, bool] = {}
    details: dict[str, Any] = {}

    baseline = contract["reference_baseline"]
    schematic_path = resolve_repo_path(baseline["schematic"])
    netlist_path = resolve_repo_path(baseline["netlist"])
    files_exist = schematic_path.is_file() and netlist_path.is_file()
    checks["baseline_files_exist"] = files_exist
    checks["baseline_hashes_match"] = files_exist and (
        sha256(schematic_path) == baseline["schematic_sha256"]
        and sha256(netlist_path) == baseline["netlist_sha256"]
    )

    reference_components, reference_nets = load_reference_netlist(netlist_path)
    reference_refs = set(reference_components)
    reference_net_names = set(reference_nets)
    checks["baseline_counts_match"] = (
        len(reference_refs) == baseline["component_count"] == 77
        and len(reference_net_names) == baseline["net_count"] == 102
    )

    component_policy = contract["reference_components"]
    deleted_rows = component_policy["delete"]
    modified_rows = component_policy["modify"]
    deleted_refs = [row["reference"] for row in deleted_rows]
    modified_refs = [row["reference"] for row in modified_rows]
    retained_refs = component_policy["retain"]
    deleted_set, modified_set, retained_set = map(set, (deleted_refs, modified_refs, retained_refs))
    checks["component_action_lists_unique"] = not any(
        duplicates(values) for values in (deleted_refs, modified_refs, retained_refs)
    )
    checks["component_actions_mutually_exclusive"] = (
        deleted_set.isdisjoint(modified_set)
        and deleted_set.isdisjoint(retained_set)
        and modified_set.isdisjoint(retained_set)
    )
    checks["reference_components_partition_exactly"] = deleted_set | modified_set | retained_set == reference_refs
    checks["component_action_counts_are_5_2_70"] = (
        len(deleted_set), len(modified_set), len(retained_set)
    ) == (5, 2, 70)
    checks["component_actions_are_exact_screen_only_set"] = (
        deleted_set == {"FPC1", "U6", "Q2", "R13", "R14"}
        and modified_set == {"C20", "C21"}
    )
    checks["retained_component_freeze_policy_is_explicit"] = (
        component_policy.get("retain_policy")
        == "all_properties_geometry_and_net_endpoints_must_equal_reference_baseline"
    )
    value_mismatches = [
        {"reference": row["reference"], "contract": row["value"], "baseline": reference_components.get(row["reference"])}
        for row in deleted_rows + modified_rows
        if row["reference"] not in reference_components
        or normalize_value(row["value"]) != normalize_value(reference_components[row["reference"]])
    ]
    checks["changed_component_values_match_baseline"] = not value_mismatches
    details["changed_component_value_mismatches"] = value_mismatches

    expected_cap_changes = {
        "C20": ({"pin": "2", "from_net": "3V3LCD", "to_net": "+5V"}, {"pin": "1", "net": "GND"}),
        "C21": ({"pin": "1", "from_net": "3V3LCD", "to_net": "+5V"}, {"pin": "2", "net": "GND"}),
    }
    required_frozen_fields = {
        "reference", "value", "description", "manufacturer", "manufacturer_part_number",
        "supplier_part_number", "footprint", "board_side",
    }
    cap_rows = {row["reference"]: row for row in modified_rows}
    checks["C20_C21_only_allow_3V3LCD_to_5V_endpoint_change"] = all(
        cap_rows[ref]["only_allowed_change"] == allowed
        and cap_rows[ref]["must_keep_endpoint"] == kept
        for ref, (allowed, kept) in expected_cap_changes.items()
    )
    checks["C20_C21_identity_and_footprint_frozen_placement_display_local"] = all(
        set(cap_rows[ref]["frozen_fields"]) == required_frozen_fields
        and cap_rows[ref].get("placement_policy") == "may_move_only_into_new_display_connector_decoupling_cluster"
        for ref in expected_cap_changes
    )
    checks["C20_C21_baseline_endpoints_match_contract"] = all(
        f"{ref}.{allowed['pin']}" in reference_nets[allowed["from_net"]]
        and f"{ref}.{kept['pin']}" in reference_nets[kept["net"]]
        for ref, (allowed, kept) in expected_cap_changes.items()
    )

    new_components = contract["new_components"]
    new_refs = [row["reference"] for row in new_components]
    new_set = set(new_refs)
    checks["new_components_are_exactly_J_DISP1_and_C_DISP1"] = (
        not duplicates(new_refs) and new_set == {"J_DISP1", "C_DISP1"}
    )
    checks["new_components_do_not_reuse_reference_designators"] = new_set.isdisjoint(reference_refs)
    c_disp = next(row for row in new_components if row["reference"] == "C_DISP1")
    checks["C_DISP1_is_100nF_DISP_to_GND"] = (
        normalize_value(c_disp["value"]) == "100nf"
        and c_disp["connections"] == {"1": "GPIO12", "2": "GND"}
    )

    display_rows = contract["display_connector"]["pin_to_net"]
    expected_sharp_map = {
        1: ("SCLK", "GPIO39"), 2: ("SI", "GPIO48"), 3: ("SCS", "GPIO47"),
        4: ("EXTCOMIN", "GPIO14"), 5: ("DISP", "GPIO12"), 6: ("VDDA", "+5V"),
        7: ("VDD", "+5V"), 8: ("EXTMODE", "+5V"), 9: ("VSS", "GND"), 10: ("VSSA", "GND"),
    }
    display_pins = [row["pin"] for row in display_rows]
    checks["sharp_connector_has_exactly_10_unique_pins"] = (
        sorted(display_pins) == list(range(1, 11)) and not duplicates(display_pins)
    )
    checks["sharp_10pin_mapping_is_exact"] = {
        row["pin"]: (row["name"], row["net"]) for row in display_rows
    } == expected_sharp_map

    net_policy = contract["reference_nets"]
    deleted_net_rows = net_policy["delete"]
    modified_net_rows = net_policy["modify"]
    deleted_nets = [row["name"] for row in deleted_net_rows]
    modified_nets = [row["from"] for row in modified_net_rows]
    retained_nets = net_policy["retain"]
    deleted_net_set, modified_net_set, retained_net_set = map(set, (deleted_nets, modified_nets, retained_nets))
    checks["net_action_lists_unique"] = not any(
        duplicates(values) for values in (deleted_nets, modified_nets, retained_nets)
    )
    checks["net_actions_mutually_exclusive"] = (
        deleted_net_set.isdisjoint(modified_net_set)
        and deleted_net_set.isdisjoint(retained_net_set)
        and modified_net_set.isdisjoint(retained_net_set)
    )
    checks["reference_nets_partition_exactly"] = deleted_net_set | modified_net_set | retained_net_set == reference_net_names
    checks["net_action_counts_are_7_8_87"] = (
        len(deleted_net_set), len(modified_net_set), len(retained_net_set)
    ) == (7, 8, 87)
    details["reference_net_partition_missing"] = sorted(reference_net_names - deleted_net_set - modified_net_set - retained_net_set)
    details["reference_net_partition_extra"] = sorted((deleted_net_set | modified_net_set | retained_net_set) - reference_net_names)
    expected_deleted_nets = {
        "3V3LCD", "/GPIO3", "Net-(Q2-B)", "Net-(Q2-C)",
        "unconnected-(FPC1-Pad6)", "unconnected-(FPC1-Pad13)", "unconnected-(FPC1-Pad14)",
    }
    expected_modified_nets = {"/GPIO48", "/GPIO12", "/GPIO14", "/GPIO47", "/GPIO39", "+5V", "GND", "3V3M"}
    checks["net_actions_are_exact_screen_only_set"] = (
        deleted_net_set == expected_deleted_nets and modified_net_set == expected_modified_nets
    )
    checks["retained_net_endpoint_freeze_policy_is_explicit"] = (
        net_policy.get("retain_policy") == "normalized_endpoint_set_must_equal_reference_baseline"
    )

    merge = next(row for row in deleted_net_rows if row["name"] == "3V3LCD")
    checks["3V3LCD_merge_into_existing_5V_is_explicit_and_bounded"] = (
        merge.get("disposition") == "merge_selected_endpoints_into_existing_net"
        and merge.get("merge_into") == "+5V"
        and set(merge.get("moved_endpoints", [])) == {"C20.2", "C21.1"}
        and set(merge.get("removed_endpoints", [])) == {"FPC1.3", "FPC1.4", "U6.2"}
        and set(merge["moved_endpoints"]) | set(merge["removed_endpoints"]) == reference_nets["3V3LCD"]
    )

    expected_net_deltas = {
        "/GPIO48": ({"FPC1.9"}, {"J_DISP1.2"}),
        "/GPIO12": ({"FPC1.10"}, {"J_DISP1.5", "C_DISP1.1"}),
        "/GPIO14": ({"FPC1.8"}, {"J_DISP1.4"}),
        "/GPIO47": ({"FPC1.7"}, {"J_DISP1.3"}),
        "/GPIO39": ({"R13.1"}, {"J_DISP1.1"}),
        "+5V": (set(), {"C20.2", "C21.1", "J_DISP1.6", "J_DISP1.7", "J_DISP1.8"}),
        "GND": ({"FPC1.1", "FPC1.5", "FPC1.12", "Q2.2", "R14.1", "U6.1"}, {"C_DISP1.2", "J_DISP1.9", "J_DISP1.10"}),
        "3V3M": ({"U6.3"}, set()),
    }
    delta_rows = {row["from"]: row for row in modified_net_rows}
    checks["modified_net_endpoint_deltas_are_exact"] = all(
        set(delta_rows[name]["remove_endpoints"]) == removed
        and set(delta_rows[name]["add_endpoints"]) == added
        for name, (removed, added) in expected_net_deltas.items()
    )
    checks["all_removed_modified_net_endpoints_exist_in_baseline"] = all(
        set(delta_rows[name]["remove_endpoints"]) <= reference_nets[name]
        for name in expected_net_deltas
    )

    frozen = contract["frozen_subsystems"]
    required_frozen = {"usb_and_download", "buttons", "power_control"}
    checks["USB_buttons_and_power_control_freezes_declared"] = required_frozen == set(frozen)
    checks["frozen_subsystem_policy_is_exact"] = all(
        subsystem.get("policy")
        == "all_component_properties_geometry_and_net_endpoints_must_equal_reference_baseline"
        for subsystem in frozen.values()
    )
    frozen_refs = {ref for subsystem in frozen.values() for ref in subsystem["components"]}
    frozen_nets = {net for subsystem in frozen.values() for net in subsystem["nets"]}
    checks["frozen_subsystem_components_are_retained"] = frozen_refs <= retained_set
    checks["frozen_subsystem_nets_are_retained"] = frozen_nets <= retained_net_set
    required_protected_nets = {
        "/D+", "/D-", "/DTR", "/RTS", "/U0RXD", "/U0TXD", "/GPIO6", "/GPIO18",
        "/VOUT", "unconnected-(U1-KEY-Pad5)",
    }
    checks["critical_USB_button_power_control_nets_are_frozen"] = required_protected_nets <= frozen_nets

    forbidden = contract["forbidden_additions"]
    forbidden_exact = set(forbidden["exact_references"])
    forbidden_prefixes = tuple(forbidden["reference_prefixes"])
    checks["no_PowerLock_or_test_point_references_added"] = (
        new_set.isdisjoint(forbidden_exact)
        and not any(ref.startswith(forbidden_prefixes) for ref in new_set)
        and {"PowerLock", "test_point"} <= set(forbidden["categories"])
    )
    checks["PowerLock_is_explicitly_absent"] = contract.get("power_lock") == {
        "included": False, "policy": "forbidden_in_screen_only_L1"
    }
    checks["test_point_list_is_explicitly_empty"] = contract.get("test_points") == []
    checks["proof_boundary_is_contract_only"] = (
        contract["proof_boundary"]["proves"]
        == "This JSON is internally consistent and exactly partitions the cited frozen 77-component/102-net baseline."
        and {"candidate_schematic_matches_contract", "screen_operation", "fabrication_readiness"}
        <= set(contract["proof_boundary"]["does_not_prove"])
    )

    failed = sorted(name for name, passed in checks.items() if not passed)
    return {
        "schema_version": 1,
        "contract": str(contract_path.resolve()),
        "baseline_netlist": str(netlist_path.resolve()),
        "summary": {
            "checks": len(checks),
            "passed": len(checks) - len(failed),
            "failed": len(failed),
            "reference_components": len(reference_refs),
            "delete_components": len(deleted_set),
            "modify_components": len(modified_set),
            "retain_components": len(retained_set),
            "new_components": len(new_set),
            "reference_nets": len(reference_net_names),
            "delete_nets": len(deleted_net_set),
            "modify_nets": len(modified_net_set),
            "retain_nets": len(retained_net_set),
        },
        "checks": checks,
        "failed_checks": failed,
        "details": details,
        "verdict": "PASS" if not failed else "FAIL",
        "scope_note": "PASS proves contract self-consistency and exact frozen-baseline partition only; it does not validate any candidate schematic/PCB, FPC direction, hardware behavior, or fabrication readiness.",
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
