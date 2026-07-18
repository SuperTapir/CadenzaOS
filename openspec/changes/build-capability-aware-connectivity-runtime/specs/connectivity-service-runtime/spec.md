## ADDED Requirements

### Requirement: Wi-Fi、BLE 和配网使用独立 snapshot
portable connectivity service SHALL 提供 `WiFiSnapshot`、`BluetoothLeSnapshot` 与 `ProvisioningSnapshot`，不得使用一个线性通用 `Connected` 枚举表达三者；普通 App SHALL 只从详细状态派生小型 connectivity summary。

#### Scenario: BLE advertising 与连接并存
- **WHEN** BLE peripheral 已有一个 peer connection且仍在 advertising
- **THEN** BLE snapshot 可同时表达 connection count 与 advertising active，不丢失任一事实

### Requirement: Wi-Fi desired、radio、association 与 IP readiness 分离
Wi-Fi snapshot SHALL 分别表达 availability、desired policy、radio lifecycle、station association、IP readiness、failure、retry 与 generation；`networkOnline` SHALL 仅在 radio ready、STA associated 且存在有效 IP 时为 true，不得把 AP association 或 `got_ip` 单独描述为互联网可达。

#### Scenario: 已关联但尚未拿到 IP
- **WHEN** 当前 generation 收到 STA connected 但尚未收到有效 got-IP
- **THEN** station 为 obtaining-address且 `networkOnline == false`

#### Scenario: IP 就绪
- **WHEN** 同一 generation 在 associated 后收到有效 got-IP
- **THEN** station 进入 online、`networkOnline == true`，但 snapshot 不声称 captive-portal 或互联网 probe 已通过

### Requirement: Desired policy 决定重连而非 callback 自行重连
driver event SHALL 只更新 observed state；service SHALL 根据 active lease、explicit disconnect、persistent policy 与 failure class 决定 connect/retry/stop。主动 disconnect 后不得自动重连，外部掉线且 persistent owner 存在时 SHALL 进入 retry。

#### Scenario: 用户主动断开
- **WHEN** user operation 释放最后 network lease并随后收到 STA disconnected
- **THEN** desired 为 idle-allowed且 service 不发出 reconnect action

#### Scenario: 外部掉线
- **WHEN** persistent network lease仍有效且当前连接因 beacon loss 断开
- **THEN** service进入 retry-wait并在可注入退避到期后请求新 generation connect

### Requirement: Portable failure 可操作且不泄漏 SDK ABI
Wi-Fi failure SHALL 至少区分 none、no-credentials、AP-not-found、auth-failed、association-timeout、DHCP-timeout、driver、retry-exhausted；BLE SHALL 提供独立 role/radio failure。raw SDK code MAY 进入受限 numeric diagnostics，但 MUST NOT 成为 portable public enum 或包含 secret。

#### Scenario: 凭据错误
- **WHEN** adapter 将 ESP authentication failure 映射到当前 generation
- **THEN** Wi-Fi snapshot 为 auth-failed/needs-user-action，不进行无限自动重试且 App 可显示重新配网建议

### Requirement: BLE role 独立于网络 readiness
BLE snapshot SHALL 分别表达 radio、advertising、scanning、connection count、provisioning role 与 generation；普通 BLE GATT connection MUST NOT 使 `networkOnline` 或 USB microphone 状态变为 true。

#### Scenario: Mac 连接控制 GATT
- **WHEN** BLE peer connection建立但 Wi-Fi 没有 IP且 USB alt-setting inactive
- **THEN** BLE connection count增加，networkOnline与USB voice active均保持 false

### Requirement: Retry、timeout 和迟到 callback 可确定性模拟
portable service SHALL 只使用注入/帧推进时间；每次 connect、scan 与 provisioning attempt SHALL 分配非零 generation。generation 不匹配的 callback SHALL 丢弃并计数；retry SHALL 使用配置化有界退避，任何 jitter MUST 可注入重放。

#### Scenario: 迟到 got-IP
- **WHEN** generation 4 已取消且 service 正在 generation 5 时收到 generation 4 got-IP
- **THEN** event 被标记 stale、snapshot 保持 generation 5 且 networkOnline不改变

#### Scenario: 有界退避
- **WHEN** recoverable failure连续发生超过配置次数
- **THEN** retry delay不超过上限，并在 attempt limit 后进入 retry-exhausted而非无限循环

### Requirement: Event storm 不饿死帧且 overflow 触发重同步
每个 ingress SHALL 有固定容量、per-frame budget、high-water与reject diagnostics；system loop SHALL 使用确定性合并顺序。边沿 event overflow 后 SHALL 标记 resync-needed并查询/重建 authoritative driver state或进入 degraded，不能静默猜测最终状态。

#### Scenario: BLE burst 不阻塞 UI
- **WHEN** NimBLE callback在一帧预算以上产生 burst
- **THEN** service只摄取预算数量、其余保留或明确拒绝，FrameCoordinator仍完成 update/render

#### Scenario: 最终状态 event 被拒绝
- **WHEN** ingress满导致 disconnect event被drop-newest
- **THEN** diagnostics设置resync-needed，随后 reconcile修正connection count或进入degraded

### Requirement: 通用 snapshot 保持有界且不含敏感字符串
connectivity fields SHALL 只包含固定大小 enum、flag、generation、计数与受限数值；通用 `SystemSnapshot` MUST NOT 包含 password、PoP、完整 SSID/BSSID、IP string、scan list或peer identity，并 SHALL 有静态 size budget test。

#### Scenario: 大量扫描结果
- **WHEN** adapter发现超过普通 snapshot容量的Wi-Fi AP或BLE peer
- **THEN** scan record不复制进SystemSnapshot，snapshot大小和每帧复制成本保持不变
