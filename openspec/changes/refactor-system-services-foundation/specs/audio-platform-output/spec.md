## MODIFIED Requirements

### Requirement: 同一 core 跨平台消费
SDL、T-Embed 与 headless SHALL 从 system sound service 的显式输出 port 消费同一 44,100 Hz、16-bit mono 渲染契约；AppRuntime SHALL 不拥有或暴露声音渲染器，平台层不得重新定义 cue 音色、语义、音量权威状态或 USB microphone 抑制策略。

#### Scenario: Headless 观测
- **WHEN** 测试提交 cue 系统命令、system host commit 并通过输出 port 拉取固定样本数
- **THEN** 测试可检查 PCM、峰值、结束静音、方向与确定性 hash 而无需设备

#### Scenario: 无输出设备
- **WHEN** 平台音频初始化失败或 headless 不创建设备
- **THEN** 图形、输入、Runtime、system service 和 voice input 继续运行，错误被报告而不是崩溃

### Requirement: 原版 T-Embed I²S task 输出
固件 SHALL 使用独立 output audio task 和 I²S0 DMA，通过 BCLK GPIO 7、WCLK GPIO 5、DOUT GPIO 6 输出 44,100 Hz、16-bit right-left frames；adapter SHALL 把 system sound output 的每个 mono sample 复制到左右 slot，并 SHALL 能与 I²S1 microphone capture task 共存而不共享可变 buffer。

#### Scenario: 固件初始化
- **WHEN** 锁定 SDK 的 output I²S driver 可用
- **THEN** driver 以指定格式和引脚启动，固定 task 持续供给 DMA，产品默认增益不采用厂商示例最大值

#### Scenario: Mono 转 I²S frame
- **WHEN** system sound service 产生一块 mono PCM
- **THEN** adapter 输出等量左右相同的 stereo frame，顺序与 MAX98357A/driver 配置一致

#### Scenario: 输入输出并发
- **WHEN** microphone I²S1 capture 与 speaker I²S0 output 同时 active
- **THEN** 两个 adapter 保持各自 task/DMA/buffer ownership，任一 consumer 落后不阻塞另一实时路径

#### Scenario: I²S 失败
- **WHEN** output driver 安装、设定引脚或写入失败
- **THEN** audio output adapter 禁用并报告诊断，主 Runtime、system services 和 microphone input 继续工作

### Requirement: 验证与真机声明
项目 SHALL 自动验证 portable sound service、命令队列、语义路由、静音/USB 抑制、SDL dummy callback、无设备降级、firmware 编译、I²S output frame 和与模拟 microphone consumer 的并发；项目 SHALL 明确区分自动化证明与实体 T-Embed 的响度、破音、延迟、回录、输入输出时钟和疲劳度验证。

#### Scenario: 自动化门禁
- **WHEN** 执行 host、desktop、shared-source 与 firmware 检查
- **THEN** system sound/并发测试和既有回归通过，桌面与固件目标成功构建

#### Scenario: 无实体设备
- **WHEN** 当前环境不能采集 T-Embed 扬声器与麦克风输出
- **THEN** 文档不得声称最终听感、回录或并发时钟通过，并列出可重复的真机步骤与预期
