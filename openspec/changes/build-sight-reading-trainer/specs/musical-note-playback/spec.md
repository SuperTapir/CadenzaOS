## ADDED Requirements

### Requirement: App 只能提交受限 MusicalNoteSet
系统 SHALL 允许具备 SoundPlay capability 的 App 提交包含 1–4 个 MIDI note 的 `MusicalNoteSet`，每个 note SHALL 位于钢琴范围 21–108；公共命令 MUST NOT 暴露 Hz、waveform、gain、duration、priority、voice、channel、文件或任意 `ToneSpec`。

#### Scenario: 提交单音
- **WHEN** 授权 App 提交只包含 MIDI note 60 的 MusicalNoteSet
- **THEN** 系统接受命令并以平台固定乐音播放 C4

#### Scenario: 提交三和弦
- **WHEN** 授权 App 提交 60、64、67
- **THEN** 系统同时播放三个音且不需要 App 分配或控制 voice

#### Scenario: 拒绝非法 note set
- **WHEN** count 为 0、超过 4 或任一 note 超出 21–108
- **THEN** 系统拒绝命令、保持现有声音状态且记录 command rejection

#### Scenario: 未授权 App 提交
- **WHEN** 不具备 SoundPlay capability 的 App 提交合法 MusicalNoteSet
- **THEN** Runtime 在到达音频服务前拒绝操作并记录 capability rejection

### Requirement: 乐音具有固定且一致的播放轮廓
音频消费者 SHALL 把有效 MIDI note 转换为十二平均律频率，并 SHALL 使用固定 waveform、attack、decay、release、gain、duration 与 priority；单音和最多四音 SHALL 服从同一 session volume，输出 SHALL 确定、有限、不整数环绕且结束后回到精确零。

#### Scenario: C4 频率
- **WHEN** 消费者播放 MIDI note 60
- **THEN** 目标基频为约 261.6256 Hz，PCM 非零且固定时值后回到零

#### Scenario: 三音最坏混音
- **WHEN** 播放合法三和弦并以 High session volume 渲染完整 PCM
- **THEN** 每个样本位于 int16 范围、没有 NaN/Infinity/环绕且尾部为精确零

### Requirement: Replay 从头重启而不叠加
消费新的 MusicalNoteSet 前，服务 SHALL 停止当前 audition voice 和其 delayed state，再同时启动新 set；重复提交同一 set SHALL 从统一 attack 重新开始，而 MUST NOT 把旧尾音叠加成额外和弦。

#### Scenario: 音身期间 Replay
- **WHEN** 同一单音尚未结束时再次提交相同 MusicalNoteSet
- **THEN** 旧音停止、新音从 attack 开始且 active voice 不随 Replay 次数累积

#### Scenario: 和弦切换
- **WHEN** 一个三和弦尚未结束时提交另一个三和弦
- **THEN** 输出切换到新和弦且旧和弦的 voice 或 delayed event 不会稍后恢复

### Requirement: MusicalNoteSet 遵守队列保留与静音安全
PlayNotes 命令 SHALL 使用固定 SPSC queue 的普通水位并可被丢弃，MUST NOT 侵占关键命令 reserve；Muted 与 StopAll SHALL 在队列饱和时仍于下一次 render 前清除 active、pending 与 delayed audition state，恢复音量后 MUST NOT 补播历史 note set。

#### Scenario: Replay 填满普通水位
- **WHEN** 生产者在消费者未排空时连续提交 PlayNotes 至普通水位
- **THEN** 后续 PlayNotes 被拒绝并记录 overflow，关键 SetVolume/StopAll 仍可进入 reserve

#### Scenario: 饱和时静音
- **WHEN** queue 饱和且请求 Muted
- **THEN** 下一次 render 输出精确零，历史 MusicalNoteSet 不会在恢复音量后补播
