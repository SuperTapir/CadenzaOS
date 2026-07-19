# Activation Timer 变更前基线

记录时间：2026-07-19。基线 commit：`3852248`（detached worktree）。本文件只保存变更前可比较证据，不把当前 Timer 实现结果写回基线。

## 自动化与桌面

- CMake host Debug 使用 AppleClang 21、bundled Python 3.12.13 配置成功；
- `ctest --output-on-failure`：71/71 通过，14.78 秒；
- SDL 3.4.12 dummy video 启动 320×170 @2x，2 frames smoke 通过；
- dummy 环境没有 CoreAudio 输出，desktop 正确降级为 graphics-only；
- 默认 `tools/check.sh` firmware 路径 `../.platformio-env/bin/pio` 在本 worktree 不存在，因此本轮未得到新的 pre-change firmware binary；最近已保存且可复核的项目基线来自 `docs/verification.md`：99,184 / 327,680 B RAM，408,997 / 3,145,728 B Flash。

初次 CMake 缓存指向已删除的 `/Library/Frameworks/Python.framework/Versions/3.13/bin/python3`；显式切换到 workspace bundled Python 后重跑通过。这是本机环境漂移，不是项目回归。

## Timer App framebuffer

变更前 `TimerApp` 是正向 elapsed demo，fresh stable render hash：

| Profile | Hash (decimal) |
| --- | ---: |
| 320×170 | 2172376209712558838 |
| 400×240 | 8667913246713477979 |

Launcher、Motion、Settings、Gallery 和 selection 完整基线保存在 `tests/snapshots/app_baselines.md`，Timer 变更不得无解释修改非 Timer 项。

## System Overlay

变更前 `SystemSurfaceCoordinator` 为 200 B，`AppRuntime` 为 24,912 B；Menu/indicator hashes：

| Profile / scene | Hash (decimal) |
| --- | ---: |
| 320×170 Menu | 17166433962280013512 |
| 400×240 Menu | 13747170315493377016 |
| 320×170 transient + indicator | 325176067826602694 |
| 400×240 transient + indicator | 3852824331461943778 |

来源为 `docs/system-overlay-implementation-report.md`，最终要以新的 size probe 和 snapshot 解释 TimerAlert/indicator 增量。

## Interaction Sound

变更前 palette 共 15 个 cue：Navigate、Boundary、Confirm、Back、ToggleOn、ToggleOff、Reject、Complete、Warning、Failure、Notification、Connect、Disconnect、PowerOn、PowerOff。每个 cue 的 44.1 kHz / 1 秒 PCM hash 在 `tests/interaction_sound_tests.cpp` 冻结；新增 Timer cue 不得改变原 15 项 hash。
