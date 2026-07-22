# Cadenza 1:1 实物对位页

这些资料用于候选连接器和已购原型器件的**断电实物门禁**，不是连接器选型冻结，也不是生产批准。

## 文件

- `fit-sheet-fh12_bottom-a4.svg`：FH12 bottom-contact 候选，1:1 顶视图。
- `fit-sheet-fh12a_top-a4.svg`：FH12A top-contact 候选，1:1 顶视图。
- `fit-sheet-fh34srj_dual-a4.svg`：FH34SRJ dual-contact 候选，1:1 顶视图。
- `fit-sheet-caliper-record-a4.svg`：EC12、三颗 6x6 按键和 10P 转接板的卡尺记录页。
- [`output/pdf/cadenza-physical-fit-sheets-a4.pdf`](../../../../../output/pdf/cadenza-physical-fit-sheets-a4.pdf)：四页合并打印版 PDF。
- `fit-sheets-manifest.json`：源封装哈希、解析后的几何和输出文件哈希。
- `verification.json`：最近一次机器验证结果。
- `generate_fit_sheets.py` / `verify_fit_sheets.py`：生成器与独立验证器。

三个连接器的 1:1 图形直接解析自：

`hardware/cadenza/libraries/display-fpc-variants/Cadenza_Display_FPC_Variants.pretty/*.kicad_mod`

图中本体框来自 `F.Fab`，焊盘来自 1–10 号 SMD pad 和两个 `MP` pad。红色是连接器端子的 Pad 1。4X 区只帮助阅读，明确不能拿来对位或测量。

## 打印方法

1. 建议打印合并 PDF；打印缩放选择 **100% / Actual Size / 实际大小**。
2. 必须关闭 **Fit to Page / 适合页面 / Shrink / 缩小超大页面**。
3. 先用卡尺或可靠直尺测纸上的 50.0 mm、10.0 mm 线段和 50 x 10 mm 方框。
4. 任一校准尺寸不正确就停止，不要继续拿连接器比对。
5. 校准正确后，才把**未焊接、未通电**的连接器放在 1:1 区比对本体、信号焊盘和固定焊片。
6. 把结果写在纸面并拍照，照片与卡尺记录一起回传。

浏览器或预览软件直接打印 SVG 时也必须选 100%；不同软件容易默认为“适合页面”，所以 PDF 更稳妥。

## 不能由打印页证明的事情

- 打印页不能证明 Sharp 屏幕 FPC 的 Pin 1 对应转接板哪一个孔。
- 打印页不能证明转接板插座是上接点还是下接点。
- 双面接触连接器也不会自动解决 Pin 1 镜像。
- 必须继续执行 `../MEASUREMENT_SESSION.md` 中的全程断电蜂鸣档通断测量。
- 连接器型号已冻结为双接触 Hirose `FH34SRJ-10S-0.5SH(50)`；在真实 FPC Pin 1、自然出线方向及 1:1 实物贴合均有证据前，**PCB 旋转角度和外壳 FPC 折弯路径保持未冻结**。

## 重新生成与验证

使用 Codex bundled PDF runtime：

```sh
PDF_PY=/Users/tapir/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/bin/python3
"$PDF_PY" hardware/cadenza/validation/physical-evidence/fit-sheets/generate_fit_sheets.py
"$PDF_PY" hardware/cadenza/validation/physical-evidence/fit-sheets/verify_fit_sheets.py
```

如果本机 `python3` 已安装 `reportlab` 和 `pypdf`，也可直接替换 `PDF_PY`。生成器不写入或修改任何 KiCad/OpenSpec 文件。
