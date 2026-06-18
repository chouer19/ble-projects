#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=env.sh
source "${SCRIPT_DIR}/env.sh"

CACHE_DIR="${LOCAL_TOOLING_DIR}/cache"
NCS_REMOTE_URL="${NCS_REMOTE_URL:-https://github.com/nrfconnect/sdk-nrf}"

HOST_OS="$(uname -s)"
HOST_ARCH="$(uname -m)"
case "${HOST_OS}" in
    Linux)
        ZEPHYR_SDK_HOST="linux-x86_64"
        ;;
    Darwin)
        if [[ "${HOST_ARCH}" == "arm64" ]]; then
            ZEPHYR_SDK_HOST="macos-aarch64"
        elif [[ "${HOST_ARCH}" == "x86_64" ]]; then
            ZEPHYR_SDK_HOST="macos-x86_64"
        else
            ZEPHYR_SDK_HOST="macos-${HOST_ARCH}"
        fi
        ;;
    *)
        ZEPHYR_SDK_HOST="unknown"
        ;;
esac
ZEPHYR_SDK_TARBALL="zephyr-sdk-${ZEPHYR_SDK_VERSION}_${ZEPHYR_SDK_HOST}.tar.xz"
ZEPHYR_SDK_URL="${ZEPHYR_SDK_URL:-https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v${ZEPHYR_SDK_VERSION}/${ZEPHYR_SDK_TARBALL}}"

NCS_MIRROR_DIR="${NCS_MIRROR_DIR:-}"
ZEPHYR_SDK_MIRROR_DIR="${ZEPHYR_SDK_MIRROR_DIR:-}"
ROVER_NCS_MIRROR="${ROVER_NCS_MIRROR:-/Users/chouer/Code/pet-lover/rover/ncs_sdk}"
ROVER_TOOLCHAIN_MIRROR="${ROVER_TOOLCHAIN_MIRROR:-/Users/chouer/Code/pet-lover/rover/.local-tooling}"

SKIP_APT=0
SKIP_BUILD_CHECK=0
FORCE=0
UV_BIN="${UV_BIN:-}"

log() { printf '\n==> %s\n' "$*"; }
warn() { printf '\n[warn] %s\n' "$*" >&2; }
die() { printf '\n[error] %s\n' "$*" >&2; exit 1; }

usage() {
    cat <<'EOF'
Usage:
  bash scripts/setup_env.sh [options]

Options:
  --skip-apt          Skip apt/brew dependency checks.
  --skip-build-check  Skip final hello_nrf52840 build verification.
  --force             Remove partial local setup directories before rebuilding.
  --help              Show this help message.

Environment overrides:
  NCS_VERSION               Default: v2.7.0
  PYTHON_VERSION            Default: 3.11
  ZEPHYR_SDK_VERSION        Default: 0.16.8
  NCS_MIRROR_DIR            Optional local mirror for sdk/ncs
  ZEPHYR_SDK_MIRROR_DIR     Optional local mirror for toolchains/zephyr-sdk-*
  ROVER_NCS_MIRROR          Default rover ncs_sdk path (auto-copy if present)
  ROVER_TOOLCHAIN_MIRROR    Default rover .local-tooling path (auto-copy if present)

Copies from rover when mirrors exist; otherwise downloads via west/curl.
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --skip-apt) SKIP_APT=1 ;;
        --skip-build-check) SKIP_BUILD_CHECK=1 ;;
        --force) FORCE=1 ;;
        --help) usage; exit 0 ;;
        *) die "Unknown option: $1" ;;
    esac
    shift
done

require_cmd() {
    command -v "$1" >/dev/null 2>&1 || die "Missing required command: $1"
}

ensure_supported_host() {
    case "${HOST_OS}" in
        Linux) [[ "${HOST_ARCH}" == "x86_64" ]] || die "Linux host currently supports x86_64 only." ;;
        Darwin) [[ "${HOST_ARCH}" == "arm64" || "${HOST_ARCH}" == "x86_64" ]] || die "macOS supports arm64/x86_64 only." ;;
        *) die "Unsupported host OS: ${HOST_OS}" ;;
    esac
}

install_apt_deps() {
    if [[ "${SKIP_APT}" == "1" ]]; then
        log "Skipping host dependency installation"
        return
    fi
    if [[ "${HOST_OS}" == "Linux" ]]; then
        require_cmd sudo
        require_cmd apt-get
        log "Installing Ubuntu build dependencies"
        sudo apt-get update
        sudo apt-get install -y --no-install-recommends \
            git cmake ninja-build gperf ccache dfu-util device-tree-compiler \
            wget curl ca-certificates xz-utils file make gcc gcc-multilib \
            g++-multilib libsdl2-dev libmagic1 python3-dev
        return
    fi
    if [[ "${HOST_OS}" == "Darwin" ]]; then
        if ! command -v brew >/dev/null 2>&1; then
            die "Homebrew is required on macOS. Install brew first, or rerun with --skip-apt."
        fi
        log "Checking common macOS dependencies via Homebrew"
        for dep in cmake ninja dtc gperf ccache; do
            if ! brew list --versions "${dep}" >/dev/null 2>&1; then
                warn "Missing brew package: ${dep} (install with: brew install ${dep})"
            fi
        done
    fi
}

ensure_uv() {
    if [[ -n "${UV_BIN}" && -x "${UV_BIN}" ]]; then return; fi
    if command -v uv >/dev/null 2>&1; then UV_BIN="$(command -v uv)"; return; fi
    log "Installing uv"
    require_cmd curl
    curl -LsSf https://astral.sh/uv/install.sh | sh
    for candidate in "$HOME/.local/bin/uv" "$HOME/.cargo/bin/uv"; do
        if [[ -x "${candidate}" ]]; then UV_BIN="${candidate}"; return; fi
    done
    command -v uv >/dev/null 2>&1 && UV_BIN="$(command -v uv)" && return
    die "uv installation completed, but the binary is still not available."
}

bootstrap_west() {
    if [[ -x "${BOOTSTRAP_VENV}/bin/west" ]]; then return; fi
    log "Creating bootstrap west environment"
    mkdir -p "${LOCAL_TOOLING_DIR}"
    "${UV_BIN}" venv --python "${PYTHON_VERSION}" "${BOOTSTRAP_VENV}"
    (
        # shellcheck disable=SC1091
        source "${BOOTSTRAP_VENV}/bin/activate"
        "${UV_BIN}" pip install west
    )
}

reset_dir_if_requested() {
    local path="$1"
    if [[ "${FORCE}" == "1" && -e "${path}" ]]; then
        log "Removing existing path: ${path}"
        rm -rf "${path}"
    fi
}

copy_tree() {
    local src="$1"
    local dst="$2"
    mkdir -p "${dst}"
    rsync -a "${src}/" "${dst}/"
}

ensure_ncs_workspace() {
    if [[ -d "${NCS_DIR}/.west" ]]; then
        log "Reusing existing sdk/ncs workspace"
        return
    fi
    reset_dir_if_requested "${NCS_DIR}"
    if [[ -e "${NCS_DIR}" ]]; then
        die "Found ${NCS_DIR}, but it is not a valid west workspace. Re-run with --force."
    fi
    if [[ -n "${NCS_MIRROR_DIR}" ]]; then
        [[ -d "${NCS_MIRROR_DIR}/.west" ]] || die "NCS_MIRROR_DIR invalid: ${NCS_MIRROR_DIR}"
        log "Copying NCS from mirror: ${NCS_MIRROR_DIR}"
        copy_tree "${NCS_MIRROR_DIR}" "${NCS_DIR}"
        rm -rf "${NCS_DIR}/.venv"
        return
    fi
    if [[ -d "${ROVER_NCS_MIRROR}/.west" ]]; then
        log "Copying NCS from rover: ${ROVER_NCS_MIRROR}"
        copy_tree "${ROVER_NCS_MIRROR}" "${NCS_DIR}"
        rm -rf "${NCS_DIR}/.venv"
        return
    fi
    log "Initializing NCS workspace (${NCS_VERSION})"
    "${BOOTSTRAP_VENV}/bin/west" init -m "${NCS_REMOTE_URL}" --mr "${NCS_VERSION}" "${NCS_DIR}"
    ( cd "${NCS_DIR}" && "${BOOTSTRAP_VENV}/bin/west" update )
}

copy_venv_from_rover() {
    local rover_venv="${ROVER_NCS_MIRROR}/.venv"
    [[ -d "${rover_venv}" ]] || return 1
    log "Copying sdk/ncs/.venv from rover (avoids re-downloading pip deps)"
    rm -rf "${NCS_DIR}/.venv"
    copy_tree "${rover_venv}" "${NCS_DIR}/.venv"
    bash "${SCRIPT_DIR}/fix_venv_paths.sh" "${rover_venv}"
}

ensure_project_venv() {
    [[ -d "${NCS_DIR}" ]] || die "Missing ${NCS_DIR}"
    if [[ ! -d "${NCS_DIR}/.venv" ]]; then
        if ! copy_venv_from_rover; then
            log "Creating sdk/ncs/.venv (fresh pip install)"
            "${UV_BIN}" venv --python "${PYTHON_VERSION}" "${NCS_DIR}/.venv"
            (
                cd "${NCS_DIR}"
                # shellcheck disable=SC1091
                source "${NCS_DIR}/.venv/bin/activate"
                "${UV_BIN}" pip install west
                "${UV_BIN}" pip install -r zephyr/scripts/requirements.txt
                "${UV_BIN}" pip install -r nrf/scripts/requirements.txt
                "${UV_BIN}" pip install -r bootloader/mcuboot/scripts/requirements.txt
            )
        fi
    else
        log "Reusing existing sdk/ncs/.venv"
    fi
}

ensure_build_tools_in_venv() {
    log "Ensuring CMake/Ninja in sdk/ncs/.venv"
    (
        cd "${NCS_DIR}"
        # shellcheck disable=SC1091
        source "${NCS_DIR}/.venv/bin/activate"
        "${UV_BIN}" pip install cmake ninja
    )
}

ensure_zephyr_sdk() {
    local sdk_dir="${LOCAL_TOOLING_DIR}/zephyr-sdk-${ZEPHYR_SDK_VERSION}"
    if [[ -x "${sdk_dir}/setup.sh" ]]; then
        log "Reusing existing Zephyr SDK"
    else
        reset_dir_if_requested "${sdk_dir}"
        if [[ -e "${sdk_dir}" ]]; then
            die "Found ${sdk_dir}, but it looks incomplete. Re-run with --force."
        fi
        mkdir -p "${LOCAL_TOOLING_DIR}" "${CACHE_DIR}"
        if [[ -n "${ZEPHYR_SDK_MIRROR_DIR}" ]]; then
            [[ -f "${ZEPHYR_SDK_MIRROR_DIR}/setup.sh" ]] || die "ZEPHYR_SDK_MIRROR_DIR invalid"
            log "Copying Zephyr SDK from mirror: ${ZEPHYR_SDK_MIRROR_DIR}"
            copy_tree "${ZEPHYR_SDK_MIRROR_DIR}" "${sdk_dir}"
        elif [[ -f "${ROVER_TOOLCHAIN_MIRROR}/zephyr-sdk-${ZEPHYR_SDK_VERSION}/setup.sh" ]]; then
            log "Copying Zephyr SDK from rover"
            copy_tree "${ROVER_TOOLCHAIN_MIRROR}/zephyr-sdk-${ZEPHYR_SDK_VERSION}" "${sdk_dir}"
        else
            local_archive="${CACHE_DIR}/${ZEPHYR_SDK_TARBALL}"
            if [[ ! -f "${local_archive}" ]]; then
                log "Downloading Zephyr SDK ${ZEPHYR_SDK_VERSION}"
                curl -L "${ZEPHYR_SDK_URL}" -o "${local_archive}"
            fi
            log "Extracting Zephyr SDK"
            tar -xf "${local_archive}" -C "${LOCAL_TOOLING_DIR}"
        fi
    fi
    log "Installing Zephyr SDK host tools (setup.sh -h)"
    ( cd "${sdk_dir}" && ./setup.sh -h )
}

ensure_nrfutil() {
    if [[ -x "${LOCAL_NRFUTIL_HOME}/bin/nrfutil" ]]; then
        log "Reusing existing nrfutil"
        return
    fi
    if [[ -x "${ROVER_TOOLCHAIN_MIRROR}/nrfutil/bin/nrfutil" ]]; then
        log "Copying nrfutil from rover"
        copy_tree "${ROVER_TOOLCHAIN_MIRROR}/nrfutil" "${LOCAL_NRFUTIL_HOME}"
        return
    fi
    warn "nrfutil not found locally. Install manually or copy from rover .local-tooling/nrfutil"
}

report_flash_tool_status() {
    export PATH="${LOCAL_NRFUTIL_HOME}/bin:${PATH}"
    if command -v nrfutil >/dev/null 2>&1; then log "Detected flash tool: nrfutil"; return; fi
    if command -v nrfjprog >/dev/null 2>&1; then log "Detected flash tool: nrfjprog"; return; fi
    if command -v JLinkExe >/dev/null 2>&1; then log "Detected flash tool: JLinkExe"; return; fi
    warn "No flash tool detected. See docs/setup.md."
}

run_build_check() {
    if [[ "${SKIP_BUILD_CHECK}" == "1" ]]; then
        log "Skipping final build verification"
        return
    fi
    log "Running hello_nrf52840 build smoke test"
    ( cd "${BLE_PROJECTS_ROOT}/projects/hello_nrf52840" && make build && make compile_commands )
}

main() {
    ensure_supported_host
    install_apt_deps
    ensure_uv
    bootstrap_west
    ensure_ncs_workspace
    ensure_project_venv
    ensure_build_tools_in_venv
    ensure_zephyr_sdk
    ensure_nrfutil
    report_flash_tool_status
    run_build_check
    log "Setup complete"
    printf '\nNext steps:\n'
    printf '  cd "%s/projects/hello_nrf52840"\n' "${BLE_PROJECTS_ROOT}"
    printf '  make build\n'
    printf '  make flash\n'
}

main "$@"
