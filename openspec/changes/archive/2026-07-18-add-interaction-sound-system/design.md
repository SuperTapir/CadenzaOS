## Context

Cadenza 当前把 GPIO/SDL 输入归一为 `InputFrame`，由 `AppRuntime` 处理系统手势和转场，App 只消费语义输入并绘制到 1-bit framebuffer。声音尚无抽象；原版 T-Embed 已提供 MAX98357A I²S 功放与扬声器，桌面端已经依赖 SDL3，headless runner 承担确定性验证。

调研固定了 16 个开源参考项目的 commit、许可证和关键源码，并分析了公开 Playdate 实机录像。证据表明 Playdate 风格不是低采样率，而是现代 44.1 kHz 能力下的短促、克制、动作化声音；舒适度主要受 attack/release、重触发密度、缓冲延迟、混音峰值和小扬声器母带影响。完整证据见 `docs/audio-reference-research.md`、`docs/audio-engine-adoption-decision.md` 与 `docs/audio-design-research.md`。

约束包括：core 不包含 Arduino/SDL header；构造后 voice、命令和逐样本路径不动态分配；声音不成为唯一反馈；声音时间不随桌面 simulation speed 变调；现有用户工作树修改必须保留。

## Goals / Non-Goals

**Goals:**

- 建立稳定、可测试的系统声音语义，并与产生视觉反馈的同一语义动作同步提交。
- 在 core 中以固定容量、确定性 mono 合成器生成 44.1 kHz、16-bit PCM。
- 支持 triangle、低增益 PolyBLEP square、确定性 noise、短 attack/release、线性音高轮廓、三声部和可预测抢占。
- 用固定 SPSC 命令队列隔离图形主线程与实时音频消费者；SDL callback、ESP32 audio task 和 headless 消费同一契约。
- 对高频旋钮输入执行聚合/冷却；Settings 提供 Muted/Low/Medium/High。

**Non-Goals:**

- 不实现背景音乐、MP3/ADPCM 解码、录音、麦克风、蓝牙或空间音频。
- 不复刻、提取或分发 Playdate/参考游戏声音资产；所有 cue 参数重新创作。
- 不向第三方 App 开放任意 `ToneSpec`、文件路径或设备 API。
- 不承诺桌面与 T-Embed 扬声器听感一致；最终响度、疲劳度和声腔表现需实体设备验收。
- 本变更不实现设置持久化。
- 本变更不实现 sample voice；但固定未来 WAV 母版、canonical PCM、manifest 与试听验收契约，避免替换音色时改动语义和平台 API。

## Decisions

### 1. App 提交语义，Runtime 拥有服务

`AppRuntime` 拥有 `InteractionSoundService`。系统转场、返回和内置 App 仅在状态确实改变时调用 `play(SoundCue)`；GPIO、SDL mapper 和 `InputReducer` 不发声。这样能区分成功、边界和拒绝，自动转场也与手动输入使用同一语义。

拒绝“所有 click/turn 自动响”：输入层不知道动作结果，会把无效动作误报为成功。

### 2. 固定 SPSC 命令队列隔离实时消费者

主线程把 `PlayCue`、`SetVolume`、`StopAll` 放入 16 项固定单生产者/单消费者队列。SDL AudioStream callback 或 ESP32 pinned audio task 是唯一消费者，也唯一拥有 voice、phase、envelope 和 noise 状态；headless 在同一测试线程同步消费并渲染。Muted/StopAll 另外发布一个无锁单值安全邮箱；消费者在消费普通队列后再次应用它，保证队列即使被关键 cue 填满也不能推迟静音。

16 项中普通 Navigate/Boundary 最多占 12 项，剩余 4 项为 Confirm/Back/Reject/SetVolume/StopAll 预留；生产者不覆盖消费者可能正在读取的 slot。被抑制或丢弃的 cue 不排队补播。公开 API 报告 accepted/dropped，诊断可观察 overflow。

拒绝初稿的“60 Hz 主循环按帧 push PCM”：SDL 官方和 Pokitto/Mozzi 源码都表明应由稳定消费者持续供给，显示慢帧不能直接转化为音频欠载。也拒绝 callback 直接访问完整 Runtime，以免引入 App 跨线程竞态。

### 3. 参数化、受限音色，而不是 sample pack 或完整 synth

每个 `SoundCue` 映射为一到两个内部 `ToneSpec`：waveform、start/end frequency、attack、duration、release、gain 和 priority。triangle 是日常主体，PolyBLEP square 只提供低增益数字边缘，noise 只用于边界/拒绝。Confirm/ToggleOn 上行，Back/ToggleOff 下行。

参数合成没有资产许可风险、体积小且易做 golden PCM。sfxr/ZzFX/DaisySP/Mozzi 功能均远超系统 cue，且部分许可证或运行时内存模型不合适，因此只借鉴窄算法与失败用例。

### 4. 三声部与平滑抢占

`AudioEngine` 固定三个内嵌 voice。两声足够组成 Confirm/Back，第三声允许短 Navigate 重叠；更多声部对系统反馈没有证据，只会增加峰值和 CPU。

空 voice 优先；满载时，新 cue 只有在 priority 不低于最低 priority 时才抢占其中最老的 voice。旧 voice 进入极短 release，再由 pending tone 接管，禁止在非零相位硬切。低优先级请求报告失败。

### 5. 逐样本安全边界

Core 输出 44,100 Hz signed 16-bit mono。逐样本路径使用 float、不使用 double、heap、`sin/pow/tan`；square 采用 PolyBLEP 边沿，noise 使用固定 seed xorshift。attack 至少 8 samples，产品 cue 实际使用 1–4 ms；release 至少 2–8 ms。多个 voice 用 float accumulator 求和，经 master gain、clamp 后转换 int16，禁止整数环绕、NaN 和非有限参数传播。

ZzFX-Playdate 的零 attack pop、double FPU watchdog 和 SamplePlayer click 是测试输入，不机械复制其 512-sample 前导静音 workaround，因为 Cadenza 不使用 Playdate SamplePlayer。

### 6. 平台输出格式与生命周期

SDL 先初始化 video，再单独尝试 audio subsystem，创建 44.1 kHz S16 mono AudioStream callback；SDL 可转换到实际设备格式。无设备时报告错误但继续运行图形。stream 在 Runtime/host 之后启动、退出前暂停并销毁，避免 callback 访问已析构对象。

当前 `espressif32@6.12.0` 固定 Arduino-ESP32 2.0.17，因此 firmware adapter 使用 `driver/i2s.h`。Core mono sample 在 128-sample固定栈/成员 buffer 中复制成 right-left stereo frame，再 `i2s_write` 到 DMA；BCLK/WCLK/DOUT 为 7/5/6。初始化或写入失败只禁用音频并发出诊断，不阻止 Runtime。

### 7. 音量、时间和可访问性属于契约

音量为 Muted/Low/Medium/High，默认 Medium。Muted 通过队列命令和安全邮箱清除 active/pending voice，之后输出精确零；即使队列饱和也不得失败，恢复时不续播旧声音。包络按音频样本计时，不随 desktop simulation speed 变化；暂停只阻止新的 App cue，已有声音自然结束。

任何 cue 都必须伴随现有或新增的视觉状态。一个 `InputFrame` 的任意 turn 幅度最多提交一次 Navigate，Navigate 有最短重触发间隔。

### 8. 将声音实现与资产交付契约分离

当前 cue 配方集中在独立 sound palette，参数合成只是首个实现。未来人工选中的声音按 `docs/audio-asset-contract.md` 交付无损 WAV 与来源/权利 manifest，再确定性转换为 44.1 kHz S16 mono canonical PCM。App 仍只提交 `SoundCue`；sample voice 不得泄漏文件路径或解码器 API。

golden/hash 只在人工选中候选后更新，它证明实现没有漂移，不作为“好听”的代理指标。最终默认音色必须经过上下文 A/B 和实体 T-Embed 重复操作验收。

## Risks / Trade-offs

- [SPSC 队列被误用为多生产者] → API 文档和测试固定所有提交来自 Runtime/main thread；未来第二生产者必须增加仲裁。
- [T-Embed 小扬声器低频缺失或 square 刺耳] → 中频、triangle 主体、低增益 PolyBLEP square、保守 master gain；实体设备验收后调参数。
- [SDL callback/ESP32 task 欠载] → 消费者按设备时钟持续供给、固定小块、记录 underflow/write failure；桌面 dummy smoke 与真机统计。
- [抢占仍产生瞬态] → 旧 voice 短 release + pending tone；golden 检查相邻样本最大跳变。
- [float 在 ESP32 超预算] → 禁止昂贵函数并做 firmware size/profile；若证据失败，再把 phase/envelope 改定点而不改语义 API。
- [用户未提交改动交叉] → 增量 patch、先检查 dirty tree、运行全部既有回归，不覆盖无关文件。

## Migration Plan

1. 先添加 AudioEngine、命令队列、语义服务和失败测试，再实现核心。
2. 接入 Runtime 与内置 App，扩展 Settings；无消费者时视觉行为保持不变。
3. 增加 SDL callback 与 T-Embed I²S task，并分别做 dummy/compile 验证。
4. 完成全量回归、文档与实体设备测试脚本。若平台适配失败，可禁用对应输出；core 和视觉路径仍可独立运行。

## Open Questions

- 最终 Sharp 设备的扬声器、声腔和功放未定；T-Embed 参数不是最终硬件母带。
- 音量持久化依赖未来系统存储服务。
- sample voice 只有在两轮真机调音仍无法获得目标纸张/机械质感时重新评估。
