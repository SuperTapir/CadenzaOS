## ADDED Requirements

### Requirement: Timer is the only current product identity
系统 SHALL 在产品文案、C++ API、host 成员、注册、测试场景、诊断标签、文档与资产中使用 Timer；`TimerApp` 与 `kTimerAppId` SHALL 取代旧 Clock 符号，且 `kTimerAppId` SHALL 保持数值 `0x0101`。系统 MUST NOT 提供旧符号兼容 alias。

#### Scenario: 全仓迁移完成
- **WHEN** 执行源码、测试、文档和资产名称审计并构建所有目标
- **THEN** 现行产品实现只暴露 Timer 命名，且 catalog ID 仍为 `0x0101`

### Requirement: Timer uses native industrial numeral atlases
Timer SHALL 使用仅含 `0–9` 与冒号的双 profile 原生 1-bit bitmap atlas 组合 `MM:SS`，MUST NOT 对通用字体进行运行时缩放。数字 SHALL 等宽，冒号 SHALL 由两个光学校正的倒角方点构成。

#### Scenario: 双 profile 绘制代表时间
- **WHEN** 在 320×170 与 400×240 profile 绘制 `10:00`、`05:00` 和 `00:01`
- **THEN** 时间体为 228×84 与 308×120，数字 advance 等宽、冒号光学居中且 geometry diagnostics 为零

### Requirement: Timer state changes have local presentation feedback
TimerApp SHALL 为 Starting、Pausing、Resuming 持有不改变服务 snapshot 的 presentation elapsed。Normal Motion SHALL 为 Start/Resume 使用约 240/180 ms 的左至右局部扫光，为 Pause 使用约 180 ms 的右至左逆向扫光；Reduced Motion SHALL 使用约 100 ms 的无位移高对比边框。

#### Scenario: 启动暂停恢复
- **WHEN** 相应 Timer 命令成功提交
- **THEN** 首帧、中点和末帧呈现对应局部反馈，且既有 ToggleOn/ToggleOff 声音保持不变

#### Scenario: App handoff 清理反馈
- **WHEN** Timer App 在反馈结束前离开并重新进入
- **THEN** 未完成反馈被清除且不会在返回后继续播放

### Requirement: Timer presentation remains allocation free
Timer 绘制和状态反馈 SHALL 使用固定资产与栈/对象内状态，不得在 steady-state update/render 中产生运行时分配。

#### Scenario: 反馈循环接受 allocation 审计
- **WHEN** 对 Start、Pause、Resume 及其关键帧执行 allocation harness
- **THEN** 运行时分配计数保持为零
