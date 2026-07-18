# Cadenza 交互音效资产标准 v1

本标准回答的是“以后拿什么声音文件来，就可以稳定替换当前合成音”，而不是规定声音必须由哪种工具生成。人耳判断负责决定它是否好听；格式、峰值、语义、来源和平台验证负责决定它是否可集成。

当前版本仍使用参数合成，尚未实现 sample voice。本文先冻结交付边界；未来接入音频文件时，不改变 App 的 `SoundCue`、Settings 音量、SDL/I²S 输出或视觉反馈契约。

## 0. 行业共识与项目选择

市面上没有一个“所有 UI 音效都必须是某个时长、某个 dB、某个 WAV 位深”的统一标准。游戏音频工具链的稳定共识是：

```text
无损高质量源文件
        ↓ import + metadata
语义 Event（变体 / gain / pitch / cooldown / polyphony）
        ↓ 离线按平台转换
SoundBank / canonical PCM
        ↓ SFX bus + 用户音量
目标设备上下文试听
```

证据：

- [FMOD Managing Assets](https://www.fmod.com/docs/2.03/studio/managing-assets.html) 推荐未压缩 48 kHz 源文件，并说明 bank 构建会重新编码，源文件压缩格式不决定运行时成本；匹配目标平台采样率可避免运行时 resampling。
- [Wwise Conversion Settings](https://www.audiokinetic.com/en/public-library/2025.1.3_9037/?id=creating_audio_conversion_settings_sharesets&source=Help) 把 sample rate、格式和声道作为逐平台离线转换设置。
- [Unity Audio Compression](https://docs.unity3d.com/Manual/AudioFiles-compression.html) 同样在 import 后按 build target 转码，并按声音长度、CPU、内存和质量选择 PCM、ADPCM 或压缩格式。
- [FMOD Events](https://www.fmod.com/docs/2.03/studio/authoring-events.html) 把 Event 定义为游戏触发的声音单元；[Multi Instruments](https://www.fmod.com/docs/2.03/studio/working-with-instruments.html) 用 shuffle/变体和有限随机化减轻重复感。
- [Apple Playing Audio](https://developer.apple.com/design/human-interface-guidelines/playing-audio) 要求非必要反馈服从静音和用户音量，并建议对高频重复声音做克制变化；[Xbox XAG 103](https://learn.microsoft.com/en-us/xbox/accessibility/xbox-accessibility-guidelines/103) 要求关键音频信息存在其他感官通道。
- [Inside Playdate](https://sdk.play.date/inside-playdate) 固定 44.1 kHz，区分短样本内存播放、长文件 streaming 与合成器，并把 ADPCM 作为设备上的体积/CPU折中。

因此下面明确标注三类规则：**行业惯例**、**Cadenza v1 决策**、**真机待定标**。后两者不能伪装成行业标准。

## 1. 交付包

每个候选音效需要同时提交：

1. 一份无损母版；
2. 一份 manifest 记录语义、版本、来源、权利和制作说明；
3. 可选的 1–3 个候选版本，用 `a`、`b`、`c` 后缀区分。

母版规则：

- **行业惯例 / Cadenza 接受**：WAV；PCM 24-bit、PCM 32-bit 或 32-bit float；
- mono 优先；若生成工具只能导出 stereo，可以交付，但左右声道必须可安全 downmix；
- **行业惯例**：48 kHz 常用作游戏音频母版；**Cadenza 接受** 44.1 kHz 或 48 kHz，转换过程保留原母版；
- 禁止 MP3、AAC 等有损文件作为母版；
- 不包含循环点、空间化、混响长尾或依赖耳机的立体声定位；
- 不得 sample clipping；分析报告记录 sample peak、true peak、RMS、DC 和频谱。保留约 3 dB 制作余量是建议，不是通用合格线；
- 必须附可商用、可修改、可再分发的权利说明。生成模型、素材库、录音来源、作者和许可证未知时，不进入仓库。

仓库命名采用：

```text
assets/audio/source/<cue>/<cue>-v<major>-<candidate>.wav
assets/audio/source/<cue>/<cue>-v<major>-<candidate>.json
```

例如 `confirm-v1-a.wav`。不要用 `final-final2.wav`，也不要用听感形容词代替系统语义。

## 2. 固定语义清单

| Cue | 要表达的动作 | 建议有效时长 | 方向/区分要求 |
| --- | --- | ---: | --- |
| `navigate` | 选择移动成功 | 20–50 ms | 最轻、最短，不应像确认 |
| `boundary` | 已到边界、没有移动 | 35–90 ms | 与 navigate 明显不同，不应像错误警报 |
| `confirm` | 打开/提交成功 | 70–150 ms | 倾向上行或展开感 |
| `back` | 返回/关闭成功 | 70–150 ms | 倾向下行或收束感，与 confirm 成对 |
| `toggle-on` | 状态开启 | 50–110 ms | 与 confirm 同族但更轻 |
| `toggle-off` | 状态关闭 | 50–110 ms | 与 back 同族但更轻 |
| `reject` | 操作不成立 | 70–180 ms | 可辨认但不能刺耳、羞辱或像系统故障 |

“建议有效时长”是听觉主体，不含最多 20 ms 的安静收尾。超出范围不是自动淘汰，但必须说明为什么更长仍不会拖慢连续操作。

任何 cue 都不能成为唯一反馈；声音文件的替换不得改变现有视觉状态和动作结果。

## 3. Cadenza 运行时成品格式

集成工具最终统一生成以下 canonical PCM，母版不需要手工做成这个格式。这些是 **Cadenza v1 平台决策**，不是市场统一标准：

- 44,100 Hz；
- signed 16-bit little-endian PCM；
- mono；
- one-shot，不循环；
- 每个文件最长 250 ms；
- 起音在文件开始后 5 ms 内出现；禁止为了“防爆音”塞入大段前导静音；
- 开头和结尾必须从近零幅度进入/离开，转换器允许增加 0.5–2 ms 的线性安全坡度；
- DC offset 低于 -60 dBFS；结束后输出必须是精确数字零。

短于 400 ms 的 UI 音效不以 LUFS 作为唯一归一化依据；LUFS 在这种时长上不稳定，也不能代表小扬声器舒适度。自动门禁检查 sample/true peak、DC、时长、前导静音、尾部和确定性 hash；响度平衡与音色仍由 A/B 试听决定。

canonical 转换默认不做自动响度归一化，也不把 gain 烘焙进母版。每个 Event 独立保存 `gain_db`，所有交互音进入统一 SFX bus，再乘用户的 Muted/Low/Medium/High。四声最坏并发由离线 peak simulation 验证；若可能 clipping，先调 Event gain，再考虑只衰减 canonical asset。不存在行业通用的 `-9 dBFS` 答案，初始 gain 必须由整套声音和 T-Embed 扬声器共同定标。

频谱建议而非硬门禁：主体尽量位于 300 Hz–4 kHz，谨慎使用 150 Hz 以下和 8 kHz 以上能量。MAX98357A 与 T-Embed 小扬声器会让低频消失、让尖锐高频更疲劳，桌面耳机好听不等于真机成立。

## 4. Manifest 最小字段

模板位于 `assets/audio/manifest.example.json`。必填字段包括：

- `cue`、`version`、`candidate`；
- `source_file`、生成/录制工具和制作日期；
- `origin`：生成模型与提示词、录音来源或素材库条目；
- `rights`：权利人、许可证、可商用/修改/分发结论及证据链接；
- `intent`：希望表达的动作和听感；
- `sha256`：母版内容摘要；
- `review`：被选中后记录桌面与真机试听结果。

提示词可能包含商业秘密时，可以保存不可逆摘要和足够的权利证据，但不能把 `origin` 留空。

## 5. 替换流水线

1. **入库前试听**：先由人选出主观喜欢的候选，不让自动指标替你决定“好听”。
2. **静态校验**：验证 WAV、采样率、声道、时长、clipping、DC、leading/tail silence、manifest 和 SHA-256。
3. **确定性转换**：downmix/resample 为 44.1 kHz S16 mono，必要时增加边缘坡度；生成 canonical PCM、转换报告和 hash，不擅自做响度归一化。
4. **引擎回归**：验证无 heap sample loop、四声并发不 clipping、Muted/StopAll、抢占、结束精确零、SDL dummy 与 firmware compile。
5. **上下文 A/B**：在真实 Launcher、Settings、打开和返回动作里与当前版本对比，而不是只在播放器里单独听。
6. **T-Embed 验收**：在设备扬声器上检查响度、破音、动作同步和连续 50 次操作的疲劳度；桌面通过不能替代此项。
7. **冻结版本**：只有候选被选中后才更新 PCM golden/hash；golden 证明“没有意外变化”，不证明“它好听”。

试听记录每项 1–5 分：语义清晰、舒适、与整套声音一致、动作同步、连续操作不疲劳。候选要成为默认值，前四项至少 4 分，连续 50 次疲劳度至少 4 分，并且不存在破音或明显 click。未做实体 T-Embed 试听时必须标记 `pending_hardware`，不得写成已验收。

## 6. 将来交给 Codex 的最小材料

你只需要提供 WAV 候选、对应 cue 名称和来源/权利信息。48 kHz/24-bit mono WAV 是最省事的通用交付，但 44.1 kHz 无损母版同样接受。Codex 应负责生成或补全 manifest、运行校验与转换、接入 sample voice、保留合成音作为可回退实现、完成自动门禁，并把实体设备试听清单交给你。除非你明确要求，集成阶段不得擅自“优化”你已经选中的音色。
