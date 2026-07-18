## ADDED Requirements

### Requirement: Runtime scope is explicit
The project SHALL define the current deliverable as a 1-bit interaction and animation runtime for Cadenza OS, while physics, collision, Tilemap, ECS, and level systems SHALL remain outside this change.

#### Scenario: Scope is reviewed
- **WHEN** a contributor reads the project and change documentation
- **THEN** the runtime goals and excluded game-engine capabilities are stated without contradicting the long-term personal-platform vision

### Requirement: Core is standard C++
Application, runtime, input semantics, graphics, and animation headers and sources SHALL compile as C++17 without Arduino, ESP32, TFT_eSPI, or SDL headers.

#### Scenario: Host compiles the core
- **WHEN** the host build compiles all core sources
- **THEN** it succeeds without defining Arduino compatibility shims

### Requirement: Platform dependencies are adapters
Hardware time, raw input, logging, memory metrics, and framebuffer presentation SHALL enter the core through explicit adapters or injected values; T-Embed, desktop, and future Sharp presentation SHALL not change App behavior contracts.

#### Scenario: Platform headers are audited
- **WHEN** core include and source trees are searched for platform APIs
- **THEN** direct `Serial`, GPIO, `millis`, `micros`, TFT_eSPI, and SDL usage is absent

### Requirement: Simulation time is controllable
The runtime SHALL update applications and animations only from an injected simulation delta, distinct from the monotonic timestamps used for input and performance metrics.

#### Scenario: Simulation is paused
- **WHEN** wall time and input polling continue while simulation delta is zero
- **THEN** App and animation state do not advance

### Requirement: Logging is optional and structured
The core SHALL emit diagnostics through an optional sink and SHALL behave correctly with a no-op sink.

#### Scenario: Runtime has no logger
- **WHEN** an App transition runs without a configured log sink
- **THEN** the transition completes without platform calls or null access

### Requirement: Input semantics are shared
Raw turn and button events SHALL be reduced by one core `InputReducer` into a per-frame `InputFrame` containing saturated turn delta, press, release, click, long-press, and held duration.

#### Scenario: Short press is reduced
- **WHEN** a button-down and button-up occur after debounce but before the long-press threshold
- **THEN** exactly one click and one release are delivered and no long-press is delivered

#### Scenario: Long press is reduced
- **WHEN** a button remains down through the configured long-press threshold
- **THEN** exactly one long-press is delivered and release does not produce a click

#### Scenario: High-speed turn input is accumulated
- **WHEN** raw turn deltas exceed the range of an 8-bit signed integer before a frame is taken
- **THEN** the frame preserves the value within the documented 16-bit saturation bounds rather than wrapping

### Requirement: Application lifecycle is platform-independent
Launcher, Clock, Motion, Settings, and Gallery SHALL implement the same static App lifecycle and registry contract on firmware, headless host, and desktop simulator.

#### Scenario: App is switched
- **WHEN** the runtime opens a registered App and completes its transition
- **THEN** the outgoing App receives `onExit`, the incoming App receives `onEnter`, and only the active App receives normal updates

#### Scenario: System return gesture is used
- **WHEN** a non-Launcher App receives long-press
- **THEN** the runtime initiates a return to Launcher without App-specific hardware handling

