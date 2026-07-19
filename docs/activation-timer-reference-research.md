# Activation Timer 参考调研

本文为 Cadenza 第一款真实工具 App 的设计与实现提供可审计依据。研究范围不是“如何画一个倒计时数字”，而是：单旋钮设备如何建立低摩擦的启动体验、倒计时为何必须脱离前台 App update、到期提醒如何跨 Launcher/System Menu 保持可靠，以及哪些 RTOS/平台 timer 机制不应直接成为 portable App 契约。

## 锁定参考

| 参考 | 版本 / commit | 许可证 | 阅读范围 | 关键结论 |
| --- | --- | --- | --- | --- |
| AOSP DeskClock | `android-security-16.0.0_r7` / `76ccf26b51403838ae8890e8ba286dea85d53c18` | Apache-2.0 | `data/Timer.kt`、`data/TimerModel.kt`、`timer/TimerService.kt`、`timer/TimerItem.kt` | running/paused/expired/reset 是领域状态；running 剩余时间由单调 start time 与绝对 expiration 推导，不靠 UI tick 递减；最近到期项交给系统 AlarmManager；到期提醒由独立 service/ringer 维持；Reset 与 Add Minute 是业务动作 |
| Zephyr | v4.4.0 / `684c9e8f32e4373a21098559f748f06915f950c9` | Apache-2.0 | `kernel/timer.c`、kernel timeout public contract | 绝对 timeout 可避免延迟中断让周期漂移；expiry callback 有明确执行上下文；这类 kernel callback 机制不应泄漏到 Cadenza App ABI |
| FreeRTOS Kernel | V11.3.0 / `9b777ae5c5b8e9e456065a00294d1e5f5f9facf5` | MIT | `timers.c`、`include/timers.h` | timer command 通过有界 daemon queue 异步处理；命令到处理之间已经过期时应立即触发；callback 不得阻塞；周期 timer 的 backlog 语义比单次倒计时复杂且无必要 |
| ESP-IDF | v5.5 / `8c750b088c7cd857d079c0eeb495da199b359461` | Apache-2.0 | `components/esp_timer/src/esp_timer.c`、官方 ESP Timer 文档 | `esp_timer_get_time()` 提供自启动以来 64-bit 微秒单调时间；light sleep 后计数会按 RTC 时间前移；deep sleep 后归零；one-shot callback 可迟到，回调应短；portable service 可采用单调 deadline 思想，但不应直接依赖 ESP callback |
| PicoTimer | Catalog 2026-05-04 页面 | 产品行为参考，不复用代码/资源 | 官方 Catalog 产品说明 | 曲柄用于设定、主确认用于 start/pause、Reset 是 App 本地动作；其两页和多按键布局不能直接照搬到 Cadenza 的单按钮模型 |
| Apple Timer | 2026-07-19 在线支持文档 | 产品行为参考，不复用代码/资源 | Timer 设置、后台、暂停与到期行为 | 用户对 timer 的类别预期是离开 App 或设备休眠后仍继续，并在任意上下文到期提醒；最近时长可快速复用 |

源码只用于独立理解语义、失效模式与 trade-off。本变更不复制 AOSP、Zephyr、FreeRTOS 或 ESP-IDF 实现，不新增第三方源文件或运行时依赖。

## 关键源码结论

### 1. 计时真相必须是 deadline，不是 render/update 次数

AOSP `Timer.remainingTime` 在 running 状态下计算：

```text
remaining = remaining_at_start - max(0, monotonic_now - start_time)
expiration = start_time + remaining_at_start
```

暂停时才把当前 remaining 固化。UI 的 20 ms refresh loop 只刷新显示，不拥有时间真相。由此可避免：

- App 被 System Menu suspend 时倒计时暂停；
- 返回 Launcher 后 active App 不再 update，倒计时消失；
- 某帧传输或调度超过 animation `dt` clamp 后永久丢失真实时间；
- UI 帧率变化造成长期漂移。

Cadenza 当前 firmware 将 presentation `dt` clamp 到 50 ms。这是动画数值保护，不能作为用户计时器的 elapsed time。Timer service 需要独立的注入式 monotonic/simulation timeline，App 只读 snapshot。

### 2. 到期是系统事件，不是 Timer 页面的一次动画

AOSP 为最近到期 timer 安排系统 AlarmManager 回调，并由 TimerService/TimerKlaxon 维持提醒。Apple 也明确 timer 在其他 App 或睡眠时继续。对 Cadenza，这意味着：

- System Menu 和 Launcher 期间 system service 继续推进 timer；
- 到期必须进入 system-owned critical presentation，而不是等 Timer App 下次 update；
- 到期提示音和视觉要持续到用户确认；Muted 时视觉仍完整；
- 触发时若按钮正处在另一个 press/long-press sequence，不得把该 sequence 的 release 当作确认。

首版只需要一个 timer，因此无需 AOSP 的多 timer 排序、持久化 DAO、重启修复、通知列表或标签。

### 3. 不直接采用 RTOS timer callback

Zephyr 与 FreeRTOS 都证明 timer callback 需要定义延迟、重入、queue、执行上下文和 backlog。Cadenza 已有单线程确定性 frame transaction 与 typed command queue；另起 RTOS callback 会引入：

- callback 与 `SystemSnapshot` commit 的并发顺序；
- ISR/daemon 到 frame 的 mailbox 和容量；
- desktop/headless 的第二套模拟实现；
- platform timer 生命周期、休眠和 SDK 差异。

对 1–60 分钟的单个倒计时，这些成本没有收益。采用“portable TimerService 在 `beginFrame` 读取注入的单调时间、生成 frozen snapshot 与到期 edge”的方式；平台只提供时间，不执行 App callback。

### 4. One-shot 语义优于周期 tick

倒计时只关心 deadline crossing。每秒或每分钟周期回调会带来 missed tick/backlog 问题，而 UI 本来每帧可从 `deadline - now` 推导显示。首版采用 one-shot domain semantics：

- running 时保存 deadline；
- pause 时保存 remaining；
- `now >= deadline` 只产生一次 Running → Expired edge；
- 提醒重复节奏由 alert policy 控制，不伪装成 timer 周期。

## 产品交互比较

PicoTimer 使用曲柄 + A/B/方向键把编辑页、运行页、Reset 和返回分开。Cadenza 只有旋钮及其按压，且长按由 System Menu 拥有，因此不能复制该手势表。为了让“开始做十分钟”只有最低摩擦，采用：

| 状态 | 旋钮 | 短按 | 长按 |
| --- | --- | --- | --- |
| Ready | 以 1 分钟步进调整 1–60 分钟；单帧大 turn 聚合为一次声音 | 立即开始 | System Menu |
| Running | 不修改 deadline；提供稳定 boundary 视觉/声音 | 暂停 | System Menu |
| Paused | 按整分钟增减 remaining，保持 1–60 分钟边界 | 继续 | System Menu |
| Expired alert | 不调整 | 确认并回到最近时长的 Ready | 由 critical surface 捕获，不打开 Menu |

Reset 不进入 System Menu。它是 App 领域动作，而当前系统菜单只拥有 Resume/Home/Sound/Motion。首版通过“Paused 后旋钮重设”和“到期确认后回到最近时长”覆盖高频 reset/repeat；若真机使用证明仍需一步 reset，再在 App 内增加有界状态，不提前扩张 App menu contribution 公共契约。

## 视觉与提醒方向

数字负责精确读数，但剩余时间还应成为可见面积：Ready 时旋钮填充一个 60 段/连续黑色时间体，Running 时按剩余比例被稳定消耗，中央显示 `MM:SS`。这比只显示数字更贴近“感受到这十分钟”的目标。

全局 presentation 分两级：

- Running/Paused 且 Timer App 不在前台：右上角持久 indicator 显示 timer 状态和向上取整分钟，不抢 input；
- Expired：critical surface 覆盖当前 App/Menu，显示完成状态、原始时长和确认提示，提醒音按有界间隔重复直到确认。

视觉第一版必须同时适配 320×170 与 400×240；禁止通过逐帧 heap、浮点三角函数密集扫描或额外全屏 framebuffer 实现。

## 采用 / 不采用

采用：

- 单个 system-owned TimerService；
- Ready / Running / Paused / Expired 封闭状态；
- 单调 deadline 与暂停 remaining；
- typed commands + frozen snapshot；
- 后台 indicator 与到期 critical surface；
- 最近时长复用，默认 10 分钟，范围 1–60 分钟；
- headless 注入时间和 deadline crossing 测试。

不采用：

- App 自己累计 `AppUpdateContext.dt`；
- RTOS/ESP timer callback 直接修改 App 或 renderer；
- 多 timer、标签、历史、番茄钟、预设列表、通用 scheduler；
- Timer 的 Reset 条目注入 System Menu；
- deep-sleep/power-loss persistence；当前系统尚无完整 sleep/persistence contract，必须另立变更验证 RTC、唤醒和存储。

## 需要实验验证的假设

- 单格 1 分钟和快速旋转线性增量在 T-Embed 编码器上是否足够快；如需 acceleration，必须先记录 encoder detent/速度 trace 再定阈值。
- 320×170 上时间体、大数字、状态文字能否同时清晰；以双 profile PNG/golden 和真机照片判断。
- TimerComplete 提示音的长度、重复间隔和响度需要独立上下文试听；自动 PCM/golden 只能证明确定性，不能代替用户听感。
- critical surface 抢占 System Menu 时的视觉与按钮 sequence 手感需要 desktop trace 和真机验证。

## 参考链接

- [AOSP DeskClock `Timer.kt`](https://android.googlesource.com/platform/packages/apps/DeskClock/+/76ccf26b51403838ae8890e8ba286dea85d53c18/src/com/android/deskclock/data/Timer.kt)
- [AOSP DeskClock `TimerModel.kt`](https://android.googlesource.com/platform/packages/apps/DeskClock/+/76ccf26b51403838ae8890e8ba286dea85d53c18/src/com/android/deskclock/data/TimerModel.kt)
- [AOSP DeskClock `TimerService.kt`](https://android.googlesource.com/platform/packages/apps/DeskClock/+/76ccf26b51403838ae8890e8ba286dea85d53c18/src/com/android/deskclock/timer/TimerService.kt)
- [Zephyr v4.4.0 `kernel/timer.c`](https://github.com/zephyrproject-rtos/zephyr/blob/684c9e8f32e4373a21098559f748f06915f950c9/kernel/timer.c)
- [FreeRTOS V11.3.0 `timers.c`](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/9b777ae5c5b8e9e456065a00294d1e5f5f9facf5/timers.c)
- [ESP-IDF v5.5 ESP Timer](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32/api-reference/system/esp_timer.html)
- [PicoTimer Catalog 页面](https://play.date/games/picotimer/)
- [Apple Timer 支持文档](https://support.apple.com/guide/iphone/set-timers-iph8241d6b2a/ios)
