# Launcher Covers

`clock.pbm`、`motion.pbm`、`settings.pbm` 与 `gallery.pbm` 是四个内置
App 经人工确认后的 canonical 350×155、1-bit 封面。`*_t_embed.pbm` 是
不裁切内容、等比缩小到 280×124 的 T-Embed 版本。它们以屏幕像素为
最终审阅单位，不在设备运行时解码 PNG、缩放或分配临时 framebuffer。

`source/` 保存四张清理后的原始 PNG，用于未来重新调阈值、支持新尺寸或
重新导出；固件不直接编译这些 PNG。

源插图由用户提供并经 AI 辅助精简；2026-07-18 使用 Pillow 12.2.0 的
LANCZOS `fit` 裁切到 350×155，再按各图注释中的固定灰度阈值转为 PBM。
PBM 是后续构建和审计的 canonical source，避免不同图片库版本改变固件
像素。

生成 packed headers：

```sh
python3 tools/pack_bitmap.py assets/launcher-covers/clock.pbm \
  lib/cadenza_apps/src/generated/clock_cover.h \
  --symbol kClockCover \
  --provenance "Cadenza original AI-assisted artwork, user-approved 2026-07-18; canonical PBM in assets/launcher-covers/clock.pbm"
```

`motion`、`settings`、`gallery` 以及对应的 `*_t_embed` 使用同样命令与
对应符号。CMake host tests 会使用 `--check` 验证八个 header 与 PBM
逐字节一致。
