## ADDED Requirements

### Requirement: 系统 surface 使用固定层级与容量
系统 SHALL 以 base App、transient feedback、单个 interactive surface 和 persistent/critical indicator 的固定顺序合成画面，interactive surface 容量 SHALL 为一且不得使用无界 stack 或逐帧 heap 分配。

#### Scenario: System Menu 覆盖 App
- **WHEN** System Menu 在稳定 App 画面上打开
- **THEN** 系统先呈现冻结 App 背景，再呈现 Menu 和更高优先级 indicator，App 不得覆盖系统 surface

#### Scenario: Interactive slot 已占用
- **WHEN** 一个普通 interactive surface 已 active 且另一个普通请求到达
- **THEN** 新请求按稳定 busy/reject policy 处理，现有 surface、App 和 framebuffer 保持有效且诊断计数增加

### Requirement: 完整按钮序列只属于一个输入 owner
系统 SHALL 将一次 button press、long-press、release/click 序列锁存给唯一 App 或 System owner；打开、关闭或替换 surface 不得把同一序列的后续事件交给另一 owner。

#### Scenario: 长按打开菜单后释放
- **WHEN** 用户在 App 中按住按钮至 long-press 打开 System Menu 后释放
- **THEN** release 只完成系统 capture，Menu 不执行当前选项且 App 不收到 release/click

#### Scenario: 菜单关闭期间保持按下
- **WHEN** 一个菜单动作在按钮序列尚未完成时关闭 Menu
- **THEN** 该序列的剩余事件仍由 System 消费，直到新的空闲序列才允许 App 接收按钮输入

#### Scenario: Menu active 时旋转
- **WHEN** interactive System Menu active 且用户产生 turn input
- **THEN** turn 只改变 Menu selection，active App 不接收该 turn

### Requirement: System Menu 暂停 App 而不退出 App
系统 SHALL 在 System Menu active 时停止 active App 的普通 update、input 和 render，使用进入 Menu 前的已提交 framebuffer 作为冻结背景；system services SHALL 继续 begin/commit，关闭 Menu 不得产生 App `onExit` 或 `onEnter`。

#### Scenario: Menu 打开多个 frame
- **WHEN** Menu 保持 active 并经过多个 frame
- **THEN** App update/render 计数和背景像素保持冻结，系统时间、平台事件摄取和系统服务状态继续推进

#### Scenario: Resume 当前 App
- **WHEN** 用户选择 Resume 或关闭 Menu
- **THEN** 同一个 App 在下一稳定 frame 恢复 update/input，期间没有 exit/enter 生命周期事件

### Requirement: Menu 与 App transition 的组合确定
普通 Menu 请求在 App transition 期间 SHALL 捕获触发输入并 defer 到 transition 完成后的首个稳定 frame，且不得冻结半完成 transition 或把输入交给 outgoing/incoming App。

#### Scenario: 转场中长按
- **WHEN** 用户在 App transition 进行时触发 Menu long-press
- **THEN** 系统捕获完整按钮序列，正常完成 transition，然后在目标 App 的稳定画面上打开 Menu

### Requirement: Surface 请求与结果异步且可诊断
系统 surface 的 open、action、close、reject SHALL 在明确 frame 阶段推进，不得从 draw 或 App callback 同步重入 Runtime/SystemService；容量耗尽、busy、invalid action 与 denied owner SHALL 产生稳定拒绝原因和累计诊断。

#### Scenario: Action 在 render 前提交
- **WHEN** Menu action 修改一个系统设置
- **THEN** action 通过 typed command 在 frame commit 阶段执行，Menu render 读取 commit 后 snapshot 且 draw 调用不改变 authoritative state

#### Scenario: 无效 action
- **WHEN** surface 收到当前 model 不支持的 action id
- **THEN** action 被拒绝、现有 surface 保持可用并记录 stable invalid-action diagnostic

### Requirement: 非交互反馈不得抢占 App 输入
Transient feedback 和 passive indicator SHALL 可覆盖 App 或 Menu 画面但不得成为按钮/turn owner；其容量、过期、去重或丢弃策略 SHALL 有界且可测试。

#### Scenario: Toast 显示期间操作 App
- **WHEN** 无 interactive surface 且 transient feedback 可见
- **THEN** App 继续 update 并接收输入，feedback 按注入时间过期而不阻塞 App
