#!/usr/bin/env bash
# Validate commit message references a valid GitHub Issue with matching Tag.
# Default: warn only (exit 0). With --strict: fail on error (exit 1).
#
# Usage:
#   bash scripts/check-issue-ref.sh <commit-ref>          (CI, non-blocking)
#   bash scripts/check-issue-ref.sh --strict <commit-ref> (commit hook, blocking)

set -euo pipefail

STRICT=false
COMMIT_REF="HEAD"

for arg in "$@"; do
  case "$arg" in
    --strict) STRICT=true ;;
    *) COMMIT_REF="$arg" ;;
  esac
done

GITHUB_REPO="yun-qilong/lcdriv"

warn() {
  echo "=========================================" >&2
  echo "ISSUE CHECK WARNING: $*" >&2
  echo "=========================================" >&2
}

die() {
  if $STRICT; then
    echo "=========================================" >&2
    echo "ISSUE CHECK FAILED: $*" >&2
    echo "=========================================" >&2
    exit 1
  else
    warn "$*"
    exit 0
  fi
}

# ---- extract fields from commit message ------------------------------
COMMIT_MSG=$(git log -1 --format=%B "$COMMIT_REF" 2>/dev/null || true)

if [[ -z "$COMMIT_MSG" ]]; then
  die "Cannot read commit message for $COMMIT_REF"
fi

SUBJECT=$(echo "$COMMIT_MSG" | head -1)

# Extract first [...] as Tag
TAG=$(echo "$SUBJECT" | grep -oP '^\[[^]]+\]' | head -1 || true)

if [[ -z "$TAG" ]]; then
  die "Subject must start with [Tag].
  Subject: $SUBJECT
  Example: [FT0001] hello window 首提交 (#1)"
fi

# [None] bypasses all checks
if [[ "$TAG" == "[None]" ]]; then
  echo "✓ [None] — issue check bypassed"
  exit 0
fi

# Extract issue number from (#N)
ISSUE_NUM=$(echo "$SUBJECT" | grep -oP '\(#\d+\)' | grep -oP '\d+' | head -1 || true)

if [[ -z "$ISSUE_NUM" ]]; then
  die "Subject must reference a GitHub Issue with (#N).
  Subject: $SUBJECT"
fi

echo "Commit: $SUBJECT"
echo "Tag:    $TAG"
echo "Issue:  #$ISSUE_NUM"

# ---- cross-validate: GitHub API --------------------------------------
# 用 mktemp 唯一临时文件（+ trap 清理），避免多个项目 CI 并发写同一 /tmp 文件相互覆盖
ISSUE_BODY="$(mktemp /tmp/issue-body.XXXXXX.json)"
trap 'rm -f "$ISSUE_BODY"' EXIT

GITHUB_API="https://api.github.com/repos/${GITHUB_REPO}/issues/${ISSUE_NUM}"
# 私有仓库需要鉴权：有 GITHUB_TOKEN 环境变量则带上（GitHub Actions 自动注入；
# Jenkins 侧需在 job/全局环境变量里配一个 PAT）。无 token 时退化为匿名（仅公开仓库可用）。
AUTH_HEADER=()
if [[ -n "${GITHUB_TOKEN:-}" ]]; then
    AUTH_HEADER=(-H "Authorization: Bearer ${GITHUB_TOKEN}")
fi
HTTP_CODE=$(curl -s -o "$ISSUE_BODY" -w "%{http_code}" \
  -H "Accept: application/vnd.github.v3+json" \
  "${AUTH_HEADER[@]}" \
  "$GITHUB_API" 2>/dev/null || echo "000")

if [[ "$HTTP_CODE" != "200" ]]; then
  die "GitHub Issue #${ISSUE_NUM} not found (HTTP $HTTP_CODE).
  Create it first: https://github.com/${GITHUB_REPO}/issues/new"
fi

# Strip brackets for comparison: [FT0001] → FT0001
TAG_CLEAN=$(echo "$TAG" | tr -d '[]')
EXPECTED_TAG_LINE="**Tag**: $TAG_CLEAN"

if grep -qP "\*\*Tag:?\*\*:?\s*${TAG_CLEAN}(?![-A-Za-z0-9])" "$ISSUE_BODY" 2>/dev/null; then
  echo "✓ Tag-Issue cross-validation passed"

  # Check if issue is closed (strict mode only)
  if $STRICT; then
    ISSUE_STATE=$(grep -oP '"state"\s*:\s*"\K\w+' "$ISSUE_BODY" | head -1)
    if [[ "$ISSUE_STATE" == "closed" ]]; then
      die "Issue #${ISSUE_NUM} is CLOSED.
  Reopen it or use a different issue."
    fi
  fi

  exit 0
else
  die "Issue #${ISSUE_NUM} body does NOT contain: $EXPECTED_TAG_LINE
  Add it to the issue body."
fi
