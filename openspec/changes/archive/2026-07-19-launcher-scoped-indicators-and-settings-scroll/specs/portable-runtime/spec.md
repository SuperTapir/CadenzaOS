## MODIFIED Requirements

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
