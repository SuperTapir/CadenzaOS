# Voice, USB, Wi-Fi and Bluetooth reference research

日期：2026-07-18
适用 change：`refactor-system-services-foundation`

## 产品路径结论

- Mac 系统级语音输入首选 USB Audio Class 2 microphone；UAC 使用 macOS 内置 class driver。
- ESP32-S3 只有 Bluetooth LE，没有 Classic Bluetooth，不能实现传统 HFP/A2DP 蓝牙耳麦。
- ESP32-S3 不是可靠的 Bluetooth LE Audio 产品基线；普通 BLE GATT 音频不会自动成为 macOS 系统麦克风。
- Wi-Fi 音频需要 Mac 端应用或虚拟音频设备，因此不作为首条零驱动系统输入路径。
- BLE/Wi-Fi 首阶段用于 provisioning、控制与状态，并通过与其他系统服务相同的 command/snapshot 边界服务小应用。

## 固定参考与源码证据

| 项目 | Commit / 许可证 | 直接阅读文件 | 结论 |
|---|---|---|---|
| TinyUSB | `aa410008e8e74b0727f8c30a1ec109ff2c37efc6` / MIT | `examples/device/cdc_uac2/src/tusb_config.h`、`usb_descriptors.c/.h`、`uac2_app.c`、`cdc_app.c` | 官方 composite example 证明 UAC2 + CDC 可共享一根线；采用其 descriptor/interface/alternate-setting 结构，产品固定为 48 kHz S16 mono microphone |
| ESP IoT Solution `usb_device_uac` | `9971a4692b5c50fbe055db786a9bd6f541372b6e` / Apache-2.0 | `components/usb/usb_device_uac/usb_device_uac.c`、`include/usb_device_uac.h`、`tusb/usb_descriptors.c`、`tusb_uac/uac_config.h`、`CHANGELOG.md`、`idf_component.yml` | v1.3.1 支持 S3、可配 rate/channel/resolution、MIC callback、双 buffer 和约 1 ms USB task；component 要求 IDF >=5.0，example >=5.1，TinyUSB dependency `^0.19.0~3` |
| LilyGO T-Embed | `17867865b2d157a3a40ebe88e6ad3f0ba113aafd` / MIT（复制前仍需文件级确认） | `examples/mic/es7210.cpp/.h`、mic example、factory codec source | 双 MEMS + ES7210；SDA 18、SCL 8、BCLK 47、LRCK 21、DIN 14、MCLK 48、I²S1。板载 MAX98357A output 使用 I²S0 pins 7/5/6，因此输入输出可分 owner，但必须真机验证 |
| Espressif `esp_codec_dev` | registry `1.5.10` / hash `896c67360ae1f8dedaf8a53dfe48c015317fa455f061a5165307892296990028` / Apache-2.0 | `device/es7210/es7210.c`、`device/include/es7210_adc.h`、`platform/audio_codec_ctrl_i2c.c`、`platform/audio_codec_data_i2s.c`、`test_apps/.../test_board.c` | 采用为 candidate codec driver，使用官方 8-bit address `0x80` 和 IDF 5.x bus handle，避免复制旧 Arduino 示例寄存器表；仅编入 ES7210，板级 pins/channel/gain 由 Cadenza adapter 注入并真机校准 |
| Arduino-ESP32 2.0.17 | `5e19e086c43d0fa5e5a596497ff8f11a0a43f6c2` / LGPL-2.1 | `tools/sdk/esp32s3/include/tinyusb/tusb_config.h` 与 PlatformIO package metadata | 当前 precompiled TinyUSB config 没有 `CFG_TUD_AUDIO`；不能靠添加一个应用源文件可靠启用 UAC |
| ESP-IDF 4.4.7 | `38eeba213aa695aabfd6d89aa9f5078dbe5a94c3` / Apache-2.0 | 当前 framework version/header 与 USB/I²S component metadata | 保留为旧 firmware 回归，不作为新 UAC 基线 |
| ESP-IDF 5.5 + Arduino component | `8c750b088c7cd857d079c0eeb495da199b359461` / Apache-2.0；Arduino 3.3.10 / LGPL-2.1-only | 官方 archive checkout、Arduino-as-component 文档、隔离 UAC2+CDC spike | 官方 `idf.py` 下 Arduino 3.3.10 + UAC 1.3.1 + TinyUSB 0.19.0~3 已构建通过；采用为迁移候选，PlatformIO IDF builder 的 duplicate action 作为已知工具链 blocker |

另外核对的 primary constraints：ESP32-S3 USB device 最多 6 个 endpoints（5 IN/OUT + 1 IN）；Apple USB Audio 技术说明建议使用内置 class driver，并明确不支持 Audio Device Class 4。最终 descriptor test 必须计算 interface/endpoint，不只比较 byte array hash。

## 已知失败案例

ESP IoT Solution changelog 显示：

- 1.2.1 修复 microphone-only 路径与构建；
- 1.2.3 修复多声道 endpoint size alignment 导致 Windows Code 10 枚举失败。

上游 issue 还报告 ESP32-S3 在 IDF 5.3/5.4 的随机杂音。该报告不能直接证明当前候选也失败，但足以把以下项目列为强制门禁：packet cadence、sample clock、underflow/overrun、buffer ownership、MIC-only build、descriptor lint 和真实 Mac 长录音。

## Voice data contract

portable capture 固定 48,000 Hz、S16、mono、10 ms/480 samples。选择 48 kHz 避免 UAC packet 的 44.1 kHz 分数 cadence；USB consumer 再切为每 1 ms 48 samples。一个 coordinator 独占 codec/I²S，analyzer 与 USB 各有固定 SPSC queue；某 consumer 满时 drop-newest，只增加自己的 overrun。

USB underflow 发送等长 silence，不能重播旧 packet。alt-setting inactive/disconnect 不累计历史，reactivation flush stale samples。普通 App 只见 availability/running/RMS/peak/VAD/error/overrun snapshot 和 start/stop command；原始 PCM API 推迟到出现真实录音/STT consumer 后另立规范。

## SDK 决策门槛

当前 Arduino 2.0.17 framework package 保持只读。隔离 spike 已在官方 ESP-IDF 5.5 checkout 上复现 Arduino component 3.3.10 + TinyUSB UAC2/CDC；详细依赖锁、descriptor 和 size/task/stack 结果见 `firmware-sdk-adoption-decision.md`。后续产品 candidate 仍必须完成：

1. descriptor endpoint/interface lint；
2. TFT/input/I²S0 output/I²S1 input 共存编译；
3. 完整 candidate RAM/flash/task/stack 报告；
4. license/attribution 更新；
5. 回滚到当前 firmware target 的独立命令。

只有以上构建门禁通过才切换产品 firmware baseline；真实设备枚举和录音仍是单独门禁。

## 真机待验证矩阵

- ES7210：左右通道、单声道 downmix、gain、noise floor、clipping、MCLK 和 timeout recovery；
- USB/macOS：System Information、Audio MIDI Setup、系统输入选择、常用录音 App、reconnect、sleep/wake；
- 30 分钟以上录音：漂移、杂音、underflow/overrun、packet continuity；
- App analyzer + USB 并发、隐私指示、系统 cue 抑制与 acoustic leakage；
- Wi-Fi/BLE 开启后的 RF coexistence、RAM/task/stack、功耗和 voice stability。

设备到货前这些项目保持 `hardware validation pending`，不能由 headless fixture、firmware compile 或 descriptor test 替代。
