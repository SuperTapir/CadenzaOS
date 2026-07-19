## Context

Runtime 已有固定四 voice `AudioEngine`、16 项 SPSC audio command queue、session volume、SDL/I²S/headless 消费者和 SoundPlay capability，但公开命令只承载 `SoundCue`。`MonoCanvas` 已有线、圆、bitmap、bounded text 与双 framebuffer profile；Launcher 按 App 注册顺序呈现，所以 SIGHT 可使用新 AppId、在 Timer 后注册而不重编号现有 App。

调研记录见 `docs/sight-reading-reference-research.md`。MusicXML 4.0、Playdate SDK 3.1.0、VexFlow 4.2.6、Bravura `02e8ed2` 与 ArduboyTones 1.0.3 的关键语义、许可证、源码和拒绝边界已经锁定。当前引擎 probe 已证明 C2–C6 与三音和弦落在频率/voice/peak 安全边界，但实体扬声器听感仍需实机验收。

## Goals / Non-Goals

**Goals:**

- 交付可每日使用的三档 SIGHT 识谱短循环，并放在 Launcher 的 Timer 后。
- 保留拼写音高与播放音高差异，正确显示自然音和常用和弦 accidental。
- 新增最小、固定容量、capability-aware 的 MusicalNoteSet，不泄漏内部 synth。
- 在 320×170 与 400×240 上保持大谱表、答案与安全确认输入清晰。
- 用确定性数据、PCM、snapshot、headless、firmware 与 size 门禁闭环。

**Non-Goals:**

- 不做语音/音高识别、评分、Anki、持久化、键盘图、节奏、调号、转位、七和弦、MIDI input、导入乐谱、sample piano、sequence 或背景音乐。
- 不提供任意 Hz、ToneSpec、instrument、note-off、tempo、channel 或文件 API。
- 不把完整 Bravura/VexFlow 带入 runtime，也不在未获批准前写正式 Cover。

## Decisions

### 1. 拼写音高与播放音高分层

`SpelledPitch` 保存 `step + accidental + octave`，中央 C 为 C4；数据层按 MusicXML 语义生成答案、固定唱名、staff position 与 canonical chord spelling，再转换为 MIDI note。替代方案是只保存 MIDI number，但会把 F# 与 Gb 合并，无法正确揭晓和弦，因此拒绝。

`LEVEL 1` 固定 C3–C5 natural；`ALL NOTES` 固定 C2–C6 natural；`COMMON CHORDS` 使用十二个 canonical root 的 major/minor 原位三和弦。单音 shuffle bag 每轮全覆盖并避免跨轮直接重复；和弦同样使用固定容量 bag，不保存历史。

### 2. MusicalNoteSet 是局部音乐命令，不是通用 synth

新增 `MusicalNoteSet`，保存 1–4 个 MIDI note，合法范围 21–108。`SystemCommand::playMusicalNotes()` 仍要求 SoundPlay capability；SystemServiceHost 验证 count/range 后交给 InteractionSoundService。App 无法控制音色、时值、gain 或 priority。

AudioCommand 增加 `PlayNotes` payload。它与 Navigate/Boundary 一样受普通水位约束，快速 Replay 不得占用四项关键 reserve。消费者收到新 note set 时先停止旧 voice/延迟 event，再以固定约 700 ms 的指数衰减 sine + second harmonic 同时提交 1–4 音；这使 Replay 明确从头开始。Muted 与 StopAll 的原子安全邮箱继续具有最终优先级。

替代方案包括为每音扩展 SoundCue、公开 Hz 或 ToneSpec；前者枚举爆炸，后两者把内部参数变成长约束并允许绕过平台音色/资源策略，均拒绝。

### 3. 最小整数记谱器

SIGHT 不引入排版树。自然音以 diatonic index 映射到半个 staff space，clef 使用固定 shift；超出五线谱的线位逐条画 ledger line。notehead/stem、staff、brace/action strip 用 MonoCanvas 原语；谱号与 accidental 由锁定 Bravura 原版离线光栅化为小型 1-bit bitmap，并记录来源/OFL。

Question 保持大谱表居中。Reveal 后谱表向左让位，右侧显示音名/固定唱名或 chord 名/组成音；底部 action strip 为 Replay/Next/Level。两 profile 共享语义布局，以 profile 尺寸选择整数 staff space 与 bitmap，不做运行时缩放或 heap。

### 4. 输入状态机以确认消除旋钮误触

Level picker 中 turn 选择 Level、click 开始。Question 中 turn 不执行导航，click Reveal 并播放。Answer 中默认选中 Next，turn 只移动 Replay/Next/Level 焦点，click 执行；Replay 后焦点留在 Replay，Next 生成新题，Level 返回 picker。长按仍由 system surface 截获。

不采用“旋转立即换题”，因为硬件旋钮易误触且会不可恢复地丢失当前题；不采用 double-click/中按时长，因为会引入延迟并与 650 ms system long-press 竞争。

### 5. App 身份、Cover 与生命周期

新增 `kSightAppId=0x0105`，保留 0x0102–0x0104；所有 host 在 Timer 后、Motion 前注册 SIGHT。SIGHT 不持有动态资源，onEnter 回到 Level picker 的稳定默认 Level，onExit 无持久化。

Cover 使用 `SIGHT` 精确标题、一个被单一 notehead 穿过/定位的夸张五线 motif 和宽阔 quiet field。候选只写入 `review/build-sight-reading-trainer/sight/`；用户明确批准双 profile pixel candidate 后，才进入 source/PBM/header/snapshot。

### 6. 测试先固定失败边界

数据测试先固定 C4=60、端点、拼写、固定唱名、bag；音频测试先固定非法 note set、reserve、重播重启、单/三音 PCM；App 测试先固定 turn 不执行、默认 Next、Replay 保持与 Level 返回；snapshot 先覆盖双 profile 极端 note/chord，再实现。

## Risks / Trade-offs

- [C2–C6 ledger line 在 320×170 过密] → 使用 profile-specific integer spacing、极端 snapshot 与 clip diagnostics；若失败只缩小 Level 2 范围，不让 renderer 越界。
- [新 PlayNotes 与 cue 相互停止] → note set 被定义为 foreground audition，Reveal 不同时发 cue；system menu/alert 仍可 StopAll 或以更高层系统动作取代它。
- [重复 Replay 填满队列] → PlayNotes 使用普通水位且可丢弃，保留关键命令；UI 状态不依赖音频接受成功。
- [三音 gain 在小扬声器削顶或浑浊] → 固定保守 per-voice gain、PCM peak test、实体 Medium/High 试听；无实机时明确 pending。
- [OFL 来源丢失或 Reserved Font Name 被误用] → 仓库记录 commit、版权和许可证；只提交派生图形，不把修改字体命名为 Bravura。
- [Cover 自动门禁被误当视觉批准] → review/formal 目录隔离，用户批准路径和日期写入 provenance 后才集成。
- [新增 App 使旧测试硬编码数量/顺序] → 更新所有内置注册夹具，并新增明确顺序断言而非只改期望计数。

## Migration Plan

1. 先落地调研、delta spec 与失败测试。
2. 增加 MusicalNoteSet 数据/queue/service/host 契约，保持旧 SoundCue 路径不变。
3. 增加音乐 glyph pipeline、SIGHT 数据模型、输入状态机与双 profile 渲染。
4. 在 firmware/desktop/headless 注册 Timer → SIGHT → Motion，并更新完整测试夹具。
5. 生成 Cover review candidate；批准后才正式集成并更新 snapshot。
6. 跑 host、desktop、firmware、shared-source、size、snapshot、diff/immutability 门禁；若需回滚，可移除 SIGHT 注册与 PlayNotes 分支，旧 cue ABI/行为保持。

## Open Questions

- Musical tone 在实体 T-Embed 的音高辨识、响度与连续 Replay 疲劳度等待真机试听；自动 PCM 只能证明安全与确定性。
