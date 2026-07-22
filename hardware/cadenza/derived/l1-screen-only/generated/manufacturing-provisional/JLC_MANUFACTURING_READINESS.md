# Cadenza L1 嘉立创制造包审计

## 当前结论

- 状态：`PASS_PROVISIONAL_DFM`
- `production_ready=false`
- Gerber、PTH/NPTH 钻孔、BOM、CPL 已可重复生成。
- BOM 与 CPL 均包含 70 个完全一致且不重复的贴装位号；最近两个元件中心相距 2.0065 mm，高于官方 0.2 mm 文件检查值；四个 `SCREW` 机械孔不进入贴装清单。
- PCB 为 2 层，外形约 130 × 60 mm；最小线宽 0.20 mm，最小报告间距 0.152 mm，最小过孔直径/钻孔 0.61/0.30 mm。
- 四个 M2 安装孔为独立 NPTH：2.0 mm，坐标和参考板不变。
- 当前 DRC 原始报告仍保留 95 项导入/库规则告警，但电气结果为 `0 unconnected`、`0 shorting_items`；Sharp 局部没有新增问题。

## 与嘉立创当前格式/工艺的对应

嘉立创 2026 KiCad 指南要求 BOM 至少包含 `Comment`、`Designator`、`Footprint`、LCSC/JLCPCB 料号，CPL 包含 `Designator`、`Mid X`、`Mid Y`、`Rotation`、`Layer`，坐标使用 mm：

- https://jlcpcb.com/help/article/how-to-generate-the-bom-and-centroid-file-from-kicad
- https://jlcpcb.com/help/article/advice-for-bom-and-cpl-files-preparation

本工程输出严格使用这些字段，并通过逐位号集合检查。官方当前 1 oz、1–2 层 FR4 最小线宽/间距为 0.10 mm；本地门禁仍采用更保守的 0.127 mm，当前实际 0.20/0.152 mm 均通过：

- https://jlcpcb.com/help/article/jlcpcb-copper-weight

Sharp FPC 连接器 `FH34SRJ-10S-0.5SH(50)` / `C324723` 在 2026-07-22 查询为 10P、0.5 mm、0.3 mm FPC、上下双接点，LCSC 显示 6,625 件现货。库存会变化，上传时仍需重新确认：

- https://www.lcsc.com/product-detail/C324723.html

## 文件

- `Cadenza-L1-Gerber.zip`
- `Cadenza-L1-JLCPCB-BOM.csv`
- `Cadenza-L1-JLCPCB-CPL.csv`
- `drill-report.txt`
- `manufacturing-bom-cpl-audit.json`
- `jlc-provisional-dfm-audit.json`
- `SHA256SUMS`

## 下单界面仍必须确认

1. Gerber 预览中只识别一个 130 × 60 mm 板框，四个 2.0 mm 孔显示为非金属化孔。
2. BOM/CPL 识别 70 个相同位号，无 `Unselected Parts`、重复位号或丢失位号。
3. 人工逐个检查有方向器件，至少包括 `U4`、`U7`、`LED1/LED2`、`CARD1`、`USB1` 和 `J_DISP1`。
4. `J_DISP1` 必须匹配 `C324723`，并确认 Pin 1 标记相对 PCB 右侧的位置。
5. 用户已确认真实屏幕与当前外壳窗口严丝合缝，并明确接受把最终 Pin 1、180° 折弯、插入、锁扣和 USB 间隙留给少量 EVT1 首板验证；不得据此扩大数量或称为量产冻结。

本报告证明文件格式和保守制造几何门禁通过，不替代嘉立创网页生产预览，也不替代真实排线试装。
