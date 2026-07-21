# Cadenza D1 design basis

Status: WIP design evidence for Rev A. This file records the external facts and
the product decisions derived from them. Mechanical dimensions and electrical
values remain subordinate to the frozen component datasheets and the Rev A
schematic/PCB.

## 1. Product intent

Cadenza D1 is a horizontal, pocketable dedicated device for CadenzaOS. Its
primary interaction model is deliberately small:

- a reflective 400 x 240 Sharp Memory LCD;
- a left-side `Menu` key and `B` key;
- a right-side rotary encoder whose centre push is `A`;
- a physical power switch;
- an internal microphone, a front-facing loudspeaker, USB-C, Wi-Fi, Bluetooth,
  persistent storage and a rechargeable battery;
- faceted enclosure edges that provide several stable desk viewing angles.

The wheel and `A` are one concentric control, not two adjacent controls. There
is no second front-face wheel.

## 2. Playdate comparison

### 2.1 Verified reference facts

Panic's official specification lists a 400 x 240 1-bit display, a built-in mono
speaker, a condenser microphone, 2.4 GHz 802.11b/g/n Wi-Fi, 4 GB flash, and
separate D-pad, A, B, Menu, Sleep and crank inputs. It lists the enclosure as
76 x 74 x 9 mm and eight hours of active battery life.

Sources:

- https://help.play.date/hardware/the-specs/
- https://help.play.date/hardware/supported-inputs/
- https://assets.play.date/help.play.date/PlaydateDimensions_1.0.pdf
- https://help.play.date/developer/designing-for-playdate/

### 2.2 What D1 adopts

- One high-salience continuous physical control plus a very small button set.
- Dedicated `Menu`, `A` and `B` semantics instead of overloaded touch UI.
- A sunlight-readable, always-legible monochrome display.
- Physical separation between display, audio apertures and thumb controls.
- On-device audio testing as a release gate, including bass audibility,
  clipping and loudness consistency.

### 2.3 What D1 intentionally changes

- Landscape body rather than Playdate's near-square portrait body.
- Rotary encoder with centre `A` push rather than a fold-out crank.
- Larger acoustic cavity and speaker target; the T-Embed speaker is not the
  product baseline.
- Bluetooth in addition to 2.4 GHz Wi-Fi.
- Faceted back/edge geometry that doubles as a passive desk stand.
- A replaceable engineering path for microphone, speaker and battery rather
  than freezing these before physical acoustic and runtime tests.

Playdate is therefore an interaction and packaging benchmark, not a board or
enclosure to clone.

## 3. KiCad and JLCEDA source-of-truth policy

KiCad is the authoritative editable electrical design for Rev A. JLCEDA Pro is
used as a manufacturing/import verification target and for JLC component
mapping. The design must not be round-tripped repeatedly between tools.

JLCEDA's current documentation says its migration assistant accepts KiCad
projects, `.kicad_sch`, `.kicad_pcb`, libraries and ZIP archives. Its dedicated
KiCad import page recommends packaging from KiCad rather than manually making
a ZIP, so referenced libraries are included. It also warns that copper zones
are rebuilt during import and may differ.

Sources:

- https://prodocs.lceda.cn/cn/import-export/import-kicad/index.html
- https://prodocs.lceda.cn/cn/import-export/easyeda-pro-format-converter/
- https://prodocs.lceda.cn/cn/format/index/

Consequences for D1:

1. Finish ERC/DRC and freeze the KiCad revision first.
2. Export Gerber and drill files from the frozen KiCad board as the independent
   fabrication reference.
3. Import a KiCad-generated project archive into a new JLCEDA D1 project.
4. In JLCEDA, re-check board outline, 4-layer stack, net count, footprint count,
   vias, keep-outs, connector orientation and every rebuilt copper zone.
5. Run JLCEDA DRC and compare its Gerber preview with the KiCad Gerber preview.
6. Map JLC/LCSC order codes only after electrical and physical equivalence is
   checked against the frozen manufacturer datasheet.
7. Do not overwrite the older D0 JLCEDA project and do not treat a successful
   import as permission to order.

## 4. Rev A evidence gates

### 4.1 Current PCB release candidate

The current electrical/manufacturing baseline is
`kicad/production-v40-final/cadenza-d1-v40.kicad_pcb`. It contains the full
four-layer antenna copper keep-out, routed LCD/PDM signals, distributed ground
stitching and four in-board mounting holes. KiCad reports:

- `0 DRC violations`;
- `0 unconnected pads`;
- `0 footprint errors`.

The independent Gerber, PTH/NPTH drill, position, IPC-D-356, statistics, STEP
and DRC outputs are in `kicad/production-v40-final/`. The earlier v38 import is
an import-workflow record only: its `H2` mounting hole was outside the board.
Do not fabricate v38.

Rev A is not ready for fabrication until all of the following have evidence:

- KiCad ERC has no unresolved electrical errors.
- KiCad DRC has no shorts, clearance failures, unconnected required nets,
  invalid drills or board-outline errors.
- The Sharp display FPC pinout, mating connector orientation and mechanical
  keep-out are checked against the exact panel/FPC datasheet or a measured,
  documented sample.
- Speaker, microphone, battery, encoder and switches each have a frozen part,
  datasheet, footprint and enclosure interface.
- Speaker chamber, front port and microphone acoustic path have physical
  prototype results; neither aperture may be sealed by adhesive or foam.
- Antenna keep-out excludes copper, battery, speaker magnet, fasteners and
  conductive coating.
- The enclosure CAD has been rendered and checked for display support, FPC bend
  radius, connector access, button travel, wheel clearance, screw bosses and
  assembly order.
- JLCEDA import comparison and JLC manufacturability checks are recorded.
- A first article is electrically tested before population or ordering of a
  larger batch.

## 5. Current unresolved items

- Freeze speaker and battery after dimensional and acoustic prototypes.
- Confirm the exact Sharp panel/FPC revision and connector mating geometry.
- Re-import v40 into the new JLCEDA D1 project and record the layer/outline/
  keep-out comparison; the v38 workflow import does not qualify v40.
- The six critical mechanical references now use frozen local STEP models; retain physical sample-fit as the final enclosure gate
  as a complete height/collision authority.
- Render, print and tolerance-check the enclosure against physical battery,
  speaker, switch, wheel and display samples.
- Freeze manufacturer and LCSC codes for every passive and remaining connector
  before treating the BOM as orderable.
