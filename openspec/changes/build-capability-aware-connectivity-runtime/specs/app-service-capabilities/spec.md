## ADDED Requirements

### Requirement: App 通过静态 descriptor 声明系统能力
每个注册 App SHALL 具有包含 `AppId`、显示元数据和固定 capability set 的静态 descriptor；Runtime SHALL 在 begin 前拒绝 invalid id、duplicate id、unknown capability bit 或超容量注册，新增 App 默认不得获得敏感能力。

#### Scenario: 新 App 默认最小权限
- **WHEN** 组合根注册一个未声明 provisioning、network lease 或 voice 能力的新 App
- **THEN** App 可以参加生命周期与渲染，但相关敏感 operation 均被拒绝且 driver 不被调用

#### Scenario: 非法 descriptor 被拒绝
- **WHEN** 组合根注册 invalid/duplicate AppId 或包含未知 capability 的 descriptor
- **THEN** Runtime 在开始执行 App 前返回稳定错误并保持既有 catalog 不变

### Requirement: App operation 携带不可由 App 伪造的 caller identity
Runtime SHALL 向 current App 提供 caller-bound operation port，port SHALL 自动附加 current `AppId` 与 generation；App-visible API MUST NOT 接受任意 caller id，系统与 USB owner SHALL 使用独立 owner kind。

#### Scenario: App 提交授权 operation
- **WHEN** current App 通过其 bound port 提交 descriptor 已授权的 operation
- **THEN** queue envelope 包含该 AppId、operation 和 generation，且 App 无需也不能传入另一个 AppId

#### Scenario: 系统操作不冒充 App
- **WHEN** Runtime 导航音或 USB lifecycle 提交系统 operation
- **THEN** envelope owner 为 System 或 Usb，而不是任一 bundled AppId

### Requirement: 授权在入口和 commit 双层验证
caller-bound port SHALL 在提交时按 descriptor 拒绝未授权 operation；`SystemServiceHost` SHALL 在 commit 时使用受信任 capability resolver 再次验证，拒绝 SHALL 区分 capability denied、invalid caller、stale generation 与 service unavailable。

#### Scenario: 绕过入口的未授权 envelope
- **WHEN** test double 或错误 adapter 直接向 system queue 注入 caller 无权执行的 operation
- **THEN** commit 拒绝该 envelope、增加 capability-denied 诊断且不改变 authoritative state

### Requirement: App 生命周期触发前台能力回收
Frame transaction SHALL 检测 active AppId 的实际切换，并在同一帧 commit 前回收退出 App 的全部 foreground resource leases；App `onExit()` SHALL NOT 需要直接调用 system service。

#### Scenario: 使用语音的 App 退出
- **WHEN** App A 持有 voice-analyzer foreground lease 并在 transition 中切换到 App B
- **THEN** A 的 lease 在 swap 帧自动回收，render snapshot 不再把 A 计为 microphone owner

#### Scenario: 持久系统 lease 不受 App 退出影响
- **WHEN** App A 退出时系统 owner 仍持有同一 resource 的 persistent lease
- **THEN** 只回收 A 的 foreground entry，resource desired state 保持 active

### Requirement: Capability 是可审计策略边界而非 native 安全沙箱
项目文档和公共 API SHALL 明确 bundled App 共享同一 native 地址空间；自动化 SHALL 证明正常入口的最小授权和 source dependency，不得把 descriptor 声称为对恶意 native code 的内存隔离。

#### Scenario: 生成平台安全说明
- **WHEN** change 准备交付
- **THEN** 文档区分 policy enforcement 与 process isolation，并把第三方 App sandbox 留作独立 change
