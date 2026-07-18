# runtime-verification Specification

## Purpose
定义 core、headless、desktop 与 firmware 的分层验证、golden/snapshot、共享源码审计、性能预算和实体设备声明规则。
## Requirements
### Requirement: Development follows a TDD gate
Every behavior introduced by this change SHALL first have an automated test or executable failing case that demonstrates the missing contract, followed by implementation and relevant regression verification.

#### Scenario: A capability task is completed
- **WHEN** an implementation task is marked complete
- **THEN** its associated test is present, was capable of failing before implementation, and passes after implementation

### Requirement: App lifecycle is tested
Automated tests SHALL cover registration, initial entry, open guards, `onExit`/`onEnter` order, transition input freeze, and system long-press return.

#### Scenario: Lifecycle suite runs
- **WHEN** the host test suite executes lifecycle cases
- **THEN** all lifecycle calls and active-App updates match the portable-runtime contract

### Requirement: Input state machine is tested
Automated tests SHALL cover debounce, short press, long press, release, repeated samples, turn direction, saturation, and frame reset behavior without GPIO.

#### Scenario: Boundary timing is tested
- **WHEN** release occurs immediately before, at, and after the long-press threshold
- **THEN** click and long-press outputs match the defined inclusive boundary

### Requirement: Transitions are tested
Automated tests SHALL cover every transition endpoint, representative midpoint, lifecycle swap, interruption guard, and source/destination immutability.

#### Scenario: Transition suite runs
- **WHEN** transitions are rendered from known source and destination patterns
- **THEN** golden endpoint and midpoint hashes match for both profiles where resolution affects geometry

### Requirement: Tween numerical behavior is tested
Automated tests SHALL cover every easing endpoint/interior sample, Tween start/mid/end, delay, completion, seek, Sequence, Parallel, and Timeline offsets.

#### Scenario: Tween golden vectors run
- **WHEN** the animation numerical suite executes
- **THEN** all values and state transitions match exact or epsilon-appropriate expectations

### Requirement: Reverse and repeat behavior is tested
Automated tests SHALL cover reverse playback, yoyo, repeat count, repeat delay, boundary crossing, and infinite-repeat safety.

#### Scenario: Repeat boundary is crossed with a large delta
- **WHEN** one update spans multiple repeat boundaries
- **THEN** final value, repeat state, and callback count match the defined traversal semantics

### Requirement: Both display profiles have snapshots
Approved screenshot/framebuffer snapshots SHALL cover Launcher, Clock, Motion, Settings, Gallery, and representative transitions at 320×170 and 400×240.

#### Scenario: Snapshot regression runs
- **WHEN** deterministic fixed-step scenes render their capture frames
- **THEN** dimensions and framebuffer hashes match approved snapshots or fail with inspectable PNG output

### Requirement: Simulator tests require no hardware
All unit, integration, deterministic replay, screenshot, and desktop smoke tests SHALL run without a connected T-Embed or physical display.

#### Scenario: Clean host executes tests
- **WHEN** tests run on a macOS host with build dependencies but no serial or GPIO device
- **THEN** the complete P7 host suite passes

### Requirement: Build matrix is verified
Completion SHALL require successful host tests, SDL3 desktop build, desktop smoke/E2E, and PlatformIO T-Embed compile, plus `git diff --check` or equivalent whitespace validation.

#### Scenario: Change completion is audited
- **WHEN** P0–P7 are claimed complete
- **THEN** fresh command output proves every required build and test gate and lists P8 as the only intentionally unexecuted phase
