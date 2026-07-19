# SIGHT 识谱训练 App 参考调研

日期：2026-07-19

## 1. 问题与边界

SIGHT 需要在 320×170 T-Embed 与 400×240 Sharp profile 上显示大谱表、
单音与原位三和弦，使用固定唱名揭晓答案，并通过现有 44.1 kHz 四声部核心
播放指定音高。它不是完整乐谱排版器、MIDI sequencer、钢琴采样器或学习记录
系统；首版不做语音识别、评分、持久化、节奏、调号、转位或间隔复习。

平台现状明确把 `ToneSpec` 定义为内部实现，而非音乐 API。因而本次调研重点
不是寻找一个完整 synth，而是确定：

- App 使用什么稳定音高模型；
- 如何在不暴露波形、gain、频率与 voice 的前提下请求单音/和弦；
- 如何以固定容量、无运行时分配的方式绘制最小必要记谱；
- 哪些成熟实现只作语义参考，哪些资产可以合法离线派生。

## 2. 锁定参考、许可证与关键源码

| 参考 | 锁定版本 / commit | 许可证 | 阅读位置 | 采用边界 |
| --- | --- | --- | --- | --- |
| [Playdate SDK Inside Playdate](https://sdk.play.date/3.1.0/Inside%20Playdate.html) | 3.1.0 | 官方 SDK 文档 | `sound.synth:playNote`、`playMIDINote`、`instrument` | MIDI note / note name 与 Hz 分层、受限 voice pool、显式 all-notes-off 语义 |
| [MusicXML](https://www.w3.org/2021/06/musicxml40/) | 4.0 | W3C Community Final Specification Agreement | pitch、clef、staff、harmony schema/tutorial | `step + alter + octave` 的拼写音高模型、C4 中央 C、和弦 root/kind 与实际音高分离 |
| [VexFlow](https://github.com/0xfe/vexflow/tree/9e443bdde82091d1e03dbab389547858aed2f6fe) | 4.2.6 / `9e443bd` | MIT | `src/tables.ts`、`stavenote.ts`、`accidental.ts` | 七级音阶索引映射半线距、clef shift、ledger line 补线与相邻和弦 notehead 错位规则；不复用 JS runtime |
| [Bravura](https://github.com/steinbergmedia/bravura/tree/02e8ed29a29115df35007d1178cebaeee26c20e1) | `02e8ed2` | SIL OFL 1.1，Reserved Font Name “Bravura” | `redist/otf/Bravura.otf`、`LICENSE.txt` | 仅离线生成 clef/accidental 1-bit 图形；保留来源与许可，不把 501 KiB OTF 带入 firmware |
| [ArduboyTones](https://github.com/MLXXXp/ArduboyTones/tree/972fe8117002da47073b6c1835117d654a03e16f) | 1.0.3 / `972fe81` | MIT | `ArduboyTonesPitches.h`、`ArduboyTones.h/.cpp` | 小设备以 pitch table + duration command 公开乐音的先例；不采用 AVR timer/square 实现与整数 Hz 量化 |

### 2.1 成熟语义

MusicXML 4.0 把 played pitch 表示为 `step`、可选 `alter`、`octave`，并明确
`C4` 是中央 C；这保留了 `F#` 与 `Gb` 的记谱差异，而 MIDI number 单独承担
播放身份。和弦符号同样把 root step/alter 与 chord kind 分离。SIGHT 因此不能
只存 MIDI number，否则常用和弦揭晓时会丢失正确拼写。

Playdate 3.1.0 同时提供 Hz 与 MIDI note 两层 API；`60=C4`，instrument 通过
固定 voice pool 获得复音，并提供 `allNotesOff()`。这证明“小型设备允许受限
乐音命令”是成熟边界，但 Playdate 的任意 synth/envelope/filter 表面积远超
Cadenza 的 App capability 需求。

VexFlow 4.2.6 的 `Tables.keyProperties()` 使用 `octave * 7 + diatonic index`
计算半线距位置，再施加 clef shift；`StaveNote::drawLedgerLines()` 只在超出五线
谱时逐线补齐，和弦中相邻半线距 notehead 会横向错位。这些规则可用少量整数
重写，并为 golden case 提供来源；完整 VexFlow 依赖 TypeScript、动态对象、
SVG/Canvas、完整字体与复杂 accidental collision，不适合固件。

Bravura 提供符合 SMuFL 的成熟谱号轮廓，但完整 OTF 为 512,924 bytes、SVG 为
1,923,347 bytes。首版只需要离线光栅化少量 glyph；生成结果作为图形资产，
记录 `02e8ed2` 与 OFL 来源，不分发或改名冒充字体本体。

ArduboyTones 用 `NOTE_C4=262` 等整数 Hz 常量和频率/时长对驱动 AVR timer，
表面积很小，但它是单/双 pin square tone、整数频率且无当前 PCM voice/queue
语义。引入只会制造第二套 audio core，因此只采用“pitch command 是数据而非
合成参数”的思想。

## 3. 候选方案比较

### 3.1 音频公共契约

| 方案 | 语义稳定性 | 命令/内存 | 可移植性 | 已知问题 | 决策 |
| --- | --- | ---: | --- | --- | --- |
| 为每个音扩展 `SoundCue` | 差：把内容数据固化成枚举 | 至少 88×组合爆炸 | 高 | 和弦无法组合，palette 与业务耦合 | 不采用 |
| App 直接提交 `ToneSpec` | 差：波形、gain、时长成为长期 API | 大 payload，验证面大 | 中 | 可绕过平台音色、音量与资源策略 | 不采用 |
| App 提交任意 Hz 数组 | 中 | 小 | 中 | NaN/range/微分音验证；记谱拼写与播放脱节 | 不采用 |
| App 提交固定最多四项 MIDI note set | 高 | 约 8 bytes payload | 高 | 只支持十二平均律与固定音色 | 采用 |

采用 `MusicalNoteSet`：最多四个 MIDI note，限定钢琴范围 21–108；App 无权设置
waveform、gain、duration、priority 或 channel。服务把 note set 转换为统一的
短衰减乐音，遵守 session volume、Muted、固定 voice、queue overflow 与
StopAll。SIGHT 只提交一音或三音。

`PlayNotes` 是可丢弃内容命令：快速连按最多占普通水位，不侵占音量、停止与
关键 cue 保留容量。消费新 note set 前停止旧 note set 并清空其尾音，使重播
语义是“从头播放”而非叠加。该局部契约保留未来音乐 subsystem 的空间，但不
提前实现 sequence、tempo、note-off、instrument 或 background music。

### 3.2 记谱实现

| 方案 | Flash / RAM | 质量 | 扩展成本 | 决策 |
| --- | ---: | --- | --- | --- |
| 完整 VexFlow / MuseScore 类排版器 | 远超预算，动态对象 | 完整 | 低（桌面） | 固件不可行 |
| 把完整 Bravura OTF 带入 runtime | 约 501 KiB + rasterizer | 高 | 中 | 不采用 |
| 手绘全部谱号和 accidental | 最小 | 谱号质量风险高 | 高 | 不采用 |
| Bravura 少量 glyph 离线转 1-bit + 原生几何 | 小型只读 bitmap | 可审计 | 与首版范围匹配 | 采用 |

五线、加线、notehead、stem、选择条与 action strip 使用 `MonoCanvas` 原语；
谱号和必要 accidental 使用离线生成、固定尺寸的 1-bit bitmap。音高模型保留
step/alter/octave，但单音 Level 只生成 natural；和弦 Level 才显示 `#`/`b`。

## 4. 最小实验

在当前 `AudioEngine` 上编译独立 probe，同时提交 C4/E4/G4 三个指数衰减
sine-resonator，并渲染一秒 PCM：

```text
C2=65.4064 Hz
C4=261.6255 Hz
C6=1046.5022 Hz
peak=14578
nonzero samples=30842
final sample=0
```

结果证明采用范围完全落在引擎 20–16,000 Hz 安全边界内，三音和弦未饱和，
0.70 秒后返回精确零；当前固定四 voice 能覆盖三和弦并保留一声容量。probe
仅验证技术可行性，不证明最终音色已通过实体扬声器试听。

几何方面，VexFlow 的整数/半整数 line 模型可直接压缩为 diatonic step：每个
相邻自然音移动半个 staff space。两种 framebuffer 只需改变 staff space、
谱号 bitmap 与 answer/action 区域，不需要动态布局树或额外 framebuffer。

## 5. 产品与工程决策

- App 名称与 Launcher title 为 `SIGHT`，注册顺序位于 Timer 后，保留现有
  AppId，不通过重编号改变公共身份。
- `LEVEL 1`：Grand staff、C3–C5（含端点）、natural 单音，默认选中。
- `ALL NOTES`：Grand staff、C2–C6（含端点）、natural 单音。
- `COMMON CHORDS`：十二个 root 的 major/minor 原位三和弦；保留正确升降号
  拼写，答案显示 chord name 与组成音名；固定唱名用于单音题。
- 使用固定 seed shuffle bag 保证一轮全覆盖、无直接重复；无评分、持久化或
  Anki 间隔。
- 问题页 click 揭晓并播放；答案页 action strip 为 `REPLAY / NEXT / LEVEL`，
  默认 `NEXT`。旋钮只移动选择，click 才执行；`REPLAY` 后焦点留在 Replay。
- 长按继续由系统菜单拥有；SIGHT 不私有化 long-press。
- MusicalNoteSet 继续服从 SoundPlay capability 和 session volume，不开放
  任意 synth 参数。
- Launcher Cover 走现有视觉 gate；未获用户明确批准的候选只进入 review，
  不写正式 source/PBM/header。

## 6. 不采用项与重新评估条件

不采用语音识别、麦克风 pitch detection、MIDI input、键盘图、成绩、学习历史、
节奏、调号、转位、七和弦、完整 88 键 ledger-line 展示、外部乐谱导入、sample
piano 与背景音乐。

以下证据出现时才重新评估：

- C2–C6 在 320×170 上无法保持谱号、notehead、ledger line 与答案同时清晰；
- 参数乐音在实体 T-Embed 上无法分辨低音或连续重播明显疲劳；
- 常用和弦要求四音及以上、转位或 sequence；
- 第二个 App 也需要持续音乐、note-off 或 instrument，说明局部 note-set 契约
  应升级为独立音乐服务；
- 用户实际使用证明需要更小 Intro、更大音域或持久化学习记录。

## 7. 验证门禁

- 数据：C4=60、C3–C5/C2–C6 端点、自然音全覆盖、和弦拼写/MIDI 转换、shuffle
  bag 无重复与确定性。
- 输入：揭晓、默认 Next、Replay 保持、旋钮仅选中不执行、Level 返回、长按
  系统菜单不回归。
- 音频：非法 count/note 拒绝、单音/三音 PCM、重播重启、Muted/StopAll、queue
  reserve/overflow、结束精确零。
- 图形：T-Embed 与 Sharp question/answer/level snapshot、极端 ledger line、
  sharp/flat chord、clip diagnostics 与无越界。
- 集成：Launcher 顺序、fallback/正式 Cover、launch handoff、headless flow、
  host、desktop、firmware/shared-source、size gate 与 `git diff --check`。
- 实体：T-Embed 音高可辨性、Low/Medium/High、三和弦削顶、连续重播疲劳；无
  实体结果时必须明确标记 hardware listening pending，不冒充已完成听感审批。
