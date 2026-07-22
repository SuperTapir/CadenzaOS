# L1 前壳 fit-check

这是从冻结的 `顶盖V7.step` 生成的可回退机械样件，不是最终工业设计或生产外壳。

脚本只做四件事：

1. 保留参考顶盖的外轮廓、安装孔和原输入开孔；
2. 填平原 74.74 × 53.86 mm TFT 窗口；
3. 切出 60.0 × 36.48 mm Sharp 窗口，对 58.8 × 35.28 mm 可视区每边留 0.60 mm；
4. 输出独立的 0.40 mm 软垫代理和带 FPC 避让口的后压框 fit-check。

`generated/l1-front-screen-assembly.step` 把前壳、软垫、屏幕包络和压框放在共同 PCB 坐标中。所有零件同时输出 STEP/STL；参数写在 JSON 中。

Sharp 原厂规格书已确认屏幕局部事实：显示面朝上时 FPC 从 6 点钟边伸出，平直触点在背面，正视图 Pin 1 在右侧，只能向屏幕背面折。仍待验证的是屏幕在外壳中采用 0°/180°（映射 `+Y/-Y`）、已购转接板是否镜像、最终连接器/锁扣和精确折叠路线、压框固定方式、打印收缩、玻璃受力、外壳闭合。因而 OpenSpec 4.2/4.3 暂不标完成。

打印、壁厚、螺柱和输入件的证据分级风险见 `PRINT_ASSEMBLY_RISK_REGISTER.md`，机器可读结果见 `L1_PRINT_ASSEMBLY_RISKS.json`。该审计只读取已有 CAD/候选 JSON，不会选择 FPC 方向：

```sh
PYTHONPATH=/Users/tapir/.cache/cadenza-cad-tools-py313 \
  /opt/homebrew/bin/python3.13 \
  hardware/cadenza/mechanical/l1-fit-check/generate_l1_risk_register.py
```
