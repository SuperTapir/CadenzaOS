# Cadenza D1 Rev A procurement audit

Status: WIP, stock snapshot taken 2026-07-21. Stock is volatile and must be
rechecked when the BOM is uploaded for assembly. LCSC catalogue availability
does not by itself prove that JLCPCB can allocate the part to a specific PCBA
order.

Generated source BOM:

- `kicad/cadenza-d1-bom-audit.csv`
- `kicad/production-v40-final/assembly/cadenza-d1-v40-procurement-draft.csv`
  is the current auditable mapping. Rows marked `TO_FREEZE` are intentionally
  not orderable yet.

## Core components

| Ref | Manufacturer part | LCSC/JLC identifier | Snapshot | Rev A policy |
| --- | --- | --- | --- | --- |
| U1 | Espressif `ESP32-S3-WROOM-1-N16R8` | LCSC `C2913202` | More than 20k shown in stock | Freeze exact flash/PSRAM variant; do not substitute a different memory variant silently |
| U2 | TI `BQ24074RGTR` | LCSC `C54313` | About 4k shown in stock | Freeze; substitute only by schematic redesign and charging/power-path review |
| U3 | TI `TPS63070RNMR` | LCSC `C109322` | Private JLC library entry frozen | Freeze exact VQFN-HR footprint and exposed-pad geometry |
| U4 | TI `TPS61023DRLR` | LCSC `C919459` | Private JLC library entry frozen | Freeze exact SOT-563 pinout and 5 V boost design |
| U5 | ADI/Maxim `MAX98357AETE+T` | LCSC `C910544` | More than 12k shown in stock | Freeze for Rev A; JLC assembly-library allocation must be confirmed after BOM import |
| U6 | Infineon `IM73D122V01XTMA1` | LCSC `C5563886` | Catalogue entry and EDA data found | Freeze through the acoustic prototype; do not replace with an analog or differently ported microphone |
| U7 | TI `BQ27441DRZR-G1A` | LCSC `C473397` | Only about 45 shown in stock | Procurement risk; reserve before ordering or qualify a redesign, not a footprint-only swap |
| U8 | ST `USBLC6-2SC6` | LCSC `C7519` | Private JLC library entry frozen | Equivalent ESD arrays require pinout, capacitance and IEC performance review |
| J1 | HCTL `HC-TYPE-C-16P-01A` | LCSC `C2997435` | Private JLC library entry frozen | Confirm tongue, shell tabs and enclosure opening before release |
| J2 | HRS `FH34SRJ-10S-0.5SH(50)` | LCSC `C324723` | About 19k shown in stock | Candidate only until owner FPC insertion/contact-side test passes |
| J3 | HRS `DM3AT-SF-PEJM5` | LCSC `C114218` | More than 7k shown in stock | Freeze exact suffix/height/ejector geometry with enclosure |

Evidence links:

- https://www.lcsc.com/product-detail/WiFi-Modules_Espressif-Systems-ESP32-S3-WROOM-1-N16R8_C2913202.html
- https://www.lcsc.com/product-detail/PMIC-Battery-Management_TI_BQ24074RGTR_BQ24074RGTR_C54313.html
- https://www.lcsc.com/product-detail/Audio-Power-OpAmps_Analog-Devices-MAX98357AETE-T_C910544.html
- https://www.lcsc.com/product-detail/C5563886.html
- https://www.lcsc.com/product-detail/Battery-Management-ICs_Texas-Instruments-BQ27441DRZR-G1A_C473397.html
- https://www.lcsc.com/product-detail/C324723.html
- https://www.lcsc.com/product-detail/C114218.html

## Owner-supplied and mechanical parts

These cannot be treated as ordinary SMT substitutions:

- one verified Sharp `LS027B7DH01` display sample;
- 1-cell protected LiPo battery with frozen body, wire exit, connector polarity,
  capacity and discharge rating;
- speaker with frozen diameter/rectangle, depth, impedance, rated power, wire
  exit and acoustic chamber requirements;
- right-side encoder/A daughterboard and mechanical wheel;
- `Menu`, `B`, reset and power-switch actuators/caps;
- enclosure halves, fasteners, display support gasket, acoustic mesh and
  controlled adhesive/foam parts.

Battery and speaker must remain DNI/TBD in an assembly order until their exact
samples have passed enclosure and functional tests.

## Substitution classes

### Class A: no substitution without electrical redesign

- MCU/module memory variant;
- charger and power-path controller;
- fuel gauge;
- audio amplifier;
- MEMS microphone technology, port direction and clock/interface;
- display and display connector orientation;
- USB-C, microSD and encoder connectors where enclosure geometry depends on the
  exact housing.

### Class B: substitute only after footprint and parameter review

- regulators, load switches and ESD devices;
- switches and board connectors;
- inductors, ferrites and current-sense resistors;
- capacitors whose voltage, dielectric, ESR or DC-bias behavior matters.

### Class C: normal controlled alternates

- ordinary 0603 resistors and non-critical ceramic capacitors with identical
  value, tolerance, voltage rating, dielectric, package and temperature rating.

Every alternate must receive its own manufacturer part number and LCSC/JLC code
in the released BOM. Never approve an editor search result solely because its
symbol name or nominal value matches.

## JLC import and allocation gate

After the final KiCad project is imported into a new JLCEDA D1 project:

1. export/upload the production BOM and centroid file;
2. bind each designator to an exact JLC/LCSC catalogue entry;
3. compare manufacturer, full suffix, package drawing, pin 1 and exposed-pad
   geometry against the KiCad footprint;
4. record Basic/Extended/consigned status and stock quantity;
5. do not accept automatic alternatives for Class A parts;
6. resolve every unmatched or zero-stock line before quotation;
7. keep display, battery, speaker, wheel and enclosure parts outside the SMT
   placement list unless explicitly included in a later mechanical-assembly
   process;
8. archive the mapped BOM with date and project revision because availability
   will change.

## Current procurement blockers

- Exact battery and speaker samples/dimensions are not frozen.
- The HRS display connector has not yet passed physical FPC insertion and
  contact-side verification.
- JLC assembly-library allocation has not been verified through the actual D1
  BOM import.
- The complete BOM still needs manufacturer part numbers and LCSC codes added
  to schematic fields before release; value-only passive rows are insufficient
  for repeatable purchasing.
