# R_PWR6 高电流桥接候选调查

本目录记录 `/VOUT -> +5V` 永久桥接方案的候选证据。L1 原理图候选现已指定
KiCad 标准 `NetTie:NetTie-2_SMD_Pad2.0mm` 的 4 × 2 mm 顶层铜桥；它仍未进入
正式 PCB 布局，也没有冻结最终回路、颈缩或温升结论。

- `R_PWR6_POWER_BRIDGE_CANDIDATE.md`：面向评审的比较与建议。
- `r_pwr6_candidate_summary.json`：机器可读的候选、计算、证据和门禁状态。
- `jlcsearch-C4103112-20260722.json`：查询时的 jlcsearch 原始响应快照。
- `vishay-wfz-jumper-30432.pdf`：厂家高电流零欧跳线数据手册，仅作电气基准；查询时未在 LCSC 找到。

当前状态：`candidate_only=true`、`selection_frozen=false`、`schematic_modified=true`、
`pcb_layout_validated=false`。2512 `RMCF2512ZT0R00 / C4103112` 保留为备选。
