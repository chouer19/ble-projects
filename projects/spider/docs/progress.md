# spider 开发进度

> 记录 Mini-Kame 四足机器人固件的功能里程碑、已验证项、已知限制与待办。

最后更新：2026-06-28（XIAO BLE Sense 多板型实机验证）

---

## 1. 项目概况

| 项 | 内容 |
|----|------|
| 平台 | nRF52840（**DK** 或 **Seeed XIAO BLE/Sense**）+ PCA9685 + 8× 舵机 |
| 机器人 | 8DOF Mini-Kame 四足（4 腿 × 2 关节） |
| 固件框架 | Zephyr / NCS v2.7.0；**同一 `src/` 多板 overlay** |
| 交互方式 | Shell（DK: UART 115200；XIAO: USB CDC）+ BLE（`SpiderBod`） |
| 手机 App | iOS/iPad 15.0+，`projects/spider-remote-ios/`（SpiderRemote） |
| 动作框架 | Kame Motion Framework（离散步态 + FSM，无 IK） |
| 日常操作 | [operations-guide.md](operations-guide.md) |

---

## 2. 里程碑

### 阶段 A：硬件驱动与调试 ✅

- [x] PCA9685 设备树 overlay（DK I2C0；XIAO I2C1 D4/D5）
- [x] Zephyr PCA9685 PWM 驱动集成
- [x] UART Shell：`spider scan/check/servo/angle/multi/all/move/speed/status/off`
- [x] 50 Hz 插值引擎（全局速度限制 + 指定时长 `move`）
- [x] 自定义 I2C 扫描（适配 nRF52 TWIM 限制）
- [x] 上电检测到 PCA9685 后自动 `stand` 复位

### 阶段 B：舵机标定 ✅

- [x] 8 路舵机全部完成站立标定与有效范围测定
- [x] 机器可读标定表 `kame_servo_cal.c` + 人类可读文档
- [x] 方向 flags 抽象（R/L 侧髋反向、R1/L2 膝特殊方向）
- [x] L2 膝方向修正（与 R1 一致，0°=抬最高）
- [x] 同侧前后腿碰撞边界实测与参数约束

### 阶段 C：Kame Motion Framework ✅

- [x] Pose 抽象层（STAND / SPLAY / TRIPOD / LIFT / 趴下）
- [x] Motion FSM 后台线程 + 步序表
- [x] 顶层动作命令（无前缀，便于串口输入）
- [x] 静态动作：stand / splay / lift / front_down / rear_down / full_down
- [x] 循环步态：forward / backward / turn_left / turn_right / spin_left / spin_right
- [x] 机身摆动：nod / shake / roll
- [x] 单腿动作：lift（静态）/ wave（循环，膝关节在最高端附近摆动）
- [x] 三脚架稳定逻辑（髋平移重心 + 膝侧倾）
- [x] 外张偏置防同侧前后腿互撞
- [x] 手动舵机命令与 Motion 互斥
- [x] `stop` = 恢复立正

### 阶段 D：实机调优 🔄 进行中

- [x] wave 招手改为在膝关节最高端附近摆动
- [x] 三脚架膝侧倾方向修正（对侧压腿 / 同侧收腿）
- [x] TRIPOD 参数调大至 25°（HIP / LEAN）
- [ ] 四条腿 lift / wave 稳定性逐一实机确认
- [ ] forward / spin 等步态在不同地面上的可靠性验证
- [ ] 步态参数（GAIT_HIP_DEG / BIAS / LIFT 等）精细微调

### 阶段 E：BLE 遥控 + iOS/iPad App ✅ 首版实机验证通过

**固件**

- [x] 抽出 `spider_servo` / `spider_control`
- [x] Shell 薄封装，逻辑经 `spider_control` 汇合
- [x] `ble_remote` + `spider_ble_protocol.h`（GATT、广播 `SpiderBod`）
- [x] BLE 断连 auto stand
- [x] `make build PROJECT=spider` 通过
- [ ] 串口命令全量回归（§3.2，待书面记录）

**SpiderRemote App**

- [x] SwiftUI + CoreBluetooth 工程（iOS 15.0+）
- [x] RemoteView / SpeedSettingsView / ConnectionView
- [x] **iPad 真机 BLE 遥控验证**（2026-06-25，效果良好）
- [x] **全 Motion 遥控**（16 种 + Stop；分区 UI：姿态/行走/趴下/机身/抬腿/招手）
- [x] **速度 Tab 自定义滑条**（动作节拍 100–2000 ms；舵机 0–600 °/s）
- [ ] iPhone 单独验证（可选，与 iPad 同工程）
- [ ] §9 测试表 T1–T9 逐项记录

### 阶段 F：多板型支持（XIAO BLE） ✅ 实机验证通过

- [x] 同一 `projects/spider` 支持 DK + XIAO，板级差异仅在 `boards/*.overlay`
- [x] `main.c` I2C 总线跟随 `pca9685` 节点（`DT_BUS`），无硬编码引脚
- [x] XIAO overlay：`i2c1` + D4/D5；USB CDC Shell（`boards/xiao_ble.conf`）
- [x] 按板型分目录构建 `build/<BOARD_SLUG>/`，切换 `BOARD` 无需 `clean`
- [x] `make flash-uf2`（默认 UF2 盘 `XIAO-SENSE`）
- [x] **XIAO BLE Sense 实机**：UF2 烧录、USB Shell、**iPad SpiderRemote BLE** 与 DK 功能一致

**文档**

- [x] README / architecture / progress / ble-remote-design
- [x] [operations-guide.md](operations-guide.md) 操作手册
- [x] [iOS 入门教程](../../spider-remote-ios/docs/ios-getting-started.md)

---

## 3. 已移除功能

| 功能 | 原因 | 时间 |
|------|------|------|
| `shift_left` / `shift_right` | 硬件无侧向 DOF；原实现仅为 roll 侧倾，无法产生净位移 | 2026-06 |

---

## 4. 已知限制

1. **无侧向平移**：每条腿仅髋(前后) + 膝(抬落)，无法实现真正 crab walk
2. **无 IK / 轨迹规划**：动作由离散 Pose 帧组成，非连续轨迹
3. **无传感器反馈**：开环控制，地面摩擦/负载变化会影响实际步态
4. **PCA9685 热插拔**：上电时不在线则驱动初始化失败，需 RESET 后恢复
5. **同侧腿碰撞**：髋参数过大可能导致 L1↔L2、R1↔R2 相向顶撞，参数已留余量但需实机观察
6. **双 speed 概念**：`spider speed`（舵机转速）与顶层 `speed`（动作节拍）易混淆；App「速度」Tab 已分开展示
7. **BLE 无状态回传**：一期不做 Notify，无法从手机读当前 Motion
8. **BLE 动作节拍下限**：手机/BLE 路径限制 100–2000 ms；串口 `speed` 仍允许任意 `>0` 的值
9. **BLE 单连接**：SpiderRemote 与 nRF Connect 不能同时连接

---

## 5. 待办 / 未来方向

### 近期

- [ ] 完成 [ble-remote-design.md](ble-remote-design.md) §9 测试表 T1–T9 并更新 [operations-guide.md](operations-guide.md) §11
- [ ] 继续阶段 D：lift/wave 稳定性、步态地面测试
- [ ] 步态参数在不同 `speed` 节拍下的对比记录

### 中期

- [ ] BLE Notify 状态回传
- [ ] 动作序列脚本化
- [ ] 低功耗待机（舵机 off + 唤醒恢复 stand）

### 长期

- [ ] IMU 姿态反馈与自适应步态
- [ ] 多机协同或上位机可视化调试

---

## 6. 变更日志（摘要）

| 日期 | 变更 |
|------|------|
| 2026-06 | 初始 PCA9685 舵机 Shell 控制 |
| 2026-06 | 8 路舵机标定完成，Kame 标定表入库 |
| 2026-06 | Kame Motion Framework 首版：静态姿态 + 对角步态 |
| 2026-06 | wave 改为膝关节最高端附近摆动 |
| 2026-06 | 三脚架膝侧倾逻辑修正；TRIPOD 参数 20°→25° |
| 2026-06 | 移除 shift_left / shift_right |
| 2026-06 | 项目文档整理（architecture / progress / README） |
| 2026-06-25 | BLE 遥控设计规格 [ble-remote-design.md](ble-remote-design.md) |
| 2026-06-25 | 固件 BLE + `spider_control` 重构；iOS SpiderRemote 首版 |
| 2026-06-25 | iOS 最低版本降至 15.0；iPad 真机 BLE 遥控验证通过 |
| 2026-06-25 | 新增 [operations-guide.md](operations-guide.md) 操作手册 |
| 2026-06-25 | SpiderRemote 升级：App 覆盖全部 Motion + 速度自定义滑条 |
| 2026-06-28 | 多板型：XIAO BLE overlay + `make flash-uf2`；XIAO Sense 串口/BLE 实机验证 |
