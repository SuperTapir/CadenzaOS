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

Useful launch options are `--profile t-embed|sharp`, `--scale 1..4`,
`--overlay`, `--device-frame`, and `--frames N`. The last option is used by the
SDL dummy-driver smoke gate.

The headless PBM dumper supports reproducible Gallery visual review without an
SDL window. App id `4` is Gallery; the first trailing number selects its page.
The `auto` form advances fixed 1/60 s frames, while the shorter form enters
Scrub and applies knob steps of 5% each:

```bash
./build/host/cadenza_dump_headless_pbm 400 4 /tmp/particles.pbm 11 auto 90
./build/host/cadenza_dump_headless_pbm 400 4 /tmp/camera-scrub.pbm 10 1
```

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
