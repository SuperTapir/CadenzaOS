# System Menu Mask Transition Specification

## Purpose
定义 System Menu 开合时对底层 App 的冻结、遮罩、方向性形变、输入捕获与端点释放契约。

## Requirements

### Requirement: Warped Menu mask reveals across the full framebuffer
warped Menu SHALL 在合成 panel 前，以 eased reveal 对整个 framebuffer 应用遮罩；逐 scanline 展开的 panel SHALL 覆盖并侵占其到达区域。

#### Scenario: Panel 尚未到达右侧区域
- **WHEN** Opening 处于代表性中间进度且某些 scanline 的 panel 尚未到达右侧
- **THEN** 该右侧区域已经具有非零 mask 覆盖，而不是保持未遮罩背景

### Requirement: Menu opening and closing are visual inverses
Opening SHALL 从无 mask/收起 panel 过渡到稳定菜单；Closing SHALL 沿同一几何和 mask coverage 完全反向。

#### Scenario: 对称进度
- **WHEN** 比较 Opening 进度 `p` 与 Closing 对应的反向进度
- **THEN** mask coverage 与 panel 占用遵循镜像时间关系且不存在固定半屏遮罩跳变

### Requirement: Fully open Menu remains pixel stable
warped Menu 在完全展开时 SHALL 与既有稳定菜单逐像素相等，左半屏最终 mask 密度不得改变。

#### Scenario: 完全展开端点
- **WHEN** warped Menu 以完成进度绘制
- **THEN** framebuffer 与稳定 Menu renderer 逐像素相等
