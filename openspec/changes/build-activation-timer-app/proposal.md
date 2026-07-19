## Why

现有 `ClockApp` 只是会正向累计秒数的交互演示，不能支持用户“先行动十分钟、到时被可靠叫回”的真实需求。Timer 还是第一个必须跨 App、System Menu 和 Launcher 持续推进的后台任务，适合现在用一个窄而完整的垂直切片验证 Cadenza 的 App 与系统服务边界。

## What Changes

- 将内置 Clock 演示替换为单任务 Activation Timer：旋钮按整分钟设置 1–60 分钟，默认 10 分钟，短按开始/暂停/继续，到期确认后保留最近时长供再次启动。
- 新增 system-owned 单 Timer 服务，以注入的单调/模拟时间和 deadline 推进 Ready、Running、Paused、Expired 状态；System Menu 暂停 App 或返回 Launcher 时仍继续计时。
- 扩展 typed App capability、`SystemCommand` 与 `SystemSnapshot`，让 Timer App 只能通过受权命令控制服务并读取冻结快照。
- 新增后台 Timer indicator 和到期 critical surface；到期提示覆盖任意 App 或 System Menu，隔离完整按钮 sequence，并持续视觉/声音提醒到用户确认。
- 新增独立 TimerComplete 声音语义和确定性合成/golden；Muted 时保留完整视觉反馈。
- 更新 320×170 与 400×240 App/overlay snapshot、headless/desktop 真实流程、firmware 构建与研究/采用文档。
- Reset 保持 App 领域语义，不向 System Menu 注入 App 条目；首版不加入多 timer、标签、历史、番茄钟、通用 scheduler 或 deep-sleep/power-loss persistence。

## Capabilities

### New Capabilities

- `activation-timer-app`: 单旋钮分钟设定、开始、暂停、继续、到期确认、最近时长复用与双 profile 视觉表达。
- `countdown-timer-service`: 单个 system-owned 倒计时的状态、deadline、typed command、capability、snapshot、后台推进和确定性时间注入。
- `timer-alert-presentation`: 后台 indicator、到期 critical surface、输入 sequence 抢占/确认和跨 App 提醒策略。

### Modified Capabilities

- `portable-runtime`: 内置 Clock 语义改为 Timer，并让 frame transaction 区分可靠 elapsed time 与可钳制的 presentation delta。
- `interaction-sound-language`: 增加非交互型 TimerComplete 提醒语义、重复抑制和静音视觉等价契约。
- `runtime-verification`: 用 Timer 状态机、deadline crossing、后台/Menu 到期、双 profile snapshot、音频 golden 和完整 E2E 替换原 Clock 演示覆盖。

## Impact

受影响区域包括 `cadenza_core` 的 App context/capability/system surface/runtime contract，`cadenza_system` 的 service host/frame coordinator，`cadenza_apps` 的 Clock 实现与注册，portable audio cue library，headless/desktop/firmware composition root、CMake/PlatformIO source list、snapshot/golden 和项目文档。不新增第三方运行时依赖；外部源码仅用于调研并记录版本、许可证与采用边界。
