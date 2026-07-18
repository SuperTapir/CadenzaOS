## MODIFIED Requirements

### Requirement: Both display profiles have snapshots
Approved screenshot/framebuffer snapshots SHALL cover Launcher, Clock, Motion, Settings, Gallery, and representative transitions at 320×170 and 400×240. Launcher coverage SHALL include Vertical and Horizontal settled states, a deterministic navigation midpoint, every built-in App Cover, generic fallback, and press/launch representative captures whose Cover pixels match their neutral counterparts; interactive tests SHALL separately prove continuity, loop direction, retarget, Reduced Motion monotonicity, Cover immutability, bounds safety and moving-click semantics rather than relying only on terminal hashes.

#### Scenario: Snapshot regression runs
- **WHEN** deterministic fixed-step scenes render their capture frames
- **THEN** dimensions and framebuffer hashes match approved snapshots or fail with inspectable PNG output, including both Launcher orientations, an animated midpoint, distinct built-in Covers, fallback and press/launch immutability representatives for each profile

#### Scenario: Launcher interaction regression runs
- **WHEN** automated Launcher cases exercise loop boundaries, repeated and reversed turns, Reduced Motion, orientation switching, and click before settle
- **THEN** logical selection, continuous visual position, immutable Cover pixels, opened App, command count, geometry/text diagnostics, and final stable frames match the launcher-card-navigation contract
