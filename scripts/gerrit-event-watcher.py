#!/usr/bin/env python3
"""占位（已迁移通用版）：AI 留痕 + CI 分流逻辑在 ~/ci_common/gerrit_event_watcher_lib.py。

本文件仅转发到通用入口；i sg 启动方式不变（python3 -u 本文件）。
配置：~/ci_common/projects/lcdriv.json
"""

import os
import sys

CI_COMMON = os.path.expanduser("~/ci_common")
PROJECT_JSON = os.path.join(CI_COMMON, "projects", "lcdriv.json")

sys.path.insert(0, CI_COMMON)
from gerrit_event_watcher_lib import main  # noqa: E402

if __name__ == "__main__":
    _args = sys.argv[1:]
    if "--config" not in _args:
        _args = ["--config", PROJECT_JSON] + _args
    sys.exit(main(_args))
