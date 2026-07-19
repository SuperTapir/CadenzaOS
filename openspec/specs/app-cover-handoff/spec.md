# App Cover Handoff Specification

## Purpose
定义 Launcher Cover 与 App 首帧之间连续、确定、无分配的交接过程及返回 Launcher 时的像素连续性契约。

## Requirements

### Requirement: App switches preserve static Cover identity
The default system transition between Launcher and an App SHALL preserve the App's static Cover identity without passing interaction, time, launch, or lifecycle state to the Cover renderer. A launch sequence SHALL be a separate renderer and returning to Launcher SHALL use a static Cover bridge.

#### Scenario: Launcher opens an App with a Cover
- **WHEN** Launcher opens a registered App that renders a Cover
- **THEN** the transition preserves the selected Cover identity into the App's independent launch sequence without changing the approved Cover pixels

#### Scenario: App has no Cover
- **WHEN** Launcher opens or returns from a registered App whose Cover renderer declines to draw
- **THEN** the bridge uses a deterministic bounded fallback and completes without invalid or clipped geometry diagnostics

### Requirement: Handoff is staged around the lifecycle midpoint
A staged handoff SHALL compose `source -> launch-or-bridge frame -> destination`, perform exactly one outgoing `onExit` followed by incoming `onEnter` at the midpoint, and SHALL NOT deliver normal App update input until the destination is stable.

#### Scenario: Transition crosses its midpoint
- **WHEN** a staged transition advances from immediately before to at or after progress 0.5
- **THEN** the last pre-midpoint frame and first post-midpoint frame meet at the same bridge state while lifecycle callbacks occur once in exit-before-enter order

#### Scenario: Input arrives during handoff
- **WHEN** turn, click, or held input is present before the transition completes
- **THEN** neither outgoing nor incoming App receives a normal update from that input

### Requirement: Handoff reuses two transition buffers
The Runtime SHALL create and preserve the final launch or bridge frame using the existing outgoing and incoming transition framebuffers and SHALL NOT add a third full-screen transition framebuffer or perform runtime heap allocation.

#### Scenario: Midpoint buffer ownership changes
- **WHEN** a staged transition reaches its lifecycle midpoint
- **THEN** the bridge becomes the outgoing phase source before the destination first frame replaces the incoming buffer

### Requirement: Enter and return have directional timing
Normal motion SHALL use a longer Launcher-to-App handoff that presents the App launch sequence and a shorter App-to-Launcher handoff that resolves directly into the App Cover and Launcher chrome. Reduced Motion SHALL preserve the identity ordering while reducing nonessential waiting and displacement.

#### Scenario: Launcher opens an App
- **WHEN** the default Normal-motion transition starts from Home and targets a visible App
- **THEN** Launcher chrome recedes into the App's independent launch sequence and its stable final frame resolves through ordered dither into the App first frame

#### Scenario: App returns to Launcher
- **WHEN** the default transition targets Home from an App
- **THEN** the App frame resolves to that App's Cover bridge and Launcher chrome returns without replaying the launch loading interval

#### Scenario: Return preserves shared Cover geometry and frame cadence
- **WHEN** each built-in App returns at fixed 30 FPS on either framebuffer profile
- **THEN** the transition starts at the exact outgoing App frame, keeps the selected Cover pixels fixed at the Launcher's content rectangle from the lifecycle midpoint onward, ends at a stable Launcher frame, and changes no more than 16% of framebuffer pixels between adjacent samples

#### Scenario: Reduced Motion is active
- **WHEN** either directional handoff begins under Reduced Motion
- **THEN** source, bridge, and destination remain visually ordered while pure wait time is no greater than under Normal motion

### Requirement: Existing direct transitions remain compatible
Transitions that do not declare staged bridge behavior SHALL retain the existing direct outgoing/incoming capture, endpoint, lifecycle, and custom-duration semantics.

#### Scenario: Caller injects a direct transition
- **WHEN** AppRuntime is constructed with or configured to use Cut, Venetian Blinds, Dip, Wipe, Iris, Dissolve, or another direct strategy
- **THEN** the Runtime does not replace its incoming source with a Cover bridge and the strategy receives the same direct frame model as before this change
