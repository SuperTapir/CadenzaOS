## ADDED Requirements

### Requirement: 配网是排他且有 generation 的会话
同时最多 SHALL 存在一个 provisioning session；只有具备 ProvisioningManage capability 的 owner 能启动。session SHALL 表达 inactive、advertising、negotiating、applying、verifying、succeeded、failed、stopping，并为每次 start 分配 generation。

#### Scenario: 并发启动
- **WHEN** session A active期间另一个owner请求启动session B
- **THEN** B以busy拒绝，A的generation和状态不变

#### Scenario: 旧会话成功回调
- **WHEN** session A已取消且B active时收到A的credential-success
- **THEN** callback作为stale丢弃，不修改B或Wi-Fi credential state

### Requirement: Candidate 使用受支持的安全配网方案
ESP-IDF 5.5 candidate SHALL 使用官方 Wi-Fi provisioning manager并默认启用 Security 2；Security 0 MUST 禁用，Security 1若保留 SHALL 由显式build配置和兼容理由控制。项目 MUST NOT 自创握手或加密协议。

#### Scenario: 默认 candidate 配置审计
- **WHEN** 检查sdkconfig、component lock和adapter初始化
- **THEN** Security 2已启用、Security 0不可选，依赖版本和hash可复现

### Requirement: Credential 和 provisioning secret 不跨越 portable 边界
password、PoP、Security 2 verifier、device secret与完整credential SHALL 只存在于受控platform credential/provisioning边界；portable command、event、snapshot、diagnostic、CDC日志、test failure与crash text MUST NOT 包含其明文。

#### Scenario: 认证失败日志
- **WHEN** 使用带唯一canary的credential触发所有失败和diagnostic路径
- **THEN** host输出、snapshot、event trace与candidate build artifact文本搜索不到canary

### Requirement: 配网支持取消、超时、失败重试与显式 reset
session owner SHALL 能取消；timeout SHALL 自动停止transport并释放session lease。auth-failed、AP-not-found与transport-failed SHALL 保留不同failure；重新尝试不得沿用旧generation。清除已配置credential SHALL 需要独立授权和显式确认语义，不因普通连接失败自动reset。

#### Scenario: 配网超时
- **WHEN** negotiating状态超过配置timeout
- **THEN** session进入stopping/failed、transport停止、lease释放且credential不被清除

#### Scenario: 显式重新配网
- **WHEN** 授权Settings owner确认reset并启动新session
- **THEN** platform按明确顺序清除credential、创建新generation并进入advertising

### Requirement: 本地隐私状态持续可见
provisioning advertising/negotiating与麦克风使用一样 SHALL 在设备UI持续显示状态；远端请求不得静默永久保持advertising，session结束、取消或timeout后最晚下一render snapshot清除指示。

#### Scenario: 远端中途断开
- **WHEN** provisioning client在negotiating中断开且session最终timeout
- **THEN** 本地指示持续到停止确认，随后清除并显示可操作failure

### Requirement: 量产存储安全与host证明分层
采用记录 SHALL 把Secure Boot、Flash Encryption、NVS encryption、制造secret注入和flash dump验证列为量产/实体门禁；host redaction test和candidate compile MUST NOT 被描述为credential-at-rest已经通过。

#### Scenario: 无真机交付状态
- **WHEN** 所有host与candidate build门禁通过但尚未执行加密flash读取验证
- **THEN** 状态明确为software/build complete、production credential storage pending
