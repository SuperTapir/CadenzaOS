# P0–P7 and interaction-audio verification record

Verified on macOS on 2026-07-18. This file records authoritative gates, not a
claim that physical-device P8 work has happened.

| Phase | Evidence |
| --- | --- |
| P0 decisions | `project-vision.md`, `reference-research.md`, and `engine-adoption-decision.md`; SDL3/U8g2/doctest/stb/gif-h adopted at narrow seams, LVGL/Tweeny full-runtime adoption rejected by measured spikes |
| P1 decoupling | `cadenza_portable_core_compile_probe` and `cadenza_shared_source_audit`; T-Embed and desktop adapters feed shared `InputReducer`, App, Runtime, canvas, and animation code |
| P2 simulator MVP | `cadenza_desktop_smoke_tests` visits every registered App including Gallery through desktop input mapping; SDL dummy launch succeeds for the real executable |
| P3 debugging | simulation controller, PNG, recording, desktop model, device-preview, and diagnostic tests cover pause/step/speed/fixed-real modes, capture, GIF finalization, HUD data, and invariance |
| P4 animation core | easing/Tween/composition/Spring suites cover endpoints, interior vectors, delay, finite/infinite repeat, yoyo/reverse, callback, seek, pause, large delta, capacity, and diagnostics |
| P5 presentation | transition/effects/particle/frame/state tests plus Gallery pages, reduced-motion propagation, deterministic cross-cycle replay, and visible reversible scrub for Spring/selection/camera/particles |
| P6 graphics | guarded framebuffer, U8g2 raster/font golden tests, bitmap composition/flip/XOR/clipping, atlas, dither phase, and off-screen immutability |
| P7 regression | 32/32 normal, 32/32 warnings-as-errors, and 32/32 ASan+UBSan tests; app and representative Gallery snapshots at both profiles emit PNG artifacts on mismatch |

Fresh completion gates:

- host Debug tests: 32/32 pass;
- strict AppleClang build (`-Wall -Wextra -Wpedantic -Werror`): 32/32 pass;
- ASan+UBSan host build: 32/32 pass;
- SDL3 3.4.12 dummy-driver executable smoke: pass at 320×170 @ 2× and
  400×240 @ 4× with HUD and device frame enabled;
- PlatformIO T-Embed clean release compile: pass;
- firmware size: 82,616 B RAM, 316,409 B Flash;
- `git diff --check`, source-manifest audit, platform-coupling audit, and
  third-party pin/license audit: pass.

The final temporal Gallery audit sampled Spring, selection, camera, and
particles at five frames across two cycles and both profiles. It caught and
fixed one-shot demos, zero-delta CameraShake state mutation, non-functional
stateful scrubbing, and a hard-coded particle origin. The accepted particle
midpoint snapshots now remain centered at both 320×170 and 400×240.

## Interaction audio automated evidence

Verified on macOS on 2026-07-18 after the audio implementation and asset
contract were added:

| Gate | Evidence |
| --- | --- |
| Research/adoption | 16 fixed repositories with commit/license/source boundaries, official Playdate docs, public-device recording analysis, and explicit adopt/reject decisions in the three `audio-*-research.md` documents |
| Portable synth | wavetable sine, triangle, PolyBLEP square, deterministic xorshift noise, precomputed exponential envelope, finite parameter rejection, minimum ramps, saturating mix, exact ending silence and four-voice priority/age/smooth-steal tests |
| Concurrency/control | fixed 16-slot SPSC FIFO, four critical slots, no producer overwrite, Navigate cooldown, full-queue overflow evidence, and out-of-band Muted/StopAll safety mailbox; a saturated 16-critical-cue queue still renders immediate zero |
| Semantic integration | Input/Action/Outcome/System expose 16 stable cues including TimerComplete; Navigate/Boundary/Confirm/Back/ToggleOn/ToggleOff/Reject routes remain covered in bundled Apps, app-open and System Menu tests；长按现在打开系统菜单，显式 Home 动作使用 Back transition |
| Headless PCM | a real Launcher turn renders 2,048 deterministic samples, hash `14994789996363689834`, nonzero bounded peak; every cue also has a 44,100-sample golden and exact silent tail |
| SDL | SDL3 3.4.12 dummy callback consumes independently of App updates, no-device failure preserves portable service, stop prevents later callbacks; real executable passes three frames at both profiles with dummy video+audio drivers |
| T-Embed adapter | host test locks GPIO 7/5/6 and right-left mono duplication; the real adapter is compiled against ESP-IDF/FreeRTOS stubs and injected with install, pin, task, partial-write and fatal-write failures; locked PlatformIO Arduino-ESP32 2.0.17 release compiles and links the independent core-0 legacy-I²S task |
| Asset workflow | all 16 cues plus four family demos export as valid 44.1 kHz, 16-bit mono RIFF/WAVE; local Select/Turn On/Turn Off references are rejected by the shared-source audit and are not build inputs |

## Activation Timer 软件验证

2026-07-19 的 `build-activation-timer-app` 结果见
[`activation-timer-implementation-report.md`](activation-timer-implementation-report.md)：
normal/strict/ASan+UBSan 各 73/73，双 profile SDL dummy、PlatformIO T-Embed、
OpenSpec/source/research/diff gates 通过。headless 和 desktop InputReducer E2E 覆盖
后台计时、System Menu Home、其他 App/transition 到期、held-button capture、确认停音
和再次开始。实体旋钮手感、屏幕、响度、疲劳与长时间运行仍待真机，不以软件结果代替。

Fresh gates after integration with the system-service foundation and the
Confirm/Back calibration fix:

- normal host: 50/50;
- AppleClang warnings-as-errors: 50/50;
- ASan+UBSan: 50/50;
- SDL dummy executable: T-Embed and Sharp profiles pass;
- PlatformIO firmware: 98,808 B RAM (30.2%), 358,805 B Flash (11.4%);
- four-voice ABI: `AudioEngine` 592 B, `InteractionSoundService` 1,728 B;
  no per-sample transcendental function or allocation; ESP32 runtime timing is
  still a physical-board measurement;
- approved-prototype correlation: Confirm 0.984379, Back 0.979893; the real App
  open/System Menu Home path renders their matching deterministic goldens;
- OpenSpec strict validation, source-manifest audit, platform-leakage audit and
  `git diff --check`: pass.

## System Overlay 软件验证

2026-07-18 的 `build-system-overlay-runtime` 软件结果：

- 参考 clone/commit/license/source/build-dependency audit：通过；
- input owner、suspend lifecycle、transition defer、typed setting、capacity/
  diagnostics tests：通过；
- 320×170 与 400×240 System Menu framebuffer golden：通过；
- desktop scripted path 访问全部 bundled App，并通过 System Menu Home 返回：通过；
- PlatformIO firmware：99,000 B RAM、361,705 B Flash；
- ESP-IDF 5.5 connectivity candidate：binary 1,188,496 B，static D/IRAM
  231,143 B；
- 实体 T-Embed 的 650 ms 手感、屏幕可读性和 100 次循环：待硬件，不能由软件门禁代替。

## Continuous Launcher 软件验证

`add-continuous-launcher-navigation` 增加以下独立证据；最终数字以该变更归档前的
fresh gate 为准：

- 逻辑目标使用无界整数、视觉位置连续追赶，并在 settled 后安全重基；单步中间态、
  首尾单 pitch 环绕、快速同向/反向 retarget、移动中打开和空目录均有自动化用例；
- Normal 与 Reduced Motion 均覆盖单调无 overshoot、精确 settled、反向 retarget
  不越界和 profile 中途切换；Normal 为 250 ms，Reduced 为 160 ms；
- Settings 的 Launcher 行通过 `SettingsWrite` command 同帧切换 Vertical/Horizontal，
  新 service session 默认恢复 Vertical；
- Launcher 快照矩阵覆盖 320×170/400×240、横纵 settled、确定中间帧、内置 Cover、
  fallback，以及按下/启动代表捕获；代表捕获必须与对应 neutral Cover 像素一致。
  交互测试另行验证 Cover 不接收状态、固定 layout bounds 与静默 viewport clip，
  避免点击变图或横向移动时内容重排；
- desktop 真实输入路径覆盖 Settings 切方向、System Menu Home/Resume、长按 release
  后稳定帧恢复、未 settled 时打开最新选择。本帧 Launcher 的 Invalid/Clipped
  geometry 计数为零；HUD 只显示本帧事件，无新事件时显示 `DIAG OK`，但历史环形
  日志仍保留供审计。

2026-07-18 fixed-ratio Cover 最终门禁证据：

- `cadenza_apps_tests`、`cadenza_launcher_snapshot_tests` 与
  `cadenza_desktop_smoke_tests` 定向门禁 3/3 通过；Timer 启动捕获与 neutral Cover
  hash 相同，Motion press 捕获与 neutral Cover hash 相同；
- host 全部 target 编译成功；使用带 Pillow 的项目 Python 时 70/70 项 CTest
  通过。八个母图来源检查证明各 profile PBM 直接来自对应 PNG，八个 Cover
  asset check 证明 packed header 与 PBM 一致；`cadenza_app_snapshot_tests` 与新增的
  `cadenza_launcher_snapshot_tests` 均已逐图审阅并批准固定比例完整画面；
- 两个 profile 的八个 PBM 均通过高分辨率母图→目标尺寸的像素级检查；T-Embed
  不再从已二值化的 350×155 PBM 二次缩小。Motion 的黑色连通块由 78 降至 56，
  ≤4 像素碎片由 43 降至 23，Settings 的对应计数由 36/17 降至 32/13；
- SDL 3.4.12 可执行文件分别以全新 dummy-driver 进程运行 320×170 与 400×240，
  `--frames 3 --overlay` 均正常退出；desktop smoke 的当前帧诊断审计通过；
- SDL 默认 `reflective` 呈现将 ink/paper 映射为 `#322F28`/`#B1AEA7`，显式
  `--palette pure` 保留 `#000000`/`#FFFFFF`；解析、精确 RGBA 值、非法 palette
  拒绝和双模式全新进程均通过。palette 不进入 framebuffer 与截图 hash；
- Retina Mac 的真实 SDL 进程报告 logical 640×340、backing 1280×680、density
  2.00x；texture 仍使用 nearest-neighbor 与 integer logical presentation；
- PlatformIO `cadenza-t-embed` release 编译成功：RAM 99,184 / 327,680 B
  (30.3%)，Flash 408,997 / 3,145,728 B (13.0%)；双 profile Cover 相比前一版
  增加 43,084 B flash，未增加静态 RAM；
- `cadenza_shared_source_audit` 与 `cadenza_portable_core_compile_probe` 2/2 通过，
  `openspec validate add-continuous-launcher-navigation --strict` 与
  `git diff --check` 通过。

反射色板只是对良好环境光下 Memory LCD 的近似，不能模拟无背光、观看角度、环境
亮度或真实黑白反差。软件证据也不能证明 T-Embed 的 30 FPS、TFT 撕裂、拖影或
高频 1-bit 相位闪烁。实体步骤见 `launcher-card-reference-research.md`。当前状态为
`fixed-ratio Cover artwork and platform gates complete / hardware motion acceptance pending`。

## Delta-spec completion audit

This is the requirement-by-requirement audit, rather than an inference from a
single broad green build:

| Delta requirement | Direct evidence | Status |
| --- | --- | --- |
| same core across platforms | one `InteractionSoundService::render` used by headless, SDL and I²S; shared-source audit and both platform builds | automated pass |
| SDL callback output/lifecycle | dummy callback/no-device/stop tests, real dummy process at both profiles, invalid audio driver exits 0 with graphics active | automated pass |
| T-Embed I²S task | configuration/frame tests, injected driver/task/write failures against the real adapter source, locked firmware link | automated pass; electrical output pending hardware |
| verification/truthful declaration | normal/strict/sanitized/SDL/firmware/OpenSpec gates plus the physical script below | automated pass; listening explicitly pending |
| fixed realtime render | idle/nonzero/end-zero tests, fixed arrays and source audit, no allocation in voice/command/sample path | automated pass |
| bounded primitives | wavetable sine, triangle, PolyBLEP edge, seeded noise, exponential/linear envelope, finite rejection/clamping and all-cue ramp tests | automated pass |
| four voices/smooth steal | priority, oldest-age selection, low-priority rejection, 64-sample release and outgoing-gain regression | automated pass |
| saturating mix | four over-gain voices reach int16 bounds without wrapping; ASan/UBSan pass | automated pass |
| fixed SPSC queue | FIFO, reserve, overflow/no-replay and saturated-critical-queue immediate mute tests | automated pass |
| semantic vocabulary | 16 stable names/definitions/goldens across four families; Runtime/App tests preserve all seven existing business routes and TimerComplete | automated pass |
| restrained directional language | per-cue duration/tone-count/ramp tests; Confirm/ToggleOn up and Back/ToggleOff down | automated pass; taste pending hardware |
| high-frequency suppression | one cue for large-turn frame, 25 ms cooldown, drop/no-later-play and post-cooldown tests | automated pass |
| volume/mute/visual equivalence | four-level cycle/order, active and saturated-queue mute, Settings immediate framebuffer change at both profiles and muted visual interaction | automated pass |

## Physical T-Embed audio acceptance script

Automation proves format, routing, timing state, deterministic PCM and firmware
linkability. It cannot prove the installed MAX98357A/speaker's pleasantness.
Use an original T-Embed in a quiet room, powered from the same source expected
for normal use:

1. Flash the verified `cadenza-t-embed` build and confirm the serial diagnostic
   `I2S audio task started`; there must be no repeated AudioUnderrun/AudioFailure.
2. At Medium, turn slowly through Launcher choices and compare selection motion
   with Navigate onset. Check every boundary, open, System Menu Home, toggle,
   About reject, and all four Settings volume states.
3. Repeat fast rotation for 10 seconds. Sound must stop when motion stops; there
   must be no delayed tick queue, display stall or watchdog reset.
4. Trigger Confirm, Back and Reject while other tails are active. Listen for
   clicks, hard cuts, integer-like wrap distortion or one channel disappearing.
5. Switch High → Muted during an active cue. The tail must stop immediately;
   all visual interactions must remain complete while muted. Restore Low and
   confirm no old cue resumes.
6. Perform at least 50 consecutive Navigate/Confirm/Back operations at Medium.
   Score semantic clarity, comfort, family consistency, sync and fatigue from
   1–5. Default acceptance requires each first four score ≥4, fatigue ≥4, and
   no click, obvious distortion or reset.
7. Repeat once with the enclosure held normally and once on a table. Record
   firmware hash, board revision, power source, volume, scores, defects and any
   selected replacement-cue version in the asset manifest/review record.

Until this script is performed, status is **automated implementation complete,
physical sound-quality acceptance pending**. Current synthesis parameters are
an editable baseline, not a claim of final product taste.

## Deliberately unexecuted P8

The following require the physical original T-Embed and remain open: encoder
direction/debounce/high-speed behavior, long-press feel, real FPS and slowest
frame, TFT tearing, simulator-to-TFT photographed pixel comparison, animation
performance budget under real transfer cost, repeated on-device regression
after effect groups, and the audio acceptance script above. Sharp hardware
driving, persistence, sample-asset playback and new Apps remain after this main
line.
