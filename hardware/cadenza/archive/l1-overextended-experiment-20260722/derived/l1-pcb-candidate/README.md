# L1 未布线 PCB 同步候选

状态：**真实 94 器件/焊盘网络同步候选；未布线、未冻结、不可下单。**

这个目录从受保护的 77-footprint 参考 PCB 生成两个派生副本：

- 删除 9 个旧 TFT/背光/滑动开关封装；
- 加入正式 L1 原理图的 26 个新器件；
- `J_DISP1` 暂用双面接触 Hirose FH34SRJ 候选，分别输出 Pin 1 旋转 0°/180° 两板；
- `SW_PWR1` 使用已按厂家图核对的 `GT-TC020A-H035-L1` 候选；
- USB1 向右平移 16.2 mm，R1/R2 CC 下拉移到接口右侧，电路拓扑不变；
- 旧显示、Power/Lock 改线和 USB 移位涉及的旧铜线已删除，等待重新布线；
- 未改写 `working-base`。

生成：

```sh
/Applications/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3 \
  hardware/cadenza/derived/l1-pcb-candidate/generate_l1_pcb_candidate.py
```

输出板：

- `Cadenza-L1-fpc-pin1-rotation-0.kicad_pcb`
- `Cadenza-L1-fpc-pin1-rotation-180.kicad_pcb`

这两个文件的意义是让后续布线、DRC 和 3D 检查有真实板对象，不代表两个方向都能生产。
真实屏幕/转接板证据到位后必须删除错误方向，只保留一个正式 L1 PCB。

## 当前核验边界

- 生成器连续运行两次时，两块输出板的 SHA-256 分别保持一致；
- KiCad 10 可读取并生成正反面 3D 预览；
- 独立同步核验为 33/33；
- 当前 DRC 为 rotation 0：194 条违规/56 个未连接，rotation 180：194/56；
- 两板跨网短路均为 0，新增器件庭院重叠为 0；
- 详细分类见 `DRC_TRIAGE.md`。

因此 `PASS_SYNC_CANDIDATE` 只表示器件和网络已同步到可继续工作的 PCB 对象，绝不
表示 DRC 通过、布线完成或可交给嘉立创生产。
