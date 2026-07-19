## MODIFIED Requirements

### Requirement: Transitions are tested
Automated tests SHALL cover every direct transition endpoint, representative midpoint, lifecycle swap, interruption guard, and source/destination immutability. Staged handoffs SHALL additionally cover pre-midpoint App launch-frame or Cover-bridge composition, exact continuity across the midpoint buffer swap, post-midpoint destination composition, direction routing, per-App distinction, fallback, motion profiles, 30 FPS adjacent-frame change bounds, fixed return Cover geometry, and the absence of a third full-screen buffer. System Menu tests SHALL cover monotonic open/close motion, closing input ownership, frozen App lifetime, one-shot sound/diagnostics, and release after full exit.

#### Scenario: Transition suite runs
- **WHEN** direct and staged transitions are rendered from known source, bridge, and destination patterns
- **THEN** golden endpoint, phase, midpoint-continuity, and direction results match for both profiles where resolution affects geometry

### Requirement: Both display profiles have snapshots
Approved screenshot/framebuffer snapshots SHALL cover Launcher, Timer, Motion, Settings, Gallery, representative direct transitions, every built-in App launch sequence, App-to-Launcher Cover handoff, and System Menu opening/open/closing keyframes at 320×170 and 400×240.

#### Scenario: Snapshot regression runs
- **WHEN** deterministic fixed-step scenes render their stable and transition capture frames
- **THEN** dimensions and framebuffer hashes match approved snapshots or fail with inspectable PNG output
