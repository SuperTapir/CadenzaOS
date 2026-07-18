## 1. 调研与定案

- [x] 1.1 固定 Playdate/embedded/desktop 参考项目 commit、许可证和关键源码，记录采用与拒绝边界
- [x] 1.2 分析公开 Playdate 实机系统提示并区分可观察事实、设计推论和母带限制
- [x] 1.3 完成音频引擎/输出采纳决策，修正 voice、buffer、thread 与 I²S wire-format 假设

## 2. 失败用例与 Portable Core

- [x] 2.1 先添加 waveform/PolyBLEP、包络、参数钳制、确定性 noise、饱和和结束静音失败测试
- [x] 2.2 先添加三声部优先级/年龄/平滑抢占与固定 SPSC 命令队列保留容量/溢出失败测试
- [x] 2.3 实现 `AudioEngine`、`ToneSpec`、三种波形、44.1 kHz PCM 与命令队列，并纳入 shared-source audit

## 3. 语义声音与 Runtime

- [x] 3.1 先添加 SoundCue、Navigate 冷却、Muted 立即停止、音量和 Runtime 路由失败测试
- [x] 3.2 实现 Cadenza cue 表与 `InteractionSoundService`，只向 App 暴露语义和会话音量
- [x] 3.3 接入 Launcher/Clock/Motion/Settings/Gallery 的实际状态变化、打开与长按返回
- [x] 3.4 扩展 Settings 为 Motion、Sound、About 三行并保持 T-Embed/Sharp bounded-text 与 snapshot 安全

## 4. 平台输出

- [x] 4.1 实现 SDL3 AudioStream callback、simulation 暂停语义、设备失败降级和生命周期测试
- [x] 4.2 实现 T-Embed legacy I²S audio task、GPIO 7/5/6、mono-to-stereo frame、错误诊断和 compile probe
- [x] 4.3 增加 headless PCM 观测、golden hash/峰值/方向和平台集成测试

## 5. 文档与完成审计

- [x] 5.1 更新架构、开发、内存/性能、验证与音效资产标准，记录替换 pipeline、控制方式与实体设备脚本
- [x] 5.2 运行音频定向、完整 host/desktop、SDL dummy、shared-source/格式和 firmware 门禁
- [x] 5.3 对照三份 delta spec 逐项审计，记录自动证据与未冒充通过的实体扬声器边界
