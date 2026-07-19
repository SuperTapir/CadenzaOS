## MODIFIED Requirements

### Requirement: AppRuntime has lifecycle and transition tests
Automated tests SHALL cover registration, initial entry, open guards, `onExit`/`onEnter` order, transition input freeze, System Menu input ownership, suspend-with-snapshot, transition defer, explicit Menu Home return, capacity diagnostics, and both framebuffer profiles.

#### Scenario: Runtime tests execute
- **WHEN** the runtime test target executes
- **THEN** it proves the opening and closing button sequences do not leak, Menu active freezes App update/render without exit/enter, system services continue, transition-time Menu requests open on the stable destination, and Home uses the configured normal Back transition
