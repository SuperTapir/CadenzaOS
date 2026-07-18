# 连接平台演进前基线

日期：2026-07-18
change：`build-capability-aware-connectivity-runtime`
分支：`codex/refactor-system-services-foundation`

本文件固定 capability routing、resource lease 和 transport-specific connectivity
改造前的行为与资源基线。它只证明 host/build 状态，不证明实体连接、RF、功耗、
credential storage 或 Mac 录音。

## Host 与 desktop

执行：

```sh
./tools/check.sh host
./tools/check.sh desktop
```

结果：

- CTest 49/49 通过，总测试时间 1.93 s；
- `cadenza_shared_source_audit` 与 portable compile probe 通过；
- SDL 3.4.12 dummy video 启动通过，320×170 @ 2x；
- runner 的 CoreAudio 不可用，desktop 按既有非致命降级关闭音频。

`sizeof(cadenza::SystemSnapshot)` 在当前 macOS arm64 debug target 为 32 B。后续
transport-specific snapshot 必须设置并测试明确的静态上限，不能把 scan list、SSID
或 peer record 放入每帧复制对象。

## Snapshot 与 PCM golden

- `tests/snapshots/app_baselines.md` SHA-256：
  `f997708ddac0f83ad86cbb3dc31b188da2d326ef2786d89749d2cf293c069705`
- `tests/snapshots/gallery_baselines.md` SHA-256：
  `fe1c7ca1f38eac5b364b83aa8987a658dba27cea7e4bdd92b59345d11c5cdeef`
- Launcher turn 后 2,048 samples PCM FNV-1a：
  `9821519019372894971`

权威逐场景 framebuffer hashes 仍在上述两个 snapshot 文档；文件 hash 只是证明本
阶段没有批量改写批准表，不能替代单个 snapshot test。

## 当前 PlatformIO firmware

执行：

```sh
./tools/check.sh firmware
```

结果：

- `espressif32@6.12.0`；
- Arduino-ESP32 2.0.17；
- RAM 92,824 / 327,680 B（28.3%）；
- flash 344,521 / 3,145,728 B（11.0%）；
- `firmware.bin` SHA-256：
  `67cf78b4ae4472c075f2a67906a01932fdde80a2892f2a261f49c824b21e4fb4`。

### Capability/lease/connectivity portable runtime 增量（2026-07-18）

在加入 caller-bound capability、owner-aware lease 与分层 Wi-Fi/BLE snapshot 后，
再次执行 `./tools/check.sh firmware`：

- RAM 94,488 / 327,680 B（28.8%，相对基线 +1,664 B）；
- flash 347,061 / 3,145,728 B（11.0%，相对基线 +2,540 B）；
- `firmware.bin` SHA-256：
  `b3e7e5843c9f175c236ba230ea145c6745b843d8b893bef09eb38bee4fa5d084`。

首次跨构建回归暴露 Arduino `bit(b)` 宏与 capability 私有 helper 同名；产品代码
改用 `capabilityBit` 后通过，未修改或屏蔽 installed framework。该案例保留为必须同时
运行 host 与 current firmware 的证据，避免 host-only 编译掩盖 SDK 宏污染。

同一增量也用显式 `IDF_PATH=/Users/tapir/Development/esp-idf-v5.5` 和本地
PlatformIO Xtensa toolchain 重新配置并构建两个 ESP-IDF 5.5 graph：

- minimal UAC2+CDC：`0x3b250` / 242,256 B（与基线相同；不组合 Runtime）；
- full hardware candidate：`0x5f660` / 390,752 B（相对 388,032 B 基线 +2,720 B）；
- full candidate 已实际编译新增 `connectivity_service.cpp`、
  `system_resource_leases.cpp` 与 capability-aware Runtime；
- CMake 的非致命 esp_rom gdbinit warning 仍存在，不影响 ELF/bin 生成；实体 heap、
RF、功耗与 Wi-Fi/BLE/USB voice 并发仍保持 pending。

### 连接 candidate 与最终软件门禁（2026-07-18）

将 ESP-IDF Wi-Fi、NimBLE、Security 2 provisioning adapter 加入完整 graph 后：

- binary 1,176,672 B，SHA-256
  `d7451febf23c35ba620b18862f5791078447909776fe96a0089b280faf14dcd4`；
- total image 1,176,519 B，Flash 1,043,608 B；
- static D/IRAM 226,823 B（66.4%，余 114,937 B）；
- 相对不带连接 adapter 的当前 full candidate，binary +785,920 B、image
  +785,912 B、static D/IRAM +79,056 B；
- 使用官方 1.5 MiB single-app-large partition，剩余约 23%。

供应链检查验证 32 个 managed components 的 7,600 个清单文件，无 hash/size
错误；ELF 符号检查覆盖 Cadenza connectivity adapter、Wi-Fi provisioning、
NimBLE init/stop 与 UAC lifecycle bridge，且 `managed_components` 无源码 diff。

最终软件回归：host 50/50、strict-warning 49/49、ASan/UBSan 49/49、TSAN
connectivity/mailbox concurrency 2/2、desktop dummy 和 current PlatformIO firmware
均通过。current firmware 最新为 RAM 94,680 / 327,680 B（28.9%）、flash
349,609 / 3,145,728 B（11.1%），`firmware.bin` SHA-256 为
`83fa867de85abb6dcb7b58ca46e11798fbb443ae79c2b12bd80f9e399bb8d4f8`。
这组结果将状态收敛为
`software/build complete / hardware validation pending`。

## ESP-IDF 5.5 UAC candidate

SDK tag/commit：`v5.5` / `8c750b088c7cd857d079c0eeb495da199b359461`。

默认 `~/.espressif` 激活在本次复验时失败：Python environment 存在，但标准 IDF
tool registry 未记录已安装 compiler/GDB/OpenOCD。既有 candidate 实际使用
PlatformIO 的 `toolchain-xtensa-esp-elf`，并由 CMake cache/build.ninja 锁定绝对
compiler 路径。为验证当前 build graph，执行：

```sh
cmake --build build-idf-arduino-composite --target all
cmake --build build-idf-arduino-composite --target size
cmake --build build-idf-hardware --target all
cmake --build build-idf-hardware --target size
```

最小 Arduino/UAC2+CDC candidate：

- binary：242,256 B（`0x3b250`），app partition 剩余 77%；
- total image：242,103 B；
- static D/IRAM：62,371 B（18.2%）；
- Flash：176,300 B；
- binary SHA-256：
  `357fb88a464bc31128e525b6f383ac985e6b2212543a2d77f4fd85714fcf4fbe`。

完整 display/input/I²S0/I²S1/UAC candidate：

- binary：388,032 B（`0x5ebc0`），app partition 剩余 63%；
- total image：387,879 B；
- static D/IRAM：146,103 B（42.8%，余 195,657 B）；
- Flash：310,880 B；
- binary SHA-256：
  `5c1343ee6e42ead3fcf088dd9a1d68ca4960ff31c86bda8168b138aaca19a4fe`。

`esp_idf_size` 小版本/分类可使 total image 比既有采用文档中的 387,911 B 少 32 B；
binary 大小和 SHA-256 未变化，说明没有产品链接内容漂移。后续 clean-build 任务必须
补齐标准化 `IDF_TOOLS_PATH`/tool registry，不能把仅依赖旧 CMake cache 的命令描述
为干净环境可复现。

## 当前待暴露的错误行为

- `ConnectivityState::Connected` 无法区分 AP association、有效 IP 与 BLE peer；
- `SystemCommandSink` 没有 caller identity或 capability policy；
- voice analyzer 使用全局 bool intent，App 退出不会自动清理；
- `PlatformEventMailbox` 类型固定且只适用于一个 producer/一个 consumer。

这些是后续 characterization/failing tests 的目标。重构不得通过修改现有视觉/音频
golden 来掩盖契约变化。
