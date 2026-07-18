# System services refactor baseline

日期：2026-07-18
适用 change：`refactor-system-services-foundation`
Git 基线：开始实施前的 `codex/refactor-system-services-foundation`，OpenSpec 文件之外 worktree clean。

本文是边界重构的 characterization baseline。数字证明当前代码在 host 和编译层可工作，不证明尚未接入的麦克风、USB、Wi-Fi 或 BLE 真机能力。

## 可重复门禁

```bash
./tools/check.sh host
./tools/check.sh desktop
./tools/check.sh firmware
./tools/check.sh diff
./tools/check.sh all
```

开始本 change 前最近一次 `all` 结果：

- normal host：39/39；
- SDL3 dummy-driver smoke：通过；
- PlatformIO `cadenza-t-embed`：通过；
- firmware static RAM：83,736 / 327,680 B（25.6%）；
- firmware flash：339,669 / 3,145,728 B（10.8%）。

严格 warning 与 sanitizer 的独立复现命令保存在 `docs/development.md`。任何修改 App lifecycle、source target、audio ownership 或 platform composition root 的任务完成前 SHALL 至少运行 focused + host；模块边界改变后 SHALL 运行 desktop + firmware；阶段里程碑 SHALL 运行 `all`。

## 行为 golden

- bundled App 两种 display profile 的 framebuffer hash：`tests/snapshots/app_baselines.md`；
- Gallery 代表场景 midpoint hash：`tests/snapshots/gallery_baselines.md`；
- Launcher turn 后 2,048 个提示音 samples 的 PCM hash：`9821519019372894971`；
- input、lifecycle、transition、Tween、sound、SDL callback 的可执行 characterization tests：`tests/` 与 CTest 39 项。

重构阶段不得用批量更新 golden 掩盖行为变化。若 hash 必须改变，任务记录 SHALL 说明规范原因、生成 inspectable artifact 并单独审核。

## 当前源码边界

开始前的已知边界债务：

- `lib/cadenza_core/include/cadenza/core/app_runtime.h` 在 core 中枚举五个 built-in App；
- App update/render 直接接收完整 `AppRuntime&`；
- `AppRuntime` 同时拥有 App 导航、音量、motion 和 sound service；
- `cmake/cadenza_core_sources.cmake` 把 `apps.cpp` 与 `gallery.cpp` 编进 `cadenza_core`；
- host/desktop/firmware composition root 从 Runtime 拉取声音输出。

新边界审计至少检查：

```bash
rg -n "Arduino|ESP32|TFT_eSPI|SDL|esp_|i2s_|tinyusb|tusb|WiFi|Bluetooth|millis|micros|Serial" \
  lib/cadenza_core lib/cadenza_system
rg -n "apps.cpp|gallery.cpp" cmake CMakeLists.txt platformio.ini
rg -n "AppRuntime[&*]|runtime\\.(sound|volume|motion)" lib/cadenza_apps tests
```

第一条命令允许命中明确的审计脚本/注释，但 portable target 的 public/private source 不得编译平台 API。最终权威检查由 `cadenza_shared_source_audit` 扩展后执行。

## 尚未获得的证据

当前 `/dev/cu.*` 与 macOS USB profiler 中没有 ESP32/T-Embed。以下证据必须标记 `hardware validation pending`：ES7210 真实通道/增益/噪声、I²S input/output 并发、Mac UAC 枚举和录音、USB 长时间时钟、睡眠/重连、RF coexistence、功耗和 acoustic leakage。
