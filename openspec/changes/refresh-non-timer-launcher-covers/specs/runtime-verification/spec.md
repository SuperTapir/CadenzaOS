## ADDED Requirements

### Requirement: 新版 Cover 的来源、端点与固件成本可验证
验证套件 SHALL 覆盖三张批准母图的双 profile 来源复现、PBM/header 一致性、Launcher 静态交互、launch p0、return bridge、snapshots、完整 build matrix 与 firmware size gate；TIMER 资产与 hash SHALL 保持原批准值。

#### Scenario: 三组批准资产进入集成
- **WHEN** 新 source、PBM 与 headers 纳入构建
- **THEN** 资产、immutability、handoff、snapshot、host/desktop/firmware-compatible build 与 size gate 全部通过

#### Scenario: 派生资产发生漂移
- **WHEN** PBM 无法由记录母图复现或 header 与 canonical PBM 不一致
- **THEN** 验证指出具体 App/profile/文件并失败，不允许用刷新 snapshot 掩盖错误
