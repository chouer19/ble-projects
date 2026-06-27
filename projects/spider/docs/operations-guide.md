# spider 操作手册

> Mini-Kame 四足机器人 — 编译、烧录、串口/BLE/手机遥控的**一站式参考**。  
> 保存当前已验证成果（含 2026-06-25 iPad 真机 BLE 遥控）。

---

## 1. 当前成果概览

| 能力 | 状态 | 说明 |
|------|------|------|
| PCA9685 + 8 路舵机 | ✅ | I2C @ 0x40，50 Hz PWM |
| UART Shell 调试 | ✅ | 115200，Tab 补全 |
| Kame Motion Framework | ✅ | 静态姿态 + 步态 + 单腿动作 |
| BLE 遥控固件 | ✅ | 广播名 `SpiderBod`，GATT `cmd` 写指令 |
| iOS/iPad App SpiderRemote | ✅ 实机验证 | iOS 15.0+，CoreBluetooth |
| 串口 + BLE 并行 | ✅ | 后到命令生效；断连 auto stand |

**尚未完成**：阶段 D 步态/三脚架精细调参；§9 完整测试表 T1–T9 逐项书面记录。

---

## 2. 硬件与环境

### 2.1 硬件清单

- nRF52840 DK（J2 J-Link 口接 Mac）
- PCA9685 模块 @ I2C **0x40**
- 8× 舵机 + **外部 5–6 V** 舵机电源（勿用 DK 3.3 V 带载）
- iPhone / **iPad**（iOS 15.0+）用于 BLE 遥控

### 2.2 接线摘要

```text
nRF52840 DK           PCA9685
3V3  → VCC    P0.26 → SDA    P0.27 → SCL    GND → GND
外部 5–6 V → V+（舵机电源）
舵机信号 ← PCA9685 PWM0~15
```

详见 [kame-servo-calibration.md](kame-servo-calibration.md)、[nrf52840-pca9685-wiring.md](../../../docs/knowledge/nrf52840-pca9685-wiring.md)。

### 2.3 软件环境

| 组件 | 版本 |
|------|------|
| 工作区 | `ble-projects` |
| NCS / Zephyr | v2.7.0 / 3.6.99-ncs2 |
| Zephyr SDK | 0.16.8 |
| 板型 | `nrf52840dk/nrf52840` |
| Mac 工具 | `make setup` 初始化 `sdk/`、`toolchains/` |

环境详情：[docs/setup.md](../../../docs/setup.md)

---

## 3. 编译与烧录

在 **ble-projects 根目录**执行：

```bash
# 编译 spider 固件
make build PROJECT=spider

# 烧录（连接 DK J2，打开 SW8 电源）
make flash-direct PROJECT=spider

# 多块板时指定序列号
cd projects/spider && NRFUTIL_SERIAL=<贴纸序列号> make flash-direct
```

**产物路径**：`projects/spider/build/zephyr/zephyr.hex`

**macOS 烧录失败 EAGAIN**：务必用 `flash-direct`，不要用 `make flash`。

**IDE 代码跳转**：

```bash
make compile_commands PROJECT=spider
```

---

## 4. 串口连接

```bash
# macOS（J-Link 虚拟串口，115200）
screen /dev/cu.usbmodem<序列号>1 115200
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

1. **遥控** — Stand / Forward / Backward / Turn / Spin / Splay / Stop（点按发一条）
2. **速度** — 动作节拍 300/500/800 ms；舵机转速 120/240/360 °/s
3. **连接** — 扫描并连接 `SpiderBod`

首次安装教程：[iOS 入门教程](../../spider-remote-ios/docs/ios-getting-started.md)

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
| — | 串口 §3.2 全量回归 | 待逐项记录 |
| — | ble-remote-design §9 T1–T9 | 待逐项记录 |
