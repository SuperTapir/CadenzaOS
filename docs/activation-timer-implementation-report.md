# Activation Timer 实现与验证报告

日期：2026-07-19  
OpenSpec change：`build-activation-timer-app`

## 产品行为

首个可用 App 继续使用 `ClockApp` / `kClockAppId` 和既有获批 Launcher Cover，
内部功能已从正向计时演示替换为单任务 Activation Timer：

- Ready 默认 10 分钟，旋钮以整分钟在 1–60 分钟内设定，短按开始；
- Running 短按暂停，误转旋钮只给 Boundary，不修改 deadline；
- Paused 旋钮在保留秒数的基础上按整分钟增减，短按继续；
- 离开 Clock、回 Launcher、打开 System Menu 或切换其他 App 时继续计时；
- 到期由 system-owned critical alert 抢占当前界面，确认后回到最近配置时长；
- Reset 不加入 System Menu。首版用 Paused 调整和到期后保留时长覆盖重设需求，
  避免为单个 App 引入长期的 Menu contribution 契约。

## 架构边界

`TimerService` 是 `SystemServiceHost` 持有的单一权威状态机。App 只能读取冻结的
`TimerSnapshot`，并通过 `TimerControl` capability 提交 Start/Pause/Resume/
SetRemaining 命令；只有 system owner 能 Acknowledge。service 固定状态、无 callback、
无平台 handle、无逐操作堆分配。

计时真相使用 64-bit 单调毫秒 deadline：firmware 注入
`esp_timer_get_time()/1000`，SDL 注入 `SDL_GetTicks()`，headless 注入模拟时间。
presentation `dt` 仍可为零或被限制到 50 ms，不影响倒计时。timestamp 回退被拒绝且
不能增加 remaining；一次大步跨越只产生一个 expiration generation。

Timer Alert 高于 System Menu。到期时若按钮 sequence 已开始，第一次 release 只解除
capture，必须下一次完整短按才确认。Alert active 时 App update 和 transition 均冻结，
system service 仍推进。后台 Running/Paused Timer 在 owner App 外显示向上取整的
`T 08` / `P 08` 指示，owner 前台不重复显示。

## 声音

新增第 16 个语义 `TimerComplete`，不改变原 15 项 PCM golden。候选由四次上行
resonator strike 构成，事件偏移为 0/180/360/540 ms，最后非零采样在
699.955 ms，随后保留 23.265 ms 静音尾部；44.1 kHz / S16 / mono 一秒
PCM FNV-1a 为 `0x1D5913CD94A70AFA`，导出 WAV SHA-256 为
`4ead3fe4da4652cc4d610ffd1f2b93400c52c75f7375deca645295626d4f7d26`，
峰值 3817（-18.67 dBFS），RMS 1171.193（-28.94 dBFS）。`demo-system.wav` SHA-256 为
`8db5ca2b314213ff837a1df8dc22757f5dd9dff8bbb5f4ec678420f56f9a802f`。

到期 edge 立即播放一次，此后每 5 秒当前帧至多再请求一次，不补播卡顿期间错过的
历史重复。Muted 下 PCM 保持全零但 alert 完整；Acknowledge 走 StopAll 安全邮箱，
清除 active voice、scheduled event 和已排队提醒。

## 自动化证据

- TimerService：Ready/Running/Paused/Expired、1/60 分钟、inclusive deadline、
  pause/resume、07:18 + 2 分钟、timestamp regression、large step、single
  generation、deterministic replay、owner/state/duration 和零分配；
- command/frame：默认拒绝 capability、commit 再校验、FIFO、update/render snapshot、
  `dt=0` 与 clamp 独立、Muted、repeat no-backlog 和 acknowledge stop；
- App/surface：Ready/Running/Paused/Expired 输入、离开 App、后台 indicator、Menu
  preemption、held-button sequence、transition freeze、双 profile alert golden 和
  alert 零分配；
- E2E：Launcher → Clock → Start → System Menu Home → Expire → Acknowledge →
  Clock → Start，以及桌面真实 InputReducer 路径；
- 320×170 Ready hash `8752186736345292702`，400×240 Ready hash
  `9246543181918641567`；两个 PNG 和 Menu 叠加 PNG 已人工检查无裁切；
- 当前 macOS ABI：`SystemSnapshot` 88 B、`TimerService` 64 B、
  `SystemSurfaceCoordinator` 208 B，仍不新增第三块 framebuffer；
- normal、warnings-as-errors、ASan+UBSan 均为 73/73；SDL 3.4.12 dummy
  T-Embed/Sharp 实进程通过；OpenSpec、source/research audit 与 diff check 通过；
- PlatformIO 6.1.19、`espressif32@6.12.0`、Arduino-ESP32 2.0.17 实际编译通过：
  RAM 100,112 / 327,680 B（30.6%），Flash 416,225 / 3,145,728 B（13.2%），
  `firmware.bin` SHA-256
  `275c3f5433015d8c4cdb39d2f92cc2064b3c0414cbc06bfbd2902f6a23801a74`；
- ESP-IDF v5.5 候选共享源码图也实际编译通过，包含 `timer_service.cpp`：
  image 1,242,819 B，生成 bin 1,242,928 / 1,536,000 B，尚余 293,072 B（19%），
  SHA-256 为
  `45601e5f9c01ea2550f60d15747db6d46c8d0ebf63f1ebd7dc75b16e327e8c69`。

## 仍需实体设备确认

自动化不能替代真实旋钮 detent 速度、1–60 分钟线性调节手感、T-Embed 屏幕可读性、
扬声器响度/疲劳感、外壳共振和长时间运行。软件实现已经完成，但 TimerComplete 最终
音色和 5 秒重复间隔在实体试听前仍是候选；deep sleep、断电/重启恢复也明确不在本次
范围内。
