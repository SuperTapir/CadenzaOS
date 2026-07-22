# L1 PCB 同步候选 DRC 分诊

日期：2026-07-22  
工具：KiCad CLI 10.0.4

## 结论

两块板均能被 KiCad 正常读取并完成 DRC，但当前状态是**未布线同步候选**，不是
DRC 通过或可下单板：

| 候选 | DRC 违规 | 未连接 |
|---|---:|---:|
| FPC Pin 1 rotation 0 | 194 | 56 |
| FPC Pin 1 rotation 180 | 194 | 56 |

56 个未连接来自新显示、Power/Lock 和移动后的 USB 区域尚未重新布线，属于当前阶段
预期结果，但在完成布线前全部是生产阻断项。

## 分类计数

| KiCad 类别 | rotation 0 | rotation 180 |
|---|---:|---:|
| `clearance` | 8 | 7 |
| `silk_overlap` | 34 | 35 |
| `lib_footprint_issues` | 68 | 68 |
| `silk_over_copper` | 32 | 32 |
| `shorting_items` | 0 | 0 |
| `courtyards_overlap` | 1 | 1 |
| `starved_thermal` | 6 | 6 |
| `copper_edge_clearance` | 13 | 13 |
| `silk_edge_clearance` | 13 | 13 |
| `text_thickness` | 5 | 5 |
| `track_dangling` | 4 | 4 |
| `padstack` | 4 | 4 |
| `annular_width` | 4 | 4 |
| `text_height` | 2 | 2 |

参考板基线为 193 条违规、0 个未连接，但总数不能直接相减：本候选加入了 26 个真实
器件，并故意删除了 142 个旧走线/过孔对象和 81 个对应的旧 teardrop zone；4 个常规
铺铜区已按新焊盘重新填充。布局清理已把新增器件造成的跨网短路降为 0，新增庭院重叠
降为 0；当前唯一庭院重叠是参考板原有的 `L2/C1`。下一步仍须完成 56 个未连接，
不能以“参考板已有告警”为免责依据。

后续受控布线候选位于 `../l1-pcb-routing-candidate/`：当前 51 个铜对象已完成三个局部
Power/Lock 网络和完整 USB 连接器网络（CC1/CC2、D+/D−、VBUS），把两种候选的
未连接从 56 降到 38；另外完成 R_PWR4 gate pulldown 和 R_PWR2 的 3V3M 下接。专项
验证把 `tracks_crossing` 纳入高风险类别；当前新增走线没有高风险 DRC。Sharp、其余 Power/Lock、LED、测试点和
高电流桥仍未完成，因此它仍不是完整布线。

原始证据：

- `drc-rotation-0.json`
- `drc-rotation-180.json`
- `rotation-0-top.png`
- `rotation-0-bottom.png`

状态标记：`routed=false`、`drc_passed=false`、`production_ready=false`、
`selection_frozen=false`。
