# 参考 PCB 自动分析限制

日期：2026-07-22

对 `working-base/*.kicad_pcb` 运行 `kicad` skill 的 `analyze_pcb.py --full`
时，分析器在 `_polygon_bbox()` 抛出 `ValueError: min() iterable argument is empty`。

原文件检查显示，EasyEDA→KiCad 导入把走线端泪滴保存成大量 `zone`；部分
`polygon/filled_polygon` 只含 KiCad `arc` 节点，没有分析器当前实现所期待的
普通 XY 点。分析器因此无法计算该对象的包围盒。

这是一项**工具解析兼容限制**，不是 DRC、断线或铺铜缺失结论：

- 不修改冻结参考 PCB 来迎合分析器；
- 当前仍使用 KiCad `pcbnew`、原始文件统计、58/58 连通门禁和人工渲染作为基线证据；
- 正式派生 PCB 保存/重铺铜后重新尝试完整 PCB 分析；若仍失败，再为分析器准备
  只用于分析的临时规范化副本，并对规范化前后网络、板框、孔位和铜对象做差异门禁；
- 在完整 PCB 分析成功前，不宣称已经完成最终 PCB/EMC/热/DFM 审查。

失败命令：

```sh
python3 /Users/tapir/.codex/skills/kicad/scripts/analyze_pcb.py \
  hardware/cadenza/working-base/Cadenza-reference-ESP32-S3-game-console-original-v2-20260722.kicad_pcb \
  --full --output hardware/cadenza/analysis/l1-baseline/pcb.json
```
