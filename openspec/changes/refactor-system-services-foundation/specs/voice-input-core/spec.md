## ADDED Requirements

### Requirement: Portable voice PCM 格式固定
voice input core SHALL 使用 48,000 Hz、signed 16-bit、mono PCM，并 SHALL 以 10 ms、480-sample 的基础 block 表达 capture 数据；格式信息 SHALL 是显式契约而非平台隐含值。

#### Scenario: Headless block 被提交
- **WHEN** headless producer 提交一个合法 voice block
- **THEN** 每个 consumer 以相同 sample 顺序观测 480 个 S16 mono samples

#### Scenario: 格式不匹配
- **WHEN** platform adapter 不能提供契约格式且没有已配置转换器
- **THEN** capture 进入可诊断 error/unavailable 状态，不得把错误格式标记为有效语音

### Requirement: 单一硬件所有权与 consumer 会话
一个 capture coordinator SHALL 独占麦克风硬件生命周期，并 SHALL 根据 analyzer、USB 和未来显式 consumer 的 active intent 启动或停止 capture；多个 consumer SHALL 能共享一次硬件采集。

#### Scenario: App 与 USB 同时请求
- **WHEN** analyzer consumer 已 active 且 USB consumer 开始 streaming
- **THEN** 麦克风硬件保持单一 running session，两者从独立边界收到同一时间线的数据

#### Scenario: 最后一个 consumer 停止
- **WHEN** 所有 active consumer 释放 intent
- **THEN** coordinator 停止 capture、清空不应保留的 stale 数据并发布 stopped 状态

### Requirement: 每个 consumer 有独立固定容量 backpressure
每个实时 PCM consumer SHALL 拥有独立固定容量 SPSC queue；producer SHALL 不等待 consumer，不得逐 block 分配。某 consumer 满时 SHALL 只为该 consumer 丢弃最新 block并增加 overrun，其他 consumer 不受影响。

#### Scenario: Analyzer 落后
- **WHEN** analyzer queue 已满而 USB queue 有空间
- **THEN** analyzer 的新 block 被丢弃并增加其 overrun，USB 仍收到该 block，capture producer 不阻塞

#### Scenario: Consumer 恢复
- **WHEN** 落后的 consumer 再次有容量
- **THEN** 它从后续新 block 恢复，不重放已丢弃历史且不读取被覆盖的 slot

### Requirement: App 通过分析快照使用语音能力
voice analyzer SHALL 从 PCM 产生 availability、capture state、active intents、RMS/peak、基础 voice activity、overrun 和 error 快照；普通 App SHALL 通过系统快照与 start/stop command 使用该能力，不得读取 DMA 指针或实时 PCM queue。

#### Scenario: App 开启语音交互
- **WHEN** App 提交 analyzer start intent 且麦克风可用
- **THEN** 后续系统快照显示 active/running，并随确定性 PCM 输入更新 level 与 activity

#### Scenario: 静音输入
- **WHEN** analyzer 连续收到全零 blocks
- **THEN** RMS/peak 为零且 voice activity 在规定释放窗口后为 false

### Requirement: Capture 状态与错误明确
voice capture SHALL 至少区分 unavailable、stopped、starting、running 和 error；状态变化、driver error、format error 和 consumer overrun SHALL 可通过 snapshot 或诊断观测，且错误不得阻止图形 Runtime 继续运行。

#### Scenario: 初始化失败
- **WHEN** microphone adapter 初始化返回错误
- **THEN** voice 快照进入 error/unavailable，所有 consumer 不收到伪造 PCM，App Runtime 仍可 update/render

### Requirement: 麦克风使用具有视觉隐私指示
任一 microphone consumer active 或 capture running 时，系统 SHALL 向 UI 提供持续可见的隐私状态；停止后 SHALL 在状态确认后移除指示。

#### Scenario: 外部 USB 使用麦克风
- **WHEN** Mac 启用 USB input stream 且没有前台 App 请求 analyzer
- **THEN** 系统快照仍标记麦克风使用中，当前 UI 能显示视觉隐私指示
