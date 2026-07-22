# L1 试打印包

运行：

```sh
PYTHONPATH=/Users/tapir/.cache/cadenza-cad-tools-py313 \
  /opt/homebrew/bin/python3.13 \
  hardware/cadenza/mechanical/l1-fit-check/build_l1_print_package.py
```

输出目录为 `generated/print-package/`，包含：

- `Cadenza-L1-front-shell.stl`：只修改 Sharp 窗口和 FPC 过线槽的前壳；
- `Cadenza-L1-bottom-shell.stl`：未修改的参考 V7 底盒；
- `Cadenza-L1-screen-retainer.stl`：1 mm 松配屏幕后框试件；
- `print-package-audit.json`：三件 STL 的三角形、包围盒、体积和水密边检查。

第一轮只建议先打印前壳和后框，用可移除双面胶做屏幕试装。确认玻璃不受力、窗口无遮挡、
FPC 可以自然折入连接器且锁扣可以操作后，再打印底盒。这里的 `PASS_TRIAL_PRINT` 只表示
STL 文件完整且网格检查通过，不代表真实屏幕/FPC 已验证。
