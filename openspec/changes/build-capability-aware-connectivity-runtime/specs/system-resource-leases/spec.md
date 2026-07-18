## ADDED Requirements

### Requirement: 长生命周期资源按 owner 和 resource 唯一持有
系统 SHALL 使用固定容量 lease table，以 `(ResourceOwner, ResourceKind)` 为唯一 key；owner SHALL 至少区分 System、Usb 与 AppId，resource SHALL 至少支持 network-online、BLE advertising、BLE scanning 和 voice analyzer。

#### Scenario: 两个 App 同时需要网络
- **WHEN** App A 与 App B 分别 acquire network-online
- **THEN** table 保存两个可诊断 owner entry，任一 App 不能覆盖另一 App 的 lease

### Requirement: Acquire 与 release 幂等且容量失败安全
重复 acquire 同一 key SHALL 成功但不得增加无法配平的计数；重复/未知 release SHALL 返回稳定结果并可诊断；table 满时 SHALL 拒绝新 lease、保留所有已有 entry 且不得启动 resource。

#### Scenario: 重复 acquire
- **WHEN** 同一 App 对同一 resource 连续 acquire 两次后 release 一次
- **THEN** table 只存在一个 entry，并在 release 后不再把该 App 计为 owner

#### Scenario: Lease table 满
- **WHEN** capacity 已满且新 owner 请求 resource
- **THEN** 请求被拒绝、capacity counter 增加、既有 owner 与 desired state 不变

### Requirement: Resource desired state 由 owner 集合推导
service SHALL 从有效 lease 集合推导 resource desired state；一个 owner release 不得关闭其他 owner 仍需要的 resource，只有最后一个 owner 释放后才允许 stop 或 autosuspend。

#### Scenario: 最后 consumer 才关闭
- **WHEN** A、B 都持有 network-online，A 先 release、B 后 release
- **THEN** A release 后 desired 仍为 online-requested，B release 后才转为 idle-allowed

### Requirement: Lease 生命周期策略明确
每个 lease SHALL 标记 Foreground、Session 或 Persistent；Foreground 在 App exit 自动回收，Session 在完成/取消/timeout 回收，Persistent 仅允许 System 或明确授权 owner，并跨 App 切换保留。

#### Scenario: 未授权 persistent lease
- **WHEN** 普通 App 请求 persistent BLE advertising
- **THEN** service 以 capability denied 拒绝，且 table 不产生 entry

#### Scenario: Session timeout
- **WHEN** provisioning session 到达配置 timeout且没有完成
- **THEN** session lease 被回收、进入 timeout failure 并请求 adapter 停止相关 role

### Requirement: Lease 诊断可证明平衡和高水位
lease runtime SHALL 暴露 acquire、release、duplicate、unknown-release、denied、capacity-reject、auto-cleanup、active-count 与 high-water counters，并能按 resource 查询 owner count，不暴露敏感 owner 数据到普通 App snapshot。

#### Scenario: 自动清理可审计
- **WHEN** Runtime 因 App exit 回收两个 foreground lease
- **THEN** auto-cleanup 增加两次，各 resource owner count 正确，普通 snapshot 只显示 derived usage
