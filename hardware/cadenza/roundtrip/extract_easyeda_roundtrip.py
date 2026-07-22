#!/usr/bin/env python3
"""Read-only extractor for an EasyEDA Pro local .eprj2 project.

EasyEDA Pro stores the imported document stream as an AES-GCM encrypted,
gzip-compressed history snapshot.  The per-project key and IV are stored in
the same SQLite file.  This script does not alter that database.
"""

from __future__ import annotations

import argparse
import base64
import collections
import gzip
import hashlib
import json
import sqlite3
from pathlib import Path
from typing import Any

from cryptography.hazmat.primitives.ciphers.aead import AESGCM


MIL_TO_MM = 0.0254


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def decrypt_history(path: Path) -> str:
    connection = sqlite3.connect(f"file:{path.resolve()}?mode=ro", uri=True)
    try:
        tables = [
            row[0]
            for row in connection.execute(
                "select name from sqlite_master where type='table' "
                "and name like 'project_history_%'"
            )
        ]
        if len(tables) != 1:
            raise ValueError(f"expected one project_history_* table, got {tables}")
        table = tables[0]
        uuid, key_hex = connection.execute(
            f'SELECT uuid, key FROM "{table}" ORDER BY id DESC LIMIT 1'
        ).fetchone()
        encoded = connection.execute(
            "SELECT dataStr FROM history_data WHERE history_uuid=? ORDER BY id DESC LIMIT 1",
            (uuid,),
        ).fetchone()[0]
    finally:
        connection.close()

    encrypted = base64.b64decode(encoded)
    compressed = AESGCM(bytes.fromhex(key_hex)).decrypt(
        bytes.fromhex(uuid), encrypted, None
    )
    return gzip.decompress(compressed).decode("utf-8")


def parse_documents(stream: str) -> list[dict[str, Any]]:
    documents: list[dict[str, Any]] = []
    current: dict[str, Any] | None = None
    for line_number, raw in enumerate(stream.splitlines(), 1):
        if not raw:
            continue
        left, separator, right = raw.partition("||")
        if not separator:
            raise ValueError(f"line {line_number}: missing record separator")
        if right.endswith("|"):
            right = right[:-1]
        header = json.loads(left)
        payload = json.loads(right) if right else None
        if header.get("type") == "DOCHEAD":
            current = {"meta": payload or {}, "records": [], "line": line_number}
            documents.append(current)
        elif current is not None:
            current["records"].append(
                {"header": header, "payload": payload, "raw": raw}
            )
    return documents


def only_document(documents: list[dict[str, Any]], doc_type: str) -> dict[str, Any]:
    matches = [doc for doc in documents if doc["meta"].get("docType") == doc_type]
    if len(matches) != 1:
        raise ValueError(f"expected one {doc_type} document, got {len(matches)}")
    return matches[0]


def records_by_type(document: dict[str, Any]) -> dict[str, list[dict[str, Any]]]:
    result: dict[str, list[dict[str, Any]]] = collections.defaultdict(list)
    for record in document["records"]:
        result[record["header"]["type"]].append(record)
    return dict(result)


def attributes(records: list[dict[str, Any]]) -> dict[str, dict[str, Any]]:
    result: dict[str, dict[str, Any]] = collections.defaultdict(dict)
    for record in records:
        if record["header"]["type"] != "ATTR" or not record["payload"]:
            continue
        payload = record["payload"]
        result[str(payload.get("parentId", ""))][str(payload.get("key", ""))] = payload.get("value")
    return dict(result)


def path_xy(path: list[Any]) -> list[tuple[float, float]]:
    """Return coordinate pairs while skipping EasyEDA path commands/arc angles."""
    points: list[tuple[float, float]] = []
    index = 0
    while index < len(path):
        value = path[index]
        if isinstance(value, str):
            if value == "ARC":
                index += 2  # command plus angle; endpoint follows
            else:
                index += 1
            continue
        if index + 1 < len(path) and isinstance(path[index + 1], (int, float)):
            points.append((float(value), float(path[index + 1])))
            index += 2
        else:
            index += 1
    return points


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("project", type=Path)
    parser.add_argument("--baseline", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()

    stream = decrypt_history(args.project)
    documents = parse_documents(stream)
    pcb_doc = only_document(documents, "PCB")
    sch_doc = only_document(documents, "SCH_PAGE")
    pcb = records_by_type(pcb_doc)
    sch = records_by_type(sch_doc)
    pcb_attrs = attributes(pcb_doc["records"])
    sch_attrs = attributes(sch_doc["records"])

    pcb_components = []
    for record in pcb.get("COMPONENT", []):
        identity = str(record["header"]["id"])
        payload = record["payload"] or {}
        attrs = pcb_attrs.get(identity, {})
        pcb_components.append(
            {
                "id": identity,
                "designator": attrs.get("Designator"),
                "footprint": attrs.get("FootprintName") or attrs.get("Footprint"),
                "x_mil": payload.get("x"),
                "y_mil": payload.get("y"),
                "rotation_deg": payload.get("angle"),
            }
        )

    outline_points: list[tuple[float, float]] = []
    outline_records = []
    for record in pcb.get("POLY", []):
        payload = record["payload"] or {}
        if payload.get("layerId") != 11:
            continue
        points = path_xy(payload.get("path") or [])
        outline_points.extend(points)
        outline_records.append(record["header"]["id"])
    xs = [point[0] for point in outline_points]
    ys = [point[1] for point in outline_points]
    outline = None
    if xs and ys:
        outline = {
            "record_ids": outline_records,
            "min_x_mil": min(xs),
            "max_x_mil": max(xs),
            "min_y_mil": min(ys),
            "max_y_mil": max(ys),
            "width_mm": round((max(xs) - min(xs)) * MIL_TO_MM, 4),
            "height_mm": round((max(ys) - min(ys)) * MIL_TO_MM, 4),
        }
        for component in pcb_components:
            if component["x_mil"] is None or component["y_mil"] is None:
                continue
            component["x_mm_from_left"] = round(
                (float(component["x_mil"]) - min(xs)) * MIL_TO_MM, 4
            )
            component["y_mm_from_bottom"] = round(
                (float(component["y_mil"]) - min(ys)) * MIL_TO_MM, 4
            )

    pad_net_values = []
    numbered_pad_records = 0
    for record in pcb.get("PAD_NET", []):
        identity = json.loads(record["header"]["id"])
        if len(identity) > 2 and identity[2] != "":
            numbered_pad_records += 1
        payload = record["payload"] or {}
        if payload.get("padNet"):
            pad_net_values.append(str(payload["padNet"]))
    routed_net_values = []
    for kind in ("LINE", "VIA", "POUR"):
        for record in pcb.get(kind, []):
            payload = record["payload"] or {}
            if payload.get("netName"):
                routed_net_values.append(str(payload["netName"]))

    schematic_refs = []
    for record in sch.get("COMPONENT", []):
        attrs = sch_attrs.get(str(record["header"]["id"]), {})
        designator = attrs.get("Designator")
        if designator and designator not in ("?", "#PWR?"):
            schematic_refs.append(str(designator))

    baseline = json.loads(args.baseline.read_text(encoding="utf-8"))
    baseline_refs = {
        item["designator"] for item in baseline["pcb"]["component_list"]
    }
    roundtrip_refs = {
        str(item["designator"]) for item in pcb_components if item["designator"]
    }

    holes = [
        item for item in pcb_components
        if str(item.get("designator") or "").startswith("SCREW")
    ]
    physical_match = {
        "component_count": len(pcb_components) == baseline["pcb"]["components"],
        "designator_set": roundtrip_refs == baseline_refs,
        "track_segments": len(pcb.get("LINE", [])) == baseline["pcb"]["routed_track_segments"],
        "vias": len(pcb.get("VIA", [])) == baseline["pcb"]["vias"],
        "pours": len(pcb.get("POUR", [])) == baseline["pcb"]["pours"],
        "board_size": bool(outline)
        and outline["width_mm"] == baseline["pcb"]["board_outline"]["primary"]["width_mm"]
        and outline["height_mm"] == baseline["pcb"]["board_outline"]["primary"]["height_mm"],
        "mounting_hole_count": len(holes) == len(baseline["pcb"]["mounting_holes"]),
    }
    network_match = {
        "expected_nets": baseline["pcb"]["nets"],
        "unique_pad_nets": len(set(pad_net_values)),
        "unique_routed_nets": len(set(routed_net_values)),
        "all_numbered_pads_have_empty_net": numbered_pad_records > 0 and not pad_net_values,
        "pass": len(set(pad_net_values)) == baseline["pcb"]["nets"],
    }

    output = {
        "schema_version": 1,
        "source": {
            "project": str(args.project.resolve()),
            "sha256": sha256(args.project),
            "history_document_count": len(documents),
            "document_types": dict(sorted(collections.Counter(
                doc["meta"].get("docType", "") for doc in documents
            ).items())),
        },
        "roundtrip": {
            "schematic": {
                "record_counts": dict(sorted((key, len(value)) for key, value in sch.items())),
                "real_designators": sorted(set(schematic_refs)),
            },
            "pcb": {
                "record_counts": dict(sorted((key, len(value)) for key, value in pcb.items())),
                "components": len(pcb_components),
                "component_list": sorted(pcb_components, key=lambda item: str(item.get("designator"))),
                "numbered_pad_records": numbered_pad_records,
                "pad_net_names": sorted(set(pad_net_values)),
                "routed_net_names": sorted(set(routed_net_values)),
                "outline": outline,
                "mounting_holes": holes,
            },
        },
        "comparison": {
            "physical": physical_match,
            "network": network_match,
            "overall_pass": all(physical_match.values()) and network_match["pass"],
        },
    }

    args.output_dir.mkdir(parents=True, exist_ok=True)
    (args.output_dir / "roundtrip-audit.json").write_text(
        json.dumps(output, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    (args.output_dir / "decrypted-document-index.json").write_text(
        json.dumps(
            [
                {
                    "docType": doc["meta"].get("docType"),
                    "uuid": doc["meta"].get("uuid"),
                    "records": len(doc["records"]),
                    "line": doc["line"],
                }
                for doc in documents
            ],
            ensure_ascii=False,
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )
    for name, document in (("roundtrip-pcb.records", pcb_doc), ("roundtrip-schematic.records", sch_doc)):
        text = "\n".join(record["raw"] for record in document["records"]) + "\n"
        (args.output_dir / name).write_text(text, encoding="utf-8")

    print(json.dumps(output["comparison"], ensure_ascii=False, indent=2))
    return 0 if output["comparison"]["overall_pass"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
