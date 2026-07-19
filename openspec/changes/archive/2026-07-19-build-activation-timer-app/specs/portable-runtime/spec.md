## MODIFIED Requirements

### Requirement: Simulation time is controllable
The runtime SHALL update applications and animations only from an injected presentation/simulation delta, while system deadline services SHALL use a separately injected nondecreasing monotonic time; pausing or clamping presentation delta SHALL not silently pause or shorten a running Timer.

#### Scenario: Simulation is paused
- **WHEN** monotonic time and input polling continue while presentation simulation delta is zero
- **THEN** App and animation state do not advance while a Running Timer remaining value continues to follow its monotonic deadline

#### Scenario: Long frame is clamped for presentation
- **WHEN** one platform frame stalls for 2 seconds and presentation delta is clamped to 50 ms
- **THEN** animation receives 50 ms while TimerService observes the full monotonic advance and does not gain 1.95 seconds

### Requirement: Application lifecycle is platform-independent
Launcher, Timer, Motion, Settings, and Gallery SHALL implement the same static App lifecycle and registry contract on firmware, headless host, and desktop simulator; System Menu SHALL suspend the active App without changing AppId, while system-owned Timer state continues independently.

#### Scenario: App is switched
- **WHEN** the runtime opens a registered App and completes its transition
- **THEN** the outgoing App receives `onExit`, the incoming App receives `onEnter`, and only the active App receives normal updates

#### Scenario: System Menu gesture is used
- **WHEN** any stable App receives long-press
- **THEN** the runtime opens the system-owned Menu over that App without changing current AppId or emitting App exit/enter

#### Scenario: Timer owner leaves foreground
- **WHEN** Timer App starts a Timer and transitions to Launcher or another App
- **THEN** Timer App stops receiving normal updates but the Timer service remains owned, visible through system snapshot/indicator and continues toward its deadline
