## MODIFIED Requirements

### Requirement: 同一 core 跨平台消费
SDL、T-Embed 与 headless SHALL 消费 portable core 的同一命令与 44,100 Hz、16-bit mono 合成渲染契约；平台层不得重新定义 cue 音色或语义。

#### Scenario: Headless 观测
- **WHEN** 测试触发任一 cue 并同步消费命令、拉取固定样本数
- **THEN** 测试可检查 PCM、峰值、包络、样本数、结束静音与确定性 hash 而无需设备

#### Scenario: 无输出设备
- **WHEN** 平台音频初始化失败或 headless 不创建设备
- **THEN** 图形、输入和 Runtime 继续运行，错误被报告而不是崩溃

### Requirement: 验证与真机声明
项目 SHALL 自动验证 portable 合成原语、延迟调度、命令队列、语义路由、静音、参考资产未嵌入、SDL dummy callback、无设备降级、firmware 编译和 I²S frame；项目 SHALL 明确区分自动化特征证明、用户上下文试听与实体 T-Embed 听感验证。

#### Scenario: 自动化门禁
- **WHEN** 执行音频特征、host、desktop、shared-source 与 firmware 检查
- **THEN** PCM golden、延迟/包络、新音频测试和既有回归通过，桌面与固件目标成功构建

#### Scenario: 无实体设备
- **WHEN** 当前环境不能采集 T-Embed 扬声器输出
- **THEN** 文档不得声称最终真机听感通过，并列出可重复的真机步骤与预期

#### Scenario: 参考资产隔离
- **WHEN** 在没有本地参考 WAV 的干净 checkout 构建全部目标
- **THEN** palette 仍可确定性构建和导出，且产物不包含参考 PCM
