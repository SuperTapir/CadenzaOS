## Why

CadenzaOS 目前缺少一个能把大谱表位置、音名、固定唱名与真实音高连接起来的短循环练习工具。现有四声部音频核心已经具备技术基础，但 App 公共契约只允许语义 `SoundCue`；现在需要在不暴露任意合成参数的前提下，增加受限乐音播放，并用一个位于 Timer 后的 SIGHT App 验证它。

## What Changes

- 新增内置 `SIGHT` App，提供 `C3–C5` 默认单音、`C2–C6` 全音域单音和常用原位大/小三和弦三档 Level。
- 新增 step/alter/octave 拼写音高、固定唱名、MIDI note 转换、确定性 shuffle bag 与最小 grand-staff 1-bit 记谱。
- 固定交互为：click 揭晓并播放；答案页 `REPLAY / NEXT / LEVEL`，默认 `NEXT`；旋钮只移动选择，click 才执行，避免误触丢题。
- 在现有 SoundPlay capability 下新增固定最多四音、钢琴 MIDI 范围、固定音色/时长的 `MusicalNoteSet` 命令；不向 App 暴露 `ToneSpec`、Hz、gain、waveform、priority 或 voice。
- 新增 Bravura 来源的离线 1-bit 谱号/升降号子集及许可/来源记录；不把完整字体或排版 runtime 带入 firmware。
- 新增 SIGHT Launcher Cover；候选在用户明确批准 pixel baseline 前只保存在 review 目录，不覆盖正式资产。
- 补齐数据、输入、音频、双 profile snapshot、Launcher 顺序、headless、desktop、firmware 与 size 验证。

## Capabilities

### New Capabilities

- `sight-reading-trainer`: 定义 SIGHT 的 Level、随机出题、记谱、答案、Replay/Next/Level 状态机、Launcher 位置与无持久化边界。
- `musical-note-playback`: 定义受限 MIDI note set、固定音色/复音、队列、音量、重播重启、静音与停止语义。

### Modified Capabilities

- `portable-audio-core`: 把当前四 voice 实现与规范对齐，并让固定 SPSC 队列安全承载可丢弃的 MusicalNoteSet，同时保留关键命令容量与实时安全边界。

## Impact

- 影响 `cadenza_core` 的音频命令/服务、`SystemCommand` 验证和便携 PCM 测试。
- 影响 `cadenza_apps` 的内置 AppId、App 类型、SIGHT 数据/渲染、Launcher 注册顺序与 Cover 资源。
- 影响 firmware、desktop/headless host 的内置 App 注册以及 Launcher/app/snapshot 测试夹具。
- 新增小型只读音乐 glyph/Cover 资产、来源文档与 OpenSpec delta；不引入运行时第三方库、heap、文件系统、网络或持久化依赖。
