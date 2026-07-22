# LS027B7DH01 显示 FPC 连接器候选

调查日期：2026-07-22（Asia/Shanghai）  
范围：Sharp `LS027B7DH01`，10 pin、0.5 mm pitch、加强板处名义厚度 0.30 mm。  
状态：**L1 主料已选定；PCB 旋转角度、Pin 1 实物方向和 FPC 最终折弯路径仍待验证。**

## 结论先行

L1 主料选定 Hirose `FH34SRJ-10S-0.5SH(50)`（LCSC/JLCPCB `C324723`）：它是 10P、0.5 mm、0.30 mm FPC、1.0 mm 高的背翻盖连接器，制造商明确说明同一个连接器可用于上触点和下触点 FPC。它能避免在实物方向尚未确认时过早押注。2026-07-22 10:06 CST 的 LCSC 官方页显示 6,625 件现货，JLCPCB 标为 Extended Part 且支持 SMT Assembly；库存是瞬时值。

但这**不等于可以跳过实物确认**：必须确认屏幕 Pin 1、铜面、FPC 弯折方向和连接器在 PCB 上的旋转角度。双面接触只能消除“铜面朝上还是朝下”的一部分风险，不能消除 Pin 1 镜像和外壳干涉风险。

若实物确认后改用更传统的 2.0 mm 前翻盖连接器：

- bottom-contact 质量优先：Hirose `FH12-10S-0.5SH(55)` / `C506791`。
- top-contact 质量优先：Hirose `FH12A-10S-0.5SH(55)` / `C3168239`。
- bottom-contact 低成本替代：JUSHUO `AFC01-S10FCA-00` / `C262659`。
- top-contact 低成本替代：Hong Cheng `HC-FPC-0.5-10P-CSH20` / `C19273947`。

其余型号保留为替代研究，不写入 L1 主 BOM。J_DISP1 已在 L1 原理图中绑定 `C324723` 及项目自建 footprint；PCB 尚未同步。

## Sharp 原厂兼容基准

Sharp 数据手册 LCY-1210401A 第 23 页明确列出：

- top-contact：SMK `CFP-4510-0150F`。
- bottom-contact：SMK `CFP-4610-0150F`、Molex `51441-1093`。

其中 Molex `51441-1093` 已被 Mouser 标为 Obsolete，不适合作为新设计的供应链主料。这里保留这些型号的意义是建立原厂兼容基准，而不是建议采购。Sharp 同页还给出 FPC 弯折区和最小弯折半径；屏幕实物方向仍为待验证。

置信度：**高**（Sharp 原厂数据手册）。

## 候选对比

|角色|器件 / C号|触点与锁扣|电气与温度|名义外形|2026-07-22 库存 / 库别|KiCad 与 3D 状态|结论|
|---|---|---|---|---|---|---|---|
|L1 主料|Hirose `FH34SRJ-10S-0.5SH(50)` / `C324723`|上下双接触、背翻盖，10P，0.5 mm，FPC 0.30 mm|0.5 A，50 Vrms AC/DC，-55…+105 ℃|约 7.0 × 3.8 × 1.0 mm|LCSC 6,625；Extended；JLC SMT|项目 footprint 已按 Hirose 厂图与 EasyEDA land pattern 交叉核对；3D 为公开尺寸建立的保守代理|已绑定原理图；仍须冻结 PCB 旋转、Pin 1 和锁扣空间|
|bottom 主候选|Hirose `FH12-10S-0.5SH(55)` / `C506791`|底接、旋转式前翻盖 ZIF，10P，0.5 mm，FPC 0.30±0.05 mm|0.5 A DC，50 V AC，-40…+85 ℃|本体约 8.1 × 5.6 × 2.0 mm|LCSC 1,465；Extended|KiCad 标准 footprint 和 STEP 均存在|实物确认底接后优先；唯一不需要从零建 KiCad 3D 的方向专用候选|
|top 替代|Hirose `FH12A-10S-0.5SH(55)` / 当前活跃映射 `C5139870`|顶接、旋转式前翻盖 ZIF，10P，0.5 mm，FPC 0.30±0.05 mm|0.5 A DC，50 V AC，-40…+85 ℃|约 9.1 × 6.2 × 2.0 mm|JLCPCB Extended、SMT；查询库存约 50；旧 `C3168239` 映射异常，不冻结|项目有已复核 footprint 和保守代理|仅在方向确认且主料不可用时评估；下单前重新查 C 号|
|bottom 替代|JUSHUO `AFC01-S10FCA-00` / `C262659`|底接、翻盖，10P，0.5 mm，FPC 0.30 mm|0.5 A，50 V，-45…+85 ℃|约 9.4 × 6.1 × 2.0 mm|JLCPCB 109,767；Extended|KiCad 标准库无；EasyEDA 有 footprint，3D 未确认|库存强、便宜；需按厂图自建/导入后逐尺寸复核|
|top 替代|Hong Cheng `HC-FPC-0.5-10P-CSH20` / `C19273947`|顶接、抽拉/滑锁，10P，0.5 mm，FPC 0.30 mm|50 V AC/DC，-25…+85 ℃；厂图未声明额定电流|约 10.6 × 5.2 × 2.0 mm|LCSC 1,650；Extended|KiCad 标准库无；EasyEDA 有 footprint，3D 未确认|可采购的 top 替代；额定电流资料不完整，量产冻结前需补正式规格书|

库存是瞬时数据，不能当作下单日保证；BOM 冻结和下单前必须重新查询。

## 为什么 L1 选择 `FH34SRJ`

优点：

- Hirose 2024 数据手册直接写明“both upper and lower contacts”，不是经销商标题推断。
- 10P、0.5 mm、0.30±0.03 mm FPC，与 Sharp 的机械接口相符。
- 1.0 mm 高、约 7.0 × 3.8 mm，占板面积明显小于 FH12/FH12A。
- LCSC/JLCPCB 库存明显好于方向专用的 top-contact Hirose 映射。

需要验证：

- 它是背翻盖，开盖活动空间与参考外壳、后压框、FPC 弯折 keepout 必须重新 fit-check。
- 更低的 1.0 mm 高度会改变 FPC 的 Z 高度和弯折路径。
- 双面接触不会自动纠正 Pin 1 镜像；PCB footprint 的 Pin 1 与屏幕 Pin 1 仍需万用表/实物照片确认。
- KiCad 标准库没有现成 footprint 和 STEP，必须建模并按制造商 land pattern 独立复核。

因此它被锁定为 L1 的连接器型号和 footprint，但不代表整板已经生产冻结；Pin 1、PCB 旋转和插拔空间仍要完成实物门禁。

## KiCad footprint 与 3D 处理

本机 KiCad `10.0.4` 标准库核查结果：

- `FH12-10S-0.5SH(55)` bottom-contact：
  - footprint：`Connector_FFC-FPC:Hirose_FH12-10S-0.5SH_1x10-1MP_P0.50mm_Horizontal`
  - STEP：`Connector_FFC-FPC.3dshapes/Hirose_FH12-10S-0.5SH_1x10-1MP_P0.50mm_Horizontal.step`
  - 使用前仍要把 footprint 与当前制造商图纸逐项比较，不能因为“标准库有”就跳过复核。
- `FH12A-10S-0.5SH(55)`、`FH34SRJ-10S-0.5SH(50)`、`AFC01-S10FCA-00`、`HC-FPC-0.5-10P-CSH20`：标准库没有精确匹配的 footprint 或 STEP。

人工工作项：

1. 依据厂图创建/导入焊盘、固定焊片、Pin 1 标记和开盖 keepout。
2. 至少由另一套来源（厂图与 EasyEDA land pattern）交叉检查焊盘中心、总宽、深度和固定脚。
3. 使用带明确 `PROXY` 标记的保守包络 STEP，并用厂图尺寸和独立 keepout 做碰撞门禁。Hirose 官网 STEP 的随附条款禁止分发、修改和商业使用，未纳入项目；不能把代理模型冒充精确模型。
4. 将连接器开盖、FPC 插拔直线空间和弯折半径加入外壳碰撞门禁。
5. footprint 通过 1:1 PDF 打印或实物贴合检查后，才能进入首板。

## 产品与数据手册链接

- Hirose FH12 bottom：<https://www.lcsc.com/product-detail/C506791.html>
- Hirose FH12A top：<https://jlcpcb.com/partdetail/HRS_Hirose-FH12A_10S_0_5SH_55/C3168239>
- Hirose FH34SRJ 双面接触：<https://www.lcsc.com/product-detail/C324723.html>
- JUSHUO bottom：<https://jlcpcb.com/partdetail/Jushuo-AFC01_S10FCA00/C262659>
- Hong Cheng top：<https://www.lcsc.com/product-detail/C19273947.html>
- Hirose FH12 系列 PDF：<https://datasheet.lcsc.com/datasheet/pdf/494542b19b75b8f334b8320444d3cc2f.pdf?productCode=C506794>
- Hirose FH34 系列 PDF：<https://datasheet.lcsc.com/datasheet/pdf/cc5093fb22bd0acee55360f91c00b2ac.pdf?productCode=C3167891>
- JUSHUO PDF：<https://jlcpcb.com/api/file/downloadByFileSystemAccessId/8588893997838712832>
- Hong Cheng PDF：<https://datasheet.lcsc.com/datasheet/pdf/eefad330c23ff0c4b9f49a452cf9927a.pdf?productCode=C19273947>
- Sharp 原厂屏幕数据手册缓存：`hardware/reference/oshwhub-project_jofcnupz/datasheets/LS027B7DH01_Sharp_LCY-1210401A.pdf`

本目录已缓存上述四份连接器制造商 PDF；文件哈希见 `candidates.json`。

## 冻结前的实物确认清单

- [ ] 拍屏幕 FPC 铜面正反两面清晰照片，并让 Pin 1 标识同框。
- [ ] 用万用表确认已购转接板的 Pin 1、10 根线是否 1:1、插座为上接还是下接。
- [ ] 测 FPC 加强板厚度，目标应接近 0.30 mm；若明显不同，停止选型。
- [ ] 在屏幕正面朝向已知的情况下，确认 FPC 允许的自然弯折方向，禁止向偏光片侧反折。
- [ ] 用纸样或打印 footprint 验证连接器方向、Pin 1 和插拔空间。
- [ ] BOM 锁定当天重新查 C号、库存、Extended 费用和可装配状态。

## 置信度与待验证项

- **高**：Sharp 推荐连接器方向基准；Hirose FH12/FH34 的 pin 数、pitch、厚度、电气额定、温度、锁扣形式和尺寸；本机 KiCad 标准库是否存在精确模型。
- **高**：2026-07-22 页面快照中的 C号、Basic/Extended 分类和当时显示库存。
- **中**：JUSHUO/Hong Cheng 的长期批次一致性；需要首板和来料实测。
- **低/待验证**：真实屏幕 FPC 铜面、Pin 1、出线方向；`C3168239` 在实际 BOM 上传时的即时可装数量；Hong Cheng 的额定电流；所有非 KiCad 标准库候选的精确 3D 可获得性。
