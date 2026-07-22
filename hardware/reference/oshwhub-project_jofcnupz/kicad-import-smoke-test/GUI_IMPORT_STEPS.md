# KiCad 完整工程导入步骤

完整原理图、项目库和 3D 模型只能从 KiCad 项目管理器的 GUI 工程导入入口生成。请在已经打开的 KiCad 10.0.4 主窗口执行：

1. 选择 `文件` → `导入非 KiCad 工程` → `EasyEDA Pro 工程`。
2. 选择只读原件：
   `/Users/tapir/Development/CadenzaOS/hardware/reference/oshwhub-project_jofcnupz/Cadenza-reference-ESP32-S3-game-console-original-v2-20260722.epro`
3. 保存目录选择：
   `/Users/tapir/Development/CadenzaOS/hardware/reference/oshwhub-project_jofcnupz/kicad-import-smoke-test/gui-full-import`
4. 如果询问工程/原理图/PCB 选择，选择项目中唯一的主原理图和主 PCB；不要覆盖 `reference-imported.kicad_pcb`。
5. 导入完成后先不要移动、删除、重铺铜或更新封装，直接关闭保存提示以外的设置对话框。

导入完成的最低文件证据：

- `.kicad_pro`
- `.kicad_sch`
- `.kicad_pcb`
- 项目符号库及 `sym-lib-table`
- 项目封装库及 `fp-lib-table`
- 若导入器支持，`EASYEDA_MODELS/` 3D 模型目录

这些文件出现后即可由自动脚本完成原理图/PCB/库/3D 的第二轮差异审计。
