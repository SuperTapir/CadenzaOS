## 1. 契约与失败门禁

- [x] 1.1 增加现行产品 Clock 符号/文案/资产残留审计，并确认迁移前会失败
- [x] 1.2 增加双 profile Timer 数字几何、状态反馈关键帧和 handoff 清理失败测试
- [x] 1.3 增加 Timer Alert 相位/循环/Reduced Motion/Muted/确认失败测试
- [x] 1.4 增加 warped Menu 全屏 mask 中间帧与 fully-open 等价失败测试

## 2. Timer 完整重命名

- [x] 2.1 将 App 类型、内置 ID、host 成员、capability 映射和注册路径迁移到 Timer，保持 ID `0x0101` 且删除旧 alias
- [x] 2.2 将测试场景、snapshot/失败 PNG、诊断标签和变量迁移到 Timer
- [x] 2.3 更新架构、实现报告、开发文档与历史决策，现行命名不再保留 Clock

## 3. Timer 数字与主画面

- [x] 3.1 建立可复现的工业仪表数字母稿与双 profile 原生 1-bit atlas 生成流程
- [x] 3.2 使用 bitmap atlas 组合等宽 `MM:SS`，重排页眉、状态牌、时间体、进度和操作提示
- [x] 3.3 输出 Ready、Running、Paused 双 profile 实尺寸预览并取得用户明确批准
- [x] 3.4 批准后更新数字相关 visual golden

## 4. Timer 状态与到期反馈

- [x] 4.1 实现 Starting/Pausing/Resuming presentation elapsed、Start/Resume 正向扫光、Pause 逆向扫光与 Reduced Motion 静态反馈
- [x] 4.2 在 Timer onEnter/onExit 清理未完成反馈并保持服务与声音语义不变
- [x] 4.3 为 SystemSurfaceCoordinator 增加 Timer Alert elapsed 生命周期和只读访问
- [x] 4.4 保留大号 `TIME UP` 告警卡，扩展克制的 elapsed/MotionProfile 导轨表现并保持输入抢占与冻结
- [x] 4.5 将 TimerComplete 更新为三次清脆铃击和末次明亮和弦，把重复周期收紧到约两秒并保持 Muted 策略

## 5. Menu mask 动画

- [x] 5.1 将 warped Menu mask 改为 eased 全 framebuffer reveal，再由逐 scanline panel 覆盖
- [x] 5.2 验证 opening/closing 反向关系和 fully-open 逐像素稳定性

## 6. TIMER Launcher Cover

- [x] 6.1 生成 1400×620 前景 TIMER 遮挡后方倾斜立体倒计时盘的平滑母稿和实尺寸审阅图
- [x] 6.2 检查 350×155、280×124、灰度体积的有序抖动、reflective palette、移动 track 与禁止元素并取得用户明确批准
- [x] 6.3 批准后生成 timer PBM/header，更新 renderer、CMake、README 与 launch/return handoff
- [x] 6.4 删除旧 Clock Cover 及生成文件并通过 Cover source/immutability/snapshot/30 FPS 门禁

## 7. 完整验证与收尾

- [x] 7.1 运行完整 host suite、strict warnings、sanitizer 和 allocation/size audit
- [x] 7.2 运行 SDL smoke/E2E 与 PlatformIO T-Embed 构建
- [x] 7.3 运行全仓命名审计和 `git diff --check`，记录最终验证证据
