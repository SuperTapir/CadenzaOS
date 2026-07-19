## MODIFIED Requirements

### Requirement: Runtime transitions are selectable strategies
AppRuntime SHALL accept a direct or staged transition strategy and duration without requiring Apps to know transition details. The default Launcher/App route SHALL select directional staged Cover handoffs, while venetian blinds and the existing direct transition set SHALL remain selectable rather than hard-coded runtime drawing.

#### Scenario: Direct transition is selected
- **WHEN** an App is opened with a specified direct transition
- **THEN** that strategy composes captured outgoing and incoming frames while lifecycle ordering remains correct

#### Scenario: Default Launcher handoff is selected
- **WHEN** the default Runtime opens an App from Launcher or returns from an App to Launcher
- **THEN** it selects the corresponding staged Cover handoff and preserves the same lifecycle ordering contract
