#!/usr/bin/env bash
# ci/step-build.sh — 编译步骤
# 环境变量: BUILD_DIR, SRC_DIR
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/common.sh"
_require_src

_skip_or_run "build" bash -c "
    cmake -B '${BUILD_DIR}/build' '${SRC_DIR}' \
        -DCMAKE_BUILD_TYPE=Release \
        -DLCDRIV_BUILD_TESTS=ON \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    && cmake --build '${BUILD_DIR}/build' --target lcdriv_ut -j \$(nproc)
"
