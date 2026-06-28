# ble-projects

长期维护的 **nRF52840 通用开发工作区**，基于 [pet-lover/rover](https://github.com) 已验证的 Zephyr/NCS 环境提取，供未来各类 BLE 与应用固件复用。

## 快速开始

```bash
cd ble-projects

# 首次（若 sdk/ 与 toolchains/ 已从 rover 复制可跳过）
make setup

# 编译示例
make build

# 烧录（连接 nRF52840 DK 后）
make flash-direct

# IDE 代码跳转
make compile_commands
```

用 **Cursor** 打开本目录根路径即可使用 `.vscode` 任务与 clangd。

## 示例工程

| 项目 | 说明 |
|------|------|
| [projects/hello_nrf52840](projects/hello_nrf52840) | 最小可运行固件：LED 闪烁、UART 输出、BLE 广播 |
| [projects/spider](projects/spider) | **8DOF Mini-Kame 四足**：PCA9685 舵机 + Kame Motion + **BLE 遥控** |
| [projects/spider-remote-ios](projects/spider-remote-ios) | **SpiderRemote** iOS/iPad App（CoreBluetooth → `SpiderBod`） |

### hello_nrf52840

- LED0 闪烁（500 ms）
- UART `printk` 输出
- BLE 不可连接广播（设备名 `Hello_nRF52840`）

### spider + SpiderRemote（当前主力项目）

```bash
# 固件：编译与烧录
make build PROJECT=spider
make flash-direct PROJECT=spider

# 串口 115200
screen /dev/cu.usbmodem<序列号>1 115200

# 手机/iPad 遥控
open projects/spider-remote-ios/SpiderRemote.xcodeproj
```

| 控制方式 | 说明 |
|----------|------|
| UART Shell | `stand` / `forward` / `stop` / `speed` / `spider scan` … |
| BLE | 广播名 `SpiderBod`，GATT 写 cmd 帧 |
| SpiderRemote App | iOS/iPad 15.0+；**全部 Motion + 两种 speed**（iPad 已验证） |

**一站式操作手册**：[projects/spider/docs/operations-guide.md](projects/spider/docs/operations-guide.md)

## 文档

| 文档 | 内容 |
|------|------|
| [docs/setup.md](docs/setup.md) | 环境配置、编译、烧录、调试、新建项目 |
| [docs/rover-analysis.md](docs/rover-analysis.md) | rover 工程结构与迁移分析 |
| [docs/knowledge/nrf52840-pca9685-wiring.md](docs/knowledge/nrf52840-pca9685-wiring.md) | nRF52840 ↔ PCA9685 I2C 连线 |
| [projects/spider/docs/operations-guide.md](projects/spider/docs/operations-guide.md) | **spider 操作总览**（编译/烧录/命令/BLE/App） |
| [projects/spider/README.md](projects/spider/README.md) | spider 固件入口 |
| [projects/spider/docs/progress.md](projects/spider/docs/progress.md) | 里程碑与变更日志 |
| [projects/spider/docs/ble-remote-design.md](projects/spider/docs/ble-remote-design.md) | BLE 协议规格 |
| [projects/spider-remote-ios/docs/ios-getting-started.md](projects/spider-remote-ios/docs/ios-getting-started.md) | iPhone/iPad App 部署教程 |

## 默认版本

- NCS **v2.7.0**
- Zephyr SDK **0.16.8**
- Board **nrf52840dk/nrf52840**

## 新建项目

```bash
cp -r templates/nrf52840_app projects/my_app
# 编辑 CMakeLists.txt、src/、prj.conf
make build PROJECT=my_app
```

## 目录一览

```
sdk/ncs/          ← 共享 NCS（勿手改，用 west 管理）
toolchains/       ← 共享编译器 + nrfutil
projects/         ← 各应用固件 + spider-remote-ios
templates/        ← 项目模板
scripts/          ← 初始化与构建脚本
```

大体积目录（`sdk/`、`toolchains/`）已加入 `.gitignore`，新机器通过 `make setup` 从 rover 镜像或网络初始化。
