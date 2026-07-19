# runtime-verification Specification

## Purpose
定义 core、headless、desktop 与 firmware 的分层验证、golden/snapshot、共享源码审计、性能预算和实体设备声明规则。
## Requirements
### Requirement: Development follows a TDD gate
Every behavior introduced by this change SHALL first have an automated test or executable failing case that demonstrates the missing contract, followed by implementation and relevant regression verification.

#### Scenario: A capability task is completed
- **WHEN** an implementation task is marked complete
- **THEN** its associated test is present, was capable of failing before implementation, and passes after implementation

### Requirement: App lifecycle is tested
Automated tests SHALL cover registration, initial entry, open guards, `onExit`/`onEnter` order, transition input freeze, and system long-press return.

#### Scenario: Lifecycle suite runs
- **WHEN** the host test suite executes lifecycle cases
- **THEN** all lifecycle calls and active-App updates match the portable-runtime contract

### Requirement: Input state machine is tested
Automated tests SHALL cover debounce, short press, long press, release, repeated samples, turn direction, saturation, and frame reset behavior without GPIO.

#### Scenario: Boundary timing is tested
- **WHEN** release occurs immediately before, at, and after the long-press threshold
- **THEN** click and long-press outputs match the defined inclusive boundary

### Requirement: Transitions are tested

Automated tests SHALL cover every direct transition endpoint, representative midpoint, lifecycle swap, interruption guard, and source/destination immutability. Staged handoffs SHALL additionally cover pre-midpoint App launch-frame or Cover-bridge composition, exact continuity across the midpoint buffer swap, post-midpoint destination composition, direction routing, per-App distinction, fallback, motion profiles, 30 FPS adjacent-frame change bounds, fixed return Cover geometry, and the absence of a third full-screen buffer. System Menu tests SHALL cover monotonic open/close motion, closing input ownership, frozen App lifetime, one-shot sound/diagnostics, and release after full exit.

#### Scenario: Transition suite runs
- **WHEN** direct and staged transitions are rendered from known source, bridge, and destination patterns
- **THEN** golden endpoint, phase, midpoint-continuity, and direction results match for both profiles where resolution affects geometry

### Requirement: Tween numerical behavior is tested
Automated tests SHALL cover every easing endpoint/interior sample, Tween start/mid/end, delay, completion, seek, Sequence, Parallel, and Timeline offsets.

#### Scenario: Tween golden vectors run
- **WHEN** the animation numerical suite executes
- **THEN** all values and state transitions match exact or epsilon-appropriate expectations

### Requirement: Reverse and repeat behavior is tested
Automated tests SHALL cover reverse playback, yoyo, repeat count, repeat delay, boundary crossing, and infinite-repeat safety.

#### Scenario: Repeat boundary is crossed with a large delta
- **WHEN** one update spans multiple repeat boundaries
- **THEN** final value, repeat state, and callback count match the defined traversal semantics

### Requirement: Both display profiles have snapshots

Approved screenshot/framebuffer snapshots SHALL cover Launcher, Clock, Motion, Settings, Gallery, system menu/overlay surfaces, and representative transitions at 320×170 and 400×240. Typography migrations SHALL update the full matrix rather than a single representative screen, and failures SHALL emit inspectable lossless PNG output.

#### Scenario: Snapshot regression runs
- **WHEN** deterministic fixed-step scenes render their capture frames after the typography migration
- **THEN** dimensions and framebuffer hashes match approved snapshots or fail with inspectable PNG output for the exact App, surface and profile

#### Scenario: Launcher interaction regression runs
- **WHEN** automated Launcher cases exercise loop boundaries, repeated and reversed turns, both Motion profiles, orientation switching, and click before settle
- **THEN** logical selection, continuous visual position, immutable Cover pixels, opened App, command count, geometry/text diagnostics, and final stable frames match the launcher-card-navigation contract

### Requirement: Simulator tests require no hardware
All unit, integration, deterministic replay, screenshot, and desktop smoke tests SHALL run without a connected T-Embed or physical display.

#### Scenario: Clean host executes tests
- **WHEN** tests run on a macOS host with build dependencies but no serial or GPIO device
- **THEN** the complete P7 host suite passes

### Requirement: Build matrix is verified
Completion SHALL require successful host tests, SDL3 desktop build, desktop smoke/E2E, and PlatformIO T-Embed compile, plus `git diff --check` or equivalent whitespace validation.

#### Scenario: Change completion is audited
- **WHEN** P0–P7 are claimed complete
- **THEN** fresh command output proves every required build and test gate and lists P8 as the only intentionally unexecuted phase

### Requirement: 新版 Cover 的来源、端点与固件成本可验证
验证套件 SHALL 覆盖三张批准母图的双 profile 来源复现、PBM/header 一致性、Launcher 静态交互、launch p0、return bridge、snapshots、完整 build matrix 与 firmware size gate；TIMER 资产与 hash SHALL 保持原批准值。

#### Scenario: 三组批准资产进入集成
- **WHEN** 新 source、PBM 与 headers 纳入构建
- **THEN** 资产、immutability、handoff、snapshot、host/desktop/firmware-compatible build 与 size gate 全部通过

#### Scenario: 派生资产发生漂移
- **WHEN** PBM 无法由记录母图复现或 header 与 canonical PBM 不一致
- **THEN** 验证指出具体 App/profile/文件并失败，不允许用刷新 snapshot 掩盖错误

### Requirement: Desktop 可近似反射屏而不改变 1-bit 真值
Mac SDL 模拟器 SHALL 默认提供基于 Playdate 1.12.3 参考的暖灰 reflective ink/paper 呈现，并 SHALL 提供显式 pure 黑白模式。palette 转换 SHALL 只发生在 SDL texture 呈现边界，不得改变 MonoFramebuffer、截图/录制、hash、headless 输出或 firmware。

#### Scenario: 默认启动 Desktop
- **WHEN** 用户未指定 palette 启动任一 framebuffer profile
- **THEN** SDL 窗口将黑白像素分别显示为 `#322F28` 与 `#B1AEA7`

#### Scenario: 检查纯 1-bit 对比
- **WHEN** 用户以 `--palette pure` 启动 Desktop
- **THEN** SDL 窗口使用 `#000000` 与 `#FFFFFF`，而两种 palette 下 canonical framebuffer hash 相同

#### Scenario: 传入未知 palette
- **WHEN** 用户传入不支持的 palette 名称
- **THEN** Desktop 拒绝启动并显示包含受支持值的 usage

#### Scenario: 在 Retina Mac 启动 Desktop
- **WHEN** SDL/macOS 提供 2x backing scale
- **THEN** 窗口请求 high-pixel-density surface，日志报告逻辑尺寸、2x 实际像素尺寸与 `2.00x` density，同时 framebuffer 仍按整数最近邻呈现

### Requirement: Cover 目标尺寸必须直接源自母图
Sharp 350×155 与 T-Embed 280×124 Cover SHALL 分别从同一高分辨率 PNG 母图直接 LANCZOS fit 并阈值化，不得从已 1-bit 化的另一个 profile 资产二次缩放。转换参数 SHALL 可复现，运行时仍 SHALL 只 blit 目标尺寸 packed bitmap。

#### Scenario: 检查 Cover 来源
- **WHEN** 对任一内置 Cover 运行母图来源检查
- **THEN** 两个 profile 的 PBM 像素分别与其记录的目标尺寸和阈值直接转换结果完全一致

### Requirement: Timer 状态机和时间边界先由失败测试锁定
自动化 SHALL 在实现前覆盖 Ready/Running/Paused/Expired、0/1/99 分钟选择与服务边界、非法命令、capability/owner、deadline 前/等于/之后、timestamp regression、large step、generation 和 zero-allocation。

#### Scenario: Deadline inclusive boundary
- **WHEN** host 分别推进到 deadline 前 1 ms、恰好 deadline 和 deadline 后 1 ms
- **THEN** 只有后两者为 Expired，expiration edge/generation/cue 次数与规格一致

### Requirement: Timer 后台与 alert 输入链路有可执行 E2E
Headless/desktop E2E SHALL 从 Launcher 打开 Timer、设定并开始、打开 System Menu或返回 Launcher、跨过 deadline、确认 alert、再次启动，并 SHALL 覆盖 alert 出现在 held button 与 App transition 中的 trace。

#### Scenario: 完整十分钟流程的缩时重放
- **WHEN** deterministic host 以缩时 monotonic trace 执行 Launcher→Timer→Start→Home→Expire→Acknowledge→Timer→Start
- **THEN** Timer 在后台连续、alert 抢占和恢复正确、最近时长保留、current App/lifecycle/input owner/声音序列全部匹配规格

### Requirement: Timer 变更通过完整构建与审计门禁
完成 SHALL 需要相关 unit/integration/snapshot/PCM/headless/desktop 测试、strict warnings、sanitizer、shared-source/platform-header/allocation audit、SDL build/smoke、PlatformIO T-Embed compile、size report 与 `git diff --check`。

#### Scenario: 完成审计
- **WHEN** Timer App 被声明完成
- **THEN** 新鲜命令输出证明所有软件门禁通过，并明确列出只能在实体 T-Embed 执行的旋钮手感、响度和长时间运行验收

### Requirement: Timer migration and presentation are exhaustively verified
自动化验证 SHALL 覆盖产品 Clock 残留审计、双 profile 代表时间几何、Start/Pause/Resume 首帧中点末帧与清理、Alert 多相位/循环/Reduced Motion/Muted/确认抢占、Menu 中间帧全屏 mask 和 fully-open 像素一致性。

#### Scenario: 完整变更门禁
- **WHEN** 该 change 声明实现完成
- **THEN** host suite、strict warnings、sanitizer、SDL smoke/E2E、allocation/size audit、PlatformIO T-Embed 构建和 `git diff --check` 均有通过证据

### Requirement: Typography visual regression must be reviewed as a complete matrix
Completion SHALL produce a deterministic contact sheet containing every built-in App, business Display typography and representative system surface at both display profiles. The review SHALL record whether each frame has accidental text clipping, overlap, illegible thin strokes, inconsistent hierarchy, excessive density, incorrect business-font replacement, or incorrect selected-row inversion; any observed severe defect SHALL be fixed before approval.

#### Scenario: Final typography baseline is audited
- **WHEN** the implementation is proposed as visually complete
- **THEN** an inspectable contact sheet and review record cover the full matrix, with no unresolved severe typography defect

### Requirement: Font flash cost must be explicit
The build SHALL report the compiled bytes for each font descriptor and their total, and SHALL enforce a documented typography flash budget without allocating runtime heap or additional framebuffer storage.

#### Scenario: Font assets are regenerated
- **WHEN** the deterministic font conversion runs
- **THEN** its manifest reports per-role bitmap, glyph metadata and kerning bytes plus the total used by all unique font resources
