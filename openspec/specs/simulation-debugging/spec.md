# simulation-debugging Specification

## Purpose
定义桌面模拟时间控制、暂停/单步/倍率、HUD、诊断、截图和确定性录制能力，使视觉与时间问题可重复调查。
## Requirements
### Requirement: Simulation can pause and single-step
The simulator SHALL pause and resume simulation updates and SHALL advance exactly one configured fixed step on each single-step command while paused.

#### Scenario: Single frame advances
- **WHEN** simulation is paused and one step is requested
- **THEN** App/runtime/animation receive exactly one fixed delta and return to paused state

### Requirement: Time scale is selectable
The simulator SHALL provide 0.25×, 0.5×, 1×, and 2× simulation speeds independent of input polling and presentation.

#### Scenario: Half speed runs
- **WHEN** wall time advances one second at 0.5×
- **THEN** simulation time advances approximately one half second subject to fixed-step quantization

### Requirement: Fixed time step is deterministic
The simulator and headless host SHALL provide a fixed-step mode in which an identical initial state, input sequence, and frame count produce identical core state and framebuffer bytes.

#### Scenario: Script is replayed twice
- **WHEN** the same deterministic script is executed in two fresh hosts
- **THEN** each sampled framebuffer hash and diagnostic event sequence match

### Requirement: PNG screenshots are supported
The desktop host SHALL write the current logical framebuffer as a lossless PNG with profile, frame index, and simulation-time metadata in its filename or sidecar data.

#### Scenario: Screenshot is captured
- **WHEN** the screenshot command is issued
- **THEN** a decodable image with exact logical dimensions and black/white pixels is written

### Requirement: Animation recording is supported
The desktop host SHALL record either an animated GIF or a lossless PNG frame sequence from simulation frames and SHALL finalize partial recordings safely.

#### Scenario: Recording is stopped
- **WHEN** several simulation frames are recorded and stop is requested
- **THEN** the output can be decoded and contains the frames in simulation order

### Requirement: Debug state is inspectable
The debugger SHALL expose current App, full InputFrame, simulation mode/speed, fixed-capacity usage/overflow counters, platform heap estimate when available, FPS, and frame duration.

#### Scenario: Capacity overflow occurs
- **WHEN** a fixed pool rejects an entry
- **THEN** the debug state shows the affected pool and an incremented overflow count

### Requirement: Canvas misuse is reported
Debug builds SHALL detect fully invalid geometry, clipped geometry, invalid sprite/atlas references, and attempted writes outside framebuffer storage.

#### Scenario: Invalid primitive is submitted
- **WHEN** a primitive has invalid dimensions or coordinates that cannot affect the canvas
- **THEN** no storage corruption occurs and a categorized diagnostic is emitted

### Requirement: Device frame preview is optional
The simulator SHALL optionally place the logical display inside a non-authoritative device-frame preview that never changes framebuffer pixels or input coordinates after conversion.

#### Scenario: Device preview is toggled
- **WHEN** preview mode changes
- **THEN** the logical framebuffer hash remains identical

### Requirement: Animation progress can be scrubbed
The debugger SHALL expose a seek control for the active Gallery demonstration or registered Timeline so progress can be moved backward and forward while paused.

#### Scenario: Timeline is scrubbed
- **WHEN** progress is changed from 80% to 20%
- **THEN** the animation and Gallery render the state defined by 20% without replaying intermediate completion callbacks
