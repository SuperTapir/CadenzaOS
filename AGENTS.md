# 项目规范

## OpenSpec 规范

OpenSpec 提案默认使用中文书写。

## T-Embed 固件刷写（默认 ESP-IDF）

T-Embed **主固件是 ESP-IDF 5.5**（Runtime + ES7210 + **UAC2 麦克风** + CDC），不是 PlatformIO。

刷机、真机验证、修 USB mic / 扬声器相关问题时：

- **默认**使用 `tools/firmware_uac.sh flash`（或 `tools/check.sh firmware` 构建后再 flash）
- **禁止**把 `pio run -t upload` / `tools/check.sh firmware-pio` 当作默认刷写路径
- PlatformIO（`firmware-pio`）是**无 UAC 的回滚路径**；误刷会导致 Mac 上看不到 Cadenza USB 麦克风
- 只有用户明确要求「刷 PlatformIO / 无 UAC 回滚」时才用 PIO
- 设备已枚举为 USB Audio 时，串口可能变成 `/dev/cu.usbmodemSPIKE_*`；若 flash 连不上，按 `docs/development.md` 先进入 download mode（按住 BOOT → 点 RST → 松 BOOT）再刷

组合根：`.research/spikes/uac_idf/`。可移植逻辑在 `lib/`，两条固件路径共享；**呈现与能力以 IDF 组合为准**。

手感相关约束（勿回退）：

- UI 帧循环必须钉在 **core 1**（`cadenza_ui`），扬声器任务在 **core 0**；不要再把帧循环放回 `app_main`/CPU0，否则会被 speaker 抢占，Launcher 滚动会明显不如 PlatformIO
- 硬件构建默认 `CONFIG_COMPILER_OPTIMIZATION_PERF`（`-O2`），不要无故改回 `-Os`

## 工程决策原则

### 先充分调研，再快速实现

涉及平台核心、长期公共契约、新子系统或难以逆转的技术选型时，必须先完成可审计的充分调研，再进入正式设计与实现。目标是踩在前人的肩膀上，优先理解、验证并复用业界和开源社区已经证明有效的思想，而不是凭直觉从零发明。

充分调研不能只阅读介绍性文章或官方 API 文档，至少应当：

- 广泛寻找有代表性的成熟产品、开源项目和失败案例；
- 锁定所研究源码的版本或 commit，并确认许可证与可复用边界；
- 阅读与当前问题直接相关的关键源码，而不是只看 README、宣传材料或二手总结；
- 比较候选方案的语义、架构、性能、内存、可移植性、维护成本和已知限制；
- 用最小实验、测试、benchmark、golden case 或真实硬件验证关键假设；
- 明确记录采用什么、不采用什么、为什么，以及哪些结论仍待验证。

推荐顺序为：

```text
广泛收集参考
  → 锁定版本、许可证与关键源码
  → 深入阅读并比较方案
  → 用实验验证关键假设
  → 形成采用/不采用决策
  → 编写或修订 OpenSpec
  → 快速、集中地实现和验证
```

在证据尚不充分时，应明确标记为探索或草稿，不得把初步印象包装成平台定案，也不得为了显得有进展而过早进入正式实现。调研充分之后则应果断收敛，避免重复调研拖慢已经明确的实现。
