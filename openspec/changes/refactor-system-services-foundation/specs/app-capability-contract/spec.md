## ADDED Requirements

### Requirement: App 生命周期使用窄 context
App update context SHALL 只提供输入、App catalog/navigation、只读系统快照和系统命令 sink；render context SHALL 只提供绘制所需 framebuffer/catalog/system snapshot，并 SHALL 不暴露完整 `AppRuntime`。

#### Scenario: App 源码边界审计
- **WHEN** bundled App 与测试 App 源码被检查
- **THEN** App 不调用 `AppRuntime` service accessor、平台 API、I²S、USB、Wi-Fi 或 BLE callback

### Requirement: App 标识不硬编码于 core
`AppId` SHALL 是具有 invalid 值的稳定值类型，内置 App 常量 SHALL 位于 bundled Apps 层；core 不得枚举 Launcher、Timer、Motion、Settings、Gallery 或以后新增的小 App。

#### Scenario: 新增测试 App
- **WHEN** 组合根注册一个 core 未知的合法 AppId 和 App 实例
- **THEN** Runtime 可以进入、更新、渲染和退出该 App，且无需修改 core source

#### Scenario: 无效或重复标识
- **WHEN** 组合根注册 invalid 或已经存在的 AppId
- **THEN** registry 明确拒绝该注册且已有 catalog 不改变

### Requirement: App catalog 固定容量且可查询
App catalog SHALL 使用编译期或构造期固定容量，不在 frame loop 分配，并 SHALL 提供稳定注册顺序、metadata 查询和容量拒绝。

#### Scenario: Catalog 容量耗尽
- **WHEN** 组合根注册超过配置容量的 App
- **THEN** 超额 App 被拒绝，已注册 App 的 id、顺序和 metadata 保持不变

### Requirement: Home App 由组合根配置
Runtime SHALL 从组合根获得合法 Home AppId，并 SHALL 用它处理初始 entry 和系统 return gesture；core 不得假定 Launcher 的数值或类型。

#### Scenario: 自定义 Home App
- **WHEN** 组合根把一个测试 App 配置为 Home，且用户在另一 App 长按返回
- **THEN** Runtime 启动到并返回该测试 App，生命周期顺序与标准导航一致

#### Scenario: Home 未注册
- **WHEN** 组合根配置未注册的 Home AppId
- **THEN** 初始化明确失败或保持安全 inactive 状态，不得跳转到任意 registry slot

### Requirement: 各组合根显式装配同一应用集合
Headless、desktop 和 firmware composition root SHALL 显式链接 `cadenza_apps` 与 `cadenza_system` 并注册产品要求的同一 built-in catalog；`cadenza_core` SHALL 不编译 bundled App/Gallery 实现。

#### Scenario: Source target 审计
- **WHEN** CMake 与 firmware source manifests 被检查
- **THEN** bundled App source 只属于 Apps target，core target 可在不链接 Apps target 时单独编译
