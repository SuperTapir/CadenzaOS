# Portable Voice Input 实现与自动化证据

日期：2026-07-18

本报告只覆盖 portable/system/headless 与 current firmware build 证据。没有实体
T-Embed，因此不代表 ES7210 真实采音、macOS 枚举、录音质量、USB 时钟稳定或
Wi-Fi/BLE 射频共存通过；这些项目继续标记为 hardware validation pending。

## 已实现契约

- 固定 PCM：48,000 Hz、signed 16-bit、mono、10 ms / 480 samples；
- 单一 `VoiceCaptureCoordinator` 聚合 Analyzer/USB consumer intent，并区分
  unavailable、stopped、starting、running、error；
- Analyzer 与 USB 各自使用容量 4 的固定 SPSC queue，producer 不等待，满时对
  该 consumer 执行 drop-newest 并累计 overrun/high-water；
- Analyzer 输出 RMS、peak、clipping 和带 attack/release 的基础 VAD；App 只能从
  `SystemSnapshot.voice` 读取结果并提交 analyzer start/stop command，不能取得 PCM；
- 任一 consumer active 时 frame coordinator 绘制持续 `MIC` 隐私标记；USB active
  时 SoundCue 默认抑制并累计 `suppressedSoundCues`；
- USB packetizer 把一个 480-sample block 切为十个 1 ms / 48-sample packet；
  underflow 补同长度 silence，inactive/disconnect/reactivation 清除 stale/partial data；
- `UsbVoiceSessionBridge` 用 callback-safe atomics 接收 USB mount 与 microphone
  alt-setting，数据 owner 在下一次 input callback 同步状态；close 后立即撤销 capture
  intent，generation 变化保证 close/open 夹在两次数据 callback 之间时也会 flush；
- headless microphone 可注入 silence、constant、sine、speech burst、error 和
  consumer overrun，使用相同 system service/queue 路径。
- `VoiceDmaNormalizer` 接受任意长度的 32-bit DMA slot chunk，跨调用保留 partial
  stereo frame/block，支持声道选择或平均、MSB/LSB alignment、S16 saturation；
  连续 3 次 timeout/read error 后清空 partial data 并进入 capture error。

## 固定容量与资源结果

在 AppleClang 21、64-bit host 上：

| 对象 | `sizeof` |
|---|---:|
| `VoiceBlock` | 968 B |
| `VoiceCaptureCoordinator`（两个 4-block queue） | 7,840 B |
| `SystemServiceHost` | 9,728 B |

10,000-block burst 测试保持 USB sequence 逐块连续且 USB high-water 为 1；故意每
11 block 才消费一次的 Analyzer 达到 high-water 4，只增加自己的 overrun，清空后
从新 sequence 恢复。该用例包含 20,064 个断言，并在 normal、strict-warning 与
ASan/UBSan 构建下通过。

current PlatformIO/Arduino 2.0.17 firmware 在链接 portable voice core 后：

- RAM：92,824 / 327,680 B（28.3%）；
- flash：344,521 / 3,145,728 B（11.0%）；
- 相对迁移前一轮 84,696 B / 342,605 B，增加 8,064 B RAM、1,892 B flash。

这只证明当前旧 firmware 能容纳 portable 状态与队列；它尚未包含 ES7210 capture
task/DMA，也不是 ESP-IDF 5.5 UAC candidate 的最终资源数。

## 已运行门禁

```text
tools/check.sh host                       49/49 pass
strict-warning disposable build          48/48 pass (desktop target disabled)
ASan/UBSan disposable build               48/48 pass (desktop target disabled)
tools/check.sh desktop                    launch pass (SDL 3.4.12; CoreAudio unavailable in runner)
cadenza_voice_input_core_tests            5 cases / 20,064 assertions pass
cadenza_usb_voice_packetizer_tests        3 cases / 47 assertions pass
cadenza_headless_microphone_tests          3 cases / 39 assertions pass
tools/check.sh firmware                    success, size as above
openspec validate ... --strict             valid
git diff --check                           pass
```

ESP-IDF 5.5 UAC spike 直接链接 portable coordinator/packetizer/session bridge 后
成功：binary 242,256 B（`0x3b250`）；共享 descriptor contract 的 host lint 为 2
cases / 31 assertions，session bridge 为 3 cases / 28 assertions。上游 component
未公开 mic lifecycle callback，因此 candidate 在 CMake 中只对锁定的
`usb_device_uac` 1.3.1 两个 callback 名做编译期重命名，Cadenza 公共 callback
先调用原实现，再发布 alt-setting 状态。ELF 符号审计确认原实现与公共入口均存在；
未修改 managed component 源码，也未用超时猜测断连。该证据仍不能替代真机
reconnect/alt-setting 验收。

完整 hardware candidate 进一步把 portable Runtime、全部 bundled Apps、u8g2、
ESP-IDF `esp_lcd` ST7789 presenter、GPIO encoder、官方 `esp_codec_dev` 1.5.10
（component hash
`896c67360ae1f8dedaf8a53dfe48c015317fa455f061a5165307892296990028`）接入后，
同一 build 同时链接 ES7210/I²C0/I²S1 input、MAX98357A/I²S0
output、DMA normalizer、portable sound/voice、session bridge 与 UAC。构建成功，
binary 388,032 B（`0x5ebc0`），size tool total image 387,911 B，DIRAM 146,103 B
（42.75%，`.bss` 85,488 B），Flash Code 235,552 B、Flash Data 75,328 B；
1 MiB app partition 尚余 63%。
speaker 独占 I²S0/core 0/4,096 B stack/4×128-frame DMA；microphone 独占
I²S1/core 1/6,144 B stack/4×240-frame DMA，两者只使用对象内 local buffer。
UI/main task 预留 8,192 B stack，ST7789 用 8-row / 5,120 B 固定 transfer buffer，
不分配整幅 RGB565 framebuffer。ELF 审计确认 Launcher、FrameCoordinator、display
presenter、UAC lifecycle 和上游委托实现同时存在。相对 current PlatformIO build，
total image 增加 43,390 B；静态 DIRAM 增加 53,279 B，但两套 SDK 的 size 分类并非
完全同口径。host 并发模型以 496 个断言验证输出队列压力不会破坏输入 timeline。
以上仍只证明 SDK/链接/静态预算，不证明真实显示方向、输入电气、同时时钟与声学
回录。

## 仍待完成

- 真机 Mac 枚举、重连/睡眠、至少 30 分钟录音、时钟漂移、声学泄漏和 RF/功耗。
