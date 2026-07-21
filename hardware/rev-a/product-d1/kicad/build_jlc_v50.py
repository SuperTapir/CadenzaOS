#!/usr/bin/env python3
"""Split the Rev-A BOM into JLC core-SMT and hand/TBD assembly sets."""

from pathlib import Path
import csv
import re


ROOT = Path(__file__).resolve().parent / "production-v50-release-20260721" / "assembly"
SOURCE = ROOT / "cadenza-d1-v50-raw-bom.csv"
CORE = ROOT / "cadenza-d1-v50-jlc-core-bom.csv"
HAND = ROOT / "cadenza-d1-v50-hand-tbd.csv"

core_fields = ["Comment", "Designator", "Footprint", "LCSC Part #", "MPN", "Manufacturer", "Quantity", "Notes"]
hand_fields = core_fields + ["Gate"]


def expand_designators(value: str) -> str:
    expanded: list[str] = []
    for token in (part.strip() for part in value.split(",")):
        match = re.fullmatch(r"([A-Z]+)(\d+)-([A-Z]*)(\d+)", token)
        if not match:
            expanded.append(token)
            continue
        prefix, first, repeated_prefix, last = match.groups()
        if repeated_prefix and repeated_prefix != prefix:
            raise ValueError(f"Mixed-prefix range: {token}")
        expanded.extend(f"{prefix}{number}" for number in range(int(first), int(last) + 1))
    return ",".join(expanded)

with SOURCE.open(newline="") as source:
    rows = list(csv.DictReader(source))

core_rows = []
hand_rows = []
for row in rows:
    lcsc = row["LCSC Part #"].strip()
    common = {
        "Comment": row["Comment"],
        "Designator": expand_designators(row["Designator"]),
        "Footprint": row["Footprint"],
        "LCSC Part #": lcsc,
        "MPN": row["MPN"],
        "Manufacturer": row["Manufacturer"],
        "Quantity": row["Quantity"],
        "Notes": "JLC core SMT" if re.fullmatch(r"C\d+", lcsc) else "Not in core SMT upload",
    }
    if re.fullmatch(r"C\d+", lcsc):
        core_rows.append(common)
    elif lcsc != "N/A":
        common["Gate"] = "Freeze exact MPN/LCSC or hand-solder after inspection"
        hand_rows.append(common)

with CORE.open("w", newline="") as output:
    writer = csv.DictWriter(output, fieldnames=core_fields)
    writer.writeheader()
    writer.writerows(core_rows)

with HAND.open("w", newline="") as output:
    writer = csv.DictWriter(output, fieldnames=hand_fields)
    writer.writeheader()
    writer.writerows(hand_rows)

print(f"core={len(core_rows)} grouped rows, hand/TBD={len(hand_rows)} grouped rows")
