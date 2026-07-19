## ADDED Requirements

### Requirement: 被动 indicator 使用显式 presentation scope
系统 SHALL 为被动/persistent indicator 使用封闭的 presentation scope；`HomeOnly` scope SHALL 仅在当前 AppId 等于配置的 Home/Launcher AppId 时合成该 indicator，且 SHALL 不成为按钮/turn owner。

#### Scenario: HomeOnly 在 Launcher 可见
- **WHEN** 一个 `HomeOnly` passive indicator 处于 active 且当前 App 为 Home
- **THEN** 系统在 persistent indicator layer 绘制该 indicator，Home App 继续接收输入

#### Scenario: HomeOnly 在非 Home App 不可见
- **WHEN** 同一 `HomeOnly` indicator 仍 active 但当前 App 不是 Home
- **THEN** 该 indicator 不绘制到 framebuffer，且不改变 App update/input 所有权

#### Scenario: System Menu 覆盖非 Home 时不显示 HomeOnly
- **WHEN** 非 Home App 前台且 System Menu active，同时存在 `HomeOnly` indicator
- **THEN** indicator 保持不绘制；Menu 关闭后若仍停留在该非 Home App，indicator 仍不绘制

### Requirement: USB MIC 隐私角标遵循 HomeOnly
当 `voice.microphoneInUse` 为真时，系统 SHALL 以 `HomeOnly` scope 绘制 MIC 隐私角标；声音 cue 抑制等非呈现策略 SHALL 不因 scope 改变而失效。

#### Scenario: Launcher 上 USB 麦占用显示 MIC
- **WHEN** USB voice streaming 使 `microphoneInUse` 为真且当前为 Home
- **THEN** framebuffer 在约定 MIC 区域出现可读角标

#### Scenario: 非 Home App 上不显示 MIC 角标
- **WHEN** `microphoneInUse` 为真且当前为非 Home App
- **THEN** 该 App 画面上不出现 MIC 角标像素，同时 cue 抑制策略仍可生效

## MODIFIED Requirements

### Requirement: 非交互反馈不得抢占 App 输入
Transient feedback 和 passive indicator SHALL 可按各自 scope 覆盖允许的画面但不得成为按钮/turn owner；其容量、过期、去重或丢弃策略 SHALL 有界且可测试。`HomeOnly` passive indicator SHALL 不得覆盖非 Home App 画面。

#### Scenario: Toast 显示期间操作 App
- **WHEN** 无 interactive surface 且 transient feedback 可见
- **THEN** App 继续 update 并接收输入，feedback 按注入时间过期而不阻塞 App

#### Scenario: HomeOnly indicator 不阻挡非 Home 输入
- **WHEN** 非 Home App 前台且仅有 `HomeOnly` passive indicator 处于逻辑 active
- **THEN** 该 App 继续 update 并接收输入，且画面上不出现该 indicator
