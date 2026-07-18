## MODIFIED Requirements

### Requirement: App lifecycle is tested
Automated tests SHALL cover fixed-capacity registration, arbitrary core-unknown AppId, invalid/duplicate/capacity rejection, configured Home App, initial entry, open guards, `onExit`/`onEnter` order, transition input freeze, narrow update/render contexts, and system long-press return.

#### Scenario: Lifecycle suite runs
- **WHEN** the host test suite executes lifecycle cases with a non-default Home App and test-only AppId
- **THEN** all registry results, lifecycle calls, active-App updates, context access, and return navigation match the portable-runtime and app-capability contracts

### Requirement: Simulator tests require no hardware
All unit, integration, deterministic replay, screenshot, descriptor/packet, headless microphone, system service, and desktop smoke tests SHALL run without a connected T-Embed, physical display, microphone, or USB audio device.

#### Scenario: Clean host executes tests
- **WHEN** tests run on a macOS host with build dependencies but no serial, GPIO, microphone, or ESP32 USB device
- **THEN** the complete automated host suite passes and reports hardware validation as pending rather than skipped success

### Requirement: Build matrix is verified
Automated completion SHALL require successful core/system/App unit tests, strict-warning and sanitizer runs where supported, approved snapshots, SDL3 desktop build and smoke/E2E, current T-Embed regression compile, reproducible candidate SDK UAC2+CDC compile, source-boundary audit, OpenSpec strict validation, and `git diff --check` or equivalent whitespace validation. Product hardware completion SHALL additionally require the documented T-Embed-to-macOS microphone acceptance suite.

#### Scenario: Automated foundation completion is audited
- **WHEN** implementation and build-ready tasks are claimed complete without physical hardware
- **THEN** fresh command output proves every automated/build gate, records dependency versions and RAM/flash/task/stack data, and labels microphone/Mac/coexistence evidence as hardware validation pending

#### Scenario: Product hardware completion is audited
- **WHEN** USB voice device and T-Embed microphone are claimed product-complete
- **THEN** dated evidence covers macOS enumeration, format, recording, reconnect, sleep/wake, long-duration clock stability, App+USB concurrency, acoustic leakage, Wi-Fi/BLE coexistence where enabled, and power/resource observations

## ADDED Requirements

### Requirement: System service frame transaction is tested
Automated tests SHALL cover platform-event ingestion, frozen update snapshot, FIFO command commit, same-frame render snapshot, command/event capacity, per-frame budget, rejection diagnostics, and callback non-reentrancy.

#### Scenario: Frame trace runs
- **WHEN** a fixture injects platform events and an App submits multiple commands in one frame
- **THEN** the trace matches ingestion → update → FIFO commit → render exactly, with deterministic snapshots and diagnostics

### Requirement: Voice input and fan-out are tested
Automated tests SHALL cover PCM format, chunk normalization, independent consumer delivery, queue overflow policy, start/stop arbitration, error recovery, analyzer level/activity, privacy state, stale flush, USB packetization and underflow silence.

#### Scenario: Slow consumer does not disturb USB
- **WHEN** analyzer is intentionally stalled while headless capture and USB packet consumption continue
- **THEN** only analyzer overrun increases, USB PCM remains ordered, capture does not block, and replay results are deterministic

### Requirement: Research and dependency decisions are auditable
For platform-core SDK, USB audio, BLE/Wi-Fi service model and codec integration, the project SHALL record pinned versions/commits, licenses, directly read source files, representative alternatives/failures, experiments, adopted/rejected decisions and unresolved hardware assumptions before declaring the corresponding implementation task complete.

#### Scenario: Architecture review is performed
- **WHEN** a reviewer audits the system-service or USB voice implementation
- **THEN** each material external design choice traces to a pinned primary source or reproducible experiment, and unresolved claims are explicitly marked pending
