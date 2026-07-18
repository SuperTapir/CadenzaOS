## ADDED Requirements

### Requirement: Apps can provide an independent launch sequence
An App SHALL be able to provide an optional const full-screen launch-frame renderer driven only by normalized progress. The Runtime MAY call it before `onEnter`; rendering a launch frame MUST NOT mutate App state, Cover pixels, lifecycle, catalog, or system services.

#### Scenario: Runtime samples a launch sequence
- **WHEN** a registered App is launched from Home and implements the launch renderer
- **THEN** the Runtime can render any progress in `[0, 1]` deterministically before `onEnter` without delivering an App update

#### Scenario: Progress is sampled repeatedly
- **WHEN** the same App, profile, and normalized progress are rendered more than once
- **THEN** the resulting framebuffer bytes are identical and no allocation or diagnostic failure occurs

### Requirement: Built-in Apps have distinct launch identities
Clock, Motion, Settings, and Animation Gallery SHALL each provide a visually distinct code-native launch sequence whose first phase continues its static Cover identity and whose final frame can hand off to the App first frame.

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
