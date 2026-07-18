# 连接服务与 App 能力平台调研

日期：2026-07-18
状态：研究结论已收敛，产品实现尚未开始
关联方向：Wi-Fi、Bluetooth LE、语音输入与小 App 系统服务

## 结论摘要

当前 `refactor-system-services-foundation` 已经解决了最危险的反向依赖：App 不再
直接持有 ESP-IDF driver 或完整 Runtime，平台 callback 也不直接进入 App。然而，
现有 `ConnectivitySnapshot` 只有 Wi-Fi/BLE 两个通用状态，所有 App 又共享同一个
`SystemCommandSink`。它足以证明边界形状，却不足以直接建设长期连接平台。

下一阶段应采用以下结构：

```text
ESP-IDF Wi-Fi events ─→ WiFi adapter mailbox ─┐
ESP-IDF IP events   ─→ WiFi adapter mailbox ─┤
NimBLE GAP events   ─→ BLE adapter mailbox  ─┤
                                              ▼
                                     ConnectivityService
                                     ├─ desired policy / leases
                                     ├─ Wi-Fi state machine
                                     ├─ BLE role state
                                     ├─ provisioning session
                                     └─ retry / timeout / diagnostics
                                              │
                           immutable snapshot │ typed operations
                                              ▼
                         per-App capability-routed context
```

- Wi-Fi 必须分别表达 radio/admin intent、AP association、IP readiness 与错误原因；
  `WIFI_EVENT_STA_CONNECTED` 不能映射为 App 可用的 `online`。
- BLE 不复用 Wi-Fi 状态机。它至少分别表达 radio、advertising、scanning、连接数和
  provisioning role；“BLE connected”不等于“网络可用”。
- 配网是独立、排他的短期会话，不是 Wi-Fi 的一个普通连接状态。凭据不进入 App
  snapshot、通用 command 或日志。
- 多个 App/系统消费者对 Wi-Fi、BLE、麦克风等昂贵资源使用有 owner 的租约/意图；
  最后一个租约释放后才能 idle/autosuspend，App 退出时由 Runtime 自动回收。
- App 注册时声明能力，由组合根路由；命令携带稳定 `AppId`，service host 验证
  caller 与 capability。当前同一地址空间只能提供策略隔离与审计，不能声称安全
  隔离。
- 每个 SDK callback producer 使用自己的 SPSC mailbox；不同 callback task 不得
  共同写一个 SPSC 队列。system loop 才负责合并、排序和状态推进。

## 当前实现审计

权威代码：

- `lib/cadenza_core/include/cadenza/core/connectivity_types.h`
- `lib/cadenza_core/include/cadenza/core/app_context.h`
- `lib/cadenza_core/src/app_runtime.cpp`
- `lib/cadenza_system/include/cadenza/system/system_service_host.h`
- `lib/cadenza_system/include/cadenza/system/platform_event_mailbox.h`

已成立的基础：

- portable core/system 不包含 ESP-IDF 头；
- App update 只看到冻结 snapshot，并通过有界 command FIFO 写入 intent；
- callback-safe mailbox 和 system-loop ingest 已经有自动化门禁；
- voice capture 已使用单 hardware owner、多 consumer 独立队列；
- queue overflow、frame budget 和 rejection 都可诊断。

尚未成立的长期契约：

1. `ConnectivityState::Connected` 同时承载 Wi-Fi association、IP ready 和 BLE peer
   connection，语义不可判定。
2. snapshot 没有 desired/observed 分离；无法区分用户主动断开、暂时掉线和需要自动
   重连。ESP-IDF 明确要求应用区分主动 `esp_wifi_disconnect()` 与外部掉线。
3. 没有 `NoCredentials`、`AuthFailed`、`ApNotFound`、`DhcpTimeout`、backoff 或
   retry attempt，设置 App 无法给出正确操作建议。
4. 所有 App 得到相同 `SystemCommandSink`，command 不携带 caller；未来无法证明
   哪个 App 开启了麦克风、网络或 provisioning，也无法在 `onExit` 后自动回收。
5. `SetVoiceAnalyzerActive(true)` 是全局布尔意图。如果未来 App 开启后退出但忘记
   关闭，硬件可以继续保持 active；Wi-Fi/BLE 若复制此模式会出现同类泄漏。
6. `PlatformEventMailbox` 是正确的 SPSC 实现，但它只能有一个 producer。组合根
   必须为 Wi-Fi、IP、NimBLE、USB、mic 等实际 producer 明确分配 mailbox，不能
   为方便而把它们接到一个全局 ingress。

## 锁定参考、源码与许可证

### ESP-IDF 5.5

- 版本：tag `v5.5`
- commit：`8c750b088c7cd857d079c0eeb495da199b359461`
- 许可证：Apache-2.0；部分组件有各自声明
- 原始源码：<https://github.com/espressif/esp-idf/tree/8c750b088c7cd857d079c0eeb495da199b359461>
- 本地只读源码：`/Users/tapir/Development/esp-idf-v5.5`
- 直接阅读：
  - `docs/en/api-guides/wifi.rst`
  - `components/esp_wifi/src/wifi_default.c`
  - `components/wifi_provisioning/src/manager.c`
  - `components/wifi_provisioning/src/handlers.c`
  - `examples/provisioning/wifi_prov_mgr/main/app_main.c`
  - `examples/bluetooth/nimble/blecsc/main/main.c`
  - `examples/bluetooth/nimble/ble_gattc_gatts_coex/main/main.c`

关键源码证据：

- `WIFI_EVENT_STA_CONNECTED` 后才启动 DHCP；只有 `IP_EVENT_STA_GOT_IP` 才说明
  socket 工作可以开始。官方文档特别警告不要在拿到 IP 前启动 socket。
- `WIFI_EVENT_STA_DISCONNECTED` 同时可能来自显式 disconnect、AP 丢失、认证失败
  和链路中断；应用需要根据 desired intent 和 reason 决定是否重连。
- provisioning manager 自身维护 `IDLE → STARTING → STARTED → CRED_RECV →
  SUCCESS/FAIL → STOPPING`，并分别暴露 auth error、AP not found、attempts remaining。
  这证明 provisioning 不应被压缩进通用 `Connecting`。
- NimBLE GAP 独立报告 advertising complete、scan/discovery complete、connect 和
  disconnect；BLE peripheral 与 central role 不能用 Wi-Fi association 状态表达。
- Wi-Fi/BLE 共用单一 2.4 GHz RF，以时分方式共存。扫描、连接和大流量阶段会改变
  时间片与实时音频风险，必须保留 workload state 和真机矩阵。

采用：ESP-IDF adapter 将原始事件归一化为 Cadenza typed events，保留 reason、
generation 和 desired intent；采用官方 provisioning manager 的 Security 2 候选与
受支持的 BLE/SoftAP transport。

不采用：App 直接订阅 default `esp_event` loop；把 driver reason code 原样泄漏为
portable ABI；在 event callback 内重连、写 UI 或执行阻塞清理。

### Zephyr Connection Manager 4.4.0

- 版本：tag `v4.4.0`
- commit：`684c9e8f32e4373a21098559f748f06915f950c9`
- 许可证：Apache-2.0
- 原始源码：<https://github.com/zephyrproject-rtos/zephyr/tree/684c9e8f32e4373a21098559f748f06915f950c9>
- 本地浅克隆：`.research/references/system-zephyr`
- 直接阅读：
  - `include/zephyr/net/conn_mgr_connectivity.h`
  - `subsys/net/conn_mgr/conn_mgr_connectivity.c`
  - `subsys/net/conn_mgr/conn_mgr_monitor.c`
  - `include/zephyr/net/net_if.h`
  - `doc/connectivity/networking/conn_mgr/main.rst`
  - `include/zephyr/pm/device_runtime.h`
  - `subsys/pm/device_runtime.c`

关键源码证据：

- readiness 是 `oper-up && has IP && !ignored`，而不是单一 link-up flag。
- desired admin state、operational association 和 L4 readiness 分层；persistent、
  explicit disconnect、timeout、fatal error 和 no configuration 有不同处理。
- persistent 接口掉线后保留自动重连意图；显式 disconnect 则清除该路径。这正是
  Cadenza 当前 snapshot 缺失的 desired/observed 分离。
- runtime PM 以 usage count 实现 get/put；只有 usage 降到零才 suspend，并把
  unbalanced put 明确诊断为错误。

采用：分层 readiness、desired policy、typed recoverable/fatal errors、usage-count
资源思想和幂等 connect/disconnect。

不采用：引入 Zephyr 网络栈、`net_if`、Kconfig 或全局 `net_mgmt` bus；Cadenza 的
目标固件仍使用 ESP-IDF 5.5。

### Tock

- commit：`7a79a1d119f85e6fb5945cee27e341334f42e4e8`
- 许可证：Apache-2.0 OR MIT（文件级 SPDX）
- 原始源码：<https://github.com/tock/tock/tree/7a79a1d119f85e6fb5945cee27e341334f42e4e8>
- 本地浅克隆：`.research/references/system-tock`
- 直接阅读：
  - `kernel/src/capabilities.rs`
  - `kernel/src/grant.rs`
  - `capsules/core/src/button.rs`
  - `capsules/core/src/i2c_master.rs`

关键源码证据：

- 敏感 kernel operation 需要不可由普通模块构造的 capability object；trusted
  composition code 决定 capability 分发。
- capsule 的 per-process state 通过 `Grant<...>` 绑定 `ProcessId`，而不是放在
  一个全局 service boolean 中；最后一个订阅者离开后才关闭共享硬件中断。
- Tock 的保证建立在 Rust type safety、process identity 与隔离上。Cadenza 当前
  bundled App 是同一 C++ 进程，不能照搬其安全声明。

采用：组合根显式授予、caller identity、每 App session state、最后 consumer 释放
硬件，以及“存在能力”和“调用能力”分离。

不采用：动态 syscall ABI、process grant allocator、Tock kernel 或把 C++ bitmask
包装成虚假的安全沙箱。

### Fuchsia Component Framework（概念对照）

- 官方文档：<https://fuchsia.dev/fuchsia-src/concepts/components/v2/introduction>
- capability routing：
  <https://fuchsia.dev/fuchsia-src/development/components/connect>
- availability：
  <https://fuchsia.dev/fuchsia-src/concepts/components/v2/capabilities/availability>

采用其“provider 声明、consumer 声明、parent/composition route、能力可能不可用”
的可审计关系。不采用 component manager、动态 realm、IPC/FIDL 或运行时发现。
Cadenza 使用固定容量静态 descriptor，在链接期/启动期完成验证。

## 目标状态模型

### Wi-Fi

不要提供一个不断膨胀的 `ConnectivityState`。建议 portable snapshot 至少分为：

```text
WiFiSnapshot
  availability: unavailable | available
  desired:      disabled | idle_allowed | online_requested
  radio:        stopped | starting | started | stopping | error
  station:      idle | needs_credentials | associating | associated |
                obtaining_address | online | retry_wait | error
  provisioning: inactive | advertising | negotiating | applying |
                verifying | succeeded | failed | stopping
  failure:      none | no_credentials | ap_not_found | auth_failed |
                association_timeout | dhcp_timeout | driver | exhausted
  retryAttempt / retryDelay / rssi / hasIpv4 / generation
```

`online` 在第一阶段定义为 station 已关联且拥有有效 IP。是否真正能访问互联网是
另一能力（DNS/captive portal/internet probe），不能由 `got_ip` 推断；没有真实
consumer 前不提前加入主动探测和额外网络流量。

### Bluetooth LE

```text
BluetoothLeSnapshot
  availability
  desiredRadio
  radio:       stopped | starting | ready | stopping | error
  advertising: inactive | starting | active | stopping | error
  scanning:    inactive | starting | active | stopping | error
  connectionCount
  bondedPeerCount (若产品需要)
  provisioningRoleActive
  failure / generation
```

BLE scan、advertise 和 peer connection 是可并存或互斥受配置影响的 role，不应
折叠成线性 `Connecting → Connected`。首个产品 role 是安全配网与设备控制；普通
GATT 不宣称为 macOS 系统麦克风。

### 资源租约

建议 service 内部保存固定容量 owner table：

```text
owner = system | usb | AppId
resource = network-online | ble-advertising | ble-scanning |
           voice-analyzer | voice-raw-stream
policy = foreground | persistent | session
```

- acquire/release 幂等；重复 acquire 不增加无法配平的引用计数。
- foreground owner 在 App `onExit`/transition 时由 Runtime 自动释放。
- persistent 只允许系统/受授权 Settings 或 provisioning owner 使用。
- service 根据 owner 集合推导 desired state；driver event 只修改 observed state。
- generation 用于丢弃旧连接/扫描/配网会话的迟到 callback。
- queue full、owner table full、capability denied、stale generation 和 driver reject
  都有独立诊断。

这比裸 `get/put` counter 多一个 owner identity，可以检测 double release、资源泄漏
和退出未清理；同时保留 Zephyr/Linux usage-count 的“最后使用者关闭硬件”优点。

## App 能力路由

注册项应从只有 `id/app/name/visible` 扩展为静态 descriptor，例如：

```text
AppDescriptor
  id, app, name, visible
  capabilities:
    sound.play
    settings.read
    settings.write
    connectivity.observe
    network.acquire
    provisioning.manage
    voice.level.observe
    voice.analyzer.acquire
    voice.raw.acquire
```

运行时为当前 App 构造 caller-bound ports；service 接收的 operation 必须带 `AppId`
和 operation generation。snapshot 可按敏感度分层：普通连接可用性可全局只读，SSID、
peer identity、凭据和 raw PCM 不进入通用 snapshot。

能力验证的目的包括：

- 审计哪个 App 可以启用高功耗或隐私敏感资源；
- 拒绝 Settings 以外的 App 修改持久配置；
- 自动清理 owner session；
- 为未来真正的隔离运行时保留语义。

它不防御同一固件内的恶意 C++ 代码，因为代码仍可绕过公共接口访问同一地址空间。
如果未来支持用户安装的第三方 App，应另立 change 比较 MPU process、Wasm sandbox
和签名原生模块，不能把本阶段 descriptor 当作安全边界。

## 配网、凭据与隐私边界

- 优先评估 ESP-IDF provisioning manager Security 2（SRP6a + AES-GCM），Security 0
  不进入产品配置；Security 1 是否保留兼容要单独决策。
- proof-of-possession、用户名/verifier 或 device secret 由制造/首次启动流程提供，
  不硬编码仓库通用常量。
- Wi-Fi credential 仅由 platform credential store/driver 持有；portable event 只
  携带结果和归一化 failure，不携带密码。
- 量产配置要启用 Secure Boot、Flash Encryption 与 NVS encryption 的组合评估。
  NVS encryption 在 ESP-IDF 中用于保护 Wi-Fi driver 写入的 credential；只在 host
  mock 中“隐藏字段”不能替代设备存储安全。
- 日志、diagnostic、CDC 和 crash dump 禁止输出 password、PoP、完整 SSID/BSSID
  或 peer identity；如确需诊断，使用显式 redaction 和短 hash。
- provisioning session 必须有本地可见状态、超时、取消、重试上限和 factory-reset
  语义；远程 App 不能静默永久开启 BLE advertising 或麦克风。

## 失败案例与必须先写的测试

1. Wi-Fi 收到 `STA_CONNECTED` 但没有 `GOT_IP`：App 必须看到 obtaining-address，
   `networkOnline == false`。
2. 用户主动 disconnect 后收到 disconnected callback：不得自动重连。
3. 外部掉线且 persistent lease 仍在：进入 retry-wait，按可注入时钟退避后重试。
4. 旧 generation 的 got-IP/scan-complete callback：必须丢弃并计数。
5. provisioning 认证失败与 AP not found：保留不同 failure 和可操作 UI。
6. App A、B 同时 acquire network；A 退出后 B 仍在线；B 退出后才允许 idle。
7. App acquire voice analyzer 后异常退出：Runtime cleanup 在同一 transition transaction
   中释放 foreground lease，privacy indicator 最晚下一帧消失。
8. 未授权 App 请求 provisioning/raw voice：稳定拒绝，不触发 adapter 调用。
9. Wi-Fi、IP、BLE 三个 callback producer burst：各自 mailbox FIFO 正确，任一
   overflow 不破坏其他 producer；system merge 有固定 per-frame budget。
10. BLE scan + Wi-Fi retry + USB voice synthetic workload：host deterministic trace
    不饿死 frame，voice consumer overrun 只影响对应 consumer。
11. credential/diagnostic fixture 搜索：任何失败路径都不包含 secret plaintext。
12. owner table 满、重复 acquire/release、stale owner：返回明确 rejection，不能 wrap
    counter 或永久保持硬件 active。

## 采用与不采用决策

采用：

- transport-specific state machines + 小型 shared readiness summary；
- desired policy 与 observed state 分离；
- owner-aware、固定容量、生命周期自动回收的 resource lease；
- composition-root capability routing 与 caller-bound ports；
- per-adapter SPSC ingress，system loop 合并；
- ESP-IDF 5.5 官方 provisioning/NimBLE adapter，portable 层保持 SDK-free；
- deterministic time、generation、retry/backoff 和 failure injection host tests。

不采用：

- 一个统一 Wi-Fi/BLE `Connected` 枚举；
- App 直接操作 `esp_wifi_*`、NimBLE handle、socket 或 credential；
- 全局 event bus、多个 producer 共写 SPSC、callback 内推进 UI/service；
- 只靠引用计数、没有 owner identity 的资源生命周期；
- 现在引入动态插件 ABI、Wasm、Matter、Zephyr 网络栈或 Mac 虚拟声卡；
- 在没有实体设备与 Mac 录音证据时宣称 Wi-Fi/BLE/USB voice 产品完成。

## 建议的 change 边界

当前 `refactor-system-services-foundation` 应保持硬件验收 pending，不继续把所有连接
产品需求塞进同一个 change。另立 `build-capability-aware-connectivity-runtime`：

1. 先演进 App descriptor、caller-bound operation 与 owner lease；保持现有 App
   行为和 snapshots 不变。
2. 实现 portable Wi-Fi/BLE/provisioning state machine、fake adapter、deterministic
   retry/generation/overflow tests。
3. 在 ESP-IDF 5.5 candidate 增加 Wi-Fi station + provisioning Security 2 + NimBLE
   adapter，并记录新的 flash/RAM/task/stack/heap 构建预算。
4. Settings 增加最小状态/配网控制 UI；其他 App 首阶段只获得 observe 或按 descriptor
   acquire 能力。
5. 真机后执行 RF coexistence、功耗、credential storage、Mac USB voice 并发和长稳
   门禁；这些实体结果仍不得用 host test 替代。

这样可以在不阻塞现有 USB voice 真机验收的情况下继续开发连接平台，也避免一个
长期不归档的巨型 change 同时承载架构、SDK、音频、配网和产品 UI。
