# Cadenza L1：Screen-only 最简 MVP

此目录于 2026-07-22 从 `hardware/cadenza/working-base/` 逐文件复制建立。复制时 39 个源文件 SHA-256 全部一致，其中 PCB SHA-256 为 `70a46bd100ed8184edcd2e90d3dbd21fff676999e0f3fa71cad1121c8c7b4101`，与 `golden-import` PCB 逐字节一致。

## L1 唯一允许的功能变化

- 将原 ST7789 12-pin TFT 显示子系统替换为 Sharp LS027B7DH01 10-pin Memory LCD；
- 增加 Sharp 必需的 5 V 供电去耦、EXTMODE 配置、EXTCOMIN 和 DISP 控制；
- 仅在原显示区域进行必要器件放置、局部走线和铺铜恢复；
- 外壳只调整屏幕窗口、屏幕支撑/软垫、压框和 FPC 避让。

## L1 明确禁止的变化

- 不移动或重构 USB-C、CH340C 和下载电路；
- 不增加独立 Power/Lock，不修改 IP5306 KEY；
- 不改造参考按键、摇杆、音频、microSD、麦克风或其他非显示子系统；
- 不移动板框、四个安装孔和非显示器件；仅 C20/C21 作为原显示去耦器件允许移动；
- 不继承 `derived/l1-candidate`、`l1-pcb-candidate` 或 `l1-pcb-routing-candidate` 的修改。

旧的过度扩展实验已封存于 `hardware/cadenza/archive/l1-overextended-experiment-20260722/`。

`SOURCE_BASELINE.sha256` 记录复制完成、添加本说明之前的 39 个 working-base 源文件哈希。只有通过最小差异门禁后，当前目录才可继续进入制造候选阶段。

## 可重复的 Screen-only 原理图候选

运行：

```bash
hardware/cadenza/derived/l1-screen-only/tools/build_and_verify_screen_only.sh
```

脚本不覆盖本目录的基线原理图。候选另存为
`generated/candidate/Cadenza-L1-Screen-Only.kicad_sch`，显示子页为
`generated/candidate/Cadenza-L1-Screen-Only-Display.kicad_sch`。KiCad CLI 新鲜导出的
基线/候选 XML netlist 和只针对 Screen-only 边界的验证报告位于 `generated/evidence/`。

当前已将 J_DISP1 锁定为 Hirose `FH34SRJ-10S-0.5SH(50)` / LCSC
`C324723`，使用经厂图与 EasyEDA land pattern 交叉核对的项目封装。PCB 候选中的连接器
坐标/角度已冻结为 `(154.625 mm, 128.500 mm), 0°`；这只冻结 CAD 放置，不代表已经确认
真实 FPC 经 180° 折叠后的触点面和 Pin 1 映射：`fpc_selection_frozen=true`、
`pcb_connector_rotation_frozen=true`、`physical_fpc_mapping_verified=false`、
`production_ready=false`。

## 当前候选与外壳 fit-check

- PCB：`generated/candidate/Cadenza-L1-Screen-Only.kicad_pcb`
- PCB 证据：`generated/evidence/PCB_CANDIDATE_REPORT.md`
- 外壳：`../../mechanical/l1-fit-check/generated/l1-front-fitcheck.step` 与 `.stl`
- 外壳审计：`../../mechanical/l1-fit-check/FITCHECK_REPRO_AUDIT.md`

当前外壳文件可用于一次牺牲性试打印和真实屏幕/FPC 试装。正式整壳与首板释放前，仍须核对
Pin 1/触点面，并完成折弯、插入、锁扣和合壳验证；机器检查不能替代这一步。
