# KiCad 未修改 PCB 导入 DRC 分诊

检查对象：`reference-imported.kicad_pcb`  
检查方式：KiCad 10.0.4，不回填铺铜、不保存电路板、不做原理图一致性检查。  
原始报告：`drc-imported.json`  
加入项目本地封装库后：`drc-imported-with-local-footprints.json`

## 结果摘要

- DRC 违规项：263
- 未连接项：0
- 错误：121
- 警告：142

从原 `.epro` 导出项目本地封装库后，第二次检查为 193 条违规、121 个错误、72 个警告、0 个未连接项。原来的 77 条“封装库不存在”已经消失，新增 7 条实例与库副本差异告警。

这 263 条不能解释为参考样机存在 263 个硬件问题。它们主要描述 EasyEDA Pro → KiCad 转换后对象语义与 KiCad 默认规则的差异。参考板的器件、网络、过孔、走线和几何已经逐项匹配，但导入结果还不是可以直接继续编辑的 golden 工程。

## 转换后必须处理

### 项目封装库已经恢复

PCB 使用 25 种唯一封装。已经通过 KiCad 官方 `fp upgrade` 从原 `.epro` 导出 `Cadenza-re-easyedapro.pretty`，25/25 名称均与导入 PCB 匹配，并通过 `fp-lib-table` 注册。原包中的第 26 个 `.efoo` 未被主 PCB 使用。

剩余 7 条 `lib_footprint_mismatch` 集中为 H1 一条和 KEY6 重复六条，说明板上实例具有局部差异；在更新封装前需要逐图形比较，不能直接用库版本覆盖。3D 模型路径仍缺少对应文件，须在完整 GUI 导入或后续模型绑定阶段补齐。

### 原铺铜被拆成 401 个固定填充区

原工程有 2 个铺铜定义。KiCad 导入后得到 401 个已填充 zone polygon，其中 Top Layer 305 个、Bottom Layer 94 个、Multi-Layer 2 个。它们保留了当前画面，但不是两个可正常重算的参数化铺铜。

大量 `clearance` 违规来自这些固定片段与已有走线/过孔的边界重叠或取整差异。若直接按 `B` 重填，不能假设能恢复参考形状。

处理：保留未修改导入板作为视觉 golden；在派生副本中按原 2 个铺铜定义重建顶/底层铜区，重新填充后逐网对照，并再次确认 0 unconnected。不得只批量排除告警。

### U4/U7 的复合焊盘语义

`shorting_items` 主要集中在 U4 的 41 号散热焊盘和 U7 的 3 号复合焊盘。EasyEDA 封装中的多个同号铜形状被导入为部分带网络、部分 `<no net>` 的独立 pad，KiCad 因此把同一物理焊盘报告为短路。

处理：以原封装、器件数据手册和焊盘坐标为证据，把复合图形整理为同一编号/网络的 KiCad 焊盘，再核对阻焊、钢网和散热过孔。

## 不单独否定参考样机的项目

- `solder_mask_bridge`：主要来自细间距/复合焊盘阻焊开窗，需要按封装与嘉立创能力检查，但不能仅凭告警判定错误。
- `silk_overlap`、`silk_over_copper`、`silk_edge_clearance`、`text_thickness`：属于丝印和字体转换问题，派生版本清理即可。
- 小于 KiCad 默认 0.2 mm 的间距：先对照参考 EasyEDA 设计规则和嘉立创实际生产能力；只有影响改动区或低于制造门禁时才升级为阻断。
- 默认 0.5 mm 板边铜间距：参考板已打样；派生板仍应按最终嘉立创规则复核，不用该默认值追溯否定原样机。
- NPTH `annular_width`、`hole_clearance`、`padstack`：优先核对机械孔/定位孔的 NPTH 属性和实际钻孔，不把无铜孔的零孔环直接当作故障。

## 转换门禁结论

- `PASS`：77 个器件、58 个网络及名称、2 层铜、78 个过孔、506 段走线、130×60 mm 板框、20 个关键坐标、0 unconnected。
- `MANUAL_REVIEW`：正反面铺铜、丝印、阻焊和 3D 模型。
- `FIX_BEFORE_EDITING`：项目本地符号库与 3D 模型、2 个可重算铺铜、U4/U7 复合焊盘、7 个封装实例差异。

因此当前导入足以证明 KiCad 转换路线可行，但不能把 `reference-imported.kicad_pcb` 直接当作 Cadenza 编辑母版。
