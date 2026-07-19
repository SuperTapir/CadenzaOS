# Activation Timer 实现与验证报告

日期：2026-07-19  
OpenSpec changes：`build-activation-timer-app`、`polish-activation-timer-presentation`

## 产品行为

首个可用 App 使用 `TimerApp` / `kTimerAppId` 和获批的 TIMER Launcher Cover，
catalog 数值仍为 `0x0101`，旧 Clock C++ 符号、兼容 alias 与 Cover 资产均已删除。
内部功能为单任务 Activation Timer：

- Ready 默认 10 分钟，旋钮以整分钟在 1–99 分钟间循环设定，短按开始；
- Running 短按暂停，误转旋钮只给 Boundary，不修改 deadline；
- Paused 旋钮按整分钟增减并将秒数归零，短按继续；
- 离开 Timer、回 Launcher、打开 System Menu 或切换其他 App 时继续计时；
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

新增第 16 个语义 `TimerComplete`，不改变原 15 项 PCM golden。最终候选为三次
清脆上行铃击：783.99 Hz 位于 0 ms、1046.50 Hz 位于 140 ms，第三击在
290 ms 叠加 1318.51/1567.98 Hz 明亮和弦；最后非零采样在 699.841 ms。
44.1 kHz / S16 / mono 一秒 PCM FNV-1a 为 `0xF5AB40E42CF75F78`，导出 WAV
SHA-256 为 `85c913ef78cbab8221c2774eeac184072ac54e91cde29f6a59ce6727ffdf5fda`，
峰值 5622，RMS 1275.800。

到期 edge 立即播放一次，此后每 2 秒当前帧至多再请求一次，不补播卡顿期间错过的
历史重复。Muted 下 PCM 保持全零但 alert 完整；Acknowledge 走 StopAll 安全邮箱，
清除 active voice、scheduled event 和已排队提醒。

## 自动化证据

- TimerService：Ready/Running/Paused/Expired、1/99 分钟、inclusive deadline、
  pause/resume、07:18 + 2 分钟、timestamp regression、large step、single
  generation、deterministic replay、owner/state/duration 和零分配；
- command/frame：默认拒绝 capability、commit 再校验、FIFO、update/render snapshot、
  `dt=0` 与 clamp 独立、Muted、repeat no-backlog 和 acknowledge stop；
- App/surface：Ready/Running/Paused/Expired 输入、离开 App、后台 indicator、Menu
  preemption、held-button sequence、transition freeze、双 profile alert golden 和
  alert 零分配；
- presentation：双 profile 原生 Timer 数字 atlas、`10:00`/`05:00`/`00:01`
  等宽与冒号光学中心、Starting/Pausing/Resuming 粗扫光首/中/末帧、Reduced
  Motion 静态双框及 onEnter/onExit 清理；
- Menu：mask 在 panel 尚未到达的整屏区域渐现、opening/closing 同进度逐像素反向
  等价、fully-open 与稳定 Menu 逐像素相同；
- Cover：1400×620 平滑母稿、350×155 与 280×124 PBM/header 可复现、reflective
  palette、静态 Cover、launch/return endpoint 与 30 FPS 邻帧变化门禁；
- E2E：Launcher → Timer → Start → System Menu Home → Expire → Acknowledge →
  Timer → Start，以及桌面真实 InputReducer 路径；
- 320×170 Ready hash `12921254497184521768`，400×240 Ready hash
  `4523606151491982558`；两个 PNG、Timer Alert、Menu 与 handoff PNG 已人工检查；
- 当前 macOS ABI：`SystemSnapshot` 88 B、`TimerService` 64 B、
  `SystemSurfaceCoordinator` 208 B，仍不新增第三块 framebuffer；
- normal host suite 77/77、warnings-as-errors 与 ASan+UBSan host suite 76/76
  均通过；最终粗扫光改动另在 strict/sanitized allocation 与 App 关键帧 focused
  suite 通过；SDL 3.4.12 dummy
  T-Embed/Sharp 实进程通过；OpenSpec、source/research/naming audit 均通过；
- PlatformIO 6.1.19、`espressif32@6.12.0`、Arduino-ESP32 2.0.17 实际编译通过：
  RAM 100,112 / 327,680 B（30.6%），Flash 433,029 / 3,145,728 B（13.8%），
  `firmware.bin` SHA-256
  `659a9d49d04dee0c8f982e124c770af656807bf875df00ae55059de47948b5c0`；
- ESP-IDF v5.5 候选共享源码图也实际编译通过，包含 `timer_service.cpp`：
  image 1,242,819 B，生成 bin 1,242,928 / 1,536,000 B，尚余 293,072 B（19%），
  SHA-256 为
  `45601e5f9c01ea2550f60d15747db6d46c8d0ebf63f1ebd7dc75b16e327e8c69`。

## 仍需实体设备确认

自动化不能替代真实旋钮 detent 速度、1–99 分钟循环调节手感、T-Embed 屏幕可读性、
扬声器响度/疲劳感、外壳共振和长时间运行。软件实现已经完成，但 TimerComplete 最终
音色和 2 秒重复间隔在实体试听前仍是候选；deep sleep、断电/重启恢复也明确不在本次
范围内。
