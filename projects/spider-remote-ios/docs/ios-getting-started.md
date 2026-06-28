# SpiderRemote — iPhone 开发、部署与联调教程

> 面向**从未做过 iOS 开发**的使用者：从零在真机上安装 SpiderRemote，并与 spider 固件联调。  
> 工程路径：`projects/spider-remote-ios/`  
> 固件广播名：**SpiderBod**

最后更新：2026-06-25

**实机状态**：iPad + SpiderRemote 已于 2026-06-25 完成 BLE 遥控验证，效果良好。

---

## 0. 你要完成什么

按顺序完成下面四块，就算「手机端第一步」全部做完：

| 步骤 | 目标 | 预计时间 |
|------|------|----------|
| A | 准备 Mac + Xcode + Apple ID | 30–60 分钟（首次下载 Xcode 更久） |
| B | 打开工程、配置签名、装到 iPhone | 15–30 分钟 |
| C | 打开 App，连接 SpiderBod | 5 分钟 |
| D | 按测试表验证遥控（可与串口并行） | 20–30 分钟 |

**必备硬件**

- Mac（Apple Silicon 或 Intel 均可）
- iPhone（iOS **15.0 或更高**，含 15.8.8）
- USB 数据线（能传数据，不是只能充电的线）
- 已接好线的 nRF52840 DK + 机器人（固件已刷入带 BLE 的版本）

**为什么不能用模拟器**

- CoreBluetooth 在 iOS **模拟器上不能连真机 BLE 外设**
- 必须用 **真机 iPhone** 测试

---

## 1. 准备 Mac 与 Xcode

### 1.1 安装 Xcode

1. 打开 Mac 上的 **App Store**
2. 搜索 **Xcode**，点击「获取 / 安装」
3. 体积约 10 GB+，请预留磁盘空间
4. 安装完成后**第一次打开 Xcode**，按提示安装额外组件（Component），等待完成

若 App Store 太慢，也可从 [Apple Developer 下载页](https://developer.apple.com/download/applications/) 下载 `.xip`（需 Apple ID 登录）。

### 1.2 确认 Xcode 命令行工具（可选）

终端执行：

```bash
xcode-select -p
```

应输出类似 `/Applications/Xcode.app/Contents/Developer`。若报错，执行：

```bash
sudo xcode-select -s /Applications/Xcode.app/Contents/Developer
```

并输入 Mac 登录密码。

### 1.3 Apple ID（免费即可）

安装 App 到**自己的 iPhone**，不需要付费的「Apple Developer Program（$99/年）」。

- 免费 Apple ID：App 约 **7 天**后需重新从 Xcode 安装一次
- 付费开发者账号：签名有效期更长，可 TestFlight 分发

本教程按**免费 Apple ID** 说明。

---

## 2. 打开 SpiderRemote 工程

### 2.1 用 Finder 或终端打开

在 ble-projects 工作区根目录：

```bash
open projects/spider-remote-ios/SpiderRemote.xcodeproj
```

或在 Finder 中进入 `projects/spider-remote-ios/`，双击 **SpiderRemote.xcodeproj**。

> 请打开 `.xcodeproj`，不是整个 `ble-projects` 文件夹。

### 2.2 工程结构（了解即可）

```text
SpiderRemote/
├── App/SpiderRemoteApp.swift       # 程序入口
├── BLE/                            # 蓝牙连接与协议
├── Models/                         # 动作枚举
├── Views/                          # 遥控 / 速度 / 连接 三个 Tab
├── Info.plist                      # 蓝牙权限文案
└── Assets.xcassets                 # 图标等资源
```

---

## 3. 连接 iPhone 并配置签名

### 3.1 连接手机

1. 用 USB 线连接 iPhone 与 Mac
2. iPhone 弹出「要信任此电脑吗？」→ 点 **信任**，输入锁屏密码
3. 打开 Xcode 窗口**顶部中央**的设备选择器（默认可能显示 "Any iOS Device"）
4. 在下拉列表中选择你的 **iPhone 名称**（例如 "Chouer's iPhone"）

若列表里没有 iPhone：

- 确认数据线能传数据
- iPhone **设置 → 隐私与安全性 → 开发者模式**（iOS 16 及以上才有此项；15.x 可忽略）：若曾开启过，需打开并重启
- 拔掉重插，解锁 iPhone 屏幕

### 3.2 配置 Signing（签名）

1. 在 Xcode **左侧文件树**最上方，点击蓝色工程图标 **SpiderRemote**
2. 中间选中 **TARGETS → SpiderRemote**
3. 打开 **Signing & Capabilities** 标签页
4. 勾选 **Automatically manage signing**
5. **Team** 下拉：选你的 Apple ID
   - 若没有：点 **Add an Account…**，用 Apple ID 登录
   - 登录后 Team 可能显示为 `Your Name (Personal Team)`
6. **Bundle Identifier** 默认是 `com.bleprojects.SpiderRemote`
   - 若提示「已被占用」，改成唯一值，例如：`com.你的英文名.SpiderRemote`

**常见 Signing 报错**

| 报错 | 处理 |
|------|------|
| Failed to register bundle identifier | 改 Bundle Identifier 为唯一字符串 |
| No profiles for … | 确认已勾选 Automatically manage signing 并选了 Team |
| iPhone is not available | 解锁手机，信任电脑，换数据线 |

---

## 4. 编译并安装到 iPhone

### 4.1 运行

1. 确认顶部设备选的是你的 **iPhone**（不是 Simulator）
2. 点击左上角 **▶ Run**，或按 **⌘R**
3. 首次编译需 1–3 分钟，底部状态栏显示 "Build Succeeded" 后 App 会自动装到手机并启动

### 4.2 首次安装：信任开发者

若 iPhone 提示「无法验证 App」或 App 闪退：

1. iPhone 打开 **设置 → 通用 → VPN 与设备管理**（或「设备管理」）
2. 在 **开发者 App** 下点你的 Apple ID
3. 点 **信任 "…"**
4. 回到主屏幕，再次打开 **SpiderRemote**

### 4.3 允许蓝牙权限

首次使用蓝牙功能时，系统会弹窗：

> 「SpiderRemote」想要使用蓝牙

点 **允许** / **好**。

若之前误点了「不允许」：

**设置 → SpiderRemote → 蓝牙** → 打开。

---

## 5. 刷固件并确认机器人就绪

在进行手机连接前，请确保 DK 上运行的是**带 BLE 的 spider 固件**。

### 5.1 编译与烧录（Mac 终端）

```bash
cd /path/to/ble-projects
make build PROJECT=spider
make flash-direct PROJECT=spider
```

### 5.2 串口确认（建议并行开着）

```bash
screen /dev/cu.usbmodem<你的序列号>1 115200
```

上电后应看到类似输出：

```text
PCA9685 driver ready.
Boot reset: Kame -> 标准站姿 (stand).
Bluetooth initialized (name: SpiderBod)
BLE advertising started (connectable)
```

若只有 PCA9685 警告、没有 Bluetooth 行，说明固件过旧或未编译 BLE，请重新 `make build` + `flash-direct`。

### 5.3 可选：用 nRF Connect 预检

在 App Store 安装 **nRF Connect for Mobile**（Nordic 官方，免费）：

1. 打开 **Scanner** 标签
2. 应能看到广播名 **SpiderBod**
3. 连接 → 发现 Service `A0010001-...` → Characteristic `A0010002-...`
4. 写入 `02`（Stop）或 `01 06 FF`（Forward）做快速验证

这一步通过后再用 SpiderRemote，排查问题更简单。

---

## 6. 在 SpiderRemote 里连接 SpiderBod

App 底部有三个 Tab：**遥控**、**速度**、**连接**。

### 6.1 连接流程

1. 打开 **连接** Tab
2. 确认顶部状态不是「蓝牙关闭」（若是，到 iPhone **设置 → 蓝牙** 打开）
3. 点 **扫描 SpiderBod**
4. 列表出现 **SpiderBod** 后，点一下开始连接
5. 状态变为 **已连接**（绿点）

### 6.2 连接失败排查

| 现象 | 检查 |
|------|------|
| 扫描不到 SpiderBod | DK 是否上电；串口是否有 `BLE advertising started`；手机与 DK 距离 < 2 m |
| 连接后立即断开 | 固件是否 crash；串口是否有 `BLE disconnected` |
| 已连接但按钮无效 | 等 1–2 秒让 GATT 发现完成；断开重连 |
| 提示未找到 Service | 固件 UUID 是否与 App 一致（见 [ble-remote-design.md](../../spider/docs/ble-remote-design.md)） |

---

## 7. 使用 App 遥控

### 7.1 交互方式：点按，不是按住

- 每个按钮 **点一下 = 发一条命令**（与串口输入一行命令相同）
- **循环动作**（Forward 等）：点一次开始循环，需再点 **Stop** 或切换其他动作才会停
- **不会**「按住走、松手停」

### 7.2 遥控 Tab（全部 Motion）

界面按分区展示，覆盖固件 **16 种 Motion** + **Stop**：

| 分区 | 按钮 | 等价串口 | 循环 |
|------|------|----------|------|
| 姿态 | Stand、Splay | `stand` / `splay` | 否 |
| 行走 | Forward、Backward、Turn L/R、Spin L/R | 同名顶层命令 | 是 |
| 趴下 | Front Down、Rear Down、Full Down | `front_down` 等 | 否 |
| 机身 | Nod、Shake、Roll | `nod` / `shake` / `roll` | 是 |
| 抬腿 | Lift L1 / R1 / L2 / R2 | `lift L1` 等 | 否 |
| 招手 | Wave L1 / R1 / L2 / R2 | `wave L1` 等 | 是 |
| 全局 | ■ Stop | `stop` | — |

常用 BLE 帧示例：

| 操作 | Hex |
|------|-----|
| Stand | `01 00 FF` |
| Forward | `01 06 FF` |
| Lift L1 | `01 02 00` |
| Wave R2 | `01 0F 03` |
| Stop | `02` |

未连接时按钮为灰色；请先完成 §6 连接。完整对照见 [operations-guide §7.4](../../spider/docs/operations-guide.md)。

### 7.3 速度 Tab

两种 speed **分开**，与串口一致：

| 区域 | 等价串口 | 预设 | 自定义 |
|------|----------|------|--------|
| 动作节拍 | `speed <ms>` | 300 / 500 / 800 ms | 滑条 100–2000 ms → **应用** |
| 舵机转速 | `spider speed <deg/s>` | 120 / 240 / 360 °/s | 滑条 0–600 °/s → **应用** |

点预设或「应用」后立即写入固件；界面「当前：xxx」为 App 本地记录（固件不回传确认）。

---

## 8. 推荐联调测试（复制执行）

建议 **串口 + 手机同时开着**，便于对比。测试项与 [ble-remote-design.md §9](../../spider/docs/ble-remote-design.md) 对应。

### 8.1 基础动作

```text
[手机] 连接 Tab → 扫描并连接 SpiderBod
[手机] 遥控 Tab → Stand          → 机器人应立正
[手机] 遥控 Tab → Forward        → 应开始循环前进
[手机] 遥控 Tab → Stop           → 应立正停止
```

### 8.2 速度等价

```text
[手机] 速度 Tab → 快 800（动作节拍）
[手机] 遥控 Tab → Forward
      → 步态应变慢

[串口] stop
[串口] speed 500                 → 恢复默认节拍

[手机] 速度 Tab → 快 360（舵机转速）
[手机] 遥控 Tab → Stand
      → 舵机移动应变快（与 spider speed 360 类似）
```

### 8.3 串口与 BLE 互斥（后到生效）

```text
[手机] Forward 开始走
[串口] stop                      → 应立刻停（串口打断 BLE）

[串口] forward
[手机] Stop                      → 应立刻停（BLE 打断串口）
```

### 8.4 断连安全

```text
[手机] Forward 开始走
[手机] 连接 Tab → 断开连接
      → 机器人应自动立正（auto stand）
[串口] 仍可输入 stand / forward  → Shell 不受影响
```

### 8.5 记录结果（建议）

在 [progress.md](../../spider/docs/progress.md) 或你自己的笔记里记录每项通过/失败，便于后续调参。

---

## 9. 常见问题 FAQ

### Q1：App 7 天后打不开

免费 Apple ID 签名的 App 约 **7 天过期**。解决：iPhone 连 Mac，Xcode 再 **⌘R** 装一次。

### Q2：能不能不用 Mac，只在 iPhone 上装？

- 从 App Store 安装：需要付费开发者账号 + 上架或 TestFlight
- 当前工程是**源码开发模式**，必须用 Xcode 从 Mac 安装

### Q3：能否用 iPad？

可以，工程支持 iPhone / iPad；BLE 行为相同。

### Q4：修改代码后如何更新手机上的 App

1. 改 Swift 文件保存
2. Xcode **⌘R** 重新 Run
3. 若仅改 UI 文案，一般不用改 Bundle ID 或签名

### Q5：SpiderRemote 与 nRF Connect 能同时连吗？

固件 `CONFIG_BT_MAX_CONN=1`，**同时只能一个 BLE  central 连接**。用 SpiderRemote 前请断开 nRF Connect。

### Q6：编译报错 "Signing for SpiderRemote requires a development team"

回到 §3.2，在 Signing & Capabilities 中选择 Team。

### Q7：机器人没反应但 App 显示已连接

1. 串口确认 PCA9685 ready、舵机供电正常
2. 先用 nRF Connect 写 `01 06 FF` 排除 App 问题
3. 断开重连，等 cmd characteristic 发现完成后再点按钮

### Q8：我的 iPhone 是 15.8.8，能装吗？

可以。工程最低版本为 **iOS 15.0**，15.8.8 完全在支持范围内。在 Xcode **General → Minimum Deployments** 中应显示 **iOS 15.0**；若你之前装过旧版（16.0 构建），重新 **⌘R** 装一次即可。

---

## 10. 下一步可以做什么

完成 §8 全部测试后，你可以：

1. **继续阶段 D 步态调参** — 仍用串口 `speed` / `forward` 更方便微调
2. **记录测试结果** — 更新 `docs/progress.md` 阶段 E 勾选项
3. **扩展 App（二期）** — nod / shake / lift 等按钮（协议已支持，UI 未做）
4. **付费开发者账号** — 若希望 App 不过期或给他人安装（TestFlight）

---

## 11. 相关文档

| 文档 | 内容 |
|------|------|
| [ble-remote-design.md](../../spider/docs/ble-remote-design.md) | BLE 协议、UUID、测试表 |
| [spider README.md](../../spider/README.md) | 固件命令与快速开始 |
| [architecture.md](../../spider/docs/architecture.md) | 固件分层与 BLE 架构 |
| [progress.md](../../spider/docs/progress.md) | 里程碑与待办 |

---

## 附录：一键命令备忘

```bash
# 打开 iOS 工程
open projects/spider-remote-ios/SpiderRemote.xcodeproj

# 编译刷固件
make build PROJECT=spider && make flash-direct PROJECT=spider

# 串口（替换为你的设备名）
screen /dev/cu.usbmodemXXXX1 115200
```

Xcode：**选真机 → Signing 选 Team → ⌘R**

App：**连接 → 扫描 SpiderBod → 遥控 / 速度**
