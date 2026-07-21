#!/usr/bin/env python3
"""Generate the auditable D1 procurement BOM from the board audit CSV."""

import argparse
import csv


FROZEN = {
    "U1": ("Espressif Systems", "ESP32-S3-WROOM-1-N16R8", "C2913202", "FROZEN_CORE", "Exact N16R8 memory variant"),
    "U2": ("Texas Instruments", "BQ24074RGTR", "C54313", "FROZEN_CORE", "Charger and power-path controller"),
    "U3": ("Texas Instruments", "TPS63070RNMR", "C109322", "FROZEN_CORE", "Buck-boost regulator"),
    "U4": ("Texas Instruments", "TPS61023DRLR", "C919459", "FROZEN_CORE", "5 V boost regulator"),
    "U5": ("Analog Devices / Maxim", "MAX98357AETE+T", "C910544", "FROZEN_CORE", "I2S mono class-D amplifier"),
    "U6": ("Infineon", "IM73D122V01XTMA1", "C5563886", "SAMPLE_GATE", "Bottom-port PDM microphone; acoustic sample gate"),
    "U7": ("Texas Instruments", "BQ27441DRZR-G1A", "C473397", "FROZEN_CORE", "Stock-risk item; no silent substitute"),
    "U8": ("STMicroelectronics", "USBLC6-2SC6", "C7519", "FROZEN_CORE", "USB ESD array"),
    "J1": ("HCTL", "HC-TYPE-C-16P-01A", "C2997435", "MECHANICAL_GATE", "Confirm shell tabs and tongue orientation"),
    "J2": ("Hirose Electric", "FH34SRJ-10S-0.5SH(50)", "C324723", "SAMPLE_GATE", "Must pass owner FPC contact-side insertion test"),
    "J3": ("Hirose Electric", "DM3AT-SF-PEJM5", "C114218", "MECHANICAL_GATE", "Confirm card direction and enclosure opening"),
}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("input")
    parser.add_argument("output")
    args = parser.parse_args()
    with open(args.input, newline="", encoding="utf-8") as source:
        rows = list(csv.DictReader(source))

    fields = [
        "Designator", "Quantity", "Comment", "Footprint", "Manufacturer",
        "Manufacturer Part", "LCSC Part", "Assembly", "Freeze Status", "Notes",
    ]
    with open(args.output, "w", newline="", encoding="utf-8") as target:
        writer = csv.DictWriter(target, fieldnames=fields)
        writer.writeheader()
        for row in rows:
            refs = row["Refs"]
            manufacturer, mpn, lcsc, status, notes = FROZEN.get(
                refs, ("", "", "", "TO_FREEZE", "Exact MPN, ratings and LCSC allocation not yet frozen")
            )
            writer.writerow({
                "Designator": refs,
                "Quantity": row["Qty"],
                "Comment": row["Value"],
                "Footprint": row["Footprint"],
                "Manufacturer": manufacturer,
                "Manufacturer Part": mpn,
                "LCSC Part": lcsc,
                "Assembly": "JLC_SMT",
                "Freeze Status": status,
                "Notes": notes,
            })


if __name__ == "__main__":
    main()
