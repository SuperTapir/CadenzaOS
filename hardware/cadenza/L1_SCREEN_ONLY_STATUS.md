# Cadenza L1 Screen-only 当前状态

日期：2026-07-22

结论：**允许制作少量 L1 EVT1 工程样机，但不是量产冻结。** 当前 L1 是“只替换 Sharp 屏幕并调整屏幕外壳”的最简 MVP。用户已确认真实屏幕与当前窗口严丝合缝，并明确接受把最终 Pin 1、FPC 折弯、锁扣和 USB 间隙留给首板验证；不再等待 L2。

## 已完成

- 旧的过度扩展候选已完整复制到 `archive/l1-overextended-experiment-20260722/`，267 个归档文件通过 SHA-256 复核。
- 新的 `derived/l1-screen-only/` 从 working-base 的 39 个文件逐项复制；源哈希全部一致。
- 新 L1 PCB SHA-256 为 `70a46bd100ed8184edcd2e90d3dbd21fff676999e0f3fa71cad1121c8c7b4101`，与 golden PCB 逐字节一致。
- 新基线由 KiCad 10 重新导出网表并通过连通门禁：77 个器件、58 个有效 PCB 网络、58/58 网络端点精确匹配。
- OpenSpec 已重写为：L1 最简可打板 MVP、L2 完整 Cadenza MVP、L3 可选优化；strict validation PASS。
- 最小修改合同已收紧为 5 个旧器件删除、2 颗原位电容局部改网、70 个参考器件冻结；新增仅 J_DISP1 和 C_DISP1。
- screen-only 原理图候选已可重复生成并通过 20/20 边界检查；组件数为 74，USB、电源、按键及其余保留网络端点零漂移。
- J_DISP1 已锁定为上下双接触的 Hirose `FH34SRJ-10S-0.5SH(50)` / `C324723`（Extended、可 SMT 装配）；项目 footprint 已按厂图与第二来源交叉核对。用户正反面照片已确认 FPC 从显示面朝上时的下边缘伸出且金手指在背面；PCB 旋转角度和折后最终 Pin 1 对应关系仍待验证。
- 自包含候选 ERC 为 617 项（14 error、603 warning），相对导入基线 691 项（18 error、673 warning）新增 error 为 0；剩余项目仍是待分诊的参考导入债务。

## L1 唯一修改

- 删除原 FPC1、U6、Q2、R13、R14。
- C20（10 µF/25 V）与 C21（100 nF/50 V）保持料号、封装、位置和旋转，只从 `3V3LCD` 改接 `+5V`，复用作 Sharp 电源去耦。
- 新增 Sharp 10-pin J_DISP1 和 DISP–GND 的 C_DISP1 100 nF。
- 复用 GPIO48/12/14/47/39；不修改 USB、电源控制、按键、音频、存储、麦克风、板框或安装孔。
- 外壳差异只允许出现在屏幕窗口、支撑/软垫、压框和 FPC 避让区域。

## EVT1 付款前仍需在嘉立创网页确认

1. Gerber 只识别一个约 130 × 60 mm 的 2 层板框，四个 2.0 mm 安装孔为 NPTH。
2. BOM/CPL 都识别相同的 70 个贴装位号；Top 62 个、Bottom 8 个，选择双面贴装。
3. `J_DISP1` 匹配 `C324723`，位于 Top 面，Pin 1 与 PCB 丝印 `P1` 同侧。
4. 人工检查 `U4`、`U7`、`USB1`、`CARD1`、`LED1`、`LED2`、`J_DISP1` 的旋转方向。
5. 只下少量工程样机；到板后按 `L1_EVT1_RELEASE.md` 断电检查，再进行首次限流上电。

L2 的 B/Menu、编码器+A 和可选 Power/Lock 不属于上述下单前置条件。
