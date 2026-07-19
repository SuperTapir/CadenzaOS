# interaction-sound-language Specification

## Purpose
定义 Cadenza OS 的交互声音语义、克制音色方向、高频输入抑制、会话音量、静音与视觉等价反馈契约。
## Requirements
### Requirement: 语义声音词汇

系统 SHALL 定义 Input 家族的 Navigate、Boundary，Action 家族的 Confirm、Back、ToggleOn、ToggleOff、Reject，Surface 家族的 MenuOpen、MenuClose，Outcome 家族的 Complete、Warning、Failure，以及 System 家族的 Notification、Connect、Disconnect、PowerOn、PowerOff；调用方 SHALL 请求交互语义而不是平台设备操作或资源路径。

#### Scenario: Launcher 选择改变
- **WHEN** 旋钮输入实际改变 Launcher 当前选择
- **THEN** 系统触发一次 Navigate，并保留同步的视觉选择反馈

#### Scenario: 打开与返回
- **WHEN** 用户确认打开 App 或长按返回 Launcher
- **THEN** 系统分别触发 Confirm 或 Back，且提示与转场开始属于同一次语义动作

#### Scenario: 无效动作
- **WHEN** 用户动作没有改变状态或被边界拒绝
- **THEN** 系统不触发成功提示，并可触发 Boundary 或 Reject

#### Scenario: System Menu 展开与收起
- **WHEN** System Menu 开始展开或收起
- **THEN** 系统分别触发 MenuOpen 或 MenuClose，声音与视觉形变同帧开始，且不得复用 Confirm 或 Back

#### Scenario: 结果与系统事件
- **WHEN** 调用方提交一个有效的 Outcome 或 System cue
- **THEN** 系统按其稳定语义播放对应 palette，且声音不替代视觉状态

### Requirement: 克制的 Cadenza 声音语言
系统 SHALL 使用短促、带 attack/release 的参数化数字音色，以 triangle 为主体、低增益 PolyBLEP square 为数字边缘；Confirm 与 ToggleOn SHALL 具有上行动势，Back 与 ToggleOff SHALL 具有下行动势，Boundary/Reject SHALL 与成功音色可区分。

#### Scenario: 成对提示方向可辨
- **WHEN** 以相同音量依次渲染 Confirm 与 Back
- **THEN** Confirm 的结束频率高于开始频率，Back 的结束频率低于开始频率

#### Scenario: 避免直流点击
- **WHEN** 任一系统 cue 开始和结束
- **THEN** 非零 attack 或 release 使波形不会从峰值瞬时切入或切断

#### Scenario: Playdate 风格密度
- **WHEN** 日常 Navigate 与 Confirm/Back 同时出现在短交互序列
- **THEN** Navigate 保持单一短音，Confirm/Back 仅使用短方向轮廓，不形成持续音乐或奖励音墙

### Requirement: 高频输入抑制

系统 SHALL 为 Navigate 使用最短重触发间隔，并 SHALL 把单个 `InputFrame` 中任意幅度的 turn 聚合为至多一个 Navigate，不得排队补播被抑制、溢出或丢弃的历史 tick。

#### Scenario: 快速连续旋转
- **WHEN** 多次 Navigate 请求发生在冷却窗口内
- **THEN** 只有第一次请求启动 voice，其余请求被丢弃且稍后不会补播

#### Scenario: 冷却后再次旋转
- **WHEN** Navigate 冷却时间已经过去并再次改变选择
- **THEN** 新的 Navigate 被接受

### Requirement: 音量、静音与视觉等价反馈

系统 SHALL 提供 Muted、Low、Medium、High 四档会话音量且默认 Medium；进入 Muted SHALL 立即停止所有 active 与 delayed voice，此后渲染全零。任何重要状态 SHALL 具有声音之外的视觉反馈。

#### Scenario: Settings 切换音量
- **WHEN** 用户在 Settings 的 Sound 行确认
- **THEN** 音量循环到下一档，屏幕文字立即显示新档位，并以新设置决定是否播放反馈

#### Scenario: 立即静音
- **WHEN** 活动声音期间把音量设为 Muted
- **THEN** 活动与延迟 voice 被清除且下一批样本全部为零

#### Scenario: 无声使用
- **WHEN** 系统处于 Muted
- **THEN** 选择、确认、返回和错误仍通过现有或新增视觉状态表达

### Requirement: TimerComplete 是持续但有界的提醒语义
系统 SHALL 提供与 Navigate、Confirm 和 Toggle 可辨的 TimerComplete cue；Expired edge SHALL 立即请求一次并在未确认时以固定有界间隔重复，frame stall SHALL 不补播错过的历史重复，确认 SHALL 停止活动及已调度提醒声音。

#### Scenario: Timer 正常到期
- **WHEN** Running Timer 首次跨过 deadline 且音量非 Muted
- **THEN** 系统请求一次 TimerComplete，之后每个提醒间隔至多一次且不会形成无界 cue queue

#### Scenario: 到期后立即确认
- **WHEN** TimerComplete 尚在播放或含 scheduled event 时用户确认 alert
- **THEN** 所有 alert voice/scheduled event 停止且稍后不再补播

#### Scenario: 静音到期
- **WHEN** volume 为 Muted 且 Timer 到期
- **THEN** PCM 保持数字零、suppressed cue 可诊断且 TimerAlert 提供完整视觉反馈

### Requirement: TimerComplete 合成保持克制和可验证
TimerComplete SHALL 使用固定容量参数化 tone/event palette、非零 attack/release 和确定 event structure；它 SHALL 足以引起注意但不得成为持续音乐、无限 voice 或 caller 自定义 waveform。

#### Scenario: PCM golden 生成
- **WHEN** portable audio core 以固定 sample rate/volume 渲染 TimerComplete
- **THEN** duration、event offsets、peak/RMS、回零和 PCM hash 匹配批准的 golden，且 cue 结束后所有样本精确为零

### Requirement: 分层的 Cadenza 声音语言
系统 SHALL 使用原创参数合成实现经人工拍板的 Semantic Hierarchy palette；Navigate、ToggleOn 与 ToggleOff SHALL 分别以 Select、Turn On、Turn Off 的时长、低调程度和动作轮廓作为本地校准参考，但 SHALL NOT 嵌入或分发参考采样。Input SHALL 最轻，Action SHALL 可辨动作类型，Surface SHALL 使用低于 Confirm/Back 的单起音形变声，Outcome 与 System MAY 使用更完整音身，但不得形成持续背景音乐。

#### Scenario: Navigate 近似校准
- **WHEN** headless 渲染 Navigate
- **THEN** 输出为极短、低注意力的合成反馈，时长、包络峰和主频范围处于已记录校准边界且结束后精确静音

#### Scenario: Toggle 成对可辨
- **WHEN** 以相同音量依次渲染 ToggleOn 与 ToggleOff
- **THEN** 两者在时长和材质上同族、动作轮廓可辨，且不被误认为 Confirm 或 Back

#### Scenario: 进入与退出保持拍板原型
- **WHEN** headless 分别渲染 App 进入使用的 Confirm 与长按返回使用的 Back
- **THEN** Confirm 保持两段上行调音敲击，Back 保持不对称的两段下行且第二段更闷更短；两者的事件 offset、有效时长、包络峰和主频范围处于已记录的 `09-semantic-hierarchy-full` 校准边界，并与旧版连续 Triangle/Square 滑音可区分

#### Scenario: Menu Surface 成对可辨
- **WHEN** headless 分别渲染 MenuOpen 与 MenuClose
- **THEN** 两者均为无延迟第二敲击的单动作声，MenuOpen 在约 160 ms 内从低音区明显上扬，MenuClose 在约 140 ms 内明显下潜，且上下文试听不被误认为 Confirm/Back

#### Scenario: 家族层级
- **WHEN** 依次渲染 Input、Action、Surface、Outcome 与 System cue
- **THEN** 每个 cue 使用唯一名称和确定性合成定义，且重要状态仍具有非声音反馈

### Requirement: 参考音频不进入产品
系统 SHALL 只记录参考音频的非版权听感特征与本地校准结论，SHALL NOT 把用户提供或任天堂参考 WAV、其 PCM、波形片段或可逆变形加入仓库、生成代码或运行时资产。

#### Scenario: 重新生成 palette
- **WHEN** 开发者构建或导出全部 cue
- **THEN** 结果仅由仓库内参数与合成原语确定，不读取本地参考文件

#### Scenario: 审计仓库资产
- **WHEN** 执行 shared-source 与音频资产审计
- **THEN** 不存在 Select、Turn On、Turn Off 或任天堂参考采样的嵌入副本
