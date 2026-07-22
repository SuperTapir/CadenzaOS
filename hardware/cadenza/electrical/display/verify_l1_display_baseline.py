#!/usr/bin/env python3
"""Check the machine-readable L1 display candidate for obvious pin/domain mistakes."""

import json
from pathlib import Path


BASELINE = Path(__file__).with_name("l1-display-electrical-baseline.json")
EXPECTED_NAMES = [
    "SCLK", "SI", "SCS", "EXTCOMIN", "DISP",
    "VDDA", "VDD", "EXTMODE", "VSS", "VSSA",
]


def main() -> None:
    data = json.loads(BASELINE.read_text(encoding="utf-8"))
    pins = data["pins"]
    assert [item["pin"] for item in pins] == list(range(1, 11))
    assert [item["name"] for item in pins] == EXPECTED_NAMES

    by_name = {item["name"]: item for item in pins}
    assert by_name["VDDA"]["connection"] == "+5V"
    assert by_name["VDD"]["connection"] == "+5V"
    assert by_name["EXTMODE"]["connection"] == "+5V"
    assert by_name["VSS"]["connection"] == "GND"
    assert by_name["VSSA"]["connection"] == "GND"
    assert by_name["DISP"]["capacitor_to_ground_uf"] >= 0.1
    assert by_name["VDDA"]["minimum_capacitor_to_ground_uf"] >= 0.1
    assert by_name["VDD"]["minimum_capacitor_to_ground_uf"] >= 1.0

    gpio_connections = {
        int(item["connection"].removeprefix("GPIO"))
        for item in pins
        if item["connection"].startswith("GPIO")
    }
    restricted = data["forbidden_or_reserved_gpio"]
    forbidden = set(restricted["strapping"]) | set(restricted["native_usb"]) | set(restricted["unavailable_on_n16r8"])
    assert gpio_connections.isdisjoint(forbidden)
    assert len(gpio_connections) == 5
    implementation = data["screen_only_implementation"]
    assert implementation["delete_reference_components"] == ["FPC1", "U6", "Q2", "R13", "R14"]
    assert [item["reference"] for item in implementation["reuse_reference_components"]] == ["C20", "C21"]
    assert implementation["new_components"] == ["J_DISP1", "C_DISP1"]
    assert implementation["test_points"] == []
    assert implementation["power_lock_included"] is False
    assert implementation["usb_changes_included"] is False
    assert data["physical_direction"]["selection_frozen"] is False
    print("PASS: screen-only mapping, reused capacitors, voltage domains, and GPIO restrictions")


if __name__ == "__main__":
    main()
