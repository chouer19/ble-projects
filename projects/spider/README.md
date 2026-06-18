# spider

**8DOF Mini-Kame 四足机器人**固件：nRF52840 通过 I2C 驱动 PCA9685，UART Shell 交互控制 8 路舵机，内置 **Kame Motion Framework** 实现离散步态与整机动作。

---

## 快速开始

```bash
# 工作区根目录
make build PROJECT=spider
make flash-direct PROJECT=spider

# 串口（J-Link vcom0，115200）
screen /dev/cu.usbmodem<序列号>1 115200
```

上电后若 PCA9685 在线，固件**自动执行 `stand`** 复位到标准站姿。

---

## 硬件接线

```
nRF52840 DK           PCA9685
--------------------------------
3V3           ----->  VCC   (逻辑电源)
GND           ----->  GND   (共地)
P0.26 (SDA)   ----->  SDA
P0.27 (SCL)   ----->  SCL
外部 5-6V     ----->  V+    (舵机电源，勿用 DK 3.3V)
舵机信号      <-----  PWM0~15
```

PCA9685 地址 A0-A5 全接 GND → **0x40**。通道映射见 [docs/kame-servo-calibration.md](docs/kame-servo-calibration.md)。

详细连线说明：[docs/knowledge/nrf52840-pca9685-wiring.md](../../docs/knowledge/nrf52840-pca9685-wiring.md)

---

## 串口命令

命令分两层：**顶层动作命令**（无前缀，快速输入）和 **`spider` 子命令**（硬件调试）。

### 顶层动作命令（Kame Motion Framework）

```text
<动作> [腿]
```

| 命令 | 类型 | 说明 |
|------|------|------|
| `stand` | 静态 | 标准站姿 |
| `splay` | 静态 | 叉腿站立 |
| `lift <L1\|R1\|L2\|R2>` | 静态 | 抬腿并保持（先进三脚架） |
| `wave <腿>` | 循环 | 招手（膝在最高端附近摆；`stop` 恢复立正） |
| `front_down` / `rear_down` / `full_down` | 静态 | 趴下 |
| `forward` / `backward` | 循环 | 前进 / 后退 |
| `turn_left` / `turn_right` | 循环 | 弧线转向 |
| `spin_left` / `spin_right` | 循环 | 原地旋转 |
| `nod` / `shake` / `roll` | 循环 | 点头 / 摇头 / 横滚 |
| `speed [ms]` | — | 动作节拍（默认 500 ms） |
| `stop` | — | 停止并恢复立正 |

> 循环动作不会自动停止，需 `stop` 或切换其他动作。手动 `spider` 舵机命令会自动打断当前动作。

完整说明：[docs/kame-motion-framework.md](docs/kame-motion-framework.md)

### 顶层舵机命令

| 命令 | 说明 |
|------|------|
| `angle <ch> <deg>` | 设通道角度 0~180（**无 spider 前缀**） |

### `spider` 子命令（调试）

| 命令 | 说明 |
|------|------|
| `spider scan` | 扫描 I2C0 总线 |
| `spider check [addr]` | 读 PCA9685 MODE 寄存器 |
| `spider servo <ch> <us>` | 设脉宽（500~2500 µs） |
| `spider multi <ch:deg> ...` | 多通道同步设角 |
| `spider all <deg>` | 已激活通道统一设角 |
| `spider move <ch> <deg> <ms>` | 指定时长匀速转动 |
| `spider speed [deg/s]` | 舵机转速（默认 240°/s，0=瞬时） |
| `spider status` | 各通道当前/目标位置 |
| `spider off <ch>` / `spider offall` | 关闭 PWM 输出 |
| `spider stand` / `spider splay [delta]` | 整机姿态（走标定表） |

---

## 建议测试顺序

```text
# 1. 硬件连通
spider scan          # 应看到 0x40
spider check

# 2. 单舵机
angle 1 130          # L1 髋站立角
spider status

# 3. 整机姿态
stand
splay

# 4. 单腿动作（先放慢节拍）
speed 800
lift L1
stop

# 5. 步态
forward
stop
```

一键站立（8 通道）：

```text
spider multi 1:130 11:40 2:75 12:140 3:145 13:40 4:75 14:140
```

---

## 源码结构

```
projects/spider/
├── boards/nrf52840dk_nrf52840.overlay   # I2C0 + PCA9685
├── include/
│   ├── kame_servo_cal.h                 # 舵机标定 API
│   ├── kame_pose.h                      # Pose 抽象
│   └── kame_motion.h                    # Motion FSM API
├── src/
│   ├── main.c                           # Shell + 插值引擎 + 后端
│   ├── kame_servo_cal.c                 # 标定表
│   ├── kame_pose.c                      # 姿态构建
│   └── kame_motion.c                    # 步序表 + FSM
├── docs/
│   ├── architecture.md                  # 软件架构
│   ├── progress.md                      # 开发进度
│   ├── kame-motion-framework.md         # 动作框架详解
│   └── kame-servo-calibration.md        # 舵机标定记录
├── prj.conf
└── CMakeLists.txt
```

---

## 文档索引

| 文档 | 内容 |
|------|------|
| [docs/architecture.md](docs/architecture.md) | 分层架构、线程模型、扩展指南 |
| [docs/progress.md](docs/progress.md) | 里程碑、已知限制、待办 |
| [docs/kame-motion-framework.md](docs/kame-motion-framework.md) | 动作命令、步态原理、可调参数 |
| [docs/kame-servo-calibration.md](docs/kame-servo-calibration.md) | 8 路舵机标定数据与测试记录 |

---

## 实现要点

- **50 Hz 插值线程**：`angle`/`servo`/`multi` 按 `spider speed` 平滑移动；首次激活通道瞬时到位
- **Motion FSM 线程**：每 `speed <ms>` 节拍推进一个 Step，构建 Pose 并下发
- **两层速度**：`spider speed` = 舵机物理转速；顶层 `speed` = 动作节拍
- **无 IK**：离散对角步态 + 外张偏置，防同侧前后腿互撞
- **无侧向 DOF**：不支持真正横移（`shift_left/right` 已移除）

---

## 当前状态

- ✅ PCA9685 驱动 + Shell 调试完整可用
- ✅ 8 路舵机标定完成
- ✅ Kame Motion Framework：静态姿态 + 步态 + 单腿动作
- 🔄 实机步态/三脚架参数微调进行中

详见 [docs/progress.md](docs/progress.md)。
