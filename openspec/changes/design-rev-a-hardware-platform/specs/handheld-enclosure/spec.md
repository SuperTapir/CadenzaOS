## ADDED Requirements

### Requirement: Screen-safe mechanical installation
The enclosure SHALL retain the LS027B7DH01 by its module perimeter and SHALL provide a display window covering the active area without placing stress on the glass or FPC tail.

#### Scenario: Screen installation
- **WHEN** the display is installed in the enclosure
- **THEN** the FPC SHALL remain within its specified bend region and radius without being used as a structural support

### Requirement: Handheld control access
The enclosure SHALL expose a smaller `Menu` key above a primary `B` key on the left, and a concentric control on the right whose outer region rotates and whose center region activates `A`, so that all actions are accessible in normal horizontal handheld use.

#### Scenario: Primary handheld operation
- **WHEN** the device is held in its intended horizontal orientation
- **THEN** a user SHALL be able to reach `Menu`, `B`, the encoder ring, and center `A` without covering the display window or confusing `A` with a separate adjacent key

### Requirement: Front audio openings
The enclosure SHALL provide a front-facing speaker grille in the lower-left acoustic region and a separate microphone opening at the upper edge, with sealed internal acoustic paths that prevent the speaker front and rear volumes from coupling to the microphone path.

#### Scenario: Unobstructed tabletop use
- **WHEN** the device is placed in either supported tabletop orientation
- **THEN** the speaker and microphone openings SHALL not be blocked by the supporting surface

### Requirement: Shared mechanical datum
The enclosure, PCB outline, display window, controls, acoustic cavities, battery cavity, mounting holes, and keepout regions SHALL be derived from one versioned mechanical parameter source rather than independently redrawn dimensions.

#### Scenario: PCB and enclosure exchange
- **WHEN** PCB DXF/STEP and enclosure STEP/STL artifacts are generated for a revision
- **THEN** their board outline, mounting holes, display datum, control centers, and connector openings SHALL agree within the documented prototype tolerance

### Requirement: Dual-angle tabletop support
The enclosure SHALL include two stable passive support surfaces that present the screen at low and high tabletop viewing angles.

#### Scenario: Low-angle placement
- **WHEN** the device is placed on its low-angle support surface
- **THEN** it SHALL remain stable during normal encoder and button operation

#### Scenario: High-angle placement
- **WHEN** the device is placed on its high-angle support surface
- **THEN** it SHALL remain stable while displaying and playing audio

### Requirement: Service and interface access
The enclosure SHALL expose USB-C and the physical master power control, and SHALL allow display, battery, and PCB service without destructive disassembly of the housing.

#### Scenario: Charging access
- **WHEN** the enclosure is assembled
- **THEN** a standard USB-C plug SHALL be insertable without removing the enclosure
