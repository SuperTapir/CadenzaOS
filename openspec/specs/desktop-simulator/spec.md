# desktop-simulator Specification

## Purpose
定义 SDL3 桌面模拟器作为共享 Runtime 的平台适配器所需的显示、输入、profile、设备外框和真实可执行 smoke 契约。
## Requirements
### Requirement: Desktop has an independent build target
The project SHALL provide a CMake desktop executable using SDL3 and the same core library compiled by the firmware target.

#### Scenario: Desktop target is built
- **WHEN** CMake configures with SDL3 available and builds the simulator target
- **THEN** the executable links without Arduino or TFT_eSPI

### Requirement: Both display profiles are selectable
The simulator SHALL run at native logical resolutions 320×170 for T-Embed and 400×240 for Sharp without non-uniform content scaling.

#### Scenario: Profile is changed
- **WHEN** the user selects either supported profile
- **THEN** the runtime receives that canvas width and height and Apps lay out against the actual dimensions

### Requirement: Integer presentation is supported
The simulator SHALL display framebuffer pixels using nearest-neighbor 1×, 2×, 3×, or 4× integer scale.

#### Scenario: Scale is selected
- **WHEN** a scale from 1 through 4 is chosen
- **THEN** one logical pixel occupies an exact square of that integer size without interpolation

### Requirement: Desktop input maps to raw device semantics
Mouse wheel and Left/Right keys SHALL emit turn events, while Space and Enter SHALL emit button events that pass through the shared InputReducer.

#### Scenario: Keyboard enters an App
- **WHEN** the user rotates selection with an arrow key and taps Enter
- **THEN** the same click semantics used by T-Embed open the selected App

#### Scenario: Desktop long press opens system menu
- **WHEN** Space or Enter is held past the configured long-press threshold in an App
- **THEN** the runtime opens the shared system menu, where Home remains an explicit action

### Requirement: Basic runtime metrics are visible
The simulator SHALL expose FPS, frame duration, current App, and recent input events without modifying the App framebuffer used for snapshots unless the overlay is explicitly included.

#### Scenario: Overlay is toggled
- **WHEN** the diagnostics overlay is enabled or disabled
- **THEN** App state and framebuffer truth remain unchanged

### Requirement: Existing Apps complete the desktop loop
Launcher, Timer, Motion, Settings, and Gallery SHALL open, receive input, render, switch, and return home in the macOS simulator.

#### Scenario: Full navigation smoke path runs
- **WHEN** scripted inputs visit every registered App and return to Launcher
- **THEN** no platform-specific alternate UI or App implementation is invoked
