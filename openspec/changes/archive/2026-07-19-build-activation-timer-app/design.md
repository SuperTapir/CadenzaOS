## Context

该变更开始时的旧实现用 `AppUpdateContext.dt` 正向累加 `elapsed_`；System Menu 会冻结 App update/render，返回 Launcher 后旧 Clock 演示也不再 update。firmware 还会把 presentation `dt` clamp 到 50 ms，因此该路径既不能实现倒计时，也不能保证真实十分钟后提醒。

Cadenza 已有固定容量 `SystemServiceHost`、typed command/frozen snapshot/frame transaction，以及单 interactive slot 的 System Surface Coordinator。`docs/activation-timer-reference-research.md` 锁定并阅读了 AOSP DeskClock、Zephyr、FreeRTOS、ESP-IDF 和同类产品：采用单调 deadline、系统持有提醒、回调短且异步等思想，但不复制实现或新增依赖。

设备只有旋钮及按压；短按由 App 使用，长按由 System Menu 使用。第一版必须同时覆盖 320×170、400×240、headless、SDL 和 T-Embed，保持无逐帧分配和确定性测试。

## Goals / Non-Goals

**Goals:**

- 交付能实际使用的单任务 Activation Timer，默认 10 分钟、范围 1–60 分钟。
- Timer 在 App、Launcher 与 System Menu 间持续推进，并在任意稳定/覆盖状态到期提醒。
- 以 typed command、capability、snapshot 和单调时间保持 App/system/platform 单向边界。
- 用可见“时间体”而非只有数字表达剩余时间。
- 以失败测试、双 profile snapshot、PCM golden、headless/desktop E2E 和 firmware build 完成验证。

**Non-Goals:**

- 多 timer、标签、历史、番茄钟、预设列表、自然语言输入或通用 scheduler。
- deep-sleep、断电或重启后的持久化；当前系统没有完整 RTC/storage/wakeup 契约。
- App 自定义 System Menu 条目或任意 callback/view 注入。
- 引入 FreeRTOS/ESP Timer callback 作为 portable service 实现。
- 在该历史变更中重新设计当时已获批的 Clock Launcher Cover（该决定后来由 `polish-activation-timer-presentation` 取代）。

## Decisions

### 1. 历史决定：当时保留旧 Clock catalog entry，内部先变为 Activation Timer

该变更当时保留 catalog 数值 `0x0101` 和旧 Clock Cover，避免把产品行为变更与 Launcher 资产重新审批耦合；原正向 chrono 演示及 `elapsed_` 状态全部移除。当前实现已由 `polish-activation-timer-presentation` 完整迁移为 `TimerApp`、`kTimerAppId` 与 TIMER Cover，且仍沿用同一 catalog 数值。App `onEnter` 只重置 presentation phase，不重置 system timer，因此离开/返回不会破坏计时。

备选是同时改名 Timer 并重做 Cover。这会扩大 snapshot、资产、用户审批与 handoff 范围，对核心体验没有增益，暂不采用。

### 2. 单个 TimerService 由 SystemServiceHost 拥有

新增封闭 `TimerState {Ready, Running, Paused, Expired}`、`TimerSnapshot` 与轻量 `TimerService`。状态包含 configured/remaining/deadline、owner AppId、expiration generation 和 alert cadence；不含 UI 指针、callback、heap 或 platform handle。

```text
Ready --Start(duration)--> Running --Pause--> Paused --Resume--> Running
                               |                  | SetRemaining
                               +--deadline------> Expired --Acknowledge--> Ready
```

只允许一个 timer。Timer App 通过 `TimerControl` capability 提交 Start/Pause/Resume/SetRemaining；到期 critical surface 以 system owner 提交 Acknowledge。非法 duration、非法 state transition、非 owner/capability 调用稳定拒绝并诊断。离开前台不会触发 foreground lease cleanup。

备选是 App 自己累计 `dt`，会在 Menu/Home 下停止；备选 RTOS callback 会引入第二套并发、mailbox 和 host 模拟语义，均不采用。

### 3. 时间真相使用注入的 monotonic now，presentation 仍用 simulation delta

新增显式 `beginFrameAt(nowMs, presentationDt)` / `runFrameAt(...)` 路径：

- firmware 从 64-bit `esp_timer_get_time()/1000` 读取单调毫秒，presentation delta 仍可 clamp；
- desktop 使用 SDL monotonic ticks；
- headless/test 使用可精确 seek/advance 的 simulation monotonic time；
- 现有 `beginFrame(dt)` / `runFrame(...)` 保留为确定性兼容入口，由 host 内部 nondecreasing fallback clock 推进。

Running 保存 `deadlineMs = nowMs + remainingMs`；每帧由 `deadline - now` 推导 snapshot。Paused 固化 remaining。倒退 timestamp 不增加 remaining，并记录稳定 clock-regression 诊断；duration 加法使用饱和/范围检查。presentation `dt = 0` 不等于 timer 暂停，只有 Pause 命令能暂停。

备选是传递真实大 `dt` 给所有动画，会破坏现有数值保护；备选每帧扣 float seconds 会积累误差且无法表达 frame stall，均不采用。

### 4. App 交互按四状态最小化

- Ready：旋钮线性调整整分钟，1–60 边界；短按立即 Start。
- Running：短按 Pause；旋钮不改变 deadline，只给 Boundary 反馈。
- Paused：旋钮按整分钟调整 remaining（边界 1–60）；短按 Resume。
- Expired：App 输入被 critical surface 截获；短按 Acknowledge，返回保留最近 configured duration 的 Ready。

每个 frame 任意幅度 turn 最多产生一个 Navigate/Boundary。Reset 不进入 System Menu；Paused 调整和 Expired 后回到最近时长覆盖首版高频重设需求。

### 5. 时间体使用有界矩形几何

App 以大号 `MM:SS` 提供精确读数，并用横向黑色 time mass 表达 `remaining/configured` 比例：填充面积连续缩短，分钟刻度提供离散感，状态 label/操作 hint 给出 Ready、Running、Paused。布局由 profile 尺寸计算，所有文本走 bounded text；不使用逐像素圆扇区、额外 framebuffer 或逐帧三角函数扫描。

Ready 的 configured duration 同时决定 time mass 的最大刻度；Running/Paused 用相同几何，避免状态切换时画面身份改变。Reduced Motion 只关闭非必要 pulse/invert，不改变进度。

### 6. Timer alert 是 system-owned critical surface

`SurfaceKind` 增加 `TimerAlert`，优先级高于 System Menu。Timer 进入 Expired 时：

- 若当前无 interactive surface，捕获当前稳定 frame 并打开 alert；
- 若 Menu 已打开，复用冻结背景并替换 Menu；
- 若 App transition 中，到期状态立即归 System，alert 在可组成的当前 frame 上呈现，不等待 App update；
- opening 时若按钮 sequence 已在进行，alert 保持 unarmed 到完整 release，不能误确认；
- alert active 时 App update/input/render 冻结，system services 继续；短按确认后关闭 alert、停止 alert audio、Timer 回 Ready，不改变 current AppId。

Running/Paused 且 owner App 不在前台时，在 persistent indicator layer 显示紧凑 `T 08` / pause 状态，不抢 input。Timer App 前台不重复显示 indicator。Critical alert 使用现有 framebuffer，不新增第三个全屏 buffer。

### 7. TimerComplete 是独立提醒语义

`SoundCue::TimerComplete` 使用固定容量合成 palette，和 Confirm/Toggle 等输入反馈可辨。Expired edge 立即请求一次，之后按固定且有界间隔重复；不排队补播因 frame stall 错过的历史重复，只在当前时刻请求至多一次。Acknowledge 调用 `stopAll()` 清除正在播放和 scheduled alert 事件。

Muted 时 cue 被抑制，但 critical surface 和 indicator 保持完整。PCM duration/event structure/hash 自动门禁只能证明确定性；最终音色仍需上下文试听记录。

### 8. Frame transaction 保持单向

每帧顺序扩展为：

```text
inject monotonic now + ingest platform events
  -> advance TimerService / detect one expiration edge
  -> freeze update SystemSnapshot
  -> coordinate system surface + update active App
  -> FIFO commit typed commands
  -> freeze render SystemSnapshot
  -> render App/transition + transient + interactive + persistent indicators
```

到期声音由 service edge 在 begin phase 请求；App 命令仍在 commit phase生效并同帧 render 可见。render 不提交命令，Timer App 不持有 service 或 platform clock。

## Risks / Trade-offs

- [已解决的历史风险] 当时保留 Clock 名称曾让“Clock 与 Timer”边界模糊 → `polish-activation-timer-presentation` 已完整迁移到 Timer 命名，不再保留兼容 alias。
- [Risk] SystemSnapshot 增加 timer 字段会扩大所有 App 可见数据 → 字段只含非敏感只读状态，控制仍由 capability/owner 限制；用 header/source audit 防止 service pointer 泄漏。
- [Risk] alert 抢占 Menu/transition 容易产生 release leakage → 先写 button-down/expiry/release trace、Menu active expiry、transition expiry 测试，再实现 sequence latch。
- [Risk] fallback `beginFrame(dt)` 与真实 monotonic 路径不一致 → contract test 对同一 trace 运行两条路径并比较 snapshot；firmware/desktop composition root 强制用 `runFrameAt`。
- [Risk] 1–60 分钟线性旋钮在真机上可能慢 → 首版锁定确定性线性语义，收集 detent trace 后再独立决定 acceleration，不凭模拟器手感发明阈值。
- [Risk] acknowledgement `stopAll()` 会停止同时存在的其他 cue → critical alert active 时系统不应播放普通交互 cue；以 alert ownership 测试锁定，未来多 channel audio 再细分 voice group。
- [Risk] 不支持 deep sleep 会低于成熟手机 Timer 预期 → 文档明确当前边界；现阶段设备没有 sleep runtime，不能用未经验证的 RTC/persistence 假装支持。

## Migration Plan

1. 保存当时旧 Clock 演示、App snapshot、System Menu、audio PCM、desktop smoke 和 firmware size 基线。
2. 先加入 TimerService/typed contract 的失败测试，再实现纯 portable state machine 和 monotonic paths。
3. 接入 SystemServiceHost/frame coordinator 与 TimerApp，保持 System Menu contract 不变。
4. 先以失败 trace 锁定 alert preemption/input owner，再扩展 surface composition 与 indicator。
5. 增加 TimerComplete 合成、PCM golden、双 profile snapshot 和完整 E2E。
6. 更新 docs/audits/build sources，运行 host/strict/sanitizer/SDL/desktop smoke/PlatformIO 门禁并生成可检查 PNG/WAV。
7. 真机执行旋钮速度、长按冲突、60 分钟跨 `micros()` 旧 wrap 风险、到期响度和持续运行检查。

回滚以 service contract、App migration、alert surface、audio cue 四个边界为单位。不得用恢复 App `dt` 累加作为 alert surface 问题的临时回滚。

## Open Questions

- TimerComplete 的最终音色与重复间隔需要用户听感批准；实现先提供受测候选和上下文 WAV。
- 如果真机证明 Paused 调整不足以覆盖“一键作废”，再决定 App 内 Reset 手势；不因此扩张 System Menu contribution。
- deep sleep/power-loss persistence 需要 RTC、wake source、storage wear 与 reboot recovery 的独立调研和 OpenSpec，不阻塞本变更的软件 Timer。
