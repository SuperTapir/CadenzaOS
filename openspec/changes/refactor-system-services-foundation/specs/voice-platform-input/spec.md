## ADDED Requirements

### Requirement: T-Embed 麦克风 adapter 使用板级配置
T-Embed adapter SHALL 通过显式板级配置初始化 ES7210 与 I²S input，默认使用已调研的 SDA 18、SCL 8、BCLK 47、LRCK 21、DIN 14、MCLK 48 和 I²S1；board pin、channel、gain 与 clock 参数不得进入 portable core。

#### Scenario: 固件配置审计
- **WHEN** T-Embed microphone target 被编译和检查
- **THEN** codec/I²S/pin API 只存在于 platform adapter，portable voice tests 不包含 Arduino 或 ESP-IDF header

### Requirement: 平台输入规范化为 portable 格式
adapter SHALL 把 codec/I²S 提供的通道布局、sample width 和 DMA chunk 规范化为连续 48 kHz S16 mono 480-sample blocks，并 SHALL 明确定义 channel selection/downmix、clipping 和 partial DMA 处理。

#### Scenario: DMA 边界不对齐
- **WHEN** 多次 I²S read 才组成一个完整 portable block
- **THEN** adapter 保持 sample 顺序，仅在积满 480 samples 时发布，剩余 samples 用于下一 block

#### Scenario: 输入超出 S16
- **WHEN** codec sample width 大于 16-bit 且数值超出 S16 范围
- **THEN** adapter 按规定缩放并饱和，不发生整数 wrap

### Requirement: Capture task 有界且失败降级
固件 capture task/DMA/buffer SHALL 使用固定容量和记录的 stack/priority/core affinity 策略；初始化、read、timeout 或 codec error SHALL 停止发布有效 PCM、增加诊断并允许主 Runtime 继续运行和重试。

#### Scenario: I²S read timeout
- **WHEN** capture task 在规定时间内未取得输入
- **THEN** 不发布未初始化或重复 block，timeout counter 增加，达到阈值后状态进入 error 或执行有界重试

### Requirement: Headless 输入可确定性重放
项目 SHALL 提供无需设备的 headless microphone adapter，可按注入时间提供常量、静音、波形、语音 burst、错误和 overrun 序列。

#### Scenario: 同一 fixture 重放
- **WHEN** 相同 fixture、时间步与 consumer 行为运行两次
- **THEN** PCM hash、level/activity、状态转换和诊断计数完全一致

### Requirement: 真机证据与自动化证据分离
自动化 SHALL 验证格式转换、buffer、状态机、headless replay 和 firmware 编译；实体 T-Embed SHALL 另行验证真实通道、增益、噪声、时钟、延迟和长时间稳定性，未执行时 SHALL 标记 hardware validation pending。

#### Scenario: 没有实体设备
- **WHEN** 没有可识别的 T-Embed serial/USB 设备
- **THEN** 测试报告不声称真实采音或音质通过，并保留可重复的真机步骤与预期指标
