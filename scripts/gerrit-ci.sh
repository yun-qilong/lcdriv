#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<EOF
用法: $0 [选项]

选项:
  --ref REF            Gerrit patchset ref (如 refs/changes/01/101/1)
  --project PROJECT    Gerrit 项目名 (默认: lcdriv)
  --branch BRANCH      目标分支 (默认: main)
  --gerrit-host HOST   Gerrit 主机 (默认: localhost)
  --gerrit-port PORT   Gerrit SSH 端口 (默认: 19418)
  --gerrit-user USER   Gerrit SSH 用户 (默认: qilyun)
  --build-dir DIR      构建目录 (默认: /tmp/lcdriv-ci-build)
  --help               显示帮助
EOF
  exit 0
}

# ---- 参数解析 -------------------------------------------------------
GERRIT_REF=""
PROJECT="${GERRIT_PROJECT:-lcdriv}"
BRANCH="${GERRIT_BRANCH:-main}"
GERRIT_HOST="${GERRIT_HOST:-localhost}"
GERRIT_PORT="${GERRIT_PORT:-19418}"
GERRIT_USER="${GERRIT_USER:-qilyun}"
BUILD_DIR="${BUILD_DIR:-/tmp/lcdriv-ci-build}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --ref) GERRIT_REF="$2"; shift 2 ;;
    --project) PROJECT="$2"; shift 2 ;;
    --branch) BRANCH="$2"; shift 2 ;;
    --gerrit-host) GERRIT_HOST="$2"; shift 2 ;;
    --gerrit-port) GERRIT_PORT="$2"; shift 2 ;;
    --gerrit-user) GERRIT_USER="$2"; shift 2 ;;
    --build-dir) BUILD_DIR="$2"; shift 2 ;;
    --help) usage ;;
    *) echo "未知选项: $1"; exit 1 ;;
  esac
done

RESULT_FILE="${BUILD_DIR}/ci-result.json"
GERRIT_URL="ssh://${GERRIT_USER}@${GERRIT_HOST}:${GERRIT_PORT}/${PROJECT}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# ---- 工具函数 -------------------------------------------------------
FAILED_COUNT=0

run_check() {
  local name="$1"
  local category="$2"
  shift 2
  local start_time end_time exit_code

  start_time=$(date +%s)
  set +e
  "$@" 2>&1 | tee "${BUILD_DIR}/${name}.log"
  exit_code=${PIPESTATUS[0]}
  set -e
  end_time=$(date +%s)

  local status="SUCCESS" summary="passed"
  if [[ $exit_code -ne 0 ]]; then
    status="FAILED"
    summary="failed (exit code ${exit_code})"
    FAILED_COUNT=$((FAILED_COUNT + 1))
  fi

  cat >> "${RESULT_FILE}.tmp" <<JSON
  {
    "name": "${name}",
    "category": "${category}",
    "status": "${status}",
    "summary": "${summary}",
    "duration_ms": $(( (end_time - start_time) * 1000 )),
    "start_time": "${start_time}",
    "end_time": "${end_time}"
  },
JSON
}

finish_result() {
  sed -i '$ s/,$//' "${RESULT_FILE}.tmp"
  echo ']' >> "${RESULT_FILE}.tmp"
  mv "${RESULT_FILE}.tmp" "${RESULT_FILE}"
}

# ---- 主流程 ---------------------------------------------------------
main() {
  if [[ -z "${GERRIT_REF}" ]]; then
    echo "错误: 需要 --ref 参数"
    exit 1
  fi

  echo "=== Gerrit CI ==="
  echo "Project : ${PROJECT}"
  echo "Ref     : ${GERRIT_REF}"
  echo "Host    : ${GERRIT_HOST}:${GERRIT_PORT}"

  rm -rf "${BUILD_DIR}"
  mkdir -p "${BUILD_DIR}"

  # ---- 拉取代码 ---------------------------------------------------
  echo ""
  echo ">>> 拉取代码: branch=${BRANCH} ref=${GERRIT_REF}"
  git init "${BUILD_DIR}/src"
  pushd "${BUILD_DIR}/src" > /dev/null
  git fetch "${GERRIT_URL}" "${BRANCH}" --depth=50
  git checkout -b ci-target FETCH_HEAD
  git fetch "${GERRIT_URL}" "${GERRIT_REF}" --depth=1
  local CHERRY_SHA
  CHERRY_SHA=$(git rev-parse FETCH_HEAD)
  if ! git cherry-pick --strategy-option=theirs "${CHERRY_SHA}"; then
    echo "WARNING: cherry-pick 冲突，使用 change 原始代码"
    git checkout "${CHERRY_SHA}"
  fi

  local BASE_SHA
  BASE_SHA=$(git rev-parse HEAD~1 2>/dev/null || echo "HEAD")
  local CHANGED_FILES
  CHANGED_FILES=$(git diff --name-only --diff-filter=ACMR "${BASE_SHA}" HEAD -- '*.cpp' '*.hpp' || true)
  popd > /dev/null

  export BUILD_DIR SRC_DIR="${BUILD_DIR}/src" CHANGED_FILES
  echo '[' > "${RESULT_FILE}.tmp"

  # ---- 各步骤（独立脚本）-----------------------------------------
  local steps=(
    "build:build:${SCRIPT_DIR}/ci/step-build.sh"
    "unit-tests:test:${SCRIPT_DIR}/ci/step-test.sh"
    "clang-format:style:${SCRIPT_DIR}/ci/step-format.sh"
    "clang-tidy:static-analysis:${SCRIPT_DIR}/ci/step-tidy.sh"
  )

  for entry in "${steps[@]}"; do
    local name="${entry%%:*}"
    local rest="${entry#*:}"
    local category="${rest%%:*}"
    local script="${rest#*:}"

    echo ""
    echo ">>> ${name}"
    run_check "${name}" "${category}" bash "${script}"
  done

  # ---- 收尾 -------------------------------------------------------
  finish_result

  echo ""
  echo "=== CI 完成 (${FAILED_COUNT} failed) ==="
  cat "${RESULT_FILE}"

  if [[ ${FAILED_COUNT} -gt 0 ]]; then
    exit 1
  fi
  exit 0
}

main "$@"
