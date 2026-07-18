## ADDED Requirements

### Requirement: Portable 层不依赖连接 SDK
core、apps和portable system public/private source SHALL 在C++17 host下构建且不包含Arduino、ESP-IDF、esp_wifi、esp_netif、NimBLE、NVS、protocomm或provisioning header；平台能力只通过typed adapter port和normalized event进入。

#### Scenario: Source boundary审计
- **WHEN** 扫描portable target include/source并单独链接host tests
- **THEN** 不存在平台header/symbol依赖，fake adapter可以完整驱动状态机

### Requirement: Callback按producer execution context使用独立SPSC ingress
每个可能并发的SDK producer SHALL 拥有独立固定容量SPSC mailbox；同一ESP-IDF串行event loop上的Wi-Fi/IP/provisioning callback MAY共享一个ingress以保留顺序，NimBLE host callback SHALL使用独立ingress。callback MUST NOT阻塞、分配逐event heap、调用App或直接推进portable service。

#### Scenario: 并发producer隔离
- **WHEN** ESP event-loop producer与NimBLE producer同时post burst
- **THEN** 两个mailbox各自保持FIFO和diagnostics，不发生多producer写SPSC的数据竞争

### Requirement: Adapter执行driver action并归一化结果
system task SHALL向adapter提交带generation的start/stop/connect/disconnect/advertise/scan/provision action；adapter SHALL把同步返回和异步callback映射为portable result/failure，不在SDK callback中自行实施产品retry策略。

#### Scenario: Driver同步拒绝connect
- **WHEN** esp_wifi connect action立即返回invalid-state
- **THEN** adapter发布当前generation的driver failure，portable service决定retry/degraded且App仍可render

### Requirement: ESP-IDF 5.5 candidate版本与配置可复现
candidate SHALL锁定ESP-IDF 5.5及解析后的Arduino/UAC/Wi-Fi provisioning/NimBLE依赖、sdkconfig与component hashes，并保持现有PlatformIO firmware为独立回滚target。连接feature SHALL可由显式Kconfig启停。

#### Scenario: Clean candidate build
- **WHEN** 在锁定ESP-IDF checkout和Python/toolchain环境从clean build directory构建完整hardware candidate
- **THEN** bundled Apps、display/input、I²S0/I²S1、UAC2+CDC、Wi-Fi、NimBLE与provisioning进入同一ELF且无本机framework patch

### Requirement: Candidate记录静态和运行时资源边界
构建证据 SHALL 记录binary、total image、DIRAM、task/stack配置、Wi-Fi/BLE buffer与coexistence Kconfig；实体运行 SHALL记录init前后free heap、largest block、task high-water与失败降级。无真机时runtime数值 SHALL明确pending。

#### Scenario: 无硬件资源报告
- **WHEN** candidate build成功但设备未到货
- **THEN** 报告包含可信静态size/task配置并把heap high-water、RF和功耗标为未验证，不填推测值

### Requirement: RF coexistence配置显式且不等于产品通过
同时启用Wi-Fi/BLE的candidate SHALL显式配置Espressif受支持的software coexistence和task affinity选择，并保留USB voice/audio workload；成功编译 MUST NOT 被视为扫描、吞吐、语音稳定或功耗通过。

#### Scenario: Coexistence build审计
- **WHEN** 检查完整candidate sdkconfig和map
- **THEN** coexistence选项与Wi-Fi/NimBLE task配置可追踪，真机矩阵仍保持pending
