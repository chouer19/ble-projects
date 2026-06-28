# spider

**8DOF Mini-Kame 四足机器人**固件：nRF52840 通过 I2C 驱动 PCA9685，UART Shell + **BLE 遥控**控制 8 路舵机，内置 **Kame Motion Framework** 实现离散步态与整机动作。

手机遥控 App：**SpiderRemote**（`projects/spider-remote-ios/`），连接广播名 **SpiderBod**。支持 **iPhone / iPad**（iOS 15.0+）；iPad BLE 遥控已于 2026-06-25（DK）、2026-06-28（XIAO BLE Sense）实机验证。

**日常操作总览**：[docs/operations-guide.md](docs/operations-guide.md)（编译、烧录、串口/BLE/App 命令）

---

## 快速开始

### 固件

```bash
# 工作区根目录 — 默认 nRF52840 DK
make build PROJECT=spider
make flash-direct PROJECT=spider

# Seeed XIAO BLE（nRF52840）
make build PROJECT=spider BOARD=xiao_ble
# 双击 RESET → 出现 XIAO-SENSE 磁盘后：
make flash-uf2 PROJECT=spider BOARD=xiao_ble

# 串口 Shell
# DK：J-Link vcom0，115200
screen /dev/cu.usbmodem<序列号>1 115200
# XIAO：USB CDC（Type-C 直连 Mac）
screen /dev/cu.usbmodem<序列号>1 115200
```

上电后若 PCA9685 在线，固件**自动执行 `stand`** 复位到标准站姿，并开始 BLE 可连接广播（设备名 `SpiderBod`）。

### 手机遥控（首次请读教程）

完整步骤（Xcode 安装、Apple ID 签名、真机部署、联调测试）：

**[projects/spider-remote-ios/docs/ios-getting-started.md](../spider-remote-ios/docs/ios-getting-started.md)**

```bash
open projects/spider-remote-ios/SpiderRemote.xcodeproj
```

---

## 硬件接线

同一套 `src/` 支持多块 nRF52840 开发板，引脚差异由 `boards/*.overlay` 描述。

### nRF52840 DK（默认）

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

板型：`nrf52840dk/nrf52840` · overlay：`boards/nrf52840dk_nrf52840.overlay`

### Seeed XIAO BLE（nRF52840）

```
XIAO BLE              PCA9685
--------------------------------
3V3           ----->  VCC
GND           ----->  GND
D4 (SDA)      ----->  SDA   (P0.4)
D5 (SCL)      ----->  SCL   (P0.5)
外部 5-6V     ----->  V+
舵机信号      <-----  PWM0~15
```

> **勿将 P0.26 作 SDA** — XIAO 上 P0.26 为板载红灯。

板型：`xiao_ble` 或 `xiao_ble/nrf52840/sense` · overlay：`boards/xiao_ble.overlay`

PCA9685 地址 A0-A5 全接 GND → **0x40**。通道映射见 [docs/kame-servo-calibration.md](docs/kame-servo-calibration.md)。

详细连线说明（DK）：[docs/knowledge/nrf52840-pca9685-wiring.md](../../docs/knowledge/nrf52840-pca9685-wiring.md)

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

## BLE 遥控

| 项 | 值 |
|----|-----|
| 广播名 | `SpiderBod` |
| Service UUID | `A0010001-0000-1000-8000-00805F9B34FB` |
| Characteristic `cmd` | `A0010002-0000-1000-8000-00805F9B34FB` |
| 写入方式 | Write Without Response（1–3 字节小端帧） |

**SpiderRemote App** 已通过 BLE 覆盖全部 **16 种 Motion** + Stop，以及两种 speed（预设 + 自定义）。`angle` / `spider` 调试命令仍仅串口。

常用 hex（nRF Connect 自测）：

| 操作 | Hex |
|------|-----|
| Forward | `01 06 FF` |
| Stop | `02` |
| 动作节拍 800 ms | `10 20 03` |
| 舵机转速 240 °/s | `11 F0 00` |

Shell 与 BLE **后到的命令生效**；BLE 断连自动立正。协议详见 [docs/ble-remote-design.md](docs/ble-remote-design.md)。

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

# 6. BLE（可选，需 nRF Connect 或 SpiderRemote App）
# nRF Connect 向 cmd 写 01 06 FF → 应开始前进
```

### BLE + 串口并行联调

```text
# 手机点 Forward 的同时，串口观察：
stop                  # 应立刻打断 BLE forward

# 或串口 forward，手机点 Stop
```

测试清单见 [docs/ble-remote-design.md](docs/ble-remote-design.md) §9。

一键站立（8 通道）：

```text
spider multi 1:130 11:40 2:75 12:140 3:145 13:40 4:75 14:140
```

---

## 源码结构

```
projects/spider/
├── boards/
│   ├── nrf52840dk_nrf52840.overlay    # DK：I2C0 P0.26/P0.27
│   ├── xiao_ble.overlay               # XIAO BLE：I2C1 D4/D5
│   ├── xiao_ble.conf                  # XIAO USB CDC Shell
│   ├── xiao_ble_nrf52840_sense.overlay
│   └── xiao_ble_nrf52840_sense.conf
├── include/
│   ├── kame_servo_cal.h
│   ├── kame_pose.h
│   ├── kame_motion.h
│   ├── spider_ble_protocol.h          # BLE UUID / Opcode
│   ├── spider_control.h
│   ├── spider_servo.h
│   └── ble_remote.h
├── src/
│   ├── main.c                         # Shell 薄封装 + main()
│   ├── spider_control.c               # 统一命令分发
│   ├── spider_servo.c                 # 舵机插值后端
│   ├── ble_remote.c                   # BLE GATT
│   ├── kame_servo_cal.c
│   ├── kame_pose.c
│   └── kame_motion.c
├── docs/
│   ├── architecture.md
│   ├── progress.md
│   ├── ble-remote-design.md
│   ├── operations-guide.md            # 操作手册（编译/烧录/遥控）
│   ├── kame-motion-framework.md
│   └── kame-servo-calibration.md
├── prj.conf
└── CMakeLists.txt

projects/spider-remote-ios/            # iOS 遥控 App
└── docs/ios-getting-started.md        # 真机部署教程
```

---

## 文档索引

| 文档 | 内容 |
|------|------|
| [docs/operations-guide.md](docs/operations-guide.md) | **操作手册**：编译烧录、命令、BLE、App |
| [docs/architecture.md](docs/architecture.md) | 分层架构、线程模型、扩展指南 |
| [docs/progress.md](docs/progress.md) | 里程碑、已知限制、待办 |
| [docs/kame-motion-framework.md](docs/kame-motion-framework.md) | 动作命令、步态原理、可调参数 |
| [docs/kame-servo-calibration.md](docs/kame-servo-calibration.md) | 8 路舵机标定数据与测试记录 |
| [docs/ble-remote-design.md](docs/ble-remote-design.md) | BLE 协议与实施阶段 |
| [iOS 入门教程](../spider-remote-ios/docs/ios-getting-started.md) | Xcode、签名、真机部署与联调 |

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
- ✅ 固件 BLE 遥控（`SpiderBod`）+ SpiderRemote 全 Motion / 全 speed
- ✅ **iPad 真机 BLE 遥控验证通过**（2026-06-25）
- 🔄 实机步态/三脚架参数微调进行中
- ⏳ §9 完整测试表 T1–T9 书面记录

详见 [docs/progress.md](docs/progress.md)。
