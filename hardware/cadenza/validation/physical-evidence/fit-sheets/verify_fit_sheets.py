#!/usr/bin/env python3
"""Verify Cadenza fit sheets against their KiCad footprint sources."""

from __future__ import annotations

import json
import math
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

from generate_fit_sheets import (
    ACTUAL_CENTER,
    FIT_DIR,
    PAGE_H_MM,
    PAGE_W_MM,
    PDF_PATH,
    ROOT,
    load_footprints,
    sha256,
)

TOL = 0.0002
SVG_NS = {"svg": "http://www.w3.org/2000/svg"}


def close(actual: float, expected: float, label: str, failures: list[str], tol: float = TOL) -> None:
    if not math.isclose(actual, expected, rel_tol=0.0, abs_tol=tol):
        failures.append(f"{label}: expected {expected}, got {actual}")


def find_id(root: ET.Element, element_id: str) -> ET.Element | None:
    for element in root.iter():
        if element.attrib.get("id") == element_id:
            return element
    return None


def verify_svg(path: Path, fp, failures: list[str]) -> dict:
    root = ET.parse(path).getroot()
    if root.attrib.get("width") != "210mm" or root.attrib.get("height") != "297mm":
        failures.append(f"{path.name}: SVG page is not explicit 210mm x 297mm")
    if root.attrib.get("viewBox") != "0 0 210 297":
        failures.append(f"{path.name}: incorrect A4 viewBox")
    if root.attrib.get("data-print-scale") != "1.0":
        failures.append(f"{path.name}: missing 1.0 print scale marker")

    text = " ".join((node.text or "") for node in root.findall(".//svg:text", SVG_NS))
    for required in (
        "PRINT AT 100%", "DISABLE FIT TO PAGE", "50 mm ruler", "10 mm ruler",
        "NOT FROZEN", "cannot prove LCD-FPC Pin 1", fp.mpn,
    ):
        if required not in text:
            failures.append(f"{path.name}: missing warning/text {required!r}")

    cal50 = find_id(root, "calibration-50mm")
    cal10 = find_id(root, "calibration-10mm")
    box = find_id(root, "calibration-box-50x10")
    if cal50 is None or cal10 is None or box is None:
        failures.append(f"{path.name}: missing calibration geometry")
    else:
        close(float(cal50.attrib["x2"]) - float(cal50.attrib["x1"]), 50.0,
              f"{path.name}: 50mm ruler", failures)
        close(float(cal10.attrib["x2"]) - float(cal10.attrib["x1"]), 10.0,
              f"{path.name}: 10mm ruler", failures)
        close(float(box.attrib["width"]), 50.0, f"{path.name}: box width", failures)
        close(float(box.attrib["height"]), 10.0, f"{path.name}: box height", failures)

    body = find_id(root, "actual-body")
    if body is None:
        failures.append(f"{path.name}: missing actual-body")
    else:
        x1, y1, x2, y2 = fp.body
        close(float(body.attrib["x"]), ACTUAL_CENTER[0] + x1,
              f"{path.name}: body x", failures)
        close(float(body.attrib["y"]), ACTUAL_CENTER[1] + y1,
              f"{path.name}: body y", failures)
        close(float(body.attrib["width"]), x2 - x1,
              f"{path.name}: body width", failures)
        close(float(body.attrib["height"]), y2 - y1,
              f"{path.name}: body height", failures)

    actual_pad_data = []
    for element in root.findall(".//svg:rect", SVG_NS):
        if element.attrib.get("id", "").startswith("actual-pad-"):
            actual_pad_data.append((
                element.attrib["data-number"],
                float(element.attrib["data-local-x"]),
                float(element.attrib["data-local-y"]),
                float(element.attrib["data-local-width"]),
                float(element.attrib["data-local-height"]),
            ))
    source_pad_data = [(p.number, p.x, p.y, p.width, p.height) for p in fp.pads]
    if sorted(actual_pad_data) != sorted(source_pad_data):
        failures.append(f"{path.name}: SVG actual-pad geometry differs from source footprint")
    if len(actual_pad_data) != 12:
        failures.append(f"{path.name}: expected 12 actual pads, got {len(actual_pad_data)}")
    return {"path": path.relative_to(ROOT).as_posix(), "pads_checked": len(actual_pad_data)}


def verify_record_svg(path: Path, failures: list[str]) -> dict:
    root = ET.parse(path).getroot()
    if root.attrib.get("width") != "210mm" or root.attrib.get("height") != "297mm":
        failures.append("caliper record SVG is not A4 in millimetres")
    text = " ".join((node.text or "") for node in root.findall(".//svg:text", SVG_NS))
    for required in (
        "EC12 ROTARY ENCODER", "6x6 TACT SWITCH", "10P FPC-TO-2.54 ADAPTER",
        "DISABLE FIT TO PAGE", "Measure the 10 mm and 50 mm rulers", "NOT FROZEN",
    ):
        if required not in text:
            failures.append(f"caliper record SVG missing {required!r}")
    return {"path": path.relative_to(ROOT).as_posix(), "record_sections_checked": 3}


def verify_pdf(path: Path, footprints, failures: list[str]) -> dict:
    try:
        from pypdf import PdfReader
    except ImportError as exc:
        failures.append(f"pypdf unavailable: {exc}")
        return {"path": path.relative_to(ROOT).as_posix(), "pages_checked": 0}

    reader = PdfReader(str(path))
    if len(reader.pages) != 4:
        failures.append(f"PDF expected 4 pages, got {len(reader.pages)}")
    page_details = []
    for index, page in enumerate(reader.pages):
        width_mm = float(page.mediabox.width) / 72.0 * 25.4
        height_mm = float(page.mediabox.height) / 72.0 * 25.4
        close(width_mm, PAGE_W_MM, f"PDF page {index + 1} width", failures, tol=0.01)
        close(height_mm, PAGE_H_MM, f"PDF page {index + 1} height", failures, tol=0.01)
        text = page.extract_text() or ""
        for required in ("PRINT AT 100%", "DISABLE FIT TO PAGE", "NOT FROZEN"):
            if required not in text:
                failures.append(f"PDF page {index + 1} missing {required!r}")
        page_details.append({"page": index + 1, "width_mm": width_mm, "height_mm": height_mm})
    for index, fp in enumerate(footprints):
        text = reader.pages[index].extract_text() or ""
        if fp.mpn not in text or "cannot prove LCD-FPC Pin 1" not in text:
            failures.append(f"PDF page {index + 1} lacks connector identity or continuity warning")
    if len(reader.pages) >= 4:
        text = reader.pages[3].extract_text() or ""
        for required in ("EC12 ROTARY ENCODER", "6x6 TACT SWITCH", "10P FPC-TO-2.54 ADAPTER"):
            if required not in text:
                failures.append(f"PDF record page missing {required!r}")
    return {"path": path.relative_to(ROOT).as_posix(), "pages_checked": len(reader.pages),
            "page_details": page_details}


def main() -> int:
    failures: list[str] = []
    manifest_path = FIT_DIR / "fit-sheets-manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if manifest.get("orientation_frozen") is not False:
        failures.append("manifest must keep orientation_frozen=false")
    if manifest.get("page") != {
        "format": "A4", "height_mm": 297.0, "pdf_pages": 4,
        "print_scale": 1.0, "width_mm": 210.0,
    }:
        failures.append("manifest A4/scale declaration differs from expected")

    footprints = load_footprints()
    candidate_by_id = {entry["id"]: entry for entry in manifest.get("candidates", [])}
    svg_results = []
    for fp in footprints:
        candidate = candidate_by_id.get(fp.identifier)
        if not candidate:
            failures.append(f"manifest missing candidate {fp.identifier}")
            continue
        if candidate.get("orientation_frozen") is not False:
            failures.append(f"candidate {fp.identifier} unexpectedly frozen")
        if candidate.get("source_sha256") != sha256(fp.source):
            failures.append(f"candidate {fp.identifier} source hash is stale")
        svg_path = ROOT / candidate["svg"]
        svg_results.append(verify_svg(svg_path, fp, failures))

    record_result = verify_record_svg(ROOT / manifest["record_svg"], failures)
    pdf_result = verify_pdf(PDF_PATH, footprints, failures)
    for output in manifest.get("outputs", []):
        path = ROOT / output["path"]
        if not path.is_file():
            failures.append(f"missing output {output['path']}")
        elif output.get("sha256") != sha256(path):
            failures.append(f"output hash mismatch {output['path']}")

    result = {
        "schema_version": 1,
        "status": "PASS" if not failures else "FAIL",
        "orientation_frozen": False,
        "source_footprints_checked": len(footprints),
        "svg": svg_results,
        "record_svg": record_result,
        "pdf": pdf_result,
        "failures": failures,
    }
    output_path = FIT_DIR / "verification.json"
    output_path.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0 if not failures else 1


if __name__ == "__main__":
    sys.exit(main())
