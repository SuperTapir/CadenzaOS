## MODIFIED Requirements

### Requirement: Pixel fonts have a legal resource pipeline
Text rendering SHALL use platform-independent bitmap font descriptors with glyph metrics, baseline, tracking, kerning, explicit semantic role, integer scaling, and documented source/license provenance. Measurement, alignment, bounded layout, black/white rasterization and clipping SHALL consume the same descriptor selected by the role. The repository SHALL include a repeatable offline conversion path and stale-output check for every compiled font resource.

#### Scenario: Text is rendered on both profiles
- **WHEN** the same role, text, alignment, position, and scale are rendered at either display profile
- **THEN** glyph pixels and metrics are produced by the same core code using the descriptor cached for that viewport, without profile- or device-dependent selection in App code

#### Scenario: White text is drawn over black
- **WHEN** a selected row draws role-based text with black set to false
- **THEN** glyph pixels are cleared to white inside the clip while transparent glyph background leaves the black row unchanged

#### Scenario: Generated font resource becomes stale
- **WHEN** a pinned source `.fnt`, image table, glyph manifest, or converter behavior changes without regenerating the compiled descriptor
- **THEN** the build-time font check fails with the stale generated path

### Requirement: Focus backgrounds have bounded rounded geometry
Menu Item focus backgrounds SHALL fill the item's available width, retain the component's outer margin, use a small deterministic corner radius, and never rasterize outside the supplied bounds. System Menu, Settings rows, Gallery selection, and confirmation buttons SHALL use the shared rounded fill primitive rather than unrelated rectangular focus labels.

#### Scenario: A menu row receives focus
- **WHEN** a menu row is selected
- **THEN** its inverted background spans the full row bounds with rounded corners, while the pixels outside those bounds remain unchanged
