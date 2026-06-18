# spider 开发进度

> 记录 Mini-Kame 四足机器人固件的功能里程碑、已验证项、已知限制与待办。

最后更新：2026-06-19

---

## 1. 项目概况

| 项 | 内容 |
|----|------|
| 平台 | nRF52840 DK + PCA9685（16 路 PWM）+ 8× 舵机 |
| 机器人 | 8DOF Mini-Kame 四足（4 腿 × 2 关节） |
| 固件框架 | Zephyr / NCS v2.7.0 |
| 交互方式 | UART Shell（115200，Tab 补全） |
| 动作框架 | Kame Motion Framework（离散步态 + FSM，无 IK） |

---

## 2. 里程碑

### 阶段 A：硬件驱动与调试 ✅

- [x] I2C0 + PCA9685 设备树 overlay
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
6. **双 speed 概念**：`spider speed`（舵机转速）与顶层 `speed`（动作节拍）易混淆

---

## 5. 待办 / 未来方向

### 近期

- [ ] 完成四条腿 lift / wave 实机稳定性测试并记录最优参数
- [ ] 行走步态在不同 `speed` 节拍下的表现对比
- [ ] 补充实机测试记录到本文档

### 中期

- [ ] BLE 遥控接口（利用 ble-projects 工作区 BLE 能力）
- [ ] 动作序列脚本化（预定义组合动作）
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
