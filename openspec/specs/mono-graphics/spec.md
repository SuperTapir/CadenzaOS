# mono-graphics Specification

## Purpose
定义跨 T-Embed 与 Sharp profile 共用的 1-bit framebuffer、MonoCanvas 图元、字体/位图光栅、裁剪与 presenter 像素契约。
## Requirements
### Requirement: Framebuffer format is canonical
The core SHALL own a row-major, MSB-first packed 1-bit framebuffer with `1` representing black, stride `(width + 7) / 8`, and supported dimensions 320×170 and 400×240.

#### Scenario: Buffer size is calculated
- **WHEN** a 400×240 framebuffer is initialized
- **THEN** its stride is 50 bytes and its active storage is exactly 12,000 bytes

#### Scenario: Pixel bit is addressed
- **WHEN** a pixel at a known coordinate is set and cleared
- **THEN** only the specified MSB-first bit changes

### Requirement: Software primitives are deterministic
The core canvas SHALL draw clear, pixel, line, rectangle, filled rectangle, circle, filled circle, and text using integer software rasterization independent of display backends.

#### Scenario: Primitive golden cases render
- **WHEN** boundary, degenerate, and representative primitives are rendered
- **THEN** their exact framebuffer bytes match approved golden results

### Requirement: Drawing is clipped and diagnosable
Every draw operation SHALL respect a half-open clip rectangle and framebuffer bounds; invalid or fully clipped primitives SHALL not write outside storage and SHALL be observable through an optional diagnostic sink.

#### Scenario: Out-of-bounds geometry is drawn
- **WHEN** a primitive crosses every canvas edge
- **THEN** in-bounds pixels are correct, guard bytes are unchanged, and a diagnostic count is updated

### Requirement: Bitmap and sprite drawing are available
The canvas SHALL draw packed 1-bit bitmaps and sprite source regions with copy, set-black, and XOR composition.

#### Scenario: Transparent-style sprite is composed
- **WHEN** a sprite region is drawn with set-black composition
- **THEN** source black pixels are applied while source white pixels leave the destination unchanged

### Requirement: Sprite atlases have fixed metadata
A sprite atlas SHALL reference one bitmap plus a fixed-capacity list of named or indexed frame rectangles and SHALL report lookup or capacity failure explicitly.

#### Scenario: Atlas capacity is exhausted
- **WHEN** a frame is added beyond atlas capacity
- **THEN** the operation fails without changing existing frame metadata

### Requirement: Basic transforms are supported
Sprite drawing SHALL support integer translation and horizontal or vertical flip while respecting source and destination clipping.

#### Scenario: Flipped clipped sprite is drawn
- **WHEN** a horizontally flipped sprite is partially outside the clip rectangle
- **THEN** visible pixels preserve the correct reversed source mapping

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

### Requirement: Pattern and dither fills are controllable
The canvas SHALL support pattern/dither fills with explicit pattern data, coverage level, and phase, including animated phase changes.

#### Scenario: Dither phase advances
- **WHEN** the same region is filled with consecutive phase values
- **THEN** black coverage remains bounded while the pixel arrangement changes deterministically

### Requirement: Regions can be inverted
The canvas SHALL invert an arbitrary clipped rectangular region using XOR semantics.

#### Scenario: Region is inverted twice
- **WHEN** the same region is inverted two times
- **THEN** the framebuffer returns exactly to its original bytes

### Requirement: Off-screen canvases are first-class
The runtime SHALL support multiple framebuffer/canvas instances for source, destination, output, and local composition without changing the drawing API.

#### Scenario: Transition uses off-screen frames
- **WHEN** outgoing and incoming Apps are rendered into separate canvases
- **THEN** composing a transition does not mutate either source canvas

### Requirement: Reusable visual assets are traceable
Every bundled font, texture, dither table, or converted asset not authored in this repository SHALL have recorded provenance and a license compatible with redistribution.

#### Scenario: Asset manifest is audited
- **WHEN** bundled visual resources are enumerated
- **THEN** each resource is marked as original, public domain, or linked to its applicable license

### Requirement: Focus backgrounds have bounded rounded geometry
Menu Item focus backgrounds SHALL fill the item's available width, retain the component's outer margin, use a small deterministic corner radius, and never rasterize outside the supplied bounds. System Menu, Settings rows, Gallery selection, and confirmation buttons SHALL use the shared rounded fill primitive rather than unrelated rectangular focus labels.

#### Scenario: A menu row receives focus
- **WHEN** a menu row is selected
- **THEN** its inverted background spans the full row bounds with rounded corners, while the pixels outside those bounds remain unchanged
