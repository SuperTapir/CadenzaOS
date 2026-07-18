# Cadenza 交互声音参考项目调研

本文记录 `add-interaction-sound-system` 的源码级参考、固定 commit、许可证、关键证据与采用边界。上游浅克隆位于被 gitignore 的 `.research/references/`，只用于复查，不是产品构建依赖。

调研日期：2026-07-18。

## 结论摘要

- Playdate 值得追随的是“现代 44.1 kHz 能力下的克制声音语言”，不是人为降采样。公开实机录像中的系统导航音约 40 ms，退出音约 0.2 s；提示依靠短包络和方向轮廓，而不是大音量。
- 成熟项目一致把调用方接口放在语义层：`menu_move`、`menu_select`、`playOK`、`playCancel`、`playTick`。Cadenza 应公开 `Navigate/Confirm/Back/...`，不让 App 选择 GPIO、文件或波形参数。
- 舒适度的共同失败用例是零 attack/release、硬切相位、无节制重触发、缓冲欠载、混音环绕和小扬声器上过强峰值。它们必须先于“音色好不好听”进入自动化门禁。
- SDL、Pokitto、Gamebuino、Mozzi 与官方 T-Embed 代码共同证明，实时消费者必须独立于可能抖动的图形帧。Cadenza 采用固定命令队列，SDL callback / ESP32 I²S task 独占合成器；不再采用“主循环每帧直接生成并写 PCM”的初稿方案。
- 首版自有窄核心采用固定声部、相位连续的 triangle、带 PolyBLEP 的少量 square、确定性 noise、短线性包络和饱和混音。不引入完整音乐/解码框架，也不复制任何 Playdate 或参考游戏资产。

## 参考矩阵

| 项目 | 固定 commit | 许可证 | 关键源码 | 借鉴 | 不采用 |
| --- | --- | --- | --- | --- | --- |
| [SDL](https://github.com/libsdl-org/SDL/tree/df7b9373613835de90034302512f3821f0df3bae) | `df7b9373613835de90034302512f3821f0df3bae` | zlib；audio examples 为 public domain | `examples/audio/01-simple-playback`、`02-simple-playback-callback`、`docs/README-migration.md` | 已有依赖；AudioStream 格式转换、失败降级、callback 按需供给 | 半秒示例缓冲；把平台 stream 暴露给 core |
| [Satellite](https://github.com/Diefonk/satellite/tree/267632ce61757d74ed5f191ecead884c7ba41103) | `267632ce61757d74ed5f191ecead884c7ba41103` | MIT | `Satellite/edit.lua` | Playdate synth、ADSR、channel 与 legato 的真实用法 | 8 轨编辑器和动态 Lua authoring runtime |
| [CS-16](https://github.com/Nanobot567/CS-16/tree/5b27f0489f25acecf560675e456a40c477e0581c) | `5b27f0489f25acecf560675e456a40c477e0581c` | MIT | `src/setup.lua` | 多 waveform/ADSR/effect；音频应用会显式关闭系统 crank sounds，避免双重反馈 | 16 轨合成器、效果器链和音乐 sequencer |
| [PocketBMPlayer](https://github.com/dr-watson-hk/PocketBMPlayer/tree/aaebbe761c2c6bec9e0754111e8cd6827ac6c8c7) | `aaebbe761c2c6bec9e0754111e8cd6827ac6c8c7` | CC0-1.0 | `src/beat_machine.c` | 把 sequence/track/instrument/synth/channel 包在独立接口后 | 动态分配的完整音乐引擎 |
| [Modular Play](https://github.com/orllewin/modularplay/tree/350b584a6e2eec84acc7e22a9f7d2cae4fff11ef) | `350b584a6e2eec84acc7e22a9f7d2cae4fff11ef` | GPL-3.0 | module/channel/effect sources | 每个模块明确拥有 channel，并在删除时 stop/remove 的生命周期纪律 | GPL 代码、模块图与完整 DAW 范围 |
| [ZzFX-Playdate](https://github.com/KilledByAPixel/ZzFX-Playdate/tree/0c21756ae81d0d90beee02c3936c96fa0042af0e) | `0c21756ae81d0d90beee02c3936c96fa0042af0e` | MIT | `src/zzfx.c` | Playdate 实机 click/性能失败证据、固定 voice pool、clamp/NaN 防御、attack 底线 | 21 参数通用 SFX API、运行时大块 PCM/float buffer 分配、round-robin 无优先级抢占 |
| [Pick Pack Pup](https://github.com/NicMagnier/PickPackPup/tree/c60fd8a13ab091d23f6b6d0262c65ecc332f66f0) | `c60fd8a13ab091d23f6b6d0262c65ecc332f66f0` | 仓库未找到许可证，视为不可复用 | `code/sfx.lua` | `menu_move/menu_select/page_turn/safe_click` 语义表；同 sample 最短重触发；轻微播放 rate 变化 | 任何代码、声音资产或参数复制 |
| [Factory Farming](https://github.com/timboe/FactoryFarming/tree/777e2aca41f430275f35ce22ac2991881dd34e01) | `777e2aca41f430275f35ce22ac2991881dd34e01` | MIT | `src/sound.c` | C 语言语义 enum、预载短 sample、音乐/SFX 独立开关 | 无冷却/优先级的直接播放策略；游戏样本资产 |
| [ArduboyTones](https://github.com/MLXXXp/ArduboyTones/tree/972fe8117002da47073b6c1835117d654a03e16f) | `972fe8117002da47073b6c1835117d654a03e16f` | MIT | `src/ArduboyTones.cpp/.h` | 极端资源约束下的固定 tone sequence 与静音回调 | 只有硬方波、无包络/复音；不能达到目标听感 |
| [PokittoLib](https://github.com/pokitto/PokittoLib/tree/4406a4f2ff39acc42beea39628f4722e4f9d2b9e) | `4406a4f2ff39acc42beea39628f4722e4f9d2b9e` | 仓库混合 BSD/MIT/Apache/GPL 等，需逐文件判断 | `Pokitto/POKITTO_LIBS/LibAudio` | 双 512-byte buffer、固定 channel、饱和 8-bit mix；`SFX8Source` 从当前 play head + 10 写入以降低 UI 延迟 | 混合许可证代码、硬件 ISR 和 8-bit unsigned 产品格式 |
| [sfxr](https://github.com/grimfang4/sfxr/tree/48ce8f1ee3951c6313cf40948a65d8f76df4d77e) | `48ce8f1ee3951c6313cf40948a65d8f76df4d77e` | MIT | `sfxr/source/main.cpp` | square/saw/sine/noise、pitch slide、attack/sustain/decay、filter/phaser 的创作参数空间 | 8× supersampling 和桌面编辑器作为设备实时核心 |
| [Mozzi](https://github.com/sensorium/Mozzi/tree/02fde2d126c2749bccc2ab145406b1cdf9e29c89) | `02fde2d126c2749bccc2ab145406b1cdf9e29c89` | LGPL-2.1-or-later | `ADSR.h`、`Oscil.h`、`internal/MozziGuts_impl_ESP32.hpp` | control/audio rate 分离、定点 wavetable phase、小而持续的输出 buffer | LGPL 依赖和跨板抽象；对几个系统 cue 过宽 |
| [Arduino Audio Tools](https://github.com/pschatzmann/arduino-audio-tools/tree/83c41d8ec0a26e5d84315882f1f91ae70f9a9455) | `83c41d8ec0a26e5d84315882f1f91ae70f9a9455` | GPL-3.0 | `src/AudioTools`、I²S/generator examples | 仅用于确认生态中 stream/info/ring-buffer 分层 | GPL、模板/格式/codec 表面积远超需求 |
| [DaisySP](https://github.com/electro-smith/DaisySP/tree/599511b740f8f3a9b8db72a0642aa45b8a23c3a3) | `599511b740f8f3a9b8db72a0642aa45b8a23c3a3` | MIT；含多个逐项声明的上游 | `Source/Synthesis/oscillator.*` | 标准 PolyBLEP square/saw 和 leaky-integrated triangle 的实现证据 | 整套 DSP；首版不需要 filter/reverb/physical modeling |
| [Gamebuino META](https://github.com/Gamebuino/Gamebuino-META/tree/5164dedd3822a427982ebe661d23de08fb79329b) | `5164dedd3822a427982ebe661d23de08fb79329b` | LGPL-3.0-or-later | `src/utility/Sound/Sound.cpp`、`Sound_FX.*` | 平台级 `playOK/playCancel/playTick`；OK 上行、Cancel 下行；square/noise + volume/frequency sweep；循环 buffer | LGPL 代码、`new/delete` handler、SAMD DAC/ISR |
| [LILYGO T-Embed](https://github.com/Xinyuan-LilyGO/T-Embed/tree/17867865b2d157a3a40ebe88e6ad3f0ba113aafd) | `17867865b2d157a3a40ebe88e6ad3f0ba113aafd` | MIT | `examples/sound/sound.ino`、`pin_config.h`、bundled `ESP32-audioI2S` | BCLK/WCLK/DOUT = 7/5/6；外部 I²S 使用 16-bit right-left frames | MP3/SPIFFS/大型 decoder；示例最大音量 21 |

## 源码证据与设计影响

### Playdate 开源项目：能力很宽，系统提示却应很窄

Satellite、CS-16、PocketBMPlayer 和 Modular Play 证明 Playdate 能承担多轨、ADSR、效果器、sequencer 和模块合成。这些项目反而排除了“因为 1-bit 视觉所以音频只能做蜂鸣器”的假设。Cadenza 只需要其中很窄的子集，并且不能把音乐引擎变成平台核心。

Pick Pack Pup 与 Factory Farming 的调用点不谈 waveform，而谈 `menu_move`、`menu_select`、`page_turn`、`no`。Gamebuino 更直接提供 `playOK/playCancel/playTick`，其 fallback pattern 让 OK 使用低到高两音、Cancel 使用高到低两音。不同平台独立得到同一结论：稳定的动作语义和方向比某一组精确频率更可迁移。

Pick Pack Pup 的 `playAtLeast` 只抑制同一个仍在播放且未超过最短 offset 的 sample，不把请求排队；`playAtRate` 允许轻微速度变化。这支持 Cadenza 的 Navigate 冷却和无积压策略，但该仓库无许可证，绝不复制代码或资产。

### 爆音不是审美问题，是必须测试的工程失败

ZzFX-Playdate 记录了两个 Playdate 特有的实机失败：SamplePlayer 启动后约 3–11 ms 可能注入 click，128 个前导零样本不够，512 个（44.1 kHz 下约 11.6 ms）才覆盖；attack 为零时至少补 9 个样本的 ramp。它还把逐样本路径从 double 改为 float，因为 Cortex-M7 没有 double FPU，软件仿真会慢到 watchdog/reset。

这不意味着 Cadenza 机械加入 512 个零样本：自有 I²S/SDL 后端没有相同 SamplePlayer bug。真正采用的是失败用例：首样本、末样本、相位抢占、设备恢复和欠载边界必须无不连续；ESP32 逐样本路径不得使用 double、heap 或昂贵通用函数。

### 8-bit/16-bit 风格与现代输出并不冲突

ArduboyTones 展示最小的定时器硬方波；Pokitto 以 8-bit unsigned PCM、固定 channel 和饱和混音获得更丰富声音；Gamebuino 在 44.1 kHz 逻辑时间上用 square/noise 的音量与周期 sweep 生成系统 OK/Cancel/Tick；Playdate 则完整输出 44.1 kHz。所谓“8/16-bit 舒适感”来自有限 voice、简单波形、短旋律和明确包络，不来自把最终 PCM 降成低质量格式。

Naive square 的不连续会产生无限谐波并折叠到可听频带。DaisySP 的 PolyBLEP 只在相位边沿附近校正，保留数字边缘而显著减轻高频 alias；它比整套 oversampling/filter 更符合窄核心。Triangle 仍作为日常提示主体，PolyBLEP square 只低增益叠加。

### 缓冲、线程与延迟

SDL3 官方把 AudioStream 作为主要接口：push 示例按水位补数据，并明确游戏应比半秒示例“显著更少地预生成”；callback 示例通常在后台线程按 `additional_amount` 生成，给不够时设备补静音。Pokitto 使用 2×512-byte buffer，但 `SFX8Source` 为低延迟直接从当前 play head + 10 写入，说明等待下一整块 buffer 对 UI cue 太慢。Mozzi 在 ESP 路径也把 I²S buffer 当输出 buffer，持续供给而不是依赖图形帧。

Cadenza 因此采用：主线程把小型语义命令放入固定 SPSC 队列；平台音频消费者独占 voice/phase/envelope 状态并持续渲染。SDL callback 与 ESP32 audio task 使用同一 `consumeCommands + render`，headless 则在测试线程同步调用。队列为关键命令预留容量，普通 Navigate 达到水位即丢弃，不允许历史点击在操作停止后补播，也不让生产者覆盖消费者正在读取的 slot。

### T-Embed 输出边界

LILYGO 固定 commit 的 `examples/sound/pin_config.h` 给出 BCLK 7、WCLK 5、DOUT 6；`sound.ino` 使用外部 I²S 并把示例库音量设为 21/21。其 bundled ESP32-audioI2S 实际配置 `I2S_BITS_PER_SAMPLE_16BIT` 与 `I2S_CHANNEL_FMT_RIGHT_LEFT`。因此 core 保持 16-bit mono，firmware adapter 在固定小 buffer 中复制为左右声道 I²S frame；不能把“mono core”误写成总线上只发一个 slot。

项目当前 `espressif32@6.12.0` 对应本地 Arduino-ESP32 package `3.20017.0`（Arduino core 2.0.17），提供 legacy `driver/i2s.h` 的 `i2s_driver_install/i2s_write`。首版 adapter 应按当前固定 toolchain 编译，而不是照搬 Arduino core 3.x 的新 `ESP_I2S.h` 文档。

## 许可证与资产规则

1. 本轮不新增音频第三方运行时依赖；SDL 已是桌面宿主依赖。
2. 无许可证的 Pick Pack Pup 只提供不可版权化的高层行为证据，不复制源码、参数表或声音。
3. GPL/LGPL/mixed-license 项目只用于比较架构与失败模式；若未来要链接，必须重新做产品许可证决策。
4. 不下载、提取、vendoring 或分发 Playdate OS/SDK 的声音资产。Playdate SDK 许可证限制 SDK 用于 Playdate 程序开发，不能拿来构建另一平台 SDK；本调研只使用公开文档与公开实机录像。
5. 所有 Cadenza cue 参数由本项目重新创作，并以真机测试迭代；参考项目的游戏音效文件不进入仓库。
