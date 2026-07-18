## ADDED Requirements

### Requirement: Required easing functions are available
The animation core SHALL implement Linear, In/Out/InOut Quad, InOut Cubic, Out Expo, Out Back, Out Bounce, and Out Elastic through a common normalized easing interface.

#### Scenario: Easing endpoints are evaluated
- **WHEN** every built-in easing is evaluated at progress 0 and 1
- **THEN** it returns exact endpoints 0 and 1

#### Scenario: Representative easing values are evaluated
- **WHEN** built-in easing functions are sampled at documented interior points
- **THEN** their values match approved formulas within the configured float tolerance

### Requirement: Typed Tween controls scalar values
`Tween<T>` SHALL interpolate supported arithmetic scalar types and SHALL expose current value, duration, state, normalized progress, update, reset, and seek.

#### Scenario: Tween reaches midpoint
- **WHEN** a linear float Tween from 10 to 20 is sought to 0.5
- **THEN** its value is 15 within tolerance

### Requirement: Tween timing modifiers are composable
Tween SHALL support start delay, repeat delay, finite or infinite repeat, and yoyo/reverse playback with defined boundary semantics.

#### Scenario: Delayed yoyo repeat runs
- **WHEN** a Tween with delay, one repeat, and yoyo advances across its full duration
- **THEN** it holds the start during delay, reaches the end, reverses, and finishes at the start

### Requirement: Completion callbacks are safe
Tween and composite animations SHALL support a non-allocating completion callback that fires once when forward playback crosses its terminal boundary; pure seek SHALL not invoke side effects by default.

#### Scenario: Completion boundary is crossed repeatedly
- **WHEN** updates remain beyond a completed Tween boundary
- **THEN** its completion callback has fired exactly once until reset or a new traversal

### Requirement: Sequences play children serially
Sequence SHALL compose fixed-capacity non-owning Playables so each child receives its local time after prior children complete.

#### Scenario: Sequence is sought
- **WHEN** a two-child Sequence is sought into the second child
- **THEN** the first child is at its terminal state and the second reflects its local progress

### Requirement: Parallel groups play children concurrently
Parallel SHALL compose fixed-capacity non-owning Playables and define total duration as the longest child duration.

#### Scenario: Unequal parallel children finish
- **WHEN** a short and long child run in Parallel to the group end
- **THEN** both are terminal and the group duration equals the long child duration

### Requirement: Timeline places animations by offset
Timeline SHALL place fixed-capacity Playables at absolute start offsets and SHALL support update, pause, reverse, total duration, normalized progress query, and seek.

#### Scenario: Overlapping timeline entries are sought
- **WHEN** progress enters a time range containing two entries
- **THEN** both entries receive the correct local progress derived from their offsets

### Requirement: Spring is physically integrated
Spring SHALL animate a scalar toward a target using configurable stiffness, damping, mass, velocity, rest thresholds, and bounded fixed substeps.

#### Scenario: Underdamped spring settles
- **WHEN** an underdamped Spring is stepped until its rest thresholds are met
- **THEN** it overshoots, converges to the target, and reports settled without numerical divergence

### Requirement: Animation capacity is predictable
Animation compositions SHALL use compile-time or explicitly supplied fixed capacity, SHALL perform no runtime heap allocation, and SHALL return failure without mutation when capacity is exceeded.

#### Scenario: Timeline is full
- **WHEN** one more entry is added to a full Timeline
- **THEN** the add operation fails, existing entries remain intact, and diagnostics record overflow

### Requirement: Host and firmware animation semantics agree
The same animation sources SHALL compile on host and ESP32, with endpoints exact and representative samples equivalent within documented numeric tolerance.

#### Scenario: Golden animation vectors run
- **WHEN** the platform-independent animation vector suite runs in host tests and firmware-build-compatible compilation
- **THEN** no platform branches alter easing, timing, repeat, or seek semantics

