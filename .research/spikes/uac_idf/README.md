# CadenzaOS ESP-IDF 5.5 UAC / T-Embed firmware composition

这是 CadenzaOS **主固件组合根**（ESP-IDF 5.5：Runtime Apps + UAC2 麦 + CDC）。
PlatformIO Arduino 2.0.17 仅作无 UAC 回滚（`tools/check.sh firmware-pio`）。

```sh
# 从仓库根目录
tools/check.sh firmware          # 构建（firmware-uac 为同义别名）
tools/firmware_uac.sh flash      # 烧录
tools/firmware_uac.sh size
tools/firmware_uac.sh connectivity
```

SDK 组合：ESP32-S3、ESP-IDF 5.5.0、Arduino-ESP32 3.3.10、`usb_device_uac`
1.3.1、TinyUSB 0.19.0~3、`esp_codec_dev` 1.5.10、UAC2 microphone-only + CDC。
portable Runtime / voice / Apps 仍在仓库 `lib/`；本目录只做板级装配，不私补
本机 PlatformIO Arduino 2.0.17 package。

已定麦克风基线（2026-07-19 真机）：双 MEMS 平均、24 dB、USB 重连强制回收
I²S/DMA 帧对齐；macOS 枚举为 48 kHz mono 系统麦。

## 前置条件

- 官方 ESP-IDF v5.5 checkout；验证 commit 为
  `8c750b088c7cd857d079c0eeb495da199b359461`。
- `install.sh esp32s3` 与 `export.sh`，或由 `tools/firmware_uac.sh` 通过
  `CADENZA_IDF_PATH` / `../esp-idf-v5.5` 自动 `source export.sh`。

## 构建（底层命令；优先用 tools/ 入口）

```sh
cd .research/spikes/uac_idf
idf.py -B build-idf-arduino-composite build size

idf.py -B build-idf-hardware \
  -DSDKCONFIG="$PWD/sdkconfig.hardware" \
  -DSDKCONFIG_DEFAULTS='sdkconfig.defaults;sdkconfig.hardware.defaults' build
idf.py -B build-idf-hardware size

idf.py -B build-idf-connectivity \
  -DSDKCONFIG="$PWD/sdkconfig.connectivity" \
  -DSDKCONFIG_DEFAULTS='sdkconfig.defaults;sdkconfig.hardware.defaults' build
idf.py -B build-idf-connectivity size
```

它链接 ESP-IDF 内置的 NVS、`esp_netif`、event loop、Wi-Fi station、NimBLE、
software coexistence、`wifi_provisioning` 与 protocomm Security 2。Security 0/1
在该配置中关闭；仓库不带通用 verifier 或 salt，量产方必须覆盖弱符号
`cadenza_load_security2_material`，从受控平台边界提供设备材料。

`sdkconfig.defaults` 只启用 `esp_codec_dev` 的 ES7210 driver，关闭未使用 codec。
默认 board config 固定 I²C0 SDA 18/SCL 8、I²S1 MCLK 48/BCLK 47/LRCK 21/DIN 14，
48 kHz、32-bit stereo DMA 输入在 portable normalizer 中平均为 S16 mono。
已定真机基线：双 MEMS 平均、24 dB in-gain。

当前完整 candidate binary 为 388,032 B（`0x5ebc0`），size tool total image
387,911 B、DIRAM 146,103 B（42.75%）、Flash Code 235,552 B、Flash Data
75,328 B。UI/main、TinyUSB、UAC mic、ES7210 capture 与 speaker 的显式 stack
预算合计 26,624 B，运行时 free heap/high-water 仍待真机测量。

2026-07-18 clean reconfigure 后，不带连接 adapter 的完整 candidate 为
390,752 B，total image 390,607 B、static D/IRAM 147,767 B。带连接 adapter 的
candidate 为 1,176,672 B，total image 1,176,519 B、static D/IRAM 226,823 B，
相对前者分别增加 785,920 B、785,912 B 与 79,056 B；1.5 MiB app partition
剩余约 23%，static D/IRAM 余 114,937 B。该差值包含 Wi-Fi、NimBLE、protocomm、
Security 2 与它们的链接闭包，不代表运行时 heap/stack 已验收。

ESP-IDF 5.5 的官方 `protocomm_nimble.c` 会自行调用 `nimble_port_init()`、覆盖
`ble_hs_cfg` callback，并创建/停止 NimBLE host task。当前 adapter 因而把普通
App BLE 与 BLE provisioning 定义为排他角色：普通 BLE 仅在需要时启动自己的
NimBLE host；Security 2 配网仅在普通 BLE 未请求/未初始化时启动，否则返回 busy。
这不是推测性的共存承诺，角色切换与资源释放仍须真机验证。

`dependencies.lock` 锁定 32 个解析组件。供应链复验逐项校验所有
`CHECKSUMS.json`，共 7,600 个文件、0 个错误；ELF 审计确认 Cadenza adapter、
`wifi_prov_mgr_start_provisioning`、NimBLE init/stop 与 UAC public/upstream callback
符号同时存在，且没有修改 `managed_components` 源码。

`dependencies.lock` 是本次成功解析得到的完整依赖锁。不要修改本机 Arduino
2.0.17 framework package 来绕过 TinyUSB audio 配置。

PlatformIO 的 ESP-IDF builder 在本 spike 中曾因同一 managed-component object
出现重复 action 而失败；`platformio.ini` 仅保留失败复现入口。候选 baseline
使用官方 `idf.py`，现有 Arduino firmware target 继续作为回滚路径。
当前 UAC component 内部会在 TinyUSB alt-setting callback 中维护 `mic_active`，
但没有向 user callback 暴露 microphone close/open。candidate CMake 对锁定的
1.3.1 两个 callback 做编译期符号重命名；Cadenza adapter 提供公共入口、先委托
原实现，再把 mount/alt-setting 写入 callback-safe `UsbVoiceSessionBridge`。该方式
不修改 managed component 源码；升级 component 时必须重跑 callback 源码与 ELF
符号审计。它证明 session wiring 可构建，不把 build-only 行为当作真机断连/重连
验收。
