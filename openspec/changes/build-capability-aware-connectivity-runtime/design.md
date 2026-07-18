## Context

前置 change `refactor-system-services-foundation` 已经建立 `cadenza_core → cadenza_system → platform adapter` 的单向依赖、冻结 snapshot、typed command、固定容量队列和一个麦克风 owner/多个 consumer 的 voice core。它也在 ESP-IDF 5.5 candidate 中证明了显示、输入、I²S0 output、ES7210/I²S1 input、UAC2+CDC 和 bundled Apps 能进入同一个 ELF，但实体 T-Embed 与 Mac 验收仍 pending。

当前连接边界只有：

```text
ConnectivitySnapshot { wifi: ConnectivityState, bluetoothLe: ConnectivityState }
ConnectivityState = unavailable | disabled | idle | connecting | connected | error
```

这个模型把四种不同语义混在一起：无线资源启停、Wi-Fi AP association、IP readiness 和 BLE GAP role。ESP-IDF 5.5 源码明确要求 `WIFI_EVENT_STA_CONNECTED` 后等待 `IP_EVENT_STA_GOT_IP` 才能开始 socket 工作，并要求区分显式 disconnect 与外部掉线；NimBLE 则独立报告 advertising、scanning、connect/disconnect。继续使用通用 `Connected` 会向 App 提供错误事实。

App 边界也只有一个无调用者身份的 `SystemCommandSink`。这可以阻止 driver 泄漏，却无法证明谁开启了网络或麦克风、谁能管理配网，以及 App 退出后谁负责释放资源。当前所有 bundled Apps 同进程、静态链接，因此 capability 只能提供组合与策略约束，不是抵御恶意 native code 的安全沙箱。

研究证据与采用边界记录在 `docs/connectivity-capability-reference-research.md`，其中锁定并阅读了：

- ESP-IDF `v5.5` / `8c750b088c7cd857d079c0eeb495da199b359461`；
- Zephyr Connection Manager `v4.4.0` / `684c9e8f32e4373a21098559f748f06915f950c9`；
- Tock / `7a79a1d119f85e6fb5945cee27e341334f42e4e8`；
- Fuchsia Component Framework capability routing 文档；
- ESP-IDF NVS encryption、RF coexistence、Wi-Fi provisioning 与 NimBLE 关键源码。

本 change 在无硬件阶段必须交付完整的 portable 行为、fake adapter、SDK candidate build 与资源报告；实体 RF、功耗、凭据落盘、真实配网和语音共存仍使用独立硬件门禁。

## Goals / Non-Goals

**Goals:**

- 让每个 App 的系统操作携带稳定 caller identity，并由静态 descriptor 声明/路由能力。
- 用固定容量、owner-aware lease 管理网络、BLE role、voice analyzer 等长生命周期资源，自动回收退出 App 的 foreground lease。
- 建立 transport-specific Wi-Fi、BLE 与 provisioning portable 状态机，严格分离 desired policy、observed state、readiness、failure 与 generation。
- 保持所有 SDK callback 在 adapter execution context 中只做有界归一化入队，由 system loop 按固定 budget 推进状态。
- 在 ESP-IDF 5.5 candidate 中加入 Wi-Fi station、NimBLE 与 Security 2 provisioning，保持现有固件回滚路径。
- 对 credential、PoP、peer identity、日志与 crash diagnostic 建立明确的敏感数据边界。
- 通过 deterministic host、source-boundary、strict warning、sanitizer、candidate build、size/task/stack/heap 与实体矩阵分层验收。

**Non-Goals:**

- 不实现 Classic Bluetooth、HFP/A2DP、Bluetooth LE Audio 或普通 GATT 音频。
- 不把 Wi-Fi/BLE 描述为 macOS 系统麦克风；USB Audio Class 仍是首条系统输入路径。
- 不交付动态第三方 App、native plugin ABI、MPU process、Wasm sandbox 或 cryptographic App signing。
- 不引入 Zephyr/Tock/Fuchsia runtime、Matter stack、全局 event bus 或新的通用协程调度器。
- 不在本 change 交付云账号、OTA、远程语音识别、网络 socket 通用 API 或任意 raw PCM 广播。
- 不以成功编译或 host 模拟替代实体 Wi-Fi/BLE RF、功耗、真实配网、NVS 加密和 USB voice 长稳证据。

## Decisions

### 1. 连接服务按语义分层，不共享线性状态枚举

portable snapshot 使用三个独立值类型：`WiFiSnapshot`、`BluetoothLeSnapshot` 与 `ProvisioningSnapshot`。Wi-Fi 分别表达 availability、desired policy、radio lifecycle、station phase、IP readiness、failure、retry 和 generation；BLE 分别表达 radio、advertising、scanning、connection count、provisioning role、failure 和 generation。

对普通 App 另提供一个小型 derived summary，例如 `networkOnline` 和 `provisioningActive`。summary 只由详细状态推导，不能反向驱动 service。

备选方案是在现有 `ConnectivityState` 增加 `ObtainingIp`、`Advertising`、`Scanning` 等枚举。该方案会产生对 Wi-Fi 合法但对 BLE 无意义、或反之的组合，也不能表达 advertising 与 connection 并存，因此不采用。

### 2. desired policy、observed driver state 与 readiness 独立

service 保存 App/system owner 推导出的 desired policy；adapter event 只修改 observed state。Wi-Fi `online` 首阶段定义为 radio ready、STA associated 且持有有效 IP。`got_ip` 不等于互联网可达，因此不增加 captive portal/DNS probe；出现真实 consumer 后另立 capability。

显式用户断开清除/释放对应 desired lease；外部掉线但 persistent owner 仍存在时进入 retry。auth failure、no AP、DHCP timeout、driver failure 和 retry exhausted 使用 portable failure class，不把 ESP reason code 变成公共 ABI；raw code只进入受限 diagnostic numeric field。

备选方案是 callback 内直接调用 `esp_wifi_connect()`。它无法可靠区分用户断开、旧 generation callback 与策略变化，并把重试时序藏在 SDK task，因此不采用。

### 3. operation 使用 caller-bound port 和双层授权

`AppCatalogEntry` 扩展静态 `AppDescriptor` 与 capability bitset。Runtime 每次 update 为 current App 构造 `AppCommandPort`，内部绑定 `AppId` 与受信任的 system sink；App API 不允许传入/伪造 caller。

command queue 保存 `SystemOperationEnvelope { owner, operation, generation }`。Runtime port 先按 descriptor 快速拒绝，`SystemServiceHost` commit 时再通过只读 capability resolver 验证 caller，避免未来错误 adapter 或测试 double 绕过入口。拒绝区分 unavailable、capability denied、capacity、stale generation 和 invalid operation。

系统导航音、USB voice 与 platform lifecycle 使用显式 `ResourceOwner::System` / `Usb`，不伪装成 App。

备选方案是给所有 App 同一个 `SystemCommands` service locator，并只在 UI 隐藏按钮。它不能审计、测试或自动清理权限，因此不采用。备选的 C++ capability object 也不能在同地址空间抵御恶意代码，因此仅采用声明与 caller routing，不作安全隔离声明。

### 4. 长生命周期资源使用 owner-aware lease，不使用全局布尔或裸引用计数

`SystemResourceLeaseTable` 使用固定容量 entry，key 为 `(ResourceOwner, ResourceKind)`，value 包含 lifetime policy 与 generation。`acquire`/`release` 幂等，重复 acquire 不递增，unknown release 返回稳定诊断。resource desired state 由 owner 集合推导；最后一个 owner 释放后才允许 adapter stop/autosuspend。

初始 resource kinds：network-online、BLE advertising、BLE scanning、voice analyzer。Provisioning 作为排他 session 单独仲裁；USB voice 保持现有 system consumer，但使用统一 owner 诊断语义。raw voice capability 只定义拒绝/保留标识，不在本 change 暴露 PCM API。

每个 lease 标记：

- `Foreground`：App 离开 active 生命周期时自动回收；
- `Session`：显式完成、取消或 timeout 回收；
- `Persistent`：只允许系统或明确授权的 Settings/provisioning owner，跨 App 切换保留。

备选的 `get/put` counter 无法定位泄漏或 double release；全局 `setActive(bool)` 又会让一个 consumer 关闭另一个 consumer 的资源，因此不采用。

### 5. App 切换清理属于 frame transaction

`FrameCoordinator` 在 App update 前记录 current `AppId`，update 后比较新的 current id。若发生 swap，它在 `commitCommands()` 前调用 system host 回收旧 App 的 foreground leases；回收与 App 本帧提交的 operation 在固定顺序中处理，render snapshot 同帧反映新 owner 集合。

顺序固定为：

```text
ingest adapter events → advance timers/state → freeze update snapshot
→ App update/transition → detect exited App and enqueue cleanup
→ commit App operations → reconcile leases/desired state
→ freeze render snapshot → render
```

`onExit()` 不能直接调用 system service；Runtime 也不依赖 `cadenza_system`。FrameCoordinator 只通过窄 cleanup port 连接两者。

备选是在每个 App 的 `onExit()` 手写 release。异常路径或未来 App 很容易遗漏，且无法集中证明，因此不采用。

### 6. callback mailbox 按 producer execution context 所有

将现有硬编码 `PlatformEventMailbox` 提炼为 trivially-copyable `SpscMailbox<T, N>`，并保留兼容 alias。一个 mailbox 只能由一个 producer execution context 写入、一个 system task 读取。

ESP-IDF default event loop 上的 Wi-Fi、IP 与 provisioning callback 由同一串行 producer 执行，可进入同一个 Wi-Fi/provisioning ingress 以保留跨 event-base 顺序；NimBLE host callback 使用独立 ingress。若配置自定义 event loop/task，则必须实例化独立 mailbox，不能假设 event base 等于 producer。

system loop 用固定 per-ingress budget 和确定性 round-robin/priority 合并。队列满采用 drop-newest 并记录 source、high-water、rejected 和 last generation；不可覆盖尚未消费的边沿事件。

备选全局 MPSC bus 增加锁、动态 observer 与不可见 delivery 语义；多个 callback 直接共写当前 SPSC 会产生数据竞争，因此均不采用。

### 7. retry、timeout 与 generation 可模拟

所有时间由已有 simulation clock/frame `dt` 推进，不在 portable 层读取 wall clock。连接 attempt、scan 和 provisioning session 都分配单调 generation；迟到 event generation 不匹配时丢弃并计数。

Wi-Fi recoverable failure 使用配置化 capped exponential backoff；默认序列和上限在 spec/test 固定，jitter 若启用必须由注入的 deterministic source 产生。auth failure、no credentials 和明确 fatal error 不无限自动重试，转入 needs-user-action/fatal。

备选由 FreeRTOS timer/callback 自行重试会使 host 无法确定性重放，并让 stop/start race 跨多个 task，因此不采用。

### 8. provisioning 是排他、安全、可取消的 session

只有具备 `ProvisioningManage` 的 owner 能启动；同时最多一个 session。状态为 inactive、advertising、negotiating、applying、verifying、succeeded、failed、stopping。start 返回 generation，所有 callback 必须匹配。

ESP-IDF candidate 首选官方 Wi-Fi provisioning manager Security 2。Security 0 禁用；Security 1 是否为兼容保留由构建配置显式决定。PoP/verifier/device secret 不进入 repo 通用常量。credential 只存在于 ESP-IDF provisioning/credential store 边界，portable event 仅携带 success/failure class。

日志与 CDC diagnostic 进行结构化 redaction，普通 snapshot 不含 password、PoP、完整 SSID/BSSID 或 peer identity。Settings 如需显示网络名，未来使用独立 privileged bounded view，不扩张全局 snapshot。

量产启用 Secure Boot、Flash Encryption 与 NVS encryption 是实体/生产门禁；host test 只能证明 API 与日志不泄漏，不能替代 flash dump 验证。

### 9. ESP-IDF adapter 保持隔离 candidate

继续锁定 ESP-IDF 5.5 与现有 Arduino-as-component/UAC candidate。candidate 增加 `esp_wifi`、`esp_netif`、NVS、wifi_provisioning/protocomm 与 NimBLE，所有 component version/hash 进入 lock 和采用记录。

adapter 只做：SDK init/deinit、callback normalization、mailbox ownership、driver command 执行与 raw error mapping。App、portable service 和 tests 不包含 ESP-IDF header。连接 feature 使用 Kconfig 开关，旧 PlatformIO target 保持构建回滚。

构建成功后记录 binary、total image、DIRAM、free heap before/after init、largest block、task/stack high-water 和 Wi-Fi/BLE buffer配置。没有真机时 heap runtime 项明确 pending，而不是填估算值。

备选立即切换唯一 firmware baseline 会把 USB voice 尚未完成的实体风险与连接迁移绑定，回滚困难，因此继续双 target。

### 10. Settings 只交付最小可解释 UI

Settings 首阶段显示 Wi-Fi radio/station/readiness、BLE role、provisioning session 和归一化可操作 failure；允许经过 capability port 执行 connect policy、start/cancel provisioning 和必要 reset。它不显示 credential，不直接浏览任意 BLE peer，也不提供 socket API。

其他 bundled App 默认只能 observe derived summary；需要 network/voice lease 的 App 必须在 descriptor 中逐个授予。测试证明新增 App 默认无敏感能力。

## Risks / Trade-offs

- [Risk] descriptor 与 commit 双层授权增加 API 和测试面 → 使用单一 capability-to-operation mapping 表与 compile-time exhaustive switch，source audit 禁止 adapter 绕过 envelope。
- [Risk] App 同进程导致 capability 不是安全沙箱 → 文档与 API 使用 policy/access 命名，不宣称 isolation；第三方 App 单独研究 MPU/Wasm。
- [Risk] transport-specific snapshot 增大 `SystemSnapshot` 与每帧复制 → 只保存小型 enum/counter/flags，不放 scan list、SSID、IP string 或 peer record；用 `sizeof` static budget test 固定上限。
- [Risk] foreground cleanup 与 transition 中点顺序复杂 → FrameCoordinator trace test 覆盖 update、swap、cleanup、commit、render，同帧 privacy/network desired snapshot 使用 golden trace。
- [Risk] generation wraparound → 使用无符号 serial comparison/仅 equality 且 0 invalid；host wrap test证明旧 callback 不被误收，实际 session 数量远低于 32-bit空间。
- [Risk] drop-newest 可能丢失最终状态 → adapter event 包含 generation 和可查询 reconcile hook；overflow 后设置 resync-needed，system task向 driver查询 authoritative state或进入 degraded，不以猜测继续。
- [Risk] Security 2 增加 flash/RAM 与配套客户端复杂度 → candidate 独立 size/build；首阶段复用官方 provisioning app/protocol，不自创 crypto。
- [Risk] Wi-Fi/BLE task、RF 和 heap 压力破坏 USB voice → 无硬件阶段做 synthetic scheduler/buffer test与构建预算，实体阶段执行扫描/连接/流量/长录音矩阵。
- [Risk] retry 策略在大量设备下同步风暴 → 保留可注入 jitter；产品默认值在部署规模明确前标记候选，真机/多设备实验后定案。
- [Risk] 两个 active changes 共享基础文件 → 新 change 明确依赖当前工作树的前置重构；提交按 change 边界组织，若前置 change API 调整则先更新本 design/spec，不复制兼容 facade。

## Migration Plan

1. 保存当前 49 项 host、snapshots、desktop、PlatformIO 和 IDF candidate size 基线；增加现有 connectivity contract characterization tests。
2. 引入 capability enum、App descriptor、caller-bound operation envelope 与 source-boundary checks；为所有 bundled Apps 显式列出最小能力，保持 UI/snapshot golden 不变。
3. 引入 owner-aware lease table和 deterministic tests；迁移 voice analyzer全局布尔意图，加入 App transition自动 cleanup。
4. 用新 Wi-Fi/BLE/provisioning types替换通用 `ConnectivityState`，先接 fake adapter，完成 readiness、retry、generation、overflow/resync 和 secret tests。
5. 扩展 Settings 最小 UI与 headless E2E；其他 App 默认 observe-only。
6. 在隔离 ESP-IDF 5.5 candidate加入 Wi-Fi/NimBLE/Security 2 adapter、Kconfig、lock和资源报告；保持旧 target。
7. 运行 focused、host、strict warning、ASan/UBSan、snapshots、desktop dummy、current firmware、完整 IDF candidate、OpenSpec strict与 diff/source audit。
8. 真机到货后执行 RF、功耗、heap/stack、配网、credential storage、USB voice并发和长稳矩阵；只有实体项通过才关闭 hardware pending。

回滚单位为 capability routing、lease runtime、portable connectivity 和 ESP-IDF adapter 的独立提交。SDK adapter失败时保留 portable service/fake tests，但不得恢复通用 `Connected` 或无 caller command；产品 target继续使用旧 PlatformIO build，candidate feature关闭。

## Open Questions

- Security 2 的制造期 username/verifier 与 device secret 如何注入、轮换和恢复，需要在明确量产流程后定案；仓库不生成通用生产 secret。
- Wi-Fi radio 在无 owner 时立即 stop 还是保留 autosuspend grace period，需要真机启动延迟与功耗数据；portable lease支持可配置 delay，默认先使用确定性短 grace。
- Settings 是否显示 SSID、IP 与 bonded peer，需要独立敏感 view和隐私产品决策；首版只显示状态与 failure。
- retry jitter 的默认范围需要结合部署规模；host先证明可注入与有界，不把未经实验的数值写成平台定案。
- ESP-IDF provisioning BLE scheme结束后是否释放 NimBLE/controller，取决于设备控制 GATT 是否常驻；candidate同时构建 keep/release两种配置并以真机heap/功耗选择。
- 真正第三方小 App 是否需要 MPU process或Wasm sandbox，应在安装/分发需求出现时单独大调研；本 change仅为未来保留 capability语义。
