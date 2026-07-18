# Engine and Library Adoption Decision

日期：2026-07-18  
适用 change：`build-portable-animation-runtime`

## 决策

Cadenza 不采用完整 UI/game engine。共享核心采用“小型自有 Runtime + 成熟窄依赖”的组合：

- **采用 SDL3**：只负责桌面窗口、事件和 framebuffer presentation。
- **采用 U8g2 的受控子集**：作为无硬件依赖的 1-bit 基础图元与字体栅格层；Cadenza 继续拥有 framebuffer、裁剪契约、诊断、sprite/dither/off-screen 组合和 presenter。
- **采用 doctest、stb_image_write、gif-h**：分别只用于 host tests、PNG 和便利 GIF 输出。
- **不采用 LVGL runtime/widgets**：保留其 display/input/animation 契约作为设计参考。
- **不直接采用 Tweeny**：保留 easing、typed tween 和 seek 语义作为参考，自有固定容量动画核心。
- **不采用 microui、Noble 或 Taxman engine**：只借鉴固定容量、转场分类和平台适配边界。

该边界不是“凡是自研更轻就自研”。规则是：第三方能力若能在不夺走 framebuffer/runtime 所有权、不破坏固定容量和确定性测试的前提下消除成熟基础工作，就采用；若集成后仍需重写大部分语义或引入第二套状态真相，就只借鉴契约。

## 可重复 spike

spike 源码位于 gitignored 的 `.research/spikes/`，上游固定 commit 见 `docs/reference-research.md`。

| 候选 | 场景 | 实测结果 | 结论 |
|---|---|---|---|
| U8g2 | 400×240 full buffer、pixel、text、单字体 | 约 6.5 KB linked code，12,000-byte BSS framebuffer，无 heap symbol；buffer 正是 row-major/MSB-first；上游已有 LS027B7DH01 400×240 setup | 采用受控子集 |
| Tweeny | 单 float tween + callback + midpoint seek | 构造 3 次分配/400 bytes；注册 callback 再 2 次/72 bytes；seek 0 次 | 不适合作为固定容量固件核心 |
| LVGL | 400×240 I1、一个 object、一个 timeline、一次 full render | 约 461 KB linked code，约 78.6 KB data/BSS；默认 64 KB pool 中使用约 16%；I1 buffer 另需 12 KB + 8-byte palette | 不采用完整 runtime |
| SDL3 | CMake package、事件、logical integer presentation | 系统已有官方 CMake target，能够只做 host adapter，不进入 core | 采用 |

数字来自 AppleClang `MinSizeRel/-Os` host spike，只用于比较依赖形态，不当作 ESP32 最终尺寸承诺。固件最终尺寸仍由 PlatformIO map/size 回归证明。

## 为什么 U8g2 是合适的复用边界

U8g2 的 `u8g2_ll_hvline_horizontal_right_lsb` 使用 `offset = y * tile_width + x/8` 和 `0x80 >> (x & 7)`，与 Cadenza 既定的 row-major/MSB-first framebuffer 完全一致。它允许 caller-owned buffer、整数图元、裁剪和字体解码，因此 host 与固件可以运行相同的 raster 代码，TFT/SDL/未来 Sharp 只 present 同一份像素。

Cadenza 不暴露 U8g2 display/device API 给 App。`MonoCanvas` 是 anti-corruption wrapper，并负责：

- 固定尺寸和外部 buffer 生命周期；
- half-open clip、无效图元和越界诊断；
- 统一 `1 = black` 语义；
- bitmap/sprite/atlas、flip、XOR、pattern/dither 和多画布组合；
- 只链接审核过的 source/font，避免整个设备和字体目录变成产品 API。

首批字体从 U8g2 中逐个选择并记录原作者/许可证；库本身的 BSD-2-Clause 不能替代字体 provenance 审核。

## 为什么不采用 LVGL 或 Tweeny

LVGL 能完成 I1、输入、动画、timeline、snapshot 和 widget，但采用它会把 App 模型改成 object tree/style/event runtime，并引入动态对象与独立内存池。Cadenza 仍需自写 Playdate 风格转场、camera、particle、sprite/state、时间调试和固定容量诊断；因此节省的代码不足以抵消第二套 runtime 和测试状态。

Tweeny 的 easing 和 seek API 很接近目标，但 delay/repeat/yoyo/composition/Spring/固定容量失败语义仍需包裹或补写，而其 `std::vector`/`std::function` 已在最小场景产生分配。直接依赖会留下几乎相同的实现工作，并破坏运行期无分配约束。

## 重新评估触发条件

出现以下任一证据时重新做 spike，而不是把本决策永久化：

- 产品转向大量表单/widget/accessibility，而不是 bespoke 1-bit interaction；
- U8g2 wrapper 需要 fork 大量上游代码或基础图元 golden test 无法稳定；
- ESP32 链接证明选定 U8g2 子集成本显著高于 host spike，且无法通过 source/font 裁剪解决；
- 新库能同时满足 caller-owned framebuffer、无运行期 heap、同步 seek、固定容量 composition 和双平台测试。
