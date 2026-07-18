## ADDED Requirements

### Requirement: 设备暴露标准 UAC2 麦克风
固件 SHALL 通过标准 USB Audio Class 2 暴露一个固定 48,000 Hz、S16、mono microphone input，并 SHALL 依赖 macOS 内置 class driver，不要求自定义 Mac driver 或把普通 BLE/Wi-Fi stream 声称为系统麦克风。

#### Scenario: Descriptor 被静态验证
- **WHEN** host descriptor test 解析 USB configuration
- **THEN** input terminal、format、sample rate、channel、interface/alternate setting、endpoint direction 和 packet size 与固定 microphone 契约一致

### Requirement: UAC2 与 CDC 组成受限 composite device
USB configuration SHALL 同时保留 CDC 控制/日志能力和 UAC2 microphone，并 SHALL 使用唯一 interface/endpoint 编号且不超过 ESP32-S3 USB device endpoint 上限。

#### Scenario: Composite descriptor 审计
- **WHEN** descriptor lint 计算全部 interface 和 endpoint
- **THEN** UAC2 与 CDC 不冲突、总数在硬件限制内，MIC-only audio path 可独立构建

### Requirement: USB packet cadence 和 underflow 确定
USB adapter SHALL 在 active input streaming 时每 1 ms 提供 48 个连续 mono samples；PCM 不足时 SHALL 发送等长 silence 并增加 underflow counter，不得重复旧 packet 或阻塞 USB callback。

#### Scenario: 正常 packet 化
- **WHEN** consumer queue 包含连续 10 ms voice block
- **THEN** adapter 依次生成十个 48-sample packets，拼接后与原 block 完全相同

#### Scenario: PCM underflow
- **WHEN** USB 请求 packet 而 consumer 没有足够新 samples
- **THEN** 缺少部分为零、packet 长度和 cadence 不变且 underflow counter 增加

### Requirement: Stream 生命周期不重放陈旧音频
USB alt-setting inactive 或 disconnect 时 SHALL 停止消费并清除不可播放历史；重新 active 时 SHALL flush stale PCM，从重新激活后的新 capture 数据开始，且 shutdown 不访问已销毁 service。

#### Scenario: 主机重新启用输入
- **WHEN** Mac 停止 stream、期间 capture 继续供给其他 consumer、随后重新启用 stream
- **THEN** USB 不播放 inactive 期间的历史，并在新数据到达前输出 silence

### Requirement: 外部语音与内部 App 可并存
USB consumer 与 App analyzer SHALL 使用独立 PCM queue 并可同时 active；USB streaming active 时系统 SHALL 默认抑制扬声器系统 cue、保留视觉反馈和麦克风隐私指示，以降低录入泄漏。

#### Scenario: USB streaming 期间交互
- **WHEN** 用户在 USB microphone active 时导航或确认
- **THEN** 视觉反馈照常发生，cue 不进入扬声器输出，USB/analyzer consumer 继续各自前进

### Requirement: SDK 路径可复现且不私补旧 framework
UAC firmware SHALL 锁定受支持的 ESP-IDF/TinyUSB/Arduino component 版本、配置、依赖锁和构建命令；项目不得依赖修改本机 Arduino-ESP32 2.0.17 预编译 TinyUSB package 来启用 `CFG_TUD_AUDIO`。

#### Scenario: 干净环境构建
- **WHEN** 按记录的依赖和命令在未修改 framework package 的环境构建 UAC target
- **THEN** UAC2+CDC target 成功编译，版本、license、RAM/flash/task/stack 报告可审计

### Requirement: Mac 真机验收是独立发布门槛
实体设备到货后 SHALL 验证 macOS 枚举、Audio MIDI Setup 格式、系统输入选择、录音幅度/通道/噪声、disconnect/reconnect、sleep/wake、至少 30 分钟时钟稳定与 App+USB 并发；未执行时 SHALL 标记 hardware validation pending。

#### Scenario: 仅 descriptor 与 firmware build 通过
- **WHEN** 自动化和候选 SDK 构建成功但没有实体设备连接 Mac
- **THEN** 项目只能声明 USB voice implementation/build ready，不得声明 Mac microphone 验收完成
