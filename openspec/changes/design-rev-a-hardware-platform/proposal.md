## Why

CadenzaOS 的软件原型已经验证了受限实体输入与反射屏的交互方向，但 T-Embed 无法提供目标产品所需的显示、音频、供电、存储和结构体验。现已确定使用 Sharp LS027B7DH01 Memory LCD，应该围绕它建立一块可验证关键风险的 Rev A 自研硬件平台。

## What Changes

- 新建 CadenzaOS Rev A 硬件平台：以 ESP32-S3 模组、LS027B7DH01、实体输入、音频、电池和本地存储为核心。
- 建立可由嘉立创 EDA 设计、审查、生产和贴装的原理图、PCB、BOM、坐标与制造文件流程。
- 设计横向掌上设备的初版机械外壳，保留正面扬声器、麦克风、USB-C、电源开关、`B` 键、滚轮与 `A` 键，并提供两种稳定的桌面支撑角度。
- 验证 LS027B7DH01 的 5V 显示电源、3V 信号域、10-pin 0.5 mm FPC 连接器和 FPC 弯折空间。
- 验证麦克风拾音、扬声器腔体、无线、待机功耗和掉电后数据持久化等关键体验。

## Capabilities

### New Capabilities

- `rev-a-hardware-platform`: 定义可制造的 Rev A 电子硬件平台、显示、电源、输入、音频、存储、无线和调试接口要求。
- `handheld-enclosure`: 定义与 PCB 协同的横向掌上外壳、屏幕安装、开孔、支撑面与可维护性要求。

### Modified Capabilities

- 无。

## Impact

- 新增 `hardware/` 设计资产、器件选型记录、生产文件与机械模型。
- 未来 ESP-IDF 硬件适配层将从 T-Embed 引脚配置迁移到 Rev A 板级定义；应用输入动作契约保持不变。
- 需要使用嘉立创 EDA 专业版、嘉立创 SMT 元件库及可生产性检查；不影响当前 T-Embed 固件的可用性。
