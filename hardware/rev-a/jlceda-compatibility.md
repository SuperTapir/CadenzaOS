# 嘉立创 EDA 专业版兼容性验证

状态：`D0 SMOKE PASS WITH FOLLOW-UP`

验证日期：2026-07-20

## 验证对象

- 嘉立创 EDA 专业版：V3.2.148 Web 版
- 本地 KiCad CLI：10.0.4
- 嘉立创工程：`Cadenza Rev A Import Smoke D0`
- 嘉立创工程 ID：`1ffcb9069f23415386f488fc3925510c`
- 工程地址：`https://pro.lceda.cn/editor#id=1ffcb9069f23415386f488fc3925510c`
- 导入文件：`generated/import-smoke-test/cadenza_rev_a_smoke-jlc-import.zip`
- 导入文件 SHA-256：`e3716c74c1c325b6953c67ed10a80aa4f732abc765e5840b4b8dbdee5d62ef17`
- 嘉立创在线备份：`main_import-smoke-d0`
- 备份说明：`Golden backup after verified KiCad import. DO NOT FABRICATE.`

> 这是格式兼容性样板，不是 Rev A 产品 PCB，禁止下单生产。

## 本地门禁结果

- KiCad 10 成功解析 legacy 5.1/5.9 兼容子集 PCB。
- KiCad 10 成功解析 legacy 原理图并导出 PDF。
- PCB DRC：0 条违规。
- 未连接项：0。
- 封装错误：0。
- ZIP 连续生成两次 SHA-256 相同，证明导入包可重复生成。
- ZIP 根目录只包含一套自包含 KiCad 工程文件：`.kicad_pcb`、`.sch`、`.lib`、`-cache.lib`、`.pro`。

## 嘉立创实际导入结果

嘉立创返回 `Import project success`，并在私人工作区创建了以下原生文档：

- `Board: cadenza_rev_a_smoke`
- `Schematic: cadenza_rev_a_smoke`
- `PCB: cadenza_rev_a_smoke`

PCB 画布实际打开并确认：

- 4 个铜层均存在：`Top Layer`、`Bottom Layer`、`In1.Cu`、`In2.Cu`。
- 2 个两针连接器 `J1`、`J2` 可见，焊盘和丝印引用保留。
- 2 个 3.2 mm NPTH 安装孔可见。
- `TEST_SIG` 顶层走线和 `GND` 底层走线均保留，并显示正确网络名。
- 矩形板框、内部地铜区边界和 `CADENZA REV A IMPORT SMOKE` 丝印保留。
- PCB 画布证据：`generated/import-smoke-test/reports/jlceda-import-pcb.png`。

## 兼容性结论

当前代码生成的 KiCad 5.1/5.9 兼容子集可以作为嘉立创 EDA 专业版的稳定交换格式。与直接生成嘉立创 V3 私有工程结构相比，该路径更容易本地解析、审查、运行 DRC、做确定性构建和版本控制。

完整 Rev A 不得仅凭“导入成功”下单。每次导入必须执行：

1. 在 KiCad CLI 中运行 DRC，并要求 0 违规、0 未连接。
2. 核对板框尺寸、层数、网络数、器件数、过孔/安装孔和关键 keepout。
3. 在嘉立创中重新铺铜；导入器会重建铜区，不能信任源文件中的旧填充缓存。
4. 在嘉立创中再次运行 DRC，并审查所有规则映射、热焊盘、阻焊扩展和差分约束。
5. 导出 Gerber、钻孔和装配文件后，用独立查看器做最终制造审查。
6. 第一版只下单少量工程样板，完成上电、USB、音频、显示、充电和热测试后再迭代。

## 尚未关闭的验证项

- 嘉立创在线备份已经创建，但 Backup Management 页面在当前自动化界面中只暴露侧栏菜单，本地 `.epro` 尚未下载。
- 画布确认了地铜区边界，但尚未在嘉立创内执行重新铺铜和嘉立创 DRC。
- 尚未用嘉立创属性面板独立复测 40 x 30 mm smoke 板尺寸；该尺寸已由 KiCad 源文件和本地结构检查证明。
- 尚未执行从 `.epro` 回导/重开验证。

这些项目不否定 KiCad 导入兼容性通过，但在把同一链路用于完整 Rev A PCB 之前必须完成。
