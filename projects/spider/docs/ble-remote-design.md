# spider BLE 遥控 — 设计规格

> 手机（iOS）通过蓝牙连接 nRF52840 DK，按钮发送指令遥控 Mini-Kame 四足机器人。  
> 本文档为**实施前设计规格**，开发时以本文为准。

最后更新：2026-06-25

---

## 1. 目标与约束

### 1.1 目标

- iOS App 通过 BLE **点按按钮**发送指令，行为与当前 UART Shell **单次输入一条命令**一致。
- 固件新增 BLE 遥控能力，**不替换**现有串口调试；手机联调时仍可用串口观察/对比。
- 玩具级产品：一期不做 Status Notify；后续可扩展。

### 1.2 已确认决策

| 项 | 决策 |
|----|------|
| UUID | 写死固定 128-bit UUID，App 与固件各维护一份相同常量 |
| Notify | 一期不做 |
| Speed | 两种 speed 均支持通过手机指令设置（与串口等价） |
| 广播名 | `SpiderBod` |
| 交互 | **点按**（非按住）；点按 = 发送一次命令 |
| 串口 | 重构允许，但**所有现有 Shell 命令行为保持不变** |
| 平台 | 仅 iOS（Swift + SwiftUI + CoreBluetooth） |
| BLE 连通性 | 用户已用 nRF Connect 验证过，不单独安排连通性测试里程碑 |

### 1.3 非目标（一期）

- Android App
- BLE OTA、配对加密、多连接
- 动作状态回传（Notify / Read）
- 替代 Shell 做舵机逐通道标定（标定仍走串口 `spider` 子命令）

---

## 2. 系统架构

```text
┌─────────────────────────────────────────────────────────────┐
│  iOS App（SpiderRemote）                                     │
│  SwiftUI 按钮 → SpiderBLEManager → CoreBluetooth Write      │
└────────────────────────────┬────────────────────────────────┘
                             │ GATT Write / Write Without Response
                             ▼
┌─────────────────────────────────────────────────────────────┐
│  spider 固件                                                 │
│  ┌─────────────┐    ┌──────────────────┐                    │
│  │ ble_remote  │───►│ spider_control   │◄─── UART Shell     │
│  │ GATT 解析   │    │ 统一命令分发      │    （main.c 薄封装）│
│  └─────────────┘    └────────┬─────────┘                    │
│                              │                               │
│              ┌───────────────┼───────────────┐               │
│              ▼               ▼               ▼               │
│        kame_motion    spider_servo*    kame_pose / cal       │
│        （动作 FSM）    （舵机转速）      （已有）              │
└────────────────────────────┬────────────────────────────────┘
                             │ I2C
                             ▼
                      PCA9685 + 8× 舵机
```

`*` `spider_servo` 为从 `main.c` 抽出的舵机后端访问层（见 §4.2），供 Shell 与 `spider_control` 共用。

**设计原则**：BLE 与 Shell **只**在 `spider_control` 汇合；禁止在 `ble_remote.c` 里直接调用 `kame_motion_request()` 以外的散落逻辑。

---

## 3. 与现有 spider 的关系

### 3.1 保留不变的部分

| 模块 | 说明 |
|------|------|
| `kame_motion.c` / `kame_pose.c` / `kame_servo_cal.c` | 动作与标定逻辑不改 |
| PCA9685 驱动、设备树、50 Hz 插值线程 | 不改算法，仅把「转速设置」入口统一到 `spider_servo` |
| UART Shell 命令**列表与语义** | 见 §3.2 |

### 3.2 串口命令兼容清单

重构后以下命令必须**行为与现在一致**（含参数范围、错误提示、互斥规则）：

**顶层动作命令**

| 命令 | 等价 API |
|------|----------|
| `stand` / `splay` / `front_down` / … | `spider_control_motion(id, NONE)` |
| `lift L1` / `wave R2` | `spider_control_motion(id, leg)` |
| `stop` | `spider_control_stop()` → 内部 `kame_motion_request(STAND)` |
| `speed [ms]` | `spider_control_set_motion_delay(ms)` / get |

**顶层舵机**

| 命令 | 说明 |
|------|------|
| `angle <ch> <deg>` | 仍走 `main.c` 现有路径；触发 `kame_motion_stop()` |

**`spider` 子命令**

| 命令 | 说明 |
|------|------|
| `spider scan/check/servo/multi/all/move/speed/status/off/stand/splay` | 全部保留，行为不变 |

### 3.3 并行调试场景

典型联调流程：

1. 手机点按 **Forward** → 机器人开始前进。
2. 串口输入 `status` 或观察 printk，确认 Motion 已切换。
3. 串口输入 `stop` 或手机点 **Stop**，验证两路控制等价。
4. 串口 `speed 800` 与手机设置动作节拍，验证步态变化一致。

**互斥**：Shell 与 BLE 可同时连接；**后到的命令生效**（与现在「串口新命令打断 Motion」一致）。不做「BLE 锁定串口」。

### 3.4 断连安全

- BLE 连接断开 → 固件调用 `spider_control_stop()`（恢复立正）。
- 与串口无关；串口仍可用。

---

## 4. 固件设计

### 4.1 新增/调整文件

| 文件 | 职责 |
|------|------|
| `include/spider_ble_protocol.h` | UUID、Opcode、motion/leg 常量（**单一真相源**，与 iOS 常量表对齐） |
| `include/spider_control.h` + `src/spider_control.c` | 统一命令分发（Motion / Stop / 两种 Speed） |
| `include/spider_servo.h` + `src/spider_servo.c` | 舵机后端：`apply`、全局 `speed_deg_s` get/set（从 `main.c` 抽出） |
| `src/ble_remote.c` | `bt_enable`、广播、`SpiderBod`、GATT 注册、Write 回调 → `spider_control` |
| `src/main.c` | Shell 薄封装调用 `spider_control` / `spider_servo`；`main()` 初始化 BLE |
| `prj.conf` | 增加 `CONFIG_BT` 等（见 §4.5） |

`CMakeLists.txt` 增加上述新 `.c` 文件。

### 4.2 `spider_control` API（草案）

```c
/* 动作：等价于顶层 motion 命令 + stop */
int spider_control_motion(kame_motion_id_t id, kame_leg_id_t leg);
int spider_control_stop(void);

/* 动作节拍：等价于顶层 speed [ms] */
int spider_control_set_motion_delay_ms(uint32_t ms);
uint32_t spider_control_get_motion_delay_ms(void);

/* 舵机转速：等价于 spider speed [deg/s]，0 = 瞬时 */
int spider_control_set_servo_speed_deg_s(uint32_t deg_s);
uint32_t spider_control_get_servo_speed_deg_s(void);

/* BLE / Shell 共用：解析并执行一帧 BLE 载荷；Shell 不使用 */
int spider_control_ble_frame(const uint8_t *data, size_t len);
```

返回值：`0` 成功，负值为 `-EINVAL` 等；BLE 侧非法帧**静默丢弃**（玩具级，不 Notify 错误）。

### 4.3 两种 Speed（与串口对齐）

| 名称 | Shell 命令 | 含义 | 默认 | 有效范围（建议） |
|------|------------|------|------|------------------|
| **动作节拍** | `speed [ms]` | Motion 每 Step 间隔 `motion_step_delay_ms` | 500 ms | 100–2000 ms（`<100` 拒绝） |
| **舵机转速** | `spider speed [deg/s]` | 插值引擎角速度 | 240 °/s | 0–600（0 = 瞬时） |

App UI 上用不同区域/文案区分，避免用户混淆（对应 [progress.md](progress.md) 已知限制 #6）。

---

## 5. BLE GATT 规格

### 5.1 UUID（写死）

| 对象 | UUID |
|------|------|
| **Service** `Spider Remote` | `A0010001-0000-1000-8000-00805F9B34FB` |
| **Characteristic** `cmd` | `A0010002-0000-1000-8000-00805F9B34FB` |

- 属性：`cmd` = **Write** + **Write Without Response**（App 优先用 Without Response，降低延迟）
- 权限：open（无配对、无加密）— 玩具场景简化连接
- 最大连接数：`CONFIG_BT_MAX_CONN=1`

### 5.2 广播

| 项 | 值 |
|----|-----|
| 设备名 | `SpiderBod`（`CONFIG_BT_DEVICE_NAME`） |
| 模式 | 可连接 Peripheral |
| 广播内容 | Flags + Complete Local Name |

App 扫描时按名称前缀或精确匹配 `SpiderBod` 过滤。

### 5.3 命令帧格式

所有指令写入 **`cmd` characteristic**，小端序，长度 **1～3 字节**。

**Byte 0 = Opcode**

| Opcode | 值 | 载荷 | 说明 |
|--------|-----|------|------|
| `SPIDER_BLE_OP_MOTION` | `0x01` | `[motion_id][leg_id]` | 执行动作 |
| `SPIDER_BLE_OP_STOP` | `0x02` | （无） | 等价串口 `stop` → stand |
| `SPIDER_BLE_OP_SET_MOTION_DELAY` | `0x10` | `[uint16 ms]` | 等价 `speed <ms>` |
| `SPIDER_BLE_OP_SET_SERVO_SPEED` | `0x11` | `[uint16 deg/s]` | 等价 `spider speed <deg/s>` |

**示例（hex）**

| 操作 | 写入 |
|------|------|
| Stand | `01 00 FF` |
| Forward | `01 06 FF` |
| Stop | `02` |
| 动作节拍 800 ms | `10 20 03` |
| 舵机转速 240 °/s | `11 F0 00` |
| Lift L1 | `01 02 00` |

### 5.4 Motion ID（与 `kame_motion_id_t` 一致）

| ID | 名称 | 循环 | 需 leg |
|----|------|------|--------|
| `0x00` | stand | 否 | 否 |
| `0x01` | splay | 否 | 否 |
| `0x02` | lift | 否 | **是** |
| `0x03` | front_down | 否 | 否 |
| `0x04` | rear_down | 否 | 否 |
| `0x05` | full_down | 否 | 否 |
| `0x06` | forward | **是** | 否 |
| `0x07` | backward | **是** | 否 |
| `0x08` | turn_left | **是** | 否 |
| `0x09` | turn_right | **是** | 否 |
| `0x0A` | spin_left | **是** | 否 |
| `0x0B` | spin_right | **是** | 否 |
| `0x0C` | nod | **是** | 否 |
| `0x0D` | shake | **是** | 否 |
| `0x0E` | roll | **是** | 否 |
| `0x0F` | wave | **是** | **是** |

> `SPIDER_BLE_OP_STOP` 与 `SPIDER_BLE_OP_MOTION` + `motion_id=0` 效果相同；App **Stop 按钮**建议发 `0x02`，语义更清晰。

### 5.5 Leg ID（与 `kame_leg_id_t` 一致）

| ID | 腿 |
|----|-----|
| `0x00` | L1 |
| `0x01` | R1 |
| `0x02` | L2 |
| `0x03` | R2 |
| `0xFF` | 无（不需要腿参数的动作） |

若 `lift` / `wave` 的 `leg_id` 为 `0xFF` 或非法值，固件**忽略**该帧（与 Shell 报错语义不同，因无 Notify）。

### 5.6 帧校验规则

| 条件 | 处理 |
|------|------|
| 长度不对 | 忽略 |
| 未知 Opcode | 忽略 |
| `motion_id >= KAME_MOTION_COUNT` | 忽略 |
| `SET_MOTION_DELAY` 且 ms=0 或 ms>2000 | 忽略 |
| `SET_SERVO_SPEED` 且 deg/s>600 | 忽略 |

---

## 6. iOS App 设计（SpiderRemote）

### 6.1 工程位置与技术栈

| 项 | 建议 |
|----|------|
| 路径 | `projects/spider-remote-ios/`（与工作区并列，协议文档交叉引用） |
| 语言 | Swift 5.9+ |
| UI | SwiftUI |
| BLE | CoreBluetooth（`CBCentralManager` / `CBPeripheral`） |
| 最低系统 | iOS 15.0（含 15.8.x；UI 使用 `NavigationView` 以兼容 15.x） |
| 依赖 | 无第三方 BLE 库 |

### 6.2 模块划分

```text
SpiderRemote/
├── App/SpiderRemoteApp.swift
├── BLE/
│   ├── SpiderBLEConstants.swift    # UUID / Opcode / MotionID（与 spider_ble_protocol.h 对齐）
│   └── SpiderBLEManager.swift      # 扫描、连接、writeCmd(Data)
├── Models/
│   └── SpiderMotion.swift          # 枚举封装
└── Views/
    ├── RootView.swift              # 连接 + 遥控 Tab
    ├── ConnectionView.swift        # 扫描列表、连接状态
    ├── RemoteView.swift            # 主遥控面板
    └── SpeedSettingsView.swift     # 两种 Speed 设置
```

### 6.3 交互模型：点按

| 按钮类型 | 行为 | 协议 |
|----------|------|------|
| 静态动作（Stand、Splay…） | 点按一次 → 发一条 MOTION | `01 id FF` |
| 循环动作（Forward…） | 点按一次 → **开始循环**（与串口相同） | `01 id FF` |
| Stop | 点按 → 停止并立正 | `02` |
| Speed 预设 | 点按 → 设置对应数值 | `10 …` / `11 …` |

**不做**按住走、松手停。循环动作需用户再点 Stop 或切换其他动作。

### 6.4 UI 线框（一期）

**Tab 1 — 遥控**

```text
┌─────────────────────────────────┐
│  SpiderBod          ● 已连接     │
├─────────────────────────────────┤
│           [ Stand ]             │
│  [Turn L]  [■ Stop]  [Turn R]   │
│          [ Forward ]            │
│         [ Backward ]            │
│   [Spin L]        [Spin R]      │
│           [ Splay ]             │
├─────────────────────────────────┤
│  更多 ▼ （二期：nod/shake/lift） │
└─────────────────────────────────┘
```

**Tab 2 — 速度**

两种 speed 分开展示，文案与串口一致：

```text
动作节拍（等同串口 speed）
  当前：500 ms
  [ 慢 300 ] [ 中 500 ] [ 快 800 ]

舵机转速（等同 spider speed）
  当前：240 °/s
  [ 慢 120 ] [ 中 240 ] [ 快 360 ]
```

- 一期用**预设按钮**即可，与「手机发指令设置」一致；二期可加 Stepper 自定义数值。
- 预设值写入固件前可通过串口 `speed` / `spider speed` 实机标定，再定 App 默认值。

### 6.5 BLE 流程

```text
1. 检查蓝牙权限 → CBCentralManager  poweredOn
2. scanForPeripherals(withServices: nil)  // 按 name 过滤 SpiderBod
3. connect → discoverServices([serviceUUID])
4. discoverCharacteristics([cmdUUID])
5. 按钮 tap → peripheral.writeValue(data, for: cmdChar,
              type: .withoutResponse)
6. disconnect / 失连 → UI 提示；机器人端 auto stand
```

### 6.6 Info.plist

- `NSBluetoothAlwaysUsageDescription`：说明用于连接 SpiderBod 机器人遥控。

### 6.7 与固件协议同步

- iOS `SpiderBLEConstants.swift` 与 `include/spider_ble_protocol.h` **字段必须一致**。
- 新增 Motion 时：改 `kame_motion.h` → 更新协议头 → 更新 Swift 常量 → 文档 §5.4 表。

---

## 7. 配置变更（`prj.conf` 草案）

```ini
# 现有 CONFIG_GPIO / I2C / PWM / SHELL 保持不变 …

CONFIG_BT=y
CONFIG_BT_PERIPHERAL=y
CONFIG_BT_DEVICE_NAME="SpiderBod"
CONFIG_BT_MAX_CONN=1
CONFIG_BT_GATT_DYNAMIC_DB=y
```

- **不**启用 `CONFIG_BT_NUS`（不走文本 UART 桥）。
- Shell 仍走 UART，与 BLE 控制器共存。

---

## 8. 实施阶段

### 阶段 1 — 固件基础 ✅（2026-06-25）

1. ✅ 抽出 `spider_servo`（转速 get/set + 现有 apply 路径）
2. ✅ 实现 `spider_control` + Shell 改薄封装
3. ⏳ **回归**：全部串口命令手动测一遍（待实机确认）
4. ✅ 实现 `ble_remote` + `spider_ble_protocol.h`
5. ⏳ nRF Connect 写 hex 自测（待实机）

`make build PROJECT=spider` 已通过。

### 阶段 2 — iOS/iPad App 最小可用 ✅（2026-06-25）

工程路径：`projects/spider-remote-ios/`。

1. ✅ Xcode 工程 + CoreBluetooth 连接 `SpiderBod`
2. ✅ RemoteView：Stand / Forward / Backward / Stop / Turn / Spin / Splay
3. ✅ SpeedSettingsView：两种 speed 预设按钮
4. ✅ **iPad 真机联调通过**（遥控与速度预设效果良好）

最低系统 **iOS 15.0+**（含 iPad；UI 使用 `NavigationView`）。

### 阶段 3 — 完善与文档 ✅

1. ✅ App：Splay、ConnectionView 断连 UI（一期范围）
2. ✅ 更新 README / architecture / progress / [operations-guide.md](operations-guide.md)
3. ⏳ 实机记录：§9 T1–T9 逐项书面结果（iPad 主路径已验证）

---

## 9. 测试要点

| # | 场景 | 通过标准 |
|---|------|----------|
| T1 | 串口回归 | §3.2 全部命令与重构前行为一致 |
| T2 | BLE Forward | 点按后进入循环步态 |
| T3 | BLE Stop | 立正，与串口 `stop` 一致 |
| T4 | BLE 设动作节拍 800 | 步态变慢，与串口 `speed 800` 一致 |
| T5 | BLE 设舵机转速 | 与 `spider speed` 一致 |
| T6 | 串口 stop 打断 BLE forward | 立即停止 |
| T7 | BLE forward 时串口 status | 状态正确 |
| T8 | 手机断连 | 机器人 auto stand |
| T9 | lift L1 需 leg | `01 02 00` 正确抬腿 |

**已验证（2026-06-25，iPad + SpiderRemote）**：连接 SpiderBod、Stand / Forward / Backward / Turn / Spin / Splay / Stop、速度预设 — 效果良好。T1/T6–T8 等待串口并行记录。

## 10. 后续扩展（不在一期）

| 项 | 说明 |
|----|------|
| Notify `status` | 上报当前 motion_id、delay、servo_speed |
| 配对 / PIN | 防误连他人机器人 |
| 自定义 speed 数值输入 | Stepper 或文本框 |
| `angle` / 标定类命令走 BLE | 仍建议仅串口 |

---

## 11. 文档索引

| 文档 | 关系 |
|------|------|
| [architecture.md](architecture.md) | 分层架构；实施后增加 ble_remote / spider_control 层 |
| [kame-motion-framework.md](kame-motion-framework.md) | 动作语义 |
| [progress.md](progress.md) | 里程碑；阶段 D 后增加 BLE 阶段 |
| [README.md](../README.md) | 用户-facing 命令表 |
| [operations-guide.md](operations-guide.md) | 编译烧录、串口/BLE/App 操作总览 |
| [iOS 入门教程](../../spider-remote-ios/docs/ios-getting-started.md) | Xcode 安装、签名、真机部署与联调 |

---

## 附录 A：`spider_ble_protocol.h` 常量预览

实施时创建该头文件，内容与下表一致：

```c
#define SPIDER_BLE_SVC_UUID   /* A0010001-... */
#define SPIDER_BLE_CHR_CMD_UUID /* A0010002-... */

#define SPIDER_BLE_OP_MOTION           0x01
#define SPIDER_BLE_OP_STOP             0x02
#define SPIDER_BLE_OP_SET_MOTION_DELAY 0x10
#define SPIDER_BLE_OP_SET_SERVO_SPEED  0x11

#define SPIDER_BLE_LEG_NONE  0xFF

#define SPIDER_BLE_MOTION_DELAY_MIN_MS  100u
#define SPIDER_BLE_MOTION_DELAY_MAX_MS  2000u
#define SPIDER_BLE_SERVO_SPEED_MAX_DEG_S 600u
```

Motion / Leg ID **不**在协议头重复定义，直接 `#include "kame_motion.h"` / `kame_pose.h` 使用枚举值。

---

## 附录 B：新增 Motion 检查清单

1. `kame_motion.h` 增加枚举（注意 ID 顺序）
2. `kame_motion.c` 注册 `motions[]`
3. `main.c` Shell 注册
4. **若需 App 遥控**：更新 §5.4 表 + iOS 按钮 + 协议文档
5. Shell 与 BLE 均通过 `spider_control_motion()`，无需两套逻辑
