## ADDED Requirements

### Requirement: Settings 列表在 overflow 时保持选中可见
Settings 主列表在行总高度超过标题与页脚之间的可视高度时 SHALL 滚动，使当前选中行始终完整落在可视窗口内；旋钮循环选择 SHALL 仍能到达每一项，包括最后一项。

#### Scenario: 320×170 到达最后一项
- **WHEN** 用户在 T-Embed profile 的 Settings 主列表连续转向最后一项
- **THEN** 最后一项完整可见且处于选中态，不被 canvas 底边或页脚裁切

#### Scenario: 选中上移时回滚
- **WHEN** 选中项从窗口底部之外的项转回靠近顶部的项
- **THEN** 滚动偏移缩小，使新的选中行完整可见，且不留下空白洞导致无法再看到顶部项

### Requirement: Settings 滚动不改变动作语义
Settings 的 turn/click 语义、capability 命令与 About 子页进入/返回 SHALL 与滚动无关；滚动仅影响呈现窗口。

#### Scenario: 末项仍打开 About
- **WHEN** 选中 ABOUT 行并短按
- **THEN** 进入 About 身份页，与是否发生过列表滚动无关
