# portable-runtime Specification

## Purpose
定义 Cadenza OS 1-bit 互动运行时的可移植边界，包括 App 生命周期、输入语义、平台适配、模拟时间、诊断和明确的非游戏引擎范围。
## Requirements
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

The runtime SHALL update applications and animations only from an injected presentation/simulation delta, while system deadline services SHALL use a separately injected nondecreasing monotonic time; pausing or clamping presentation delta SHALL not silently pause or shorten a running Timer.

#### Scenario: Simulation is paused
- **WHEN** monotonic time and input polling continue while presentation simulation delta is zero
- **THEN** App and animation state do not advance while a Running Timer remaining value continues to follow its monotonic deadline

#### Scenario: Long frame is clamped for presentation
- **WHEN** one platform frame stalls for 2 seconds and presentation delta is clamped to 50 ms
- **THEN** animation receives 50 ms while TimerService observes the full monotonic advance and does not gain 1.95 seconds

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

Launcher, Timer, Motion, Settings, and Gallery SHALL implement the same static App lifecycle and registry contract on firmware, headless host, and desktop simulator; the configured Home App SHALL be reached through an explicit System Menu action rather than a direct long-press transition. System Menu SHALL suspend the active App without changing AppId, while system-owned Timer state continues independently. Timer service remaining active SHALL NOT imply that its passive status indicator is visible on non-Home Apps.

#### Scenario: App is switched
- **WHEN** the runtime opens a registered App and completes its transition
- **THEN** the outgoing App receives `onExit`, the incoming App receives `onEnter`, and only the active App receives normal updates

#### Scenario: System Menu gesture is used
- **WHEN** any stable App receives long-press
- **THEN** the runtime opens the system-owned Menu over that App without changing current AppId or emitting App exit/enter

#### Scenario: System Menu returns Home
- **WHEN** a non-Home App is active and the user confirms Home in System Menu
- **THEN** the Menu closes and the runtime initiates a normal transition to the composition root configured Home App without App-specific hardware handling

#### Scenario: Active App is suspended by Menu
- **WHEN** System Menu remains active for one or more frames
- **THEN** the active App receives no normal update/input/render until resume, system services continue, and resume does not emit App exit/enter

#### Scenario: Timer owner leaves foreground
- **WHEN** Timer App starts a Timer and transitions to Launcher or another App
- **THEN** Timer App stops receiving normal updates but the Timer service remains owned, continues toward its deadline, remains visible in system snapshot, and its passive indicator is shown only while Home/Launcher is the current App

