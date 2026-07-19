## ADDED Requirements

### Requirement: SIGHT 提供三档固定练习
系统 SHALL 提供内置 `SIGHT` App：默认 `LEVEL 1` 使用 grand staff 与 C3–C5 含端点的 natural 单音，`ALL NOTES` 使用 grand staff 与 C2–C6 含端点的 natural 单音，`COMMON CHORDS` 使用 canonical spelling 的十二 root major/minor 原位三和弦；首版 SHALL NOT 生成评分、持久化、语音识别、节奏、调号、转位或七和弦。

#### Scenario: 首次进入 SIGHT
- **WHEN** 新会话从 Launcher 打开 SIGHT
- **THEN** Level picker 默认选中 C3–C5 natural 的 `LEVEL 1`

#### Scenario: 选择 All Notes
- **WHEN** 用户在 Level picker 选择 `ALL NOTES` 并开始
- **THEN** 每题只来自 C2–C6 含端点的自然单音集合

#### Scenario: 选择 Common Chords
- **WHEN** 用户选择 `COMMON CHORDS`
- **THEN** 每题是 canonical root spelling 的 major 或 minor 原位三和弦，必要时显示 sharp 或 flat

### Requirement: 题目使用拼写音高和固定唱名
系统 SHALL 以 step、alter、octave 保存题目，SHALL 以 C4 作为中央 C，SHALL 从拼写音高分别计算 MIDI 播放音高、谱面位置、字母音名和固定唱名，且 MUST NOT 因 enharmonic MIDI number 相同而丢失 accidental spelling。

#### Scenario: 揭晓中央 C
- **WHEN** 当前题为中央 C
- **THEN** 音符位于 grand staff 中央加线，答案显示 `C4` 与 `DO`，播放 MIDI note 60

#### Scenario: 揭晓带升号和弦
- **WHEN** 当前题为 D major 原位三和弦
- **THEN** 谱面和答案保留 `D F# A` 拼写，播放对应三个 MIDI note

### Requirement: Shuffle bag 均衡且无历史依赖
每个 Level SHALL 使用固定容量、确定性 seed 的 shuffle bag，使一轮内每个候选恰好出现一次，并 SHALL 在可能时避免相邻轮边界重复；App SHALL NOT 读取或写入跨会话学习历史。

#### Scenario: 完成一轮单音
- **WHEN** LEVEL 1 连续生成完整一轮题目
- **THEN** C3–C5 的每个自然音恰好出现一次且顺序由 seed 确定

#### Scenario: 重新进入 App
- **WHEN** 用户离开并在新会话重新进入 SIGHT
- **THEN** App 不依赖上一会话的答题、Replay 或 Level 历史

### Requirement: Reveal 与答案操作需要显式确认
Question 状态 SHALL 忽略 turn 导航并仅由 click 揭晓且播放；Answer 状态 SHALL 显示 `REPLAY / NEXT / LEVEL`、默认选择 `NEXT`，turn SHALL 只移动选择且 MUST NOT 执行动作，click SHALL 执行当前选择。Replay SHALL 重播当前题并保持 Replay 选中，Next SHALL 生成下一题，Level SHALL 返回 Level picker；long press SHALL 继续由系统菜单拥有。

#### Scenario: 题目页误转旋钮
- **WHEN** Question 状态收到任意 turn 且没有 click
- **THEN** 当前题、Level 与 Reveal 状态保持不变

#### Scenario: 揭晓后的默认动作
- **WHEN** 用户 click 揭晓题目
- **THEN** 答案出现、音高播放一次且 action focus 为 `NEXT`

#### Scenario: 旋钮只改变答案操作焦点
- **WHEN** Answer 状态从 `NEXT` 转到 `REPLAY`
- **THEN** 当前题和音频不改变，只有焦点改变，直到 click 才重播

#### Scenario: 连续 Replay
- **WHEN** 用户选择 `REPLAY` 并连续 click
- **THEN** 每次 click 都从头重播同一题且焦点保持 `REPLAY`

#### Scenario: 返回 Level picker
- **WHEN** 用户选择 `LEVEL` 并 click
- **THEN** App 返回 Level picker 且不把旋钮移动本身当作确认

### Requirement: 双 profile 最小记谱保持可读和有界
SIGHT SHALL 在 320×170 与 400×240 使用五线、谱号、notehead、stem、ledger line 和必要 accidental 表达当前题，Question SHALL 让谱面成为主焦点，Answer SHALL 保留同一题并显示音名/固定唱名或 chord 名/组成音。所有绘制 SHALL 保持在 framebuffer 和分配 bounds 内、无运行时缩放、文件加载、heap 或额外 framebuffer。

#### Scenario: 双 profile 中央 C
- **WHEN** C4 Question 与 Answer 分别在两个 profile 渲染
- **THEN** 中央加线、notehead、答案与 action strip 清楚且无 invalid/clipped geometry diagnostic

#### Scenario: All Notes 极端端点
- **WHEN** C2 或 C6 在两个 profile 渲染
- **THEN** 所需 ledger line 完整、notehead 不越界且答案区域不覆盖关键谱面

#### Scenario: accidental chord
- **WHEN** 含 sharp 或 flat 的三和弦在两个 profile 渲染
- **THEN** accidental、相邻 notehead、stem 与答案可区分且无 bounds 外写入

### Requirement: SIGHT 位于 Timer 后并具有静态 Cover
系统 SHALL 使用独立稳定 AppId 注册 SIGHT，并 SHALL 在所有内置 host 的 Launcher 顺序中放在 Timer 后、Motion 前。SIGHT Cover SHALL 遵守现有静态、双 profile、等尺寸 blit、批准与 handoff 契约；未获用户明确批准时 SHALL 使用安全 fallback 且候选只存在 review 目录。

#### Scenario: 浏览 Launcher 顺序
- **WHEN** 用户从 Timer 向前导航一个 App
- **THEN** Launcher 选择 SIGHT，再向前选择 Motion

#### Scenario: Cover 尚未批准
- **WHEN** SIGHT pixel candidate 尚无用户明确批准
- **THEN** Runtime 使用确定 fallback，正式 source/PBM/header/golden 未被候选覆盖

#### Scenario: 打开与返回 SIGHT
- **WHEN** Launcher 打开 SIGHT 后再返回 Home
- **THEN** staged handoff 完成、静态 Cover 或 fallback identity 保持有界且输入不会泄漏到 App
