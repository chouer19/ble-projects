# spider 软件架构

> 8DOF Mini-Kame 四足机器人固件 — 模块划分、数据流与线程模型。

---

## 1. 总体目标

在 nRF52840 + PCA9685 平台上，通过 UART Shell 交互控制 8 路舵机，并实现 **Kame Motion Framework**：离散步态、无 IK、无轨迹规划，适合资源受限的嵌入式环境。

设计原则：

- 动作代码**不出现 PCA9685 通道号**，一律通过 `servo_id` / `leg_id` 访问
- 通道映射与方向语义集中在 `kame_servo_cal`
- 姿态由「各舵机目标角」直接描述，不做逆运动学
- 同一时刻只运行一个 Motion；手动舵机命令与 Motion 互斥

---

## 2. 分层架构

```text
┌─────────────────────────────────────────────────────────┐
│  UART Shell（main.c）                                    │
│  · 顶层动作命令：stand / forward / lift / wave / stop …  │
│  · spider 子命令：scan / angle / multi / status …        │
└──────────────────────────┬──────────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────┐
│  kame_motion（FSM + 步序表）                             │
│  · motions[]：每个 Motion = 若干 Step                     │
│  · 后台线程：每 motion_step_delay_ms 推进一 Step         │
│  · 产出 kame_pose_t → kame_pose_apply()                  │
└──────────────────────────┬──────────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────┐
│  kame_pose（静态/单腿/步态 Pose 构建）                    │
│  · STAND / SPLAY / TRIPOD / LIFT / 步态填充 …            │
│  · 调用 kame_servo_*_deg() 做定向偏移                    │
└──────────────────────────┬──────────────────────────────┘
                           │ kame_servo_apply(servo_id, deg)
┌──────────────────────────▼──────────────────────────────┐
│  kame_servo_cal（标定表 + 方向抽象）                      │
│  · servo_id → 通道 / 范围 / 站立角 / flags               │
│  · hip_forward / knee_up / knee_top / splay             │
└──────────────────────────┬──────────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────┐
│  main.c 舵机后端 + 50 Hz 插值线程                        │
│  · deg → 脉宽(500~2500 µs) → pwm_set(pca9685)           │
│  · 全局速度 spider speed（默认 240°/s）                  │
└──────────────────────────┬──────────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────┐
│  Zephyr PCA9685 PWM 驱动（I2C0 @ 0x40）                  │
└─────────────────────────────────────────────────────────┘
```

---

## 3. 源文件职责

| 文件 | 职责 |
|------|------|
| `src/main.c` | Shell 命令注册、I2C 诊断、50 Hz 插值引擎、`kame_servo_apply()` 实现、上电自动 `stand` |
| `include/kame_servo_cal.h` + `src/kame_servo_cal.c` | 8 路舵机标定表、角度钳位、方向语义辅助函数 |
| `include/kame_pose.h` + `src/kame_pose.c` | 腿映射、静态姿态、三脚架/抬腿、Pose 下发 |
| `include/kame_motion.h` + `src/kame_motion.c` | Motion 枚举、步序表、FSM 后台线程 |
| `boards/nrf52840dk_nrf52840.overlay` | I2C0 挂载 PCA9685 @ 0x40，100 kHz |
| `prj.conf` | I2C / PWM / Shell / UART 配置 |
| `CMakeLists.txt` | 编译上述 4 个 `.c` 源文件 |

---

## 4. 线程模型

固件运行 **2 个应用层后台线程**（另加 Zephyr 系统线程）：

| 线程 | 栈 | 优先级 | 周期 | 作用 |
|------|-----|--------|------|------|
| `motion_tid` | 2048 B | 5 | 20 ms (50 Hz) | 舵机插值：把各通道 `cur_us` 逐步推向 `tgt_us` |
| `kame_motion_tid` | 1536 B | 6 | `motion_step_delay_ms`（默认 500 ms） | Motion FSM：构建 Pose 并下发 |

**两层「速度」概念**：

- `spider speed <deg/s>` — 舵机物理转动速度（插值引擎），默认 240°/s
- `speed <ms>`（顶层）— Motion 每 Step 之间的等待节拍，默认 500 ms

若节拍过短而舵机尚未到位就进入下一步，会出现「追不上」现象。

---

## 5. 硬件抽象

### 5.1 腿与舵机 ID

```text
        前 (Front)
   L1 ──────────── R1
   │                │
   │     机身       │
   │                │
   L2 ──────────── R2
        后 (Rear)
```

| 腿 | 髋关节 `servo_id` | 腿关节 `servo_id` | PCA9685 通道 |
|----|-------------------|-------------------|--------------|
| L1 | `KAME_SERVO_L1_HIP` | `KAME_SERVO_L1_KNEE` | 1 / 11 |
| R1 | `KAME_SERVO_R1_HIP` | `KAME_SERVO_R1_KNEE` | 4 / 14 |
| L2 | `KAME_SERVO_L2_HIP` | `KAME_SERVO_L2_KNEE` | 2 / 12 |
| R2 | `KAME_SERVO_R2_HIP` | `KAME_SERVO_R2_KNEE` | 3 / 13 |

### 5.2 舵机方向 flags

标定表 `flags` 字段描述安装差异，供姿态生成自动取号：

| 标志 | 含义 |
|------|------|
| `KAME_SERVO_F_HIP` | 髋关节（否则为膝/腿关节） |
| `KAME_SERVO_F_FRONT` | 前腿 (L1/R1) |
| `KAME_SERVO_F_HIP_INC_BACKWARD` | 角度增大时爪向后（L 侧髋） |
| `KAME_SERVO_F_KNEE_0_BELLY` | 0° 端为收爪/腹下（R2/L1 膝） |

> L2 膝与 R1 同向（0°=抬最高），**不**置 `KNEE_0_BELLY`。详见 [kame-servo-calibration.md](kame-servo-calibration.md)。

### 5.3 机械约束

- 每条腿 **2 DOF**：髋（水平前后摆）+ 膝（竖直抬落），**无独立侧向 DOF**
- 同侧前后腿髋相向靠拢有碰撞风险；步态/三脚架参数留有 ~15° 余量
- 横移（crab walk）在当前硬件上不可实现；曾尝试的 `shift_left/right` 已移除

---

## 6. Motion FSM 生命周期

```text
kame_motion_request(id, leg)
    │
    ├─ 递增 generation（打断旧 Motion）
    ├─ step = 0, active = true
    └─ 唤醒 motion 线程
            │
            ▼
    motions[id].build(leg, step, &pose)   ← 构建当前 Step 的 Pose
            │
            ▼
    kame_pose_apply(&pose)                ← 逐舵机 kame_servo_apply()
            │
            ▼
    step++，等待 motion_step_delay_ms
            │
            ├─ 静态动作：step >= steps → active=false，停在末态
            └─ 循环动作：step >= steps → step=0，继续循环
```

**打断规则**：

- 新 Motion 请求 → 立即切换
- `kame_motion_stop()` → 停止推进（手动命令路径会先调用）
- 任意 `spider angle/servo/multi/...` 手动命令 → 自动 `kame_motion_stop()`

`stop` 命令等价于 `kame_motion_request(KAME_MOTION_STAND)`，恢复立正。

---

## 7. 设备树与驱动

`boards/nrf52840dk_nrf52840.overlay`：

```dts
&i2c0 {
    status = "okay";
    clock-frequency = <100000>;
    pca9685: pca9685@40 {
        compatible = "nxp,pca9685-pwm";
        reg = <0x40>;
        #pwm-cells = <2>;
        status = "okay";
    };
};
```

- I2C0 默认引脚 P0.26 (SDA) / P0.27 (SCL)，与 DK Arduino 排针对齐
- 舵机 PWM 50 Hz，脉宽 500~2500 µs 线性映射 0~180°
- I2C 扫描用「读 1 字节」探测（nRF52 TWIM 不支持 0 长度传输）

接线详见工作区 [docs/knowledge/nrf52840-pca9685-wiring.md](../../../docs/knowledge/nrf52840-pca9685-wiring.md)。

---

## 8. 扩展指南

### 新增 Motion

1. 在 `kame_motion.h` 枚举中添加 `KAME_MOTION_*`
2. 在 `kame_motion.c` 实现 `step_*()` 构建函数
3. 在 `motions[]` 表注册（name / cyclic / needs_leg / steps / build）
4. 在 `main.c` 用 `SHELL_CMD_ARG_REGISTER` 注册顶层命令

### 微调步态/姿态

优先修改命名常量，改后 `make build PROJECT=spider`：

- 姿态参数 → `include/kame_pose.h`
- 步态参数 → `src/kame_motion.c` 顶部宏
- 舵机标定 → `src/kame_servo_cal.c`（同步更新 [kame-servo-calibration.md](kame-servo-calibration.md)）

### 后续可能方向

- BLE 遥控（复用 ble-projects 工作区能力）
- 传感器反馈（IMU 姿态闭环）
- 更精细的步态参数在线调节（Shell 或 BLE 配置）
