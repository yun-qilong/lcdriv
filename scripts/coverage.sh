#!/usr/bin/env bash
# 一键生成覆盖率报告（gcovr）
# 用法：./scripts/coverage.sh [构建目录]   （默认 build-coverage）
set -euo pipefail

cd "$(dirname "$0")/.."

BUILD_DIR="${1:-build-coverage}"

cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Debug \
      -DLCDRIV_BUILD_TESTS=ON -DLCDRIV_COVERAGE=ON
cmake --build "$BUILD_DIR" --target lcdriv_ut -j "$(nproc)"
ctest --test-dir "$BUILD_DIR" --output-on-failure -L lcdriv

mkdir -p coverage
gcovr -r . --object-directory "$BUILD_DIR" --exclude 'tests/' \
      --html --html-details -o coverage/index.html \
      --xml -o coverage/coverage.xml \
      --print-summary

echo
echo "HTML: coverage/index.html"
echo "XML:  coverage/coverage.xml"
