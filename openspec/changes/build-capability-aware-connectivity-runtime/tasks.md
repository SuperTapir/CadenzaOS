## 1. 研究、基线与失败用例

- [x] 1.1 审计当前 connectivity snapshot、SystemCommandSink、AppRuntime transition、SystemServiceHost 与 SPSC mailbox，记录结构性差距
- [x] 1.2 锁定并阅读 ESP-IDF 5.5、Zephyr 4.4.0、Tock 与 Fuchsia capability routing 的版本、许可证、关键源码和采用边界
- [x] 1.3 将状态分层、能力路由、资源租约、配网安全、失败案例与真机未知记录到 `docs/connectivity-capability-reference-research.md`
- [x] 1.4 保存当前 host/desktop/PlatformIO/ESP-IDF candidate、snapshot hash、`sizeof(SystemSnapshot)` 与 firmware size 基线
- [x] 1.5 为现有通用 ConnectivityState、无 caller command 和 voice analyzer 退出不清理写 characterization/failing tests
- [x] 1.6 增加研究证据审计，锁定参考 commit、关键文件、许可证与原始链接，确保研究 clone 不成为构建依赖

## 2. 通用 mailbox 与 operation envelope

- [x] 2.1 先写 generic trivially-copyable SPSC mailbox 的 FIFO、full/drop-newest、diagnostics、真实线程与 index wraparound tests
- [x] 2.2 将 PlatformEventMailbox 提炼为 `SpscMailbox<T, Capacity>` 并保留必要兼容 alias，验证 host/TSAN 可用门禁
- [x] 2.3 定义 ResourceOwner、SystemOperationEnvelope、generation 与精确 rejection enums，写 invalid/stale/system/usb/app owner tests
- [x] 2.4 将 SystemServiceHost command queue 迁移到 envelope，拆分 App caller-bound submit 与受限 system submit
- [x] 2.5 更新 queue high-water/rejection diagnostics 与 source audit，禁止 App API 传入任意 caller id

## 3. App descriptor 与能力路由

- [x] 3.1 先写 App descriptor invalid/duplicate/unknown capability/capacity/default-deny tests
- [x] 3.2 定义固定 AppCapabilitySet、capability-to-operation mapping 与包含能力的 AppDescriptor
- [x] 3.3 扩展 AppCatalog/AppRuntime 注册 API并迁移所有 tests/composition roots，不引入动态分配
- [x] 3.4 为每个 bundled App 列出最小 capability，证明新增 App默认没有 provisioning/network/voice敏感能力
- [x] 3.5 实现 current-App-bound operation port，入口按 descriptor 拒绝未授权 operation
- [x] 3.6 向 SystemServiceHost 注入只读 capability resolver并在commit执行第二层授权
- [x] 3.7 增加 authorized、denied、invalid caller、错误adapter绕过与系统owner回归tests
- [x] 3.8 更新平台架构与安全说明，明确policy enforcement不等于native process sandbox

## 4. Owner-aware 系统资源租约

- [x] 4.1 先写双owner、重复acquire、unknown release、capacity full、last-owner和high-water tests
- [x] 4.2 实现固定容量 `SystemResourceLeaseTable`、ResourceKind 与 Foreground/Session/Persistent policy
- [x] 4.3 实现按resource owner count推导desired state，拒绝普通App persistent lease
- [x] 4.4 增加acquire/release/duplicate/denied/capacity/auto-cleanup diagnostics与查询API
- [x] 4.5 迁移voice analyzer从全局bool intent到App/System owner lease，同时保持USB独立consumer
- [x] 4.6 修改FrameCoordinator在active App实际swap后、command commit前自动回收旧App foreground leases
- [x] 4.7 增加transition中点、App退出、其他owner保留、privacy indicator同帧render golden trace

## 5. Portable connectivity 状态机

- [x] 5.1 先写Wi-Fi associated-before-IP、got-IP、explicit disconnect、external loss、failure mapping与online summary tests
- [x] 5.2 定义有界 WiFiSnapshot、WiFiDesiredPolicy、radio/station phases、failure、retry与generation
- [x] 5.3 先写BLE advertising/scanning/connection并存且不影响network/USB voice的tests
- [x] 5.4 定义有界 BluetoothLeSnapshot、radio/role states、connection count、failure与generation
- [x] 5.5 定义小型derived ConnectivitySummary并删除通用ConnectivityState公共语义
- [x] 5.6 实现desired/observed分离、network lease reconciliation与幂等driver action trace
- [x] 5.7 实现可注入时间的connect/scan timeout、capped backoff、attempt limit与deterministic jitter source
- [x] 5.8 实现generation分配、0 invalid、迟到callback丢弃和wraparound tests
- [x] 5.9 实现per-ingress frame budget、deterministic merge、overflow resync-needed与authoritative reconcile/degraded路径
- [x] 5.10 增加SystemSnapshot静态size budget和禁止SSID/BSSID/IP/scan list/peer identity的contract tests

## 6. 安全配网与 Settings 能力

- [x] 6.1 先写排他session、unauthorized/busy、generation、cancel、timeout、failure、retry和reset tests
- [x] 6.2 实现 ProvisioningSnapshot、session state machine、Session lease与owner arbitration
- [x] 6.3 定义platform credential/provisioning ports，确保portable types不携带password、PoP、verifier或device secret
- [x] 6.4 使用唯一secret canary覆盖成功/失败/overflow/diagnostic/headless dump并实现结构化redaction
- [x] 6.5 扩展Settings最小连接UI与操作，只显示radio/readiness/role/session/failure，不显示credential
- [x] 6.6 增加Settings start/cancel/reprovision确认、权限拒绝、持续本地状态与timeout清除E2E tests
- [x] 6.7 记录Security 2、Security 0禁用、Security 1兼容选项与生产secret/NVS安全待验证边界

## 7. Headless/desktop composition 与确定性验证

- [x] 7.1 实现fake connectivity/provisioning adapter与可检查driver action trace
- [x] 7.2 在headless host组合App descriptors、lease runtime、connectivity service和fake adapter
- [x] 7.3 增加Wi-Fi成功/失败/重试、BLE roles、配网、App切换与多owner headless scenario
- [x] 7.4 在desktop simulator加入不依赖真实网络的连接状态注入/调试路径并保持dummy smoke
- [x] 7.5 增加多ingress callback burst + 60 Hz frame + synthetic voice/USB consumer并发测试，证明不饿死UI
- [x] 7.6 扩展core-alone/shared-source audit，禁止portable target回引ESP-IDF、Arduino、socket、NVS或NimBLE

## 8. ESP-IDF 5.5 Wi-Fi/BLE/provisioning candidate

- [x] 8.1 锁定并记录candidate新增ESP-IDF内置/managed components、版本、hash、许可证和Kconfig
- [x] 8.2 实现ESP event-loop Wi-Fi/IP/provisioning ingress与generation-aware action adapter
- [x] 8.3 实现NimBLE radio/advertising/scanning/connection ingress与action adapter，保持独立producer mailbox
- [x] 8.4 接入官方Wi-Fi provisioning manager Security 2，禁用Security 0并避免仓库通用生产secret
- [x] 8.5 配置NVS、esp_netif、Wi-Fi station、NimBLE、software coexistence与明确task affinity候选
- [x] 8.6 将connectivity service和Settings UI组合进完整display/input/I²S0/I²S1/UAC2+CDC hardware ELF
- [x] 8.7 从clean build运行最小与完整candidate，审计ELF symbols、component lock和无managed source patch
- [x] 8.8 记录binary/total image/DIRAM/task/stack/buffer差异，将无真机free-heap/high-water/RF/power明确标pending
- [x] 8.9 保持current PlatformIO firmware独立构建回滚并记录两条可复现命令

## 9. 软件质量门禁与交付证据

- [x] 9.1 运行focused与完整host tests，确认新增失败用例和全部既有snapshot/golden
- [x] 9.2 运行独立strict-warning构建，vendor warning与产品source gate分开审计
- [x] 9.3 运行ASan/UBSan及可用条件下TSAN mailbox/adapter concurrency tests
- [x] 9.4 运行desktop dummy、current firmware、ESP-IDF minimal/complete candidate与size reports
- [x] 9.5 运行secret canary、source boundary、OpenSpec strict、`git diff --check`与task completion audit
- [x] 9.6 更新开发、平台架构、SDK采用、连接实现和未执行实体证据文档，状态写为software/build complete、hardware validation pending

## 10. 实体 T-Embed 连接、语音与安全验收（设备到货后）

- [ ] 10.1 验证真实Wi-Fi配网、Security 2客户端、credential reset、主动断开、外部掉线、重连退避和AP/DHCP失败
- [ ] 10.2 验证NimBLE advertising/scanning/control GATT、peer连接、provisioning role切换与BLE资源释放
- [ ] 10.3 读取/检查加密flash与NVS边界，验证Secure Boot/Flash Encryption/NVS encryption及日志/crash/CDC无secret
- [ ] 10.4 测量Wi-Fi/BLE init前后free heap、largest block、各task stack high-water、buffer、功耗与温升
- [ ] 10.5 执行Wi-Fi/BLE RF coexistence矩阵：idle/scan/connect/traffic × USB voice/App analyzer/I²S output
- [ ] 10.6 执行断连/重连、sleep/wake、前台App切换与至少30分钟USB录音，记录voice overrun/underflow/clock和连接counter
- [ ] 10.7 根据实体数据确定radio autosuspend grace、retry jitter、BLE keep/release与资源限制，回写design/spec和采用记录
- [ ] 10.8 所有实体门禁通过后同步主specs、关闭hardware pending并分别准备归档两个相关changes
