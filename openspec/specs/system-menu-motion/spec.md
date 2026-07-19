# System Menu Motion Specification

## Purpose
定义 System Menu 的逐扫描线运动、开合方向差异、Reduced Motion 时序以及双 profile 确定性呈现。

## Requirements

### Requirement: System Menu opens and closes with directional panel motion
System Menu SHALL enter from the right with bounded ease-out motion and SHALL exit fully to the right with bounded ease-in motion while the frozen App remains visible underneath. During motion, its leading edge SHALL be diagonally staggered by scanline and its content SHALL be temporarily compressed between that edge and the fixed right boundary rather than moving as a rigid rectangle.

#### Scenario: Menu opens
- **WHEN** a valid long-press opens System Menu
- **THEN** reveal progress advances monotonically from 0 to 1, the panel unfolds from a diagonally staggered compressed edge into its undistorted resting right-side bounds, and the App frame remains frozen

#### Scenario: Menu resumes the App
- **WHEN** Resume or the close gesture is accepted while System Menu is open
- **THEN** reveal progress decreases monotonically to 0, the panel compresses toward a diagonally staggered edge until fully off-screen, and the App resumes only after closing completes

### Requirement: Menu deformation is bounded and deterministic
The panel deformation SHALL use fixed-capacity integer scanline mapping with caller-owned scratch storage, SHALL render identical pixels for identical state, and SHALL be exactly absent at fully open progress.

#### Scenario: Stable Menu is rendered
- **WHEN** reveal progress is 1
- **THEN** the warped composition exactly matches the approved undistorted Menu panel layout at both display profiles

#### Scenario: Intermediate deformation is rendered repeatedly
- **WHEN** the same opening or closing progress is rendered twice over the same frozen App
- **THEN** framebuffer bytes match exactly, the right edge remains anchored, and no geometry or allocation diagnostic occurs

### Requirement: Closing retains surface ownership
During closing, System Menu SHALL remain the active composition and input owner but SHALL reject navigation and repeated actions. The close sound and diagnostic SHALL be emitted exactly once when closing begins.

#### Scenario: Input arrives while closing
- **WHEN** turn, click, long-press, or release input arrives before reveal progress reaches 0
- **THEN** the input is consumed without changing selection, reopening the menu, or updating the frozen App

#### Scenario: Closing completes
- **WHEN** reveal progress reaches 0
- **THEN** the interactive surface slot is released exactly once and the next ordinary input can reach the App

### Requirement: Home action does not stack Menu close and App return animations
Selecting Home SHALL release Menu composition before initiating the App-to-Launcher handoff, while preserving the accepted action's Back sound and frozen-frame continuity.

#### Scenario: Home is selected
- **WHEN** a non-Home App confirms the Home menu item
- **THEN** System Menu ceases composition and Runtime starts exactly one App return transition without waiting for or simultaneously rendering the Resume close animation

### Requirement: Reduced Motion keeps the same state semantics
Reduced Motion SHALL preserve opening, open, closing, and released states while using no greater duration or travel emphasis than Normal motion.

#### Scenario: Motion profile is reduced
- **WHEN** System Menu opens and closes under Reduced Motion
- **THEN** surface ownership, frozen App behavior, directional entry/exit, and completion events match Normal motion with reduced nonessential motion
