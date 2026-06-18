# 共享 Makefile 片段：被各 projects/<name>/Makefile include
# 要求调用方先设置 APP_DIR := $(CURDIR)

BLE_PROJECTS_ROOT ?= $(abspath $(APP_DIR)/../..)
SCRIPTS_DIR := $(BLE_PROJECTS_ROOT)/scripts

NCS_DIR := $(BLE_PROJECTS_ROOT)/sdk/ncs
VENV_DIR := $(NCS_DIR)/.venv
ZEPHYR_BASE := $(NCS_DIR)/zephyr
LOCAL_TOOLING_DIR := $(BLE_PROJECTS_ROOT)/toolchains
ZEPHYR_SDK_INSTALL_DIR := $(firstword $(wildcard $(LOCAL_TOOLING_DIR)/zephyr-sdk-*))
LOCAL_NRFUTIL_HOME := $(LOCAL_TOOLING_DIR)/nrfutil

BOARD ?= nrf52840dk/nrf52840
FLASH_RUNNER ?= auto
BUILD_DIR := $(APP_DIR)/build

.PHONY: help build flash flash-direct clean menuconfig compile_commands

help:
	@echo "Project: $(notdir $(APP_DIR))"
	@echo "  make build              # 编译"
	@echo "  make flash              # 烧录 (west)"
	@echo "  make flash-direct       # 烧录 (nrfutil direct, macOS 推荐)"
	@echo "  make menuconfig         # Kconfig"
	@echo "  make compile_commands   # clangd 编译数据库"
	@echo "  make clean              # 清理 build/"
	@echo ""
	@echo "Variables: BOARD=$(BOARD)  FLASH_RUNNER=$(FLASH_RUNNER)"

build:
	@echo "Building $(notdir $(APP_DIR)) for $(BOARD)..."
	@bash -c '\
		if [ ! -f "$(VENV_DIR)/bin/activate" ]; then \
			echo "Missing sdk/ncs/.venv. Run from workspace root: make setup"; \
			exit 1; \
		fi; \
		source "$(VENV_DIR)/bin/activate" && \
		source "$(ZEPHYR_BASE)/zephyr-env.sh" && \
		if [ -n "$(ZEPHYR_SDK_INSTALL_DIR)" ]; then \
			export ZEPHYR_SDK_INSTALL_DIR="$(ZEPHYR_SDK_INSTALL_DIR)"; \
		else \
			echo "Warning: Zephyr SDK not found under $(LOCAL_TOOLING_DIR)"; \
		fi; \
		export NRFUTIL_HOME="$(LOCAL_NRFUTIL_HOME)"; \
		export PATH="$(LOCAL_NRFUTIL_HOME)/bin:$$PATH"; \
		cd "$(NCS_DIR)" && \
		west build -b "$(BOARD)" -d "$(BUILD_DIR)" "$(APP_DIR)" && \
		if [ -f "$(BUILD_DIR)/zephyr/zephyr.hex" ]; then \
			echo "Build OK: $(BUILD_DIR)/zephyr/zephyr.hex"; \
		else \
			echo "Build finished but zephyr.hex not found"; exit 1; \
		fi'

flash:
	@echo "Flashing $(notdir $(APP_DIR))..."
	@bash -c '\
		if [ ! -d "$(BUILD_DIR)" ]; then echo "Run: make build"; exit 1; fi; \
		if [ ! -f "$(VENV_DIR)/bin/activate" ]; then echo "Run: make setup"; exit 1; fi; \
		source "$(VENV_DIR)/bin/activate" && \
		source "$(ZEPHYR_BASE)/zephyr-env.sh" && \
		if [ -n "$(ZEPHYR_SDK_INSTALL_DIR)" ]; then export ZEPHYR_SDK_INSTALL_DIR="$(ZEPHYR_SDK_INSTALL_DIR)"; fi; \
		export NRFUTIL_HOME="$(LOCAL_NRFUTIL_HOME)"; \
		export PATH="$(LOCAL_NRFUTIL_HOME)/bin:$$PATH"; \
		runner="$(FLASH_RUNNER)"; \
		if [ "$$runner" = "auto" ]; then \
			if command -v nrfutil >/dev/null 2>&1; then runner="nrfutil"; \
			elif command -v nrfjprog >/dev/null 2>&1; then runner="nrfjprog"; \
			elif command -v JLinkExe >/dev/null 2>&1; then runner="jlink"; \
			else echo "No flash runner found."; exit 1; fi; \
		fi; \
		cd "$(NCS_DIR)" && west flash -r "$$runner" -d "$(BUILD_DIR)"'

flash-direct:
	@echo "Flashing via nrfutil device program..."
	@bash -c '\
		NRFUTIL="$(LOCAL_NRFUTIL_HOME)/bin/nrfutil"; \
		HEX="$(BUILD_DIR)/zephyr/zephyr.hex"; \
		if [ ! -x "$$NRFUTIL" ]; then echo "Missing $$NRFUTIL"; exit 1; fi; \
		if [ ! -f "$$HEX" ]; then echo "Run: make build"; exit 1; fi; \
		SN="$(NRFUTIL_SERIAL)"; \
		if [ -n "$$SN" ]; then \
			exec "$$NRFUTIL" device program --firmware "$$HEX" --serial-number "$$SN" --options reset=RESET_PIN; \
		else \
			exec "$$NRFUTIL" device program --firmware "$$HEX" --options reset=RESET_PIN; \
		fi'

clean:
	rm -rf "$(BUILD_DIR)"

menuconfig:
	@bash -c '\
		source "$(VENV_DIR)/bin/activate" && \
		source "$(ZEPHYR_BASE)/zephyr-env.sh" && \
		if [ -n "$(ZEPHYR_SDK_INSTALL_DIR)" ]; then export ZEPHYR_SDK_INSTALL_DIR="$(ZEPHYR_SDK_INSTALL_DIR)"; fi; \
		export NRFUTIL_HOME="$(LOCAL_NRFUTIL_HOME)"; \
		export PATH="$(LOCAL_NRFUTIL_HOME)/bin:$$PATH"; \
		cd "$(NCS_DIR)" && west build -t menuconfig -d "$(BUILD_DIR)" "$(APP_DIR)"'

compile_commands: build
	@ln -sf build/compile_commands.json "$(APP_DIR)/compile_commands.json"
	@ln -sf "$(APP_DIR)/compile_commands.json" "$(BLE_PROJECTS_ROOT)/compile_commands.json" 2>/dev/null || true
	@echo "Linked compile_commands.json for clangd"
