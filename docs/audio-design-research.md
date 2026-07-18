# Cadenza 交互声音与 Playdate 听感调研

> 状态：源码参考、公开实机样本和平台边界调研已完成；最终实体 T-Embed 听感仍是实现后的硬件验收项。完整参考矩阵见 `docs/audio-reference-research.md`，采纳边界见 `docs/audio-engine-adoption-decision.md`。

本调研服务于 Cadenza 的系统交互声音，不讨论背景音乐、语音或录音。资料优先采用平台厂商、芯片厂商与硬件厂商的一手文档；“事实”“设计推论”“待验证”分开记录，避免把审美判断冒充硬件结论。

## 结论

Cadenza 不应追求“低采样率等于复古”的表面效果。Playdate 的 1-bit 视觉背后是 44.1 kHz 的现代音频系统；其值得继承的是约束方法：短促、克制、与动作同步、在小扬声器上成立、允许静音、永远不承担唯一信息通道。

Cadenza 的声音意象定为“纸张视觉 × 精密小机构 × 微型数字乐器”。实现从首轮 Triangle/Square 草案收敛为四声部 Semantic Hierarchy：Sine 共振与指数衰减承担低调、圆润的触点，Triangle/Square/Noise 保留给方向、边界和拒绝；确认/开启使用上行动势，返回/关闭使用下行动势，旋钮 tick 需要冷却而不能排队轰炸。

## 已拍板的 Semantic Hierarchy

当前 palette 共 15 项，并按信息重要度控制密度，而不是让所有操作复用同一个滑音：

| 家族 | Cue | 设计职责 |
| --- | --- | --- |
| Input | Navigate、Boundary | 高频操作的低调触点；到边界必须与移动成功不同 |
| Action | Confirm、Back、ToggleOn、ToggleOff、Reject | 用方向、时序和材质区分进入/返回、状态开/关与未执行 |
| Outcome | Complete、Warning、Failure | 只用于确有结果的较重要事件，允许更完整但仍短促的结构 |
| System | Notification、Connect、Disconnect、PowerOn、PowerOff | 用于非直接输入或硬件生命周期，不伪造不存在的触发点 |

Navigate/Select v6 与 ToggleOn/ToggleOff 已在本地 A/B 试听中获用户接受；正式实现只保存重新创作的合成参数，不嵌入三个参考 WAV。Select 使用四个约 1.64–5.84 kHz 的极短阻尼共振器；Toggle 以两个错时事件和相反方向提高 On/Off 区分度。详细测量、相似度和 fallback 边界见 [`audio-palette-calibration.md`](audio-palette-calibration.md)。

## Playdate 公开实机样本分析

样本来自公开的 [Playdate - Unboxing and Setup](https://www.youtube.com/watch?v=OwCXGcFwhJI)，不是 Playdate SDK 或系统资产。分析保留原视频时间轴，以 44.1 kHz 解码公开音轨、2,048-sample Hann window / 256-sample hop 查看频谱，并用对应视频帧确认动作：

| 时间点 | 画面动作 | 可观察声音 | 设计影响 |
| --- | --- | --- | --- |
| `10:29.15` 附近 | System Menu 从 `screenshot` 移到 `saved` | 约 40 ms 的稳定短音，主峰约 860 Hz | Navigate 应非常短、单一、轻，不承担奖励感 |
| `10:34.00–10:34.20` | 关闭 System Menu，回到 Launcher | 约 0.2 s，从约 940 Hz 向约 660 Hz 下行 | Back/Close 用下降轮廓表达收束，长度仍明显短于转场动画 |

录像包含讲解人声、房间声、视频压缩和相机/设备距离未知，因此以上是“数量级与轮廓证据”，不是对 Playdate 私有母带的精确还原。Cadenza 不复制这些频率或声音；参数将重新创作。最重要的观察是：Playdate 的日常系统反馈并不持续铺底，也不使用夸张的 chiptune 乐句。

## 一手资料事实

### Playdate：复古外观不等于受限音频

- [Designing for Playdate](https://help.play.date/developer/designing-for-playdate/) 明确说明 Playdate 可播放 44,100 Hz 的压缩或未压缩音频，并要求在设备扬声器与耳机上验证低频缺失、峰值噪声和样本响度一致性。
- [Inside Playdate / Sound](https://sdk.play.date/inside-playdate) 记录了短样本内存播放、长文件流式播放、ADPCM/MP3/PCM、实时 synth、channel、signal 与 effect。合成器提供正弦、方波、锯齿、三角、噪声、wavetable、ADSR、音高/振幅调制；说明“computer-y”是可选择的音色，不是硬件上限。
- [Playdate Pulp](https://play.date/pulp/docs/) 把声音创作压缩成五种 voice（sine、square、sawtooth、triangle、noise）、音符网格和 ADSR。Sound 只允许单 voice、最多四小节；Song 才允许五 voice、最多 32 小节。这个产品分层证明：交互短音值得拥有比音乐更严格的创作边界。
- Pulp 文档还允许 Launcher 卡片配置 launch sound。声音不是游戏启动后的附加物，而可以与 Launcher 的选中、启动动画共同组成第一次接触。
- [Playdate UX 与无障碍检查清单](https://help.play.date/developer/user-experience-checklist/) 要求音量一致且不过响，在真实 Playdate 和多种环境中试听，并为依赖声音的玩法提供可见提示。
- [Playdate 用户手册](https://help.play.date/hardware/manual/) 把系统音量作为随时可调的全局能力，而不是由单个游戏各自解释。

### 通用交互反馈：重要程度、可关闭、重复变化

- [Apple HIG: Feedback](https://developer.apple.com/design/human-interface-guidelines/feedback) 要求反馈强度与信息重要程度匹配，并建议用视觉、声音、触觉等多种通道共同表达，避免声音成为唯一通道。
- [Apple HIG: Playing audio](https://developer.apple.com/design/human-interface-guidelines/playing-audio) 指出非必要交互音应服从静音与用户音量预期；重复声音可以轻微改变音高与音量以降低机械疲劳。
- 这并不意味着随机化每次 cue 的语义音程。系统反馈的方向性必须稳定；只允许不改变身份的微小变化。

### T-Embed 与 ESP32-S3：原型硬件已经具备数字音频链路

- [LILYGO T-Embed 官方资料](https://wiki.lilygo.cc/products/t-embed-series/t-embed/) 记录原版设备带 MAX98357A I²S 放大器与内置扬声器。
- [LILYGO 官方 sound 示例](https://github.com/Xinyuan-LilyGO/T-Embed/tree/master/examples/sound) 将 BCLK/WCLK/DOUT 分别定义为 GPIO 7/5/6，并使用 I²S 输出。厂商示例把库音量设为最大诊断值 21；这不能直接当作产品默认响度。
- [Arduino-ESP32 I²S 文档](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/i2s.html) 支持标准 I²S、mono/stereo、8/16-bit 与可选引脚映射。Cadenza 只需 44.1 kHz、16-bit mono PCM，不需要媒体解码框架。
- ESP32-S3 的 [LEDC 文档](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/ledc.html) 也能产生 PWM，但 T-Embed 已有 I²S DAC/功放链路；使用 LEDC 蜂鸣器会主动丢失现有硬件能力，且不利于未来 sample voice，因此不选。

## 从 Playdate 推导出的 Cadenza 原则

以下为设计推论，不是 Panic 对其系统私有音色参数的公开说明。

### 1. 语义先于波形

声音应绑定“选择真的改变”“切换真的完成”“动作被拒绝”，而不是绑定 GPIO 边沿。相同点击在不同状态下可以是 Confirm、Toggle 或 Reject；只有 Runtime/App 知道结果。

### 2. 声音与动画共用动作方向

| 语义 | 视觉动作 | 声音轮廓 |
| --- | --- | --- |
| Navigate | 选择卡片弹一下 | 极短、干燥、单音 |
| Confirm | 内容展开/进入 | 上行二音或上滑 |
| Back | 内容收回/返回 | Confirm 的下行镜像 |
| ToggleOn | 开关进入活跃态 | 短上滑 |
| ToggleOff | 开关进入安静态 | 更短下滑 |
| Boundary | 位置不再移动 | 低、钝、少量噪声 |
| Reject | 动作未执行 | 与成功音不同的短噪声脉冲 |

### 3. 舒适来自密度控制

- 一个 `InputFrame` 即使聚合多个 detent，也只触发一个 Navigate。
- Navigate 设置最短重触发间隔；窗口内请求直接丢弃，不排队补播。
- 错误音不能比成功音更响，只需更容易区分。
- 重要完成音必须稀少；日常选择不应借用“奖励”动机。

### 4. 小扬声器是母带目标

电脑音箱能重放的低频可能在 T-Embed 上消失；裸方波高频又可能在小声腔中显得尖锐。首版集中使用中频、短 attack/release 和保守 gain，最终频率与响度必须在实体设备上调整。

## 首轮参数范围（历史设计起点）

| Cue | 建议时长 | 方向 | 原语 | 重复策略 |
| --- | ---: | --- | --- | --- |
| Navigate | 28–40 ms | 轻微上扬或固定 | triangle + 低增益 square | 约 20 ms 冷却 |
| Boundary | 45–70 ms | 下落 | triangle/noise | 不排队 |
| Confirm | 90–140 ms | 上行 | 两个 triangle voice | 高优先级 |
| Back | 80–130 ms | 下行 | 两个 triangle voice | 高优先级 |
| Toggle | 60–100 ms | on 上行/off 下行 | triangle/square | 状态改变才播放 |
| Reject | 60–100 ms | 不规则下落 | noise + triangle | 高于 Navigate 优先级 |

所有范围都必须满足：开头和结尾不在峰值硬切、混音饱和而不环绕、Muted 立即清空 voice。

该表保留为设计演进记录，不再是当前 palette 的实现约束。获批 Select 约 10 ms，Toggle 约 63–65 ms；当前 15 项的精确参数与 golden 以 `sound_cue_library.cpp` 和测试为准，最终扬声器 gain 仍由真机决定。

## 评估矩阵

自动化可以证明：

- PCM 格式、确定性、非零输出、最终回零；
- 包络首尾、音高方向、复音抢占、饱和；
- Navigate 冷却、静音、Settings 文案和 Runtime 路由；
- SDL/firmware 构建与设备失败降级。

自动化不能证明：

- T-Embed 声腔中的实际响度、刺耳程度与低频可闻性；
- 旋钮连续操作一分钟后的疲劳感；
- 动画可见冲击与扬声器出声之间的端到端延迟；
- 最终 Sharp 定制硬件上的音色迁移效果。

## 真机验证脚本

1. 系统音量 Medium，距离设备约正常手持距离；慢转 24 格，确认每次有效选择有反馈且无爆音。
2. 一秒内快速转动多格，确认声音密度被抑制、不在停止后继续补响。
3. 连续执行打开/长按返回 20 次，确认 Confirm/Back 方向可辨且转场起点同步。
4. 在 Settings 循环 High → Muted → Low → Medium；Muted 切换瞬间不得残留尾音，视觉仍完整表达状态。
5. 分别在安静室内、普通环境噪声下试听；检查 Low 可用、Medium 舒适、High 不破音。
6. 录制扬声器输出并检查直流点击、削顶和明显缓冲间断；记录固件 FPS/最慢帧，确认 I²S 不破坏图形交互。

通过以上实体步骤前，项目只能声称“音频逻辑、输出链路与构建已验证”，不能声称“最终听感已验收”。
