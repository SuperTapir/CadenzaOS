# L1 PCB 放置可行性候选

状态：**未冻结；未创建 PCB；不可用于下单。**

这个目录只回答一个问题：在保留参考板 ESP32-S3 模组、天线、板框和四孔的前提下，
Sharp 屏幕 FPC、USB-C 与 Power/Lock 是否存在不互相打架的平面位置。

当前证据显示：

- Sharp FPC 向 `+Y`（上边）时与 ESP32 天线区域重叠；不采用。
- 屏幕在平面内旋转 180°、FPC 向 `-Y`（下边）时与原 USB-C 重叠。
- 保持 USB 电路拓扑不变，把 `USB1` 沿下边向右平移 `16.2 mm` 后，FPC keepout、USB1、
  U3 与其他保留封装之间没有平面重叠。
- USB-C 的 `R1/R2`（CC1/CC2 5.1 kΩ 下拉）随接口移到其右侧、U3 下方；拓扑和值不变，
  与 USB 外形约 1.4 mm、与 U3 约 3.37 mm 平面间距。
- Power/Lock 的 top-edge-A 与保留的 `KEY2` 平面重叠，明确排除；right-edge-B 无保留
  封装平面碰撞，作为当前主候选，最终仍需按钮帽、导向和后壳实物验证。
- FPC 暂以双面接触 Hirose `FH34SRJ-10S-0.5SH(50)` 做布局包络；它只能消除触点面
  依赖，不能消除 Pin 1 方向，因此 PCB 仍保留 0°/180° 两个候选。

可视化见 `placement-feasibility.svg`，机器证据见 `placement-feasibility.json`。

运行：

```sh
/Applications/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3 \
  hardware/cadenza/derived/l1-pcb-placement/analyze_l1_placement.py
```

这个检查只比较 XY 包络，不证明 PCB 两面的 Z 高度、连接器开盖空间、FPC 插拔和弯折、
USB 外壳开孔、90 Ω 差分几何或完整闭壳。下一阶段应先生成 USB 平移后的非冻结 PCB
副本，再检查走线、地回流、3D 与外壳端口。
