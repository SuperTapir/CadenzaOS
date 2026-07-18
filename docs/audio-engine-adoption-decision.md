# Audio Engine and Output Adoption Decision

日期：2026-07-18
适用 change：`add-interaction-sound-system`

## 决策

Cadenza 不采用完整 synth、音乐引擎或 decoder。采用“小型自有交互声音核心 + 已有 SDL3 + ESP-IDF legacy I²S adapter”的边界：

- **采用 SDL3 AudioStream callback**：桌面平台按需拉取，SDL 负责设备格式转换和生命周期。
- **采用当前 Arduino-ESP32 2.0.17 的 `driver/i2s.h`**：T-Embed audio task 持续生成并写入 DMA；mono core 在 adapter 中复制为 stereo I²S frames。
- **自有固定容量核心**：语义命令队列、固定 voice、triangle、PolyBLEP square、noise、attack/release、linear pitch contour、饱和混音和音量。
- **借鉴但不依赖 DaisySP、ZzFX、Gamebuino、Pokitto、Mozzi 与 sfxr**：只采用经过重写的窄算法/契约和失败用例。
- **不采用 Arduino Audio Tools、Mozzi、Modular Play、PocketBM 或通用 ZzFX runtime**：许可证、动态内存或功能表面积与系统 cue 不匹配。
- **首版不采用 sample pack**：避免资源许可证、flash/解码和跨扬声器母带问题；保留未来固定 PCM voice 的扩展边界。

## 为什么不是蜂鸣器、低采样率或纯 naive square

Playdate 官方明确使用 44.1 kHz，Gamebuino 的系统提示也在现代采样时间上用受限 square/noise。低位风格来自有限原语和短动作轮廓，不需要故意把输出降为 8 kHz/8-bit。T-Embed 已有 MAX98357A 数字功放，改用 PWM 蜂鸣器会放弃现成硬件并扩大后续 sample voice 的迁移成本。

纯 naive square 在高音/滑音上产生折叠 alias，短促也不等于无害。Cadenza 以 triangle 为主体，只在需要数字边缘时低增益加入 PolyBLEP square；这样保留“微型数字乐器”的身份，又避免所有提示都像尖锐按键音。

## 为什么核心自有而不是引入库

目标只有约 7 个系统语义、最多少量并发 voice、三种波形与简单包络。Mozzi/DaisySP/Arduino Audio Tools 都包含远超需求的 oscillator、filter、effect、codec 或跨板抽象；其中 Mozzi 为 LGPL、Arduino Audio Tools 为 GPL。即使引入，语义命令、冷却、抢占、静音、诊断、双平台生命周期和 headless golden 仍必须自写。

自有实现不是凭空发明：PolyBLEP 边沿、包络量化、float-on-MCU、饱和和缓冲策略均有固定源码证据，测试也针对这些证据中的真实失败。核心禁止逐样本 `sin/pow/tan`、double 和 heap，保持可审计。

## 线程与所有权

```text
App / Runtime (main thread)
        │ semantic cue + volume command
        ▼
fixed SPSC command queue
        │
        ├── headless: test thread drains + renders exact samples
        ├── SDL: AudioStream callback drains + renders requested bytes
        └── ESP32: pinned audio task drains + renders + i2s_write(DMA)

Only the consumer owns voices, phase, envelope and noise state.
```

相比初稿的主循环 push PCM，此方案把显示 presenter、截图、录制或慢帧与音频时钟隔离。相比让 callback 直接读写 Runtime，固定命令队列避免跨线程访问 App/Runtime 状态。队列是单生产者/单消费者；若未来出现第二生产者，必须新增仲裁而不是假定线程安全。

## 固定边界

- Core PCM：44,100 Hz、signed 16-bit、mono。
- I²S wire：44,100 Hz、16-bit、right-left stereo frames；左右复制 core sample。
- 原首版 voice：3 个。该约束已被 15 项 Semantic Hierarchy 变更取代；获批 Select 的四共振器模型提供了增加到固定 4 声部的直接证据，其余 cue 仍不得超过该上限。
- Command queue：固定 16 个命令，其中普通 Navigate/Boundary 最多占 12 项，预留 4 项给 Confirm/Back/Reject/音量/StopAll。生产者不覆盖消费者可能正在读取的 slot；不得在停止操作后补播被抑制的重复输入。Muted/StopAll 另有原子安全邮箱，在消费旧命令后最终应用，因而 16 个关键 cue 填满队列也不能阻塞静音。
- 每声 attack 至少 8 samples，产品 cue 实际采用 1–4 ms；release 至少 2–8 ms。抢占时对旧 voice 做极短 release，不能硬切相位。
- 混音先使用 float accumulator，再限制 master peak 并转换为 int16；不让整数求和溢出。

以上是首版容量承诺，不是音乐 API。第三方 App 只看到有限 `SoundCue`，看不到任意 `ToneSpec`。

## 输出缓冲目标

- SDL callback 按请求量即时生成，不主动排队长段静音；stream 启动后无 cue 时也返回零。
- ESP32 audio task 使用 128 mono samples（约 2.9 ms）为生成块，DMA buffer 的最终 count/len 通过 compile probe 和真机欠载计数调整。
- 从语义命令提交到音频起始的目标上限为 30 ms；公开 Playdate 实机导航音约 40 ms 长，ZzFX-Playdate 还容忍约 11.6 ms 前导静音以绕过设备 click，因此 30 ms 是保守可感知目标，不是假装零延迟。

## 可重复验证

自动化必须覆盖：

- 每个 cue 的首尾连续、有效时长、峰值、频率方向与确定性 hash；
- zero attack、zero release、零/负频率、NaN/非法 gain 等输入的钳制；
- PolyBLEP square 相比 naive square 的高频折叠能量回归（离线 probe，不必成为公开 API）；
- voice 满载时的优先级/年龄抢占和短 release；
- queue overflow、Navigate 冷却、Muted 立即清空、不会延迟补播；
- SDL dummy driver callback smoke、无设备降级和销毁；
- firmware compile、shared-source header audit、I²S stereo duplication 和错误诊断；
- 全部既有 host/snapshot/desktop 回归。

实体 T-Embed 仍必须验证：Low/Medium/High 实际响度、尖锐/低频丢失、连续旋钮一分钟疲劳度、端到端延迟、DMA underrun、与显示最慢帧并行。无实体证据时不得宣称最终听感通过。

## 重新评估触发条件

- 系统 cue 需要纸张、机械或环境质感，参数合成无法在两轮真机调音后达到目标；
- 第三方 App 需要自定义音色或背景音乐；
- ESP32 profile 证明 PolyBLEP/float 超出预算；
- SDL callback 与固件 task 无法共享命令/引擎契约；
- 最终 Sharp 硬件更换功放、扬声器或输出格式。
