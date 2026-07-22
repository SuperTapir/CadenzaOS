# Cadenza Display FPC Variant Library

这是 Sharp LS027B7DH01 的 10P、0.5 mm FPC **隔离候选封装库**。它不会被主工程自动加载，也不表示已选定连接器。

包含：

- bottom contact：Hirose `FH12-10S-0.5SH(55)` / `C506791`
- top contact：Hirose `FH12A-10S-0.5SH(55)` / `C3168239`
- top + bottom dual contact：Hirose `FH34SRJ-10S-0.5SH(50)` / `C324723`

## 在隔离 KiCad 工程中加载

把 `fp-lib-table.example` 的库条目复制到测试工程的 `fp-lib-table`。示例中的 `${KIPRJMOD}` 假设测试工程位于 CadenzaOS 仓库根；若不是，请改成该 `.pretty` 目录的绝对路径或合适的环境变量路径。

对于两个代理 3D 模型，在 KiCad Preferences → Configure Paths 添加：

```text
CADENZA_DISPLAY_FPC_3D_DIR = /Users/tapir/Development/CadenzaOS/hardware/cadenza/libraries/display-fpc-variants/3dmodels
```

FH12 bottom 的精确模型直接使用 `${KICAD10_3DMODEL_DIR}`。

## 重复生成与检查

```sh
PYTHONPATH=/Users/tapir/.cache/cadenza-cad-tools \
  /Users/tapir/.local/share/uv/python/cpython-3.12.11-macos-aarch64-none/bin/python3.12 \
  hardware/cadenza/libraries/display-fpc-variants/generate_library.py

python3 hardware/cadenza/libraries/display-fpc-variants/verify_library.py

/Applications/KiCad.app/Contents/MacOS/kicad-cli fp upgrade \
  hardware/cadenza/libraries/display-fpc-variants/Cadenza_Display_FPC_Variants.pretty \
  --output /tmp/cadenza-display-fpc-kicad-parse --force
```

自动检查核对 1–10 焊盘号、Pitch、Pad 1 端点、焊盘尺寸、固定焊片和 3D 路径。KiCad CLI 的临时 upgrade 是语法解析门禁，不会写回候选库。

完整证据与限制见 [PIN1_AND_DIMENSION_AUDIT.md](PIN1_AND_DIMENSION_AUDIT.md)。
