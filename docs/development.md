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
- `cadenza_core_smoke`: host-only test entry point, to be replaced by the
  vendored test framework in the next foundation task;
- `cadenza_desktop`: SDL3 host adapter; it must not become a second App/UI
  implementation.

For a core/test-only environment without SDL3, configure with
`-DCADENZA_BUILD_DESKTOP=OFF`.

`cadenza_legacy_core_is_not_portable_yet` is a temporary characterization
test: it passes only while the legacy public headers fail host compilation on
their direct Arduino/TFT dependency. The platform-decoupling milestone must
invert it into a normal successful compile test; it is not a permanent waiver.

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
