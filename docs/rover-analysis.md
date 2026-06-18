# Rover 工程分析报告

本文档基于 `pet-lover/rover/` 的现有实现，说明其编译系统、SDK/工具链来源、烧录与调试方式，并为通用 nRF52840 工作区 `ble-projects/` 的目录划分提供依据。

分析时间：2026-06-08  
分析对象：`/Users/chouer/Code/pet-lover/rover/`

---

## 1. 工程概览

Rover 是上层 monorepo 中的**产品固件应用**，基于 **nRF Connect SDK（NCS）+ Zephyr RTOS**，目标板型为 **nRF52840 DK**（`nrf52840dk/nrf52840`）。

**重要结论**：Rover **未使用** 传统 nRF5 SDK（裸机 SDK）；全部构建走 **west + CMake + Zephyr**。不存在独立的 CMSIS 或 SEGGER 源码树——它们作为 NCS/Zephyr SDK 的子模块或预编译工具存在。

| 组件 | Rover 中的位置 | 版本（已验证） |
|------|----------------|----------------|
| nRF Connect SDK | `rover/ncs_sdk/` | **v2.7.0**（`nrf/VERSION` = 2.7.0） |
| Zephyr RTOS | `rover/ncs_sdk/zephyr/` | 3.6.99（NCS 2.7.0 捆绑） |
| nrfxlib / modules | `rover/ncs_sdk/nrfxlib/`、`modules/` | 随 NCS west manifest |
| Zephyr SDK（ARM GCC） | `rover/.local-tooling/zephyr-sdk-0.16.8/` | **0.16.8**（gcc 12.2.0） |
| nrfutil | `rover/.local-tooling/nrfutil/` | **8.1.1** |
| Python 环境 | `rover/ncs_sdk/.venv/` | Python 3.11 + west |
| Bootstrap venv | `rover/.local-tooling/.bootstrap-venv/` | 仅用于首次 `west init` |

磁盘占用（实测）：

- `ncs_sdk/` ≈ **5.1 GB**
- `.local-tooling/` ≈ **7.8 GB**（主要为 Zephyr SDK 交叉编译器）

---

## 2. 编译系统

### 2.1 构建流程

```
Makefile → west build → CMake（Zephyr）→ Ninja → zephyr.hex
```

关键约定（`rover/Makefile`）：

1. 在 **`ncs_sdk/`** 目录执行 `west`（找到 `.west` workspace 根）。
2. 应用源码在 **`rover/`**（out-of-tree 构建）。
3. 构建产物在 **`rover/build/`**。
4. 通过 `ZEPHYR_SDK_INSTALL_DIR` 指向本地 Zephyr SDK，**不依赖**全局 `~/.cmake/packages/Zephyr-sdk`。

### 2.2 核心文件

| 文件 | 角色 |
|------|------|
| `Makefile` | 日常入口：`setup` / `build` / `flash` / `menuconfig` / `compile_commands` |
| `CMakeLists.txt` | Zephyr 应用 CMake：`find_package(Zephyr)` + `target_sources` |
| `prj.conf` | Kconfig 片段（GPIO、串口、栈大小等） |
| `scripts/setup_env.sh` | 一键初始化：uv、NCS workspace、Zephyr SDK、冒烟编译 |
| `boards/*.overlay` | 设备树 overlay（如 UART1 接模组） |

### 2.3 初始化脚本逻辑

`scripts/setup_env.sh` 执行顺序：

1. 安装主机依赖（Linux apt / macOS brew 检查）
2. 安装 `uv`（Python 包管理）
3. Bootstrap venv 安装 `west`
4. `west init` + `west update` 创建 `ncs_sdk/`（或从 `NCS_MIRROR_DIR` 复制）
5. 在 `ncs_sdk/.venv` 安装 Python 依赖（zephyr/nrf/mcuboot requirements）
6. 下载或复制 Zephyr SDK 到 `.local-tooling/zephyr-sdk-*`
7. `./setup.sh -h` 安装 SDK 主机工具（**不**注册全局 CMake）
8. `make build` + `make compile_commands` 冒烟验证

---

## 3. SDK 来源与内容

### 3.1 nRF Connect SDK（west workspace）

- **远程**：`https://github.com/nrfconnect/sdk-nrf`
- **标签**：`v2.7.0`
- **管理工具**：`west`（manifest 在 `nrf/west.yml`）

`ncs_sdk/` 顶层目录：

| 目录 | 说明 | 是否共享 |
|------|------|----------|
| `.west/` | west 元数据 | 共享（随 SDK） |
| `.venv/` | 项目 Python 环境 | 共享（工作区级重建） |
| `zephyr/` | Zephyr RTOS 内核与驱动 | 共享 |
| `nrf/` | Nordic 扩展（驱动、样本、协议栈配置） | 共享 |
| `nrfxlib/` | SoftDevice 控制器、MPSL 等二进制库 | 共享 |
| `modules/` | HAL/CMSIS/crypto 等 west 子模块 | 共享 |
| `bootloader/` | MCUboot | 共享 |
| `tools/` | NCS 工具脚本 | 共享 |

**不存在**独立的「nRF5 SDK」目录；若需裸机开发，需另建工程，不在本工作区范围。

### 3.2 CMSIS / HAL

位于 `ncs_sdk/modules/hal/`、`modules/cmsis/` 等，由 `west update` 拉取，**不应单独拷贝或手改**。

### 3.3 SEGGER

- nRF52840 DK 板载 **J-Link OB** 调试器（硬件）
- **JLinkExe** / **nrfjprog** 为**主机侧**工具，Rover 脚本**不捆绑下载**
- Zephyr SDK 内可能含 OpenOCD/J-Link  gdb 脚本，但 SEGGER 软件需用户自行安装

---

## 4. Toolchain 来源

| 工具 | 来源 | Rover 路径 |
|------|------|------------|
| ARM GCC（交叉编译） | Zephyr SDK 发布包 | `.local-tooling/zephyr-sdk-0.16.8/arm-zephyr-eabi/` |
| CMake / Ninja | `ncs_sdk/.venv`（uv pip 安装） | 虚拟环境内 |
| west | bootstrap venv + 项目 venv | `.local-tooling/.bootstrap-venv` + `ncs_sdk/.venv` |
| dtc / gperf 等 | 主机系统（brew/apt） | 系统 PATH |
| nrfutil | 本地目录（非 west 管理） | `.local-tooling/nrfutil/bin/nrfutil` |

Zephyr SDK 下载 URL 模式：

```
https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v0.16.8/zephyr-sdk-0.16.8_<host>.tar.xz
```

macOS arm64 主机包：`macos-aarch64`；x86_64：`macos-x86_64`。

---

## 5. 烧录工具

`Makefile` 中 `FLASH_RUNNER=auto` 优先级：

1. **nrfutil**（推荐；Rover 在 macOS 上 `west flash` 的 nrfutil runner 可能 EAGAIN，提供 `flash-direct` 绕过）
2. **nrfjprog**（Nordic Command Line Tools）
3. **jlink**（SEGGER J-Link）

烧录产物：`build/zephyr/zephyr.hex`

`flash-direct` 直接调用：

```bash
.local-tooling/nrfutil/bin/nrfutil device program --firmware build/zephyr/zephyr.hex --options reset=RESET_PIN
```

---

## 6. 调试配置

Rover **未提交** `.vscode/` 配置；开发工作流以 **命令行 + Makefile + clangd** 为主：

- `make compile_commands` → 符号链接 `compile_commands.json` → **clangd** 代码跳转/补全
- 硬件调试：nRF52840 DK 板载 J-Link，可用 `west debug` 或 VS Code Cortex-Debug 扩展（需在 `ble-projects` 新建）

文档参考：`rover/docs/nrf52840_ubuntu_vim_uv_setup.md`（全局环境说明）、`rover/docs/troubleshooting.md`（排障）。

---

## 7. BLE 相关配置

Rover **主应用未启用 BLE**（`prj.conf` 无 `CONFIG_BT`）；当前固件聚焦 SIM7080G 蜂窝模组 UART 桥接。

BLE 基础设施在 **NCS/Zephyr 样本**中已验证可用，路径示例：

| 类型 | 路径（相对 `ncs_sdk/`） |
|------|-------------------------|
| Zephyr 通用 | `zephyr/samples/bluetooth/broadcaster/`、`peripheral/` |
| Nordic 扩展 | `nrf/samples/bluetooth/peripheral_lbs/`、`central_uart/` 等 |

新工程的 hello 模板可参考 `zephyr/samples/basic/blinky` + `zephyr/samples/bluetooth/broadcaster` 组合最小功能。

---

## 8. 第三方依赖

| 依赖 | 管理方式 | 说明 |
|------|----------|------|
| Python 3.11 | uv venv | 构建脚本运行时 |
| west | pip（venv） | NCS workspace 管理 |
| cmake, ninja | pip（venv） | 构建后端 |
| Zephyr Python reqs | pip | `zephyr/scripts/requirements.txt` 等 |
| nrfxlib 二进制 | west | BLE 控制器固件等 |
| 业务 Python 工具 | `rover/tools/` | **产品专用**，不迁移 |

---

## 9. 目录分类（共享 vs 独立）

### 9.1 属于 SDK（应放在 `ble-projects/sdk/`，所有项目共享）

```
sdk/ncs/                    # 完整 west workspace（自 rover/ncs_sdk 复制）
  ├── .west/
  ├── zephyr/
  ├── nrf/
  ├── nrfxlib/
  ├── modules/              # 含 CMSIS、HAL、crypto 等
  └── bootloader/
```

### 9.2 属于 Toolchain（应放在 `ble-projects/toolchains/`，所有项目共享）

```
toolchains/
  ├── zephyr-sdk-0.16.8/    # ARM GCC + gdb + 相关工具
  ├── nrfutil/              # 烧录 CLI
  ├── .bootstrap-venv/      # west 初始化用
  └── cache/                # SDK 压缩包缓存（可选）
```

以及工作区级 Python 环境：

```
sdk/ncs/.venv/              # west、cmake、ninja、Python 构建依赖
```

### 9.3 属于业务代码（每个项目独立，放在 `ble-projects/projects/<name>/`）

```
projects/<app>/
  ├── src/                  # 应用 C 源码
  ├── CMakeLists.txt
  ├── prj.conf
  ├── boards/*.overlay      # 可选设备树
  ├── build/                # 编译产物（gitignore）
  └── Makefile              # 可薄封装，指向工作区 scripts/
```

**Rover 中属于业务、不应迁入通用工作区的部分**：

- `rover/src/main.c`（SIM7080G / MQTT 产品逻辑）
- `rover/tests/sim7080_*`（蜂窝模组回归测试）
- `rover/tools/`（Python 桥接脚本）
- `rover/docs/` 中产品路线、SIM7080 接线等文档

### 9.4 应共享的脚本与文档（`ble-projects/scripts/`、`docs/`）

- `setup_env.sh`（路径改为工作区根）
- 环境变量约定（`env.sh`）
- `docs/setup.md`、本分析报告
- `.vscode/`（tasks、launch、c_cpp_properties）
- `templates/nrf52840_app/`（新项目脚手架）

### 9.5 不应提交 Git 的目录

与 rover `.gitignore` 一致：

- `sdk/ncs/`（体积大，本地复制或镜像初始化）
- `toolchains/zephyr-sdk-*`、`toolchains/.bootstrap-venv`
- `projects/*/build/`
- `compile_commands.json`（符号链接）

---

## 10. 迁移到 ble-projects 的映射

| Rover 路径 | ble-projects 路径 |
|------------|-------------------|
| `rover/ncs_sdk/` | `ble-projects/sdk/ncs/` |
| `rover/.local-tooling/zephyr-sdk-*` | `ble-projects/toolchains/zephyr-sdk-*` |
| `rover/.local-tooling/nrfutil/` | `ble-projects/toolchains/nrfutil/` |
| `rover/.local-tooling/.bootstrap-venv/` | `ble-projects/toolchains/.bootstrap-venv/` |
| `rover/Makefile` + `scripts/` | `ble-projects/scripts/` + 各项目 `Makefile` |
| `rover/src/` + `prj.conf` | `ble-projects/projects/hello_nrf52840/`（最小示例） |
| （无）`.vscode/` | `ble-projects/.vscode/`（新建） |

---

## 11. 风险与注意事项

1. **路径硬编码**：`ncs_sdk/.venv` 和 west 缓存可能含绝对路径；从 rover 复制后需在 `ble-projects` 重建 `.venv`（`setup_env.sh` 已处理）。
2. **macOS 烧录**：优先使用 `make flash-direct` 规避 west nrfutil runner 的 EAGAIN。
3. **版本锁定**：在通用工作区保持 NCS v2.7.0 + Zephyr SDK 0.16.8，与 rover 已验证组合一致，升级需全工作区回归。
4. **无 nRF5 SDK**：若未来需要裸机 SDK，应在 `ble-projects` 另建 `sdk/nrf5/` 分支目录，与 NCS 并存但独立 Makefile。

---

## 12. 结论

Rover 的成功经验可归纳为：

- **Repo-local 自举**：SDK 与工具链放在工程旁，不污染全局、不重复下载。
- **west out-of-tree 构建**：SDK 一份、应用多份。
- **Makefile 统一入口**：降低 Zephyr 学习曲线。
- **compile_commands + clangd**：无 IDE 依赖的 LSP 支持。

`ble-projects/` 应将 SDK/工具链**上提至工作区根**共享，各 `projects/` 仅保留应用源码与 `build/`，从而实现长期维护的通用 nRF52840 开发框架。
