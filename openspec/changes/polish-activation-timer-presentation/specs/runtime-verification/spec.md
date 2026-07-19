## MODIFIED Requirements

### Requirement: Both display profiles have snapshots
Approved screenshot/framebuffer snapshots SHALL cover Launcher, Timer, Motion, Settings, Gallery, representative transitions, Timer state/alert feedback, Menu mask transition and representative proportional system-font layouts at 320×170 and 400×240. Timer numeral and Cover snapshots MUST NOT be accepted until the user explicitly approves their visual baselines.

#### Scenario: Snapshot regression runs
- **WHEN** deterministic fixed-step scenes render their capture frames
- **THEN** dimensions and framebuffer hashes match approved snapshots or fail with inspectable PNG output, and any unapproved Timer numeral or Cover candidate remains outside approved golden data

## ADDED Requirements

### Requirement: Timer migration and presentation are exhaustively verified
自动化验证 SHALL 覆盖产品 Clock 残留审计、双 profile 代表时间几何、Start/Pause/Resume 首帧中点末帧与清理、Alert 多相位/循环/Reduced Motion/Muted/确认抢占、Menu 中间帧全屏 mask 和 fully-open 像素一致性。

#### Scenario: 完整变更门禁
- **WHEN** 该 change 声明实现完成
- **THEN** host suite、strict warnings、sanitizer、SDL smoke/E2E、allocation/size audit、PlatformIO T-Embed 构建和 `git diff --check` 均有通过证据
