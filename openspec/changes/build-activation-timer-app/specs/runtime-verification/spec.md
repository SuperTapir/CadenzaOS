## MODIFIED Requirements

### Requirement: Both display profiles have snapshots
Approved screenshot/framebuffer snapshots SHALL cover Launcher, Clock/Activation Timer Ready/Running/Paused, Motion, Settings, Gallery, Timer background indicator, TimerAlert, and representative transitions at 320×170 and 400×240.

#### Scenario: Snapshot regression runs
- **WHEN** deterministic fixed-step scenes render their capture frames
- **THEN** dimensions and framebuffer hashes match approved snapshots or fail with inspectable PNG output, including monotonic time-mass decrease and alert bounds

## ADDED Requirements

### Requirement: Timer 状态机和时间边界先由失败测试锁定
自动化 SHALL 在实现前覆盖 Ready/Running/Paused/Expired、1/60 分钟边界、非法命令、capability/owner、deadline 前/等于/之后、timestamp regression、large step、generation 和 zero-allocation。

#### Scenario: Deadline inclusive boundary
- **WHEN** host 分别推进到 deadline 前 1 ms、恰好 deadline 和 deadline 后 1 ms
- **THEN** 只有后两者为 Expired，expiration edge/generation/cue 次数与规格一致

### Requirement: Timer 后台与 alert 输入链路有可执行 E2E
Headless/desktop E2E SHALL 从 Launcher 打开 Clock、设定并开始、打开 System Menu或返回 Launcher、跨过 deadline、确认 alert、再次启动，并 SHALL 覆盖 alert 出现在 held button 与 App transition 中的 trace。

#### Scenario: 完整十分钟流程的缩时重放
- **WHEN** deterministic host 以缩时 monotonic trace 执行 Launcher→Clock→Start→Home→Expire→Acknowledge→Clock→Start
- **THEN** Timer 在后台连续、alert 抢占和恢复正确、最近时长保留、current App/lifecycle/input owner/声音序列全部匹配规格

### Requirement: Timer 变更通过完整构建与审计门禁
完成 SHALL 需要相关 unit/integration/snapshot/PCM/headless/desktop 测试、strict warnings、sanitizer、shared-source/platform-header/allocation audit、SDL build/smoke、PlatformIO T-Embed compile、size report 与 `git diff --check`。

#### Scenario: 完成审计
- **WHEN** Timer App 被声明完成
- **THEN** 新鲜命令输出证明所有软件门禁通过，并明确列出只能在实体 T-Embed 执行的旋钮手感、响度和长时间运行验收
