# 候选固件 SDK 采用决策

日期：2026-07-18
适用 change：`refactor-system-services-foundation`
结论状态：**采用为迁移候选；硬件能力仍待真机验证**

## 决策

CadenzaOS 的下一代 T-Embed 固件采用以下受支持组合继续开发：

- ESP-IDF `v5.5`，commit `8c750b088c7cd857d079c0eeb495da199b359461`；
- Arduino-ESP32 component `3.3.10`；
- ESP IoT Solution `usb_device_uac` `1.3.1`；
- Espressif TinyUSB component `0.19.0~3`；
- Espressif `esp_codec_dev` `1.5.10`（只启用 ES7210）；
- ESP32-S3 UAC2 microphone-only + CDC composite。

这是渐进迁移决策，不是立即删除现有 PlatformIO Arduino 2.0.17 target，也不是
对语音产品能力的完成声明。现有固件 target 保持可构建回滚路径；portable core、
system services 和 Apps 先完成边界重构，候选 adapter 再接入。实体 T-Embed 到货
前，Mac 枚举、真实采音、长录音时钟和 RF coexistence 都保持 pending。

不采用以下路径：

- 不修改 `/Users/tapir/.platformio` 内的 Arduino 2.0.17 预编译 TinyUSB；该版本未
  启用 audio class，私补会形成不可复现 framework fork。
- 不以 PlatformIO 的 ESP-IDF builder 作为候选基线；本 spike 遇到 managed
  component 的同一 object 重复 action。官方 `idf.py` 可在相同源码和锁文件下
  构建成功。
- 不把 BLE GATT 或 Wi-Fi 音频描述成 macOS 系统麦克风；首条系统级输入路径是
  USB Audio Class，BLE/Wi-Fi 先承担配网、控制和状态。

## 输入与完整性证据

用户提供的官方 archive：

- 文件：`/Users/tapir/Downloads/esp-idf-v5.5.zip`
- SHA-256：`54880b2565de01dc1d1210330e4514e8b2f816ea62219068ad943e6a0e8b588f`
- `unzip -tq`：通过；archive 包含 `.git` 与递归 submodules
- 解压目录：`/Users/tapir/Development/esp-idf-v5.5`
- tag/commit：`v5.5` / `8c750b088c7cd857d079c0eeb495da199b359461`

可审计 spike 位于 `.research/spikes/uac_idf`。`src/idf_component.yml` 固定直接
依赖，`dependencies.lock` 保存 32 个解析后的直接/传递组件及 component hash，
`sdkconfig.defaults` 固定 MIC-only、48 kHz、16-bit、1 ms 和 Arduino 非 autostart
模式。

## 构建实验

从 spike 目录使用官方 ESP-IDF checkout 执行：

```sh
idf.py -B build-idf-arduino-composite build
idf.py -B build-idf-arduino-composite size
```

结果：构建和链接通过，无应用源码 warning。镜像 `0x3a8c0` bytes；size tool
报告 total image 239,699 B。静态内存为 DIRAM 53,523 B，其中 `.data` 10,896 B、
`.bss` 4,104 B；Flash Code 130,124 B，Flash Data 43,740 B。1 MiB app partition
剩余约 77%。

共享 descriptor contract 加入 host lint 后，又把仓库内 portable
`VoiceCaptureCoordinator`、双 consumer 固定队列和 `UsbVoicePacketizer` 直接链接
到 UAC input callback，并以 10 ms synthetic capture task 驱动。该路径仍构建
成功。加入 session lifecycle 后最新镜像 `0x3b250`（binary 242,256 B）。
上游 1.3.1 component 在内部 `tud_audio_set_itf_cb` /
`tud_audio_set_itf_close_EP_cb` 维护 `mic_active`，但 `uac_device_config_t` 没有
向 user callback 暴露 open/close 事件。GNU ld `--wrap` 实验虽能链接，却因调用与
定义同属上游 object 而未截获，已拒绝。最终方案在 candidate CMake 中对锁定的
1.3.1 callback 做编译期符号重命名，由 Cadenza 公共 callback 先委托未修改的上游
实现，再把 alt-setting 写入 callback-safe session bridge。ELF 同时存在上游重命名
符号和公共入口；没有修改本机或 managed component 源码，也没有超时猜测。

第二个 build 配置同时设置 `CONFIG_CADENZA_T_EMBED_RUNTIME=y`、
`CONFIG_CADENZA_T_EMBED_MICROPHONE=y` 和
`CONFIG_CADENZA_T_EMBED_SPEAKER=y`，完整链接全部 bundled Apps、u8g2、原生
`esp_lcd` ST7789、GPIO encoder、`esp_codec_dev` ES7210、I²C0、I²S1 input、
I²S0 output、portable system/voice/sound 和同一 UAC consumer。
默认板级 pins 为 SDA 18、SCL 8、BCLK 47、LRCK 21、DIN 14、MCLK 48；输入为
48 kHz、32-bit stereo，在 portable 边界平均并饱和为 S16 mono。完整 candidate
结果 binary 388,032 B（`0x5ebc0`），size tool total image 387,911 B、DIRAM
146,103 B（42.75%，`.bss` 85,488 B）、Flash Code 235,552 B、Flash Data
75,328 B；1 MiB app partition 剩余 63%。
I²S0 output task 使用 core 0 / 4,096 B stack / 4 × 128-frame DMA；I²S1
capture task 使用 core 1 / 6,144 B stack / 4 × 240 stereo-frame DMA。两个
adapter 的 channel、task、DMA 和 chunk buffer 分开所有。ST7789 使用 40 MHz SPI2
与固定 8-row / 5,120 B transfer buffer；encoder 使用 GPIO 0/1/2 polling。30 dB
gain、MIC1|MIC2、显示 mirror/rotation 和平均策略只是待真机确认的 board config。

对照不含 Arduino component、但同为 UAC2+CDC 的成功构建：total image
211,055 B、DIRAM 51,295 B。Arduino 兼容层在这个最小链接闭包中增加 28,644 B
image 和 2,228 B DIRAM。完整 candidate 相对 current PlatformIO firmware 的
344,521 B flash / 92,824 B RAM，total image 增加 43,390 B，静态 DIRAM 增加
53,279 B；由于 ESP-IDF size tool 与 PlatformIO summary 分类不同，这只用于量级
判断。当前仍有 195,657 B 静态 DIRAM 余量，没有出现迫使整体重写的资源证据，
但 task stack、LCD DMA 与运行时 heap 必须真机测量。

### Wi-Fi、BLE 与安全配网增量

同一 ESP-IDF 5.5 graph 已继续组合内置 NVS、`esp_netif`、Wi-Fi station、NimBLE、
software coexistence、Wi-Fi provisioning manager 与 protocomm Security 2。
Security 0/1 明确关闭；量产 Security 2 verifier/salt 不进入仓库，默认 weak platform
hook 在没有设备材料时安全失败。

完整连接 candidate binary 为 1,176,672 B，total image 1,176,519 B、static
D/IRAM 226,823 B（余 114,937 B），相对当前无连接 full candidate 分别增加
785,920 B、785,912 B 与 79,056 B。使用 1.5 MiB app partition 后仍余约 23%。
该体积增长主要来自 Wi-Fi/NimBLE/protocomm/Security 2 链接闭包，尚未形成必须重写
portable 架构的证据，但会进入真机 heap/stack/功耗门禁。

官方 v5.5 `protocomm_nimble.c` 自行初始化并拥有 NimBLE host 与 `ble_hs_cfg`；
因此当前采用“普通 App BLE”和“BLE provisioning”排他角色，由 platform adapter
仲裁。未采用双重初始化、修改官方 transport 或宣称二者可同时运行。未来若产品要求
配网期间持续提供自定义 GATT，需要单独研究共享 host transport 或其他配网承载。

UAC component 创建两个任务：

| 任务 | priority | affinity | stack |
|---|---:|---|---:|
| TinyUSB device | 5 | no affinity | 4096 B |
| USB microphone | 5 | core 1 | 4096 B |
| synthetic capture（仅 spike） | 5 | core 1 | 4096 B |
| ES7210 capture（hardware build） | 5 | core 1 | 6144 B |
| I²S0 speaker（hardware build） | 2 | core 0 | 4096 B |
| UI/main（hardware build） | 1 | core 0 | 8192 B |

UAC 的两个 4096 B stack 在 component 源码中固定，其余候选任务也由 FreeRTOS
从 heap 取得；以上显式任务 stack 合计 26,624 B，不能只看静态 `.bss`。产品
adapter 接入后必须记录启动前后 free heap 与各 task high-water；当前只证明预算
形状和构建关系。

## Descriptor 预算

固定 interface：

| interface | 用途 |
|---:|---|
| 0 | UAC2 Audio Control |
| 1 | UAC2 microphone Audio Streaming |
| 2 | CDC Control |
| 3 | CDC Data |

固定 endpoint address：

| address | 用途 |
|---:|---|
| `0x81` | microphone isochronous IN |
| `0x82` | CDC notification IN |
| `0x03` | CDC data OUT |
| `0x83` | CDC data IN |

除 EP0 外使用 3 个 endpoint number、3 个 IN direction 和 1 个 OUT direction，
在 ESP32-S3 device endpoint 上限内。音频为 48,000 Hz、S16、mono、1 ms interval；
portable 480-sample block 后续切成每包 48 samples。descriptor host lint、
alternate-setting lifecycle、close/open generation flush 和 underflow silence 已有
独立 host 门禁；真实 host 行为仍在真机门禁中。

## 许可证与维护边界

- ESP-IDF、ESP IoT Solution UAC：Apache-2.0；
- Arduino-ESP32：LGPL-2.1-only；
- TinyUSB：MIT；
- 每个 registry component 的版本和 hash 以 `dependencies.lock` 为准。

最终 firmware 发布前更新 `THIRD_PARTY_NOTICES.md`，并审计 Arduino 3.x 带入但
未链接的传递组件。依赖数量较多是维护成本信号；它不等于全部代码进入镜像，
但 CI cache、供应链扫描和许可证审计必须覆盖 lock 中所有下载内容。

## 后续门禁

1. 保持现有 firmware build 绿色，并持续构建独立 complete candidate IDF target。
2. 真机到货后确认 ST7789 方向/偏移、encoder 电气和 I²S0/I²S1 并发。
3. 执行 Mac 枚举、输入选择、重连/睡眠唤醒和至少 30 分钟录音。
4. Wi-Fi/BLE 开启后测 heap、task stack、RF coexistence、功耗和 voice stability。

在第 2–4 项通过之前，对外状态只能写作
`host/build foundation complete / hardware validation pending`。
