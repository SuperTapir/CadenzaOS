## ADDED Requirements

### Requirement: Portable状态机由deterministic fake adapter完整覆盖
项目 SHALL 用可注入时间与fake adapter自动覆盖Wi-Fi association/IP readiness、BLE roles、provisioning、retry/backoff、timeout、generation、failure mapping和driver rejection；测试 SHALL 断言action trace而不只断言最终enum。

#### Scenario: Wi-Fi完整成功trace
- **WHEN** fake adapter按start、associated、got-IP序列响应network lease
- **THEN** trace证明action/event/frame顺序且update snapshot从不提前报告online

### Requirement: Capability与lease失败模式有自动化门禁
项目 SHALL 覆盖默认无权限、双层授权、invalid caller、table full、duplicate acquire、unknown release、两个owner、foreground auto-cleanup、persistent保留和App transition同帧render语义。

#### Scenario: App切换资源回收E2E
- **WHEN** headless Runtime从持有voice/network foreground lease的App切换到无能力App
- **THEN** golden trace证明cleanup在commit前执行、其他owner不受影响且隐私状态正确

### Requirement: Callback并发、burst、overflow和resync可执行验证
项目 SHALL 对SPSC mailbox运行真实线程producer/consumer测试、wraparound、TSAN可用条件下检查，并对多ingress burst、budget、drop-newest、stale generation和resync建立deterministic tests。

#### Scenario: Mailbox索引wraparound
- **WHEN** 测试把读写索引推进跨过32-bit边界并保持生产/消费
- **THEN** capacity判断、FIFO与diagnostics仍正确，无未定义行为或旧event重现

### Requirement: Sensitive数据使用canary和边界审计
测试 SHALL 把唯一secret canary注入credential/PoP/peer fixture并遍历成功、失败、拒绝、timeout、diagnostic与headless dump；所有普通artifact和日志 SHALL不含canary，privileged test store SHALL明确销毁fixture。

#### Scenario: Secret redaction回归
- **WHEN** auth failure、queue overflow和crash-style diagnostic同时发生
- **THEN** stdout、snapshot dump、trace和serialized diagnostic均不包含canary

### Requirement: 软件交付运行完整质量门禁
每个阶段里程碑 SHALL 运行focused tests、完整host、strict warnings、ASan/UBSan、snapshot/golden、desktop dummy、source/core-alone audit、current firmware、完整ESP-IDF candidate、OpenSpec strict validation和`git diff --check`；任何例外 SHALL记录根因和影响范围。

#### Scenario: Candidate adapter变更
- **WHEN** ESP-IDF adapter、sdkconfig或component lock改变
- **THEN** current firmware与完整candidate均从clean/incremental适当构建，并更新size差异证据

### Requirement: 自动化证据与实体产品证据分层
项目 SHALL 把host/software、firmware build和实体hardware三层证据分别记录。实体层 SHALL至少覆盖真实配网、断连/重连、credential加密存储、Wi-Fi/BLE扫描与连接共存、RAM/heap/stack、功耗、RF、前台App、USB voice与至少30分钟稳定性；实体未执行时不得archive为完整产品能力。

#### Scenario: 无设备阶段完成软件工作
- **WHEN** 所有portable和candidate build任务通过但T-Embed仍未到货
- **THEN** software/build任务可完成，hardware任务保持unchecked且状态明确为hardware validation pending

#### Scenario: 实体门禁失败
- **WHEN** Wi-Fi/BLE coexistence导致USB voice overrun或功耗超过接受边界
- **THEN** change保持未归档，记录复现、counter、配置和采用限制后回到设计/实现循环
