# portable-audio-core Specification

## Purpose
定义无需平台 API 和运行时分配的 44.1 kHz 四声部合成、带限原语、平滑抢占、饱和混音与实时安全命令队列。
## Requirements
### Requirement: 固定容量实时安全渲染

系统 SHALL 在 portable core 中把 active 与 delayed tone 渲染为 44,100 Hz、signed 16-bit、单声道 PCM，并 SHALL 在构造后不为命令、调度 event、voice 或逐样本渲染执行动态分配。

#### Scenario: 渲染短交互声音
- **WHEN** 消费者提交有效 tone 并请求任意正数个样本
- **THEN** core 输出对应数量的 PCM，活动期间包含非零样本且结束后回到精确零

#### Scenario: 空闲渲染
- **WHEN** 没有 active、pending 或 delayed voice
- **THEN** core 为整个请求输出零样本

#### Scenario: 渲染延迟短音
- **WHEN** 消费者提交包含立即与延迟 tone 的有效 cue 并请求任意正数个样本
- **THEN** core 输出对应数量的 PCM，tone 按定义 offset 起音且全部结束后回到精确零

### Requirement: 受限且带限的音色原语

系统 SHALL 支持 triangle、wavetable sine、PolyBLEP square 与确定性 noise，SHALL 支持起止频率、延迟、attack、总时长、linear 或 exponential decay、release、gain 与 priority，并 SHALL 钳制非法或非有限参数。

#### Scenario: 音高与包络
- **WHEN** tone 的起止频率不同且 attack/release 非零
- **THEN** 相位增量随生命周期变化，首尾接近零且方向与频率定义一致

#### Scenario: Square 边沿
- **WHEN** 渲染 square tone
- **THEN** 相位不连续处应用带限校正，不直接输出未经校正的硬边沿

#### Scenario: 确定性 Noise
- **WHEN** 两个全新引擎以相同 seed、tone 和样本数量渲染 noise
- **THEN** 两次 PCM 逐样本一致

#### Scenario: 非法参数
- **WHEN** tone 含负延迟、零/负频率、过长时值、超范围 gain 或非有限浮点值
- **THEN** core 拒绝或钳制为安全有限范围，输出不含未定义行为

#### Scenario: 指数衰减
- **WHEN** tone 选择 exponential decay
- **THEN** 峰值后的包络单调快速下降且逐样本路径不调用昂贵指数函数

#### Scenario: Wavetable sine
- **WHEN** 渲染 sine tone
- **THEN** 输出使用固定只读 wavetable、保持确定性且不出现 square 硬边沿

### Requirement: 饱和且有限的混音

系统 SHALL 使用足够范围的累加器混合 voice，应用 master gain 后饱和到 `int16_t`，不得整数环绕或传播 NaN/Infinity。

#### Scenario: 多个高增益 voice
- **WHEN** 四个高增益 voice 同时达到峰值
- **THEN** 每个输出均位于 -32768 到 32767 且峰值被限制而非环绕

### Requirement: 固定 SPSC 语义命令队列
系统 SHALL 提供固定 16 项的单生产者/单消费者命令队列，承载 PlayCue、PlayNotes、SetVolume 与 StopAll；消费者 SHALL 是 voice 状态的唯一所有者，队列 SHALL 为关键命令保留至少 4 项容量且生产者不得覆盖消费者可见 slot。Navigate、Boundary 与 PlayNotes SHALL 使用普通可丢弃水位。Muted 与 StopAll SHALL 另有无锁安全邮箱，使队列饱和时仍能在下一次 render 前清空声音。

#### Scenario: 正常生产消费
- **WHEN** 主线程按顺序提交多个 cue、note set 与控制命令且音频消费者排空队列
- **THEN** 命令以提交顺序生效且不需要锁或动态分配

#### Scenario: 重复内容溢出
- **WHEN** 普通命令已达到非关键水位且新 Navigate、Boundary 或 PlayNotes 到达
- **THEN** 请求被拒绝并记录 overflow，保留容量不被占用且历史请求不会稍后补播

#### Scenario: 关键提示使用保留容量
- **WHEN** 普通命令已达到非关键水位且 Confirm、Back、Reject、SetVolume 或 StopAll 到达
- **THEN** 关键命令在保留容量内被接受且全部既有命令顺序不变

#### Scenario: 饱和队列不能阻塞静音
- **WHEN** 16 项队列已被关键 cue 填满且主线程请求 Muted
- **THEN** 请求仍被接受，消费者清空既有与排队 voice，并在该 render 输出精确零

### Requirement: 四声部与平滑确定性抢占
系统 SHALL 提供固定四声部和固定容量 delayed event；满载时 SHALL 仅在新 tone priority 不低于现有最低 priority 时选择其中最老的 voice，并 SHALL 先短 release 再启动 pending tone。有效 MusicalNoteSet SHALL 同时使用不超过四声且重播前 SHALL 清理旧 audition state。

#### Scenario: 延迟 event 到点
- **WHEN** cue 包含尚未到 offset 的 tone
- **THEN** consumer 在目标样本处提交 tone，且任何时刻同时 active 的 palette voice 不超过四声

#### Scenario: 高优先级抢占
- **WHEN** 四个 voice 已满且触发更高优先级 tone
- **THEN** 最低优先级中最老的 voice 平滑释放，随后渲染新 tone

#### Scenario: StopAll 清理延迟 event
- **WHEN** cue 或 MusicalNoteSet 有 active、pending 或尚未起音的 delayed state 且消费 StopAll 或 Muted
- **THEN** active、pending 与 delayed 状态全部清空，历史 event 不会稍后补播

#### Scenario: 三音 MusicalNoteSet
- **WHEN** consumer 接受三个合法 MIDI note
- **THEN** 三个 tone 同时起音、active voice 不超过四且固定容量不分配 heap

#### Scenario: 低优先级被拒绝
- **WHEN** 四个 voice 都被更高优先级 tone 占用且触发低优先级 tone
- **THEN** 现有 voice 保持活动且触发调用报告失败

#### Scenario: 抢占连续性
- **WHEN** 非零相位 voice 被抢占
- **THEN** 输出不得直接从旧相位峰值硬切到新 tone 峰值
