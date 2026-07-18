## MODIFIED Requirements

### Requirement: 固定容量实时安全渲染
系统 SHALL 在 portable core 中把 active 与 delayed tone 渲染为 44,100 Hz、signed 16-bit、单声道 PCM，并 SHALL 在构造后不为命令、调度 event、voice 或逐样本渲染执行动态分配。

#### Scenario: 渲染延迟短音
- **WHEN** 消费者提交包含立即与延迟 tone 的有效 cue 并请求任意正数个样本
- **THEN** core 输出对应数量的 PCM，tone 按定义 offset 起音且全部结束后回到精确零

#### Scenario: 空闲渲染
- **WHEN** 没有 active、pending 或 delayed voice
- **THEN** core 为整个请求输出零样本

### Requirement: 受限且带限的音色原语
系统 SHALL 支持 triangle、wavetable sine、PolyBLEP square 与确定性 noise，SHALL 支持起止频率、延迟、attack、总时长、linear 或 exponential decay、release、gain 与 priority，并 SHALL 钳制非法或非有限参数。

#### Scenario: 音高与包络
- **WHEN** tone 的起止频率不同且 attack/release 非零
- **THEN** 相位增量随生命周期变化，首尾接近零且方向与频率定义一致

#### Scenario: 指数衰减
- **WHEN** tone 选择 exponential decay
- **THEN** 峰值后的包络单调快速下降且逐样本路径不调用昂贵指数函数

#### Scenario: Wavetable sine
- **WHEN** 渲染 sine tone
- **THEN** 输出使用固定只读 wavetable、保持确定性且不出现 square 硬边沿

#### Scenario: 非法参数
- **WHEN** tone 含负延迟、零/负频率、过长时值、超范围 gain 或非有限浮点值
- **THEN** core 拒绝或钳制为安全有限范围，输出不含未定义行为

### Requirement: 四声部与平滑确定性抢占
系统 SHALL 提供固定四声部和固定容量 delayed event；满载时 SHALL 仅在新 tone priority 不低于现有最低 priority 时选择其中最老的 voice，并 SHALL 先短 release 再启动 pending tone。

#### Scenario: 延迟 event 到点
- **WHEN** cue 包含尚未到 offset 的 tone
- **THEN** consumer 在目标样本处提交 tone，且任何时刻同时 active 的 palette voice 不超过四声

#### Scenario: 高优先级抢占
- **WHEN** 四个 voice 已满且触发更高优先级 tone
- **THEN** 最低优先级中最老的 voice 平滑释放，随后渲染新 tone

#### Scenario: StopAll 清理延迟 event
- **WHEN** cue 有尚未起音的 delayed event 且消费 StopAll 或 Muted
- **THEN** active、pending 与 delayed 状态全部清空，历史 event 不会稍后补播

### Requirement: 饱和且有限的混音
系统 SHALL 使用足够范围的累加器混合 voice，应用 master gain 后饱和到 `int16_t`，不得整数环绕或传播 NaN/Infinity。

#### Scenario: 多个高增益 voice
- **WHEN** 四个高增益 voice 同时达到峰值
- **THEN** 每个输出均位于 -32768 到 32767 且峰值被限制而非环绕
