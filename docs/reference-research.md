# Cadenza 1-bit Runtime 参考项目调研

本文记录 `build-portable-animation-runtime` 的上游参考、固定 commit、许可证、可借鉴设计和明确不采用的范围。参考仓库的浅克隆位于被 gitignore 的 `.research/references/`；它们只用于阅读，不是隐式构建依赖。

调研日期：2026-07-18。

## 结论摘要

- 桌面宿主采用 SDL3 + CMake；SDL2 已进入维护/弃用阶段，不作为新目标。
- Cadenza 自己拥有规范化 full 1-bit framebuffer；基础图元和字体栅格采用受控 U8g2 子集，sprite/dither/composition 仍由 Cadenza wrapper 定义。400×240 单 buffer 仅 12,000 bytes，值得用内存换取截图、离屏转场和精确测试。
- 动画 API 借鉴 LVGL、Tweeny 和 Playdate Timeline 的 offset/seek/reverse/repeat 语义，但不直接引入它们的动态分配模型。
- 固定容量与 clip stack 思路借鉴 microui；Cadenza 将容量耗尽从 assertion 改成可返回、可诊断的失败。
- 转场借鉴 Noble 的 Cover/Mix 分类以及 Taxman 的平台适配、dither 和 render texture；不引入 ECS、物理、Tilemap 或动态对象系统。
- 代码依赖优先复用已验证边界的成熟库；视觉资产不从参考引擎复制。首批 U8g2 字体逐个审核 provenance，font8x8 只保留为合法 fallback。

## 参考矩阵

| 项目 | 固定 commit | 许可证 | 借鉴 | 不采用 |
|---|---|---|---|---|
| [SDL](https://github.com/libsdl-org/SDL/tree/df7b9373613835de90034302512f3821f0df3bae) | `df7b9373613835de90034302512f3821f0df3bae` | zlib | SDL3 events、streaming texture、logical integer presentation、官方 CMake target | SDL-owned App/render state；SDL2 新代码 |
| [LVGL](https://github.com/lvgl/lvgl/tree/a73e59f6d144785012e44ab93f1f3d4dde48a180) | `a73e59f6d144785012e44ab93f1f3d4dde48a180` | MIT | display/input 分层；timeline offset、seek、reverse、repeat；动画冲突语义 | Widget/runtime；timeline 的 `malloc/realloc` descriptor array |
| [U8g2](https://github.com/olikraus/u8g2/tree/ab9e48b2228351e9476682a70b7f3ee4909cd585) | `ab9e48b2228351e9476682a70b7f3ee4909cd585` | BSD-2-Clause | **采用受控子集**：caller-owned full buffer、基础图元、字体、clip；未来可复用 Sharp setup | 不暴露 display/device API；不链接整套驱动与字体；不让其拥有 Runtime |
| [microui](https://github.com/rxi/microui/tree/0850aba860959c3e75fb3e97120ca92957f9d057) | `0850aba860959c3e75fb3e97120ca92957f9d057` | MIT | 固定数组、clip stack、renderer 边界、小 API | Immediate-mode widget；256 KB command list；overflow assertion |
| [Tweeny](https://github.com/mobius3/tweeny/tree/d50d96dfa61b115fb0d9fce59f7bbe4488f9b9b0) | `d50d96dfa61b115fb0d9fce59f7bbe4488f9b9b0` | MIT | typed tween、multi-point、step/seek、正反时间、easing 命名 | `std::function` callbacks、dynamic vectors、完整模板表面积；上游正在重写的 API |
| [NobleEngine](https://github.com/NobleRobot/NobleEngine/tree/93ffd6eb29d61c1af1cb8d40b78daf75c4e85946) | `93ffd6eb29d61c1af1cb8d40b78daf75c4e85946` | 混合：多数 MIT，logo 单独受限 | Transition 策略；Cover（中点换场）/Mix（新旧画面混合）；默认属性和 easing；dither dissolve | Scene/game engine；任何 logo、截图或 transition asset；Lua 动态对象 |
| [Taxman Engine](https://github.com/McDevon/taxman-engine/tree/51981885dcb9bb7b467cb578785ac68a084fd8ab) | `51981885dcb9bb7b467cb578785ac68a084fd8ab` | MIT | C platform adapter；400×240 Sharp 实例；dither、atlas、off-screen、profiler、确定性意识 | ECS/physics/collision/Tilemap；`calloc/realloc` render textures；硬编码分辨率 |
| [Timeline for Playdate](https://github.com/mierau/playdate-timeline/tree/7bfdd75f7b9cf4ef813d71b0621efac530985fe8) | `7bfdd75f7b9cf4ef813d71b0621efac530985fe8` | README 声明 Public Domain | 命名 range、append、overlap、loop range、查询 active progress | Lua table 分配；无稳定顺序的 `pairs`；seek 后需 update 才传播的行为 |
| [font8x8](https://github.com/dhepper/font8x8/tree/8e279d2d864e79128e96188a6b9526cfa3fbfef9) | `8e279d2d864e79128e96188a6b9526cfa3fbfef9` | Public Domain | 基础 ASCII 8×8 glyph 数据和明确 provenance | 直接暴露其 LSB-first row 格式；未经筛选的扩展字符集 |
| [doctest](https://github.com/doctest/doctest/tree/d44d4f6e66232d716af82f00a063759e9d0e50d6) | `d44d4f6e66232d716af82f00a063759e9d0e50d6` | MIT | 固定版本 single-header host test framework | 固件运行期依赖 |
| [stb](https://github.com/nothings/stb/tree/31c1ad37456438565541f4919958214b6e762fb4) | `31c1ad37456438565541f4919958214b6e762fb4` | Public Domain 或 MIT 双选 | `stb_image_write.h` 的桌面 PNG 输出 | 图片解码和固件依赖；其余未使用 headers |
| [gif-h](https://github.com/charlietangora/gif-h/tree/70b645280d5e687f5217177c9cfa2889b0a2ad5f) | `70b645280d5e687f5217177c9cfa2889b0a2ad5f` | Unlicense | 小型 GIF writer 和按帧 finalize API | 把 GIF 当权威快照格式；内部 RGBA buffer 进入核心 |

## 源码证据与设计影响

### 动画组合

LVGL `lv_anim_timeline_add` 复制 animation descriptor，并通过 `lv_realloc` 扩展数组；`set_progress` 会立即把绝对时间映射到每个 child。Tweeny 的 easing/callback 类型使用 `std::function`，callback queues 使用 `std::vector`。这证实两者的 API 语义成熟，但默认内存模型不符合本项目“桌面与 ESP32 同一固定边界”。

Cadenza 因此采用：

- typed `Tween<T>`；
- 非 owning `Playable*` composition；
- fixed-capacity Sequence/Parallel/Timeline；
- `add` 显式返回失败并累计 overflow diagnostic；
- `seek` 立即应用且默认不触发 completion side effect。

Playdate Timeline 的 range/append/loop API 很适合 Gallery 编排，但其 `gotoTime` 只修改时间，直到下一次 `update` 才把 progress 传播给 blocks。Cadenza 将 seek 定义为同步、可截图的权威操作。

### 转场与离屏画面

Noble 的 Cover transition 在遮满时激活新 scene；Mix transition 先截取旧 scene，再让新旧画面同时出现。这个分类解释了为什么当前硬编码百叶窗只需单画布，而 dissolve/slide 需要旧帧快照。

Taxman 的 `RenderTexture` 证明 off-screen 对 dither、transform 和场景转场有价值，但实现通过 `calloc/realloc` 管理像素。Cadenza 使用三个固定最大 buffer（source/destination/output），避免运行期分配；约 36 KB 的成本在 P8 再用真机测量判断是否需要复用。

### 图形、U8g2 spike 与固定容量

U8g2 的 page loop 节省 RAM，却要求重复执行全部绘图命令，不适合 seek 后截图、多画布转场和快照测试。Cadenza 的最大 full buffer 为 12 KB，因此选择 caller-owned full-buffer。

本地 400×240 spike 验证 `u8g2_ll_hvline_horizontal_right_lsb` 与 Cadenza row-major/MSB-first 契约一致。仅链接 pixel、text 和一个字体时，host executable 的有效代码约 6.5 KB、BSS 正好 12,000 bytes，且无 heap symbol；上游还已有 LS027B7DH01 400×240 Sharp setup。由此从“只参考 U8g2”改为“通过 `MonoCanvas` wrapper 采用其受控 raster/font 子集”。完整比较见 `docs/engine-adoption-decision.md`。

microui 展示了固定 command/root/clip stacks 的可移植性，但 command overflow 通过 `expect` 终止。Cadenza 的 atlas、timeline、particle 和 state table 在满容量时保持已有内容不变、返回错误，并把 overflow 暴露给 HUD 和测试。

### 字体与输出

首批字体优先从 U8g2 中选择，但每个字体都要追溯原作者与许可证；U8g2 的代码许可证不自动覆盖所有字体素材。font8x8 基础 ASCII 是 public domain，保留为独立 fallback；若使用，构建时转换其逐行 LSB-first 数据，framebuffer 仍保持 MSB-first。

PNG 和 GIF 都是 desktop writer：核心只暴露 framebuffer。PNG/原始 framebuffer hash 是 snapshot 权威；GIF 只供快速预览，因为 gif-h 接收 RGBA 且输出大小/量化不适合作为精确证据。

## 许可证与资产规则

1. 所有 vendored 代码固定到上表 commit，并复制对应 license 到 third-party notices。
2. Noble logo、示例 screenshots、transition image tables 和其他参考项目视觉资产一律不复制。
3. U8g2 字体不能仅因库代码是 BSD 就假设每个上游字体可自由复用；每种字体单独验证 provenance。
4. Cadenza 新纹理优先自行创作；公共领域或开源素材必须在 asset manifest 中逐项记录。
5. `.research/references/` 不进入版本控制，也不作为构建成功的隐式前提。
