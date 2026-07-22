# L1 原理图隔离副本：编辑前基线证明

这里保存的是 **开始修改 L1 原理图之前** 的只读证据。它不是生产门禁，也不表示
该参考设计已经适合下单；它只回答一个问题：隔离副本在编辑前是否仍与已记录的
工作基线一致。

运行：

```bash
python3 hardware/cadenza/derived/l1-schematic-dryrun/baseline-proof/verify_pre_edit_baseline.py
```

验证器会检查：

- `SOURCE_MANIFEST.sha256` 恰有 39 项，且所有列出的来源文件 SHA-256 均匹配；
- 派生目录中的 `.kicad_sch` 与清单记录的 SHA-256 相同；
- `pre-edit.netlist.xml` 与 `hardware/cadenza/working-base/connectivity.netlist.xml`
  的元件引用、每个元件的 value/footprint、每条网络的端点集合语义相同；
- 基线恰有 77 个元件、102 条网络；
- ERC 恰有 691 项（18 error、673 warning），且各违规类型数量精确匹配；
- `pre-edit.pdf` 存在且非空；若系统提供 `pdfinfo`，还会要求页数大于零。

唯一输出文件是 [verification.json](./verification.json)。验证器不会启动 KiCad，也不会
修改原理图、`working-base`、golden 文件或 OpenSpec。`baseline-proof` 下新增本 README、
验证脚本和结果 JSON 是允许的；它们不属于来源清单，不能反向更新该清单来掩盖差异。

PDF 的逐页渲染人工检查另见 [VISUAL_INSPECTION.md](./VISUAL_INSPECTION.md)。机器页数
检查和人工画面检查用途不同，不能相互替代。
