## MODIFIED Requirements

### Requirement: Both display profiles have snapshots
Approved screenshot/framebuffer snapshots SHALL cover Launcher, Clock, Motion, Settings, Gallery, system menu/overlay surfaces, and representative transitions at 320×170 and 400×240. Typography migrations SHALL update the full matrix rather than a single representative screen, and failures SHALL emit inspectable lossless PNG output.

#### Scenario: Snapshot regression runs
- **WHEN** deterministic fixed-step scenes render their capture frames after the typography migration
- **THEN** dimensions and framebuffer hashes match approved snapshots or fail with inspectable PNG output for the exact App, surface and profile

## ADDED Requirements

### Requirement: Typography visual regression must be reviewed as a complete matrix
Completion SHALL produce a deterministic contact sheet containing every built-in App, business Display typography and representative system surface at both display profiles. The review SHALL record whether each frame has accidental text clipping, overlap, illegible thin strokes, inconsistent hierarchy, excessive density, incorrect business-font replacement, or incorrect selected-row inversion; any observed severe defect SHALL be fixed before approval.

#### Scenario: Final typography baseline is audited
- **WHEN** the implementation is proposed as visually complete
- **THEN** an inspectable contact sheet and review record cover the full matrix, with no unresolved severe typography defect

### Requirement: Font flash cost must be explicit
The build SHALL report the compiled bytes for each font descriptor and their total, and SHALL enforce a documented typography flash budget without allocating runtime heap or additional framebuffer storage.

#### Scenario: Font assets are regenerated
- **WHEN** the deterministic font conversion runs
- **THEN** its manifest reports per-role bitmap, glyph metadata and kerning bytes plus the total used by all unique font resources
