# ble-projects 环境配置指南

通用 nRF52840 开发工作区，基于 `pet-lover/rover` 已验证的 NCS + Zephyr SDK 环境提取而来。

## 目录结构

```text
ble-projects/
├── sdk/
│   └── ncs/                    # nRF Connect SDK west workspace（共享）
├── toolchains/
│   ├── zephyr-sdk-0.16.8/      # ARM GCC 交叉编译器（共享）
│   ├── nrfutil/                # 烧录 CLI（共享）
│   └── .bootstrap-venv/        # west init 用（共享）
├── templates/
│   └── nrf52840_app/           # 新项目脚手架
├── projects/
│   └── hello_nrf52840/         # 示例工程（LED + UART + BLE 广播）
├── scripts/
│   ├── env.sh                  # 环境变量
│   ├── setup_env.sh            # 一键初始化
│   ├── fix_venv_paths.sh       # 复制 venv 后修正路径
│   └── project.mk              # 各项目 Makefile 共享片段
├── docs/
│   ├── rover-analysis.md       # rover 工程分析
│   └── setup.md                # 本文档
├── .vscode/                    # Cursor/VS Code 任务与调试
├── Makefile                    # 工作区入口
└── README.md
```

### 共享 vs 独立

| 共享（工作区一份） | 独立（每个项目一份） |
|-------------------|---------------------|
| `sdk/ncs/` | `projects/<app>/src/` |
| `toolchains/zephyr-sdk-*` | `projects/<app>/prj.conf` |
| `toolchains/nrfutil/` | `projects/<app>/CMakeLists.txt` |
| `sdk/ncs/.venv/` | `projects/<app>/build/` |
| `scripts/` | `projects/<app>/boards/*.overlay`（可选） |

---

## 版本信息

| 组件 | 版本 | 来源 |
|------|------|------|
| nRF Connect SDK | **v2.7.0** | `sdk/ncs/nrf/VERSION` |
| Zephyr RTOS | 3.6.99-ncs2 | NCS 捆绑 |
| Zephyr SDK | **0.16.8** | `toolchains/zephyr-sdk-0.16.8/` |
| ARM GCC | 12.2.0 | Zephyr SDK 内置 |
| nrfutil | **8.1.1** | `toolchains/nrfutil/` |
| Python | 3.11 | `sdk/ncs/.venv` |
| 默认板型 | `nrf52840dk/nrf52840` | Makefile `BOARD` |

**说明**：本工作区使用 NCS（Zephyr），**不包含**传统 nRF5 SDK。CMSIS/HAL/SEGGER 模块位于 `sdk/ncs/modules/` 内，由 west 管理。

---

## 首次初始化

### 方式 A：从 rover 复制（推荐，本机已存在 rover）

SDK 与工具链已从 rover 复制到本工作区。若需在新机器上重建：

```bash
cd /Users/chouer/Code/ble-projects

# 指定 rover 镜像路径（默认已内置）
ROVER_NCS_MIRROR=/path/to/pet-lover/rover/ncs_sdk \
ROVER_TOOLCHAIN_MIRROR=/path/to/pet-lover/rover/.local-tooling \
bash scripts/setup_env.sh --skip-apt
```

### 方式 B：无 rover 时从网络下载

```bash
cd ble-projects
make setup
# 或
bash scripts/setup_env.sh
```

脚本会：安装 uv → 初始化/复制 NCS → 配置 Python venv → 安装 Zephyr SDK → 冒烟编译 hello 工程。

---

## 编译

```bash
cd ble-projects

# 编译默认项目 hello_nrf52840
make build

# 编译指定项目
make build PROJECT=my_app

# 或在项目目录内
cd projects/hello_nrf52840
make build
```

成功产物：

- `projects/<app>/build/zephyr/zephyr.hex` — 烧录用
- `projects/<app>/build/zephyr/zephyr.elf` — 调试用

---

## 烧录

连接 nRF52840 DK 的 **J2（J-Link）** 接口，打开板载电源 SW8。

```bash
# macOS 推荐：绕过 west nrfutil runner 的 EAGAIN 问题
make flash-direct

# 或 west flash（自动选择 runner）
make flash

# 指定项目
make flash-direct PROJECT=hello_nrf52840

# 多块板时指定序列号
cd projects/hello_nrf52840
NRFUTIL_SERIAL=<贴纸序列号> make flash-direct
```

烧录工具优先级（`FLASH_RUNNER=auto`）：

1. `toolchains/nrfutil/bin/nrfutil`
2. 系统 `nrfjprog`（Nordic Command Line Tools）
3. 系统 `JLinkExe`

### spider 四足机器人

```bash
# nRF52840 DK（默认）
make build PROJECT=spider
make flash-direct PROJECT=spider

# Seeed XIAO BLE / Sense
make build PROJECT=spider BOARD=xiao_ble
make flash-uf2 PROJECT=spider BOARD=xiao_ble   # 先双击 RESET 进入 UF2

screen /dev/cu.usbmodem<序列号>1 115200
```

操作命令、BLE 协议、iPad/iPhone App 部署见 [projects/spider/docs/operations-guide.md](../projects/spider/docs/operations-guide.md)。

---

## 调试

### 串口日志

DK 的 USB 虚拟串口（通常 115200）可看到 `printk` 输出：

```bash
# macOS 示例
screen /dev/tty.usbmodem* 115200
```

hello 工程上电后会打印 banner 和 LED 状态，并启动 BLE 不可连接广播（设备名 `Hello_nRF52840`）。

### J-Link 调试（VS Code / Cursor）

1. 安装扩展：**Cortex-Debug**
2. 先编译：`make build`
3. 按 F5 选择 **ble: Debug hello_nrf52840 (J-Link)**

`launch.json` 已配置 `nRF52840_xxAA` + SWD + 板载 J-Link。

---

## IDE / LSP 配置

用 Cursor 打开 **`ble-projects/`** 根目录（非单个 project 子目录）。

```bash
make compile_commands
# 或
make compile_commands PROJECT=hello_nrf52840
```

会在工作区根生成 `compile_commands.json` 符号链接，供：

- **clangd** 代码跳转与补全
- **C/C++** 扩展 IntelliSense

`.vscode/tasks.json` 提供：setup / build / flash / compile_commands 任务。

---

## 创建新项目

```bash
cd ble-projects

# 1. 从模板复制
cp -r templates/nrf52840_app projects/my_app

# 2. 修改项目名
#    编辑 projects/my_app/CMakeLists.txt 中 project(my_app)

# 3. 编写 src/main.c、prj.conf

# 4. 编译烧录
make build PROJECT=my_app
make flash-direct PROJECT=my_app
```

可选：添加 `projects/my_app/boards/nrf52840dk_nrf52840.overlay` 自定义设备树。

参考 NCS 样本（在已初始化的 `sdk/ncs/` 内）：

- LED：`zephyr/samples/basic/blinky`
- BLE：`zephyr/samples/bluetooth/broadcaster`
- Nordic BLE：`nrf/samples/bluetooth/peripheral_lbs`

---

## 环境变量

| 变量 | 默认 | 说明 |
|------|------|------|
| `NCS_VERSION` | v2.7.0 | NCS 标签 |
| `ZEPHYR_SDK_VERSION` | 0.16.8 | Zephyr SDK 版本 |
| `PYTHON_VERSION` | 3.11 | venv Python |
| `BOARD` | nrf52840dk/nrf52840 | 目标板 |
| `FLASH_RUNNER` | auto | 烧录后端 |
| `PROJECT` | hello_nrf52840 | 工作区 Makefile 目标项目 |
| `ROVER_NCS_MIRROR` | pet-lover/rover/ncs_sdk | 复制源 |
| `ROVER_TOOLCHAIN_MIRROR` | pet-lover/rover/.local-tooling | 工具链复制源 |

---

## 常见问题

### 1. `Missing sdk/ncs/.venv`

```bash
bash scripts/setup_env.sh --skip-apt
```

若本机有 rover，脚本会优先从 rover 复制 `.venv` 并修正路径。

### 2. macOS `make flash` 报 EAGAIN

改用 `make flash-direct`。

### 3. `Could NOT find Dtc`

```bash
brew install dtc
```

### 4. 无连接设备时烧录失败

`nrfutil device list` 显示 0 台设备时，请检查 USB 线、SW8 电源、是否使用 J2 口。

### 5. 升级 SDK 版本

修改 `NCS_VERSION` / `ZEPHYR_SDK_VERSION` 后：

```bash
bash scripts/setup_env.sh --force --skip-apt
```

需全工作区回归 `make build` 与各项目测试。

---

## 验证清单（已完成）

- [x] SDK 路径：`sdk/ncs/` 含 `.west`、`zephyr`、`nrf`
- [x] Toolchain 路径：`toolchains/zephyr-sdk-0.16.8/`
- [x] hello_nrf52840 编译通过（179/179）
- [x] 链接生成 `zephyr.elf` / `zephyr.hex`
- [x] `compile_commands.json` 可用于 LSP
- [x] `.vscode` 任务与调试配置就绪
