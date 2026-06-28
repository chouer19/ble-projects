# ble-projects 工作区 Makefile

BLE_PROJECTS_ROOT := $(CURDIR)
DEFAULT_PROJECT := hello_nrf52840
PROJECT ?= $(DEFAULT_PROJECT)
PROJECT_DIR := $(BLE_PROJECTS_ROOT)/projects/$(PROJECT)
UF2_VOLUME ?= XIAO-SENSE

.PHONY: all help setup build flash flash-direct flash-uf2 clean menuconfig compile_commands

all: build

help:
	@echo "ble-projects workspace"
	@echo ""
	@echo "  make setup                          # 初始化 SDK/工具链（优先从 rover 复制）"
	@echo "  make build   [PROJECT=<name>]       # 编译项目（默认 hello_nrf52840）"
	@echo "  make flash   [PROJECT=<name>]"
	@echo "  make flash-direct                   # DK + J-Link"
	@echo "  make flash-uf2                        # XIAO UF2（需 Bootloader 盘 $(UF2_VOLUME)）"
	@echo "  make compile_commands"
	@echo "  make clean"
	@echo ""
	@echo "Projects under projects/:"
	@ls -1 "$(BLE_PROJECTS_ROOT)/projects" 2>/dev/null || true

setup:
	@bash scripts/setup_env.sh --skip-build-check

build flash flash-direct flash-uf2 clean menuconfig compile_commands:
	@if [ ! -d "$(PROJECT_DIR)" ]; then \
		echo "Unknown project: $(PROJECT)"; exit 1; \
	fi
	@$(MAKE) -C "$(PROJECT_DIR)" $@
