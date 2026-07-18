## 1. Research and Build Foundation

- [x] 1.1 Record reference-project commit SHAs, licenses, reusable ideas, rejected scope, and asset provenance rules in a research note
- [x] 1.2 Run reproducible LVGL/U8g2/Tweeny/SDL adoption spikes and record the measured adopt/wrap/reject boundary
- [x] 1.3 Add CMake core/test/desktop targets and documented macOS bootstrap for CMake and SDL3
- [x] 1.4 Integrate a pinned, source/font-curated U8g2 subset and vendor pinned lightweight test, PNG, and GIF writers with third-party notices and license verification
- [x] 1.5 Add a failing host compile test that proves current core headers depend on Arduino/TFT types
- [x] 1.6 Establish a red-green-refactor command loop for focused tests, full host tests, desktop build, firmware build, and diff checks

## 2. Portable Core Services and Input

- [x] 2.1 Write host tests for platform-free core headers, no-op/recording diagnostics, and injected simulation time
- [x] 2.2 Implement standard C++ core types, diagnostic sink, and SimulationClock without platform includes
- [x] 2.3 Write InputReducer tests for debounce, short/long press boundaries, release, repeated samples, held duration, turn sign, saturation, and frame reset
- [x] 2.4 Implement RawInputEvent, InputReducer, InputFrame, and explicit input configuration without allocation
- [x] 2.5 Write adapter contract tests using fake raw-input and monotonic-time sources
- [x] 2.6 Split T-Embed GPIO sampling from shared input reduction and route Serial through the diagnostic adapter

## 3. Canonical Framebuffer and Rasterizer

- [x] 3.1 Write guarded-buffer tests for dimensions, stride, MSB-first bit addressing, clear, and out-of-bounds safety
- [x] 3.2 Implement fixed-capacity MonoFramebuffer for 320×170 and 400×240 profiles
- [x] 3.3 Write exact golden tests for pixel, line, rect, fillRect, circle, fillCircle, clipping, and invalid geometry diagnostics
- [x] 3.4 Implement deterministic MonoCanvas contract and half-open clip/diagnostic wrapper over the curated U8g2 raster subset
- [x] 3.5 Select or author a legally reusable bitmap font, record provenance, and write glyph metric/alignment/integer-scale golden tests
- [x] 3.6 Integrate approved U8g2 font data behind platform-independent text measurement, alignment, baseline, and rasterization APIs
- [x] 3.7 Write bitmap composition, source clipping, translation, flip, XOR/inversion, and overlap tests
- [x] 3.8 Implement BitmapView, sprite drawing, transforms, composition modes, and region inversion
- [x] 3.9 Write fixed-capacity SpriteAtlas lookup/capacity tests and implement atlas descriptors plus the offline conversion entry point
- [x] 3.10 Write dither coverage/phase determinism tests and implement pattern/dither fills with explicit animated phase
- [x] 3.11 Write source/destination immutability tests and implement first-class off-screen framebuffer/canvas usage

## 4. Runtime and Firmware Migration

- [x] 4.1 Write fake-App tests for registration, initial entry, open guards, lifecycle order, active update, input freeze, and long-press return
- [x] 4.2 Migrate App/AppRuntime to the portable core and structured diagnostics while preserving lifecycle behavior
- [x] 4.3 Write transition capture tests proving outgoing/incoming frame ownership and exact endpoint output
- [x] 4.4 Implement transition strategy injection and dual off-screen App capture with a temporary cut/blinds strategy
- [x] 4.5 Migrate Launcher, Clock, Motion, and Settings away from Arduino/TFT APIs and onto the software canvas/font
- [x] 4.6 Write presenter contract tests and replace TFT-owned drawing with a framebuffer-only TftPresenter
- [x] 4.7 Rebuild the T-Embed target and remove superseded Arduino-coupled core/canvas code only after the new firmware path compiles

## 5. Headless Host and Baseline Verification

- [x] 5.1 Write deterministic runner tests for fixed dt, scripted input, replay equality, and framebuffer hashing
- [x] 5.2 Implement HeadlessHost, App composition root, deterministic scripts, and inspectable diagnostics
- [x] 5.3 Write and approve initial 320×170 and 400×240 snapshots for Launcher, Clock, Motion, and Settings
- [x] 5.4 Add a build-source audit proving host and firmware compile the same core/App modules

## 6. SDL3 Desktop Simulator MVP

- [x] 6.1 Write isolated tests for profile selection, integer-scale validation, raw key/wheel/button mapping, and overlay separation
- [x] 6.2 Implement SDL3 window, nearest-neighbor framebuffer texture presentation, 320×170/400×240 profiles, and 1×–4× scale
- [x] 6.3 Implement mouse-wheel/arrow turn and Space/Enter press/release adapters through InputReducer
- [x] 6.4 Implement non-authoritative FPS, frame-time, current-App, and recent-input overlay
- [x] 6.5 Add a desktop smoke test that visits Launcher, Clock, Motion, and Settings and returns home without alternate UI code

## 7. Simulator Debugging and Capture

- [x] 7.1 Write SimulationController tests for pause/resume, exact single-step, 0.25×/0.5×/1×/2× speed, and fixed/real-step switching
- [x] 7.2 Implement debugger controls and ensure wall-time input continues while simulation is paused
- [x] 7.3 Write PNG encoder/output tests for exact dimensions, colors, naming metadata, and decode round-trip
- [x] 7.4 Implement lossless PNG screenshots from the canonical framebuffer
- [x] 7.5 Write recording tests for frame ordering, pause behavior, stop/finalize, and partial cleanup
- [x] 7.6 Implement PNG frame-sequence recording and convenience GIF recording
- [x] 7.7 Implement debug HUD fields for App, InputFrame, simulation state, capacities/overflows, heap estimate, FPS, and frame time
- [x] 7.8 Write device-preview invariance tests and implement an optional frame that preserves framebuffer hashes and converted input coordinates
- [x] 7.9 Add inspectable canvas/atlas/pool misuse diagnostics to the simulator and tests

## 8. Animation Core

- [x] 8.1 Write exact endpoint and golden interior-vector tests for all required easing functions
- [x] 8.2 Implement normalized Easing functions with clamped input and exact endpoints
- [x] 8.3 Write typed Tween tests for update, value, reset, state, progress query, seek, and large-delta boundary crossing
- [x] 8.4 Implement allocation-free Tween<T> with delay, repeat delay/count, yoyo/reverse, and non-allocating completion callback
- [x] 8.5 Write callback tests proving one-shot traversal behavior and side-effect-free seek
- [x] 8.6 Write Sequence local-time/seek/capacity tests and implement fixed-capacity non-owning Sequence
- [x] 8.7 Write Parallel duration/local-time/seek/capacity tests and implement fixed-capacity non-owning Parallel
- [x] 8.8 Write overlapping-offset Timeline update/reverse/repeat/seek/capacity tests and implement fixed-capacity Timeline
- [x] 8.9 Write Spring overshoot/settling/stability/large-delta tests and implement bounded fixed-substep Spring
- [x] 8.10 Compile golden animation vectors for host and ESP32-compatible targets and document numeric tolerances

## 9. Transition and Motion Presentation

- [x] 9.1 Write exact endpoint, representative midpoint, geometry, and immutability tests for every required transition at both profiles
- [x] 9.2 Implement Dip/Fade and horizontal wipe transition strategies
- [x] 9.3 Implement diagonal wipe, iris, and configurable venetian-blinds transition strategies
- [x] 9.4 Implement checker/dither dissolve with deterministic coverage and phase
- [x] 9.5 Replace the temporary runtime transition with selectable strategies and preserve lifecycle/input tests
- [x] 9.6 Write interruption/reduced-motion tests and implement reusable selection overshoot/pulse feedback
- [x] 9.7 Write deterministic seed, bounded amplitude, decay, and neutral-end tests for camera effects
- [x] 9.8 Implement camera punch and deterministic camera shake with normal/reduced-motion profiles

## 10. Particles and Sprite Animation

- [x] 10.1 Write particle spawn/update/expiry/seed/full-pool-policy/overflow tests
- [x] 10.2 Implement fixed-capacity particle pool, emitters, primitive/sprite rendering, and deterministic PRNG
- [x] 10.3 Write once/loop/ping-pong frame animation and large-delta tests
- [x] 10.4 Implement atlas-backed frame animation with query and seek support
- [x] 10.5 Write fixed-capacity state/trigger/reset/invalid-transition tests
- [x] 10.6 Implement animation state machine with explicit transition results and diagnostics

## 11. Animation Gallery and Visual QA

- [x] 11.1 Write Gallery lifecycle/navigation/page-registry and knob-scrub tests
- [x] 11.2 Implement Gallery pages for easing, Tween composition, Spring, and all transitions
- [x] 11.3 Implement Gallery pages for selection feedback, camera effects, particles, sprite/state animation, and dither/patterns
- [x] 11.4 Add paused progress scrubbing controls that seek Gallery Playables without completion side effects
- [x] 11.5 Connect Settings motion mode to normal/reduced-motion effect profiles and test propagation
- [x] 11.6 Perform desktop visual QA at both profiles, centralize comfortable duration/amplitude defaults, and record accepted trade-offs
- [x] 11.7 Approve Gallery and representative effect snapshots at 320×170 and 400×240

## 12. P7 Full Regression and Handoff

- [ ] 12.1 Complete and run the full App lifecycle, input, transition, Tween, reverse/repeat, graph, and debugging unit/integration suites
- [ ] 12.2 Run deterministic replay and all approved 320×170/400×240 framebuffer snapshots with inspectable PNG failure artifacts
- [ ] 12.3 Run the macOS SDL3 E2E path for every App, long-press return, pause/single-step, screenshot, recording, and progress scrub
- [ ] 12.4 Run a clean PlatformIO T-Embed compile and confirm host/simulator tests require no attached hardware
- [ ] 12.5 Run warnings-as-errors where supported, sanitizer host tests, `git diff --check`, and repository Arduino/platform-coupling audits
- [ ] 12.6 Update README, architecture, controls, build/test instructions, research references, third-party notices, memory budgets, and explicit P8 deferrals
- [ ] 12.7 Audit every P0–P7 requirement against authoritative test/build/runtime evidence and leave only P8 unexecuted
