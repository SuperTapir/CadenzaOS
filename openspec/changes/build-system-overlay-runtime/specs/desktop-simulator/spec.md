## MODIFIED Requirements

### Requirement: Desktop input maps to raw device semantics
Mouse wheel and Left/Right keys SHALL emit turn events, while Space and Enter SHALL emit button events that pass through the shared InputReducer and System Overlay input router.

#### Scenario: Keyboard enters an App
- **WHEN** the user rotates selection with an arrow key and taps Enter
- **THEN** the same click semantics used by T-Embed open the selected App

#### Scenario: Desktop long press opens System Menu
- **WHEN** Space or Enter is held past the configured long-press threshold in any stable App
- **THEN** the runtime opens the same system-owned Menu used on T-Embed without immediately changing current App

#### Scenario: Desktop Menu returns home
- **WHEN** scripted desktop input selects Home in System Menu from a non-Home App
- **THEN** the runtime closes Menu and returns to the configured Home App through the shared transition path

### Requirement: Existing Apps complete the desktop loop
Launcher, Timer, Motion, Settings, and Gallery SHALL open, receive input, render, switch, open/close System Menu, and return home through its explicit Home action in the macOS simulator.

#### Scenario: Full navigation smoke path runs
- **WHEN** scripted inputs visit every registered App and use System Menu Home to return to Launcher
- **THEN** no platform-specific alternate UI, App implementation, or direct long-press Home shortcut is invoked
