# System Overlay 实现与验证记录

状态：portable 实现与软件门禁完成；实体 T-Embed 的手感、可读性和 100 次循环验收待真机。

## 已交付边界

- `SystemSurfaceCoordinator`：单 interactive slot、完整按键序列 capture、转场 defer、固定容量 transient queue 和稳定 rejection diagnostics；
- System Menu：只保留 Resume、Sound、Motion，以及非 Home 场景的 Home action；Connectivity/Device 等无动作状态不占菜单空间；不进入 App catalog，不暴露 callback、driver 或任意 View；
- suspend-with-snapshot：Menu active 时冻结 App update/input/render 与背景，但 `SystemServiceHost` 继续 begin/commit；
- stateless `Panel`、`MenuRow`、`Selection`、`VolumeIndicator` 与 `Switch`；
- 固定合成顺序：App/transition → transient → interactive Menu → FrameCoordinator critical indicator；
- 320×170 与 400×240 使用同一 model、layout policy 和 renderer；不透明 Menu
  drawer 在 160 ms 内从右侧滑入，mask 同步渐强，左侧冻结 App 仍保留上下文，
  视觉语义对齐 Playdate 的覆盖式 System Menu。最终视觉去掉标题栏、箭头、双边框
  和横向 value 文本列；可操作项使用大字号，Sound 始终显示实心/空心音量柱，Motion
  使用紧凑横向开关，不绘制无动作底栏，Home 页面不显示冗余 Home action。

## 输入与生命周期证据

`cadenza_system_surface_tests` 与 `cadenza_app_runtime_tests` 锁定：

- opening long-press 的 held/release/click 不确认 Menu；
- closing long-press 到 release 全部由 System 消费，下一新序列才恢复 App；
- Menu turn 不进入 App，快速 delta 有界且每帧最多一个 Navigate intent；
- 转场中的 Menu 请求先 capture，目标 App stable 后再冻结并打开；
- Menu 多帧期间 App 不 update/render，也不发生 exit/enter；
- 平台事件和 setting command 仍在 frame transaction 中提交；
- busy、denied、invalid action 和 transient full 都保留 surface 并累计诊断。

双 profile Menu golden：

- 320×170 右侧 Menu drawer：`17166433962280013512`；
- 400×240 右侧 Menu drawer：`13747170315493377016`；
- 320×170 transient + indicator：`325176067826602694`；
- 400×240 transient + indicator：`3852824331461943778`。

## 内存与固件差异

Apple arm64 Debug 对象探针：

| 对象 | 大小 |
|---|---:|
| `MonoFramebuffer` | 12,024 B |
| `SystemSurfaceCoordinator` | 200 B |
| `AppRuntime` | 24,912 B |

`system_overlay_size_probe` 带编译期断言，要求 `AppRuntime` 小于三块最大
`MonoFramebuffer`；实现只复用既有 `outgoingFrame_`/`incomingFrame_`，没有第三块
12 KiB framebuffer。

PlatformIO Arduino-ESP32 2.0.17：

| 指标 | 改动前 | 当前 | 增量 |
|---|---:|---:|---:|
| static RAM | 98,808 B | 99,000 B | +192 B |
| flash | 358,805 B | 361,705 B | +2,900 B |

ESP-IDF 5.5 connectivity candidate 也实际编译 `system_surface.cpp`：binary
1,188,496 B、total image 1,188,351 B、static D/IRAM 231,143 B、Flash
1,052,528 B。Overlay 没有新增 task 或 stack，coordinator 继续在既有 frame
transaction 内同步推进。GCC 14 candidate 发现并修复了 Xtensa 上 `int32_t` 为 `long` 时
`std::min/std::max(int32_t, int)` 推导失败的问题。

## 尚不能由软件替代的验收

以下结论必须在实体 320×170 T-Embed 上完成，不能以 simulator 声称通过：

1. 650 ms 长按是否适合频繁打开；
2. 触发 release、关闭 held release 和快速连续操作的真实旋钮/按键手感；
3. Menu 文本、dither、selection、实心/空心音量格与开关在实际面板上的可读性；
4. Muted/有声反馈等价性以及至少 100 次 open/close 的稳定性。
