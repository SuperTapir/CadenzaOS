#!/usr/bin/env python3
"""Verify the immutable L1 dry-run baseline before schematic editing.

This intentionally reads existing artifacts only.  Its sole output is the
machine-readable ``verification.json`` beside this script.
"""

from __future__ import annotations

import json
import html
import re
import shutil
import subprocess
import sys
from collections import Counter
from pathlib import Path


HERE = Path(__file__).resolve().parent
DERIVED = HERE.parent
WORKING_BASE = DERIVED.parents[1] / "working-base"
MANIFEST = DERIVED / "SOURCE_MANIFEST.sha256"
SCHEMATIC = DERIVED / "Cadenza-reference-ESP32-S3-game-console-original-v2-20260722.kicad_sch"
PRE_EDIT_NETLIST = HERE / "pre-edit.netlist.xml"
REFERENCE_NETLIST = WORKING_BASE / "connectivity.netlist.xml"
ERC = HERE / "pre-edit.erc.json"
PDF = HERE / "pre-edit.pdf"
OUTPUT = HERE / "verification.json"

EXPECTED_MANIFEST_ENTRIES = 39
EXPECTED_COMPONENTS = 77
EXPECTED_NETS = 102
EXPECTED_ERC_SEVERITIES = {"error": 18, "warning": 673}
EXPECTED_ERC_TYPES = {
    "endpoint_off_grid": 389,
    "footprint_link_issues": 3,
    "lib_symbol_mismatch": 33,
    "pin_not_driven": 14,
    "pin_to_pin": 187,
    "power_pin_not_driven": 4,
    "unconnected_wire_endpoint": 61,
}


def sha256(path: Path) -> str:
    # The host Python bundled on some macOS installations cannot load its
    # OpenSSL hashing module because of local code-signing policy.  Use the OS
    # checksum utility so this evidence gate remains runnable in that state.
    shasum = shutil.which("shasum")
    sha256sum = shutil.which("sha256sum")
    if shasum:
        command = [shasum, "-a", "256", str(path)]
    elif sha256sum:
        command = [sha256sum, str(path)]
    else:
        raise RuntimeError("neither shasum nor sha256sum is available")
    result = subprocess.run(command, capture_output=True, text=True, check=False)
    if result.returncode != 0:
        raise RuntimeError(result.stderr.strip() or f"checksum failed for {path}")
    digest = result.stdout.split(maxsplit=1)[0].lower()
    if len(digest) != 64:
        raise RuntimeError(f"checksum utility returned malformed SHA-256 for {path}")
    return digest


def add_check(checks: list[dict], name: str, passed: bool, **evidence: object) -> None:
    checks.append({"name": name, "pass": bool(passed), **evidence})


def read_manifest() -> tuple[list[tuple[str, Path]], list[str]]:
    entries: list[tuple[str, Path]] = []
    errors: list[str] = []
    if not MANIFEST.is_file():
        return entries, [f"missing manifest: {MANIFEST}"]
    for line_number, raw_line in enumerate(MANIFEST.read_text(encoding="utf-8").splitlines(), 1):
        if not raw_line.strip():
            continue
        parts = raw_line.split(maxsplit=1)
        if len(parts) != 2 or len(parts[0]) != 64:
            errors.append(f"line {line_number}: malformed entry")
            continue
        relative = parts[1].strip()
        if relative.startswith("./"):
            relative = relative[2:]
        candidate = (DERIVED / relative).resolve()
        try:
            candidate.relative_to(DERIVED.resolve())
        except ValueError:
            errors.append(f"line {line_number}: path escapes derived directory")
            continue
        entries.append((parts[0].lower(), candidate))
    return entries, errors


def parse_netlist(path: Path) -> tuple[dict[str, tuple[str, str]], set[frozenset[tuple[str, str]]]]:
    # Keep this parser deliberately narrow: KiCad's XML netlist format is
    # regular here, and avoiding ElementTree lets the gate run even when the
    # host macOS rejects Python's signed pyexpat dynamic library.
    document = path.read_text(encoding="utf-8")
    components_match = re.search(r"<components>(.*?)</components>", document, re.DOTALL)
    nets_match = re.search(r"<nets>(.*?)</nets>", document, re.DOTALL)
    if components_match is None or nets_match is None:
        raise ValueError(f"missing components or nets section in {path}")

    def attr(tag: str, name: str) -> str:
        match = re.search(rf'\b{re.escape(name)}="([^"]*)"', tag)
        if match is None:
            raise ValueError(f"missing {name} attribute in {tag[:80]}")
        return html.unescape(match.group(1))

    def child_text(body: str, name: str) -> str:
        match = re.search(rf"<{re.escape(name)}>(.*?)</{re.escape(name)}>", body, re.DOTALL)
        return html.unescape(match.group(1).strip()) if match else ""

    components: dict[str, tuple[str, str]] = {}
    for match in re.finditer(
        r"<comp\s+([^>]*)>(.*?)</comp>", components_match.group(1), re.DOTALL
    ):
        ref = attr(match.group(1), "ref")
        value = child_text(match.group(2), "value")
        footprint = child_text(match.group(2), "footprint")
        components[ref] = (value, footprint)

    # Net names and numeric codes are export details; electrical semantics are
    # the unordered sets of component-pin endpoints belonging to each net.
    nets: set[frozenset[tuple[str, str]]] = set()
    for match in re.finditer(
        r"<net\s+[^>]*>(.*?)</net>", nets_match.group(1), re.DOTALL
    ):
        endpoints = frozenset(
            (attr(node.group(1), "ref"), attr(node.group(1), "pin"))
            for node in re.finditer(r"<node\s+([^>]*)/>", match.group(1))
        )
        nets.add(endpoints)
    return components, nets


def pdfinfo_page_count(path: Path) -> tuple[int | None, str]:
    executable = shutil.which("pdfinfo")
    if executable is None:
        return None, "pdfinfo unavailable; existence/non-empty check only"
    result = subprocess.run(
        [executable, str(path)], capture_output=True, text=True, check=False
    )
    if result.returncode != 0:
        return -1, result.stderr.strip() or "pdfinfo failed"
    for line in result.stdout.splitlines():
        if line.startswith("Pages:"):
            try:
                return int(line.split(":", 1)[1].strip()), "pdfinfo"
            except ValueError:
                break
    return -1, "pdfinfo did not report a valid page count"


def main() -> int:
    checks: list[dict] = []

    entries, manifest_parse_errors = read_manifest()
    add_check(
        checks,
        "manifest_entry_count",
        not manifest_parse_errors and len(entries) == EXPECTED_MANIFEST_ENTRIES,
        expected=EXPECTED_MANIFEST_ENTRIES,
        actual=len(entries),
        parse_errors=manifest_parse_errors,
    )

    manifest_mismatches: list[dict[str, str]] = []
    for expected, path in entries:
        if not path.is_file():
            manifest_mismatches.append({"path": str(path.relative_to(DERIVED)), "problem": "missing"})
            continue
        actual = sha256(path)
        if actual != expected:
            manifest_mismatches.append(
                {
                    "path": str(path.relative_to(DERIVED)),
                    "problem": "sha256 mismatch",
                    "expected": expected,
                    "actual": actual,
                }
            )
    add_check(
        checks,
        "manifest_files_match",
        len(entries) == EXPECTED_MANIFEST_ENTRIES and not manifest_mismatches,
        checked=len(entries),
        mismatches=manifest_mismatches,
        note="Unlisted files in baseline-proof are permitted and do not alter the source manifest.",
    )

    schematic_manifest_hash = next(
        (expected for expected, path in entries if path == SCHEMATIC.resolve()), None
    )
    schematic_actual_hash = sha256(SCHEMATIC) if SCHEMATIC.is_file() else None
    add_check(
        checks,
        "derived_schematic_matches_manifest",
        schematic_manifest_hash is not None and schematic_actual_hash == schematic_manifest_hash,
        expected=schematic_manifest_hash,
        actual=schematic_actual_hash,
        path=str(SCHEMATIC),
    )

    try:
        pre_components, pre_nets = parse_netlist(PRE_EDIT_NETLIST)
        reference_components, reference_nets = parse_netlist(REFERENCE_NETLIST)
        netlist_error = None
    except (OSError, ValueError, KeyError) as exc:
        pre_components, pre_nets = {}, set()
        reference_components, reference_nets = {}, set()
        netlist_error = str(exc)

    component_differences = {
        ref: {"pre_edit": pre_components.get(ref), "working_base": reference_components.get(ref)}
        for ref in sorted(set(pre_components) | set(reference_components))
        if pre_components.get(ref) != reference_components.get(ref)
    }
    add_check(
        checks,
        "netlist_components_semantically_equal",
        netlist_error is None and not component_differences,
        differences=component_differences,
        parse_error=netlist_error,
    )
    add_check(
        checks,
        "netlist_endpoints_semantically_equal",
        netlist_error is None and pre_nets == reference_nets,
        pre_edit_only_count=len(pre_nets - reference_nets),
        working_base_only_count=len(reference_nets - pre_nets),
        parse_error=netlist_error,
    )
    add_check(
        checks,
        "baseline_counts",
        len(pre_components) == EXPECTED_COMPONENTS and len(pre_nets) == EXPECTED_NETS,
        expected={"components": EXPECTED_COMPONENTS, "nets": EXPECTED_NETS},
        actual={"components": len(pre_components), "nets": len(pre_nets)},
    )

    try:
        erc_document = json.loads(ERC.read_text(encoding="utf-8"))
        violations = [
            violation
            for sheet in erc_document.get("sheets", [])
            for violation in sheet.get("violations", [])
        ]
        erc_error = None
    except (OSError, json.JSONDecodeError) as exc:
        violations = []
        erc_error = str(exc)
    severities = dict(sorted(Counter(v.get("severity") for v in violations).items()))
    types = dict(sorted(Counter(v.get("type") for v in violations).items()))
    add_check(
        checks,
        "erc_exact_distribution",
        erc_error is None
        and len(violations) == 691
        and severities == EXPECTED_ERC_SEVERITIES
        and types == EXPECTED_ERC_TYPES,
        expected={
            "total": 691,
            "severities": EXPECTED_ERC_SEVERITIES,
            "types": EXPECTED_ERC_TYPES,
        },
        actual={"total": len(violations), "severities": severities, "types": types},
        parse_error=erc_error,
    )

    pdf_exists_nonempty = PDF.is_file() and PDF.stat().st_size > 0
    pages, page_source = pdfinfo_page_count(PDF) if pdf_exists_nonempty else (-1, "missing or empty")
    pdf_pass = pdf_exists_nonempty and (pages is None or pages > 0)
    add_check(
        checks,
        "pre_edit_pdf",
        pdf_pass,
        exists=PDF.is_file(),
        bytes=PDF.stat().st_size if PDF.is_file() else 0,
        pages=pages,
        page_check=page_source,
    )

    passed = all(check["pass"] for check in checks)
    result = {
        "schema_version": 1,
        "gate": "l1-schematic-pre-edit-baseline",
        "status": "PASS" if passed else "FAIL",
        "pass": passed,
        "read_only_scope": {
            "manifest": str(MANIFEST),
            "working_base_netlist": str(REFERENCE_NETLIST),
            "note": "The verifier does not invoke KiCad or modify the schematic, working-base, golden files, or OpenSpec.",
        },
        "checks": checks,
    }
    OUTPUT.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"{result['status']}: {sum(c['pass'] for c in checks)}/{len(checks)} checks")
    print(OUTPUT)
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
