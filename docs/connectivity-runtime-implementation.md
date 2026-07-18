# 连接与安全配网实现状态

日期：2026-07-18
OpenSpec change：`build-capability-aware-connectivity-runtime`

## Portable 契约

`ConnectivitySnapshot` 只包含 enum、flag、generation、计数和有界数值。Wi-Fi
分别表达 desired policy、radio、station association/IP readiness、failure 与 retry；
BLE 分别表达 radio、advertising、scanning、connection count 和 provisioning role。
普通 App 只消费 derived summary，不获得 SSID、BSSID、IP string、scan list 或 peer
identity。

每次 Wi-Fi connect 和 provisioning session 使用非零 generation。旧 generation 的
associated、got-IP、disconnect 或 provisioning callback 只增加 stale counter，不改变
权威状态。网络 desired state 由 owner-aware lease 推导；主动释放最后 lease 不重连，
外部掉线且仍有 owner 才进入有界 retry。

## 配网安全边界

portable `ProvisioningPlatformPort` 只有以下 generation-aware lifecycle 操作：

- reset stored credentials；
- start Security 2 provisioning；
- stop provisioning。

它没有 password、PoP、Security 2 verifier、device secret 或 credential 参数。上述
数据只能存在于 ESP-IDF platform adapter、官方 provisioning manager 和受控存储边界。
snapshot、operation、action trace、diagnostic、CDC 和 headless dump 均不得复制明文。

候选固件的默认方案是 ESP-IDF 5.5 官方 provisioning manager Security 2：

- Security 0 必须禁用，不能作为调试捷径进入产品配置；
- Security 1 仅可作为显式兼容 build option，并需要单独采用理由；
- 不自行设计握手、密钥交换或加密协议；
- 清除 credential 必须由具备 `ProvisioningManage` capability 的 Settings owner
  执行二次确认，普通连接失败不会自动 reset；
- session 排他、owner-bound、可取消、有 timeout，结束后释放 Session lease。

## Settings 交互

Settings 的固定 descriptor 具有 `NetworkAcquire` 与 `ProvisioningManage`，其他
bundled App 没有这些能力，新 App 默认也没有。UI 只显示 `WIFI: OFF/CONNECT/
ONLINE/ACTION` 和 `SETUP: START/ADVERTISING/PAIRING/APPLYING/STOPPING/FAILED/
CONFIRM RESET`。失败后的重新配网需要连续两次明确点击：第一次进入确认状态，第二次
才产生 `ResetCredentials`，随后以新 generation 启动 session。

当前 Settings 的产品语义是“连接控制与安全配网入口”，不是通用网络设备管理器：

- 点击 `WIFI` 获取/释放 owner-bound network lease；存在已保存凭据时由平台连接，
  没有凭据或认证失败时显示 `ACTION`；
- 点击 `SETUP` 启动/取消 BLE Security 2 配网，credential 由外部受信客户端提交，
  设备 UI 不接触密码；
- BLE advertising/scanning/connection 已有 capability-aware 系统命令、状态机、
  adapter 与桌面注入路径，但当前 Settings 没有暴露这两个调试开关。

本里程碑不加入 SSID 扫描列表、屏上密码输入、蓝牙 peer 列表、pair/bond/unpair、
remembered-device 管理。320×170 单旋钮设备上临时加入这些交互，会同时扩大
credential 输入、peer identity、持久化、分页、超时和删除确认契约。它们作为下一
版本的独立“Connectivity Manager UI”需求评估；当前安全配网入口已经足以完成首次
联网，应用则继续只通过 capability 和 lease 使用连接能力。

## ESP-IDF 5.5 candidate

platform adapter 已完成以下 build-level 组合：

- NVS、`esp_netif`、default event loop、Wi-Fi station 与 Wi-Fi/IP/provisioning
  callback ingress；
- 独立 Wi-Fi 与 NimBLE SPSC producer mailbox，在 service/main context 合并；
- NimBLE central/peripheral/broadcaster/observer、最多 3 connections、host task
  固定 core 0 / 4096 B stack，以及 software coexistence；
- 官方 provisioning manager + BLE scheme + Security 2；Security 0/1 在 Kconfig
  中关闭，弱 material hook 默认失败且不携带仓库 secret；
- display/input、全部 Apps、I²S0 output、ES7210/I²S1 input、UAC2+CDC 和连接服务
  进入同一个 ELF。

官方 ESP-IDF v5.5 `components/protocomm/src/transports/protocomm_nimble.c`
会自行初始化 NimBLE、覆盖 `ble_hs_cfg` callback 并管理 host task。普通 App BLE
与 BLE provisioning 因而由 adapter 做排他仲裁，不能在当前官方 transport 上假装
同时拥有 host。该限制、provisioning 结束后的角色释放和重新启动均列入真机矩阵。

直接依赖锁为 ESP-IDF 5.5 commit `8c750b088c7cd857d079c0eeb495da199b359461`、
Arduino-ESP32 3.3.10、`usb_device_uac` 1.3.1、TinyUSB 0.19.0~3、
`esp_codec_dev` 1.5.10；完整传递锁含 32 个组件。官方 checksum 逐文件复验
7,600/7,600 通过，未修改 managed component 源码。

clean candidate 结果：

| 配置 | binary | total image | static D/IRAM | D/IRAM 余量 |
|---|---:|---:|---:|---:|
| minimal UAC2+CDC | 242,256 B | 242,103 B | 62,371 B | 279,389 B |
| full hardware，无连接 adapter | 390,752 B | 390,607 B | 147,767 B | 193,993 B |
| full hardware + connectivity | 1,176,672 B | 1,176,519 B | 226,823 B | 114,937 B |

连接闭包相对 full hardware 增加 785,920 B binary、785,912 B image 与 79,056 B
static D/IRAM，因此改用官方 1.5 MiB single-app-large partition，仍余约 23%。
这证明可链接和静态预算尚可，不证明 runtime free heap、task high-water、RF 或功耗。

## 证据分层

当前已证明的是 host/software 与 firmware-build 边界：状态机、能力拒绝、secret
canary、50/50 host、49/49 strict-warning、49/49 ASan/UBSan、desktop dummy、
PlatformIO 与 ESP-IDF candidate 编译。以下不能由编译或 host canary 代替，
仍是设备到货后的实体/量产门禁：

- Secure Boot、Flash Encryption、NVS encryption 和制造 secret 注入；
- flash/NVS 读取验证及 crash/CDC 日志检查；
- BLE Security 2 客户端互操作；
- Wi-Fi/BLE coexistence、heap/stack、功耗、温升和 USB voice 稳定性。
