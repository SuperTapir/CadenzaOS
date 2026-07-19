# App Launch Sequence Specification

## Purpose
定义各 App 在系统交接之后可独立实现的启动序列边界、上下文输入、端点一致性、生命周期隔离、双 profile 确定性与降级行为。

## Requirements

### Requirement: Apps can provide an independent launch sequence
An App SHALL be able to provide an optional const full-screen launch-frame renderer driven by normalized progress and a read-only AppRenderContext. The Runtime MAY call it before `onEnter`; rendering a launch frame MUST NOT mutate App state, Cover pixels, lifecycle, catalog, or system services.

#### Scenario: Runtime samples a launch sequence
- **WHEN** a registered App is launched from Home and implements the launch renderer
- **THEN** the Runtime can render any progress in `[0, 1]` deterministically before `onEnter` without delivering an App update

#### Scenario: Progress is sampled repeatedly
- **WHEN** the same App, profile, and normalized progress are rendered more than once
- **THEN** the resulting framebuffer bytes are identical and no allocation or diagnostic failure occurs

### Requirement: Built-in Apps have distinct launch identities
Timer, Motion, Settings, and Animation Gallery SHALL each provide a visually distinct code-native launch sequence whose first phase continues its static Cover identity and whose final frame can hand off to the App first frame.

#### Scenario: Built-in launch frames are compared
- **WHEN** representative early, middle, and final launch progress values are captured for all built-in Apps
- **THEN** every App has distinct framebuffer hashes at one or more representative phases and all frames remain valid at both display profiles

### Requirement: Missing launch sequences have a bounded fallback
If an App does not implement launch-frame rendering, Runtime SHALL use a deterministic static Cover bridge or bounded name fallback and SHALL still preserve lifecycle ordering and transition completion.

#### Scenario: Third-party App has no launch renderer
- **WHEN** Launcher opens a registered App whose launch renderer returns false
- **THEN** Runtime uses the fallback handoff without calling the renderer again as ordinary App content or exposing uninitialized transition pixels

### Requirement: Launch-frame progress is scrubbable for verification
Launch renderers SHALL produce valid frames for forward, repeated, and decreasing progress values without completion callbacks or irreversible side effects.

#### Scenario: Verification seeks backward
- **WHEN** a test renders progress 0.8 followed by 0.3 and then 0.8 again
- **THEN** both 0.8 frames match exactly and the 0.3 frame represents the earlier phase

### Requirement: Launch sequence endpoints are visually continuous
Each built-in launch sequence SHALL begin with the exact centered static Cover bridge and SHALL end with the exact first App framebuffer produced for the same App state and SystemSnapshot after its normal `onEnter` semantics. Intermediate frames SHALL evolve between those identities without a hard replacement by an unrelated full-screen composition.

#### Scenario: Launch endpoints are compared
- **WHEN** progress 0 and progress 1 are captured for a built-in App at either display profile
- **THEN** progress 0 matches the centered Cover bridge byte-for-byte and progress 1 matches the App first frame byte-for-byte

#### Scenario: Launch is sampled at 30 FPS
- **WHEN** the full handoff is rendered at fixed 30 FPS
- **THEN** adjacent frames change only a bounded portion of framebuffer pixels and no single frame performs an unmasked full-composition jump
