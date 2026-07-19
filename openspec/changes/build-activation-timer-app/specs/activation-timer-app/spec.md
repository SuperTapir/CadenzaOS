## ADDED Requirements

### Requirement: Activation Timer 提供低摩擦分钟设定
Clock App SHALL 在没有活动倒计时时进入 Ready，默认配置 10 分钟，并 SHALL 允许旋钮以整分钟在 1–60 分钟内调整；单帧任意 turn delta SHALL 只产生一次语义反馈。

#### Scenario: 首次进入 Timer
- **WHEN** 系统尚未启动或恢复任何 Timer 且用户打开 Clock App
- **THEN** App 显示 10:00、Ready 状态、可见时间体和按下开始提示

#### Scenario: 设定达到边界
- **WHEN** Ready 为 1 或 60 分钟且旋钮继续越界
- **THEN** duration 保持在边界、视觉不越界并至多产生一次 Boundary

### Requirement: 单按钮控制开始暂停和继续
Clock App SHALL 在 Ready 短按开始，在 Running 短按暂停，在 Paused 短按继续；长按 SHALL 始终保留给 System Menu。

#### Scenario: 开始十分钟行动
- **WHEN** Ready 配置为 10 分钟且用户短按
- **THEN** Timer 进入 Running、remaining 从 10:00 开始按系统单调时间减少并产生成功视觉/声音反馈

#### Scenario: 暂停后继续
- **WHEN** Running 短按后经过任意系统时间再短按
- **THEN** Paused 期间 remaining 不变且 Resume 从同一 remaining 建立新 deadline

### Requirement: 暂停状态允许重设剩余分钟
Paused 状态 SHALL 允许旋钮按整分钟调整 remaining 到 1–60 分钟，并 SHALL 以新 remaining 继续；Running 旋钮 SHALL 不修改 deadline。

#### Scenario: 暂停时延长时间
- **WHEN** Paused remaining 为 07:18 且旋钮增加 2 分钟
- **THEN** remaining 变为 09:18、configured duration 同步为新的可复用时长且一次反馈表达状态改变

#### Scenario: 运行时误转旋钮
- **WHEN** Running 收到任意 turn delta
- **THEN** deadline 和 remaining 真相不改变并至多产生一次 Boundary

### Requirement: 到期确认保留最近时长
Expired SHALL 由系统 alert 接管输入；用户确认后 Timer SHALL 回到 Ready 并保留最近 configured duration，不自动开始下一轮。

#### Scenario: 完成后再来一次
- **WHEN** 10 分钟 Timer 到期且用户短按确认
- **THEN** alert 停止、Clock App 下次显示 Ready 10:00，用户再短按才开始新一轮

### Requirement: 剩余时间同时提供精确和感知表达
Clock App SHALL 同时渲染 `MM:SS` 和随剩余比例单调减少的 1-bit 时间体，并 SHALL 在 Ready、Running、Paused 和 Reduced Motion 下保持无声音也可识别的状态。

#### Scenario: 双 profile 运行画面
- **WHEN** 同一 10 分钟 Timer 在 320×170 和 400×240 分别渲染 10:00、05:00 和 00:01
- **THEN** 数字、状态、操作提示和时间体均在 bounds 内可读，时间体填充比例依次单调减少且不依赖颜色

### Requirement: App 生命周期不拥有 Timer 真相
Clock App 的 `onEnter`、`onExit`、update 和 render SHALL 不重置或累加 authoritative Timer；离开并返回 SHALL 读取同一个 system snapshot。

#### Scenario: 返回 Launcher 后重开
- **WHEN** Running Timer 离开 Clock App、在 Launcher 经过两分钟后重新打开
- **THEN** current Timer identity/state 保持且 remaining 减少约两分钟，不从初始时长重启
