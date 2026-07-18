# Cadenza OS 平台架构（原型 v0.1）

当前目标不是把三个 Demo 做完整，而是确定一套小应用可以长期生长的稳定边界。Demo 只负责证明平台契约确实可用。

## 运行链路

```text
T-Embed GPIO / SDL keyboard+mouse
              ↓ RawInputEvent + monotonic timestamp
          InputReducer
              ↓ InputFrame
          AppRuntime ─── lifecycle / long-press home / Transition
              ↓
Launcher / Clock / Motion / Settings / Animation Gallery
              ↓ MonoCanvas + animation/presentation core
      canonical 1-bit MonoFramebuffer
              ↓
TftPresenter / SDL3 texture / PNG+GIF / snapshot hash / future Sharp presenter
```

## 平台负责什么

- 初始化硬件和维护稳定的帧循环；
- 将旋钮和按钮统一成与具体 GPIO 无关的 `InputFrame`；
- 注册应用、维护当前应用，并调用生命周期；
- 拦截“长按返回 Launcher”这样的系统手势；
- 绘制统一的应用切换转场；
- 提供 row-major、MSB-first、`1 = black` 的 1-bit 画布，禁止应用依赖彩色
  TFT 或 SDL 特性；
- 将显示提交与应用绘制分离。

## 应用负责什么

每个内置应用只实现五个很小的接口：

```cpp
const char* name() const;
void onEnter();
void onExit();
void update(float dt, const InputFrame& input, AppRuntime& runtime);
void render(MonoCanvas& canvas);
```

应用不读取 GPIO，不直接持有屏幕，不自行实现页面切换，也不通过 `delay()` 控制节奏。状态只属于应用实例；系统可以随时离开它，再通过 `onEnter()` 重新进入。

## 关键决定

### 输入采样不绑定帧率

主循环每次迭代都采样编码器，事件累积到下一帧再交给应用。这样即使某帧传输较慢，也不会因为只在 60 FPS 的绘制点读取 GPIO 而轻易丢失旋转边沿。

### 先做静态注册

应用目前随固件编译并在 `setup()` 中注册。这个选择让生命周期、内存占用和崩溃边界更容易验证。SD 卡资源、脚本应用和动态加载等到平台契约稳定后再做。

### 唯一权威画面是 1-bit framebuffer

T-Embed 虽然是彩屏，应用拿到的仍然只有黑白图元接口。U8g2 的精选 raster
与字体代码只写入 caller-owned framebuffer；TFT、SDL、PNG、GIF 和测试都只
读取同一份像素。Sharp profile 已在桌面和快照中验证，未来只需增加 presenter，
不再替换画布或 App 代码。

### 两块屏幕使用各自的原生分辨率

T-Embed 后端使用 320 × 170，Sharp 后端使用 400 × 240。平台不对整幅画面做非等比缩放；应用从 `MonoCanvas` 读取实际宽高，按区域、边距和锚点响应式布局。这样既不会把 Sharp 的像素构图锁死在临时硬件上，也能充分利用 T-Embed 的整块屏幕。

### 动画与内存边界

Tween、Sequence、Parallel、Timeline、Spring、粒子和状态机都使用值语义或
固定容量，不在逐帧路径分配。SDL3、stb 和 gif-h 只属于 desktop adapter；
core 不包含 Arduino、TFT_eSPI 或 SDL header。转场采用明确的 source/
destination framebuffer 所有权，代价见 [`memory-budget.md`](memory-budget.md)。

## P8 之后的平台工作

1. 增加应用级持久化存储命名空间；
2. 根据真机测量决定是否复用 Gallery/Runtime 的离屏 framebuffer；
3. 实现 Sharp presenter，并在实体屏上对比 400×240 快照；
4. 定义声音、休眠、背光/对比度等系统服务，而不是让应用直接操作硬件；
5. 声音、持久化与新应用放在当前平台解耦/动画主线之后。
