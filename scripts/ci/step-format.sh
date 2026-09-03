#!/usr/bin/env bash
# ci/step-format.sh — clang-format 检查步骤
# 环境变量: BUILD_DIR, SRC_DIR, CHANGED_FILES（可选）
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/common.sh"
_require_src

CHANGED_FILES="${CHANGED_FILES:-}"

if [[ -z "$CHANGED_FILES" ]]; then
    echo "no changed C++ files"
    exit 0
fi

command -v clang-format >/dev/null 2>&1 || { echo "clang-format not found"; exit 1; }

rc=0
for f in $CHANGED_FILES; do
    [[ -f "${SRC_DIR}/${f}" ]] || continue
    echo "  checking: ${f}"
    clang-format --dry-run --Werror "${SRC_DIR}/${f}" || rc=1
done
exit ${rc}
