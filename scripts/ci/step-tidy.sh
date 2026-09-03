#!/usr/bin/env bash
# ci/step-tidy.sh — clang-tidy 静态分析步骤
# 环境变量: BUILD_DIR, SRC_DIR
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/common.sh"
_require_src

_skip_or_run "tidy" bash -c "
    cd '${SRC_DIR}'
    python3 scripts/run_tidy.py --build-dir '${BUILD_DIR}/build'
"
