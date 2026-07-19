# Cadenza development

## macOS prerequisites

The desktop simulator requires CMake and SDL3. Install them with Homebrew:

```bash
HOMEBREW_NO_INSTALL_CLEANUP=1 brew install cmake sdl3
```

PlatformIO remains the firmware build system. Use the existing repository-local
PlatformIO environment described in `README.md`.

## Configure, build, and test

```bash
cmake -S . -B build/host -DCMAKE_BUILD_TYPE=Debug
cmake --build build/host
ctest --test-dir build/host --output-on-failure
./build/host/cadenza_desktop
```

For the usual simulator development loop, use the repository's dependency-free
helper:

```bash
./tools/simulator.py dev --profile t-embed --scale 2 --overlay
```

It watches desktop/core C, C++, headers, and CMake inputs. A successful
incremental build restarts the SDL process; a failed build leaves the previous
process alive and waits for the next save. This is process-level live reload,
not in-process C++ hot module replacement, so runtime state resets after a
reload. Other commands are `build`, `run`, and `package`. The package command
creates a self-contained, ad-hoc-signed macOS app and zip under `dist/`; it is a
development package rather than a notarized release.

The top-level targets have deliberately narrow ownership:

- `cadenza_core`: standard C++17 shared runtime code;
- `cadenza_host`: deterministic, window-free composition root and replay;
- doctest executables: focused unit/integration/snapshot regression targets;
- `cadenza_desktop`: SDL3 host adapter; it must not become a second App/UI
  implementation.

For a core/test-only environment without SDL3, configure with
`-DCADENZA_BUILD_DESKTOP=OFF`.

`cadenza_portable_core_compile_probe` includes the public core headers in a
normal host target. `cadenza_shared_source_audit` also rejects Arduino,
TFT_eSPI, SDL, GPIO, `millis`, `micros`, and `Serial` leakage from the portable
core/host tree and verifies that CMake and PlatformIO discover the same core
sources.

## Desktop controls

- mouse wheel or Left/Right: encoder rotation;
- Space or Enter: button (tap for click, hold for system Home);
- F1: debug HUD;
- F2: pause/resume;
- F3: one exact fixed step while paused;
- F4: fixed-step/real-step mode;
- F5: cycle 0.25×, 0.5×, 1×, 2×;
- F6: lossless PNG screenshot;
- F7: start/stop PNG sequence plus convenience GIF recording.

Clock / Activation Timer 中，Left/Right 或滚轮按整分钟调整，Enter/Space 短按依
Ready/Running/Paused 状态执行开始、暂停或继续。运行中长按仍打开 System Menu；
返回 Launcher 后 Timer 继续，并显示向上取整的后台分钟指示。到期 alert 捕获按钮，
短按确认后回到最近时长。软件暂停/倍速只影响 presentation；desktop Timer 使用
SDL 单调 wall time。

Useful launch options are `--profile t-embed|sharp`, `--scale 1..4`,
`--palette reflective|pure`, `--overlay`, `--device-frame`, and `--frames N`.
`reflective` is the default and maps 1-bit ink/paper to `#322F28`/`#B1AEA7`,
matching the two-color palette extracted from the Playdate SDK 1.12.3 design
reference captures. `pure` maps to `#000000`/`#FFFFFF`. This is presentation
only: lossless screenshots, recordings, framebuffer hashes, headless output,
and firmware remain canonical 1-bit. `--frames` is used by the SDL dummy-driver
smoke gate.

On macOS the SDL window requests a high-pixel-density backing surface. Startup
prints logical window size, backing pixel size, and density; a Retina display
should normally report `2.00x`. The renderer still uses integer logical
presentation and nearest-neighbor texture scaling, so Retina adds backing
detail without smoothing the emulated framebuffer pixels.

The headless PBM dumper supports reproducible Gallery visual review without an
SDL window. App id `4` is Gallery; the first trailing number selects its page.
The `auto` form advances fixed 1/60 s frames, while the shorter form enters
Scrub and applies knob steps of 5% each:

```bash
./build/host/cadenza_dump_headless_pbm 400 4 /tmp/particles.pbm 11 auto 90
./build/host/cadenza_dump_headless_pbm 400 4 /tmp/camera-scrub.pbm 10 1
```

## Launcher and Cover workflow

Launcher uses a continuous cyclic track. Left/Right or the wheel changes the
logical selection immediately; a click opens that selection even while the
track is still moving. Normal motion uses a monotonic 250 ms out-cubic settle;
Reduced Motion shortens it to 160 ms. Neither profile overshoots or pulls the
card back after it reaches center. Open Settings, move to `LAUNCHER`, and click to switch
between session-only `VERTICAL` and `HORIZONTAL`. Like the current Sound and
Motion settings, this value resets when a new service session starts; it is not
persisted to disk or NVS.

Each App may implement the const `renderLauncherCover` hook. The renderer gets
only fixed full-card bounds: Cover is a static, non-interactive identity image,
so selection, press, long press, launch, time, and App lifecycle state are not
inputs and must not change its pixels. Launcher only translates and clips the
fixed scene to the visible track viewport; a Cover must never re-layout against
the changing visible slice. A future launch animation is a separate resource,
not another Cover state. Apps without a Cover use the bounded-name fallback.

The four built-in Covers use complete fixed-ratio packed bitmaps: 350x155 for
Sharp and 280x124 for T-Embed. Both are generated directly from the same
high-resolution PNG master at their target size; never resize an already
thresholded PBM, because that fragments thin geometry. Horizontal and vertical
Launcher layouts use the same image size for a profile; only the track axis and
pitch change. Editable PNG sources, canonical PBMs, generation provenance, and
rebuild commands live in `assets/launcher-covers/`.

Run the focused visual and interaction gates with:

```bash
cmake --build build/host --target \
  cadenza_apps_tests cadenza_launcher_snapshot_tests \
  cadenza_desktop_smoke_tests
./build/host/cadenza_apps_tests
./build/host/cadenza_launcher_snapshot_tests
./build/host/cadenza_desktop_smoke_tests
```

Snapshot failures write inspectable PNGs for both framebuffer profiles. Review
the deterministic midpoint as an image sequence, not only as hashes. The F1
HUD now displays only diagnostics emitted in the current frame (`DIAG OK` when
none); the diagnostic ring still retains history for tests and debugging.
Reference evidence, adopted/rejected choices, and the physical 30 FPS/1-bit
acceptance script are in `docs/launcher-card-reference-research.md`.

## Interaction sound workflow

Apps call semantic `SoundCue` values only. The authored synthesis palette lives
in `lib/cadenza_core/src/sound_cue_library.cpp`; changing it does not require
editing Apps, Runtime, SDL, or I²S code. Export the current palette as standard
44.1 kHz mono WAV files for listening or A/B comparison:

```bash
cmake --build build/host --target cadenza_dump_sound_cues
./build/host/cadenza_dump_sound_cues --all /tmp/cadenza-cues
./build/host/cadenza_dump_sound_cues confirm /tmp/confirm.wav
```

The exporter writes all 15 Input/Action/Outcome/System cues and four family
context demos. It is an audition aid, not an aesthetic approval test. A PCM
golden is updated only after a sound is deliberately accepted.

For externally generated or recorded replacements, keep the lossless master
and follow `docs/audio-asset-contract.md`. The preferred handoff is 48 kHz,
24-bit mono WAV plus cue name and provenance/rights metadata. Do not manually
convert a master to embedded PCM or normalize it to an invented universal dB
target; the future importer owns deterministic platform conversion and Event
gain, and the final decision requires in-context T-Embed listening.

Settings cycles the session volume `Medium → High → Muted → Low → Medium`.
Audio output failure is nonfatal: the graphics/input runtime continues and the
adapter emits a diagnostic. Use the SDL dummy driver to test callback ownership
without a physical device:

```bash
SDL_AUDIODRIVER=dummy SDL_VIDEODRIVER=dummy \
  ./build/host/cadenza_desktop --profile t-embed --frames 3
```

The desktop simulator uses the same portable connectivity service with a fake
adapter; it never opens a real network connection. Press `F8` to request Wi-Fi,
`F9` to inject a link loss, `F10` to toggle BLE advertising, `F12` to toggle BLE
scanning, and `F11` to start then complete a Security 2 provisioning session.
The `F1` overlay shows the resulting radio, role, failure, and provisioning
states.

## Red-green-refactor loop

The checked-in command wrapper keeps local evidence repeatable:

```bash
tools/check.sh focused vendor_smoke  # red/green for one behavior
tools/check.sh host                  # all host tests
tools/check.sh desktop               # SDL3 target and launch smoke
tools/check.sh firmware              # PlatformIO T-Embed compile
tools/check.sh diff                  # whitespace/error audit
tools/check.sh all                   # complete current matrix
```

Candidate ESP-IDF 5.5 UAC builds live in an isolated, reproducible spike and
never patch the installed PlatformIO Arduino framework:

```bash
cd .research/spikes/uac_idf
export IDF_PATH=/path/to/esp-idf-v5.5
export ESP_IDF_VERSION=5.5
idf.py -B build-idf-arduino-composite build size

# Build the complete Runtime/display/encoder/I2S0/I2S1/UAC candidate;
# do not flash without an original T-Embed.
idf.py -B build-idf-hardware \
  -DSDKCONFIG="$PWD/sdkconfig.hardware" \
  -DSDKCONFIG_DEFAULTS='sdkconfig.defaults;sdkconfig.hardware.defaults' build size

# Build the complete hardware candidate plus Wi-Fi/NimBLE/Security 2.
# Use -j1 on a memory-constrained runner.
idf.py -B build-idf-connectivity \
  -DSDKCONFIG="$PWD/sdkconfig.connectivity" \
  -DSDKCONFIG_DEFAULTS='sdkconfig.defaults;sdkconfig.hardware.defaults' build size
```

The component versions and registry hashes are locked in `dependencies.lock`.
If configuration or dependency resolution fails, fix the isolated environment
or lock; modifying `/Users/tapir/.platformio/packages/framework-arduinoespressif32`
is not an accepted recovery path. The hardware-enabled build is compile-only
until the display/input, ES7210/I²S concurrency and macOS acceptance matrix has
been executed.

ESP-IDF 5.5 的 BLE provisioning transport 自行拥有 NimBLE host。连接 candidate
因此让普通 App BLE 与 provisioning BLE 排他运行；不要在未重做 transport ownership
之前从 App 绕过 system service 同时初始化第二个 NimBLE host。Security 2 material
必须由 privileged platform hook 注入，默认 weak hook 会安全失败。

Write or expose the failing case first, run the focused command and retain its
failure reason, implement the smallest complete behavior, then run focused and
host checks. Run desktop/firmware checks whenever an adapter or shared source
list changes; run `all` before a WIP milestone commit.

The firmware command defaults to `../.platformio-env/bin/pio`. Override it with
the task-specific `CADENZA_PIO` environment variable when PlatformIO is installed
elsewhere.

Snapshot hash failures write inspectable PNGs under the active build
directory's `snapshot-failures/` folder. Accepted hashes and capture points are
documented in `tests/snapshots/`.

Strict and sanitizer verification can use disposable build directories:

```bash
cmake -S . -B /tmp/cadenza-strict -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wno-deprecated-declarations"
cmake --build /tmp/cadenza-strict --parallel
ctest --test-dir /tmp/cadenza-strict --output-on-failure

cmake -S . -B /tmp/cadenza-sanitize \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build /tmp/cadenza-sanitize --parallel
ctest --test-dir /tmp/cadenza-sanitize --output-on-failure
```
