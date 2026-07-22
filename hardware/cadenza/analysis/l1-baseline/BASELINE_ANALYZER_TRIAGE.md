# L1 参考基线分析器告警分诊

日期：2026-07-22  
范围：参考 KiCad 导入基线；不是最终 Cadenza 派生板设计审查。

## 结论

`schematic.json` 的 5 条 `error` 不能直接解释为“参考板有 5 个电气错误”：

| 规则 | 数量 | 分诊 | 依据 |
|---|---:|---|---|
| `DS-001` | 1 | 生产资料缺口，不是电路失效证据 | KiCad 导入文件没有项目级 `datasheets/` 缓存；关键数据手册另存在参考证据目录。最终派生板仍须建立完整数据手册清单。 |
| `SS-001` | 1 | 最终下单前阻断；不否定参考样机 | 导入符号的 MPN 字段覆盖率为 0，但参考评估已另行建立关键 BOM/库存快照。最终 BOM 必须把精确 MPN/LCSC 写回生产资料。 |
| `VM-001` | 3 | 分析器误报 | GPIO40/41/42 是 ESP32-S3 的 3.3 V I2S 输出，连接到 5 V 供电的 MAX98357A 数字输入。MAX98357A 的数字音频输入 `VIH` 最低仅 1.3 V，`VIL` 最高 0.6 V；3.3 V 高电平满足要求。其 VDD、LRCLK、BCLK、DIN 对地绝对最大范围为 -0.3–6 V。 |

因此目前没有从这 5 条自动告警中得到新的“适配阻断”。`DS-001` 和 `SS-001`
仍是最终生产资料门禁，不能因为参考样机能运行就忽略；三条 `VM-001` 则不应要求
增加 I2S 电平转换器。

## 可复核证据

- 自动分析输入：`schematic.json`。
- 参考关键 BOM：
  `hardware/reference/oshwhub-project_jofcnupz/analysis/CRITICAL_BOM_SNAPSHOT.md`。
- MAX98357A/MAX98357B 原厂数据手册 Rev.16：
  <https://www.analog.com/media/en/technical-documentation/data-sheets/MAX98357A-MAX98357B.pdf>
  - PDF 第 4 页（页面索引 P3）：VDD、LRCLK、BCLK、DIN 对地绝对最大 -0.3–6 V；VDD 工作范围 2.5–5.5 V。
  - PDF 第 6–7 页（页面索引 P5–P6）：数字音频输入 `VIH` 最低 1.3 V、`VIL` 最高 0.6 V。

## 仍未覆盖

- 这不是 MAX98357A 完整热、功率或 4 Ω 扬声器审查。
- 最终派生 PCB 完成后，仍需重新运行原理图、PCB、交叉、热、SPICE 和 EMC 分析。
- 最终 BOM 的 MPN、LCSC 料号、封装、极性与 CPL 旋转必须在嘉立创预览中复核。
