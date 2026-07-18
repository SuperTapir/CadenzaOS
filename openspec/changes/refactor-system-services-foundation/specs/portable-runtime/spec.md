## MODIFIED Requirements

### Requirement: Core is standard C++
Application contracts, runtime, input semantics, graphics, and animation headers and sources SHALL compile as C++17 without bundled App implementations or Arduino, ESP32, TFT_eSPI, SDL, Wi-Fi, Bluetooth, I²S, TinyUSB, or USB Audio headers; system services SHALL remain a separately linkable portable layer.

#### Scenario: Host compiles the core alone
- **WHEN** the host build compiles and links the core target without bundled Apps, system implementations, or platform adapters
- **THEN** it succeeds without defining platform compatibility shims

#### Scenario: Source boundary is audited
- **WHEN** core include and source manifests are searched
- **THEN** bundled App/Gallery implementations and platform service headers are absent

### Requirement: Platform dependencies are adapters
Hardware time, raw input, logging, memory metrics, framebuffer presentation, sound output, microphone input, USB, Wi-Fi, and Bluetooth SHALL enter portable layers through explicit adapters or injected values; platform callbacks SHALL not directly invoke App lifecycle or mutate Runtime state. T-Embed, desktop, headless, and future Sharp presentation SHALL not change App behavior contracts.

#### Scenario: Platform headers are audited
- **WHEN** core and portable system include/source trees are searched for platform APIs
- **THEN** direct `Serial`, GPIO, `millis`, `micros`, TFT_eSPI, SDL, ESP-IDF, I²S, TinyUSB, Wi-Fi, and Bluetooth usage is absent

#### Scenario: Asynchronous platform event arrives
- **WHEN** a platform adapter receives a callback outside the main frame
- **THEN** it marshals the event through its bounded adapter boundary for later system ingestion and does not call an App

### Requirement: Application lifecycle is platform-independent
Every App registered by a composition root SHALL implement the same static lifecycle and narrow update/render context contract on firmware, headless host, and desktop simulator; the core SHALL not enumerate built-in App types, and system navigation SHALL use the configured Home AppId.

#### Scenario: App is switched
- **WHEN** the runtime opens any registered AppId and completes its transition
- **THEN** the outgoing App receives `onExit`, the incoming App receives `onEnter`, and only the active App receives normal updates

#### Scenario: System return gesture is used
- **WHEN** a non-Home App receives long-press
- **THEN** the runtime initiates a return to the composition root's configured Home App without App-specific hardware handling

#### Scenario: Core-unknown App is registered
- **WHEN** a composition root registers a legal AppId not declared by core
- **THEN** the App can participate in lifecycle, update, render, and navigation without changing core sources
