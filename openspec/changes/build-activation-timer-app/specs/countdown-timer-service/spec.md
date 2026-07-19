## ADDED Requirements

### Requirement: 系统拥有单个封闭状态 Timer
SystemServiceHost SHALL 拥有至多一个 `Ready`、`Running`、`Paused` 或 `Expired` Timer，并 SHALL 在 frozen `SystemSnapshot` 中暴露 configured duration、remaining、owner 和 expiration generation，不暴露 callback、driver 或可变 service。

#### Scenario: 初始快照
- **WHEN** SystemServiceHost 初始化
- **THEN** Timer 为 Ready、configured/remaining 为 10 分钟、owner invalid 且 generation 为 0

### Requirement: Running 使用单调 deadline
TimerService SHALL 以注入的 nondecreasing monotonic time 建立 deadline 并从 `deadline - now` 推导 remaining；presentation/App simulation delta SHALL 不成为 Timer 的权威时间。

#### Scenario: App simulation 冻结但时间继续
- **WHEN** presentation delta 连续为 0 而注入 monotonic now 前进 120 秒
- **THEN** Running remaining 减少 120 秒且 App/animation state仍可保持冻结

#### Scenario: 时间源倒退
- **WHEN** 注入 now 小于上一帧 now
- **THEN** Timer remaining 不增加、deadline 不后移并记录稳定 clock-regression 诊断

### Requirement: Timer 命令类型化且受 capability 与 owner 约束
App SHALL 只能以 `TimerControl` capability 提交 Start、Pause、Resume 和 SetRemaining；host SHALL 在提交和 commit 时验证 caller、1–60 分钟范围及合法状态，并 SHALL 允许 system owner Acknowledge Expired。

#### Scenario: 无权限 App 启动 Timer
- **WHEN** 不含 TimerControl 的 App 提交 Start
- **THEN** 命令以 CapabilityDenied 拒绝且 Timer snapshot 不变

#### Scenario: 非法状态继续
- **WHEN** Ready Timer 收到 Resume 或 Running Timer 收到 SetRemaining
- **THEN** 命令以稳定 InvalidState/InvalidCommand 原因拒绝且 deadline 不变

### Requirement: Timer 跨前台生命周期继续
Running Timer SHALL 在 owner App exit、Launcher active 或 System Menu suspend-with-snapshot 期间继续，且 foreground resource cleanup SHALL 不取消 session Timer。

#### Scenario: System Menu 覆盖到期
- **WHEN** Running Timer 剩余 5 秒且 System Menu 保持打开 10 秒
- **THEN** Timer 在 deadline crossing 时进入 Expired，不能等 Menu 关闭后才计算到期

### Requirement: 到期边沿精确一次且无 backlog
TimerService SHALL 在 `now >= deadline` 的首帧只产生一次 Running→Expired edge并递增 generation；frame stall 跨过 deadline 或多个提醒间隔 SHALL 不补播历史到期/提醒事件。

#### Scenario: 大步跨越 deadline
- **WHEN** remaining 为 1 秒且下一个 monotonic now 前进 30 秒
- **THEN** Timer 只进入一次 Expired、generation 只增加 1 且该帧至多请求一次 TimerComplete

### Requirement: Timer 服务确定且无逐帧分配
TimerService、command、snapshot 和 diagnostics SHALL 使用固定值类型与有界状态，host/headless/desktop/firmware 同一源码 SHALL 对相同 time/command trace 产生相同状态。

#### Scenario: 重放状态 trace
- **WHEN** 两个 host 重放相同 Start、advance、Pause、SetRemaining、Resume、Acknowledge trace
- **THEN** 每帧 TimerSnapshot、generation、rejection 和 cue request 序列一致且 allocation probe 为零
