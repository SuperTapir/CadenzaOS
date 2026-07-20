## ADDED Requirements

### Requirement: LS027B7DH01 display integration
The Rev A hardware platform SHALL directly support the Sharp LS027B7DH01 display through a 10-pin, 0.5 mm FPC connector, 5V display supply rails, 3.3V-compatible control signals, and the display manufacturer's required local decoupling.

#### Scenario: Display connector and power review
- **WHEN** the Rev A schematic and PCB are reviewed before fabrication
- **THEN** the review SHALL confirm all ten display pins, dedicated 5V display supply, local VDDA/VDD capacitors, and accessible display test points

#### Scenario: Display brings up on the first board
- **WHEN** firmware sends initialization, COM inversion, and a known frame pattern
- **THEN** the display SHALL show the pattern without visible persistent damage or FPC stress

### Requirement: Input action hardware
The Rev A hardware platform SHALL provide separate `Menu` and `B` buttons on the left and a concentric rotary encoder/center `A` control on the right. The encoder SHALL provide true two-phase incremental output, and its center press SHALL be the `A` action rather than an adjacent button. The board support package SHALL expose Navigate, Confirm/Primary, Back, and Menu actions without requiring application-specific GPIO knowledge.

#### Scenario: Core navigation input
- **WHEN** a user rotates the outer encoder region or presses its center `A` region
- **THEN** the board support package SHALL emit Navigate or Confirm/Primary actions respectively

#### Scenario: Menu and return actions
- **WHEN** a user activates `Menu` or `B`
- **THEN** the board support package SHALL emit Menu or Back actions respectively

### Requirement: Audio acquisition and playback
The Rev A hardware platform SHALL provide a digital microphone path and a front-facing speaker path with physically separated microphone and speaker openings.

#### Scenario: Near-field recording
- **WHEN** a user speaks near the microphone opening while the speaker is idle
- **THEN** the recorded signal SHALL be usable for voice capture according to the recorded Rev A acceptance test

#### Scenario: Audible playback
- **WHEN** the device plays a standard voice test clip through its internal speaker
- **THEN** speech SHALL remain intelligible at the recorded Rev A listening distance

### Requirement: Power, data, and connectivity foundation
The Rev A hardware platform SHALL include a rechargeable battery system, USB-C data and charging interface, physical master power control, non-volatile system storage, Wi-Fi, and Bluetooth provided by an ESP32-S3 module.

#### Scenario: Power-off transport state
- **WHEN** the physical master power switch is set to off
- **THEN** the main system SHALL not remain operational from the battery

#### Scenario: Persistent system state
- **WHEN** system settings are saved and the device is power-cycled
- **THEN** the settings SHALL be available after the next successful boot

### Requirement: Manufacturing and debug readiness
The Rev A hardware platform SHALL be designed as a four-layer PCB and SHALL include programming, boot, power, display, and ground test access sufficient for first-board bring-up.

#### Scenario: Pre-fabrication review
- **WHEN** manufacturing files are prepared
- **THEN** the design SHALL pass configured electrical and physical design-rule checks and export Gerber, BOM, and placement data

### Requirement: Code-first interoperable design source
The Rev A PCB and enclosure SHALL be generated from version-controlled machine-readable parameters into open/interoperable KiCad, DXF, STEP/STL, and BOM artifacts before import into JLCEDA Pro. JLCEDA-specific source generation SHALL not be the sole authoritative design representation.

#### Scenario: JLCEDA Pro import
- **WHEN** a generated KiCad project and mechanical artifacts are imported into JLCEDA Pro
- **THEN** an import audit SHALL compare board dimensions, layer count, net count, footprint count, mounting holes, critical coordinates, and rebuilt copper against the generated source before manufacturing output is accepted

#### Scenario: Repeatable regeneration
- **WHEN** a frozen mechanical or electrical parameter changes
- **THEN** the generation command SHALL reproduce the affected PCB, enclosure, BOM, and firmware mapping artifacts from the same revisioned source without relying on browser drawing actions
