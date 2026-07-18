## Why

当前系统服务基础已经阻止 App 直接依赖平台驱动，但 Wi-Fi 与 Bluetooth LE 仍被压缩成同一个模糊 `Connected` 状态，App command 也没有调用者身份、能力授予或退出回收语义。继续直接接入 ESP-IDF 会把关联、IP readiness、配网、BLE role、隐私资源和重连策略重新耦合进全局 Runtime，因此需要在首个连接 App 出现前建立可验证的连接状态机与能力路由。

## What Changes

- **BREAKING** 用 transport-specific Wi-Fi、Bluetooth LE 与 provisioning snapshot 取代通用 `ConnectivityState`；分别表达 desired policy、radio/link/IP readiness、role、failure、retry 和 generation。
- **BREAKING** App 注册改为静态 descriptor，声明可使用的系统能力；Runtime 向当前 App 提供 caller-bound operation port，系统服务按 `AppId` 验证授权并记录拒绝。
- 新增固定容量、owner-aware 的资源租约：网络、BLE role 和语音 analyzer 等资源由系统/App owner 获取，App 退出时自动回收，最后一个 owner 释放后才能 idle。
- 新增 portable connectivity service：在 system loop 中推进 Wi-Fi、BLE、配网、退避和超时状态机；平台 callback 只进入各 adapter 独立的 SPSC mailbox。
- 新增安全配网契约：凭据不进入 portable snapshot/command/日志，首个 ESP-IDF 候选使用官方 provisioning manager Security 2，并为 Secure Boot、Flash/NVS encryption 保留量产门禁。
- 在 ESP-IDF 5.5 candidate 中加入 Wi-Fi station、NimBLE 与 provisioning adapter；保持 host/CMake 和现有 PlatformIO 回滚 target 独立。
- 新增 deterministic host/fake-adapter、burst/overflow、generation、retry、capability denial、lease cleanup、secret redaction、candidate build 与实体 RF/功耗验收门禁。

## Capabilities

### New Capabilities

- `app-service-capabilities`: App descriptor、caller identity、能力路由、授权拒绝与前台资源自动回收。
- `system-resource-leases`: 固定容量 owner-aware 资源意图、幂等 acquire/release、最后 consumer 关闭与泄漏诊断。
- `connectivity-service-runtime`: Wi-Fi/BLE 分层 snapshot、desired/observed 状态机、generation、retry/backoff、错误与 system-loop transaction。
- `secure-device-provisioning`: 排他配网会话、凭据/日志边界、安全方案、取消/超时/重试与 reset 语义。
- `connectivity-platform-adapters`: ESP-IDF 5.5 Wi-Fi/IP/NimBLE/provisioning callback 归一化、独立 mailbox、SDK candidate 和资源预算。
- `connectivity-verification`: host、边界、candidate build、secret audit 与真机 RF/功耗/稳定性分层证据。

### Modified Capabilities

无。当前前置 change 尚未同步进主 specs；本 change 以新 capability 明确增量契约，并保持 `refactor-system-services-foundation` 的硬件验收独立 pending。

## Impact

- 影响 `AppCatalog`、`AppRuntime`、`AppUpdateContext`、`SystemServiceHost`、connectivity types、frame transaction、host/desktop/firmware composition roots 与相关 tests。
- 新增 portable connectivity/lease 模块、headless fake adapter、Settings 最小连接状态与操作界面，以及 ESP-IDF 5.5 candidate adapter。
- candidate 增加 ESP-IDF Wi-Fi、esp_netif、NVS、provisioning、protocomm 与 NimBLE 依赖；必须重新记录 flash、DIRAM、heap、task/stack 和 coexistence 配置。
- 不引入 Zephyr/Tock/Fuchsia 运行时依赖，不交付 Classic Bluetooth、BLE Audio、Wi-Fi 虚拟声卡、动态第三方 App 或 Mac driver。
