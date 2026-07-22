# Cadenza 相对参考板的 BOM 增删候选

状态：**规划用，未分配最终位号，不能上传嘉立创下单。**  
机器表：`bom-delta-candidate.csv`。

这张表只回答“参考板变成目标 L1/L2 后，物料范围怎样变化”。最终生产 BOM 仍必须
从完成后的 KiCad 原理图导出，并与 CPL、PCB 位号和嘉立创匹配结果交叉核对。

## L1 显示

- 删除旧 12P TFT FPC、3V3LCD LDO/两颗旧电容和背光三极管/两颗电阻。
- 新增 Sharp 10P 0.5 mm FPC、DISP 0.1 µF、VDDA 0.1 µF、VDD 1 µF。
- FPC 料号必须等实物触点方向确认。显示电容已有 0805 Basic 候选：100 nF
  `CC0805KRX7R9BB104 / C49678`、1 µF `CL21B105KBFNNNE / C28323`；它们仍须
  在最终 KiCad 中按 Sharp 引脚就近放置，不能只核对 BOM 数值。

## Power/Lock

- 新增独立瞬时开关、BAT54C 双肖特基隔离、强制关机 NMOS、门极/感知电阻、
  默认断开的旁路焊桥和测试点。
- `BAT54C C916424`、`AO3400A C20917` 是当前候选，不是已冻结；KEY 肖特基压降、
  USB 插入和长按行为必须在首板实测。
- `R_PWR6` 已采用 KiCad 标准 `NetTie-2_SMD_Pad2.0mm` 作为 PCB 铜桥候选，并排除
  BOM/CPL。其 4 × 2 mm、1 oz 名义估算在 2.1 A 下约 0.985 mΩ、2.07 mV、4.34 mW；
  这不包含正式布局中的颈缩、热升和铜厚公差，仍不是冻结结论。
- 现有外壳候选是从边缘向内按，因此开关主候选为侧按
  `GT-TC020A-H035-L1 / C17179533`；`TS24CA 250gf 013 / C5373430` 是高库存侧按
  替代。二者都为 Extended 且 footprint 不兼容，样品与推杆 fit-check 通过前不冻结。

## L2 输入与原生 USB

- 删除摇杆、CH340C、自动下载双晶体管及其 DTR/RTS 路线；删除哪些旧按键须等
  B/Menu 最终复用策略确认。
- 新增 EC12、B/Menu、USB ESD、两颗 USB 串阻、VBUS 检测分压、恢复焊盘和
  USB shield 调试网络。
- USB 22 Ω 串阻采用 `0805W8F220JT5E / C17561` 候选。VBUS 上臂优先使用两颗
  Basic `150 kΩ / C17470` 并联成 75 kΩ；空间不足时才使用单颗 Extended
  `75 kΩ / C17819`。两种方案必须在原理图中互斥。
- EC12 与 6×6 按键大概率需要通孔/手工后焊；在目标料号和嘉立创装配能力确认前，
  不把它们计入“可全自动 PCBA”。
- 五路输入各预留 10 kΩ 外上拉、1 kΩ 接点串阻和可选 RC 电容；39 nF 只是 SPICE
  探索值，真实 EC12 脉宽测量前默认允许 DNP，不能因仿真通过而冻结。

## 当前缺口

1. FPC 顶接/底接方向与最终 MPN。
2. EC12 精确型号、轴型、轴长、固定脚与采购号。
3. B/Menu 与 Power/Lock 是否共用一种开关，还是正面/侧边采用不同操作方向。
4. USB-C 连接器现有 C49261586/C49261588 资料冲突。
5. 原生 USB ESD、VBUS sense 与 IP5306 输入限流的最终数据手册审查。
6. 下单时重新确认 `sourcing/new-passives/` 中候选的 JLCPCB 库属性和库存。
7. L2 五颗可选滤波电容的最终值与 MPN；当前 39 nF 库料均非 Basic，且数值仍待
   EC12 实物波形决定。

这些缺口解决后，才把候选表写回 KiCad 的 `MPN`、`Manufacturer`、`LCSC` 和
`BOM Comments` 字段。
