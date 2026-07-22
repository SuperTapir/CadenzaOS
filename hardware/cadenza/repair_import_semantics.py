#!/usr/bin/env python3
"""Repair deterministic EasyEDA-Pro schematic import helper semantics.

This stage intentionally does not touch the PCB, the 77 real schematic
symbols, component placement, or copper.  It only:

* removes the imported A4 sheet pseudo-symbol;
* replaces 20 two-pin EasyEDA net-alias bridge symbols with canonical net
  labels matching the frozen PCB net names;
* uniquely annotates imported power symbols and removes their bogus empty
  footprint link.

The script asserts the complete expected alias mapping before writing.
"""

from __future__ import annotations

import argparse
import collections
import json
import shutil
import sys
from pathlib import Path


SKIP_CACHE = Path("/Users/tapir/.cache/cadenza-kicad-tools")

EXPECTED_ALIASES = {
    "KEY_MENU": "GPIO18",
    "KEY_OPTION": "GPIO8",
    "KEY_SELECT": "GPIO16",
    "KEY_START": "GPIO17",
    "KEY_A": "GPIO15",
    "KEY_B": "GPIO5",
    "KEY_UP": "GPIO7",
    "KEY_LEFT": "GPIO19",
    "KEY_DOWN": "GPIO20",
    "KEY_RIGHT": "GPIO6",
    "SD_CMD": "GPIO11",
    "SD_CLK": "GPIO13",
    "SD_DATA": "GPIO9",
    "SD_CD": "GPIO10",
    "TFT_BLK": "GPIO39",
    "TFT_MOSI": "GPIO12",
    "TFT_CLK": "GPIO48",
    "TFT_DC": "GPIO47",
    "TFT_RST": "GPIO3",
    "TFT_CS": "GPIO14",
}


def point(value) -> tuple[float, float]:
    return round(float(value[0]), 6), round(float(value[1]), 6)


def build_wire_graph(schematic):
    graph: dict[tuple[float, float], set[tuple[float, float]]] = collections.defaultdict(set)
    labels: dict[tuple[float, float], list] = collections.defaultdict(list)
    for wire in schematic.wire:
        a = point(wire.start.value)
        b = point(wire.end.value)
        graph[a].add(b)
        graph[b].add(a)
    for label in schematic.label:
        labels[point(label.at.value)].append(label)
    return graph, labels


def connected_points(graph, start):
    seen = {start}
    queue = [start]
    while queue:
        current = queue.pop()
        for other in graph[current]:
            if other not in seen:
                seen.add(other)
                queue.append(other)
    return seen


def infer_alias(symbol, graph, labels):
    sides = []
    for pin in symbol.pin:
        start = (round(pin.location.x, 6), round(pin.location.y, 6))
        component = connected_points(graph, start)
        side_labels = [label for p in component for label in labels[p]]
        sides.append({"pin": pin, "start": start, "points": component, "labels": side_labels})

    names = {str(label.value) for side in sides for label in side["labels"]}
    function_names = sorted(names & EXPECTED_ALIASES.keys())
    gpio_names = sorted(names & set(EXPECTED_ALIASES.values()))

    if len(function_names) == 1:
        function_name = function_names[0]
        canonical = EXPECTED_ALIASES[function_name]
    elif len(gpio_names) == 1:
        canonical = gpio_names[0]
        function_name = next(name for name, gpio in EXPECTED_ALIASES.items() if gpio == canonical)
    else:
        raise RuntimeError(
            f"Cannot uniquely identify alias symbol {symbol.uuid.value}: labels={sorted(names)}"
        )

    if gpio_names and gpio_names != [canonical]:
        raise RuntimeError(
            f"Alias {function_name} expected {canonical}, observed GPIO labels {gpio_names}"
        )
    return function_name, canonical, sides


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("schematic", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()

    if str(SKIP_CACHE) not in sys.path:
        sys.path.insert(0, str(SKIP_CACHE))
    import skip

    source = args.schematic.resolve()
    output = (args.output or source).resolve()
    report_path = (args.report or output.with_suffix(".semantic-repair.json")).resolve()
    if output != source:
        output.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, output)

    schematic = skip.Schematic(str(output))
    graph, labels = build_wire_graph(schematic)
    bridge_symbols = [s for s in schematic.symbol if str(s.lib_id.value).endswith(":Short-Symbol")]
    if len(bridge_symbols) != 20:
        raise RuntimeError(f"Expected 20 alias bridges, found {len(bridge_symbols)}")

    inferred = []
    for symbol in bridge_symbols:
        function_name, canonical, sides = infer_alias(symbol, graph, labels)
        inferred.append((function_name, canonical, symbol, sides))
    inferred_map = {function_name: canonical for function_name, canonical, _, _ in inferred}
    if inferred_map != EXPECTED_ALIASES:
        missing = sorted(set(EXPECTED_ALIASES) - set(inferred_map))
        extra = sorted(set(inferred_map) - set(EXPECTED_ALIASES))
        wrong = {
            k: (EXPECTED_ALIASES[k], inferred_map[k])
            for k in EXPECTED_ALIASES.keys() & inferred_map.keys()
            if EXPECTED_ALIASES[k] != inferred_map[k]
        }
        raise RuntimeError(f"Alias map mismatch: missing={missing}, extra={extra}, wrong={wrong}")

    # EasyEDA can place more than one functional label for a signal.  Some of
    # those duplicates are on a separate wire island and therefore are not
    # reachable from the two-pin bridge itself.  Once all 20 bridge mappings
    # have been validated above, normalize every matching label globally.
    # This is deliberately value-limited to EXPECTED_ALIASES so unrelated net
    # names remain untouched.
    globally_renamed_labels = []
    for label in schematic.label:
        old = str(label.value)
        if old in EXPECTED_ALIASES:
            canonical = EXPECTED_ALIASES[old]
            label.value = canonical
            globally_renamed_labels.append(
                {
                    "from": old,
                    "to": canonical,
                    "at": [float(label.at.value[0]), float(label.at.value[1])],
                }
            )

    alias_report = []
    for function_name, canonical, symbol, sides in inferred:
        renamed = []
        for side in sides:
            side_labels = side["labels"]
            for label in side_labels:
                old = str(label.value)
                if old != canonical:
                    label.value = canonical
                    renamed.append(old)
        alias_report.append(
            {
                "uuid": str(symbol.uuid.value),
                "functional_name": function_name,
                "canonical_pcb_net": canonical,
                "renamed_labels": sorted(set(renamed)),
            }
        )
        symbol.delete()

    sheet_symbols = [
        s for s in schematic.symbol if str(s.lib_id.value).endswith(":Sheet-Symbol_A4")
    ]
    if len(sheet_symbols) != 1:
        raise RuntimeError(f"Expected one sheet pseudo-symbol, found {len(sheet_symbols)}")
    sheet_symbols[0].delete()

    power_symbols = [
        s
        for s in schematic.symbol
        if str(s.property.Reference.value) == "#PWR?"
        and any(
            str(s.lib_id.value).endswith(suffix)
            for suffix in (":Ground-GND", ":Power-VCC", ":Power-5V")
        )
    ]
    if len(power_symbols) != 69:
        raise RuntimeError(f"Expected 69 imported power symbols, found {len(power_symbols)}")
    power_report = []
    for index, symbol in enumerate(power_symbols, start=1):
        reference = f"#PWR{index:03d}"
        symbol.property.Reference.value = reference
        symbol.instances.project.path.reference.value = reference
        symbol.property.Footprint.value = ""
        symbol.in_bom.value = False
        symbol.on_board.value = False
        symbol.in_pos_files.value = False
        power_report.append(
            {
                "reference": reference,
                "lib_id": str(symbol.lib_id.value),
                "uuid": str(symbol.uuid.value),
            }
        )

    schematic.write(str(output))
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(
        json.dumps(
            {
                "source": str(source),
                "output": str(output),
                "removed_sheet_pseudo_symbols": 1,
                "repaired_alias_bridges": alias_report,
                "globally_renamed_labels": globally_renamed_labels,
                "annotated_power_symbols": power_report,
            },
            ensure_ascii=False,
            indent=2,
        )
        + "\n"
    )
    print(
        json.dumps(
            {
                "alias_bridges": len(alias_report),
                "power_symbols": len(power_report),
                "sheet_pseudo_symbols": 1,
            },
            ensure_ascii=False,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
