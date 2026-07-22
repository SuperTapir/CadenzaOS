# Sharp 10P FPC 候选封装核对报告

状态：**L1 已选择 FH34SRJ 双接触型号和 footprint；PCB 旋转角度、Pin 1 实物方向与真实出线方向仍未冻结。**

## 核对结果

|候选|厂家图纸接触形式|Pad 1 方向（封装局部坐标）|信号焊盘|固定焊片|3D 状态|
|---|---|---|---|---|---|
|Hirose `FH12-10S-0.5SH(55)`|bottom contact|左端，`x=-2.25 mm`|10 × `0.30 × 1.30 mm`；0.50 mm pitch；中心 `y=-1.85 mm`|`x=±4.15, y=1.40 mm`；`1.80 × 2.20 mm`|映射 KiCad 10 精确型号 STEP|
|Hirose `FH12A-10S-0.5SH(55)`|top contact|左端，`x=-2.25 mm`|与 FH12 系列推荐 10P land table 相同|与 FH12 系列推荐 10P land table 相同|仅保守包络 `PROXY`，不是精确外形|
|Hirose `FH34SRJ-10S-0.5SH(50)`|top and bottom contact|右端，`x=+2.25 mm`；编号向左递增|10 × `0.30 × 0.80 mm`；0.50 mm pitch；中心 `y=0`|`x=±3.05, y=2.90 mm`；`0.80 × 0.80 mm`|仅保守包络 `PROXY`，不是精确外形|

## 证据链

- `Hirose_FH12_series_2020.pdf` PDF 第 4 页：`FH12-10S` 是 10P bottom-contact；本体 `B=8.1 mm`、`C=9.1 mm`，极性标志在左侧。
- 同 PDF 第 6 页：`FH12A-10S` 是 10P top-contact；本体 `A=9.1 mm`、深度 6.2 mm、高度 2.0 mm，极性标志在左侧。
- 同 PDF 第 10 页：0.5 mm top/bottom 共用推荐 land/mask 表；10P 的 `A=4.5 mm`、`B=10.1 mm`、`C=6.5 mm`，信号 land `0.30 mm`、固定焊片 land `1.80 × 2.20 mm`。
- `Hirose_FH34_series_2024.pdf` PDF 第 6 页：`FH34SRJ-10S` 的 `A=7.0 mm`、`B=4.5 mm`、`C=5.53 mm`、`D=6.38 mm`，图中明确标出 Contact No. 1 在右侧。
- 同 PDF 第 8 页：推荐 PCB pattern 明确给出信号 land `0.30 × 0.80 mm`、固定焊片 `0.80 × 0.80 mm`，10P 表中 `E=6.1 mm`、`F=6.9 mm`。
- 同 PDF 第 8 页 FPC 图：top-contact 的 Contact No. 1 在右侧，bottom-contact 的 Contact No. 1 在左侧。这是同一连接器两种 FPC 铜面视图，不是说 PCB 焊盘编号可以镜像。

PDF 均来自 `hardware/cadenza/sourcing/display-fpc/datasheets/`，其 SHA-256 已记录在上游 `candidates.json`。

## Pin 1 语义

封装的焊盘编号描述的是**连接器端子号**，不会因屏幕 FPC 朝上或朝下而自动翻转。尤其是 FH34SRJ：双面接触只解决铜面兼容，不解决 Pin 1 镜像。最终板上旋转角度必须用真实屏幕、转接板通断测量和 1:1 打印共同确认。

## 3D 边界

- FH12 bottom 使用本机 KiCad 10.0.4 标准模型；其文件名精确对应 `FH12-10S-0.5SH`。
- Hirose 官方网页提供 FH34SRJ/FH12A 简化 STEP，但下载包条款禁止分发、修改和商业使用，因此不纳入本项目。库内 STEP 是依据公开厂图尺寸建立的保守包络，文件名包含 `PROXY`；只可用于粗略碰撞检查。
- 在 KiCad 路径配置中把 `CADENZA_DISPLAY_FPC_3D_DIR` 指向本目录下的 `3dmodels/`，代理才能显示。
- 锁扣打开后的运动包络、FPC 插拔空间和弯折半径未包含在代理 STEP 内，应继续使用外壳 fit-check 的独立 keepout。

## 进入正式 PCB 前仍必须完成

1. 全程断电确认真实 Sharp FPC 的 Pin 1、铜面、加强板厚度和自然出线方向。
2. 打印候选 footprint 为 1:1，拿实物连接器贴合信号焊盘及固定焊片。
3. 生产资料明确把 `PROXY` 当保守包络，并用 2D 厂图、开盖 keepout 和 1:1 实物贴合复核；除非另行取得允许商业使用的精确 CAD 授权，不得把 Hirose 官网受限 STEP 纳入工程。
4. BOM 冻结当日重新确认 C 号、库存和嘉立创可贴装状态。

结论：L1 已选择 FH34SRJ 的型号与 footprint；其 PCB 旋转和 Pin 1 仍未达到直接下单的证据门槛。

## 自动门禁实测

- `verify_library.py`：`PASS`，3 个候选、每个恰有 1–10 号信号焊盘，Pad 1 端点、Pitch、焊盘/固定片尺寸及 3D 路径均与 `footprint-specs.json` 一致。
- KiCad 10.0.4 `fp upgrade --force`：三份 `.kicad_mod` 全部成功解析到临时输出目录；候选源文件未被改写。
- KiCad 10.0.4 `fp export svg`：三份封装均成功输出 F.Cu/F.SilkS/F.Fab/F.CrtYd 视图。
- 两个 `PROXY.step`：OpenCascade 读取成功，各 1 个根 shape，均非空。
