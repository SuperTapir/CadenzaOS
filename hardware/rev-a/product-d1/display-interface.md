# LS027B7DH01 interface freeze

Status: WIP evidence gate. The display is owner-supplied and is the only fixed
external component in Rev A. Do not order the PCB until the sample/FPC and the
board connector pass the checks in this document.

## Authoritative reference

Sharp specification `LCY-1210401A`, model `LS027B7DH01`:

- https://media.digikey.com/pdf/Data%20Sheets/Sharp%20PDFs/LS027B7DH01.pdf

Distributor HTML mirrors are useful for search, but the PDF and the physical
sample control the design.

## Electrical interface

The FPC has ten terminals:

| Pin | Signal | Function |
| ---: | --- | --- |
| 1 | `SCLK` | Serial clock input |
| 2 | `SI` | Serial data input |
| 3 | `SCS` | Chip-select input |
| 4 | `EXTCOMIN` | External COM-inversion input |
| 5 | `DISP` | Display on/off input; pixel memory is retained when low |
| 6 | `VDDA` | Analog panel supply |
| 7 | `VDD` | Digital panel supply |
| 8 | `EXTMODE` | COM-inversion mode selection |
| 9 | `VSS` | Logic ground |
| 10 | `VSSA` | Analog ground |

The published recommended operating conditions are:

- `VDDA` and `VDD`: 4.8 V minimum, 5.0 V typical, 5.5 V maximum;
- logic high: 2.70 V minimum, operation near 3 V recommended;
- logic low: `VSS` to `VSS + 0.15 V`;
- `VDD` and `VDDA` should rise together, or `VDD` first; they should fall
  together, or `VDD` first;
- `DISP` and `EXTCOMIN` sequencing must follow the datasheet timing chart.

Therefore the ESP32-S3 may drive the logic signals at 3.3 V, but the panel
supplies require a regulated 5 V rail. A bare 3.3 V connection to pins 6/7 is
not compliant with the published operating range.

Recommended local capacitors from Sharp's external-circuit example:

- `DISP` to `VSS`: 0.1 uF ceramic;
- `VDDA` to `VSS`: at least 0.1 uF ceramic;
- `VDD` to `VSS`: at least 1 uF ceramic.

The final schematic must show these capacitors close to the FPC connector and
must identify the 5 V source, enable behavior and discharge/power-down path.

## Mechanical interface

Published panel dimensions:

- active/viewing area: 58.8 x 35.28 mm;
- module outline, excluding protrusions: 62.8 x 42.82 x 1.64 mm;
- pixel array: 400 x 240 at 0.147 mm pitch.

FPC handling constraints:

- bend only 0.8 to 6.0 mm from the glass edge;
- minimum inside bend radius: 0.45 mm;
- do not bend toward the polarizer-film side;
- do not let the FPC touch the glass edge;
- avoid stress at the glass/FPC bonded region;
- Sharp limits repeated 180-degree bending to three cycles.

The enclosure must support the glass perimeter without point loading, leave the
bonded FPC neck unstressed, and provide a controlled bend channel rather than
folding the cable against a PCB or screw boss.

## Connector gate

Sharp lists these connectors for the illustrated FPC orientations:

- SMK `CFP-4610-0150F`, bottom-side contact;
- Molex `51441-1093`, bottom-side contact;
- SMK `CFP-4510-0150F`, top-side contact after the specified fold.

The current D1 design uses Hirose `FH34SRJ-10S-0.5SH` as a candidate. It is not
frozen merely because it is a 10-position, 0.5 mm-pitch connector. Before PCB
release, verify all of the following from manufacturer drawings and the actual
sample:

1. exposed-contact side of the owner's FPC;
2. top/bottom contact orientation after the intended enclosure fold;
3. pin-1 location viewed from the PCB side;
4. 0.5 mm pitch and compatible FPC thickness;
5. insertion depth, actuator clearance and FPC exit direction;
6. electrical pad order from `SCLK` through `VSSA`;
7. footprint toe/heel dimensions and courtyard;
8. no enclosure feature enters the actuator service envelope.

Preferred decision: use one of Sharp's recommended connector families when it
is purchasable and mechanically suitable. Retain the Hirose part only after a
documented equivalence check and a physical insertion test.

## Release evidence

Attach the following evidence to the Rev A manufacturing package:

- clear macro photographs of both FPC sides with pin 1 marked;
- caliper measurements of FPC width, pitch, stiffener thickness and exposed
  contact length;
- connector manufacturer drawing and footprint revision;
- schematic pin-map review signed against the table above;
- continuity test from every connector pad to its intended circuit node on an
  unpowered first article;
- current-limited 5 V power-up with the specified sequencing;
- display update, all-white/all-black, COM inversion and long-idle tests;
- enclosure fit check with no FPC crease or glass edge load.

