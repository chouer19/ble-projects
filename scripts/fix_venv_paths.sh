#!/usr/bin/env bash
# 修正从其他机器/路径复制来的 .venv 中的文本路径（不修改 .pyc 等二进制）
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=env.sh
source "${SCRIPT_DIR}/env.sh"

OLD_PREFIX="${1:-/Users/chouer/Code/pet-lover/rover/ncs_sdk/.venv}"
NEW_PREFIX="${VENV_DIR}"

if [[ ! -d "${VENV_DIR}" ]]; then
    echo "Missing ${VENV_DIR}"
    exit 1
fi

echo "Rewriting venv text paths:"
echo "  ${OLD_PREFIX} -> ${NEW_PREFIX}"

# pyvenv.cfg
if [[ -f "${VENV_DIR}/pyvenv.cfg" ]]; then
    sed -i '' "s|${OLD_PREFIX}|${NEW_PREFIX}|g" "${VENV_DIR}/pyvenv.cfg" 2>/dev/null || \
    sed -i "s|${OLD_PREFIX}|${NEW_PREFIX}|g" "${VENV_DIR}/pyvenv.cfg"
fi

# bin/ 下脚本（跳过二进制）
for f in "${VENV_DIR}/bin/"*; do
    [[ -f "$f" ]] || continue
    if file "$f" | grep -q "text"; then
        sed -i '' "s|${OLD_PREFIX}|${NEW_PREFIX}|g" "$f" 2>/dev/null || \
        sed -i "s|${OLD_PREFIX}|${NEW_PREFIX}|g" "$f"
    fi
done

echo "Done."
