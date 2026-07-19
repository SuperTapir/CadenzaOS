## Context

变更前 Runtime 已有 44.1 kHz S16 mono 三声部 `AudioEngine`、固定 SPSC 语义命令队列、会话音量、Navigate 冷却，以及 SDL/I²S/headless 三个平台消费者。原 `SoundCue` 只有七项，palette 每项只支持同时起音的一至两个线性包络 Tone，难以表达已拍板原型中的短气泡、错时敲击、快速衰减与较完整结果音。

本次试听先确定了 15 项 Semantic Hierarchy palette，随后针对 System Menu 增补两项 Surface cue。用户同意正式实现不逐样本复制试听 WAV，只要合成结果在时长、低调程度、动作轮廓和上下文听感上没有明显偏离。Select、Turn On、Turn Off 只作为 Navigate、ToggleOn、ToggleOff 的本地校准参考，不进入仓库或产品；本地试听工作区 `09-semantic-hierarchy-full/confirm.wav` 与 `back.wav` 是 Confirm/Back 已拍板的原创方向原型，正式合成必须保留其双事件节奏、调音敲击材质、方向和时长边界，不能退回旧版连续滑音动机。MenuOpen/MenuClose 使用项目合成器生成原创 V2 试听稿，用户已拍板较低中心音区、较长音身和更明显的上扬/下潜幅度。

## Goals / Non-Goals

**Goals:**

- 把词汇扩展为 Input、Action、Surface、Outcome、System 五个家族；Surface 新增 MenuOpen、MenuClose。
- 用小范围、确定性的合成原语近似还原已确认 palette，而不是继续使用同质滑音。
- 支持 sample-offset 延迟起音、快速指数衰减与柔和正弦共振，同时保持固定四声部、无堆、无昂贵逐样本函数。
- 保持 `SoundCue` 语义调用、命令队列、音量、冷却、SDL 与 I²S 输出契约不变。
- 用 PCM/golden、包络、时长、频谱特征、抢占/静音测试和上下文试听证明没有明显偏离。

**Non-Goals:**

- 不实现 sample voice、WAV 解析、文件系统、流媒体、背景音乐或第三方 App 任意合成 API。
- 不复制、嵌入、变形分发用户提供或任天堂参考音频。
- 不自动为尚不存在的业务事件增加虚假触发点。
- 不把自动频谱指标或桌面试听冒充实体 T-Embed 最终听感。

## Decisions

### 1. 扩展参数合成而不是加入 sample voice

用户接受听感近似，因此保留参数合成的低体积和无资产授权优势。参考 WAV 仅离线测量时长、短时 RMS、主频轨迹与静音结构；正式参数重新创作，不追求 waveform/hash 相似。

备选的全 PCM palette 可精确复现试听，但增加约 359 KiB 只读数据、资产 manifest/授权和第二种 authoring 模型，当前收益不足以覆盖成本，因此拒绝。

### 2. ToneSpec 增加受限时间结构

`ToneSpec` 增加 envelope curve、decay time 与 initial phase；Waveform 增加由固定小型 wavetable 实现的 Sine。`SoundEvent` 在 tone 外保存 delay，指数衰减使用每音预计算的乘数，不在逐样本调用 `exp`。现有 Linear attack/release 保留；指数衰减仍叠加最短 attack 和尾部 release，避免非设计性的文件边缘点击。

`SoundCueDefinition` 最大 event 数从 2 增至 4，每个 event 独立携带 delay。InteractionSoundService 维护固定容量 delayed event 调度器，在 render 消费端按 offset 分块提交。AudioEngine 固定 voice 从 3 增至 4：Select v6 的四个短阻尼共振器可同时起音，其余 palette 任意时刻同样不得超过四声重叠。

### 3. 正弦采用固定 wavetable

使用 compile-time quarter-wave 小型只读 wavetable和线性插值，不在 sample loop 调用 `sin`。Sine 用于柔和气泡和系统共振；Triangle/Square/Noise 原语继续存在。测试固定 PCM/hash 和相位连续性。

### 4. 参考相似度采用特征边界与人工 A/B

Navigate、Toggle、Confirm 与 Back 的自动门禁记录：总时长、非静音区间、短时包络峰数量、事件 offset、主频范围、peak/RMS 与结束静音。允许参数合成与参考逐样本不同；特征不得出现数量级偏差。Confirm/Back 必须分别保持两段上行与不对称下行的调音敲击，且与旧版单段 Triangle/Square 滑音可区分。最终通过条件仍是用户在单音与连续上下文中判断“不天差地别”。Select 获批版本额外保存四共振器模型的波形相关性，作为防止后续实现漂移的校准证据，而非产品运行时依赖。

### 5. 语义扩展不等于伪造触发点

`SoundCue` 增加 Complete、Warning、Failure、Notification、Connect、Disconnect、PowerOn、PowerOff。现有七项路由保持。新增 cue 先作为稳定内部词汇与可测试 palette；只有业务确实产生对应事件时才接入 `play()`。

### 6. 调度、抢占与静音保持单消费者所有权

延迟 event 只存在音频消费者拥有的固定数组中。新 cue 到达时按 priority 提交其 event；Muted/StopAll 同时清空 active voice 和 delayed event。队列饱和、Navigate 冷却、master gain 与平台 callback/task 形态不变。Select 的四组频率、衰减、幅度和相位来自获批 v6 的阻尼共振拟合，不保存参考 PCM。

### 7. System Menu 使用专属 Surface cue

System Menu 展开/收起是同一层级 Surface 的形变，不是确认进入内容或跨层级返回，因此不得复用 Confirm/Back。MenuOpen 采用一次同时起音的短噪声触感与低音区正弦上扬，约 160 ms、约 340→680 Hz；MenuClose 使用同族材质下潜，约 140 ms、约 640→290 Hz。两者与 0.20 秒视觉动画同帧触发，不增加第二次敲击或延迟落点，避免重新产生 Confirm 感。该方案只新增两个 `SoundCueDefinition` 和路由，不增加 voice、调度容量、样本资产或平台接口。

## Risks / Trade-offs

- [合成结果与参考仍有听感差距] → dump 全部 cue 和连续操作 demo，按用户 A/B 迭代；不以 golden 冒充好听。
- [延迟 event 调度增加状态复杂度] → 固定容量、单消费者、最多四声重叠；为 delay、清空、抢占和确定性写失败测试。
- [指数衰减或 sine 在 ESP32 超预算] → 每音预计算乘数、固定 wavetable；执行 firmware compile/size 和 host benchmark smoke。
- [高增益 Toggle 并发削顶] → event gain 与 master gain 分离，四声最坏并发饱和测试，真机再定标。
- [新增语义无人触发] → 允许稳定词汇先存在，不为覆盖率伪造业务事件。
- [golden 冻结了错误但确定的 Confirm/Back] → 先以拍板原型建立独立失败门禁，再改参数并更新 golden；不得用新 golden 反向证明听感正确。
- [Surface cue 连续滑音过尖或过短] → 使用用户拍板的低音区 V2 边界锁定方向、时长和单起音结构；实体设备仍复核小扬声器低频可辨识性。

## Migration Plan

1. 先添加延迟 event、Sine、指数衰减、完整 cue 与特征失败测试。
2. 实现引擎原语、固定调度器和 Semantic Hierarchy 参数表。
3. 导出合成试听，与已拍板参考进行特征检查和上下文 A/B。
4. 更新音频文档，运行 host、desktop、shared-source 与 firmware 门禁。
5. 若听感不可接受，回到 palette 参数；不在同一变更临时引入 sample voice。

## Open Questions

- 实体 T-Embed 的 Medium/High gain 与连续 Navigate 疲劳度仍需按真机脚本定标。
