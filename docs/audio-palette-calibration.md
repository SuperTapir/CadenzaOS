# Semantic Hierarchy 合成校准记录

> 状态：Navigate v6、ToggleOn v2、ToggleOff v2 与原创 Menu Surface V2 已通过用户 A/B；参考 WAV 仅存在于本地试听工作区，不属于仓库输入、构建依赖或产品资产。

## 校准边界

用户接受以参数合成近似还原参考声音，要求听感不得“天差地别”，并保留逐 cue fallback。这里的自动特征只负责发现数量级偏差，不代替人工听感：

- 总时长偏差不超过参考的 15% 或 2 ms（取较大者）；
- 有效响度事件数量一致，事件间静音结构不得消失；
- 主导频段保持在同一听觉区间，不得从短高频触点变成长低频音符；
- 原始 peak 不用于直接归一化产品响度，合成候选 RMS 与参考相差超过 4 dB 时必须解释或调整 event gain；
- cue 结束后回到精确数字零；连续 Navigate 不得补播历史输入；
- 用户在单音与上下文 demo 中确认没有明显身份、材质或动作轮廓偏差。

## 本地参考测量

测量口径为 44.1 kHz S16 mono 本地校准副本、全文件 Hann FFT spectral centroid、1 ms RMS block 与 -45 dBFS active threshold。SHA-256 只用于证明本次分析对象，不能用于恢复或分发参考音频。

| Cue | 本地源摘要 | 时长 | Peak / RMS | Spectral centroid | Active spans |
| --- | --- | ---: | ---: | ---: | --- |
| Navigate / Select | `877cbda83810…` | 10.431 ms | -7.71 / -24.02 dBFS | 3027 Hz | 3–6 ms |
| ToggleOn / Turn On | `cf04e7f2cc02…` | 63.424 ms | -0.07 / -19.96 dBFS | 3263 Hz | 4–15 ms、45–55 ms |
| ToggleOff / Turn Off | `6ffaa6afbff8…` | 65.125 ms | -0.05 / -20.05 dBFS | 3368 Hz | 7–15 ms、49–56 ms |

可观察结构：Navigate 是约 10 ms 文件中的单次极短高频触点；ToggleOn/Off 都是两个短事件，中间约 30 ms 数字静音。两项 Toggle 的区别来自事件时序和第二段方向，不来自持续旋律。

## 首个合成候选

候选只使用重新创作的 Sine/Triangle 频率轨迹、短 attack、预计算指数衰减和 release，不包含参考 PCM。独立 A/B 页面位于本地试听工作区 `audio-study/synth-approximation.html`。

| Cue | 时长 | Peak / RMS | Spectral centroid | 自动判断 |
| --- | ---: | ---: | ---: | --- |
| Navigate synth v6 | 10.431 ms | -8.11 / -24.3 dBFS | 波形拟合 | 四个原创阻尼共振器；去除参考底噪后的全文件相关性 0.972、归一化 RMSE 0.233；用户通过 |
| ToggleOn synth v2 | 63.492 ms | -0.10 / -18.2 dBFS | 1887 Hz | 时长/双事件通过；第二段强化上行；用户通过 |
| ToggleOff synth v2 | 64.989 ms | -0.10 / -18.3 dBFS | 2599 Hz | 时长/双事件通过；第二段强化下行；用户通过 |

频谱重心低于参考不自动判失败：参考的瞬态噪声会显著抬高 centroid，而主观身份更多由短时主频、crest factor 与双事件节奏决定。若人工判断合成版过闷或过于音符化，再增加原创高频瞬态比例，不复制参考波形。

Select v6 使用四个阻尼共振器（约 1.64、2.29、2.85、5.84 kHz），分别具有独立初相位与 0.24–0.50 ms 衰减。参数由最小二乘与局部坐标搜索得到；模型只保存频率、衰减、幅度和相位，不保存参考采样序列。v5 仍保留在本地试听工作区作为 fallback。

## 正式运行时复核

获批参数移入 portable C++ 后，以 Medium master gain 反归一化，与试听页 WAV 在共同长度内比较：Navigate 相关性 0.999878，ToggleOn 0.992850，ToggleOff 0.999870。三项均保持原事件时序；ToggleOn 首事件明确延迟 3.5 ms，避免试听稿与运行时错位。正式 15-cue PCM golden 已冻结，所有 cue 结束后精确回零。用户对最终 Select 表示“很棒就这样吧”，因此该版本进入实现基线；后续改音必须显式更新试听结论与 golden。

## Confirm / Back 回归校准

2026-07-18 兼容性复核发现，正式 C++ 的 Confirm/Back 虽然拥有确定性 golden，但没有落在用户拍板的 `09-semantic-hierarchy-full` 进入/退出原型边界。golden 只能证明错误输出没有漂移，不能证明听感移植正确。以下原创试听 WAV 仅作为本地校准输入，不是运行时或构建依赖：

| Cue | 总时长 | Peak / RMS | 第一事件 | 第二事件 | 结构边界 |
| --- | ---: | ---: | --- | --- | --- |
| Confirm | 244.989 ms | -12.41 / -23.40 dBFS | 0 ms，约 660 Hz，尾部约 170 ms | 60–64 ms，约 990 Hz，延续到 245 ms | 两段固定调音敲击，上行音程；不得变成连续上滑 |
| Back | 210.000 ms | -10.50 / -23.40 dBFS | 0 ms，约 990 Hz，尾部约 130 ms | 40–45 ms，约 590 Hz，延续到 210 ms | 不对称下行，第二段更闷；不得复用 Confirm 或旧连续下滑 |

自动门禁直接约束事件数量、offset、每段主频漂移、事件时长与总时长；测试已先对旧正式实现产生预期失败，再随校准实现转绿。修正后的正式输出与获批原型在共同长度内的相关性为 Confirm 0.984379、Back 0.979893，有效结束时间分别为 244.92 ms、209.93 ms；对应 PCM golden 已更新。

App 集成回归从 Launcher 实际打开应用，并通过显式 Home 动作返回；App open 使用 Confirm，Home transition 使用 Back，两者均通过 `SystemServiceHost`。由此同时锁定业务语义、系统服务路由和合成输出，避免试听稿更新而 App 仍播放旧声。上下文试听导出为 `confirm-fixed.wav` 与 `back-fixed.wav`，仅用于人工验收，不进入构建。

## Menu Surface V2 校准

System Menu 原先展开复用 Confirm、收起复用 Back。两者虽然方向不同，却同属 210–245 ms 的双敲 Action，导致高频 Surface 操作都被听成难听的 confirm。修订后新增同时起音、无第二敲击的 MenuOpen/MenuClose；试听稿直接使用正式 `AudioEngine` 与 Medium master gain 生成，不依赖外部参考音频：

| Cue | 有效时长 | Peak / RMS | 主 tone | 触感 tone | 用户结论 |
| --- | ---: | ---: | --- | --- | --- |
| MenuOpen V2 | 159.9 ms | -17.39 / -28.89 dBFS | Sine，约 340→680 Hz，指数衰减 | 同时起音的 48 ms 低增益 Noise | 更低、更长且上升幅度明确；通过 |
| MenuClose V2 | 139.9 ms | -17.75 / -30.22 dBFS | Sine，约 640→290 Hz，指数衰减 | 同时起音的 42 ms 低增益 Noise | 更低、更长且下潜幅度明确；通过 |

正式独立 PCM golden 分别为 `0x7E65C35377921CA6` 与 `0x68D82E874AB181F1`。真实 Runtime 在先播放 App Confirm、再展开和收起 Menu 的上下文中，golden 分别为 `0xFFD867CDEBDCD06A` 与 `0xBC8674B8C5CA39A5`；Noise 状态会随此前 voice sequence 确定性演进，因此上下文 hash 与 fresh-service hash 不同。两 cue 均在 0.20 秒视觉形变内结束并与动画起点同帧提交。

## Fallback

任一 cue 的合成近似未通过人工 A/B 时，先继续调整对应 palette 参数；若受限合成器仍无法达到可接受听感，则为该 cue 单独提出 sample voice 变更。不得在当前实现中悄悄嵌入本地参考 WAV。
