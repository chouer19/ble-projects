# ble-projects 工作区环境变量（被 Makefile / setup_env.sh source）
# 用法: source "$(git rev-parse --show-toplevel 2>/dev/null || echo "$BLE_PROJECTS_ROOT")/scripts/env.sh"

if [[ -z "${BLE_PROJECTS_ROOT:-}" ]]; then
    _env_script_dir="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)"
    BLE_PROJECTS_ROOT="$(cd "${_env_script_dir}/.." && pwd)"
fi

export BLE_PROJECTS_ROOT

NCS_DIR="${BLE_PROJECTS_ROOT}/sdk/ncs"
LOCAL_TOOLING_DIR="${BLE_PROJECTS_ROOT}/toolchains"
VENV_DIR="${NCS_DIR}/.venv"
ZEPHYR_BASE="${NCS_DIR}/zephyr"
BOOTSTRAP_VENV="${LOCAL_TOOLING_DIR}/.bootstrap-venv"
LOCAL_NRFUTIL_HOME="${LOCAL_TOOLING_DIR}/nrfutil"

NCS_VERSION="${NCS_VERSION:-v2.7.0}"
PYTHON_VERSION="${PYTHON_VERSION:-3.11}"
ZEPHYR_SDK_VERSION="${ZEPHYR_SDK_VERSION:-0.16.8}"
ZEPHYR_SDK_INSTALL_DIR="${ZEPHYR_SDK_INSTALL_DIR:-$(ls -d "${LOCAL_TOOLING_DIR}/zephyr-sdk-${ZEPHYR_SDK_VERSION}" 2>/dev/null | head -1)}"

BOARD="${BOARD:-nrf52840dk/nrf52840}"
FLASH_RUNNER="${FLASH_RUNNER:-auto}"

export NCS_DIR LOCAL_TOOLING_DIR VENV_DIR ZEPHYR_BASE BOOTSTRAP_VENV LOCAL_NRFUTIL_HOME
export NCS_VERSION PYTHON_VERSION ZEPHYR_SDK_VERSION ZEPHYR_SDK_INSTALL_DIR BOARD FLASH_RUNNER
