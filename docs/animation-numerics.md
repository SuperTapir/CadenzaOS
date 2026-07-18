# Animation numeric contract

Animation math uses C++ `float` on host and ESP32. Normalized easing inputs are
clamped to `[0, 1]`; every easing returns exact `0` and `1` at its endpoints.
Golden interior vectors in `cadenza/animation/golden_vectors.h` use an absolute
tolerance of `1e-5`, covering expected `libm` differences for `pow` and `sin`.

Tween arithmetic and composition tests use exact assertions for binary-exact
linear fractions and `1e-5` otherwise. Spring integration uses semi-implicit
Euler with a fixed `1/120 s` maximum substep and at most 32 substeps per update;
large excess delta is intentionally discarded to bound CPU and prevent a
backlog spiral. Settled springs snap exactly to target and zero velocity.

`src/animation_golden_probe.cpp` is compiled by the ESP32 PlatformIO target and
instantiates the same easing vectors, Tween, Sequence, and Spring templates.
Host tests execute the numerical assertions; the firmware build proves the
same C++17 surface and float operations compile with the ESP32 toolchain.
