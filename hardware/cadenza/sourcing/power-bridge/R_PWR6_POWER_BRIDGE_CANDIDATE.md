# R_PWR6 高电流桥接候选调查

查询时间：2026-07-22 05:32:15 +08:00（2026-07-21T21:32:15Z）

## 结论

首选候选是 **PCB net-tie 短宽铜桥**，备选候选是 **Stackpole RMCF2512ZT0R00（LCSC C4103112，2512，0 Ω）**。两者都还没有冻结，也没有写入原理图或 PCB。

首选 net-tie 的理由：这里需要的是永久连接，而不是限流、熔断或配置电阻。用铜桥可以去掉器件残余电阻、贴装费和库存依赖；KiCad 正式支持两个不同网络由 net-tie footprint 内连续铜皮短接，并可正确通过 DRC。它必须做成自定义、短而宽的电源 net-tie，不能直接拿 0.5 mm 小信号库封装使用。

备选 C4103112 的理由：它是当前 LCSC 可检索、可贴装、可返修的明确器件，厂家给 RMCF2512 跳线额定电流 3 A，高于 2.1 A 目标。但厂家同时给出跳线电阻最大 0.05 Ω；按最坏值计算，2.1 A 时压降 105 mV、发热 0.221 W。电流余量只有约 42.9%，所以它适合作为首板保守备选，不是“无损铜条”。

## 输入边界

- 功能：移除参考设计旧 SW1 后，将 `/VOUT` 永久接到 `+5V`。
- 设计目标电流：2.1 A 连续路径；本调查用 3 A 作为 PCB 铜桥的布线检查目标。
- 当前合同：`R_PWR6 = 0R_HIGH_CURRENT_TBD`，footprint 空。
- 本调查不证明 IP5306、连接器、过孔、整段 +5V 走线或热设计能连续承受 2.1 A；这些仍需整条电源路径复核和首板实测。

## 方案比较

| 方案 | 电流、压降与发热 | 嘉立创制造/装配 | 返修性 | 判断 |
|---|---|---|---|---|
| 自定义 PCB net-tie | 由铜厚、最窄宽度、长度和温升决定。以 1 oz、1.5 mm 宽、1 mm 长的简化室温铜计算，约 0.328 mΩ，2.1 A 时约 0.69 mV / 1.45 mW；这只是几何 sanity check，不是热认证 | 无 BOM、无贴装、无扩展料费。需正确设置 net-tie pad group，并让桥及相邻电源路径至少按 3 A 检查 | 不能像电阻一样直接取下；可预留切断位置和测试焊盘 | **首选候选** |
| 专用 1206 高电流 0 Ω（Vishay WFZ1206 基准） | 厂家额定 38 A、最大 0.5 mΩ；2.1 A 最坏约 1.05 mV / 2.21 mW | 电气很合适，但本次 LCSC 精确查询 0 条，不能假定嘉立创可贴 | 易更换 | 电气基准；当前不作为 LCSC 首选 |
| 普通 1206 0 Ω（以 Stackpole RMCF1206 系列为例） | 厂家跳线额定 2 A，低于 2.1 A 目标；最大 0.05 Ω | 常见、便宜，但规格不满足当前目标 | 易更换 | **排除** |
| 2512 0 Ω：RMCF2512ZT0R00 / C4103112 | 额定 3 A、最大 0.05 Ω；2.1 A 最坏 105 mV / 0.221 W | jlcsearch 快照：库存 6740，`is_basic=false`、`is_preferred=false`，单价字段约 US$0.0451；装配时应按扩展料预期并在上传页复核 | 易更换，也方便首板断开测流 | **备选候选** |
| 默认闭合焊桥 | 电流能力没有独立器件额定值，取决于细颈铜宽、铜厚、焊料形状和工艺；不应仅凭“默认闭合”认定可承受 2.1 A | 可免 BOM，但开放焊盘、可切细颈和锡量会引入工艺变量；若做成宽铜桥，本质上应按 net-tie 管理 | 最容易切开/补焊，但重复性较差 | 不用于正式高电流通路；最多作实验选件 |

## 证据与计算

1. Stackpole 厂家 RMCF/RMCP 数据手册（Rev 2026-01-07）给出：RMCF1206 跳线额定 2 A，RMCF2512 跳线额定 3 A、短时过载 15 A，跳线电阻最大 0.05 Ω。  
   厂家 URL：<https://www.seielect.com/catalog/sei-rmcf_rmcp.pdf>
2. Vishay 厂家 WFZ Jumper 数据手册（Document 30432，Rev 2025-06-12）给出：WFZ1206 额定 38 A、WFZ2512 额定 63 A，0603–2512 最大电阻 0.0005 Ω。目录内保存了原 PDF。  
   厂家 URL：<https://www.vishay.com/docs/30432/wfz-jumper.pdf>
3. KiCad 官方文档说明 net-tie 可让两个不同网络在 footprint 内由连续铜皮连接，并通过 net-tie group 合法通过 DRC。  
   URL：<https://docs.kicad.org/master/en/pcbnew/pcbnew.html#creating-net-ties>
4. JLCPCB 官方资料给出 1 oz 铜厚约 35 µm，并举例：外层 1 oz、3 A、10 °C 温升约需 1.27 mm 线宽。本报告因此将 1.5 mm 作为候选 net-tie/相邻短路径的最低设计起点，而不是最终认证值。  
   URL：<https://jlcpcb.com/help/article/jlcpcb-copper-weight>、<https://jlcpcb.com/blog/copper-weight-and-trace-width-balance>
5. jlcsearch 精确查询 `C4103112` 返回一条：`RMCF2512ZT0R00`、2512、库存 6740、非 Basic、非 Preferred。原始响应已保存并记录 SHA-256。jlcsearch 是社区接口，不是 JLCPCB 官方库存 API；下单前必须在 JLCPCB BOM 匹配页再次确认。

公式：`Vdrop = I × R`，`P = I² × R`。铜桥估算采用铜电阻率 1.724e-8 Ω·m、1 oz = 35 µm、宽 1.5 mm、长 1 mm；未包含蚀刻公差、温升、过孔、焊盘和邻接走线。

## 若后续采用首选 net-tie

这不是本次修改指令，而是进入 PCB 实现前必须满足的门禁：

1. 原理图中仍保留一个明确的两引脚桥接元件，后续再决定是否把 `R_PWR6` 改名为 `NT_PWR1`；不得无痕合并 `/VOUT` 与 `+5V`。
2. 创建自定义 2-pad net-tie footprint，pad 1/2 加入同一个 net-tie group；桥接铜连续、短、宽，不采用 KiCad 0.5 mm 小信号 net-tie。
3. 以 1 oz 外层铜为前提，桥及邻接路径起点宽度至少 1.5 mm，并优先接入铜皮；若出现过孔，单独核算数量、孔径和温升。
4. ERC/DRC、Gerber 视觉检查和嘉立创回导后，确认两个网络只在该 footprint 内短接。
5. 首板用可调负载从小电流升到 2.1 A，测桥两端压降和温升；没有该实测前不得标记生产就绪。

## 若后续采用备选 C4103112

1. footprint 需按厂家 2512 推荐焊盘核对，不只按名称匹配。
2. BOM 必须写 Manufacturer=`Stackpole Electronics`、MPN=`RMCF2512ZT0R00`、LCSC=`C4103112`，并注明非 Basic 候选。
3. 下单当日重新查库存、JLCPCB 可贴装面/封装映射和扩展料费。
4. 首板在 2.1 A 负载下测实际压降与温升；若压降明显影响 +5V 负载，回退到 net-tie 或采购真正的低毫欧高电流 jumper。

## 待验证

- `2.1 A` 是目标上限还是持续工作点，负载峰值和持续时间尚未锁定。
- 整条 `/VOUT -> +5V` 的走线、铜皮、过孔、连接器与 IP5306 热能力尚未复核。
- JLCPCB 官方装配页对 C4103112 的实时库存、扩展料费、可贴装面和替代料，必须在生成真实 BOM/CPL 后复核。
- net-tie 最终几何、板层铜厚、与外壳/其他器件的空间关系尚未设计。
- 首板压降、温升、启动浪涌和短路行为尚未实测。

## 状态

- 推荐：首选候选 = 自定义 PCB net-tie；备选候选 = C4103112。
- 置信度：拓扑建议 `medium-high`；C4103112 电气规格 `high`；实时库存/装配属性 `medium`。
- `selection_frozen=false`
- `schematic_modified=false`
- `pcb_modified=false`
- `production_ready=false`
