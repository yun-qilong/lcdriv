pipeline {
  agent any
  parameters {
    choice(name: 'CI_MODE', choices: ['full', 'precheck'])
  }
  // 私有仓库 issue 检查需要 GitHub PAT（Jenkins UI 建 credential 'lcdriv-github-token'，
  // Secret text 类型，token 需有 repo 读权限）。未建时退化为匿名访问，私有仓库会检查失败。
  environment {
    GITHUB_TOKEN = credentials('lcdriv-github-token')
  }
  stages {
    stage('Checkout') {
      steps {
        checkout([$class: 'GitSCM',
          branches: [[name: 'FETCH_HEAD']],
          userRemoteConfigs: [[refspec: params.GERRIT_REFSPEC ?: 'refs/heads/main', url: 'ssh://qilyun@localhost:19418/lcdriv']],
          extensions: [[$class: 'RelativeTargetDirectory', relativeTargetDir: 'src']]
        ])
      }
    }
    stage('Issue Check') {
      when { expression { params.CI_MODE == 'full' } }
      steps {
        catchError(buildResult: 'FAILURE', stageResult: 'FAILURE') {
          dir('src') {
            sh 'bash scripts/check-issue-ref.sh --strict FETCH_HEAD'
          }
        }
      }
    }
    stage('Build') {
      steps {
        dir('src') {
          sh '''
          if [ ! -f CMakeLists.txt ]; then
            echo "SKIP: 尚无 CMakeLists.txt（源码未落地），跳过 Build"
            exit 0
          fi
          cmake -B build -DCMAKE_BUILD_TYPE=Release -DLCDRIV_BUILD_TESTS=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON && cmake --build build --target lcdriv lcdriv_ut -j $(nproc)
          '''
        }
      }
    }
    stage('Test') {
      when { expression { fileExists('src/CMakeLists.txt') } }
      steps {
        dir('src/build') { sh 'ctest --output-on-failure -L lcdriv' }
      }
    }
    stage('Format') {
      when { expression { params.CI_MODE == 'full' } }
      steps {
        dir('src') {
          sh '''#!/bin/bash
FAIL=0
FILES=$(find . -name "*.cpp" -o -name "*.hpp" | grep -v "/generated/" | grep -v "/build/" | sort)
for f in $FILES; do
  [ -f "$f" ] || continue
  if ! /usr/bin/clang-format-15 --dry-run --Werror "$f" 2>/dev/null; then
    echo ""; echo "==== FORMAT ISSUES: $f ===="
    diff -u "$f" <(/usr/bin/clang-format-15 "$f") 2>/dev/null || true
    FAIL=1
  fi
done
if [ "$FAIL" != "0" ]; then echo "Format check FAILED"; exit 1; fi
echo "Format check passed"'''
        }
      }
    }
    stage('Tidy') {
      when { expression { params.CI_MODE == 'full' && fileExists('src/CMakeLists.txt') } }
      steps {
        dir('src') {
          sh '''echo "=== clang-tidy diagnostics ==="
TIDY_BIN="$(ls -d "$HOME"/tools/llvm-*/bin/clang-tidy 2>/dev/null | sort -V | tail -1)"
[ -n "$TIDY_BIN" ] || TIDY_BIN="clang-tidy-14"
echo "Using: $TIDY_BIN"
"$TIDY_BIN" --version || echo "version check failed"
echo "================================"
CLANG_TIDY_BIN="$TIDY_BIN" python3 scripts/run_tidy.py'''
        }
      }
    }
  }
}
