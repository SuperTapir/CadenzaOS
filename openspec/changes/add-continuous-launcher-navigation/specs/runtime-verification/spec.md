## MODIFIED Requirements

### Requirement: Both display profiles have snapshots
Approved screenshot/framebuffer snapshots SHALL cover Launcher, Clock, Motion, Settings, Gallery, and representative transitions at 320×170 and 400×240. Launcher coverage SHALL include Vertical and Horizontal settled states, a deterministic navigation midpoint, every built-in App Cover, generic fallback, and press/launch representative captures whose Cover pixels match their neutral counterparts; interactive tests SHALL separately prove continuity, loop direction, retarget, Normal/Reduced Motion monotonicity and exact settling, Cover immutability, bounds safety and moving-click semantics rather than relying only on terminal hashes.

#### Scenario: Snapshot regression runs
- **WHEN** deterministic fixed-step scenes render their capture frames
- **THEN** dimensions and framebuffer hashes match approved snapshots or fail with inspectable PNG output, including both Launcher orientations, an animated midpoint, distinct built-in Covers, fallback and press/launch immutability representatives for each profile

#### Scenario: Launcher interaction regression runs
- **WHEN** automated Launcher cases exercise loop boundaries, repeated and reversed turns, both Motion profiles, orientation switching, and click before settle
- **THEN** logical selection, continuous visual position, immutable Cover pixels, opened App, command count, geometry/text diagnostics, and final stable frames match the launcher-card-navigation contract

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
