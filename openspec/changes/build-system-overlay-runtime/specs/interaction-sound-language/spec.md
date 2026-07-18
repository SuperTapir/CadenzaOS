## MODIFIED Requirements

### Requirement: 语义声音词汇
系统 SHALL 至少定义 Navigate、Boundary、Confirm、Back、ToggleOn、ToggleOff 与 Reject 提示，并 SHALL 让调用方请求交互语义而不是平台设备操作或资源路径。

#### Scenario: Launcher 或 System Menu 选择改变
- **WHEN** 旋钮输入实际改变 Launcher 或 System Menu 当前选择
- **THEN** 系统触发一次 Navigate，并保留同步的视觉选择反馈

#### Scenario: 打开 App 与 System Menu
- **WHEN** 用户确认打开 App 或长按打开 System Menu
- **THEN** 系统触发 Confirm，且提示与对应 surface/transition 开始属于同一次语义动作

#### Scenario: 关闭 Menu 与返回 Home
- **WHEN** 用户关闭 System Menu 或确认 Menu 中的 Home 动作
- **THEN** 系统触发 Back，且 Menu 关闭及可选 Home transition 属于同一次语义动作

#### Scenario: 无效动作
- **WHEN** 用户动作没有改变状态或被边界拒绝
- **THEN** 系统不触发成功提示，并可触发 Boundary 或 Reject
