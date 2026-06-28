# spider 操作手册

> Mini-Kame 四足机器人 — 编译、烧录、串口/BLE/手机遥控的**一站式参考**。  
> 保存当前已验证成果（含 2026-06-25 iPad BLE 遥控、2026-06-28 XIAO BLE Sense）。

---

## 1. 当前成果概览

| 能力 | 状态 | 说明 |
|------|------|------|
| PCA9685 + 8 路舵机 | ✅ | I2C @ 0x40，50 Hz PWM |
| UART / USB Shell | ✅ | DK: J-Link 115200；XIAO: USB CDC 115200 |
| Kame Motion Framework | ✅ | 静态姿态 + 步态 + 单腿动作 |
| BLE 遥控固件 | ✅ | 广播名 `SpiderBod`，GATT `cmd` 写指令 |
| iOS/iPad App SpiderRemote | ✅ 实机验证 | iOS 15.0+；**全部 Motion + 两种 speed** |
| nRF52840 DK 平台 | ✅ 实机验证 | J-Link 烧录 + 串口 + BLE |
| Seeed XIAO BLE Sense | ✅ 实机验证 | UF2 烧录 + USB Shell + iPad BLE |
| 串口 + BLE 并行 | ✅ | 后到命令生效；断连 auto stand |

**尚未完成**：阶段 D 步态/三脚架精细调参；§9 完整测试表 T1–T9 逐项书面记录。

---

## 2. 硬件与环境

### 2.1 硬件清单

**nRF52840 DK（默认，已实机验证）**

- nRF52840 DK（J2 J-Link 口接 Mac）
- PCA9685 模块 @ I2C **0x40**
- 8× 舵机 + **外部 5–6 V** 舵机电源（勿用 DK 3.3 V 带载）
- iPhone / **iPad**（iOS 15.0+）用于 BLE 遥控

**Seeed XIAO BLE Sense（nRF52840，已实机验证）**

- Seeed XIAO BLE 或 XIAO BLE Sense
- 同上 PCA9685 + 舵机 + 外部电源
- Type-C 接 Mac（Shell：USB CDC；烧录：`make flash-uf2` 或拖放 `zephyr.uf2`）

### 2.2 接线摘要

**DK（I2C0）**

```text
nRF52840 DK           PCA9685
3V3  → VCC    P0.26 → SDA    P0.27 → SCL    GND → GND
外部 5–6 V → V+（舵机电源）
舵机信号 ← PCA9685 PWM0~15
```

**XIAO BLE（I2C1，D4/D5）**

```text
XIAO BLE              PCA9685
3V3  → VCC    D4 → SDA    D5 → SCL    GND → GND
外部 5–6 V → V+
舵机信号 ← PCA9685 PWM0~15
```

> XIAO 上 P0.26 为红灯，不可作 I2C SDA。

详见 [kame-servo-calibration.md](kame-servo-calibration.md)、[nrf52840-pca9685-wiring.md](../../../docs/knowledge/nrf52840-pca9685-wiring.md)。

### 2.3 软件环境

| 组件 | 版本 |
|------|------|
| 工作区 | `ble-projects` |
| NCS / Zephyr | v2.7.0 / 3.6.99-ncs2 |
| Zephyr SDK | 0.16.8 |
| 板型 | `nrf52840dk/nrf52840`（默认）或 `xiao_ble` |
| Mac 工具 | `make setup` 初始化 `sdk/`、`toolchains/` |

环境详情：[docs/setup.md](../../../docs/setup.md)

---

## 3. 编译与烧录

在 **ble-projects 根目录**执行。切换 `BOARD` 时使用独立 `build/<板型>/` 目录，**无需 `clean`**。

### 3.1 命令速查

| 目标 | 编译 | 烧录 |
|------|------|------|
| nRF52840 DK（默认） | `make build PROJECT=spider` | `make flash-direct PROJECT=spider` |
| XIAO BLE / Sense | `make build PROJECT=spider BOARD=xiao_ble` | `make flash-uf2 PROJECT=spider BOARD=xiao_ble` |

### 3.2 详细步骤

```bash
# --- DK ---
make build PROJECT=spider
make flash-direct PROJECT=spider
# 多块 DK：cd projects/spider && NRFUTIL_SERIAL=<序列号> make flash-direct

# --- XIAO BLE / Sense ---
make build PROJECT=spider BOARD=xiao_ble
# 1. 快速双击 XIAO RESET，直到 Mac 出现 UF2 磁盘（本机为 XIAO-SENSE）
# 2. 烧录
make flash-uf2 PROJECT=spider BOARD=xiao_ble
# 磁盘名不同：make flash-uf2 PROJECT=spider BOARD=xiao_ble UF2_VOLUME=<磁盘名>
```

**产物路径**（每块板独立 `build/<板型>/`，切换 `BOARD` 无需 `clean`）：

| 板型 | 路径 |
|------|------|
| DK | `projects/spider/build/nrf52840dk_nrf52840/zephyr/zephyr.hex` |
| XIAO | `projects/spider/build/xiao_ble/zephyr/zephyr.uf2`（及 `.hex`） |

**macOS DK 烧录失败 EAGAIN**：务必用 `flash-direct`，不要用 `make flash`。

**XIAO 勿用 `flash-direct`**：无 J-Link 时用 `flash-uf2`（或手动复制 `.uf2` 到 UF2 磁盘）。

**IDE 代码跳转**：

```bash
make compile_commands PROJECT=spider
```

---

## 4. 串口连接

```bash
# macOS，115200
# DK：J-Link 虚拟串口（设备名末尾通常为 1）
screen /dev/cu.usbmodem<序列号>1 115200
# XIAO：USB CDC（Type-C 直连，同样多为 usbmodem）
# 退出 screen：Ctrl+A 然后 K，再 Y
```

上电正常时应看到：

```text
PCA9685 driver ready.
Boot reset: Kame -> 标准站姿 (stand).
Bluetooth initialized (name: SpiderBod)
BLE advertising started (connectable)
```

---

## 5. 控制方式总览

```text
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│ UART Shell  │     │ BLE 写帧    │     │ SpiderRemote│
│ (串口)      │     │ nRF Connect │     │ iPhone/iPad │
└──────┬──────┘     └──────┬──────┘     └──────┬──────┘
       │                   │                   │
       └───────────────────┼───────────────────┘
                           ▼
                  spider_control（统一分发）
                           ▼
                  kame_motion + spider_servo
                           ▼
                     PCA9685 → 舵机
```

**互斥规则**：

- Shell 与 BLE **可同时连接**；**后到的命令生效**
- 手动舵机命令（`angle` / `spider servo` 等）会打断 Motion
- BLE **断连** → 固件自动 `stand`
- BLE **仅 1 个连接**；SpiderRemote 与 nRF Connect 不能同时连

---

## 6. 串口命令速查

### 6.1 顶层动作（无前缀，最常用）

| 命令 | 说明 |
|------|------|
| `stand` | 标准站姿 |
| `splay` | 叉腿站立 |
| `lift L1` / `L1\|R1\|L2\|R2` | 抬腿（静态） |
| `wave L1` | 招手（循环） |
| `front_down` / `rear_down` / `full_down` | 趴下 |
| `forward` / `backward` | 前进 / 后退（循环） |
| `turn_left` / `turn_right` | 弧线转向（循环） |
| `spin_left` / `spin_right` | 原地旋转（循环） |
| `nod` / `shake` / `roll` | 机身摆动（循环） |
| `speed [ms]` | 动作节拍，默认 500 ms |
| `stop` | 停止并恢复立正 |
| `angle <ch> <deg>` | 单通道角度 0–180 |

> 循环动作需 `stop` 或切换其他动作才会停。

### 6.2 `spider` 子命令（调试）

| 命令 | 说明 |
|------|------|
| `spider scan` | I2C 扫描（应见 0x40） |
| `spider check` | 读 PCA9685 MODE 寄存器 |
| `spider servo <ch> <us>` | 设脉宽 500–2500 µs |
| `spider multi <ch:deg> ...` | 多通道同步设角 |
| `spider all <deg>` | 已激活通道统一设角 |
| `spider move <ch> <deg> <ms>` | 定时匀速转动 |
| `spider speed [deg/s]` | 舵机转速，默认 240，0=瞬时 |
| `spider status` | 通道当前/目标位置 |
| `spider off <ch>` / `offall` | 关闭 PWM |
| `spider stand` / `spider splay [delta]` | 整机姿态（标定表） |

完整语义：[kame-motion-framework.md](kame-motion-framework.md)

### 6.3 两种 speed（易混，务必区分）

| 名称 | 命令 | 含义 | 默认 |
|------|------|------|------|
| **动作节拍** | `speed <ms>` | Motion 每步等待时间 | 500 ms |
| **舵机转速** | `spider speed <deg/s>` | 插值角速度 | 240 °/s |

---

## 7. BLE / 手机遥控

### 7.1 GATT 参数

| 项 | 值 |
|----|-----|
| 广播名 | `SpiderBod` |
| Service | `A0010001-0000-1000-8000-00805F9B34FB` |
| Characteristic `cmd` | `A0010002-0000-1000-8000-00805F9B34FB` |
| 写入 | Write Without Response，1–3 字节 |

### 7.2 常用 BLE 帧（hex）

| 操作 | 写入 |
|------|------|
| Stand | `01 00 FF` |
| Forward | `01 06 FF` |
| Backward | `01 07 FF` |
| Stop | `02` |
| 动作节拍 800 ms | `10 20 03` |
| 舵机转速 240 °/s | `11 F0 00` |
| Lift L1 | `01 02 00` |

协议全文：[ble-remote-design.md](ble-remote-design.md)

### 7.3 SpiderRemote App（iPhone / iPad）

| 项 | 说明 |
|----|------|
| 工程 | `projects/spider-remote-ios/` |
| 最低系统 | iOS **15.0+**（含 15.8.x；**iPad 已验证**） |
| 打开 | `open projects/spider-remote-ios/SpiderRemote.xcodeproj` |
| 部署 | Xcode 选真机 → Signing → **⌘R** |

**App 三 Tab**：

1. **遥控** — 全部 16 种 Motion + Stop（姿态 / 行走 / 趴下 / 机身 / 抬腿×4 / 招手×4）
2. **速度** — 预设 + 滑条自定义（动作节拍 100–2000 ms；舵机 0–600 °/s）
3. **连接** — 扫描并连接 `SpiderBod`

> 舵机标定、`angle`、I2C 调试等仍仅串口；BLE 覆盖全部 Motion 与两种 speed。

### 7.4 App 遥控 ↔ 串口命令对照

| App 分区 / 按钮 | 等价串口 | 循环 |
|-----------------|----------|------|
| Stand | `stand` | 否 |
| Splay | `splay` | 否 |
| Forward / Backward | `forward` / `backward` | 是 |
| Turn L / R | `turn_left` / `turn_right` | 是 |
| Spin L / R | `spin_left` / `spin_right` | 是 |
| Front / Rear / Full Down | `front_down` / `rear_down` / `full_down` | 否 |
| Nod / Shake / Roll | `nod` / `shake` / `roll` | 是 |
| Lift L1…R2 | `lift L1` … | 否 |
| Wave L1…R2 | `wave L1` … | 是 |
| Stop | `stop` | — |
| 速度预设 / 滑条 | `speed <ms>` / `spider speed <deg/s>` | — |

---

## 8. 推荐操作流程

### 8.1 首次上电检查

```text
spider scan          → 0x40
spider check
stand
```

### 8.2 串口步态试跑

```text
speed 800
forward
stop
```

### 8.3 手机遥控（iPad / iPhone）

1. 确认固件已刷入且串口有 `BLE advertising started`
2. 打开 SpiderRemote → **连接** → 扫描 **SpiderBod**
3. **遥控** → Stand → Forward → Stop
4. **速度** → 试预设节拍与转速

### 8.4 串口 + 手机并行

```text
# 手机 Forward 时
stop                 # 串口应立即打断

# 手机 Forward 后断开 BLE
                     # 机器人应 auto stand
```

---

## 9. 源码与架构速查

| 模块 | 文件 | 职责 |
|------|------|------|
| Shell | `main.c` | 命令注册、I2C 诊断 |
| 统一控制 | `spider_control.c` | Motion / Stop / Speed / BLE 帧解析 |
| 舵机后端 | `spider_servo.c` | 50 Hz 插值、`kame_servo_apply` |
| BLE | `ble_remote.c` | GATT、广播、断连回调 |
| 协议 | `spider_ble_protocol.h` | UUID、Opcode（与 iOS 对齐） |
| 动作 | `kame_motion.c` | FSM、步序表 |
| 姿态 | `kame_pose.c` | Pose 构建 |
| 标定 | `kame_servo_cal.c` | 8 路舵机参数 |

架构图：[architecture.md](architecture.md)

---

## 10. 文档索引

| 文档 | 用途 |
|------|------|
| [operations-guide.md](operations-guide.md) | **本文 — 日常操作总览** |
| [progress.md](progress.md) | 里程碑与变更日志 |
| [architecture.md](architecture.md) | 软件分层与线程 |
| [ble-remote-design.md](ble-remote-design.md) | BLE 协议与测试表 |
| [kame-motion-framework.md](kame-motion-framework.md) | 动作原理与参数 |
| [kame-servo-calibration.md](kame-servo-calibration.md) | 舵机标定数据 |
| [iOS 入门教程](../../spider-remote-ios/docs/ios-getting-started.md) | Xcode 安装与真机部署 |
| [工作区 setup.md](../../../docs/setup.md) | NCS 环境、编译烧录通用说明 |

---

## 11. 实机验证记录

| 日期 | 项目 | 结果 |
|------|------|------|
| 2026-06-25 | iPad + SpiderRemote 连接 SpiderBod 遥控 | ✅ 效果良好（Stand / 步态 / Stop / 速度预设） |
| 2026-06-25 | SpiderRemote 全 Motion + 速度滑条 | ✅ App 覆盖 BLE 全部遥控能力 |
| — | 串口 §3.2 全量回归 | 待逐项记录 |
| — | ble-remote-design §9 T1–T9 | 待逐项记录 |
