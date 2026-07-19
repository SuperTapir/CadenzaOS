# 2026-07-19 responsive typography visual review

Reviewed artifact: `typography-contact-sheet.png`

Scope: 17 deterministic scenes at T-Embed 320×170 and Sharp 400×240,
including Launcher, Timer static and transition states, Motion, Settings,
Animation Gallery, system menu, passive overlay, long text, and selected-row
inversion.

## Resolution contract

- Both viewports use the same `Hero`, `Title`, `Body`, `Menu`, `Compact`, `Caption`, and `Footer`
  semantics.
- T-Embed resolves `Title` to Roobert 20 Medium and `Compact` to Roobert 10
  Bold. Sharp resolves them to 24 Medium and 11 Bold.
- Both resolve `Body` to 20 Medium and `Caption` to 11 Medium.
- Both resolve `Hero` to 24 Medium; the Timer alert action uses measured text
  width plus padding and stays within the card inner frame.
- Both resolve App action strips to `Footer` 9 Mono Condensed with a 14 px
  native line box.
- Timer numerals remain the dedicated seven-segment atlas; Launcher Covers
  remain their approved bitmap assets.

## Findings and disposition

- The first responsive export exposed the old forced `SET` / `TINGS` split.
  Settings now measures the semantic `Title` and derives its panel geometry
  from that cached metric, producing one intact `SETTINGS` title at both
  viewports without a device branch.
- App action strips were too large and heavy at 11 Medium. Timer, Motion,
  Settings, and Gallery now share the 14 px `Footer`; review confirms the
  lighter strip remains legible at both viewports without touching business
  Display typography.
- System Menu, Settings, Gallery selection, and the Timer confirmation button
  now use the shared rounded focus fill. System Menu focus spans the complete
  row bounds, removing the extra unfilled inset while preserving the component
  margin.
- Timer state badges now derive width and height from `Compact` metrics plus
  padding, so `ACTIVE` and `PAUSED` no longer touch or cross their background.
  The alert title uses 24 Medium `Hero`; its action button is also metric-sized
  and remains inside the card at both viewports.
- System Menu and Settings rows use 11 Bold `Menu` on both viewports. Settings
  removes the table-like unselected borders and derives row height/gaps from
  cached metrics, using the available vertical space instead of compressing six
  boxes together.
- Settings Motion and Sound reuse the System Menu toggle and volume bars,
  including selected-row inversion; connection and orientation states retain
  explicit text instead of ambiguous icons.
- No reviewed frame has unresolved text clipping, overlap, broken selected-row
  inversion, illegible thin strokes, or accidental replacement of business
  Display typography.
- Compact labels on 320×170 are visibly denser than 400×240 while retaining
  the same hierarchy and call-site roles. Body and caption rhythm remains
  stable across both viewports.

Result: host visual regression has no unresolved severe defect. Physical comfort, reflection, PPI, and
viewing-distance judgment on actual T-Embed and Sharp hardware remains a
separate hardware review boundary.
