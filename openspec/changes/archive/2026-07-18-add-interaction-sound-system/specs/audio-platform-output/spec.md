## ADDED Requirements

### Requirement: 同一 core 跨平台消费
SDL、T-Embed 与 headless SHALL 消费 portable core 的同一命令与 44,100 Hz、16-bit mono 渲染契约；平台层不得重新定义 cue 音色或语义。

#### Scenario: Headless 观测
- **WHEN** 测试触发 cue 并同步消费命令、拉取固定样本数
- **THEN** 测试可检查 PCM、峰值、结束静音、方向与确定性 hash 而无需设备

#### Scenario: 无输出设备
- **WHEN** 平台音频初始化失败或 headless 不创建设备
- **THEN** 图形、输入和 Runtime 继续运行，错误被报告而不是崩溃

### Requirement: SDL AudioStream callback 输出
桌面应用 SHALL 请求 44,100 Hz、S16、mono AudioStream，由 callback 按设备请求持续消费命令和生成 PCM；音频 SHALL 按真实设备时间而非 simulation speed 运行，并 SHALL 安全停止、销毁。

#### Scenario: 桌面交互
- **WHEN** AudioStream 已启动且主线程提交 cue
- **THEN** callback 在后台生成 PCM，图形循环不被同步播放阻塞

#### Scenario: 调试暂停
- **WHEN** simulation 被暂停且已有 cue 尚未结束
- **THEN** 已有 cue 按真实音频时间自然结束，暂停期间不因 App update 产生新 cue

#### Scenario: 生命周期关闭
- **WHEN** 桌面应用退出
- **THEN** stream 在 Runtime/host 析构前停止并销毁，callback 不访问失效状态

### Requirement: 原版 T-Embed I²S task 输出
固件 SHALL 使用独立 audio task 和 I²S DMA，通过 BCLK GPIO 7、WCLK GPIO 5、DOUT GPIO 6 输出 44,100 Hz、16-bit right-left frames；adapter SHALL 把每个 core mono sample 复制到左右 slot。

#### Scenario: 固件初始化
- **WHEN** 当前 Arduino-ESP32 2.0.17 legacy I²S driver 可用
- **THEN** driver 以指定格式和引脚启动，固定 task 持续供给 DMA，产品默认增益不采用厂商示例最大值

#### Scenario: Mono 转 I²S frame
- **WHEN** core 产生一块 mono PCM
- **THEN** adapter 输出等量左右相同的 stereo frame，顺序与 MAX98357A/driver 配置一致

#### Scenario: I²S 失败
- **WHEN** driver 安装、设定引脚或写入失败
- **THEN** audio adapter 禁用并报告诊断，主 Runtime 继续工作

### Requirement: 验证与真机声明
项目 SHALL 自动验证 portable core、命令队列、语义路由、静音、SDL dummy callback、无设备降级、firmware 编译和 I²S frame；项目 SHALL 明确区分自动化证明与实体 T-Embed 的响度、破音、延迟和疲劳度验证。

#### Scenario: 自动化门禁
- **WHEN** 执行 host、desktop、shared-source 与 firmware 检查
- **THEN** 新音频测试和既有回归通过，桌面与固件目标成功构建

#### Scenario: 无实体设备
- **WHEN** 当前环境不能采集 T-Embed 扬声器输出
- **THEN** 文档不得声称最终听感通过，并列出可重复的真机步骤与预期
