#!/usr/bin/env bash
# ci/step-test.sh — 单元测试步骤
# 环境变量: BUILD_DIR, SRC_DIR
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/common.sh"
_require_src

_skip_or_run "test" bash -c "
    cd '${BUILD_DIR}/build' && ctest --output-on-failure -L lcdriv
"
