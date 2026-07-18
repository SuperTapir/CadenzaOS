# animation-presentation Specification

## Purpose
定义建立在动画核心之上的转场、选择反馈、相机效果、粒子和 Gallery 展示契约，并保证 reduced-motion、可逆预览与双分辨率行为一致。
## Requirements
### Requirement: Runtime transitions are selectable strategies
AppRuntime SHALL accept a transition strategy and duration without requiring Apps to know transition details, and the existing venetian-blinds behavior SHALL be one selectable strategy rather than hard-coded runtime drawing.

#### Scenario: Transition is selected
- **WHEN** an App is opened with a specified transition
- **THEN** that strategy composes captured outgoing and incoming frames while lifecycle ordering remains correct

### Requirement: Core transition set is complete
The presentation layer SHALL implement Dip/Fade, horizontal wipe, diagonal wipe, iris, venetian blinds, and checker/dither dissolve for 1-bit source and destination frames.

#### Scenario: Transition endpoints are rendered
- **WHEN** every transition renders progress 0 and progress 1
- **THEN** output exactly equals source at 0 and destination at 1

### Requirement: Selection feedback has overshoot and pulse
The UI SHALL provide reusable selection feedback with a short overshoot or pulse envelope that is interruptible and respects reduced-motion settings.

#### Scenario: Selection changes rapidly
- **WHEN** repeated turn input changes selection before feedback completes
- **THEN** feedback retargets continuously without a visible state jump or unbounded amplitude

### Requirement: Camera punch and shake are deterministic
The presentation layer SHALL provide a short camera punch and bounded camera shake using explicit duration, amplitude, decay, and seed.

#### Scenario: Shake is replayed
- **WHEN** two fresh effects use the same seed and fixed time sequence
- **THEN** their camera offsets match and return exactly to neutral at completion

### Requirement: Particles use a fixed pool
The presentation layer SHALL provide fixed-capacity particles with position, velocity, lifetime, optional acceleration, visual primitive/sprite, deterministic seed, and an explicit full-pool policy.

#### Scenario: Particle pool is full
- **WHEN** an emitter requests more particles than capacity
- **THEN** the configured reject-or-replace policy is applied without allocation or storage corruption

### Requirement: Frame animation and state machines are available
Sprite animation SHALL support atlas frame sequences in once, loop, and ping-pong modes, and a fixed-capacity state machine SHALL switch named animation states through explicit triggers.

#### Scenario: State transition changes animation
- **WHEN** a valid trigger moves from idle to active
- **THEN** the configured active frame sequence begins according to its reset policy

### Requirement: Knob can directly control progress
Presentation demos SHALL allow turn input to scrub a Tween, transition, frame animation, or Timeline progress without requiring time playback.

#### Scenario: Knob reverses progress
- **WHEN** turn delta changes controlled progress from a larger to a smaller value
- **THEN** the visual state moves backward deterministically without completion side effects

### Requirement: Animation Gallery centralizes visual QA
The runtime SHALL register an Animation Gallery App containing discoverable pages for easing, Tween composition, Spring, transitions, selection feedback, camera effects, particles, sprite/state animation, and dither/pattern effects.

#### Scenario: Gallery is navigated
- **WHEN** the user rotates through Gallery pages and presses to change demo mode
- **THEN** every required effect can be previewed and its relevant progress or parameters are visible

### Requirement: Motion comfort is configurable
Effects SHALL consume a normal or reduced-motion profile that reduces shake, elastic overshoot, camera displacement, and particle density without disabling core navigation feedback.

#### Scenario: Reduced motion is enabled
- **WHEN** Settings selects quiet/reduced motion
- **THEN** Gallery and runtime effects use reduced amplitudes and density while transitions remain functional
