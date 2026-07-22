# Cadenza L1 原理图隔离候选

状态：**已建立隔离副本，尚未实施 L1 电气修改。**

本目录在 2026-07-22 从 `hardware/cadenza/working-base/` 完整复制，初始 39 个文件与
来源逐文件 SHA-256 相同。`working-base` 和 `golden-import` 是受保护输入，禁止在其中
保存任何功能修改。

## 允许的本轮范围

- 只在本目录的 `.kicad_sch` 中实施
  `hardware/cadenza/gates/l1-schematic-delta-contract.json`；
- `J_DISP1` 使用 1x10 电气符号，但 `Footprint` 必须为空；
- 不选择 FH12、FH12A、FH34 或任何 FPC 物理朝向；
- 原理图门禁通过前禁止 Update PCB from Schematic；
- 不移动既有 PCB 器件、走线、板框或安装孔；
- 不勾选依赖真实 FPC/屏幕/Power-Lock 的 OpenSpec 任务。

## 初始复制证明

- 来源文件数：39
- 来源相对路径清单摘要：
  `c444ecc4110a098e981b125bdfac1e4369e61f5a4331c150eabfb368c9df7061`
- 完整逐文件记录：`SOURCE_MANIFEST.sha256`

真正编辑前后必须重新验证 `working-base`、`golden-import` 哈希未变化，并由 KiCad 10
重新导出 XML 网表、ERC JSON 和 PDF/SVG。基线 ERC 已有导入型告警，采用差分审查，
不能用总数归零作为可靠性证明。

具体 GUI 删除、新增、符号选择和逐端点接线见
[`L1_SCHEMATIC_EDIT_WORKSHEET.md`](./L1_SCHEMATIC_EDIT_WORKSHEET.md)。
