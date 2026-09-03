#!/usr/bin/env bash
# ci/common.sh — CI 共享工具函数
# 由各 step 脚本 source，不单独执行。

# ---- 颜色 ----
RED=$'\033[0;31m'
GREEN=$'\033[0;32m'
YELLOW=$'\033[1;33m'
NC=$'\033[0m'

# ---- 环境变量约定 ----
# BUILD_DIR    — CI 构建根目录（由 gerrit-ci.sh 传入）
# SRC_DIR      — 源码目录（通常 $BUILD_DIR/src）
# CHANGED_FILES— 变更的 C++ 文件列表（空格分隔）

_require_env() {
    local name="$1"
    if [[ -z "${!name:-}" ]]; then
        echo -e "${RED}错误: 环境变量 ${name} 未设置${NC}" >&2
        exit 1
    fi
}

_require_src() {
    _require_env BUILD_DIR
    _require_env SRC_DIR
    if [[ ! -d "$SRC_DIR" ]]; then
        echo -e "${RED}错误: 源码目录不存在: ${SRC_DIR}${NC}" >&2
        exit 1
    fi
}

_has_cmake() {
    [[ -f "${SRC_DIR}/CMakeLists.txt" ]]
}

_skip_or_run() {
    local label="$1"
    shift
    if ! _has_cmake; then
        echo "SKIP: 无 CMakeLists.txt（源码未落地）"
        return 0
    fi
    "$@"
}
